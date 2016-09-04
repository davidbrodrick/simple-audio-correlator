//
// Copyright (C) David Brodrick
// Copyright (C) CSIRO Australia Telescope National Facility
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.
//
//$Id: sacmkwav.cc,v 1.3 2004/04/15 12:25:26 brodo Exp $

//SACMKWAV is for downloading raw audio data and building a .wav file

#include <TCPstream.h>
#include <IntegPeriod.h>
#include <TimeCoord.h>
#include <iostream>
#include <sstream>
#include <cstdlib>

using namespace::std;

int _numdatarequests = 0; //The number of data requests specified by user
string *_datarequests; //Holds all data request command strings
string _server = string("localhost");
int _samprate = 0; //Sampling rate used by the server

//Big ugly thing to parse some cryptic command line arguments.
void parseArgs(int argc, char *argv[]);
//Parse a date string and return the corresponding time
timegen_t parseDate(istringstream &indate);
//Print a usage message and exit
void usage();
//Manages loading of a data request and .wav file generation
void doRequest(istringstream &command, int set);
//Load all data specified in the command
void loadData(istringstream &command, IntegPeriod *&data, int &count);
//Write the given data to a file
void writeData(IntegPeriod *data, int datlen, char *fname);


/////////////////////////////////////////////////////////////////
int main (int argc, char *argv[])
{
  //Parse user arguments and set variables appropriately
  parseArgs(argc, argv);

  //Each data request specifies some data we want to load, process
  //or display
  if (_numdatarequests==0) {
    cerr << "You must specify some data to get with the -F or -T options\n";
    usage();
  }

  for (int i=0; i<_numdatarequests; i++) {
    //Handle each of the specified data sets
    istringstream tmpstr (_datarequests[i]);
    doRequest(tmpstr, i);
  }

  return 0;
}


/////////////////////////////////////////////////////////////////
//Service a request by loading, processing and displaying
void doRequest(istringstream &command, int set)
{
  int count = 0;
  IntegPeriod *data = NULL;

  if (_samprate==0) {
    cerr << "ERROR: No sampling rate was available, cannot write audio file!\n";
    exit(1);
  }

  loadData(command, data, count);
  if (count==0) {
    cerr << "No data was returned for that time!\n";
    return;
  }

  //Write data to file
  writeData(data, count, "data.out");
  cerr << "Server is using " << _samprate << " Hz sampling rate\n";
  IntegPeriod::writeWAVE(data, count, _samprate, "data.wav");
  delete[] data;
}


/////////////////////////////////////////////////////////////////
//Load all data specified in the request command
void loadData(istringstream &command, IntegPeriod *&data, int &count)
{
  count = 0;
  data = NULL;
  IntegPeriod *tempdata;
  int tempcount;
  timegen_t start=0, end=0;
  string str;

  cerr << "STARTING TO BUILD NEW DATA SET\n";
  while (!command.eof()) {
    command >> str;
    if (str=="-T") {
      //we are going to load a time window from the network
      command >> str;
      if (!command.good()) {
        cerr << "Bad argument after -T option (1)\n\n";
        usage();
      }
      if (str=="0") {
        //Treat zero as right now
        start = getAbs();
      } else {
        //otherwise parse the time from argument
        istringstream argdate1(str);
        start = parseDate(argdate1);
      }
      command >> str;
      if (str=="0") {
        //Treat zero as '15 seconds after start time'
        end = start + 15000000ll;
      } else {
        //otherwise parse the time from argument
        istringstream argdate2(str);
        end = parseDate(argdate2);
      }
      cerr << "Loading from network:\n";

      if (!IntegPeriod::loadraw(tempdata, tempcount, start, end,
				_samprate, _server.c_str())) {
        cerr << "Could not obtain requested data, quitting\n";
        exit(1);
      }
    } else if (str=="-F") {
      //we are going to load a file, get the filename
      command >> str;
      if (!command.good()) {
        cerr << "Bad argument after -F option\n\n";
        usage();
      }
      cerr << "Loading data from file \'" << str << "\'";
      if (!IntegPeriod::load(tempdata, tempcount, str.c_str(), true)) {
        cerr << "Could not obtain requested data, quitting\n";
        exit(1);
      }
      if (tempcount>0) {
        start = tempdata[0].timeStamp;
      }
      cerr << "\tOK " << tempcount << " loaded\n";
    } else {
      if (command.eof()) break;
      usage();
    }

    if (tempcount!=0 && count!=0) {
      cerr << "Merging\t";
      IntegPeriod::merge(data, count, data, count, tempdata, tempcount);
      cerr << "DONE\n";
    } else if (tempcount!=0) {
      //First data, no point 'merging'
      data = tempdata;
      count = tempcount;
    }
  }
  cerr << "END DATA SET\n";
}


