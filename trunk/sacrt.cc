//
// Copyright (C) David Brodrick
// Copyright (C) CSIRO Australia Telescope National Facility
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.
//
// $Id: sacrt.cc,v 1.2 2004/07/31 00:41:18 brodo Exp brodo $

// sacrt - monitor one or more sac servers in real-time


#include <PlotArea.h>
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
#include <unistd.h>
#include <cstring>

extern "C" {
#include <cpgplot.h>
    int main(int argc, char *argv[]);
}

using namespace::std;

bool _RFI = true;  //Whether or not to perform basic RFI processing
bool _LST = false; //Whether to display time as system LST
bool _offsetGraphs = false; //Whether to rescale data and offset lines
timegen_t _lastOffset = 0; //Time we last calculated the graph offsets
int _updatePeriod = 5; //Time between server polls, in seconds
timegen_t _graphSpan = 3600000000ll; //Time span of the graph
timegen_t _rescalePeriod = 30000000ll; //Time between graph rescales
timegen_t _lastScale = 0; //Time we last rescaled the graphs
float _rxmax = 0.0;
float _xmax = 0.0;
float _xmin = 0.0;

int _numservers = 0;
char **_serverNames = NULL; //Internet names of each server
int *_serverPorts = NULL; //Port to use to connect to each server
TCPstream **_servers = NULL; //TCPstream connections to each server
bool *_audioServers = NULL; //Flag which servers are polled for audio
int _sampRate = 16000; //The sampling rate of "the" audio server....
IntegPeriod **_data = NULL; //Current data for each system
int *_numData = NULL; //Number of data currently held for each system
IntegPeriod **_procDataC = NULL; //Current data, processed for RFI
int *_numProcDataC = NULL; //Number of processed data for each system
IntegPeriod **_procDataF = NULL; //Current data, processed for RFI
int *_numProcDataF = NULL; //Number of processed data for each system
angle_t *_serverLong = NULL; //Longitude of each system
float *_serverScale1 = NULL; //Data scale factor for input 1
float *_serverScale2 = NULL; //Data scale factor for input 2
float *_serverScaleX = NULL; //Data scale factor for cross correlation
float *_serverOffset1 = NULL; //Data offset for input 1
float *_serverOffset2 = NULL; //Data offset for input 2


//Check the connection to the specified server and reconnect if need be
bool checkServer(int num);
//Reconnect to the specified server
bool reconnectServer(int num);
//Get the latest data for each system
void updateData();
//Purge any expired data
void purgeOldData();
//Perform RFI processing and possibly scale/offset the data
void processData();
//Calculate the scale/offset numbers to apply to the data
void calculateOffsets();
//Draw graphs of the current data
void drawGraphs();
//Applies very crude cleaning for the raw data
void cleanDataCrude(IntegPeriod *&res, int &rescount,
		    IntegPeriod *data, int count);
//Applies an automated cleaning suite to the specified data
void cleanDataFine(IntegPeriod *&res, int &rescount,
		   IntegPeriod *data, int count);

//Print a simple "usage" message and exit
void usage()
{
  cerr << "sacrt - monitor one or more Simple Audio Correlator servers in real-time.\n";
  cerr << "sacrt is free software by David Brodrick (brodrick@fastmail.fm)\n";
  cerr << "USAGE:\n";
  cerr << "-s <servername>\tConnect to the given server, eg \"simple.atnf.csiro.au\"\n";
  cerr << "\t\tGive multiple -s arguments to connect to multiple servers.\n";
  cerr << "-t <time>\tGraph time span in hours, in HH:MM format, eg \"0:30\"\n";
  cerr << "-u <seconds>\tUpdate interval in seconds, eg, \"10\"\n";
  cerr << "-r <seconds>\tRescale the graphs every this many seconds, eg \"60\"\n";
  cerr << "-o\t\tScale the power levels to a common value and then offset them\n";
  cerr << "-LST\t\tShow the time axis in server Local Sidereal Time\n";
  cerr << endl;
  exit(1);
}


