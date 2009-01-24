//
// Copyright (C) David Brodrick
// Copyright (C) CSIRO Australia Telescope National Facility
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.
//
// $Id: sacmon.cc,v 1.12 2005/02/16 10:50:11 brodo Exp brodo $

//This program has evolved over several years and it shows.
//A GUI-based and cleaned up rewrite is due.

#include <PlotArea.h>
#include <SACUtil.h>
#include <RFI.h>
#include <TCPstream.h>
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
  gif,
  dump
} displayT;


//Next comes a whole bunch of variables that controls the way sacmon
//should work. Most of these are set in 'parseArgs' from arguments.
displayT _display = screen; //Output device to display data to
bool _writeASCII = true; //Whether or not to output ASCII text data
bool _twotimes = false; //Set automatically if need to show two sets of data
bool _withlines = false; //Display only post-RFI and use lines
bool _withdots = false; //Display only post-RFI and use dots
bool _LST     = false; //Recalculate all timestamps to a sidereal time?
bool _complex = false;  //Plot cross power or dodgy 'complex' measurements
char *_savefile = NULL; //File to write any output graphs to (if not screen)
float _minbeam = 2.0; //"min beam size" to use in model. Probably leave alone.
bool _rfi   = true; //Should we perform RFI processing
timegen_t _inttime= 30000000; //Default integration time if RFI processing
timegen_t _earliest = 0; //The earliest timestamp on the graphs
float _sigma = 1.2; //How many std dev from mean is to be considered RFI
float _noiselimit = 0; //How large can std dev be to mean else flag as RFI
int _numdatarequests = 0; //The number of data requests specified by user
string *_datarequests; //Holds all data request command strings
string _server = string("localhost");
IntegPeriod *_ref = NULL; //Amplitude reference for scaling data
int _reflen = 0; //Length of the reference
const float LONGNOTINIT = 1000.0;
float site_long = LONGNOTINIT;
float _maxscale  = -1.0; //Maximum scale to use, default is auto
float _maxxscale = -1.0; //Maximum scale for cross correlation
float _minxscale = -1.0; //Minimum scale for cross correlation
bool _onlyX = false; //Do we only show the fringes?
bool _noX   = false; //Do we ignore the fringes, and only show the inputs?
bool _only1 = false; //Only show input 1
bool _only2 = false; //Only show input 2
bool _invertBG = false; //Should we invert the background color?
string _comment = string(""); //Comment for top of graph

//Big ugly thing to parse some cryptic command line arguments.
void parseArgs(int argc, char *argv[]);
//Parse a date string and return the corresponding time
timegen_t parseDate(istringstream &indate);
//Print a usage message and exit
void usage();
//Manages loading of a data request, RFI processing and display
void doRequest(istringstream &command, int set);
//Initialise the graphs
void graphInit(IntegPeriod *procdata, int count,
	       float tmin, float tmax, int set);
//Draw the graphs
void drawGraphs(IntegPeriod *procdata, int count, bool withlines,
		bool rfi, int set);
//Load all data specified in the command
void loadData(istringstream &command, IntegPeriod *&data, int &count);
//Write the given data to a file in native binary format
void writeData(IntegPeriod *data, int datlen, char *fname);
//Write the given data to a file in ASCII text format
void writeASCII(IntegPeriod *data, int datlen, char *fname);
//Do all RFI processing with user specified settings
void cleanData2(IntegPeriod *&res, int &rescount,
	       IntegPeriod *data, int count);
//Scale values of 'data' to approximate those in 'ref'
void scale(IntegPeriod *ref, int reflen,
	   IntegPeriod *data, int datalen);
//Find bidirectional closest match or return -1
int match(int i, IntegPeriod *a, int alen,
          IntegPeriod *b, int blen);


/////////////////////////////////////////////////////////////////
int main (int argc, char *argv[])
{
  //Parse user arguments and set variables appropriately
  parseArgs(argc, argv);

  if (_numdatarequests==0) {
    cerr << "You must specify some data to view with the -F or -T options\n";
    usage();
  }

  for (int i=0; i<_numdatarequests && i<2; i++) {
    //Handle each of the specified data sets, we can only
    //display two because the graphs aren't setup for more.
    //There's no reason they couldn't be.
    istringstream tmpstr (_datarequests[i]);
    doRequest(tmpstr, i);
  }

  //Draw the "comment" now that we know the world coordinates for it
  if (_noX || _only1 || _only2 || _onlyX) {
    cpgsch(1.1);
    cpgsvp(0.0,1.0,0.96,1.0);
  } else {
    cpgsch(1.5);
    cpgsvp(0.0,1.0,1.96,1.99);
  }
  cpgsci(1);
  cpgtext(0,0, _comment.c_str());

  //Close the PGPLOT graphics device
  cpgclos();

  return 0;
}


