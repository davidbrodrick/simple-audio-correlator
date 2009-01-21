//
// Copyright (C) David Brodrick
// Copyright (C) CSIRO Australia
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.
//
// $Id: $

#include <DataForwarder.h>
#include <IntegPeriod.h>
#include <TCPstream.h>
#include <signal.h>
#include <unistd.h> //for sleep

void stayalive(int sig)
{
  //Do nothing, just don't die from SIGPIPE if a connection drops out
}



///////////////////////////////////////////////////////////////////////
//Constructor
DataForwarder::DataForwarder(char *proxyhost, int proxyport,
			     char *serverhost, int serverport)
:itsLastSent(-1),
itsProxyName(proxyhost),
itsProxyPort(proxyport),
itsProxySock(socket(AF_INET,SOCK_STREAM,0)),
itsProxySAddr(IPaddress(itsProxyName), itsProxyPort),
itsServerName(serverhost),
itsServerPort(serverport),
itsServerSock(socket(AF_INET,SOCK_STREAM,0)),
itsServerSAddr(IPaddress(itsServerName), itsServerPort)
{
  ThreadedObject();

  //Ensure we don't crash from any broken pipes (SIGPIPE:13)
  signal(13, stayalive);

  int value = 1;
  if( setsockopt(itsProxySock, SOL_SOCKET, SO_REUSEADDR,
		 (char*)&value, sizeof(value)) < 0 )
    CurrentNetCallback::on_error("setsockopt SO_REUSEADDR error");
  if( setsockopt(itsServerSock, SOL_SOCKET, SO_REUSEADDR,
		 (char*)&value, sizeof(value)) < 0 )
    CurrentNetCallback::on_error("setsockopt SO_REUSEADDR error");


}


///////////////////////////////////////////////////////////////////////
//Destructor
DataForwarder::~DataForwarder()
{  }


///////////////////////////////////////////////////////////////////////
//
void DataForwarder::run()
{
  //Detach our thread
  pthread_detach(itsThread);

  while (itsKeepRunning) {
    if (!checkConnections()) {
      //Wait for a few secods and try to reconnect
      sleep(3);
      continue;
    }

    if (itsLastSent==-1) {
      //We've just connected, need to check the timestamp of the
      //most recent data that we have already forwarded to the proxy
      if (!getLastSent()) {
	//There was an error communicating with the proxy
	continue;
      }
      //If there is a back log of data, go play catch-up
      const timegen_t onehour = 3600000000ll;
      timegen_t timenow = getAbs();
      if (timenow-itsLastSent>onehour) {
	cerr << "Need to catch-up on " << printTime(timenow-itsLastSent)
	     << " worth of data\n";
	if (!sendBackLog()) {
          cerr << "Failed to forward the entire back log of data\n";
	  continue;
	}
      }
    }

    //Send the latest data to the proxy
    sendNew();

    //Wait here until we next need to send data
    sleep(5);
  }
}


///////////////////////////////////////////////////////////////////////
//Check both network connections and return true if all's good
bool DataForwarder::checkConnections()
{
  if (!itsServer.good() || !itsServer.rdbuf()->is_open() || itsServer.eof()) {
    if (!connectServer()) {
      return false;
    }
  }

  if (!itsProxy.good() || !itsProxy.rdbuf()->is_open() || itsProxy.eof()) {
    if (!connectProxy()) {
      return false;
    }
  }
  return true;
}


///////////////////////////////////////////////////////////////////////
//Ask the proxy server for the timestamp of the last data we sent
bool DataForwarder::getLastSent()
{
  //Never send more than this much data
  const timegen_t oneweek = 7*86400000000ll;

  //Check the connection to the server
  if (!itsProxy.good()) return false;

  //Ask the server for the timestamp of the latest data it has
  timegen_t lastsent = 0;
  itsProxy << "GET_LAST\n";
  itsProxy.flush();
  itsProxy >> lastsent;
  //Check if we got the response okay
  if (itsProxy.fail()) {
    cerr << "Lost proxy connection after command \"GET_LAST\"\n";
    return false;
  }

  cout << "Proxy returned \"lastsent\" = " << lastsent << endl;

  timegen_t timenow = getAbs();
  if (timenow==-1 || timenow-lastsent>oneweek) {
    //Limit the quantity of data to send to no more then 1 week
    cerr << "Limiting data request period to ONE WEEK\n";
    lastsent = timenow - oneweek;
  }
  itsLastSent = lastsent;
  return true;
}

