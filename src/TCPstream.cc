// This may look like C code, but it is really -*- C++ -*-
/*
 ************************************************************************
 *
 * 				TCP Streams
 *
 * The network code is based to a large extent on my tcp_relay.cc application;
 * GNU's libg++-2.7.2/libio/ implementation of the standard C++ library
 * provided guidance and inspiration for TCPbuf and TCPstream, In particular,
 * source code files fstream.{h,cc}, filebuf.{h,cc}, fileops.c,
 * strstream.{h,cc}, strops.c, genops.c. File indstream.{h,cc} shows the
 * minimal streambuf functionality needed to be supported. iostream.info-1:
 * windowbuf provides some hints, although they aren't very relevant as it
 * turns out (ignore buffering altogether).
 *
 * $Id TCPstream.cc,v 2.7 2000/11/15 03:06:44 oleg Exp oleg $
 * $Id: TCPstream.cc,v 1.4 2004/04/15 12:31:40 brodo Exp $
 *
 ************************************************************************
 */

#ifdef __GNUC__
#pragma implementation
#endif

#include "TCPstream.h"
#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <cstring>

/*
 *------------------------------------------------------------------------
 *        		A part of my TCP/IP class lib
 */

#if defined(unix)
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/file.h>
#include <netinet/tcp.h>
#include <stdarg.h>

#undef B_BEOS_VERSION

void _error(const char * message,...)
{
  va_list args;
  va_start(args,message);		/* Init 'args' to the beginning of */
					/* the variable length list of args*/
  fprintf(stderr,"\n_error:\n"); 	
  vfprintf(stderr,message,args);
  fputs("\n",stderr);
#ifdef __MWERKS__
  exit(4);
#else  
  abort();
#endif
}

void message(const char * text,...)
{
}


class ostream_withassign : public ostream {
public:
  ostream_withassign(streambuf* sb) :
      ostream(sb) {}
  ostream_withassign& operator=(ostream& rhs)
    {  if (&rhs != (ostream*)this) init (rhs.rdbuf ()); return *this; }
  ostream_withassign& operator=(ostream_withassign& rhs)
    { operator= (static_cast<ostream&> (rhs)); return *this; }
  ostream_withassign& operator=(streambuf* sb)
    { init(sb); return *this; }
};
//#endif

#include <iostream>



extern "C"
{


                                // Convertion between the host and the network
                                // byte orders
#if !defined(htons) && !defined(linux)
unsigned short htons(unsigned int data);        // For a short data item
unsigned short ntohs(unsigned int data);        // For a short data item
unsigned long  htonl(unsigned long data);       // For a long data item
#endif
#include <arpa/inet.h>
}
#define SOCKETclose(Socket) ::close(Socket)
#define SOCKETwrite(Socket,Buffer,Nbytes) ::write(Socket,Buffer,Nbytes)
#define SOCKETread(Socket,Buffer,Nbytes) ::read(Socket,Buffer,Nbytes)
#define SOCKET_WOULDBLOCK() (errno == EAGAIN || errno == EINTR)

#elif  defined(B_BEOS_VERSION)
			// Alas, like WinSock from the blasted Win platform
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/file.h>
#define SOCKETclose(Socket) ::close(Socket)
#define SOCKETwrite(Socket,Buffer,Nbytes) ::send(Socket,Buffer,Nbytes,0)
#define SOCKETread(Socket,Buffer,Nbytes) ::recv(Socket,Buffer,Nbytes,0)
#define SOCKET_WOULDBLOCK() (errno == EAGAIN || errno == EINTR)

#else
				// A blasted Win platform...
#define sleep(X) Sleep((X)*1000)
#define SOCKETclose(Socket) ::closesocket(Socket)
#define SOCKETwrite(Socket,Buffer,Nbytes) ::send(Socket,Buffer,Nbytes,0)
#define SOCKETread(Socket,Buffer,Nbytes) ::recv(Socket,Buffer,Nbytes,0)
#define SOCKET_WOULDBLOCK() (WSAGetLastError() == WSAEWOULDBLOCK)

			// Initialize the WinSock - Win32 socket library