/////////////////////////////////////////////////////////////////
int main (int argc, char *argv[])
{
  //Loop through command line arguments and count number of servers
  for (int i=1; i<argc; i++) {
    if (strcmp(argv[i], "-s")==0) _numservers++;
    else if (strcmp(argv[i], "-S")==0) _numservers++;
    else if (strcmp(argv[i], "-server")==0) _numservers++;
    else if (strcmp(argv[i], "--server")==0) _numservers++;
  }
  if (_numservers==0) usage();

  //We now know number of servers, create the corresponding arrays
  _serverNames = new char*[_numservers];
  _serverPorts = new int[_numservers];
  _servers = new TCPstream*[_numservers];
  _audioServers = new bool[_numservers];
  _data = new IntegPeriod*[_numservers];
  _numData = new int[_numservers];
  _procDataC = new IntegPeriod*[_numservers];
  _numProcDataC = new int[_numservers];
  _procDataF = new IntegPeriod*[_numservers];
  _numProcDataF = new int[_numservers];
  _serverLong = new angle_t[_numservers];
  _serverScale1 = new float[_numservers];
  _serverScale2 = new float[_numservers];
  _serverScaleX = new float[_numservers];
  _serverOffset1 = new float[_numservers];
  _serverOffset2 = new float[_numservers];
  for (int i=0; i<_numservers; i++) {
    _serverNames[i] = NULL;
    _serverPorts[i] = 31234; //sac default
    _servers[i] = NULL;
    _audioServers[i] = false;
    _data[i] = NULL;
    _numData[i] = 0;
    _procDataC[i] = NULL;
    _numProcDataC[i] = 0;
    _procDataF[i] = NULL;
    _numProcDataF[i] = 0;
    _serverLong[i] = 0.0;
    _serverScale1[i] = 1.0;
    _serverScale2[i] = 1.0;
    _serverScaleX[i] = 1.0;
    _serverOffset1[i] = 0.0;
    _serverOffset2[i] = 0.0;
  }
  int currserver = 0;
  //Now parse each of the arguments
  for (int i=1; i<argc; i++) {
    string arg = string(argv[i]);
    if (arg == "-t") {
      //Set the time span of the graph
      _graphSpan = -1;
      if (i+2>argc) {
	cerr << "ERROR: The -t argument must be followed by the desired time span of the\n";
	cerr << "graph, in HH:MM 24 hour format, eg 12:00 for 12 hours.\n";
	_graphSpan = -1;
	while (_graphSpan==-1) {
	  cerr << "You can enter the time span of the graph now: ";
	  cin >> arg;
	  _graphSpan = parseTime(arg);
	}
      } else {
	_graphSpan = parseTime(string(argv[i+1]));
	if (_graphSpan == -1) {
	  cerr << "ERROR: The -t argument must be followed by the desired time span of the\n";
	  cerr << "graph, in HH:MM 24 hour format, eg 12:00 for 12 hours.\n\n";
          exit(1);
	}
	i++;
      }
    } else if (arg=="-s"||arg=="-S"||arg=="-server"||arg=="--server") {
      _serverNames[currserver] = argv[i+1];
      i++;
      if (!reconnectServer(currserver)) {
	cerr << "ERROR: Cannot connect to " << _serverNames[currserver] << endl;
	exit(1);
      }
      currserver++;
    } else if (arg == "-u") {
      //Update interval in seconds
      if (i+2>argc) {
	cerr << "ERROR: The -u argument must be followed by an update interval in seconds\n";
	_updatePeriod = -1;
	while (_updatePeriod<0) {
	  cerr << "You can enter update interval now: ";
	  cin >> _updatePeriod;
	  if (cin.fail()) {
	    cerr << "does not compute\n";
            exit(1);
	  }
	  if (_updatePeriod<0) {
            cerr << "Righteo, we're gonna go flat out!\n";
	    _updatePeriod = 0;
	  }
	}
      } else {
        i++;
	_updatePeriod = atoi(argv[i]);
	if (_updatePeriod<0) {
	  cerr << "ERROR: The -u argument must be followed an update interval in seconds\n";
	  exit(1);
	}
      }
    } else if (arg == "-r") {
      //Specify the interval to rescale the graphs in seconds
      if (i+2>argc) {
	cerr << "ERROR: The -r argument must be followed by a time interval in seconds\n";
	cerr << "eg \"-r 30\" to rescale the graphs every 30 seconds\n";
	_rescalePeriod = -1;
	while (_rescalePeriod<=0 || _rescalePeriod>1000000) {
	  cerr << "You can enter the rescale interval in seconds now: ";
	  cin >> _rescalePeriod;
	  if (cin.fail()) {
	    cerr << "does not compute\n";
	    exit(1);
	  }
	}
      } else {
	i++;
	_rescalePeriod = atoi(argv[i]);
	if (_rescalePeriod<=0 || _rescalePeriod>1000000) {
	  cerr << "ERROR: The -r argument must be followed an rescale interval in seconds\n";
          cerr << "eg \"-r 30\" to rescale the graphs every 30 seconds\n";
	  exit(1);
	}
      }
      _rescalePeriod *= 1000000; //Convert to microseconds
    } else if (arg == "-o") {
      //Rescale and offset the receiver data
      _offsetGraphs = true;
    } else if (arg == "-a") {
      //Specify the server to get the audio from
      if (i+2>argc) {
	//PRINT AN ERROR MESSAGE
	cerr << "ERROR: The -a option must be followed by a server name\n";
	cerr << "\tIt is used to tell the program to download audio from that server\n";
	exit(1);
      }
      bool foundone = false;
      int q=0;
      for (; q<currserver; q++) {
	if (strcmp(argv[i+1], _serverNames[q])==0) {
	  foundone = true;
	  break;
	}
      }
      if (!foundone) {
	cerr << "ERROR: The name \"" << argv[i] << "\" after -a isn't recognised\n";
	cerr << "\tThe -a option must be follwed by a server name argument\n";
	cerr << "\tIt is used to tell the program to download audio from that server\n";
	cerr << "HINT: Be sure to use the -a option *after* the -s server declaration\n";
	exit(1);
      }
      _audioServers[q] = true;
      i += 2;
    } else if (arg == "-LST") {
      //Show data by telescope Local Sidereal Time
      _LST = true;
    } else {
      cerr << "UNKNOWN ARGUMENT \"" << arg << "\"\n";
      usage();
    }
  }
  //Next need to work out graph start time and download bulk of data.
  for (int i=0; i<_numservers; i++) {
    timegen_t now = getAbs();
    timegen_t start = now - _graphSpan;
    cout << "Initialising data from " << _serverNames[i] << endl;
    IntegPeriod::load(_data[i], _numData[i], now, start, *_servers[i], false);
  }

  if (cpgbeg(0, "/XS", 2, 1) != 1)
//  if (cpgbeg(0, "/tmp/graph.ps/CPS", 2, 2) != 1)
    exit(1);
  cpgsvp(0.05,0.95,0.1,0.9);
  PlotArea::setPopulation(2);
  cpgsch(1.2);

  processData();
  if (_offsetGraphs) calculateOffsets();
  drawGraphs();

  //Start main program loop
  while (true) {
    sleep(_updatePeriod);
    updateData();
    purgeOldData();
    processData();
    if (_offsetGraphs) calculateOffsets();
    drawGraphs();
    cerr << endl;
  }
}


