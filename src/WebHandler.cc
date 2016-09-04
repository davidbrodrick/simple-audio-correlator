//
// Copyright (C) David Brodrick
// Copyright (C) CSIRO Australia
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.
//
// $Id: WebHandler.cc,v 1.9 2005/02/09 09:01:07 brodo Exp $

#include <WebHandler.h>
#include <WebMaster.h>
#include <StoreMaster.h>
#include <IntegPeriod.h>
#include <TCPstream.h>
#include <ConfigFile.h>
#include <RFI.h>
#include <unistd.h> //for sleep
#include <stdlib.h> //for free


///////////////////////////////////////////////////////////////////////
//Constructor
WebHandler::WebHandler(int listening_socket, StoreMaster *store,
		       StoreMaster *rawstore, WebMaster *master)
:ThreadedObject(),
itsSock(listening_socket),
itsClient(),
itsSocket(IPaddress(),1),
itsStore(store),
itsRawStore(rawstore),
itsMaster(master),
itsError(false)
{  }


///////////////////////////////////////////////////////////////////////
//Destructor
WebHandler::~WebHandler()
{  }


///////////////////////////////////////////////////////////////////////
//
void WebHandler::run()
{
  //Detach our thread
  pthread_detach(itsThread);

  //Wait here until we get a new connection
  itsClient.rdbuf()->accept(itsSock,itsSocket);
  itsClient.rdbuf()->set_blocking_io(true);

  cerr << "New Connection from " << itsSocket << endl;

  while (itsKeepRunning && !itsError && itsClient.good()) {
    //Read a command line from the client connection /// timeout?
    char line[1001];
    line[1000] = '\0';
    itsClient.getline(line, 1000);
    if (line[0]=='\0' || line[0]=='\r' || line[0]=='\n') continue;
    cerr << "\nLINE IS: " << line << "\n";
    //Create a string stream from the client's command
    istringstream command(line);
    //Parse and service the command
    serviceCommand(command);
  }

  //Ensure connection is closed and exit thread
  dropConnection();
}


///////////////////////////////////////////////////////////////////////
//Ensure TCP connection is closed and exit our thread
void WebHandler::dropConnection()
{
  itsClient << "\nERROR\n";
  //Ensure TCP link is closed
  itsClient.flush();
  itsClient.close();

  //Print an appropriate message
  if (itsError) {
    cerr << "Lost Connection to " << itsSocket << endl;
  } else {
//    cerr << "Closed Connection to " << itsSocket << endl;
  }

  //Disconnect from the WebMaster, our job is over
  itsKeepRunning = false;
  itsMaster->disconnect(this);
  pthread_exit(NULL);
}


///////////////////////////////////////////////////////////////////////
//Parse the command and service it
void WebHandler::serviceCommand(istringstream &command)
{
  //Read the command from the client
  string directive;
  command >> directive;
  //Check for error
  if (command.fail()) {
    itsError = true;
    return;
  }
  //Check what the client wishes us to do
  if (directive == "BETWEEN") {
    doBetween(command);
  } else if (directive == "RAW-BETWEEN") {
    doRawBetween(command);
  } else if (directive == "AFTER") {
    doAfterASCII(command);
  } else if (directive == "LOCATION") {
    ConfigFile *config = itsMaster->getConfig();
    itsClient << config->getLongitude() << "\t"
              << config->getLatitude() << endl;
  } else if (directive == "VERSION") {
    //Return the server and IntegPeriod version
    itsClient << "SAC 1.1\n";
  } else {
    cerr << directive << endl;
    dropConnection();
  }
}