class SocketInit : public WSAData
{
  static SocketInit me;	// The only instance of this class that can be made
  SocketInit(void);	// And this private constructor would make it
public:
  ~SocketInit(void)		{ WSACleanup(); }
};

SocketInit SocketInit::me;

			// Init the socket library
SocketInit::SocketInit(void)
{
  const WORD version_needed = MAKEWORD(1,1); // lo-order byte: major version
  if( const int err = WSAStartup(version_needed, this) )
    _error("winsock.dll was not found, WSAStartup returned %d\n",
    		  err);

	// Confirm that the Windows Sockets DLL supports 1.1
	// Note that if the DLL supports versions greater
	// than 1.1 in addition to 1.1, it will still return
	// 1.1 in wVersion since that is the version we requested
  if( wVersion != version_needed )
   _error("Found only version %x of WinSock DLL "
   		 "\nwhich is not what we asked for, %x",
          wVersion,version_needed);

//  message("\nWinSock initialized\nDescription: %s\nStatus %s\n",
//  		    szDescription,szSystemStatus);
}
#endif

#if !defined(BSIZE)
#define BSIZE 8192
#endif


/*
 *------------------------------------------------------------------------
 *                 Error handling and default callbacks
 */

			// By default, do blocking i/o
bool NetCallback::async_io_hint(void)
{ return false; }


void NetCallback::yield(void)
{
  message("yielding...\n");
  sleep(1);
}
  				// This function is called when an i/o error
  				// has occurred. If this function ever returns
  				// (which it ought not), stream's 'fail' bit
  				// will be set
void NetCallback::on_error(const char reason [])
{
  //cerr << "AAARRRGGGHHHHHH\n\n";
  //const int saved_errno = errno;
  perror(reason);
  //exit(100+saved_errno);
}

NetCallback NetCallback::default_callback;
NetCallback * CurrentNetCallback::current = &NetCallback::default_callback;


				// Every NetCallback object automatically
				// registers itself, and unregisters
				// upon destruction
NetCallback::NetCallback(void)	{ CurrentNetCallback::do_register(this); }
NetCallback::~NetCallback(void)	{ CurrentNetCallback::unregister(this); }

NetCallback * CurrentNetCallback::do_register(NetCallback * callback)
{
  NetCallback * old = current;
  assert( current = callback );
  return old;
}

void CurrentNetCallback::unregister(NetCallback *)
{
  current = &NetCallback::default_callback;
}

                                // Make sure that a system call went well
                                // If a system call (say, fcntl()) fails
                                // it returns -1 and sets the errno
#define has_failedXX(FX,LINE,FILE) ( (FX) < 0 ? \
	(CurrentNetCallback::on_error("Failed system call " #FX " at line " LINE \
			" of " FILE "\n"),true) : false)
#define QMAKESTR(X) #X
#define has_failedXY(FX,LINE) has_failedXX(FX,QMAKESTR(LINE),__FILE__)
#define has_failed(FX) has_failedXY(FX,__LINE__)


/*
 *------------------------------------------------------------------------
 *                        Class IPaddress that provides
 *                    addressing and naming in the IP network
 */


                                // Obtains the IP address of a specified
                                // host. The host name may be specified either
                                // in a dot notation, or as a host "name".
                                // Name resolution is performed in the
                                // latter case.
IPaddress::IPaddress(const char * host_name)
{
                                        // First check to see if the host
                                        // name is given in the IP address
                                        // dot notation
  struct hostent *host_ptr = ::gethostbyname(host_name);
  
  if( host_ptr == 0 )
  {
    cerr << "Host name '" << host_name << "' cannot be resolved";
    has_failed(-1);
    return;
  }
  if( host_ptr->h_addrtype != AF_INET )
  {
    cerr << "Host name '" << host_name
    	     << "' isn't an Internet site, or so the DNS says";
    has_failed(-1);
    return;
  }

  memcpy(&address,host_ptr->h_addr,sizeof(address));
}


				// PrintOn the address; in a nice
				// form if possible (that is, trying
  				// to back-resolve to a symbolic
  				// host name)
                                // We use the fact that the IP address is in
                                // the net order, that is, the MSB first
