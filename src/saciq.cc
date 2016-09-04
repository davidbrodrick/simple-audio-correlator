//
// Copyright (C) David Brodrick
// Copyright (C) CSIRO Australia Telescope National Facility
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.
//
// $Id: saciq.cc,v 1.3 2004/04/07 13:16:48 brodo Exp brodo $

//Generates complex data from separate in-phase and quadrature data.


#include <PlotArea.h>
#include <IntegPeriod.h>
#include <TimeCoord.h>
#include <iostream>
#include <sstream>
#include <math.h>
#include <assert.h>
#include <cstdlib>

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


//Next comes a whole bunch of variables that controls the way sacmon
//should work. Most of these are set in 'parseArgs' from arguments.
displayT _display = screen; //Output device to display data to
char *_savefile = NULL; //File to write any output graphs to (if not screen)
bool _writeASCII = true; //Whether or not to output ASCII text data
IntegPeriod *_I = NULL; //The "in-phase" data for quadrature analysis
int _Ilen = 0; //Length of the in-phase dataset
IntegPeriod *_Q = NULL; //The "quadrature" data for quadrature analysis
int _Qlen = 0; //Length of the quadrature dataset
bool _flip = false; //Negate the phases

//Calibration parameters
float _Bi = 0.0; //Bias (offset) of in-phase fringes
float _Bq = 0.0; //Bias of quadrature fringes
float _a  = 1.0; //Gain scalar for in-phase data
float _phi = 0.0; //Phase correction to apply to quadrature data


/////////////////////////////////////////////////////////////////
//Generate new "quadrature dataset" from I and Q data
void getquadrature(IntegPeriod *&res, int &reslen,
		   IntegPeriod *i, int ilen,
		   IntegPeriod *q, int qlen,
		   float Bi, float Bq,
		   float a,  float phi);
//Big ugly thing to parse some cryptic command line arguments.
void parseArgs(int argc, char *argv[]);
//Parse a time string (HH:MM[:SS.S]) and return the corresponding time
timegen_t parseTime(istringstream &indate);
//Write the given data to a file in native binary format
void writeData(IntegPeriod *data, int datlen, char *fname);
//Write the given data to a file in ASCII text format
void writeASCII(IntegPeriod *data, int datlen, char *fname);
//Find bidirectional closest match or return -1
int match(int i, IntegPeriod *a, int alen,
          IntegPeriod *b, int blen);
//Print a usage message and exit
void usage();