/////////////////////////////////////////////////////////////////
//Service a request by loading, processing and displaying
void doRequest(istringstream &command, int set)
{
  int count = 0;
  IntegPeriod *data = NULL;

  loadData(command, data, count);
  if (count==0) return;

  //If requested, do some RFI filtering
  IntegPeriod *cleandata = data;
  int cleancount=count;
  if (_rfi) {
    cleanData2(cleandata, cleancount, data, count);
  }

  //Work out the max and min time labels to use on the graph
  float tmin = 0.0;
  float tmax = 0.0;
  if (!_LST && data[0].timeStamp>86400000000ll) {
    tmin = 0.0;
    tmax = (data[count-1].timeStamp-data[0].timeStamp)/float(3600000000ll);;
  } else {
    tmin = data[0].timeStamp/float(3600000000ll);
    tmax = data[count-1].timeStamp/float(3600000000ll);
  }

  //draw the graphs
  graphInit(cleandata, cleancount, tmin, tmax, set);
  if (!_withlines && !_withdots) {
    drawGraphs(data, count, false, true, set);
  }
  if (_withdots) {
    drawGraphs(cleandata, cleancount, false, false, set);
  } else {
    drawGraphs(cleandata, cleancount, true, false, set);
  }

  //Write data to file
  writeData(cleandata, cleancount, "data.out");
  if (_writeASCII) writeASCII(cleandata, cleancount, "data.txt");

  if (cleandata!=data) delete[] cleandata;
  delete[] data;
}


/////////////////////////////////////////////////////////////////
//Clean the data according to any user defined arguments
void cleanData2(IntegPeriod *&res, int &rescount,
	       IntegPeriod *data, int count)
{
  if (data==NULL || count==0) return;

  res = NULL;
  rescount = 0;
  IntegPeriod *tempdata = NULL, *tempdata2 = NULL;
  int tempcount = 0, tempcount2 = 0;

  markOutliers(data, count, 2.0, 20);
  IntegPeriod::purgeFlagged(tempdata, tempcount, data, count);
  if (tempdata==NULL || tempcount==0) return;


  powerClean(tempdata, tempcount, 1.2, 20);
  IntegPeriod::purgeFlagged(tempdata2, tempcount2, tempdata, tempcount);
  delete[] tempdata;
  if (tempdata2==NULL || tempcount2==0) return;

  //Perform second pass
  markOutliers(tempdata2, tempcount2, _sigma, 20, _noiselimit?true:false);
  IntegPeriod::integrate(res, rescount, tempdata2, tempcount2, _inttime);
  delete[] tempdata2;

  cerr << "Finished cleaning data\n";
}


