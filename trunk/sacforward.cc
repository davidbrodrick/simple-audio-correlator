//
// Copyright (C) David Brodrick
// Copyright (C) CSIRO Australia Telescope National Facility
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.
//
// $Id: $

//Forward data from a SAC server onto a SAC proxy server

#include <DataForwarder.h>
#include <iostream>
#include <sstream>
#include <unistd.h>
#include <cstdlib>

using namespace::std;

//Name of the SAC server to forward data from, with default
char *_servername = "localhost";
//Port on the server, with default
int _serverport = 31234;
//Name of proxy server
char *_proxyname = NULL;
//Port on proxy server
int _proxyport = 31234;

//Print a usage message and exit
void usage();
//Parse the command line arguments
void parseArgs(int argc, char *argv[]);


/////////////////////////////////////////////////////////////////
int main (int argc, char *argv[])
{
  //Parse user arguments and set variables appropriately
  parseArgs(argc, argv);

  DataForwarder *foo = new DataForwarder(_proxyname, _proxyport,
					 _servername, _serverport);
  foo->start();

  while (1) {
    sleep(10);
  }
}

/////////////////////////////////////////////////////////////////
//Big ugly thing to parse some cryptic command line arguments
void parseArgs(int argc, char *argv[])
{
  if (argc<2) usage();

  _proxyname = argv[1];

  int i=2;
  //Loop throught the arguments
  for (; i<argc; i++) {
    string tempstr;
    tempstr = (argv[i]);

    if (tempstr == "-s") {
      istringstream tmp(argv[i+1]);
      string fname;
      tmp >> fname;
      i++;
    } else if (tempstr == "-pp") {
      //Proxy port
      istringstream tmp(argv[i+1]);
      int p;
      tmp >> p;
      if (tmp.fail() || p<1 || p>65535) {
	cerr << "\n-pp must be followed by a port number between 1-65535";
        usage();
      }
      _proxyport = p;
      i++;
    }
  }
}


/////////////////////////////////////////////////////////////////
//Print a usage message and exit
void usage()
{
  cerr << "\nsacforward: Daemon which forwards SAC data onto a proxy server.\n"
    << "USAGE: sacforward <proxyhost> [-pp <port>][-s <host>][-sp <port>\n"
    << "\t<proxyhost>\tName or IP address of machine to forward data to\n"
    << "\t-pp <port>\tForward data to the specified TCP port on the proxy\n"
    << "\t-s  <host>\tGather the data from the specified host (default localhost)\n"
    << "\t-sp <port>\tGather data from the specified port (default 31234)\n\n";
  exit(1);
}
