//
// Copyright (C) David Brodrick
// Copyright (C) CSIRO Australia Telescope National Facility
// Copyright (C) Swinburne University of Technology
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.
//
// $Id:  $

#include <TimeCoord.h>
#include <SolarFlare.h>
#include <PlotArea.h>
#include <iostream>
#include <stdlib.h>
#include <sstream>
#include <iomanip>
#include <time.h>
#include <sys/time.h>
#include <math.h>
#include <assert.h>

extern "C" {
#include <cpgplot.h>
    int main(int argc, char *argv[]);
}

using namespace::std;

//Different ways we can output the graph.
typedef enum displayT {
  screen,
  postscript,
  png,
  dump
} displayT;

displayT _display = screen; //Output device to display data to
char *_savefile = NULL; //File to write any output graphs to (if not screen)

int _numflares = 0; //Number of solar flares being processed
SolarFlare *_flares = NULL; //Pointers to the data from each file
char **_flarenames = NULL;

//Print a usage message and exit
void usage();
//Parse the config file and load raw data for this flare
void loadFlare(int num);

/////////////////////////////////////////////////////////////////
int main (int argc, char *argv[])
{
  ostringstream out;
  switch (_display) {
  case screen:
    out << "/XS";
    break;
  case postscript:
    out << _savefile << "/CPS";
    break;
  case png:
    out << _savefile << "/PNG";
    break;
  case dump:
    out << _savefile << "/VWD";
    break;
  default:
    assert(false);
    break;
  }

  if (cpgbeg(0, out.str().c_str(), 3, 1) != 1)
    exit(1);
  cpgsvp(0.05,0.95,0.1,0.9);
  PlotArea::setPopulation(3);
  cpgsch(1.2);

  _numflares = argc-1;
  _flares = new SolarFlare[_numflares];
  _flarenames = new char*[_numflares];
  for (int i=1; i<argc; i++) {
    _flarenames[i-1] = argv[i];
    loadFlare(i-1);
  }

  return 0;
}


//////////////////////////////////////////////////////////////////
//Parse the config file and load raw data for this flare
void loadFlare(int num)
{
  ifstream tfile(_flarenames[num], ios::in);
  if (!tfile.good()) {
    cerr << "ERROR opening " << _flarenames[num] << endl;
    exit(1);
  }

  _flares[num] = SolarFlare();

  char line[1001];
  line[1000] = '\0';
  tfile.getline(line, 1000);
  istringstream l(line);
  float lat, lon;
  l >> lon >> lat;
  if (lon>180.0 || lon<-180.0 || lat<-90 || lat>90) {
    cerr << "ERROR reading site location\n";
    exit(1);
  }
  lon=PI*lon/180.0;
  lat=PI*lat/180.0;
  _flares[num].setSite(lon, lat);

  tfile.getline(line, 1000);
  string radiometer_file = string(line);
  int channel = 1;
  tfile.getline(line, 1000);
  if (line[0]=='1') {
    channel = 1;
  } else if (line[0]=='2') {
    channel = 2;
  } else {
    cerr << "ERROR reading radiometer channel\n";
    exit(1);
  }
  tfile.getline(line, 1000);
  istringstream q1(line);
  float noise = 0.0;
  q1 >> noise;
  _flares[num].loadRadiometer(radiometer_file, channel, noise);

  tfile.getline(line, 1000);
  string reference_file = string(line);
  tfile.getline(line, 1000);
  if (line[0]=='1') {
    channel = 1;
  } else if (line[0]=='2') {
    channel = 2;
  } else {
    cerr << "ERROR reading reference channel\n";
    exit(1);
  }
  tfile.getline(line, 1000);
  istringstream q2(line);
  noise = 0.0;
  q2 >> noise;
  if (!_flares[num].loadReference(reference_file, channel, noise)) {
    cerr << "ERROR reading reference data file \"" << reference_file << "\"\n";
    exit(1);
  }

  tfile.getline(line, 1000);
  istringstream n(line);
  int numpairs = 0;
  n >> numpairs;
  timegen_t refcals[numpairs*2];
  if (numpairs<=0) {
    cerr << "ERROR reading number of reference calibration times\n";
    exit(1);
  }
  for (int i=0; i<numpairs; i++) {
    tfile.getline(line, 1000);
    istringstream q(line);
    string t1, t2;
    q >> t1 >> t2;
    refcals[2*i] = parseTime(t1);
    refcals[2*i+1] = parseTime(t2);
  }
  _flares[num].setReferenceCalTimes(refcals, 2*numpairs);
  _flares[num].calibrateReference();
  _flares[num].displayRadiometer(0);
  _flares[num].getAbsorption();
  //_flares[num].displayAbsorption(1);
  _flares[num].getElevation();
  _flares[num].displayElevation(1);
  _flares[num].correctChapman();
  //_flares[num].displayCorrected(3);

  tfile.getline(line, 1000);
  string goes_file = string(line);
  if (!_flares[num].loadGOES(goes_file)) {
    cerr << "ERROR reading GOES data file \"" << goes_file << "\"\n";
    exit(1);
  }
  //_flares[num].displayGOES(2);

  tfile.getline(line, 1000);
  istringstream w(line);
  numpairs = 0;
  w >> numpairs;
  timegen_t goescals[numpairs*2];
  if (numpairs<=0) {
    cerr << "ERROR reading number of GOES calibration times\n";
    exit(1);
  }
  for (int i=0; i<numpairs; i++) {
    tfile.getline(line, 1000);
    istringstream q(line);
    string t1, t2;
    q >> t1 >> t2;
    goescals[2*i]   = parseTime(t1);
    goescals[2*i+1] = parseTime(t2);
  }
  _flares[num].setGOESCalTimes(goescals, numpairs*2);
  _flares[num].calibrateGOES();
  _flares[num].displayFitAndGOES(2);
  cout << _flarenames[num] << ":\t";
  cout << "Alpha=" << _flares[num].getAlpha();
  cout << "\tN=" << _flares[num].getN();
  cout << "\tTau=" << _flares[num].getTau()/1000000.0;
  cout << endl;
}


//////////////////////////////////////////////////////////////////
//Print a much needed error message and exit
void usage()
{
  cerr << endl;
  exit(1);
}