ostream& operator << (ostream& os, const IPaddress addr)
{
  struct hostent * const host_ptr =
  	::gethostbyaddr((char *)&addr.address,
                        sizeof(addr.address), AF_INET);
  if( host_ptr != 0 )
    return os << host_ptr->h_name;

				// Reverse DNS failed, use print in
                                // the dot notation
  char buffer[80];
  const unsigned int native_addr = ntohl(addr.address);
  sprintf(buffer,"%0d.%0d.%0d.%0d", (native_addr >> 24) & 0xff,
          (native_addr >> 16) & 0xff, (native_addr >> 8) & 0xff,
          native_addr & 0xff);
  return os << buffer;
}

/*
 *------------------------------------------------------------------------
 *                              Sockets
 */
                                // Create an IP address space entity
SocketAddr::SocketAddr(const IPaddress host, const short port_no)
{
  assert( port_no > 0 );
  sin_family = AF_INET;
  sin_port = htons((short)port_no);
  sin_addr.s_addr = host.net_addr();
}

                        // PrintON: a socket address
ostream& operator << (ostream& os, const SocketAddr& addr)
{
  assert( addr.sin_family == AF_INET );
  return os << IPaddress(addr.sin_addr.s_addr) << ':'
	    << (unsigned short)ntohs((short)addr.sin_port);
}


/*
 *------------------------------------------------------------------------
 *                 		TCPbuffer
 *
 *
 */

			// A constructor that doesn't do much...
TCPbuf::TCPbuf(void)
: socket_handle(-1),
buf_ptr(0)
{
}

			// Initialize the TCP buffer: attempting to connect
			// to a (hopefully listening) socket on
			// probably another computer/port
			// On any error, first call CurrentNetCallback::on_error(),
			// and if it returns, return NULL
TCPbuf* TCPbuf::connect(const SocketAddr target_address)
{
  if( is_open() )
    return 0;				// already connected

  if( has_failed(socket_handle = socket(AF_INET,SOCK_STREAM,0)) )
    return 0;

//  set_blocking_io(false);
  
  for(;;)
  {
    const int connect_status =
    	::connect(socket_handle, (sockaddr *)target_address,
		  sizeof(target_address));
    if( connect_status == 0 )
      break;
    //    if( errno == EINPROGRESS || errno == EINTR )
    //      break; //callbacks.yield();
    else
      return has_failed(connect_status), (TCPbuf*)0;
  }
  
  set_blocking_io(!CurrentNetCallback::async_io_hint());
  
			// As we do all the buffering ourselves
			// See man tcp(7P) for more details on TCP_NODELAY
#if !defined(B_BEOS_VERSION)
  set_sock_opt(TCP_NODELAY,true,IPPROTO_TCP);
  //message("\nSend buffer size %d\n",get_sock_opt(SO_SNDBUF,int()));
#endif
  return this;
}

			// Another initialization function:
  			// Take a file handle (which is supposed to be a
  			// listening socket), accept a connection if any,
  			// and return the corresponding TCPbuf
  			// for that connection.
  			// On exit, peeraddr would be an addr of the
  			// connected peer
			// On any error, first call CurrentNetCallback::on_error(),
			// and if it returns, return NULL
TCPbuf* TCPbuf::accept(int listening_socket, SocketAddr& peeraddr)
{
  if( is_open() )
    return 0;				// already connected
  
  for(;;)
  {
    socklen_t target_addr_size = sizeof(peeraddr);
    socket_handle = ::accept(listening_socket,(sockaddr *)peeraddr,
		  &target_addr_size);
    if( socket_handle >= 0 )
      break;			// Successfully accepted the connection
    if( SOCKET_WOULDBLOCK() )
      CurrentNetCallback::yield();
    else
      return has_failed(socket_handle), (TCPbuf*)0;
  }
  
  set_blocking_io(!CurrentNetCallback::async_io_hint());
  
			// As we do all the buffering ourselves
			// See man tcp(7P) for more details on TCP_NODELAY
#if !defined(B_BEOS_VERSION)
  set_sock_opt(TCP_NODELAY,true,IPPROTO_TCP);
  //message("\nSend buffer size %d\n",get_sock_opt(SO_SNDBUF,int()));
#endif
  return this;
}


				// Close the socket