///////////////////////////////////////////////////////////////////////
//Handle command which wants all data between two argument epochs
void WebHandler::doBetween(istringstream &command)
{
  //Read the argument epochs from the command stream
  unsigned long long sinceepoch = readepoch(command);
  unsigned long long endepoch = readepoch(command);
  if (endepoch<sinceepoch && endepoch!=0) {
    unsigned long long temp = sinceepoch;
    sinceepoch = endepoch;
    endepoch = temp;
  }

  //Read which data the client wishes to be returned
  bool keepcross, keepinputs, keepaudio, cleandata;
  command >> keepcross >> keepinputs >> keepaudio;
  if (command.fail()) {
    itsError = true;
    dropConnection();
  }

  command >> cleandata;
  if (command.fail()) cleandata = false;

  //Get the requested data from the store
  int count;
  IntegPeriod **data = itsStore->get(sinceepoch, endepoch, count);

  if (cleandata && data!=NULL && count>0) {
    IntegPeriod *tempdata = NULL, *tdata=new IntegPeriod[count];
    for (int i=0; i<count; i++) {
      tdata[i] = *data[i];
      delete data[i];
    }
    delete[] data;

    int tempcount;
    cleanData(tempdata, tempcount, tdata, count);
    cerr << "DONE CLEANING";

    count = tempcount;
    data = new IntegPeriod*[count];
    for (int i=0; i<count; i++) data[i] = new IntegPeriod(tempdata[i]);
    delete[] tempdata;
    delete[] tdata;
  }

  //Inform the client how many periods, possibly 0, we will send
  itsClient << count << endl;
  //Ensure we could write to client okay
  if (!itsClient.good()) {itsError=true;}
  //If there was no data we have nothing to send
  if (data!=NULL && !itsError) {
    int i=0;
    //Send each integration period unless there is an error
    for (i=0; i<count && !itsError; i++) {
      //Discard any data which the client doesn't want
      data[i]->keepOnly(keepcross, keepinputs, keepaudio);
      //Send the data to the client
      itsClient << (*data[i]);
      if (!itsClient.good()) {itsError=true;}
    }
    if (!itsClient.good()) {itsError=true;}
  }

  //Clean up data, if there is any
  if (data!=NULL) {
    for (int i=0;i<count; i++) delete data[i];
    delete[] data;
  }
}


///////////////////////////////////////////////////////////////////////
//Handle command which wants all RAW data between two argument epochs
void WebHandler::doRawBetween(istringstream &command)
{
  if (itsRawStore==NULL) {
    cerr << "Cannot service request for RAW data: NO RAW STORE\n";
    itsClient << "0\n";
    return;
  }

  //Read the argument epochs from the command stream
  unsigned long long sinceepoch = readepoch(command);
  unsigned long long endepoch = readepoch(command);
  if (endepoch<sinceepoch && endepoch!=0) {
    unsigned long long temp = sinceepoch;
    sinceepoch = endepoch;
    endepoch = temp;
  }

  //Get the requested data from the RAW store
  int count;
  IntegPeriod **data = itsRawStore->get(sinceepoch, endepoch, count);
  //Inform the client how many periods, possibly 0, we will send
  itsClient << count << endl;
  //Ensure we could write to client okay
  if (!itsClient.good()) {itsError=true;}
  //If there was no data we have nothing to send
  if (data!=NULL && !itsError) {
    //Tell the client what our sampling rate is
    ConfigFile *config = itsMaster->getConfig();
    itsClient << config->getSampRate();
    int i=0;
    //Send each integration period unless there is an error
    for (i=0; i<count && !itsError; i++) {
      //Send the data to the client
      itsClient << (*data[i]);
      if (!itsClient.good()) {itsError=true;}
    }
    if (!itsClient.good()) {itsError=true;}
  }

  //Clean up data, if there is any
  if (data!=NULL) {
    for (int i=0;i<count; i++) delete data[i];
    delete[] data;
  }
}


//Handle command which wants all data after an epoch in ASCII
void WebHandler::doAfterASCII(istringstream &command)
{
  //Read the argument epoch from the command stream
  unsigned long long epoch = readepoch(command);
  if (command.fail()) {
    cerr << "\tFAILED to read epoch" << endl;
    itsError = true;
    dropConnection();
  }

  int numdata = 0;
  IntegPeriod **data = NULL;

  if (epoch==0) {
    IntegPeriod *foo = itsStore->getRecent();
    if (foo!=NULL) {
      numdata = 1;
      data = new IntegPeriod*[1];
      data[0] = foo;
    }
  } else {
    data = itsStore->get(epoch, 0, numdata);
  }

  if (numdata==0 || data==NULL) {
    itsClient << "0\n";
  } else {
    //Tell client how many lines we will return
    itsClient << numdata << endl;
    for (int i=0; i<numdata; i++) {
      itsClient << data[i]->timeStamp << " "
	<< data[i]->power1 << " " << data[i]->power2 << " "
	<< data[i]->powerX << endl;
      //And delete
      delete data[i];
    }
    delete[] data;
  }
}
