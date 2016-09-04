//
// Copyright (C) David Brodrick
// Copyright (C) CSIRO Australia Telescope National Facility
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.
//
// $Id: sacedit.cc,v 1.2 2004/11/28 09:50:34 brodo Exp brodo $

//An interactive pgplot based program to cut chop and generally stuff
//around with sets of data.

#include <PlotArea.h>
#include <RFI.h>
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

IntegPeriod *_data = NULL; //The data set as it stands
int _datalen = 0; //Length of the data set
IntegPeriod *_undo = NULL; //The previous data set
int _undolen = 0; //Length of the previous data set

timegen_t _timeoffset = 0; //Mapping between relative and absolute times


//Tell 'em what it's all about
void usage();
//Command help for once they've figured out how to start the program
void help();
//Draw a graph of the data between the specified times
void drawGraph(timegen_t start, timegen_t end);
//Parse two times from the string and return them, or false if it fails
bool parseTwoTimes(istringstream &str, timegen_t &one, timegen_t &two);
//Parse a time string HH:MM or HH.decimal and return corresponding time
timegen_t parseTime(string indate);
//Call this before creating a new _data reference
void nextUndo();
//Write the given data to a file in native binary format
void writeData(const char *fname);
//Write the given data to a file in ASCII text format
void writeASCII(const char *fname);

