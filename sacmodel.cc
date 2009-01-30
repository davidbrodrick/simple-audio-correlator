//
// Copyright (C) David Brodrick
// Copyright (C) CSIRO Australia Telescope National Facility
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.
//
// $Id:  $


#include <PlotArea.h>
#include <RFI.h>
#include <TCPstream.h>
#include <Site.h>
#include <IntegPeriod.h>
#include <TimeCoord.h>
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

int _numfiles = 0; //Number of different data, baselines, systems, etc
IntegPeriod **_data = NULL; //Pointers to the data from each file
int *_numdata = NULL; //Number of data in each file
Site **_sites = NULL; //Pointers to the info about receiving stations

int _numsources = 0;
ExtendedSource **_sources = NULL;

//Buffer of the response of each system to each source
IntegPeriod ***_cache = NULL;

//Print a much needed error message and exit
void usage();

ExtendedSource *replaceSource(int srcnum,
			      float maxra, float minra, int numra,
			      float maxdec, float mindec, int numdec,
			      bool useheuristics=false);

/////////////////////////////////////////////////////////////////
int main (int argc, char *argv[])
{
  if (argc==1) usage();

  //Loop through command line arguments and count number of file args
  for (int i=1; i<argc; i++) {
    if (strcmp(argv[i], "-F")==0) _numfiles++;
    else if (strcmp(argv[i], "-f")==0) _numfiles++;
    else if (strcmp(argv[i], "-file")==0) _numfiles++;
    else if (strcmp(argv[i], "--file")==0) _numfiles++;
  }
  if (_numfiles==0) {
    usage();
    exit(1);
  }
  //Allocate the data structures for each set of data
  _data = new IntegPeriod*[_numfiles];
  _cache = new IntegPeriod**[_numfiles];
  _sites = new Site*[_numfiles];
  _numdata = new int[_numfiles];
  for (int i=0; i<_numfiles; i++) {
    _data[i] = NULL;
    _sites[i] = NULL;
    _numdata[i] = 0;
  }

  //Load the data and info about each system
  int currdata = 0;
  for (int i=0; i<argc; i++) {
    string arg = string(argv[i]);
    if (arg=="-F" || arg=="-f" || arg=="-file" || arg=="--file") {
      if (argc<i+8) {
	if (argc>i+1) cerr << "After \"" << argv[i+1] << "\" argument:\n";
	usage();
      }
      i++;
      //First is meant to come the file name
      bool badload = !IntegPeriod::load(_data[currdata],
					_numdata[currdata], argv[i]);
      if (badload || _data[currdata]==NULL || _numdata[currdata]==0) {
	cerr << "I just tried to load \"" << argv[i] << "\" and had no luck.\n";
	usage();
      }
      cout << "Loaded " << _numdata[currdata] << " from " << argv[i] << endl;
      //Build a string out of the next 6 arguments
      string arg;
      for (int j=i+1; j<i+7; j++) {
	arg += argv[j];
	arg += " ";
      }
      _sites[currdata] = Site::parseSite(arg);
      if (_sites[currdata]==NULL) exit(1);
      i+=6;

      //Apply the phase shift
      /*for (int j=0; j<_numdata[currdata]; j++) {
	_data[currdata][j].phase -= _sites[currdata]->getPhaseOffset();
	if (_data[currdata][j].phase<-PI)
	  _data[currdata][j].phase = 2*PI+_data[currdata][j].phase;
	else if (_data[currdata][j].phase>PI)
	  _data[currdata][j].phase = -2*PI+_data[currdata][j].phase;
      }*/

      currdata++;
    }
  }

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

  if (cpgbeg(0, out.str().c_str(), 2, 2) != 1)
    exit(1);
  cpgsvp(0.05,0.95,0.1,0.9);
  PlotArea::setPopulation(4);
  cpgsch(1.2);

  PlotArea *x = PlotArea::getPlotArea(0);
  x->setTitle("Observed Amplitudes");
  x->setAxisY("Amplitude", 50000, 0, false);
  x->setAxisX("Time (Hours)", 24, 0, false);

  for (int sys=0; sys<_numfiles; sys++) {
    float temp[_numdata[sys]];
    float timedata[_numdata[sys]];
    for (int i=0; i<_numdata[sys]; i++) {
      temp[i] = _data[sys][i].amplitude;
      timedata[i] = _data[sys][i].timeStamp/float(3600000000.0);
    }
    x->plotPoints(_numdata[sys], timedata, temp, 3+sys);
  }

  x = PlotArea::getPlotArea(1);
  x->setTitle("Observed Phases");
  x->setAxisY("Phase", PI, -PI, false);
  x->setAxisX("Time (Hours)", 24, 0, false);

  for (int sys=0; sys<_numfiles; sys++) {
    float temp[_numdata[sys]];
    float timedata[_numdata[sys]];
    for (int i=0; i<_numdata[sys]; i++) {
      temp[i] = _data[sys][i].phase;
      timedata[i] = _data[sys][i].timeStamp/float(3600000000.0);
    }
    x->plotPoints(_numdata[sys], timedata, temp, 3+sys);
  }

  _numsources = 15;
  _sources = new ExtendedSource*[_numsources];
  pair_t startpos = {PI,0};
  for (int i=0; i<_numsources; i++) {
    _sources[i] = new ExtendedSource(startpos, 0, 0);
  }

  for (int i=0; i<_numfiles; i++) {
    _cache[i] = new IntegPeriod*[_numsources];
    for (int j=0; j<_numsources; j++) {
      _cache[i][j] = new IntegPeriod[_numdata[i]];
    }
  }

  int placement = 1;
  int ragrid = 70;
  int decgrid = 40;
  int numloops = 0;
  bool useheuristics = true;
  while (1) {
    if (numloops==3) {
      numloops=0;
      //knock out a random source
      int src = (int)(_numsources*(float(rand())/RAND_MAX));
      cout << "Randomly knocking out source {" << printRADec(_sources[src]->getPosition()) << " " << 180*_sources[src]->getRadius()/PI << "deg " << _sources[src]->getFlux() << "}\n";
      _sources[src]->setFlux(0.0);
    }
    numloops++;

    for (int src=0; src<_numsources; src++) {
      ExtendedSource *newsrc = replaceSource(src, 2*PI, 0, ragrid,
					     -10*PI/180, -70*PI/180,
					     decgrid, useheuristics);
      delete _sources[src];
      _sources[src] = newsrc;

      cout << "SUMMARY(" << placement << "): ";
      for (int src=0; src<_numsources; src++) {
	if (_sources[src]->getFlux(0)!=0.0)
	  cout << "{" << printRADec(_sources[src]->getPosition()) << " " << 180*_sources[src]->getRadius()/PI << "deg " << _sources[src]->getFlux() << "}\t";
      }
      cout << endl;
      placement++;
    }
    ragrid = 180;
    decgrid = 90;
    if (numloops>1) useheuristics = false;
  }

  return 0;
}