/////////////////////////////////////////////////////////////////
//Get the latest data for each system
void updateData()
{
  for (int i=0; i<_numservers; i++) {
    if (!checkServer(i)) {
      cerr << "No connection to " << _serverNames[i] << endl;
      continue;
    }
    timegen_t lastdata;
    if (_data[i]!=NULL && _numData[i]>0) {
      lastdata = _data[i][_numData[i]-1].timeStamp;
    } else {
      lastdata = getAbs() - _graphSpan;
    }

    IntegPeriod *newdata = NULL;
    int newlen = 0;

    cout << "Polling " << _serverNames[i] << endl;
    IntegPeriod::load(newdata, newlen, lastdata, 0, *_servers[i], false);

    if (newdata==NULL||newlen==0) {
      cerr << "Got no data from " << _serverNames[i] << endl;
    } else {
      if (_data[i]!=NULL && _numData[i]>0) {
	int biglen = _numData[i] + newlen;
	IntegPeriod *bigdata = new IntegPeriod[biglen];
	for (int j=0; j<_numData[i]; j++) {
	  bigdata[j] = _data[i][j];
	}
	for (int j=0; j<newlen; j++) {
	  bigdata[_numData[i]+j] = newdata[j];
	}
	delete[] _data[i];
	delete[] newdata;
	_data[i] = bigdata;
        _numData[i] = biglen;
      } else {
	_data[i] = newdata;
        _numData[i] = newlen;
      }
    }
  }
}


