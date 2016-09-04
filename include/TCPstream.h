// This may look like C code, but it is really -*- C++ -*-
/*
 ************************************************************************
 *
 * 				TCP Streams
 *
 * A standard C++ stream to push data to and take data from a TCP stream.
 * The functionality is identical to that of an fstream: the only difference
 * is that it may take really a while for an i/o operation on a TCP channel
 * to finish. It's unreasonable to make the whole application wait. Thus
 * an application is given a chance to tell a TCP buffer being created
 * that it does not want the network i/o to block. Application does it
 * by deriving and instantiating a NetCallback object. If the currently
 * registered NetCallback::async_io_hint() returns true, a newly created
 * TCP socket is turned into a (POSIX) non-blocking mode;
 * when the stream wants to flush/fill its buffer and the operation can't
 * be completed immediately, the stream calls NetCallback::yield(). 
 * When this function returns, the stream gives another try to the
 * input/output operation.
 * 
 * The network class hierarchy is borrowed from my tcp_relay.cc project.
 * The stream stuff is gleaned from libg++ libio implementation
 *
 * $Id: TCPstream.h,v 1.2 2008/03/05 08:09:07 brodo Exp $
 *
 ************************************************************************
 */

#ifndef __GNUC__
#pragma once
#else
#pragma interface
#endif

#ifndef __TCPstream_h
#define __TCPstream_h

#include <stdio.h>
#if defined(unix) || defined(B_BEOS_VERSION)
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#else
#include <winsock.h>
#endif
#include <iostream>
#if defined(B_BEOS_VERSION)
typedef  basic_iostream<char, char_traits<char> > iostream;
#endif

using namespace::std;

class SocketAddr;

				// An "abstract" class to declare functions
				// called when i/o is in progress, but not
				// finished yet
				// (say, when we have nothing to read)
				// Besides defining the interface, this
				// class provides a suitable default behavior
class NetCallback
{
public:
  virtual bool async_io_hint(void);
  virtual void yield(void);
  				// This function is called when an i/o error
  				// has occurred. If this function ever returns
  				// (which it ought not), stream's 'fail' bit
  				// will be set
  virtual void on_error(const char reason []);
  
  NetCallback(void);		// Registers this callback object
  virtual ~NetCallback(void);	// Unregisters this callback object

  static NetCallback default_callback;
};


				// This class keeps track of the currently
				// registered callback object and provides
				// static functions as suitable aliases
				// for those of the registered callback object.
				// This class serves only as a namespace
				// and a forwarder; one can't create an
				// object of this class
class CurrentNetCallback
{
  friend class NetCallback;

  static NetCallback * current;
  static NetCallback * do_register(NetCallback *);
  static void unregister(NetCallback *);
  
  CurrentNetCallback(void);	// Not implemented: constructing is
  CurrentNetCallback(const CurrentNetCallback&); // prohibited
public:
  static bool async_io_hint(void)	{ return current->async_io_hint(); }
  static void yield(void)		{ current->yield(); }
  static void on_error(const char reason []) { current->on_error(reason); }
};


                    // Networking classes
class IPaddress
{
  friend class SocketAddr;

  unsigned long address;                // Address: 4 bytes in the network
                                        // byte order
  IPaddress(const unsigned int netaddr) : address(netaddr) {}

public:
  IPaddress(void) : address(INADDR_ANY) {}   // Wildcard address
  IPaddress(const char * name);         // Involves the name resolution
  unsigned long net_addr(void) const { return address; }
  
  					// PrintOn the address; in a nice
  					// form if possible (that is, trying
  					// to back-resolve to a symbolic
  					// host name)
  friend ostream& operator << (ostream& os, const IPaddress addr);
  friend ostream& operator << (ostream& os, const SocketAddr& addr);
};



class SocketAddr : sockaddr_in
{
  friend class StreamSocket;
  friend class UDPsocketIn;
  SocketAddr(void) {}

public:
  SocketAddr(const IPaddress host, const short port_no);
  operator sockaddr * (void) const      { return (sockaddr *)this; }
  friend ostream& operator << (ostream& os, const SocketAddr& addr);
};


				// TCPstream buffer
				// Its streambuf parent is supposed to handle
				// all buffering chores. We only have to
				// allocate the buffer and provide for its
				// filling/flushing