//////////////////////////////////////////////////////////////////
//Get the RA of the period with the highest residual amplitude error
float suggestRA(IntegPeriod **testdata)
{
  const int len = 864;
  float eacherr[len];
  int numcontrib[len];
  for (int i=0; i<len; i++) {
    eacherr[i]=0.0;
    numcontrib[i]=0;
  }

  for (int sys=0; sys<_numfiles; sys++) {
    for (int i=0; i<_numdata[sys]; i++) {
      //Only suggest this if the test data is lower in flux
      if (testdata[sys][i].amplitude>=_data[sys][i].amplitude) continue;
      float ierr = _data[sys][i].amplitude*cos(_data[sys][i].phase)
	- testdata[sys][i].amplitude*cos(testdata[sys][i].phase);
      float qerr = _data[sys][i].amplitude*sin(_data[sys][i].phase)
	- testdata[sys][i].amplitude*sin(testdata[sys][i].phase);
      float thiserr = sqrt(ierr*ierr + qerr*qerr);
      //Find the right LST/RA bin for this data point   
      int index = (int)(len*(float(testdata[sys][i].timeStamp)/86400000000ll));
      eacherr[index] += thiserr;
      numcontrib[index]++;
    }
  }

  //Normalise each bin
  for (int i=0; i<len; i++) {
    if (numcontrib[i]!=0) eacherr[i]/=numcontrib[i];
  }

  //Find the maximum
  float maxerr = 0;
  int bestra = 0;
  for (int i=0; i<len; i++) {
    if (maxerr==0 || eacherr[i]>maxerr) {
      bestra = i;
      maxerr = eacherr[i];
    }
  }

  float res = 2*PI*(float(bestra)/len);
  cout << "suggested RA = " << 12*res/PI << endl;
  return res;
}