////////////////////////////////////////////////////////////////////////
//Write the given data to a single file
void writeData(IntegPeriod *data, int datlen, char *fname) {
  ofstream datfile(fname, ios::out|ios::binary);
  for (int i=0; i<datlen; i++) {
    //cout << ts << " " <<
    //    data[i].power1 << " " <<
    //    data[i].power2 << " " <<
    //    data[i].powerX << "\n";
    datfile << data[i];
  }
}


/////////////////////////////////////////////////////////////////
//Prse some cryptic command line arguments
void parseArgs(int argc, char *argv[])
{
  if (argc<3) usage();

  int i=1;
  //Loop throught the arguments
  for (; i<argc; i++) {
    string tempstr;
    tempstr = (argv[i]);

    if (tempstr == "-x") {
      istringstream tmp(argv[i+1]);
      tmp >> _server;
      cerr << "Will load network data from: " << _server << endl;
      i++;
    } else if (tempstr=="-T" || tempstr=="-F") {
      //A sequence of data sources to merge
      ostringstream argcom;
      while (i<argc) {
        if (tempstr=="-T") {
          argcom << " -T ";
          if (argc<i+3) {
            cerr << "Insufficient arguments after -T option\n";
            usage();
          }
          argcom << argv[i+1] << " " << argv[i+2];
          i+=3;
        } else if (tempstr=="-F") {
          argcom << " -F ";
          argcom << argv[i+1] << " ";
          i+=2;
        } else break;
        if (i<argc) tempstr = argv[i];
      }
      i--;
      string *newcommands = new string[_numdatarequests+1];
      for (int j=0; j<_numdatarequests; j++) {
        newcommands[j] = _datarequests[j];
      }
      newcommands[_numdatarequests] = argcom.str();
      _numdatarequests++;
      delete[] _datarequests;
      _datarequests = newcommands;
    } else if (tempstr=="-r") {
      //Specify the sampling rate
      istringstream tmp(argv[i+1]);
      tmp >> _samprate;
      cerr << "Will write data with " << _samprate << " sampling rate\n";
      i++;      
    } else {
      cerr << "Bad argument \'"<< tempstr << "\'\n";
      usage();
    }
  }
  cerr << "Generated " << _numdatarequests << " commands:" << endl;
  for (int j=0; j<_numdatarequests; j++) {
    cerr << _datarequests[j] << endl;
  }
}


/////////////////////////////////////////////////////////////////
//Print a usage message and exit
void usage()
{
  cerr << "\nUSAGE: sackmwav <this needs writing>\n"
    << "See README.sacmkwav for usage details.\n\n.";
  exit(1);
}


/////////////////////////////////////////////////////////////////
//Read date in the format 2002.05.01.23:30:19
timegen_t parseDate(istringstream &indate)
{
  timegen_t bat = 0;
  struct tm brktm;

  int year, month, day, hour, min, sec;
  char temp;

  indate >> year;
  indate >> temp;
  if (temp!='.') {
    if (year==0) return 0;
    else usage();
  }
  brktm.tm_year = year-1900;

  indate >> month;
  indate >> temp;
  if (temp!='.') usage();
  brktm.tm_mon = month-1;

  indate >> day;
  indate >> temp;
  if (temp!='.') usage();
  brktm.tm_mday = day;

  indate >> hour;
  indate >> temp;
  if (temp!=':') usage();
  brktm.tm_hour = hour;

  indate >> min;
  indate >> temp;
  if (temp!=':' || !indate.good()) usage();
  brktm.tm_min = min;

  indate >> sec;
  if (sec<0 || sec>60) usage();
  brktm.tm_sec = sec;

  bat = mktime(&brktm);
  bat*=1000000;

  return bat;
}

