//
// Copyright (C) David Brodrick
// Copyright (C) CSIRO Australia Telescope National Facility
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.
//
// $Id: $

#include <PlotArea.h>
#include <RFI.h>
#include <IntegPeriod.h>
#include <TimeCoord.h>
#include <Site.h>
#include <Source.h>
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

bool _LST = false; //Recalculate all timestamps to a sidereal time?
bool _rfi = true; //Should we perform RFI processing
timegen_t _inttime = 60000000; //Default integration time if RFI processing
float _sigma = 1.2; //How many std dev from mean is to be considered RFI
float _noiselimit = 0; //How large can std dev be to mean else flag as RFI

int _numsources = 0;
Source **_sources = NULL;
Site *_site = NULL;

//Print a usage message and exit
void usage();

/////////////////////////////////////////////////////////////////
int main (int argc, char *argv[])
{
  ///LOOP THROUGH COMMAND LINE ARGUMENTS AND COUNT NUMBER OF SOURCES
  for (int i=1; i<argc; i++)
    if (strcmp(argv[i], "-s")==0) _numsources++;
  if (_numsources==0) usage();
  _sources = new Source*[_numsources];
  _numsources = 0;

  ///THE FIRST ARGUMENT SHOULD DEFINE THE SITE TO SIMULATE
  string arg1 = string(argv[1]);
  _site = Site::parseSite(arg1);
  if (_site==NULL) {
    exit(1);
  }

  ///GO THROUGH THE REST OF THE ARGUMENTS PARSING SOURCES
  for (int i=2; i<argc-1; i++) {
    if (strcmp(argv[i], "-s")==0) {
      string arg = string(argv[i+1]);
      _sources[_numsources] = AstroPointSource::parseSource(arg);
      if (_sources[_numsources]==NULL) {
        exit(1);
      }
      _numsources++;
    }
  }

  ///ALLOCATE THE MEMORY FOR OUR RESULT DATA
  const int numperiods = 8640; //Every 10s
  IntegPeriod *data = new IntegPeriod[numperiods];
  for (int i=0; i<numperiods; i++) {
    data[i].timeStamp = (timegen_t)((i/float(numperiods-1))*86400000000.0);
    data[i].power1 = 0.0;
    data[i].power2 = 0.0;
    data[i].powerX = 0.0;
    data[i].phase = 0.0;
    data[i].amplitude = 0.0;
  }

  ///CALCULATE THE RESPONSE OF THE INTERFEROMETER
  for (int i=0; i<_numsources; i++) {
    _site->getResponse(data, numperiods, *_sources[i]);
  }

  ///WRITE THE OUTPUT FILE
  ofstream datfile("sim.out", ios::out|ios::binary);
  for (int i=0; i<numperiods; i++) datfile << data[i];
  
  //_savefile="/tmp/foo.png";
  //_display=png;
  ///FINALLY, GO AND DRAW THE GRAPHS
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

  if (cpgbeg(0, out.str().c_str(), 1, 3) != 1)
    exit(1);
  cpgsvp(0.05,0.95,0.1,0.9);
  PlotArea::setPopulation(3);
  cpgsch(1.2);

  float temp[numperiods];
  float timedata[numperiods];
  float max = 0;
  float min = 0;
  for (int i=0; i<numperiods;i++) {
    temp[i] = data[i].powerX;
    if (temp[i]>max) max=temp[i];
    if (temp[i]<min) min=temp[i];
    timedata[i] = data[i].timeStamp/float(3600000000.0);
  }
  PlotArea *x = PlotArea::getPlotArea(0);
  x->setTitle("In-phase Channel Fringes");
  x->setAxisY("Power", max, min, false);
  x->setAxisX("Time (Hours)", timedata[numperiods-1], timedata[0], false);
  x->plotLine(numperiods, timedata, temp, 2);

  max = 0;
  for (int i=0; i<numperiods;i++) {
    temp[i] = data[i].amplitude;
    if (temp[i]>max) max=temp[i];
  }
  x = PlotArea::getPlotArea(1);
  x->setTitle("Amplitude");
  x->setAxisY("Power", max, 0, false);
  x->setAxisX("Time (Hours)", timedata[numperiods-1], timedata[0], false);
  x->plotLine(numperiods, timedata, temp, 4);

  for (int i=0; i<numperiods;i++) {
    temp[i] = data[i].phase;
  }
  x = PlotArea::getPlotArea(2);
  x->setTitle("Phase");
  x->setAxisY("Phase", PI, -PI, false);
  x->setAxisX("Time (Hours)", timedata[numperiods-1], timedata[0], false);
  x->plotPoints(numperiods, timedata, temp, 3);

  //Close the pgplot device
  cpgclos();

  return 0;
}


/////////////////////////////////////////////////////////////////
//Print a usage message and exit
void usage()
{
  cerr << "\nUSAGE: sacsim <site_def> -s <source_def1> -s <source_defN>\n";
  cerr << " This models the interferometer amplitude and phase reponse to a number of\n";
  cerr << " astronomical radio sources.\n\n";
  cerr << endl;
  cerr << " The site definition must come first in the format \"lon lat EW NS F phi\"\n";
  cerr << " (the quotes are required). Where:\n";
  cerr << "  lon\tObserving site longitude in degrees (east is positive)\n";
  cerr << "  lat\tObserving site latitude in degrees (north is positive)\n";
  cerr << "  EW\tEast-west baseline component, in metres\n";
  cerr << "  NS\tNorth-south baseline component, in metres\n";
  cerr << "  F\tObserving frequency in MHz\n";
  cerr << "  phi\tphase offset, in degrees\n";
  cerr << endl;
  cerr << " Then comes however many source definitions you wish to include. Each of\n";
  cerr << " these has the form -s \"RA Dec Flux [Size]\" (again the quotes are required).\n";
  cerr << " Where:\n";
  cerr << "  RA\tRight ascension of the radio source, in hours\n";
  cerr << "  Dec\tDeclination of the source, in degrees (north is positive)\n";
  cerr << "  Flux\tSource flux, in arbitrary units, Janskies if you like\n";
  cerr << "  Size\tOptional source radius in degrees, otherwise point source assumed\n";
  cerr << endl;
  exit(1);
}