/////////////////////////////////////////////////////////////////
//Return the location of the specified network server
bool getServerLoc(int port, const char *server, pair_t &loc)
{
  loc.c1 = LONGNOTINIT;
  loc.c2 = LONGNOTINIT;
  SocketAddr server_addr = SocketAddr(IPaddress(server), port);

  int listening_socket = socket(AF_INET,SOCK_STREAM,0);
  if( listening_socket < 0 )
    CurrentNetCallback::on_error("Socket creation error");

  int value = 1;
  if( setsockopt(listening_socket, SOL_SOCKET, SO_REUSEADDR,
		 (char*)&value, sizeof(value)) < 0 )
    CurrentNetCallback::on_error("setsockopt SO_REUSEADDR error");

  TCPstream dsrc;
  if (dsrc.rdbuf()->connect(server_addr)!=NULL) {
      dsrc.rdbuf()->set_blocking_io(true);

      if (dsrc.good() && dsrc.rdbuf()->is_open() && !dsrc.eof()) {
	//Finally, ask it's location
	dsrc << "LOCATION\n";
	dsrc >> loc.c1 >> loc.c2;
	if (dsrc.fail() || dsrc.eof()) {
	  cerr << "Error reading location from server\n";
	  dsrc.close();
	  return false;
	}
	if (loc.c1>180.0 || loc.c1<-180.0 || loc.c2>90.0 || loc.c2<-90.0) {
	  cerr << "Returned location was invalid: " << loc.c1 << " " << loc.c2 << endl;
	  dsrc.close();
	  return false;
	}
	dsrc.close();
	cout << "Telescope location is: " << loc.c1 << " " << loc.c2 << endl;
	loc.c1 = PI*loc.c1/180.0;
        loc.c2 = PI*loc.c2/180.0;
	return true;
      }
  }
  dsrc.close();
  return false;
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
  timegen_t realstart=-1; //earliest time out of any dataset
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
	//Treat zero as '24 hours ago'
        start = getAbs() - 86400000000ll;
      } else {
        //otherwise parse the time from argument
	istringstream argdate1(str);
	start = parseDate(argdate1);
      }
      if (realstart==-1 || start<realstart) realstart = start;
      command >> str;
      if (str=="0") {
	//Treat zero as '24 hours after start time'
        end = start + 86400000000ll;
      } else {
        //otherwise parse the time from argument
	istringstream argdate2(str);
	end = parseDate(argdate2);
      }
      cerr << "Loading from network:\n";

      //Work out if we should request server-side pre-processing
      bool procserver = true;
      if (_server=="localhost" || !_rfi) procserver = false;
      //And then ask the server for the data
      if (!IntegPeriod::load(tempdata, tempcount, start, end,
			     _server.c_str(), 31234, procserver)) {
	cerr << "Could not obtain requested data, quitting\n";
	exit(1);
      }
      if (_LST) {
	//We can ask the server exactly what its location is
	pair_t loc = {LONGNOTINIT, 0.0};
	if (!getServerLoc(31234, _server.c_str(), loc)) {
	  cerr << "Couldn't get location for " << _server << endl;
	  loc.c1 = LONGNOTINIT;
	  while (loc.c1>180.0 || loc.c1<-180.0) {
	    cerr << "Please enter the system's longitude (degrees East): ";
	    cin >> loc.c1;
	  }
          loc.c1 = PI*loc.c1/180.0;
	}
	for (int j=0; j<tempcount; j++) {
	  tempdata[j].timeStamp = Abs2LST(tempdata[j].timeStamp, loc.c1);
	}
      }
    } else if (str=="-F") {
      //we are going to load a file, get the filename
      command >> str;
      if (!command.good()) {
        cerr << "Bad argument after -F option\n\n";
	usage();
      }
      cerr << "Loading data from file \'" << str << "\'";
      if (!IntegPeriod::load(tempdata, tempcount, str.c_str())) {
	cerr << "Could not obtain requested data, quitting\n";
	exit(1);
      }
      if (tempcount>0) {
	start = tempdata[0].timeStamp;
      }
      if (realstart==-1 || start<realstart) realstart = start;
      cerr << "\tOK " << tempcount << " loaded\n";
      //If data has audio attached, reprocess it.
      bool firstaudio = true;
      for (int i=0; i<tempcount; i++) {
	if (tempdata[i].rawAudio!=NULL) {
	  if (firstaudio) {
	    firstaudio = false;
	    cerr << "Found raw audio data: REPROCESSING DATA\n";
	  }
	  tempdata[i].doCorrelations();
	  //Free up the memory
	  tempdata[i].keepOnly(1,1,0);
	}
	if (_LST && tempdata[i].timeStamp>86400000000ll) {
	  if (site_long == LONGNOTINIT) {
	    //Need to tell user to specify the longitude
	    cerr << "In order to convert " << str << " to LST you must\n";
	    while (site_long>180.0 || site_long<-180.0) {
	      cerr << "enter the telescope longitude: ";
	      cin >> site_long;
	    }
	    site_long = PI*site_long/180.0;
	  }
	  tempdata[i].timeStamp = Abs2LST(tempdata[i].timeStamp, site_long);
	}
      }
    } else {
      if (command.eof()) break;
      usage();
    }

    if (_LST) {
      if (tempdata[0].timeStamp>86400000000ll) { //Not if already in LST
	//Convert times to sidereal
	for (int i=0; i<tempcount; i++) {
	  tempdata[i].timeStamp = Abs2LST(tempdata[i].timeStamp, site_long);
	}
      }

      //Catch sidereal wrap to keep data sorted
      cerr << "Sorting\t";
      IntegPeriod::sort(tempdata, tempcount);
      cerr << "DONE\n";

      //Scale data to match gains, if processing allowed
      if (_rfi) {
	if (_ref!=NULL) {
          //Scale all data to the nominated reference
	  scale(_ref, _reflen, tempdata, tempcount);
	} else {
	  //Scale gain of channel 2 to match channel 1
          //scale(tempdata, tempcount, tempdata, tempcount);
	}
      }
    } else if (_ref!=NULL) {
      //Scale all data to the nominated reference
      scale(_ref, _reflen, tempdata, tempcount);
    }


    if (tempcount!=0 && count!=0) {
      //Merge the new data
      cerr << "Merging\t";
      IntegPeriod::merge(data, count, data, count, tempdata, tempcount);
      cerr << "DONE\n";
    } else if (tempcount!=0) {
      data = tempdata;
      count = tempcount;
    }
  }

  if (!_LST) {
    //If sidereal time mode wasn't selected on the command line
    //then display times as an offset from the start
    for (int i=0; i<count; i++) {
      //if (data[i].timeStamp>86400000000ll) {
	data[i].timeStamp = data[i].timeStamp-realstart;
      //}
    }
  }


  cerr << "END DATA SET\n";
}


