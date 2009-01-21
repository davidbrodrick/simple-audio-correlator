//
// Copyright (C) David Brodrick
// Copyright (C) CSIRO Australia Telescope National Facility
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.
//
// $Id: $

//The DataForwarder is an optional thread which will forward all of
//our data to a remote data storage/proxy thing. This was developed
//as a way to serve data to the world if your telescope correlation
//computer is stuck behind a firewall (for instance an ATNF observatory)
//and for the purposes of establishing a central archive site.

///The proxy and this should really use a full cryptographic challenge 
///scheme to protect the remote proxy server from third part attacks..

#ifndef _DATAFORWARDER_HDR_
#define _DATAFORWARDER_HDR_

#include <ThreadedObject.h>
#include <TCPstream.h>
#include <TimeCoord.h>
#include <sstream>
#include <time.h>

using namespace::std;

class StoreMaster;
class TCPstream;
class SocketAddr;

class DataForwarder : public ThreadedObject {
public:
  DataForwarder(char *serverhost,  int serverport,
		char *forwardhost, int forwardport);

  ~DataForwarder();

  //Main loop of connecting and then forwarding data
  void run();

private:
  //Ask the proxy server for the timestamp of the last data we sent
  bool getLastSent();

  //Check both network connections and return true if all's good
  //Tries to reconnect if one connection is down
  bool checkConnections();

  //Try to connect to the data server
  bool connectServer();

  //Try to connect to the proxy server
  bool connectProxy();

  //Get some data from the telescope and send it to the proxy server
  void sendData();

  //Play "Catch-up" to send a backlog of data to the proxy
  bool sendBackLog();

  //Send the latest bit of data to the proxy
  bool sendNew();

  //Timestamp of the last data we sent to the proxy server
  timegen_t itsLastSent;

  //Hostname of the proxy server
  char *itsProxyName;
  //Port to connect to the proxy on
  int itsProxyPort;
  //The socket to the proxy server
  int itsProxySock;
  //The TCP connection to our client
  TCPstream itsProxy;
  //Socket reference, contains client address, etc. for Proxy
  SocketAddr itsProxySAddr;

  //Hostname of the telescope data server
  char *itsServerName;
  //Port to use to connect to the data server
  int itsServerPort;
  //The socket to the telescope data server
  int itsServerSock;
  //The TCP connection to the telescope data server
  TCPstream itsServer;
  //Socket reference, contains client address, etc. for telescope
  SocketAddr itsServerSAddr;
};

#endif
