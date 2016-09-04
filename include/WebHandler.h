//
// Copyright (C) David Brodrick
// Copyright (C) CSIRO Australia Telescope National Facility
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.
//
// $Id: WebHandler.h,v 1.5 2004/04/03 01:44:36 brodo Exp $

//For the duration of it's existence the WebHandler parses and
//services requests from the client. We deregister with the WebMaster
//when the connection times out or is otherwise closed.

#ifndef _WEBHANDLER_HDR_
#define _WEBHANDLER_HDR_

#include <ThreadedObject.h>
#include <TCPstream.h>
#include <sstream>
#include <time.h>

using namespace::std;

class StoreMaster;
class WebMaster;
class TCPstream;
class SocketAddr;

class WebHandler : public ThreadedObject {
public:
  //Create a handler for the given TCP stream on the given socket.
  //Data is provided to the client from the specified StoreMaster and
  //raw audio data from the 'rawstore' if non-NULL.
  WebHandler(int listening_socket, StoreMaster *store,
	     StoreMaster *rawstore, WebMaster *master);

  ~WebHandler();

  //Main loop of parsing and servicing client connections
  void run();

private:
  //Close the TCP socket and inform WebMaster we have finished. This
  //effectively kills the thread and causes self deletion, "The End".
  void dropConnection();
  //Parse the command and service it
  void serviceCommand(istringstream &command);

  //Handle command which wants all data between two argument epochs
  void doBetween(istringstream &command);
  //Handle command which wants all raw data between two argument epochs
  void doRawBetween(istringstream &command);
  //Handle command which wants all data after an epoch in ASCII
  void doAfterASCII(istringstream &command);

  //Inline for reading, and checking, a time stamp from the client
  inline
  long long readepoch(istringstream &arg) {
    long long res;
    arg >> res;
    //Ensure there was no problem reading the epoch value
    if (arg.fail()) {
      itsError = true;
      dropConnection();
    }

    if (res!=0) {
      //Do a reality check on argument, drop client if rubbish
      time_t checktime = res/1000000;
      struct tm *checkutc = gmtime(&checktime);
      //This program will be retired by 2200 - I'll bet my life on it!
      if (checkutc->tm_year+1900<1990 || checkutc->tm_year+1900>2200) {
	dropConnection();
      }
    }
    return res;
  }

  //The real fair dinkum socket
  int itsSock;
  //The TCP connection to our client
  TCPstream itsClient;
  //Socket reference, contains client address, etc.
  SocketAddr itsSocket;
  //The store from which we retrieve data for our client
  StoreMaster *itsStore;
  //The rolling store from which we retrieve raw audio data for our client
  StoreMaster *itsRawStore;
  //The connection who did spawn us, and from whom we shall disconnect
  WebMaster *itsMaster;
  //Have we encounter an error yet
  bool itsError;
};

#endif