/////////////////////////////////////////////////////////////////
//Initialise the graphs for normal drawing mode - not modelling mode
void graphInit(IntegPeriod *procdata, int count,
	       float tmin, float tmax, int stgraph)
{
  static bool first = true;
  if(first) {
    ostringstream out;
    switch (_display) {
    case screen:
      out << "/XS";
      break;
    case postscript:
      if (_twotimes) {
	out << _savefile << "/VCPS";
      } else {
	  //out << _savefile << "/VCPS";
	out << _savefile << "/CPS";
      }
      break;
    case png:
      out << _savefile << "/PNG";
      break;
    case gif:
      out << _savefile << "/GIF";
      break;
    case dump:
      out << _savefile << "/VWD";
      break;
    default:
      assert(false);
      break;
    }

    if (cpgbeg(0, out.str().c_str(), 1, 1) != 1)
      exit(1);

    //Setup the colour palette
    if (_invertBG) setupPaletteWhite();
    else setupPalette();

    if (!_noX&&!_onlyX&&!_only1&&!_only2) cpgsvp(0.05,0.95,0.1,0.9);
    else cpgsvp(0.1,0.9,0.1,0.9);

    if (_only1 || _only2 || _onlyX || _noX) {
      PlotArea::setPopulation(1);
    } else if (_twotimes) {
      PlotArea::setPopulation(3);
    } else {
      PlotArea::setPopulation(2);
    }
    first = false;
  }

  //Set the font size
  cpgsch(1.2);

  float xmin, ymin, xmax, ymax;
  PlotArea *x;

  ymin=999999999, ymax=0;
  xmin=999999999, xmax=-999999999;
  for (int i=0; i<count; i++) {
    if (!_complex) {
      if ((procdata[i].power1<ymin)&&!_only2) ymin = procdata[i].power1;
      if ((procdata[i].power2<ymin)&&!_only1) ymin = procdata[i].power2;
      if ((procdata[i].power1>ymax)&&!_only2) ymax = procdata[i].power1;
      if ((procdata[i].power2>ymax)&&!_only1) ymax = procdata[i].power2;
      if ((procdata[i].powerX<xmin)) xmin = procdata[i].powerX;
      if ((procdata[i].powerX>xmax)) xmax = procdata[i].powerX;
    } else {
      if (procdata[i].amplitude < ymin) ymin = procdata[i].amplitude;
      if (procdata[i].amplitude > ymax) ymax = procdata[i].amplitude;
    }
  }
  ymax+=ymax/10.0;
  ymin-=ymax/10.0;
  float temp=(xmax-xmin)/8;
  xmax+=temp; xmin-=temp;

  //Undo all that good work if user has overridden autoscaling
  if (_maxscale!=-1.0) ymax = _maxscale;
  if (_maxxscale!=-1.0) xmax = _maxxscale;
  if (_minxscale!=-1.0) xmin = _minxscale;

  if (!_onlyX && (!stgraph || !_twotimes)) {
    x = PlotArea::getPlotArea(0);
    if (!_complex) x->setTitle("Input Powers");
    else x->setTitle("Visibility Amplitude");
    if (_LST) {
      x->setAxisX("Local Sidereal Time (Hours)", tmax, tmin, false);
    } else {
      x->setAxisX("Time (Hours)", tmax, tmin, false);
    }
    x->setAxisY("Power", ymax, 0.0, false);
  }

  //Setup the graph for Cross Power
  if (!_noX && !_only1 && !_only2) {
    if (_onlyX) x = PlotArea::getPlotArea(0);
    else x = PlotArea::getPlotArea(stgraph+1);
    if (!_complex) x->setTitle("Cross Correlation");
    else x->setTitle("Visibility Phase");
    x->setAxisY("Correlation", xmax, xmin, false);
    if (_LST) {
      x->setAxisX("Local Sidereal Time (Hours)", tmax, tmin, false);
    } else {
      x->setAxisX("Time (Hours)", tmax, tmin, false);
    }
  }
}


