//
// Copyright (C) David Brodrick, 2002-2004
// Copyright (C) CSIRO Australia Telescope National Facility
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.
//
// $Id: WebMaster.h,v 1.6 2008/03/05 08:10:19 brodo Exp $

//This class opens a listening socket on the specified TCP port and 
//spawns threaded WebHandler objects, up to some maximum number, when 
//clients connect. Each of these WebHandlers will service the client
//for the duration of the TCP connection.
//
//The underlaying TCPstream class was developed by: Oleg XX & YY ZZ

#ifndef _WEBMASTER_HDR_
#define _WEBMASTER_HDR_

#include <ThreadedObject.h>

class WebHandler;
class ConfigFile;
class StoreMaster;

class WebMaster : public ThreadedObject {
  friend class WebHandler;

public:
  //Default specs for port number and max number of clients is below
  WebMaster(StoreMaster *store, ConfigFile *conf);
  WebMaster(StoreMaster *store, StoreMaster *rawstore, ConfigFile *conf);
  virtual ~WebMaster();

  //Return the ConfigFile to get system information from.
  //This is used by the WebHandlers.
  ConfigFile *getConfig() {return itsConfig;}

private:
  //Main loop for the WebMaster. Here we open a listening socket
  //and spawn new WebHandler objects when a client connects
  void run();
  //Clients call this to indicate when they have exited.
  //We then decrement
  void disconnect(WebHandler*);
  //Reference to the store where all the data is stored
  StoreMaster *itsStore;
  //Reference to the temporary store for raw audio data
  StoreMaster *itsRawStore;
  //The port number we should listen on
  int itsPort;
  //How many client connections we currently have
  int itsNumClients;
  //The maximum number of client connections we are prepared to open
  int itsMaxClients;
  //The ConfigFile to pull system information from
  ConfigFile *itsConfig;
};

#endif
