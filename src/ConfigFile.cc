//
// Copyright (C) David Brodrick
// Copyright (C) CSIRO Australia Telescope National Facility
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.
//
// $Id: ConfigFile.cc,v 1.8 2004/03/23 12:26:14 brodo Exp $

#include <ConfigFile.h>
#include <iostream>
#include <cstdlib>

#define MAXLINELEN 1001

ConfigFile::ConfigFile(const char *fname)
  :itsDoRealTime(false),
  itsIntegTime(1000),
  itsAudioDev("/dev/dsp"),
  itsSampRate(8000),
  itsStoreDir("/tmp/"),
  itsKeepAudio(false),
  itsKeepSpectra(true),
  itsRawStoreDir("/tmp/sacraw/"),
  itsMaxRawAge(86400000000ll),
  itsStoreRaw(false),
  itsServerPort(31234),
  itsMaxClients(5),
  itsNumBins(64),
  itsLatitude(-30.3147),
  itsLongitude(149.5616),
  itsGain1(1.0),
  itsGain2(1.0),
  itsFile(fname),
  itsLineNum(0)
{
  if (itsFile.fail()) {
    cerr << "ERROR: Couldn't open \"" << fname << "\"\n";
    exit(1);
  }
  parseFile();
}


////////////////////////////////////////////////////////////////////////
//Return the next meaningful line from the file.
istringstream *ConfigFile::nextLine()
{
  char line[MAXLINELEN];
  line[MAXLINELEN-1] = '\0';
  bool done = false;
  while (!done && !itsFile.eof()) {
    itsLineNum++;
    itsFile.getline(line, MAXLINELEN-1);
    /*if (numread == MAXLINELEN-1) {
      cerr << "ERROR: Line " << itsLineNum << " is longer than "
	   << MAXLINELEN-1 << " characters.\n";
      exit(1);
    }*/
    //Check if the line is empty
    //if (numread==0) continue;
    //Check if the line is a comment
    //Should really check first non whitespace char
    if (line[0]=='#' || line[0]=='\0') continue;
    //Otherwise we found a valid line
    done = true;
  }
  return new istringstream(line);
}


////////////////////////////////////////////////////////////////////////
//Parses each line from the file and uses the info to specify
//values for the appropriate fields.
void ConfigFile::parseFile()
{
  while (!itsFile.eof()) {
    istringstream *line = nextLine();
    string key;
    *line >> key;
    if (key=="realtime:") {
      string val;
      *line >> val;
      if (val=="true") itsDoRealTime = true;
      else if (val=="false") itsDoRealTime = false;
      else {
	cerr << "ERROR: Line " << itsLineNum << ": \"realtime:\" expects "
	  << "a \"true\" or \"false\" argument\n";
        exit(1);
      }
    } else if (key=="integtime:") {
      int val;
      *line >> val;
      if (val<=0 || val>5000) {
	cerr << "ERROR: Line " << itsLineNum << ": \"integtime:\" expects "
	  << "a value between 1 and 5000\n";
	exit(1);
      }
      itsIntegTime = val;
    } else if (key=="storedir:") {
      *line >> itsStoreDir;
    } else if (key=="rawstoredir:") {
      *line >> itsRawStoreDir;
    } else if (key=="storeraw:") {
      string val;
      *line >> val;
      if (val=="true") itsStoreRaw = true;
      else if (val=="false") itsStoreRaw = false;
      else {
	cerr << "ERROR: Line " << itsLineNum << ": \"storeraw:\" expects "
	  << "a \"true\" or \"false\" argument\n";
        exit(1);
      }
    } else if (key=="maxrawage:") {
      *line >> itsMaxRawAge;
      if (itsMaxRawAge<60 || itsMaxRawAge>86400*7) {
        cerr << "ERROR: Line " << itsLineNum << ": \"maxrawage:\" expects "
	  << "a value between 60 (1 minute) and\n"
	  << "604800 (1 week). If you need a longer period you can "
	  << "make the main data\nstore record raw audio using the "
	  << "\'saveraw\' keyword\n";
        exit(1);
      }
      itsMaxRawAge*=1000000; //Convert from seconds to microseconds
    } else if (key=="audiodev:") {
      *line >> itsAudioDev;
    } else if (key=="samprate:") {
      int val;
      *line >> val;
      if (val<4000 || val>48000) {
	cerr << "ERROR: Line " << itsLineNum << ": \"samprate:\" expects "
	  << "a value between 4000 and 48000\n";
	exit(1);
      }
      itsSampRate = val;
    } else if (key=="port:") {
      int val;
      *line >> val;
      if (val<1 || val>65535) {
	cerr << "ERROR: Line " << itsLineNum << ": \"port:\" expects "
	  << "a port number between 1 and 65535\n";
	exit(1);
      }
      itsServerPort = val;
    } else if (key=="maxclients:") {
      int val;
      *line >> val;
      if (val<0 || val>1000) {
	cerr << "ERROR: Line " << itsLineNum << ": \"maxclients:\" expects "
	  << "an argument between 0 and 1000\n";
	exit(1);
      }
      itsMaxClients = val;
    } else if (key=="numbins:") {
      int val;
      *line >> val;
      if (val<4 || val>4096) {
	cerr << "ERROR: Line " << itsLineNum << ": \"numbins:\" expects "
	  << "a value between 4 and 4096\n";
	exit(1);
      }
      itsNumBins = val;
    } else if (key=="savespec:") {
      string val;
      *line >> val;
      if (val=="true") itsKeepSpectra = true;
      else if (val=="false") itsKeepSpectra = false;
      else {
	cerr << "ERROR: Line " << itsLineNum << ": \"savespec:\" expects "
	  << "a \"true\" or \"false\" argument\n";
        exit(1);
      }
    } else if (key=="saveraw:") {
      string val;
      *line >> val;
      if (val=="true") itsKeepAudio = true;
      else if (val=="false") itsKeepAudio = false;
      else {
	cerr << "ERROR: Line " << itsLineNum << ": \"saveraw:\" expects "
	  << "a \"true\" or \"false\" argument\n";
        exit(1);
      }
    } else if (key=="latitude:") {
      float val;
      *line >> val;
      if (val<-90.0 || val>90.0) {
	cerr << "ERROR: Line " << itsLineNum << ": \"latitude:\" expects "
	  << "a value in degrees,\n"
	  << "North is positive (up to 90) and South is negative (down to -90)\n";
	exit(1);
      }
      itsLatitude = val;
    } else if (key=="longitude:") {
      float val;
      *line >> val;
      if (val<-180.0 || val>180.0) {
	cerr << "ERROR: Line " << itsLineNum << ": \"longitude:\" expects "
	  << "a value in degrees,\n"
	  << "East is positive (up to 180) and West is negative (down to -180)\n";
	exit(1);
      }
      itsLongitude = val;
    } else if (key=="gain1:") {
      *line >> itsGain1;
    } else if (key=="gain2:") {
      *line >> itsGain2;
    } else {
      cerr << "Unknown meaning, line " << itsLineNum << " starts: "
	   << key << endl;
    }
  }
}