/////////////////////////////////////////////////////////////////
int main (int argc, char *argv[])
{
  //Parse user arguments and set variables appropriately
  parseArgs(argc, argv);

  if (_I==NULL || _Q==NULL) {
    cerr << "You must specify both in-phase (-i) and quadrature (-q) data\n";
    usage();
  }

  IntegPeriod *data = NULL;
  int datalen = 0;

  cerr << "Matching data and calculating: please wait...\n";
  getquadrature(data, datalen, _I, _Ilen, _Q, _Qlen,
		_Bi, _Bq, _a, _phi);
  writeData(data, datalen, "complex.out");
  if (_writeASCII) writeASCII(data, datalen, "complex.txt");

  ostringstream out;
  switch (_display) {
  case screen:
    out << "/XS";
    break;
  case postscript:
    //out << _savefile << "/CPS";
    out << _savefile << "/VPS";
    break;
  case png:
    out << _savefile << "/PNG";
    cerr << "out = " << out.str() << endl;
    break;
  case dump:
    out << _savefile << "/VWD";
    cerr << "out = " << out.str() << endl;
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

  float temp[datalen];
  float timedata[datalen];
  float maxpow = 0.0;
  for (int i=0; i<datalen;i++) {
      temp[i] = data[i].amplitude;
      if (temp[i]>maxpow) maxpow = temp[i];
      timedata[i] = data[i].timeStamp/float(3600000000.0);
  }
  PlotArea *x = PlotArea::getPlotArea(1);
  x->setTitle("Amplitude");
  x->setAxisY("Amplitude", maxpow, 0.0, false);
  x->setAxisX("Local Sidereal Time (Hours)", timedata[datalen-1], timedata[0], false);
  x->plotLine(datalen, timedata, temp, 4);


  for (int i=0; i<datalen;i++) {
      temp[i] = data[i].phase;
  }
  x = PlotArea::getPlotArea(2);//3
  x->setTitle("Phase");
  x->setAxisY("Phase", PI, -PI, false);
  x->setAxisX("Local Sidereal Time (Hours)", timedata[datalen-1], timedata[0], false);
  x->plotPoints(datalen, timedata, temp, 10);

  float minpow = 0.0;
  maxpow = 0.0;
  float temp2[datalen];
  for (int i=0; i<datalen;i++) {
    temp[i] = data[i].power1;
    temp2[i] = data[i].power2;
    if (temp[i]>maxpow) maxpow = temp[i];
    if (temp[i]<minpow) minpow = temp[i];
    if (temp2[i]>maxpow) maxpow = temp2[i];
    if (temp2[i]<minpow) minpow = temp2[i];
    timedata[i] = data[i].timeStamp/float(3600000000.0);
  }
  x = PlotArea::getPlotArea(0);//2
  //x->setTitle("Corrected Data");
  x->setTitle("Observed Fringe Patterns");
  x->setAxisY("Power", maxpow, minpow, false);
  x->setAxisX("Local Sidereal Time (Hours)", timedata[datalen-1], timedata[0], false);
  x->plotPoints(datalen, timedata, temp, 2);
  x->plotPoints(datalen, timedata, temp2, 6);


/*  float tempI[_Ilen];
  float timeI[_Ilen];
  maxpow = 0.0;
  minpow = 0.0;
  for (int i=0; i<_Ilen; i++) {
    tempI[i] = _I[i].powerX;
    if (tempI[i]>maxpow) maxpow = tempI[i];
    if (tempI[i]<minpow) minpow = tempI[i];
    timeI[i] = _I[i].timeStamp/float(3600000000.0);
  }
  float tempQ[_Qlen];
  float timeQ[_Qlen];
  for (int i=0; i<_Qlen; i++) {
    tempQ[i] = _Q[i].powerX;
    if (tempQ[i]>maxpow) maxpow = tempQ[i];
    if (tempQ[i]<minpow) minpow = tempQ[i];
    timeQ[i] = _Q[i].timeStamp/float(3600000000.0);
  }
  x = PlotArea::getPlotArea(0);
  x->setTitle("Cross Correlations");
  x->setAxisY("Power", maxpow+maxpow/10.0, minpow-minpow/10.0, false);
  x->setAxisX("Time (Hours)", timedata[datalen-1], timedata[0], false);
  x->plotPoints(_Ilen, timeI, tempI, 8);
  x->plotPoints(_Qlen, timeQ, tempQ, 6);
  */
  //Close the pgplot device
  cpgclos();

  return 0;
}


/////////////////////////////////////////////////////////////////
//Big ugly thing to parse some cryptic command line arguments
void parseArgs(int argc, char *argv[])
{
  if (argc<3) usage();

  int i=1;
  //Loop throught the arguments
  for (; i<argc; i++)
    {
      string tempstr;
      tempstr = (argv[i]);

      if (tempstr == "-f") {
        _flip = true;
	cerr << "Will negate the output phases\n";
      } else if (tempstr == "-p" || tempstr=="-phi") {
	istringstream tmp(argv[i+1]);
	float b;
	tmp >> b;
	if (tmp.fail()) usage();
	_phi = PI*b/180.0;
	cerr << "Will correct for " << _phi << " degree quadrature phase error\n";
      } else if (tempstr == "-bq") {
	istringstream tmp(argv[i+1]);
	float b;
	tmp >> b;
	if (tmp.fail()) usage();
	_Bq = b;
	cerr << "Will correct for " << _Bq << " bias on quadrature channel\n";
      } else if (tempstr == "-bi") {
	istringstream tmp(argv[i+1]);
	float b;
	tmp >> b;
	if (tmp.fail()) usage();
	_Bi = b;
	cerr << "Will correct for " << _Bi << " bias on in-phase channel\n";
      } else if (tempstr == "-O") {
	istringstream tmp(argv[i+1]);
	string fname;
	tmp >> fname;
	if (fname.find(".png")!=string::npos || fname.find(".PNG")!=string::npos) {
	  _display = png;
	  cerr << "Will write PNG output to " << fname << endl;
	} else if (fname.find(".ps")!=string::npos || fname.find(".PS")!=string::npos) {
	  _display = postscript;
	  cerr << "Will write postscript output to " << fname << endl;
	} else if (fname.find(".xwd")!=string::npos || fname.find(".XWD")!=string::npos) {
	  _display = dump;
	  cerr << "Will write XWD output to " << fname << endl;
	} else {usage();}

	_savefile = argv[i+1];
	i+=1;
      } else if (tempstr == "-i") {
	if (!IntegPeriod::load(_I, _Ilen, argv[i+1])) {
	  cerr << "ERROR: Couldn't load in-phase data from "
	    << argv[i+1] << endl;
          usage();
	}
	if (_I==NULL || _Ilen==0) {
	  cerr << "Couldn't load in-phase data from " << argv[i+1] << endl;
          usage();
	}
	cerr << "\"I\" has " << _Ilen << " periods\n";
	i+=1;
      } else if (tempstr == "-q" || tempstr == "-Q") {
	if (!IntegPeriod::load(_Q, _Qlen, argv[i+1])) {
	  cerr << "ERROR: Couldn't load quadrature data from "
	    << argv[i+1] << endl;
          usage();
	}
	if (_Q==NULL || _Qlen==0) {
	  cerr << "Couldn't load quadrature data from " << argv[i+1] << endl;
          usage();
	}
	cerr << "\"Q\" has " << _Qlen << " periods\n";
	i+=1;
      }
    }
}


/////////////////////////////////////////////////////////////////
//Read time in the format HH:MM[:SS[.S]]
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


/////////////////////////////////////////////////////////////////
//Scale both inputs and cross to match channel 1 of the reference
void scale(IntegPeriod *ref, int reflen,
	   IntegPeriod *data, int datalen)
{
  //Save hassles for someone one day.
  if (ref==NULL || reflen==0 || data==NULL || datalen==0) return;

  int count=0;
  double gainavg1=0.0, gainavg2=0.0;

  for (int r1=0; r1<reflen; r1++) {
    int d1 = match(r1, ref, reflen, data, datalen);
    if (d1==-1) continue; //No nearest match

    cerr << "matched " << r1 << " to " << d1 << endl;
    //Both gains are calculated WRT channel 1 of reference
    if (data[d1].power1!=0.0) gainavg1 += ref[r1].power1/data[d1].power1;
    if (data[d1].power2!=0.0) gainavg2 += ref[r1].power1/data[d1].power2;
    count++;
  }

  gainavg1/=(float)count;
  gainavg2/=(float)count;
  float gainX = gainavg1*gainavg2;
  cerr << "gains:\t" << gainavg1 << "\t" << gainavg2 << "\t" << gainX << endl;

  //When all finished, go through and scale each IntegPeriod
  for (int d2=0; d2<datalen; d2++) {
    data[d2].power1 *= gainavg1;
    data[d2].power2 *= gainavg2;
    data[d2].powerX *= gainX;
    data[d2].amplitude *= gainX;
  }
}


/////////////////////////////////////////////////////////////////
//Find bidirectional closest match or return -1
//This is a shit house implementation
int match(int i, IntegPeriod *a, int alen,
	  IntegPeriod *b, int blen)
{
  if (a==NULL || alen==0 || b==NULL || blen==0) return -1;

  //Need to match up appropriate samples
  int b2=-1, a1=0;

  long long bestdiff = a[i].timeStamp - b[0].timeStamp;
  if (bestdiff<0) bestdiff = -bestdiff; //Get absolute
  for (int b1=0; b1<blen; b1++) {
    long long thisdiff = a[i].timeStamp - b[b1].timeStamp;
    if (thisdiff<0) thisdiff = -thisdiff; //Get absolute
    //Compare this with best
    if (thisdiff<=bestdiff) {
      bestdiff = thisdiff;
      b2 = b1;
    }
  }
  if (b2==-1) return -1;

  //Then also check if this ref epoch is closest to the data
  long long backdiff = a[0].timeStamp - b[b2].timeStamp;
  if (backdiff<0) backdiff = -backdiff; //Get absolute
  for (int a2=0; a2<alen; a2++) {
    long long thisdiff = a[a2].timeStamp - b[b2].timeStamp;
    if (thisdiff<0) thisdiff = -thisdiff; //Get absolute
    //Compare this with best
    if (thisdiff<backdiff) {
      backdiff = thisdiff;
      a1 = a2;
    }
  }

  if (i!=a1) {
    //Not the closest match in both directions
    //cerr << "failed " << a[a1].timeStamp << "\t" << b[b1].timeStamp << endl;
    return -1;
  }
  return b2;
}


/////////////////////////////////////////////////////////////////
//Generate new "quadrature dataset" from I and Q data
void getquadrature(IntegPeriod *&res, int &reslen,
		   IntegPeriod *i, int ilen,
		   IntegPeriod *q, int qlen,
		   float Bi, float Bq,
		   float a,  float phi)

{
  res = NULL;
  reslen = 0;

  float cphi = cos(phi);
  float sphi = sin(phi);
  float A = 1.0/a;
  float C = -sphi/(a*cphi);
  float D = 1.0/cphi;

  res = new IntegPeriod[ilen>qlen?ilen:qlen];
  int resi = 0;

  for (int i1=0; i1<ilen; i1++) {
    int q1 = match(i1, i, ilen, q, qlen);
    if (q1==-1) continue;

    res[resi].timeStamp = (i[i1].timeStamp+q[q1].timeStamp)/2ll;
    res[resi].powerX = 0.0; //What can we do here?

    float thisi = i[i1].powerX - Bi;
    float thisq = q[q1].powerX - Bq;
    thisq = C*thisi + D*thisq;
    thisi = A*thisi;

    res[resi].power1 = thisi;
    res[resi].power2 = thisq;

    res[resi].amplitude = sqrt(thisi*thisi + thisq*thisq);
    res[resi].phase = atan2(thisq, thisi);
    if (_flip) res[resi].phase = -res[resi].phase;
    resi++;
    reslen++;
  }
  cout << "Of " << ilen << " and " << qlen << " arguments " << reslen
       << " were time-matched\n";
}


////////////////////////////////////////////////////////////////////////
//Write the given data to a file in native binary format
void writeData(IntegPeriod *data, int datlen, char *fname) {
  ofstream datfile(fname, ios::out|ios::binary);
  for (int i=0; i<datlen; i++) datfile << data[i];
}


////////////////////////////////////////////////////////////////////////
//Write the given data to a file in ASCII format
void writeASCII(IntegPeriod *data, int datlen, char *fname) {
  ofstream txtfile(fname, ios::out);
  for (int i=0; i<datlen; i++) {
    txtfile << (data[i].timeStamp/1000000.0) << " " <<
      data[i].amplitude << " " << (data[i].phase*360)/(2*PI) << "\n";
  }
}


/////////////////////////////////////////////////////////////////
//Print a usage message and exit
void usage()
{
  cerr << "\nsaciq: Calculate complex data from separate in-phase and "
    << "quadrature-phase\ndata generated by the simple audio correlator\n"
    << "USAGE: saciq -i <file> -q <file>\n"
    << "\t-i\tSAC data file containing in-phase data\n"
    << "\t-q\tSAC data file containing quadrature-phase data\n"
    << "\t-bi\tCorrect for this \"DC\" offset in the in-phase fringes\n"
    << "\t-bq\tCorrect for this \"DC\" offset in the quadrature fringes\n"
    << "\t-p\tCorrect for this non-orthogonal phase error (degrees)\n"
    << "\t-f\tFlip the output phases (ie, as if antennas were swapped)\n"
    << "\t-O\tPrint the graph to this file instead of screen\n"
    << "See README.saciq for usage details.\n\n.";
  exit(1);
}