/////////////////////////////////////////////////////////////////
void drawGraphs(IntegPeriod *procdata, int count, bool lines, bool rfi, int set) {
  int realcount=0;

  float temp[count];
  float timedata[count];
  PlotArea *x = NULL;

  for (int i=0; i<count;i++) {
    temp[i] = procdata[i].power1;
    if (_LST) {
      timedata[i] = procdata[i].timeStamp/float(3600000000.0);
    } else {
      timedata[i] = procdata[i].timeStamp/float(3600000000.0);
    }
    realcount++;
  }


  if (!_onlyX) {
    if (_ref!=NULL && _LST && _rfi) {
      float reftemp[_reflen];
      float reftime[_reflen];
      for (int i=0; i<_reflen;i++) {
	reftemp[i] = _ref[i].power1;
	reftime[i] = _ref[i].timeStamp/float(3600000000.0);
      }
      x = PlotArea::getPlotArea(0);
      x->plotPoints(_reflen, reftime, reftemp, 2);
    }

    if (!_complex) {
      if ((!_twotimes || !set)) {
	x = PlotArea::getPlotArea(set);
	if (!_only2) {
	  //Only display input powers for first data request
	  if (lines) {
	    x->plotLine(realcount, timedata, temp, 3);
	  } else {
	    x->plotPoints(realcount, timedata, temp, 2);
	  }
	}
	if (!_only1) {
	  for (int i=0; i<count;i++) temp[i] = procdata[i].power2;
	  if (lines) {
	    x->plotLine(realcount, timedata, temp, 5);
	  } else {
	    x->plotPoints(realcount, timedata, temp, 4);
	  }
	}
      }
    }
  }

  if (!_noX && !_only1 && !_only2) {
    if (_onlyX) x = PlotArea::getPlotArea(0);
    else x = PlotArea::getPlotArea(set+1);

    if (_complex) {
      //Display "complex" visibilities
      for (int i=0; i<count;i++) temp[i] = procdata[i].phase;
      x->setAxisY("Phase", PI, -PI, false);
      if (lines)
	x->plotPoints(realcount, timedata, temp, 7);
      else
	x->plotPoints(realcount, timedata, temp, 6);

      //Plot the amplitude on the same graph as the input powers
      x = PlotArea::getPlotArea(set);
      for (int i=0; i<count;i++) temp[i] = procdata[i].amplitude;

      if (lines) {
	x->plotLine(realcount, timedata, temp, 3);
      } else {
	x->plotPoints(realcount, timedata, temp, 2);
      }

    } else {
      //Display standard cross power data
      //for (int i=0; i<count;i++) temp[i] = procdata[i].amplitude*cos(procdata[i].phase);
      for (int i=0; i<count;i++) temp[i] = procdata[i].powerX;

      if (lines) {
	x->plotLine(realcount, timedata, temp, 7);
      } else
	x->plotPoints(realcount, timedata, temp, 6);

      //Display Q
      /*for (int i=0; i<count;i++) temp[i] = procdata[i].amplitude*sin(procdata[i].phase);
       if (lines)
       x->plotLine(realcount, timedata, temp, 7);
       else
       x->plotPoints(realcount, timedata, temp, 5);*/
    }
  }
}