class TCPbuf : public streambuf
{
  int socket_handle;		// Socket handle = file handle
  
  				// These are the functions that actually
  				// read/write from the socket to/from
  				// a given buffer. They return the number
  				// of bytes transmitted (or EOF on error)
  				// CurrentNetCallback::yield() is called if 
  				// the operation can't be completed quickly
  				// and async io was requested
  int write(const char * buffer, const int n);
  int read(char * buffer, const int n);

//#if defined(B_BEOS_VERSION)
  struct Buffer {
    char * start;
    char * end;
    bool do_dispose;
    void dispose(void)
    { if(!do_dispose) return;
      //free(start); start = end = 0; do_dispose = false;
      start = end = 0; do_dispose = false;
    }
    Buffer(void) : start(0), end(0), do_dispose(false) {}
    ~Buffer(void) { dispose(); }
  };
  Buffer buffer;
//#endif
  
protected:
				// Standard streambuf functions we have to
				// provide an implementation for
  virtual int overflow(int c = EOF);	// Write out a "put area"
  virtual int underflow(void);		// Fill in a "get area"
  virtual int sync(void);		// Commit all uncommitted writes
  virtual int doallocate(void);		// Allocate a new buffer

//#if defined(B_BEOS_VERSION)
			// we use the same buffer for get and put areas
  char * base(void) const { return buffer.start; }
  char * ebuf(void) const { return buffer.end; }
  void setb(char * buffer_beg, char * buffer_end, bool do_dispose)
    { buffer.start = buffer_beg; buffer.end = buffer_end;
      buffer.do_dispose = do_dispose;}
//#endif

  				// Some private TCP-specific stuff:
  				// getting and setting socket's options
  int get_sock_opt(const int opt_name, const int dummy, const int level = SOL_SOCKET);
  bool get_sock_opt(const int opt_name, const bool dummy, const int level = SOL_SOCKET);
  void set_sock_opt(const int opt_name, const int value, const int level = SOL_SOCKET);
  void set_sock_opt(const int opt_name, const bool value, const int level = SOL_SOCKET);

public:
				// Standard stream buffer interface
  TCPbuf(void);
  ~TCPbuf(void);
  char *buf_ptr;
 
  bool is_open(void) const { return socket_handle >= 0; }
  TCPbuf* close(void);
  virtual TCPbuf* setbuf(char* p, const int len);
  
  				// TCP stream is strictly sequential
#if !defined(B_BEOS_VERSION)
  //streampos seekoff(streamoff, ios::seek_dir, int)	{ return EOF; }
#endif

  	// We don't re-define xsputn() xsgetn() on purpose:
	// Let us use the default implementation, which stuffs/takes
	// data from the buffer
  //  int xsputn(const char* s, const int n);
  //    int xsgetn(char* s, const int n);

  				// Actual connection operation
  TCPbuf* connect(const SocketAddr target_address);
  
  void dump(const char title []) const;	// print out some diagnostics

  				// Some TCP specific stuff
  void set_blocking_io(const bool onoff);
				// Enable/disable SIGIO upon arriving of a
				// new packet
  void enable_sigio(const bool onoff);
  
  			// Take a file handle (which is supposed to be a
  			// listening socket), accept a connection if any,
  			// and return the corresponding TCPbuf
  			// for that connection.
  			// On exit, peeraddr would be an addr of the
  			// connected peer
  TCPbuf* accept(int listening_socket, SocketAddr& peeraddr);
};


class TCPstream : public iostream
{
  mutable TCPbuf tcp_buffer;	// mutable so rdbuf() can be const
public:
  TCPstream(void);
//  TCPstream(StreamSocket& socket);
  void connect(const SocketAddr target_address);

  				// Standard C++ stream fodder
  TCPbuf* rdbuf(void) const	{ return &tcp_buffer; }
  bool is_open() const 		{ return rdbuf()->is_open(); }

  void close(void);
//#if !defined(__GNUC__)
   		// Borland and VC++ do not have ios::set() to set state bits
  //void set(ios::io_state bit) { clear(rdstate() | bit); }
  void set(std::_Ios_Iostate bit) { clear(rdstate() | bit); }
//#endif
};


#endif