/////////////////////////////////////////////////////////////////
//Purge any expired data
void purgeOldData()
{
  timegen_t expirytime = getAbs() - _graphSpan;
  for (int i=0; i<_numservers; i++) {
    if (_data[i]==NULL || _numData[i]==0) continue;
    int j=0;
    while (j<_numData[i] && _data[i][j].timeStamp<expirytime) j++;
    int newlen = _numData[i] - j;
    if (newlen<=0) {
      _numData[i] = 0;
      if (_data[i]!=NULL) delete[] _data[i];
      _data[i] = NULL;
    } else if (j!=0) {
      IntegPeriod *newdata = new IntegPeriod[newlen];
      for (int k=j; k<_numData[i]; k++) {
        newdata[k-j] = _data[i][k];
      }
      delete[] _data[i];
      _data[i] = newdata;
      _numData[i] = newlen;
      cout << "Purged " << j << " old integration periods for "
	   << _serverNames[i] << endl;

    }
  }
}


/////////////////////////////////////////////////////////////////
//Draw graphs of the current data
void drawGraphs()
{
  if (_lastScale==0 || getAbs()-_lastScale>_rescalePeriod) {
    float rawmax = 0.0;
    for (int i=0; i<_numservers; i++) {
      if (_RFI) {
	if (_procDataC[i]==NULL || _numProcDataC[i]==0) continue;
	for (int j=0; j<_numProcDataC[i]; j++) {
	  if (_procDataC[i][j].power1>rawmax) rawmax = _procDataC[i][j].power1;
	  if (_procDataC[i][j].power2>rawmax) rawmax = _procDataC[i][j].power2;
	}
      } else {
	if (_data[i]==NULL || _numData[i]==0) continue;
	for (int j=0; j<_numData[i]; j++) {
	  if (_data[i][j].power1>rawmax) rawmax = _data[i][j].power1;
	  if (_data[i][j].power2>rawmax) rawmax = _data[i][j].power2;
	}
      }
    }
    _rxmax = rawmax;
  }

  PlotArea *p = PlotArea::getPlotArea(0);
  p->setTitle("(A) Raw Detected Power");
  p->setAxisY("Power", _rxmax, 0.0, false);
  if (_LST) {
    p->setAxisX("Local Sidereal Time (Hours)", 24.0, 0.0, false);
  } else {
    p->setAxisX("Time (Hours)", 0.0, -_graphSpan/3600000000.0, false);
  }

  timegen_t now = getAbs();


  for (int i=0; i<_numservers; i++) {
    if (_procDataC[i]==NULL || _numProcDataC[i]<=0) continue;
    float tdata[_numProcDataC[i]];
    float p1data[_numProcDataC[i]];
    float p2data[_numProcDataC[i]];
    for (int j=0; j<_numProcDataC[i]; j++) {
      p1data[j] = _procDataC[i][j].power1;
      p2data[j] = _procDataC[i][j].power2;
      if (_LST) {
	tdata[j] = Abs2LST(_procDataC[i][j].timeStamp, _serverLong[i])/3600000000.0;
      } else {
        tdata[j] = (_procDataC[i][j].timeStamp-now)/3600000000.0;
      }
    }
    p->plotPoints(_numProcDataC[i], tdata, p1data, 2*i+2);
    p->plotPoints(_numProcDataC[i], tdata, p2data, 2*i+1+2);
  }

  if (_lastScale==0 || getAbs()-_lastScale>_rescalePeriod) {
    _lastScale = getAbs();
    float xmax = 0.0;
    float xmin = 0.0;
    for (int i=0; i<_numservers; i++) {
      if (_procDataF[i]==NULL || _numProcDataF[i]==0) continue;
      for (int j=0; j<_numProcDataF[i]; j++) {
	if (_procDataF[i][j].powerX>xmax) xmax = _procDataF[i][j].powerX;
	if (_procDataF[i][j].powerX<xmin) xmin = _procDataF[i][j].powerX;
      }
    }
    _xmax = xmax;
    _xmin = xmin;
  }

  p = PlotArea::getPlotArea(1);
  p->setTitle("(B) Interferometer Output");
  p->setAxisY("Power", _xmax+_xmax/20.0, _xmin-_xmin/20.0, false);
  if (_LST) {
    p->setAxisX("Time (Hours)", 24.0, 0.0, false);
  } else {
    p->setAxisX("Time (Hours)", 0.0, -_graphSpan/3600000000.0, false);
  }
  //Plot the raw fringe data as dots
  for (int i=0; i<_numservers; i++) {
      if (_procDataC[i]==NULL || _numProcDataC[i]==0) continue;
    float tdata[_numProcDataC[i]];
    float xdata[_numProcDataC[i]];
    for (int j=0; j<_numProcDataC[i]; j++) {
      xdata[j] = _procDataC[i][j].powerX;
      if (_LST) {
	tdata[j] = Abs2LST(_procDataC[i][j].timeStamp, _serverLong[i])/3600000000.0;
      } else {
        tdata[j] = (_procDataC[i][j].timeStamp-now)/3600000000.0;
      }
    }
    p->plotPoints(_numProcDataC[i], tdata, xdata, 2*i+2);
  }
  //Plot the fully processed fringe data as lines
  for (int i=0; i<_numservers; i++) {
    if (_procDataF[i]==NULL || _numProcDataF[i]==0) continue;
    float tdata[_numProcDataF[i]];
    float xdata[_numProcDataF[i]];
    for (int j=0; j<_numProcDataF[i]; j++) {
      xdata[j] = _procDataF[i][j].powerX;
      if (_LST) {
	tdata[j] = Abs2LST(_procDataF[i][j].timeStamp, _serverLong[i])/3600000000.0;
      } else {
	tdata[j] = (_procDataF[i][j].timeStamp-now)/3600000000.0;
      }
    }
    p->plotLine(_numProcDataF[i], tdata, xdata, 2*i+1+2);
  }
}