TCPbuf* TCPbuf::close(void)
{
  assert( socket_handle >= 0 );
  const int socket_being_closed = socket_handle;
  socket_handle = -1;
  return has_failed(SOCKETclose(socket_being_closed)) ? 0 : this;
}

TCPbuf::~TCPbuf(void)
{
  if( is_open() )
      close();
}


			// Set up a POSIX-style blocking/non-blocking I/O
			// In a blocking mode (default), reading from a socket
			// blocks if there is nothing to read. By the same
			// token, when the output buffer is full, writing blocks
			// until the buffers would be emptied (into the link).
			// In a non-blocking mode, read/write always return
			// right away, telling how many bytes they have
			// transmitted (if any), or -1 with errno set to EAGAIN.
			// The latter case means that nothing can be transmitted
			// at the moment, so the operation has to be retried
			// later
void TCPbuf::set_blocking_io(const bool onoff)
{
#ifdef unix
  int arg = ::fcntl(socket_handle,F_GETFL,0);
  has_failed( arg );
  has_failed( ::fcntl(socket_handle,F_SETFL,onoff ? arg & ~O_NONBLOCK :
		 arg | O_NONBLOCK) );
#else
  unsigned long ioctl_arg = onoff ? 1 : 0;
//  has_failed(ioctlsocket(socket_handle,FIONBIO,&ioctl_arg));
#endif
}


			// A set of overloaded functions to get
			// a bool or int socket's option
int TCPbuf::get_sock_opt(const int opt_name, const int, const int level)
{
#if defined(B_BEOS_VERSION)
  _error("getsockopt is not supported under BeOS R4\n");
  return 0;
#else
  int opt = 0; socklen_t len = sizeof(opt);
  has_failed( ::getsockopt(socket_handle, level, opt_name,
			   (char*)&opt, &len) );
  return opt;
#endif
}

bool TCPbuf::get_sock_opt(const int opt_name, const bool, const int level)
{
  return get_sock_opt(opt_name,int(),level);
}

			// A set of overloaded functions to set
			// a bool or int socket's option
void TCPbuf::set_sock_opt(const int opt_name, const int value, const int level)
{
  has_failed( ::setsockopt(socket_handle, level, opt_name,
			   (char*)&value, sizeof(value)) );
}

void TCPbuf::set_sock_opt(const int opt_name, const bool value, const int level)
{
  set_sock_opt(opt_name,value ? 1 : 0,level);
}

				// Enable/disable SIGIO upon arriving of a
				// new packet
void TCPbuf::enable_sigio(const bool onoff)
{
#if 0
  int arg = onoff ? 1 : 0;
  has_failed( ioctl(socket_handle,FIOASYNC,&arg) );
  arg = onoff ? -getpid() : 0;
                                // Tell which process should get SIGIO
  has_failed( ioctl(socket_handle,SIOCSPGRP,&arg) );
  
//  message("after enable_sigio: flags 0x%x\n",fcntl(socket_handle,F_GETFL,0));
//  message("owner %d\n",fcntl(socket_handle,F_GETOWN,0));
#endif
}

			// Write characters to the stream, giving time
			// Return the number of characters actually written
			// (which is always n, or EOF in case of error)
int TCPbuf::write(const char * buffer, const int n)
{
  if( !is_open() )
    return EOF;
  if( n == 0 )
    return 0;
  
  const char * const buffer_end = buffer + n;
  while( buffer < buffer_end )
  {
    const int char_written = SOCKETwrite(socket_handle,buffer,buffer_end-buffer);
    if( char_written > 0 )
      buffer += char_written;
    else
      if( char_written < 0 && !(SOCKET_WOULDBLOCK()) )
        return EOF; //has_failed(char_written),
      else
        CurrentNetCallback::yield();
  }
  return n;
}

			// Read characters from the stream into a given
			// buffer (of given size n)
			// If there is nothing to read, yield and keep
			// trying
			// Return the number of characters actually read, or
			// 0 (in the case of EOF) or EOF on error