///////////////////////////////////////////////////////////////////////
//Try to connect to the data server
bool DataForwarder::connectServer()
{
  cerr << "Trying to connect to data server\n";
  if (itsServer.rdbuf()->is_open()) itsServer.close();

  if (itsServer.rdbuf()->connect(itsServerSAddr)!=NULL) {
    itsServer.rdbuf()->set_blocking_io(true);
    if (itsServer.good() && itsServer.rdbuf()->is_open() && !itsServer.eof()) {
      return true;
    }
  }
  return false;
}


///////////////////////////////////////////////////////////////////////
//Try to connect to the proxy server
bool DataForwarder::connectProxy()
{
  cerr << "Trying to connect to proxy\n";
  if (itsProxy.rdbuf()->is_open()) itsProxy.close();

  if (itsProxy.rdbuf()->connect(itsProxySAddr)!=NULL) {
    itsProxy.rdbuf()->set_blocking_io(true);
    if (itsProxy.good() && itsProxy.rdbuf()->is_open() && !itsProxy.eof()) {
      return true;
    }
  }
  return false;
}


///////////////////////////////////////////////////////////////////////
//Play "Catch-up" to send a backlog of data to the proxy
bool DataForwarder::sendBackLog()
{
  IntegPeriod *thischunk=NULL;
  int thischunksize=0;

  //We do the catching up in 1 hour chunks
  const timegen_t onehour = 3600000000ll;
  timegen_t timenow = getAbs();



  while (timenow-itsLastSent>onehour) {
    if (!itsServer.rdbuf()->is_open()) {
      cerr << "YIKES####";
      return false;
    }

    //Load the data from the telescope data server
    IntegPeriod::load(thischunk, thischunksize,
		      itsLastSent, itsLastSent+onehour,
		      itsServer, false);
    if (!itsServer.rdbuf()->is_open()) {
      cerr << "FOOBAR!!";
      return false;
    }

    //Advance the time counter, whether we got data or not
    itsLastSent += onehour;

    //If we got data, send it on to the proxy server
    if (thischunk!=NULL && thischunksize>0) {
      itsProxy << "SENDING " << thischunksize << endl;
      for (int i=0; i<thischunksize; i++) {
	itsProxy << thischunk[i];
	if (!itsProxy.good() || itsProxy.eof()) {
	  //Reset this so that we ask the server again next time
	  itsLastSent = -1;
	  cerr << "Lost connection to proxy server\n";
          break;
	}
      }
      delete[] thischunk;
      thischunk = NULL;
      thischunksize=0;
    }
  }

  if (!itsProxy.good() || itsProxy.eof()) return false;
  return true;
}


///////////////////////////////////////////////////////////////////////
//Send the latest bit of data to the proxy
bool DataForwarder::sendNew()
{
  cerr << "Trying to send latest data\n";

  IntegPeriod *thischunk=NULL;
  int thischunksize=0;

  //Load the data from the server
  IntegPeriod::load(thischunk, thischunksize,
		    itsLastSent, 0,
		    itsServer, false);
  if (!itsServer.good() || itsServer.eof()) {
    return false;
  }

  //If we got data, send it on to the proxy server
  if (thischunk!=NULL&&thischunksize>0) {
    itsProxy << "SENDING " << thischunksize << endl;
    for (int i=0; i<thischunksize; i++) {
      itsProxy << thischunk[i];
      if (!itsProxy.good() || itsProxy.eof()) {
	//Reset this so that we ask the server again next time
	itsLastSent = -1;
	cerr << "Lost connection to proxy server\n";
	break;
      }
    }
    itsLastSent = thischunk[thischunksize-1].timeStamp;
    delete[] thischunk;
    thischunk = NULL;
    thischunksize=0;
  }

  itsProxy.flush();
  if (!itsProxy.good() || itsProxy.eof()) return false;
  return true;
}