/////////////////////////////////////////////////////////////////
//Check the connection to the specified server and reconnect if need be
bool checkServer(int num)
{
  if (_servers[num]!=NULL && _servers[num]->good() &&
      _servers[num]->rdbuf()->is_open() && !_servers[num]->eof()) return true;
  else return reconnectServer(num);
}


/////////////////////////////////////////////////////////////////
//Reconnect to the specified server
bool reconnectServer(int num)
{
  if (_servers[num]!=NULL) delete _servers[num];
  _servers[num] = new TCPstream();
  cout << "Reconnecting to " << _serverNames[num] << endl;
  string server = _serverNames[num];
  int port = _serverPorts[num];

  SocketAddr server_addr = SocketAddr(IPaddress(server.c_str()), port);

  int listening_socket = socket(AF_INET,SOCK_STREAM,0);
  if( listening_socket < 0 )
    CurrentNetCallback::on_error("Socket creation error");

  int value = 1;
  if( setsockopt(listening_socket, SOL_SOCKET, SO_REUSEADDR,
		 (char*)&value, sizeof(value)) < 0 )
    CurrentNetCallback::on_error("setsockopt SO_REUSEADDR error");
  struct timeval tv = {10, 0};
  if( setsockopt(listening_socket, SOL_SOCKET, SO_RCVTIMEO,
		 (char*)&tv, sizeof(tv)) < 0 )
    CurrentNetCallback::on_error("setsockopt SO_REUSEADDR error");

  if (_servers[num]->rdbuf()->connect(server_addr)!=NULL) {
      _servers[num]->rdbuf()->set_blocking_io(true);

      //Clear any old error flags
      _servers[num]->clear();

      if (_servers[num]->good() && _servers[num]->rdbuf()->is_open()
	  && !_servers[num]->eof()) {
	//Ask the server for it's location
        pair_t loc;
	*_servers[num] << "LOCATION\n";
	*_servers[num] >> loc.c1 >> loc.c2;
	if (_servers[num]->fail() || _servers[num]->eof()) {
	  cerr << "Error reading location from server\n";
	  _servers[num]->close();
	  return false;
	}
	if (loc.c1>180.0 || loc.c1<-180.0 || loc.c2>90.0 || loc.c2<-90.0) {
	  cerr << "Returned location was invalid: " << loc.c1 << " " << loc.c2 << endl;
	  _servers[num]->close();
	  return false;
	}
	cout << "Telescope location is: " << loc.c1 << " " << loc.c2 << endl;
	loc.c1 = PI*loc.c1/180.0;
	loc.c2 = PI*loc.c2/180.0;
        _serverLong[num] = loc.c1;
	return true;
      }
  }
  _servers[num]->close();
  cout << "Failed to connect to " << _serverNames[num] << endl;
  return false;
}