/////////////////////////////////////////////////////////////////
int main (int argc, char *argv[])
{
  if (argc!=2) usage();

  IntegPeriod *tempdata = NULL;
  bool badload = !IntegPeriod::load(tempdata, _datalen, argv[1]);
  if (badload || tempdata==NULL || _datalen==0) {
    cerr << "I just tried to load \"" << argv[1] << "\" and had no luck.\n";
    usage();
  }
  _data = tempdata;
  IntegPeriod::sort(_data, _datalen);
  //delete[] tempdata;
  cout << "Loaded " << _datalen << " from " << argv[1] << endl;

  if (_data[0].timeStamp>2*86400000000ll) {
    cout << "Timestamps indicate absolute time, mapping to relative time..\n";
    _timeoffset = _data[0].timeStamp;
    for (int i=0; i<_datalen; i++) {
      _data[i].timeStamp -= _timeoffset;
    }
  }

  if (cpgbeg(0, "/XS", 2, 2) != 1)
    exit(1);
  cpgsvp(0.05,0.95,0.1,0.9);
  PlotArea::setPopulation(2);
  cpgsch(1.2);

  help();

  timegen_t currstart = _data[0].timeStamp;
  timegen_t currend = _data[_datalen-1].timeStamp;
  while (true) {
    drawGraph(currstart, currend);

    //I've always wanted to use this prompt :-))
    cout << "input command: ";
    char line[1001];
    line[1000] = '\0';
    cin.getline(line, 1000);
    istringstream command(line);
    string instruction;
    command >> instruction;
    if (command.fail() || instruction=="") continue;

    if (instruction=="o" || instruction=="out") {
      //zoom Out
      cout << "OK, zooming right out!\n";
      currstart = _data[0].timeStamp;
      currend = _data[_datalen-1].timeStamp;

    } else if (instruction=="z" || instruction=="zoom") {
      //Zoom into the data between the specified times
      timegen_t tempstart=0, tempend=0;
      if (!parseTwoTimes(command, tempstart, tempend)) {
	cerr << "You need to specify the range to zoom to.\n";
        continue;
      }
      if ((tempstart<_data[0].timeStamp && tempend<_data[0].timeStamp) ||
	  (tempstart>_data[_datalen-1].timeStamp && tempend>_data[_datalen-1].timeStamp)) {
	cerr << "All data would be outside that time range!\n";
	cerr << "The minimum is " << _data[0].timeStamp/3600000000.0 << endl;
	cerr << "The maximum is " << _data[_datalen-1].timeStamp/3600000000.0 << endl;
        continue;
      }
      currstart = tempstart;
      currend = tempend;

    } else if (instruction=="d" || instruction=="r"
	       || instruction=="remove") {
      //Delete or Remove data between the specified times
      //Zoom into the data between the specified times
      timegen_t tempstart=0, tempend=0;
      if (!parseTwoTimes(command, tempstart, tempend)) {
	cerr << "You need to specify the range to zoom to.\n";
        continue;
      }
      //Count how many will be left
      int count = 0;
      for (int i=0; i<_datalen; i++) {
	if (_data[i].timeStamp<tempstart || _data[i].timeStamp>tempend) count++;
      }
      if (count==0) {
	cerr << "NO! All data will be deleted using those time settings\n";
	continue;
      }
      //Save the current data
      nextUndo();
      IntegPeriod *tempdata = new IntegPeriod[count];
      count = 0;
      for (int i=0; i<_datalen; i++) {
	if (_data[i].timeStamp<tempstart || _data[i].timeStamp>tempend) {
          tempdata[count] = _data[i];
	  count++;
	}
      }
      _data = tempdata;
      _datalen = count;
      //Only rezoom if there are now no data on the screen
      bool datafound = false;
      for (int i=0; i<_datalen; i++) {
	if (_data[i].timeStamp>=currstart && _data[i].timeStamp<=currend) {
	  datafound = true;
	  break;
	}
      }
      if (!datafound) {
	currstart = _data[0].timeStamp;
	currend = _data[_datalen-1].timeStamp;
      }

    } else if (instruction=="offset") {
      //Apply an offset to the specified radiometer channel
      char chan = '1';
      command >> chan;
      if (chan!='1' && chan!='2' && chan!='X' && chan!='x') {
	cerr << "The first argument must specify which radiometer channel to"
	  " offset\nThis should either be \"1\",\"2\" or \"X\"\n";
        continue;
      }

      timegen_t tempstart=0, tempend=0;
      if (!parseTwoTimes(command, tempstart, tempend)) {
	cerr << "You need to specify the time range to offset.\n";
        continue;
      }

      float offset = 0.0;
      command >> offset;
      if (command.fail()) {
	cerr << "The last argument must specify the offset amount!\n";
        continue;
      }

      //Copy the original data
      IntegPeriod *temp = new IntegPeriod[_datalen];
      //Apply the offset
      for (int i=0; i<_datalen; i++) {
        temp[i] = _data[i];
      }
      //Save the current data
      nextUndo();
      _data = temp;
      //Apply the offset
      if (chan=='1') {
	for (int i=0; i<_datalen; i++) {
	  if (_data[i].timeStamp>tempstart && _data[i].timeStamp<tempend) {
	    _data[i].power1 += offset;
	  }
	}
      } else if (chan=='2') {
	for (int i=0; i<_datalen; i++) {
	  if (_data[i].timeStamp>tempstart && _data[i].timeStamp<tempend) {
	    _data[i].power2 += offset;
	  }
	}
      } else {
	for (int i=0; i<_datalen; i++) {
	  if (_data[i].timeStamp>tempstart && _data[i].timeStamp<tempend) {
	    _data[i].powerX += offset;
	  }
	}
      }

    } else if (instruction=="avg") {
      //Get average of the specified channel over the time range
      char chan = '1';
      command >> chan;
      if (chan!='1' && chan!='2' && chan!='X' && chan!='x') {
	cerr << "The first argument must specify which radiometer channel to"
	  " offset\nThis should either be \"1\",\"2\" or \"X\"\n";
        continue;
      }

      timegen_t tempstart=0, tempend=0;
      if (!parseTwoTimes(command, tempstart, tempend)) {
	cerr << "You need to specify the time range to measure average over.\n";
        continue;
      }

      //Measure the average
      double sum = 0.0;
      int count = 0;
      if (chan=='1') {
	for (int i=0; i<_datalen; i++)
	  if (_data[i].timeStamp>tempstart && _data[i].timeStamp<tempend) {
	    sum += _data[i].power1;
            count++;
	  }
      } else if (chan=='2') {
	for (int i=0; i<_datalen; i++)
	  if (_data[i].timeStamp>tempstart && _data[i].timeStamp<tempend) {
	    sum += _data[i].power2;
            count++;
	  }
      } else {
	for (int i=0; i<_datalen; i++)
	  if (_data[i].timeStamp>tempstart && _data[i].timeStamp<tempend) {
	    sum += _data[i].powerX;
	    count++;
	  }
      }
      cout << "THE AVERAGE IS " << sum/count << endl;

    } else if (instruction=="clip" || instruction=="c") {
      //Clip all data > the specified threshold
      float threshold = 0.0;
      command >> threshold;
      if (command.fail()) {
	cerr << "You must specify the threshold value!\n";
        continue;
      }

      //First count how many will be left
      int count=0;
      for (int i=0; i<_datalen; i++) {
	if (_data[i].power1<threshold && _data[i].power2<threshold) {
	  count++;
	}
      }
      //Reality check
      if (count==0) {
	cerr << "All data would eb removed with those settings!!\n";
        continue;
      }
      //Keep the data which is within the threshold
      IntegPeriod *temp = new IntegPeriod[count];
      count = 0;
      for (int i=0; i<_datalen; i++) {
	if (_data[i].power1<threshold && _data[i].power2<threshold) {
	  temp[count] = _data[i];
	  count++;
	}
      }
      //Save the current data
      nextUndo();
      _data = temp;
      _datalen = count;

    } else if (instruction=="i" || instruction=="integrate"
	       || instruction=="average") {
      //Apply the specified integration time
      float inttime = 0.0;
      command >> inttime;
      if (command.fail() || isnan(inttime) || inttime<=0) {
	cerr << "You must specify the threshold value!\n";
	continue;
      }
      timegen_t inttime2 = (timegen_t)(inttime*1000000);
      //Save the current data
      nextUndo();
      IntegPeriod *temp = NULL;
      int templen = 0;
      //Do the integration
      IntegPeriod::integrate(temp, templen, _data, _datalen, inttime2);
      //Save the results
      _data = temp;
      _datalen = templen;

    } else if (instruction=="filter" || instruction=="f") {
      //Discard data more than sigma standard deviations from local mean
      float sigma = 0.0;
      command >> sigma;
      if (command.fail() || isnan(sigma) || sigma<0) {
	cerr << "You must specify how many standard deviations above the local mean\n";
	continue;
      }
      timegen_t tempstart=0, tempend=0;
      if (!parseTwoTimes(command, tempstart, tempend)) {
	cerr << "You need to specify a range of times to filter.\n";
	continue;
      }

      //Count how many data periods are inside the time range
      int first = -1;
      int count = 0;
      for (int i=0; i<_datalen; i++) {
	if (_data[i].timeStamp>tempstart && _data[i].timeStamp<tempend) {
          if (first==-1) first = i;
	  count++;
	}
      }
      if (count==0) {
	cerr << "No data is inside the specified time range!\n";
	continue;
      }

      //Save the current data
      nextUndo();

      //Copy the current data
      IntegPeriod *temp = new IntegPeriod[_datalen];
      int templen = _datalen;
      for (int i=0; i<_datalen; i++) temp[i] = _data[i];
      //Flag the RFI affected data
      markOutliers(temp+first, count, sigma, 21);
      //Discard the flagged data
      IntegPeriod::purgeFlagged(_data, _datalen, temp, templen);

    } else if (instruction=="u" || instruction=="undo") {
      //A very important one..
      if (_undo!=NULL && _undolen>0) {
        cout << "OK, undoing last modification\n";
	delete[] _data;
	_data = _undo;
	_datalen = _undolen;
	_undo = NULL;
        _undolen = 0;
      } else {
	cout << "Cannot undo\n";
      }
    } else if (instruction=="abort") {
      //Get out of here
      cerr << "Bye!\n";
      exit(0);
    } else if (instruction=="x" || instruction=="q" || instruction=="exit"
	       || instruction=="quit" || instruction=="bye") {
      //Get out of here
      //Write the data out
      cout << "Writing " << _datalen << " integration periods to \"data.out\".\n";
      writeData("data.out");
      exit(0);
    } else if (instruction=="w" || instruction=="write") {
      //Write data out with an optionally specified filename
      string fn;
      command >> fn;
      if (command.fail()) {
        cout << "Writing " << _datalen << " integration periods to \"data.out\".\n";
	writeData("data.out");
      } else {
	cout << "Writing " << _datalen << " integration periods to \"" << fn << "\".\n";
        writeData(fn.c_str());
      }
    } else if (instruction=="LST") {
      //Convert times to LST
      if (_data[0].timeStamp<=10*86400000000ll) {
	cerr << "Can only convert data with absolute timestamps to LST\n";
        continue;
      }
    } else if (instruction=="t") {
      //Convert times to relative
      cout << "Converting timestamps to be offset from "
	   << printTime(_data[0].timeStamp);
      //Save the current data first
      nextUndo();
      IntegPeriod *tempdata = new IntegPeriod[_datalen];
      for (int i=0; i<_datalen; i++) {
	tempdata[i] = _data[i];
        tempdata[i].timeStamp -= _data[0].timeStamp;
      }
      _data = tempdata;
      currstart = _data[0].timeStamp;
      currend = _data[_datalen-1].timeStamp;
      _timeoffset = 0; //Clear this
    } else {
      help();
    }
  }

  return 0;
}