int TCPbuf::read(char * buffer, const int n)
{
  if( !is_open() )
    return EOF;
  if( n == 0 )
    return 0;
   
  for(;;)
  {
    const int char_read = SOCKETread(socket_handle,buffer,n);
    if( char_read >= 0 )
      return char_read;
    if( SOCKET_WOULDBLOCK() )
      CurrentNetCallback::yield();
    else
      return EOF;
  }
}

/*
 *------------------------------------------------------------------------
 *               Implementing the standard streambuf protocol
 *
 * Ripped from libg++-2.7.2/libio/iostream.info-1
 *
 * "Streambuf buffer management is fairly sophisticated
 * (this is a nice way to say "complicated"). The standard protocol has the
 * following "areas":
 * 	The "put area" contains characters waiting for output.
 *	The "get area" contains characters available for reading.
 *
 * The following methods are used to manipulate these areas.  These are
 * all protected methods, which are intended to be used by virtual function
 * in classes derived from `streambuf'.  They are also all ANSI/ISO-standard,
 * and the ugly names are traditional.  (Note that if a pointer points to
 * the 'end' of an area, it means that it points to the character after
 * the area.)
 * 	char* streambuf::pbase() const
 *	Returns a pointer to the start of the put area.
 *
 *	char* streambuf::epptr() const
 *	Returns a pointer to the end of the put area.
 *
 *	char* streambuf::pptr() const
 *	If pptr() < epptr(), the pptr() returns a pointer to the
 *	current put position.  (In that case, the next write will
 *	overwrite *pptr(), and increment pptr()). Otherwise, there is
 *	no put position available (and the next character written will
 *	cause streambuf::overflow() to be called).
 *
 *	void streambuf::pbump(int N)
 *	Add N to the current put pointer.  No error checking is done.
 *
 *	void streambuf::setp(char* P, char* E)
 * 	Sets the start of the put area to P, the end of the put area to E,
 *	and the current put pointer to P (also).
 *
 *	char* streambuf::eback() const
 *	Returns a pointer to the start of the get area.
 *
 *	char* streambuf::egptr() const
 * 	Returns a pointer to the end of the get area.
 *
 *	char* streambuf::gptr() const
 *	If gptr() < egptr(), then gptr() returns a pointer to the
 *	current get position.  (In that case the next read will read
 *	*gptr(), and possibly increment gptr()). Otherwise, there is
 *	no read position available (and the next read will cause
 *	streambuf::underflow() to be called).
 *
 *	void streambuf:gbump(int N)
 *	Add N to the current get pointer.  No error checking is done.
 *
 *	void streambuf::setg(char* B, char* P, char* E)
 *	Sets the start of the get area to B, the end of the get area to E,
 *	and the current get pointer to P."
 *
 * For the TCPbuf, we use a single buffer for both reading _and_ writing.
 * underflow() commits all unfinished writes, and fills in the buffer;
 * any put operation discards all unread data, writing to the beginning of
 * the buffer.
 * The buffer is accessed via methods base() (which gives a pointer to
 * the beginning of the buffer) and ebuf(), returning the buffer's end.
 */
 
			// Flush (write out) the put area, resetting
			// pptr if the write was successful
			// Return 0, or EOF on error
int TCPbuf::sync(void)
{
  const int n = pptr() - pbase();
  assert( n >= 0 );
  if( n == 0 )
    return 0;
  return write(pbase(), n) == n ? (pbump(-n), 0) : EOF;
}

			// Write out the buffer into the communication channel
			// After that, put a character c
			// (unless it's EOF)
			// Return 0, or EOF on error
			// This method allocates a buffer if there wasn't any,
			// and switches it to the "put mode" (discarding
			// all un-get data)
