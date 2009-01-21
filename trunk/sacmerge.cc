//
// Copyright (C) David Brodrick
// Copyright (C) CSIRO Australia Telescope National Facility
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.
//
// $Id: $

//A basic one for merging data sets together, possibly with RFI processing

#include <PlotArea.h>
#include <RFI.h>
#include <IntegPeriod.h>
#include <TimeCoord.h>
#include <iostream>
#include <stdlib.h>
#include <sstream>
#include <iomanip>
#include <sys/stat.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>
#include <assert.h>

extern "C" {
#include <cpgplot.h>
    int main(int argc, char *argv[]);
}

using namespace::std;

bool _display = true; //Should we display the resulting data set?

//These control the order in which data are merged
bool _relative = false; //Merge by offset from start of the file?

//These control the way the different data are actually merged together
bool _average = true; //Should we integrate data or just merge?
bool _rfi = true; //Should we perform RFI processing?
timegen_t _inttime = 10000000; //Default integration time
float _sigma = 2.0; //How many std dev from mean is to be considered RFI

//We can use a reference dataset for calibraton purposes
IntegPeriod *_refdata = NULL; //Calibration reference
int _reflen = 0; //Length of calibration data set

//These are the references to the data we need to combine
int _numdata = 0; //The number of data sets to merge
char **_datanames = NULL; //File names for the different data sets
IntegPeriod **_data = NULL; //Pointers to the different data sets
int *_datalen = NULL; //Length of each of the data sets

//The result data
IntegPeriod *_resdata = NULL; //Pointer to the final result data
int _reslen = 0; //Length of result data set
char *_resfile = "data.out"; //Name of the result file

//Print a usage message and exit
void usage();
//Parse the user arguments
void parseArgs(int argc, char **argv);
//Load the files named in "_datanames" into "_data"
void loadData();
//Recalculate all timestamps as offset from the start of the file
void getRelativeTimes();
//Calibrate data against the reference set
void calibrateData();
//Merge the different data sets into one data set
void mergeData();
//Do RFI processing on the data
void processData();
//Integrate data
void averageData();
//Draw a graph of the result data
void plotData();


/////////////////////////////////////////////////////////////////
int main (int argc, char *argv[])
{
  parseArgs(argc, argv);
  loadData();
  if (_relative) getRelativeTimes();
  if (_refdata!=NULL) calibrateData();
  mergeData();
  if (_rfi) processData();
  if (_average) averageData();
  IntegPeriod::write(_resdata, _reslen, _resfile);
  if (_display) plotData();
  return 0;
}


/////////////////////////////////////////////////////////////////
//Recalculate all timestamps as offset from the start of the file
void getRelativeTimes()
{
  for (int i=0; i<_numdata; i++) {
    timegen_t earliest = _data[i][0].timeStamp;
    for (int j=1; j<_datalen[i]; j++) {
      _data[i][j].timeStamp -= earliest;
    }
  }
}


/////////////////////////////////////////////////////////////////
//Calibrate data against the reference set
void calibrateData()
{
  for (int i=0; i<_numdata; i++) {
    cout << _datanames[i] << ":\n";
    IntegPeriod::rescale(_data[i], _datalen[i],
			 _refdata, _reflen);
  }
}


/////////////////////////////////////////////////////////////////
//Integrate the data
void averageData()
{
  IntegPeriod::integrate(_resdata, _reslen, _resdata, _reslen, _inttime);
}


/////////////////////////////////////////////////////////////////
//Do RFI processing on the data
void processData()
{
  cout << "RFI processing data..";
  powerClean(_resdata, _reslen, 1.4, 10);
  markOutliers(_resdata, _reslen, _sigma, 20, false);
  int d = IntegPeriod::purgeFlagged(_resdata, _reslen, _resdata, _reslen);
  cout << "\tdiscarded " << d << endl;
}


/////////////////////////////////////////////////////////////////
//Merge the different data sets into one data set
void mergeData()
{
  //Count how many data periods there are in all
  cout << "Merging data sets\n";
  for (int i=0; i<_numdata; i++) _reslen+=_datalen[i];
  _resdata = new IntegPeriod[_reslen];
  int resi = 0;
  for (int i=0; i<_numdata; i++) {
    for (int j=0; j<_datalen[i]; j++) {
      _resdata[resi] = _data[i][j];
      resi++;
    }
  }
  cout << "Time-sorting data\n";
  IntegPeriod::sort(_resdata, _reslen);
}


/////////////////////////////////////////////////////////////////
//Draw a graph of the result data
void plotData()
{

}