//////////////////////////////////////////////////////////////////
//Print a much needed error message and exit
void usage()
{
  cerr << "ERROR: You must specify a SAC data file argument\n";
  exit(1);
}


//////////////////////////////////////////////////////////////////
//Provide some help for those that managed to start the program
void help()
{
  cerr << "\nsacedit is an interactive tool for browsing and editing SAC data files:\n";
  cerr << "Some commands take time arguments, these can be given in 9:12 or 9.2 formats.\n";
  cerr << "z time time\tZoom to show the data between the given times\n";
  cerr << "o\t\tZoom out to show all data\n";
  cerr << "d time time\tDelete the data between the given times\n";
  cerr << "offset chan time time value\tOffset the given channel (1,2orX) by amount\n";
  cerr << "avg chan time time\tPrint average value of the channel (1,2orX) over period\n";
  cerr << "c value\t\tRemove all data points > the specified value\n";
  cerr << "i seconds\tIntegrate the data over the specified number of seconds\n";
  cerr << "f value time time\tFilter all data more than value standard deviations from local mean\n";
  cerr << "u\t\tUndo the last change\n";
  cerr << "w filename\tWrite the current data out to a file\n";
  cerr << "q\t\tQuit the program, writing data to \"data.out\"\n";
  cerr << "abort\t\tQuit the program without writing a file\n";
}