int TCPbuf::overflow(int ch)
{
  if( sync() == EOF )
    return EOF;
  if( base() == 0 )			// If there wasn't any buffer,
    doallocate();			// ... make one

  setg(base(),base(),base());		// Make the get area completely empty
  setp(base(),ebuf());			// Give all the buffer to the put area

  assert( pptr() != 0 );
  if( ch != EOF )
    *pptr() = ch, pbump(1);
  return 0;
}

#if 0
			// Optimization: writing out a memory block
			// directly, bypassing the put area
			// Return the number of characters written
			// But we DO want to direct all output to the
			// buffer, and write to the channel only
			// when buffer overflows (or flushed)
			// That's why we do *not* define this function
int TCPbuf::xsputn(const char * block, const int n)
{
  return sync() == EOF ? 0 : write(block, n);
}
#endif

			// Fill in the get area, and return its first
			// character
			// We actually read into the main buffer from
			// the beginning (syncing pending output if was any)
			// and set the get area to that part of the buffer.
			// We also set pptr() = eptr() so that the first write
			// would call overflow() (which would discard the
			// read data)
int TCPbuf::underflow(void)
{
  if( gptr() < egptr() )
    return *(unsigned char*)gptr();

  if( sync() == EOF )	// commit all pending output first
    return EOF;		// libg++ uses switch_to_get_mode() in here

  if( base() == 0 )	// If there wasn't any buffer, make one first
    doallocate();
  
  assert( base() );
  const int count = read(base(),ebuf() - base());
  setg(base(),base(),base() + (count <= 0 ? 0 : count));
  setp(base(),base());		// no put area - do overflow on the first put
  return count <= 0 ? EOF : *(unsigned char*)gptr();
}

			// Allocate a new buffer
int TCPbuf::doallocate(void)
{
  const int size = BSIZE;
//  message("\nallocating TCPStream buffer of size %d\n",size);
  char *p;			// have to do malloc() as ~streambuf() does
  // free() on the buffer
  assert(buf_ptr==0);
  assert( (p = (char *)malloc(size)) );
  setb(p,p+size,true);
  //  setb(p,p+size,false);
  buf_ptr = p;
  return 1;
}

			// Associate a user buffer with the TCPbuf
TCPbuf* TCPbuf::setbuf(char* p, const int len)
{
  message("\ndoing TCPbuf::setbuf()\n");
#if defined(B_BEOS_VERSION)
  buffer.dispose();
  setb(p,p+len,false);
#else
  if( streambuf::setbuf(p,len) == 0 )
    return 0;
#endif
  setp(base(),base());
  setg(base(),base(),base());
  return this;
}


			// The status of the buffer
			// (mainly for the debugging purposes)
void TCPbuf::dump(const char title []) const
{
  message("\n\n---> TCP buffer dump '%s'",title);
  message("\nsocket handle %d",socket_handle);
  message("\nput area: 0x%x through 0x%x + %d, current pointer at +%d",
  	  pbase(),pbase(),epptr()-pbase(),pptr()-pbase());
  message("\nget area: 0x%x through 0x%x + %d, current pointer at +%d",
  	  eback(), eback(), egptr()-eback(), gptr() - eback());
  message("\nbuffer overall: 0x%x through 0x%x + %d",
  	  base(),base(),ebuf()-base());
  message("\n\n");
}

/*
 *------------------------------------------------------------------------
 *                 		TCPstream
 * standard fodder, gleaned from fstream.cc and modified a bit
 */

#if 0
TCPstream::TCPstream(StreamSocket& socket)
{
  init(&tcp_buffer);
  if( !rdbuf()->connect(socket) )
    set(ios::badbit);
}
#endif

TCPstream::TCPstream(void)
#if defined(B_BEOS_VERSION)
: basic_iostream(0)
#else
: iostream(&tcp_buffer)
#endif
{
  //init(&tcp_buffer);
}

void TCPstream::connect(const SocketAddr target_address)
{
  clear();
  if( !rdbuf()->connect(target_address) )
    set(ios::badbit);
}

void TCPstream::close(void)
{
  if( !rdbuf()->close() )
      set(ios::failbit);
  if (rdbuf()->buf_ptr) free(rdbuf()->buf_ptr);
}
