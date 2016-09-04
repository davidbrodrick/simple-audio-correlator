//
// Copyright (C) David Brodrick, 2002
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.
//
// $Id: WebMaster.cc,v 1.5 2004/01/20 11:31:37 brodo Exp $

#include <WebMaster.h>
#include <WebHandler.h>
#include <IntegPeriod.h>
#include <TCPstream.h>
#include <StoreMaster.h>
#include <ConfigFile.h>
#include <iostream>
#include <unistd.h>
#include <signal.h>
#include <assert.h>


WebMaster::WebMaster(StoreMaster *store, ConfigFile *conf)
:ThreadedObject(),
itsStore(store),
itsRawStore(NULL),
itsPort(conf->getServerPort()),
itsNumClients(0),
itsMaxClients(conf->getMaxClients()),
itsConfig(conf)
{
}


WebMaster::WebMaster(StoreMaster *store,
		     StoreMaster *rawstore,
		     ConfigFile *conf)
:ThreadedObject(),
itsStore(store),
itsRawStore(rawstore),
itsPort(conf->getServerPort()),
itsNumClients(0),
itsMaxClients(conf->getMaxClients()),
itsConfig(conf)
{
}


WebMaster::~WebMaster()
{
}

void dontdie(int sig)
{
  //Do nothing, just don't die from SIGPIPE if a connection drops out
}

void WebMaster::run()
{
  //Listen for a connection from anyone
  SocketAddr server_addr = SocketAddr(IPaddress(), itsPort);

  int listening_socket = socket(AF_INET,SOCK_STREAM,0);
  if( listening_socket < 0 )
    CurrentNetCallback::on_error("Socket creation error");

  int value = 1;
  if( setsockopt(listening_socket, SOL_SOCKET, SO_REUSEADDR,
		 (char*)&value, sizeof(value)) < 0 )
    CurrentNetCallback::on_error("setsockopt SO_REUSEADDR error");

  if( bind(listening_socket, (sockaddr *)server_addr,sizeof(server_addr)) < 0 )
    CurrentNetCallback::on_error("Socket binding error");

  //Queue at most 5 clients before refusing connections
  if( listen(listening_socket, 5) < 0 )
    CurrentNetCallback::on_error("Socket listen error");

  //Ensure we don't crash from any broken pipes (SIGPIPE:13)
  signal(13 , dontdie);

  //Main loop
  while (itsKeepRunning) {
    //Check if we want to spawn any more servers
    Lock();
    if (itsNumClients>=itsMaxClients) {
      //We're not interested, wait to allow others to disconnect
      Unlock();
      sleep(1);
      //Go and check again
      continue;
    }
    Unlock();

    //We have one more client, keep track of this
    Lock();
    itsNumClients++;
    assert(itsNumClients>=0&&itsNumClients<=itsMaxClients);
    Unlock();

    //Spawn a new client handler to manage the new connection
    WebHandler *newhandler = new WebHandler(listening_socket,itsStore,
					    itsRawStore,this);
    newhandler->start();
  }

  cerr << "WebMaster Exitting\n";
  close(listening_socket);
  //Kill our thread, we have finished
  itsKeepRunning = false;
}


void WebMaster::disconnect(WebHandler *deadman)
{
  Lock();
  //One less client active now
  itsNumClients--;
  assert(itsNumClients>=0&&itsNumClients<=itsMaxClients);
  Unlock();
  //We have finished with that handler
  /// Probably illegal to delete this than return to it as caller
  delete deadman;
}