//////////////////////////////////////////////////////////////////
//Draw a graph of the data between the specified times
void drawGraph(timegen_t start, timegen_t end)
{
  //Count how many periods match
  int count = 0;
  for (int i=0; i<_datalen; i++) {
    if (_data[i].timeStamp>=start && _data[i].timeStamp<=end) count++;
  }

  float p1[count];
  float p2[count];
  float pX[count];
  float t[count];
  float pmax = 0.0;

  count = 0;
  for (int i=0; i<_datalen; i++) {
    if (_data[i].timeStamp>=start && _data[i].timeStamp<=end) {
      p1[count] = _data[i].power1;
      if (p1[count]>pmax) pmax = p1[count];
      p2[count] = _data[i].power2;
      if (p2[count]>pmax) pmax = p2[count];
      pX[count] = _data[i].powerX;
      t[count]  = _data[i].timeStamp/3600000000.0;
      count++;
    }
  }

  PlotArea *x = PlotArea::getPlotArea(0);
  x->setTitle("(A) Radiometer Outputs");
  x->setAxisY("Power", pmax + pmax/20.0, 0.0, false);
  x->setAxisX("Time (Hours)", t[count-1], t[0], false);
  x->plotLine(count, t, p1, 4);
  x->plotLine(count, t, p2, 5);

  x = PlotArea::getPlotArea(1);
  x->setTitle("(B) Interferometer Output");
  x->setAxisY("Power", 0.0, 0.0, true);
  x->setAxisX("Time (Hours)", t[count-1], t[0], false);
  x->plotLine(count, t, pX, 6);

}


//////////////////////////////////////////////////////////////////
//
bool parseTwoTimes(istringstream &command, timegen_t &one, timegen_t &two)
{
  string foo;
  command >> foo;
  one = parseTime(foo);
  if (command.fail() || one==-1) {

    return false;
  }
  command >> foo;
  two = parseTime(foo);
  if (command.fail() || two==-1) {

    return false;
  }

  if (one>two) {
    timegen_t foo = one;
    one = two;
    two = foo;
  }

  return true;
}


////////////////////////////////////////////////////////////////////////
//Save the current data to the undo buffer and remove old delete buffer
void nextUndo()
{
  if (_undo!=NULL && _undolen>0) delete[] _undo;
  _undo = _data;
  _undolen = _datalen;
}


////////////////////////////////////////////////////////////////////////
//Write the data to a file in native binary format
void writeData(const char *fname) {
  //If we remapped timestamps then map them back
  if (_timeoffset!=0) {
    cout << "Remapping timestamps to absolute times\n";
    for (int i=0; i<_datalen; i++) {
      _data[i].timeStamp += _timeoffset;
    }
  }

  ofstream datfile(fname, ios::out|ios::binary);
  for (int i=0; i<_datalen; i++) {
    datfile << _data[i];
  }

  //And then change them back again
  if (_timeoffset!=0) {
    for (int i=0; i<_datalen; i++) {
      _data[i].timeStamp -= _timeoffset;
    }
  }
}


////////////////////////////////////////////////////////////////////////
//Write the data to a file in ASCII text format
void writeASCII(const char *fname) {
  //If we remapped timestamps then map them back
  if (_timeoffset!=0) {
    for (int i=0; i<_datalen; i++) {
      _data[i].timeStamp += _timeoffset;
    }
  }

  ofstream txtfile(fname, ios::out);
  for (int i=0; i<_datalen; i++) {
    txtfile << (_data[i].timeStamp/1000000.0) << " " <<
      _data[i].power1 << " " <<
      _data[i].power2 << " " <<
      _data[i].powerX << "\n";
  }

  //And then change them back again
  if (_timeoffset!=0) {
    for (int i=0; i<_datalen; i++) {
      _data[i].timeStamp -= _timeoffset;
    }
  }
}