/////////////////////////////////////////////////////////////////
//Perform RFI processing and possibly scale/offset the data
void processData()
{
  //Perform two levels of RFI processing
  for (int i=0; i<_numservers; i++) {
    cerr << "PROCESSING " << _serverNames[i] << endl;
    //Delete old crude data
    if (_procDataC[i]!=NULL&&_numProcDataC[i]>0) {
      delete[] _procDataC[i];
      _procDataC[i] = NULL;
      _numProcDataC[i] = 0;
    }
    if (_data[i]!=NULL && _numData[i]>0) {
      //Cant process it if we dont have it..
      cleanDataCrude(_procDataC[i], _numProcDataC[i],
		     _data[i], _numData[i]);
      cerr << "GOT " << _numProcDataC[i] << " CRUDE DATA FOR " << _serverNames[i] << endl;
    }

    if (i>0) {
      if (_offsetGraphs) IntegPeriod::rescale(_procDataC[i], _numProcDataC[i],
			   _procDataF[0], _numProcDataF[0]);
      //Delete old fine data
      if (_procDataF[i]!=NULL && _numProcDataF[i]!=0) {
	delete[] _procDataF[i];
	_procDataF[i] = NULL;
	_numProcDataF[i] = 0;
      }
      if (_procDataC[i]!=NULL && _numProcDataC[i]>0) {
	cleanDataFine(_procDataF[i], _numProcDataF[i],
		      _procDataC[i], _numProcDataC[i]);
	cerr << "GOT " << _numProcDataF[i] << "  FINE DATA FOR " << _serverNames[i] << endl;
      }
    } else {
      //Delete old fine data
      if (_procDataF[i]!=NULL && _numProcDataF[i]!=0) {
	delete[] _procDataF[i];
	_procDataF[i] = NULL;
	_numProcDataF[i] = 0;
      }
      if (_procDataC[i]!=NULL && _numProcDataC[i]>0) {
	cleanDataFine(_procDataF[i], _numProcDataF[i],
		      _procDataC[i], _numProcDataC[i]);
	cerr << "GOT " << _numProcDataF[i] << "  FINE DATA FOR " << _serverNames[i] << endl;
      }
      if (_offsetGraphs) IntegPeriod::rescale(_procDataC[i], _numProcDataC[i],
			   _procDataF[0], _numProcDataF[0]);
      //Delete old fine data
      if (_procDataF[i]!=NULL && _numProcDataF[i]!=0) {
	delete[] _procDataF[i];
	_procDataF[i] = NULL;
	_numProcDataF[i] = 0;
      }
      if (_procDataC[i]!=NULL && _numProcDataC[i]>0) {
	cleanDataFine(_procDataF[i], _numProcDataF[i],
		      _procDataC[i], _numProcDataC[i]);
	cerr << "GOT " << _numProcDataF[i] << "  FINE DATA FOR " << _serverNames[i] << endl;
      }
    }
  }
}