////////////////////////////////////////////////////////////////////////
//Write the given data to a file in native binary format
void writeData(IntegPeriod *data, int datlen, char *fname) {
  ofstream datfile(fname, ios::out|ios::binary);
  for (int i=0; i<datlen; i++) {
    datfile << data[i];
  }
}


////////////////////////////////////////////////////////////////////////
//Write the given data to a file in ASCII text format
void writeASCII(IntegPeriod *data, int datlen, char *fname) {
  ofstream txtfile(fname, ios::out);
  for (int i=0; i<datlen; i++) {
    txtfile << (data[i].timeStamp/1000000.0) << " " <<
      data[i].power1 << " " <<
      data[i].power2 << " " <<
      data[i].powerX << "\n";
  }
}


/////////////////////////////////////////////////////////////////
//Big ugly thing to parse some cryptic command line arguments
void parseArgs(int argc, char *argv[])
{
  if (argc<3) usage();

  int i=1;
  //Loop throught the arguments
  for (; i<argc; i++) {
    string tempstr;
    tempstr = (argv[i]);

    if (tempstr == "-R") {
      istringstream tmp(argv[i+1]);
      bool yesno;
      tmp >> yesno;
      if (tmp.fail()) {usage();}
      _rfi = yesno;
      cerr << "Turning RFI removal " << (_rfi?"ON":"OFF") << endl;
      i+=1;
    } else if (tempstr == "-I") {
      if (argc<i+2) {
        cerr << "Insufficient arguments after -I option\n";
        usage();
      }

      istringstream tmp(argv[i+1]);
      tmp >> _inttime;
      if (tmp.fail()) {usage();}
      _inttime *= 1000000;
      cerr << "Using " << _inttime/1000000 << " second integration periods\n";
      i+=1;
    } else if (tempstr == "-S") {
      if (argc<i+2) {
        cerr << "Insufficient arguments after -S option\n";
        usage();
      }
      istringstream tmp(argv[i+1]);
      tmp >> _sigma;
      if (tmp.fail()) {usage();}
      cerr << "Will discard points more than " << _sigma << " std dev above mean\n";
      i+=1;
    } else if (tempstr == "-N") {
      if (argc<i+2) {
        cerr << "Insufficient arguments after -N option\n";
        usage();
      }

      istringstream tmp(argv[i+1]);
      tmp >> _noiselimit;
      if (tmp.fail()) {usage();}
      cerr << "Will discard blocks noisier than " << _noiselimit << endl;
      i+=1;
    } else if (tempstr == "-x") {
      istringstream tmp(argv[i+1]);
      tmp >> _server;
      cerr << "Will load network data from: " << _server << endl;
      i++;
    } else if (tempstr == "-X") {
      _onlyX = true;
      cerr << "Will only show the fringes in the graphs\n";
    } else if (tempstr == "-noX") {
      _noX = true;
      cerr << "Will only show the two inputs in the graphs\n";
    } else if (tempstr == "-1") {
      _only1 = true;
      cerr << "Will only show input 1 in the graphs\n";
    } else if (tempstr == "-2") {
      _only2 = true;
      cerr << "Will only show input 2 in the graphs\n";
    } else if (tempstr == "-w" || tempstr == "-white") {
      _invertBG = true;
      cerr << "Will use a white background\n";
    } else if (tempstr == "-max") {
      if (argc<i+1) {
        cerr << "Insufficient arguments after -max option\n";
        usage();
      }
      istringstream tmp(argv[i+1]);
      tmp >> _maxscale;
      if (tmp.fail()) {usage();}
      cerr << "Setting maximum scale for inputs to " << _maxscale << endl;
      i+=1;
    } else if (tempstr == "-maxX") {
      if (argc<i+1) {
        cerr << "Insufficient arguments after -maxX option\n";
        usage();
      }
      istringstream tmp(argv[i+1]);
      tmp >> _maxxscale;
      if (tmp.fail()) {usage();}
      cerr << "Setting maximum scale for cross-correlation to " << _maxxscale << endl;
      i+=1;
    } else if (tempstr == "-minX") {
      if (argc<i+1) {
        cerr << "Insufficient arguments after -minX option\n";
        usage();
      }
      istringstream tmp(argv[i+1]);
      tmp >> _minxscale;
      if (tmp.fail()) {usage();}
      cerr << "Setting minimum scale for cross-correlation to " << _minxscale << endl;
      i+=1;
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
      } else if (fname.find(".gif")!=string::npos || fname.find(".GIF")!=string::npos) {
        _display = gif;
        cerr << "Will write GIF output to " << fname << endl;
      } else if (fname.find(".xwd")!=string::npos || fname.find(".XWD")!=string::npos) {
        _display = dump;
        cerr << "Will write XWD output to " << fname << endl;
      } else {usage();}
        _savefile = argv[i+1];
      i+=1;
    } else if (tempstr == "-comment" || tempstr == "--comment") {
      //Ensure the argument follows, the shell decodes any ""'s for us
      if (argc<=i+1){
        cerr << "ERROR: The \"" << tempstr << "\" command requires a \"comment\" argument, eg:\n";
        cerr << "\t" << tempstr << " \"2005/02/17, Bloggsville Observatory, 10MHz\"\n";
        exit(1);
      }
      _comment = string(argv[i+1]);
      i+=1;
    } else if (tempstr == "-C") {
      _complex = true;
      cerr << "Will display \"complex\" visibilities\n";
    } else if (tempstr == "-l") {
      if (argc<i+2) {
	cerr << "Insufficient arguments after -l option\n";
	usage();
      }
      istringstream tmp(argv[i+1]);
      tmp >> site_long;
      if (tmp.fail() || site_long>180.0 || site_long<-180.0) {
	cerr << "ERROR: Longitude must be in degrees (East is +ve)\n";
	usage();
      }
      cerr << "Telescope longitude is " << site_long << endl;
      site_long = PI*site_long/180.0;
      i+=1;
    } else if (tempstr == "-LST") {
      _LST = true;
      cerr << "Will convert times from absolute to local sidereal\n";
    } else if (tempstr == "-L") {
      _withlines = true;
      cerr << "Only clean lines will be drawn\n";
    } else if (tempstr == "-D") {
      cerr << "Clean data will be displayed with dots\n";
      _withdots = true;
    } else if (tempstr == "-ref" || tempstr == "-r") {
      if (!IntegPeriod::load(_ref, _reflen, argv[i+1])) {
	cerr << "ERROR: Couldn't load reference file: "
	  << argv[i+1] << endl;
	usage();
      }
      if (_ref==NULL || _reflen==0) {
	cerr << "Couldn't load data from " << argv[i+1] << endl;
	usage();
      }

      cerr << "Will scale input channels to " << argv[i+1] << " reference\n";
      cerr << "Reference has " << _reflen << " periods\n";
      i+=1;
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
    } else {
      cerr << "Bad argument \'"<< tempstr << "\'\n";
      usage();
    }
  }
  if (_numdatarequests>0) {
    cerr << "Generated " << _numdatarequests << " commands:" << endl;
    for (int j=0; j<_numdatarequests; j++) {
      cerr << _datarequests[j] << endl;
    }
    if (_numdatarequests>1) _twotimes = true;
  }
}


/////////////////////////////////////////////////////////////////
//Print a usage message and exit
void usage()
{
  cerr << "\nUSAGE: sacmon <this needs rewriting>\n"
    << "See README.sacmon for usage details.\n\n.";
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


/////////////////////////////////////////////////////////////////
//Scale both inputs and cross to match channel 1 of the reference
void scale(IntegPeriod *ref, int reflen,
	   IntegPeriod *data, int datalen)
{
  if (ref==NULL || reflen==0 || data==NULL || datalen==0) return;

  IntegPeriod::translate(data, datalen, ref, reflen);
  return;

  int count=0;
  double gainavg1=0.0, gainavg2=0.0;

  for (int r1=0; r1<reflen; r1++) {
    int d1 = match(r1, ref, reflen, data, datalen);
    if (d1==-1) continue; //No nearest match

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
int match(int i, IntegPeriod *a, int alen,
	  IntegPeriod *b, int blen)
{
  if (a==NULL || alen==0 || b==NULL || blen==0) return -1;

  //Need to match up appropriate samples
  //Dodgy slow implementation too
  int b2=-1, a1=-1;

  long long bestdiff = a[i].timeStamp - b[0].timeStamp;
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
      if (thisdiff<=backdiff) {
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