/////////////////////////////////////////////////////////////////
//Load the files named in "_datanames" into "_data"
void loadData()
{
  _data = new IntegPeriod*[_numdata];
  _datalen = new int[_numdata];

  for (int i=0; i<_numdata; i++) {
    if (!IntegPeriod::load(_data[i], _datalen[i], _datanames[i])
	|| _datalen[i]==0 || _data[i]==NULL) {
      cerr << "ERROR: \"" << _datanames[i] << "\" is not a valid data file\n";
      exit(1);
    }
    cout << "Loaded " << _datalen[i] << " data from "
         << _datanames[i] << " OK!\n";
  }
}


/////////////////////////////////////////////////////////////////
//Add another filename to "_datanames"
void addFileName(char *fname)
{
  char **temp = new char*[_numdata+1];
  for (int i=0; i<_numdata; i++) {
    temp[i] = _datanames[i];
  }
  temp[_numdata] = fname;
  if (_datanames!=NULL) delete[] _datanames;
  _datanames = temp;
  _numdata++;
}

/////////////////////////////////////////////////////////////////
//Parse the user arguments
void parseArgs(int argc, char **argv)
{
  //Loop throught the arguments
  for (int i=1; i<argc; i++) {
    string tempstr;
    tempstr = (argv[i]);

    if (tempstr == "-a") {
      //Disable data averaging - just time-merge data
      _average = false;
    } else if (tempstr == "-o") {
      //Merge data by their time offset from the first data in each file
      _relative = true;
    } else if (tempstr == "-r") {
      //Disable RFI processing
      _rfi = false;
    } else if (tempstr == "-i" || tempstr == "-I") {
      //Set the data integration period
      if (argc<i+2) {
	cerr << "ERROR: Insufficient arguments after -i option\n";
	cerr << "You should specify the integration (averaging) time in seconds\n";
	exit(1);
      }
      istringstream tmp(argv[i+1]);
      tmp >> _inttime;
      if (tmp.fail() || _inttime<0 || _inttime>100000) {
	cerr << "ERROR: Bad argument after -i option\n";
	cerr << "You should specify the integration (averaging) time in seconds\n";
	exit(1);
      }
      _inttime *= 1000000;
      i++;
    } else if (tempstr == "-s") {
      //Controls amount of \"RFI\" processing. 0<=s<=10
      if (argc<i+2) {
	cerr << "ERROR: Insufficient arguments after -s option\n";
	cerr << "This controls how much data is rejected as RFI, 0=lots, 10=not much\n";
	exit(1);
      }
      istringstream tmp(argv[i+1]);
      tmp >> _sigma;
      if (tmp.fail() || _sigma<0.0) {
	cerr << "ERROR: Insufficient arguments after -s option\n";
	cerr << "This controls how much data is rejected as RFI, 0=lots, 10=not much\n";
	exit(1);
      }
      i++;
    } else if (tempstr == "-c") {
      //File containing data to calibrate against
      if (argc<i+2) {
	cerr << "ERROR: -c expects the filename of the calibration data\n";
	exit(1);
      }
      cout << "will calibrate against " << argv[i+1] << endl;
      if (!IntegPeriod::load(_refdata, _reflen, argv[i+1]) ||
	  _reflen ==0 || _refdata==NULL) {
	cerr << "ERROR: -c expects the filename of the calibration data\n";
	exit(1);
      }
      i++;
    } else {
      //Assume it is a file name
      struct stat buf;
      if (stat(argv[i],&buf)==-1) { //Check if the file exists
	//cerr << "ERROR: File \"" << "\" could not be opened.\n";
        usage();
      }
      //Looks like it exists, so add it to the queue of data
      addFileName(argv[i]);
    }
  }
}

/////////////////////////////////////////////////////////////////
//Print a usage message and exit
void usage()
{
  cerr << "USAGE: sacmerge [options] file1 [file2] [file3..]\n"
    << "Combines two or more \"SAC\" data files into a single data set.\n"
    << "-a\t\tDisable data averaging - just merge the data sets\n"
    << "-i <seconds>\tSet the data integration/averaging period in seconds\n"
    << "-r\t\tDisable RFI processing\n"
    << "-s <number>\tControls amount of \"RFI\" processing. 0(lots)<=s<=10(little)\n"
    << "-c <filename>\tFile containing data to calibrate against\n"
    << "-o\t\tMerge by time offset from the first datum in each file\n"
    << "\t\t(rather than merging by the absolute timestamp like normal)\n";
  exit(1);
}