/////////////////////////////////////////////////////////////////
//Calculate scales and offsets to apply to the data
void calculateOffsets()
{
  if (_offsetGraphs &&_numProcDataF[0]>0 && _procDataF[0]!=NULL) {
    //Find the maximum so that we can offset the others correctly
    float maxIn1 = 0.0;
    for (int i=0; i<_numProcDataF[0]; i++) {
      if (_procDataF[0][i].power1>maxIn1) maxIn1 = _procDataF[0][i].power1;
    }

    for (int i=0; i<_numservers; i++) {
      if (_procDataC[i]==NULL || _numProcDataC[i]==0 ||
	  _procDataF[i]==NULL || _numProcDataF[i]==0) continue;
      for (int j=0; j<_numProcDataC[i]; j++) {
	_procDataC[i][j].power1 = _procDataC[i][j].power1 + (i*2)*(maxIn1/3.0);
	_procDataC[i][j].power2 = _procDataC[i][j].power2 + (i*2+1)*(maxIn1/3.0);
      }
      for (int j=0; j<_numProcDataF[i]; j++) {
	_procDataF[i][j].power1 = _procDataF[i][j].power1 + (i*2)*(maxIn1/3.0);
	_procDataF[i][j].power2 = _procDataF[i][j].power2 + (i*2+1)*(maxIn1/3.0);
      }
    }
  }
}


/////////////////////////////////////////////////////////////////
//Applies very crude cleaning for the raw data
void cleanDataCrude(IntegPeriod *&res, int &rescount,
		    IntegPeriod *data, int count)
{
  powerClean(data, count, 1.2, 15);
  IntegPeriod::purgeFlagged(res, rescount, data, count);
}


/////////////////////////////////////////////////////////////////
//Applies an automated cleaning suite to the specified data
void cleanDataFine(IntegPeriod *&res, int &rescount,
		   IntegPeriod *data, int count)
{
  if (data==NULL || count==0) return;

  res = NULL;
  rescount = 0;
  IntegPeriod *tempdata = NULL, *tempdata2 = NULL;
  int tempcount = 0, tempcount2 = 0;

  markOutliers(data, count, 1.0, 20);
  IntegPeriod::purgeFlagged(tempdata, tempcount, data, count);
  if (tempdata==NULL || tempcount==0) return;


  powerClean(tempdata, tempcount, 1.15, 20);
  IntegPeriod::purgeFlagged(tempdata2, tempcount2, tempdata, tempcount);
  delete[] tempdata;
  if (tempdata2==NULL || tempcount2==0) return;

  //Perform second pass
  markOutliers(tempdata2, tempcount2, 5.0, 30, true);
  //IntegPeriod::purgeFlagged(res, rescount, tempdata2, tempcount2);
  IntegPeriod::integrate(res, rescount, tempdata2, tempcount2, 20000000);
  delete[] tempdata2;
}