//////////////////////////////////////////////////////////////////
//Get a figure of fit for the given data with the observed data
float assessFit(IntegPeriod **testdata)
{
  double sum = 0.0;
  for (int sys=0; sys<_numfiles; sys++) {
    double thissum = 0.0;
    for (int i=0; i<_numdata[sys]; i++) {
      float ierr = _data[sys][i].amplitude*cos(_data[sys][i].phase)
	- testdata[sys][i].amplitude*cos(testdata[sys][i].phase);
      float qerr = _data[sys][i].amplitude*sin(_data[sys][i].phase)
	- testdata[sys][i].amplitude*sin(testdata[sys][i].phase);
      thissum += ierr*ierr + qerr*qerr;
    }
    sum += sqrt(thissum/_numdata[sys]);
  }
  return sum/_numfiles;
}

//////////////////////////////////////////////////////////////////
//Find a new source that does better than this old source
ExtendedSource *replaceSource(int srcnum,
			      float maxra, float minra, int numra,
			      float maxdec, float mindec, int numdec,
			      bool useheuristics)
{
  //Just a check to make life easy
  if (maxdec<mindec) {
    float temp = mindec;
    mindec = maxdec;
    maxdec = temp;
  }

  float rares = (maxra-minra)/numra;
  float decres = (maxdec-mindec)/numdec;
  float mapres;
  if (decres>rares) mapres = rares;
  else mapres = decres;

  //Need to get response of each system to the sky as it would be
  //without this source
  IntegPeriod *withoutsrc[_numfiles];
  IntegPeriod *onlysrc[_numfiles];
  IntegPeriod *tempdata[_numfiles];
  for (int i=0; i<_numfiles; i++) {
    withoutsrc[i] = new IntegPeriod[_numdata[i]];
    onlysrc[i] = new IntegPeriod[_numdata[i]];
    tempdata[i] = new IntegPeriod[_numdata[i]];
  }
  for (int sys=0; sys<_numfiles; sys++) {
    for (int i=0; i<_numdata[sys]; i++) {
      withoutsrc[sys][i].timeStamp = _data[sys][i].timeStamp;
    }
  }

  //cerr << "Computing site responses without source #" << srcnum << endl;
  for (int sys=0; sys<_numfiles; sys++) {
    for (int src=0; src<_numsources; src++) {
      if (src==srcnum || _sources[src]->getFlux(0)==0.0) continue;
      _sites[sys]->getResponse(withoutsrc[sys], _numdata[sys], *_sources[src]);
    }
  }

  float rastart = minra;
  float raend = maxra;
  double rastep = (maxra-minra)/numra;
  double decstep = (maxdec-mindec)/numdec;
  float bestra = 0.0;
  float bestdec = 0.0;
  float bestfit = -1;

  if (useheuristics) {
    float suggested = suggestRA(withoutsrc);
    rastart = suggested - PI*3.5/12.0;
    if (rastart<0) rastart=0.0;
    raend   = suggested + PI*3.5/12.0;
    if (raend>2*PI) raend=2*PI;
  }

  for (double ra=rastart; ra<=raend; ra=ra+rastep) {
    cerr << 12*ra/PI << "\t" << 12*bestra/PI << "\t" << 180*bestdec/PI << endl;
    for (double dec=mindec; dec<=maxdec; dec=dec+decstep) {
      //Have to reinitialise the system responses
      for (int i=0; i<_numfiles; i++) {
	for (int j=0; j<_numdata[i]; j++) {
	  onlysrc[i][j].clear();
          onlysrc[i][j].timeStamp = withoutsrc[i][j].timeStamp;
	  tempdata[i][j] = withoutsrc[i][j];
	}
      }

      //Create the new trial source
      pair_t radec = {ra, dec};
      AstroPointSource trialsrc(radec, 1000);

      //Then go and calculate the responses to the new trial position
      for (int sys=0; sys<_numfiles; sys++) {
	_sites[sys]->getResponse(onlysrc[sys], _numdata[sys], trialsrc);
      }

      //Combine this, and the rest of the sources
      for (int sys=0; sys<_numfiles; sys++) {
	for (int i=0; i<_numdata[sys]; i++) {
          tempdata[sys][i] += onlysrc[sys][i];
	}
      }

      //Measure the fit for this position
      float fit = assessFit(tempdata);
      //cerr << fit << endl;

      //If it's better, keep it
      if (fit<bestfit || bestfit==-1) {
	bestfit = fit;
	bestra = ra;
	bestdec = dec;

	PlotArea *x = PlotArea::getPlotArea(2);
	x->setTitle("Modelled Amplitudes");
	x->setAxisY("Amplitude", 50000, 0, false);
	x->setAxisX("Time (Hours)", 24, 0, false);

	for (int sys=0; sys<_numfiles; sys++) {
	  float temp[_numdata[sys]];
	  float timedata[_numdata[sys]];
	  for (int i=0; i<_numdata[sys]; i++) {
	    temp[i] = tempdata[sys][i].amplitude;
	    timedata[i] = tempdata[sys][i].timeStamp/float(3600000000.0);
	  }
	  x->plotPoints(_numdata[sys], timedata, temp, 3+sys);
	}

	x = PlotArea::getPlotArea(3);
	x->setTitle("Modelled Phases");
	x->setAxisY("Phase", PI, -PI, false);
	x->setAxisX("Time (Hours)", 24, 0, false);

	for (int sys=0; sys<_numfiles; sys++) {
	  float temp[_numdata[sys]];
	  float timedata[_numdata[sys]];
	  for (int i=0; i<_numdata[sys]; i++) {
	    temp[i] = tempdata[sys][i].phase;
	    timedata[i] = tempdata[sys][i].timeStamp/float(3600000000.0);
	  }
	  x->plotPoints(_numdata[sys], timedata, temp, 3+sys);
	}
      }
    }
  }

  pair_t newpos = {bestra, bestdec};
  ExtendedSource *res = new ExtendedSource(newpos, 1000, mapres/2);

  bestfit  = 0;
  float bestsize = 0;
  float bestflux = 0;
  float lastsize = -1;

  for (float size=mapres/2.0; size<PI*8.0/180.0; size+=mapres/2.0) {
    if (size>mapres/2.0 && lastsize!=-1 && bestsize!=lastsize) break;
    lastsize=size;
    res->setRadius(size);
    float lastfit = 0.0;
    for (float flux=1000; flux<15001; flux+=1000) {
      res->setFlux(flux);
      //Then go and calculate the response for this trial source
      for (int sys=0; sys<_numfiles; sys++) {
	_sites[sys]->getResponse(onlysrc[sys], _numdata[sys], *res);
      }

      //Combine this, and the rest of the sources
      for (int sys=0; sys<_numfiles; sys++) {
	for (int i=0; i<_numdata[sys]; i++) {
          tempdata[sys][i] += onlysrc[sys][i];
	}
      }

      //Measure the fit for this position
      float fit = assessFit(tempdata);

      //If this flux gives an inferior fit then no point trying higher fluxes
      if (fit>lastfit && lastfit!=0) break;
      lastfit = fit;

      if (bestfit==0 || fit<bestfit) {
	bestfit = fit;
	bestsize = size;
	bestflux = flux;
      }

      cout << flux << "\t" << 180*size/PI << "\t" << bestflux << "\t" << 180*bestsize/PI << endl;

      //Have to reinitialise the system responses
      for (int i=0; i<_numfiles; i++) {
	for (int j=0; j<_numdata[i]; j++) {
	  onlysrc[i][j].clear();
	  onlysrc[i][j].timeStamp = withoutsrc[i][j].timeStamp;
	  tempdata[i][j] = withoutsrc[i][j];
	}
      }
    }
  }
  res->setRadius(bestsize);
  res->setFlux(bestflux);

  for (int i=0; i<_numfiles; i++) {
    delete[] withoutsrc[i];
    delete[] onlysrc[i];
    delete[] tempdata[i];
  }

  return res;
}


//////////////////////////////////////////////////////////////////
//Print a much needed error message and exit
void usage()
{
  cerr << "This takes a number of complex visibility files generated by saciq and\n";
  cerr << "tries to determine the positions of the radio sources on the sky which\n";
  cerr << "would produce that data.\n\n"; 
  cerr << "Each file must start with a -f and needs the following arguments:\n";
  cerr << "Data file name\n";
  cerr << "longitude, in degrees, East is +ve, West is -ve\n";
  cerr << "latitude, in degrees, North is +ve, South is -ve\n";
  cerr << "baseline, East-West component, in metres\n";
  cerr << "baseline, North-South component, in metres\n";
  cerr << "frequency, in MHz\n";
  cerr << "phase offset, in degrees, -180 to 180, set to 0.0 if unsure\n";
  exit(1);
}
