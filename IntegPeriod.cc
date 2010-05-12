//
// Copyright (C) David Brodrick
// Copyright (C) CSIRO Australia Telescope National Facility
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.
//
// $Id: IntegPeriod.cc,v 1.24 2004/11/21 01:58:21 brodo Exp brodo $

//Class to encapsulate the information produced each telescope integration
//period. This wraps up most operations which can be performed on an
//integration period, such as correlating raw data, printing the data
//in a simple ASCII text format, writing and saving data from disk, etc.

#include <IntegPeriod.h>
#include <TimeCoord.h>
#include <RFI.h>
#include <sstream>
#include <iostream>
#include <string>
#include <time.h>
#include <assert.h>

///////////////////////////////////////////////////////////////////////
//Constructor
IntegPeriod::IntegPeriod()
  :audioLen(0),
  rawAudio(0),
  numBins(-1),
  input1Spec(0),
  input2Spec(0),
  crossSpec(0),
  phaseSpec(0),
  power1(0.0),
  power2(0.0),
  powerX(0.0),
  amplitude(0.0),
  phase(0.0),
  RFI(false),
  audio1(0),
  audio2(0)
{
}


///////////////////////////////////////////////////////////////////////
//Destructor
IntegPeriod::~IntegPeriod()
{
  if (phaseSpec)  delete[] phaseSpec;
  if (crossSpec)  delete[] crossSpec;
  if (input1Spec) delete[] input1Spec;
  if (input2Spec) delete[] input2Spec;
  if (rawAudio)   delete[] rawAudio;
  if (audio1)     delete[] audio1;
  if (audio2)     delete[] audio2;
}


///////////////////////////////////////////////////////////////////////
//Convert intervleaved audio to two zero mean signed arrays
void IntegPeriod::processAudio()
{
  //Create storage space for the processed audio
  if (audio1) delete[] audio1;
  audio1 = new float[audioLen];
  if (audio2) delete[] audio2;
  audio2 = new float[audioLen];

  //Pull out the LEFT channel and convert to float
  double mean1=0.0, mean2=0.0;
  for (int cnt=0; cnt<audioLen; cnt++) {
    audio1[cnt] = static_cast<float>(rawAudio[2*cnt]);
    audio2[cnt] = static_cast<float>(rawAudio[2*cnt+1]);
    mean1 += audio1[cnt];
    mean2 += audio2[cnt];
  }
  //Calculate mean and subtract from each sample, this converts
  //the data to signed with zero mean, removing any DC offset.
  mean1 = (mean1/(double)audioLen);
  mean2 = (mean2/(double)audioLen);
  for (int cnt=0; cnt<audioLen; cnt++) {
    audio1[cnt]-=(float)mean1;
  }
  for (int cnt=0; cnt<audioLen; cnt++) {
    audio2[cnt]-=(float)mean2;
  }
}


///////////////////////////////////////////////////////////////////////
//Perform correlations for each input and the cross product of them
void IntegPeriod::doCorrelations()
{
  processAudio();

  power1 = correlate(audioLen, audio1, audio1);
  power2 = correlate(audioLen, audio2, audio2);
  powerX = correlate(audioLen, audio1, audio2);

  power1/=audioLen;
  power2/=audioLen;
  powerX/=audioLen;

  amplitude = phase = 0.0;

  delete[] audio1; audio1 = NULL;
  delete[] audio2; audio2 = NULL;
}


///////////////////////////////////////////////////////////////////////
//
float IntegPeriod::correlate(int len, float *ch1, float *ch2)
{
  double res = 0.0;
  for (int i=0; i<len; i++) res += ch1[i]*ch2[i];
  return (float)res;
}


///////////////////////////////////////////////////////////////////////
//Discard any data except the indicated data
void IntegPeriod::keepOnly(bool cross, bool inputs, bool audio)
{
  if (!cross && phaseSpec) {
    delete[] phaseSpec;
    phaseSpec = 0;
  }
  if (!cross && crossSpec) {
    delete[] crossSpec;
    crossSpec = 0;
  }
  if (!inputs && input1Spec) {
    delete[] input1Spec;
    input1Spec = 0;
  }
  if (!inputs && input2Spec) {
    delete[] input2Spec;
    input2Spec = 0;
  }
  if (!audio && rawAudio) {
    delete[] rawAudio;
    rawAudio = 0;
  }
}


///////////////////////////////////////////////////////////////////////
//Display operator for cout
ostream &operator<<(ostream& os, IntegPeriod& per)
{
  time_t tstamp = per.timeStamp/1000000;
  struct tm *utc = gmtime(&tstamp);
//  char *timestr = asctime(utc);
//  os << "PERIOD START:\t" << timestr;
  os << utc->tm_year+1900 <<"-"<< utc->tm_mon+1  <<"-"<< utc->tm_mday
      << "-" << utc->tm_hour << ":"<< utc->tm_min << ":" << utc->tm_sec;

  //Write the powers first
//  os << "CROSS  POWER:\t" << per.powerX << endl;
//  os << "INPUT1 POWER:\t" << per.power1 << endl;
//  os << "INPUT2 POWER:\t" << per.power2 << endl;
//  os << "AMPLITUDE   :\t" << per.amplitude << endl;
//  os << "PHASE       :\t" << per.phase << endl;

  os << " " << per.power1;
  os << " " << per.power2;
  os << " " << per.powerX;
  os << " " << per.amplitude;
  os << " " << per.phase;

  if (per.RFI) {
    os << " RFI";
  }


#if (0)
  for (int i=0; i<per.numBins; i++) {
    os << per.input1Spec[i] << " ";
  }
  os << endl;
#endif
  os << endl;

  return os;
}

///////////////////////////////////////////////////////////////////////
//Operator for saving to a file
ofstream &operator<<(ofstream& os, const IntegPeriod& per)
{
  //Caclculate the total length we will save to assist
  //with faster seeks through large files
  int len = sizeof(long long) + 5*sizeof(float)
    + 3*sizeof(int) + 5;
  if (per.phaseSpec)  len += per.numBins*sizeof(float);
  if (per.crossSpec)  len += per.numBins*sizeof(float);
  if (per.input1Spec) len += per.numBins*sizeof(float);
  if (per.input2Spec) len += per.numBins*sizeof(float);
  if (per.rawAudio)   len += 2*sizeof(audio_t)*per.audioLen;

  //Write the size to the file
  os.write((char*)&len, sizeof(int));

  //Write the timestamp
  os.write((char*)&per.timeStamp, sizeof(long long));
  //Write the powers
  os.write((char*)&per.powerX, sizeof(float));
  os.write((char*)&per.power1, sizeof(float));
  os.write((char*)&per.power2, sizeof(float));
  os.write((char*)&per.amplitude, sizeof(float));
  os.write((char*)&per.phase,     sizeof(float));
  //Write the number of spectral channels
  os.write((char*)&per.numBins, sizeof(int));
  //Write if the block has identified interference
  if (per.RFI)  os << "R"; else os << " ";
  //Write out which spectra we will be saving
  if (per.phaseSpec)  os << "P"; else os << " ";
  if (per.crossSpec)  os << "X"; else os << " ";
  if (per.input1Spec) os << "1"; else os << " ";
  if (per.input2Spec) os << "2"; else os << " ";
  //Write out the spectral data
  if (per.phaseSpec)
    os.write((char*)per.phaseSpec,  per.numBins*sizeof(float));
  if (per.crossSpec)
    os.write((char*)per.crossSpec,  per.numBins*sizeof(float));
  if (per.input1Spec)
    os.write((char*)per.input1Spec, per.numBins*sizeof(float));
  if (per.input2Spec)
    os.write((char*)per.input2Spec, per.numBins*sizeof(float));
  //Write out the raw audio data
  if (per.rawAudio) {
    os.write((char*)&per.audioLen, sizeof(int));
    os.write((char*)per.rawAudio, 2*sizeof(audio_t)*per.audioLen);
  } else {
    int temp = 0;
    os.write((char*)&temp, sizeof(int));
  }

  return os;
}


///////////////////////////////////////////////////////////////////////
//Operator for writing across network
TCPstream &operator<<(TCPstream& os, const IntegPeriod& per)
{
  //Caclculate the total length we will save to assist
  //with faster seeks through large files
  int len = sizeof(long long) + 5*sizeof(float)
    + 3*sizeof(int) + 5;
  if (per.phaseSpec)  len += per.numBins*sizeof(float);
  if (per.crossSpec)  len += per.numBins*sizeof(float);
  if (per.input1Spec) len += per.numBins*sizeof(float);
  if (per.input2Spec) len += per.numBins*sizeof(float);
  if (per.rawAudio)   len += 2*sizeof(audio_t)*per.audioLen;

  //Write the size to the file
  os.write((char*)&len, sizeof(int));

  //Write the timestamp
  os.write((char*)&per.timeStamp, sizeof(long long));
  //Write the powers
  os.write((char*)&per.powerX, sizeof(float));
  os.write((char*)&per.power1, sizeof(float));
  os.write((char*)&per.power2, sizeof(float));
  os.write((char*)&per.amplitude, sizeof(float));
  os.write((char*)&per.phase,     sizeof(float));
  //Write the number of spectral channels
  os.write((char*)&per.numBins, sizeof(int));
  //Write if the block has identified interference
  if (per.RFI)  os << "R"; else os << " ";
  //Write out which spectra we will be saving
  if (per.phaseSpec)  os << "P"; else os << " ";
  if (per.crossSpec)  os << "X"; else os << " ";
  if (per.input1Spec) os << "1"; else os << " ";
  if (per.input2Spec) os << "2"; else os << " ";
  //Write out the spectral data
  if (per.phaseSpec)
    os.write((char*)per.phaseSpec,  per.numBins*sizeof(float));
  if (per.crossSpec)
    os.write((char*)per.crossSpec,  per.numBins*sizeof(float));
  if (per.input1Spec)
    os.write((char*)per.input1Spec, per.numBins*sizeof(float));
  if (per.input2Spec)
    os.write((char*)per.input2Spec, per.numBins*sizeof(float));
  //Write out the raw audio data
  if (per.rawAudio) {
    os.write((char*)&per.audioLen, sizeof(int));
    os.write((char*)per.rawAudio, 2*sizeof(audio_t)*per.audioLen);
  } else {
    int temp = 0;
    os.write((char*)&temp, sizeof(int));
  }

  return os;
}


///////////////////////////////////////////////////////////////////////
//Operator for recovering from a serialised state
istream &operator>>(istream& is, IntegPeriod& per)
{
  int tempint;
  //Read how many bytes, though we don't use it here
  is.read((char*)&tempint, sizeof(int));
  //Read in the time stamp
  is.read((char*)&per.timeStamp, sizeof(long long));
  //Read in the powers
  is.read((char*)&per.powerX, sizeof(float));
  is.read((char*)&per.power1, sizeof(float));
  is.read((char*)&per.power2, sizeof(float));
  is.read((char*)&per.amplitude, sizeof(float));
  is.read((char*)&per.phase, sizeof(float));

  //Read the number of spectral channels
  is.read((char*)&per.numBins, sizeof(int));
  //Determine which frequency spectra were saved
  char tempchar;
  is.read((char*)&tempchar, sizeof(char));
  if (tempchar=='R') per.RFI = true;
  else if (tempchar!=' ') {
    //cerr << "R:PARSE ERROR WHILE LOADING IntegPeriod (" << tempchar <<")\n";
    return is;
  } else {
      per.RFI = false;
  }

  bool getP = false;
  is.read((char*)&tempchar, sizeof(char));
  if (tempchar=='P') getP = true;
  else if (tempchar!=' ') {
    //cerr << "P:PARSE ERROR WHILE LOADING IntegPeriod (" << tempchar <<")\n";
    return is;
  }
  bool getX = false;
  is.read((char*)&tempchar, sizeof(char));
  if (tempchar=='X') getX = true;
  else if (tempchar!=' ') {
    //cerr << "X:PARSE ERROR WHILE LOADING IntegPeriod\n";
    return is;
  }
  bool get1 = false;
  is.read((char*)&tempchar, sizeof(char));
  if (tempchar=='1') get1 = true;
  else if (tempchar!=' ') {
    //cerr << "1:PARSE ERROR WHILE LOADING IntegPeriod\n";
    return is;
  }
  bool get2 = false;
  is.read((char*)&tempchar, sizeof(char));
  if (tempchar=='2') get2 = true;
  else if (tempchar!=' ') {
    //cerr << "2:PARSE ERROR WHILE LOADING IntegPeriod\n";
    return is;
  }
  //Load those spectra which were saved
  if (per.phaseSpec) delete[] per.phaseSpec;
  if (per.crossSpec) delete[] per.crossSpec;
  if (per.input1Spec) delete[] per.input1Spec;
  if (per.input2Spec) delete[] per.input2Spec;
  if (getP) {
    per.phaseSpec = new float[per.numBins];
    is.read((char*)per.phaseSpec, per.numBins*sizeof(float));
  }
  if (getX) {
    per.crossSpec = new float[per.numBins];
    is.read((char*)per.crossSpec, per.numBins*sizeof(float));
  }
  if (get1) {
    per.input1Spec = new float[per.numBins];
    is.read((char*)per.input1Spec, per.numBins*sizeof(float));
  }
  if (get2) {
    per.input2Spec = new float[per.numBins];
    is.read((char*)per.input2Spec, per.numBins*sizeof(float));
  }
  //Read length of audio which was saved
  is.read((char*)&tempint, sizeof(int));
  per.audioLen = tempint;
  if (tempint != 0) {
    per.rawAudio = new audio_t[2*tempint];
    is.read((char*)per.rawAudio, 2*sizeof(audio_t)*per.audioLen);
  }
  return is;
}


///////////////////////////////////////////////////////////////////////
IntegPeriod IntegPeriod::operator+(IntegPeriod &rhs)
{
  IntegPeriod res;
//  int i;
  //Take the earliest timestamp
  if (timeStamp<rhs.timeStamp && timeStamp!=0)
    res.timeStamp = timeStamp;
  else
    res.timeStamp = rhs.timeStamp;

  //  assert(numBins==rhs.numBins);

  //Add powers
  res.powerX = powerX + rhs.powerX;
  res.power1 = power1 + rhs.power1;
  res.power2 = power2 + rhs.power2;
  float x = amplitude*cos(phase) + rhs.amplitude*cos(rhs.phase);
  float y = amplitude*sin(phase) + rhs.amplitude*sin(rhs.phase);
  res.amplitude = sqrt(x*x + y*y);
  res.phase     = atan2(y,x);

  //Check RFI
  if (RFI || rhs.RFI) res.RFI = true;

  //Accumulate the frequency spectra
/*  if (this->crossSpec&&rhs.crossSpec) {
    res.crossSpec = new float[this->numBins];
    cerr << res.crossSpec << " " << this->crossSpec << " " <<rhs.crossSpec<<endl;
    for (i=0; i<rhs.numBins; i++) {
      res.crossSpec[i] = (this->crossSpec[i]+rhs.crossSpec[i]);
    }
  }*/
/*  if (this->input1Spec&&rhs.input1Spec) {
    res.input1Spec = new float[this->numBins];
    for (i=0; i<this->numBins; i++)
      res.input1Spec[i] = this->input1Spec[i]+rhs.input1Spec[i];
  }*/
/*  if (this->input2Spec&&rhs.input2Spec) {
    res.input2Spec = new float[this->numBins];
    for (i=0; i<this->numBins; i++)
      res.input2Spec[i] = this->input2Spec[i]+rhs.input2Spec[i];
  }
*/
  //Concatenate the audio
/*  if (rawAudio&&rhs.rawAudio) {
    res.rawAudio = new audio_t[2*(audioLen+rhs.audioLen)];
    if (rhs>timeStamp) {
      for (i=0; i<2*audioLen; i++)
	res.rawAudio[i] = rawAudio[i];
      for (; i<2*audioLen+2*rhs.audioLen; i++)
	res.rawAudio[i] = rhs.rawAudio[i-2*audioLen];
    } else {
      for (i=0; i<2*rhs.audioLen; i++)
	res.rawAudio[i] = rhs.rawAudio[i];
      for (; i<2*audioLen+2*rhs.audioLen; i++)
	res.rawAudio[i] = rawAudio[i-2*rhs.audioLen];
    }
  } else if (this->rawAudio) {
    res.rawAudio = new audio_t[2*this->audioLen];
    for (i=0; i<2*this->audioLen; i++)
      res.rawAudio[i] = this->rawAudio[i];
  } else if (rhs.rawAudio) {
    res.rawAudio = new audio_t[2*rhs.audioLen];
    for (i=0; i<2*rhs.audioLen; i++)
      res.rawAudio[i] = rhs.rawAudio[i];
  }
  res.audioLen = this->audioLen+rhs.audioLen;
  */
  return res;
}

///////////////////////////////////////////////////////////////////////
void IntegPeriod::operator+=(IntegPeriod &rhs)
{
  //Take the earliest timestamp
  if (timeStamp>rhs.timeStamp || timeStamp==0)
    timeStamp = rhs.timeStamp;

  //assert(numBins==-1||numBins==rhs.numBins);
  numBins=rhs.numBins;

  //Add powers
  powerX = powerX + rhs.powerX;
  power1 = power1 + rhs.power1;
  power2 = power2 + rhs.power2;
  float x = amplitude*cos(phase) + rhs.amplitude*cos(rhs.phase);
  float y = amplitude*sin(phase) + rhs.amplitude*sin(rhs.phase);
  amplitude = sqrt(x*x + y*y);
  phase     = atan2(y,x);

  //Check RFI
  if (RFI || rhs.RFI) RFI = true;
}

///////////////////////////////////////////////////////////////////////
IntegPeriod IntegPeriod::operator/(int div)
{
  IntegPeriod res;
  //time stamp
  res.timeStamp = timeStamp;
  //powers
  res.powerX = powerX/(float)div;
  res.power1 = power1/(float)div;
  res.power2 = power2/(float)div;
  res.amplitude = amplitude/(float)div;
  res.phase     = phase;

  return res;
}

///////////////////////////////////////////////////////////////////////
void IntegPeriod::operator/=(int div)
{
  //powers
  powerX /= (float)div;
  power1 /= (float)div;
  power2 /= (float)div;
  amplitude /= (float)div;
}

///////////////////////////////////////////////////////////////////////
int IntegPeriod::operator==(long long tstamp)
{
  if (tstamp == timeStamp) return 1;
  else return 0;
}

///////////////////////////////////////////////////////////////////////
int IntegPeriod::operator<=(long long tstamp)
{
  if (tstamp <= timeStamp) return 1;
  else return 0;
}

///////////////////////////////////////////////////////////////////////
int IntegPeriod::operator>=(long long tstamp)
{
  if (tstamp >= timeStamp) return 1;
  else return 0;
}

///////////////////////////////////////////////////////////////////////
int IntegPeriod::operator<(long long tstamp)
{
  if (tstamp < timeStamp) return 1;
  else return 0;
}

///////////////////////////////////////////////////////////////////////
int IntegPeriod::operator>(long long tstamp)
{
  if (tstamp > timeStamp) return 1;
  else return 0;
}

///////////////////////////////////////////////////////////////////////
signed long long IntegPeriod::operator-(long long tstamp)
{
  return timeStamp-tstamp;
}


///////////////////////////////////////////////////////////////////////
int IntegPeriod::operator==(IntegPeriod &rhs)
{
  if (rhs.timeStamp == timeStamp) return 1;
  else return 0;
}

///////////////////////////////////////////////////////////////////////
int IntegPeriod::operator<=(IntegPeriod &rhs)
{
  if (timeStamp <= rhs.timeStamp) return 1;
  else return 0;
}

///////////////////////////////////////////////////////////////////////
int IntegPeriod::operator>=(IntegPeriod &rhs)
{
  if (timeStamp >= rhs.timeStamp) return 1;
  else return 0;
}

///////////////////////////////////////////////////////////////////////
int IntegPeriod::operator<(IntegPeriod &rhs)
{
  if (timeStamp < rhs.timeStamp) return 1;
  else return 0;
}

///////////////////////////////////////////////////////////////////////
int IntegPeriod::operator>(IntegPeriod &rhs)
{
  if (timeStamp > rhs.timeStamp) return 1;
  else return 0;
}

///////////////////////////////////////////////////////////////////////
float IntegPeriod::operator[](int index)
{
  if (crossSpec && index>=0 && index<numBins)
    return crossSpec[index];
  else return 0;
}

///////////////////////////////////////////////////////////////////////
signed long long IntegPeriod::operator-(IntegPeriod &rhs)
{
  return timeStamp-rhs.timeStamp;
}


///////////////////////////////////////////////////////////////////////
void IntegPeriod::operator=(const IntegPeriod &rhs)
{
  numBins = rhs.numBins;
  timeStamp = rhs.timeStamp;
  RFI = rhs.RFI;
  audioLen = rhs.audioLen;
  powerX = rhs.powerX;
  power1 = rhs.power1;
  power2 = rhs.power2;
  amplitude = rhs.amplitude;
  phase = rhs.phase;

  //Ensure we remove old data
  if (phaseSpec) {
    delete[] phaseSpec;
    phaseSpec=0;
  }
  if (crossSpec) {
    delete[] crossSpec;
    crossSpec=0;
  }
  if (input1Spec) {
    delete[] input1Spec;
    input1Spec=0;
  }
  if (input2Spec) {
    delete[] input2Spec;
    input2Spec=0;
  }
  if (rawAudio) {
    delete[] rawAudio;
    rawAudio=0;
  }

  //Copy the frequency spectra
  if (rhs.phaseSpec) {
    phaseSpec = new float[numBins];
    for (int i=0; i<numBins; i++)
      phaseSpec[i] = rhs.phaseSpec[i];
  }
  if (rhs.crossSpec) {
    crossSpec = new float[numBins];
    for (int i=0; i<numBins; i++)
      crossSpec[i] = rhs.crossSpec[i];
  }
  if (rhs.input1Spec) {
    input1Spec = new float[numBins];
    for (int i=0; i<numBins; i++)
      input1Spec[i] = rhs.input1Spec[i];
  }
  if (rhs.input2Spec) {
    input2Spec = new float[numBins];
    for (int i=0; i<numBins; i++)
      input2Spec[i] = rhs.input2Spec[i];
  }
  if (rhs.rawAudio) {
    int len = 2*audioLen;
    rawAudio = new audio_t[len];
    for (int i=0; i<len; i++)
      rawAudio[i] = rhs.rawAudio[i];
  }
}


bool IntegPeriod::loadraw(IntegPeriod *&data, int &count,
			  long long start, long long end,
                          int &samprate, const char *server, int port)
{
  count = 0;
  data = NULL;
  bool res = false;

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
	  //load the data
	  res = loadraw(data,count,start,end,samprate,dsrc);
	  //Ensure network connection is closed
	  dsrc.flush();
	  if (dsrc.good()) {
	      dsrc.close();
	      cerr << "CLOSED CONNECTION\n";
	  }
      } else {
	  //dsrc.close();
	  dsrc.rdbuf()->close();
	  cerr << "CLOSED AFTER ERROR 1\n";
	  res = false;
      }
  } else {
      dsrc.close();
      //dsrc.rdbuf()->close();
      cerr << "CLOSED AFTER ERROR 2\n";
      res = false;
  }

  return res;
}


bool IntegPeriod::loadraw(IntegPeriod *&data, int &count,
			  long long start, long long end,
                          int &samprate, TCPstream &sock)
{
  data = NULL;
  count = 0;

  if (sock.good()) {
      //Request the data from the network server
      sock << "RAW-BETWEEN " << start << " " << end << endl;

      //Read their response line, how many periods will it be sending
      char line[1001];
      line[1000] = '\0';
      sock.getline(line, 1000);
      istringstream countstr(line);
      countstr >> count;

      if (count<0 || count>10000000) {
	  cerr << "Silly count returned by server (" << count << ")\n";
	  return false;
      }
      if (count==0) {
	  cerr << "Server returned no data for the specified period\n";
	  return true;
      }

      cerr << "Server will return " << count << " integration periods\n";
      data = new IntegPeriod[count];

      //Read the sampling rate
      sock >> samprate;

      //Some var's to give download %age statistics
      float printstep = 0.05;
      float nextprint = 0.0;

      //Read each integration period from the network server
      for (int i=0; i<count; i++) {
	  sock >> data[i];
	  if (!sock.good()) {
	      cerr << "ERROR reading from network\n";
	      delete[] data;
	      return false;
	  }
	  if (i/float(count) >= nextprint) {
	      if (nextprint!=0.0) {
		cout << "\b\b\b\b\b\b\b\b\b\b\b\b\b\b";
	      }
	      cout << (int)(100*i/float(count)) << "%" << flush;
              nextprint += printstep;
	  }
      }
      cout << "\b\b\b\b\b\b\b\b\b\b\b\b";
      cout << "100%  OK\n";
      cerr << "OK\n";
      return true;
  } else return false;
}


bool IntegPeriod::load(IntegPeriod *&data, int &count,
		       long long start, long long end,
		       const char *server, int port,
		       bool preprocess)
{
  count = 0;
  data = NULL;
  bool res = false;

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
	  //load the data
	  res = load(data,count,start,end,dsrc,preprocess);
	  //Ensure network connection is closed
	  dsrc.flush();
	  if (dsrc.good()) {
	      dsrc.close();
	      cerr << "CLOSED CONNECTION\n";
	  }
      } else {
	  //dsrc.close();
	  dsrc.rdbuf()->close();
	  cerr << "CLOSED AFTER ERROR 1\n";
	  res = false;
      }
  } else {
      dsrc.close();
      //dsrc.rdbuf()->close();
      cerr << "CLOSED AFTER ERROR 2\n";
      res = false;
  }

  return res;
}

bool IntegPeriod::load(IntegPeriod *&data, int &count,
		       long long start, long long end,
		       TCPstream &sock, bool preprocess)
{
  data = NULL;
  count = 0;

  if (sock.good()) {
      //Request the data from the network server
      sock << "BETWEEN " << start << " "<< end << " 0 0 0 ";
      if (preprocess) sock << "1\n";
      else sock << "0\n";

      //Read their response line, how many periods will it be sending
      char line[1001];
      line[1000] = '\0';
      sock.getline(line, 1000);
      istringstream countstr(line);
      countstr >> count;

      if (count<0 || count>10000000) {
	  cerr << "Silly count returned by server (" << count << ")\n";
	  return false;
      }
      if (count==0) {
	  cerr << "Server returned no data for the specified period\n";
	  return true;
      }

      cout << "Server will return " << count << " integration periods\n";
      data = new IntegPeriod[count];

      float printstep = 0.05;
      float nextprint = 0.0;

      //Read each integration period from the network server
      for (int i=0; i<count; i++) {
	  if (i/float(count) >= nextprint) {
	      if (nextprint!=0.0) {
		cout << "\b\b\b\b\b\b\b\b\b\b\b\b\b\b";
	      }
	      cout << (int)(100*i/float(count)) << "%" << flush;
              nextprint += printstep;
	  }
	  sock >> data[i];
	  if (!sock.good()) {
	      cerr << "ERROR reading from network\n";
	      delete[] data;
	      return false;
	  }
      }
      cout << "\b\b\b\b\b\b\b\b\b\b\b\b";
      cout << "100%  OK\n";
      return true;
  } else return false;
}


bool IntegPeriod::load(IntegPeriod *&data, int &count, const char *infile, bool keepaudio)
{
  data = NULL;
  count = 0;

  ifstream tfile(infile, ios::in|ios::binary);
  if (!tfile.good()) {
    cerr << "ERROR opening " << infile << endl;
    return false;
  }
  bool res = load(data,count,tfile,keepaudio);
  tfile.close();
  return res;
}

bool IntegPeriod::load(IntegPeriod *&data, int &count, ifstream &infile, bool keepaudio)
{
  data = NULL;
  count = 0;

  if (!infile.good()) {
    cerr << "ERROR1 bad file stream given as argument\n";
    return false;
  }

  int arrlen = 1;
  data = new IntegPeriod[arrlen];

  bool firstaudio = true;

  while (infile.good() && !infile.eof() ){//  && count<3600) { //HACK
    infile >> data[count];
    if (!infile.good() || infile.eof()) {
      if (count==0) {
	cerr << "ERROR2 bad file stream given as argument\n";
	return false;
      } else break;
    }
    count++;

    //If there is audio, then reprocess the period and delete the
    //audio. Use the loadraw command if you want to play audio...
    if (data[count-1].rawAudio!=NULL) {
      if (firstaudio) {
        firstaudio = false;
        cerr << "Found raw audio data: REPROCESSING DATA\n";
      }
      data[count-1].doCorrelations();
      if (!keepaudio) {
        //Free up the memory
        data[count-1].keepOnly(1,1,0);
      }
    }

    if (count==arrlen) {
      //time to double the array size
      arrlen*=2;
      IntegPeriod *tdata = new IntegPeriod[arrlen];
      //and copy the old data over
      for (int i=0; i<count; i++) {
	tdata[i] = data[i];
      }
      delete[] data;
      data = tdata;
    }
  }
  return true;
}

void IntegPeriod::merge(IntegPeriod *&resdata, int &rescount,
			IntegPeriod *data1, int count1,
			IntegPeriod *data2, int count2)
{
  rescount = count1 + count2;
  resdata = new IntegPeriod[rescount];

  int c1=0, c2=0;
  for (int i=0; i<rescount; i++) {
    if (c1<count1 && c2<count2 && data1[c1].timeStamp<=data2[c2].timeStamp) {
      resdata[i] = data1[c1];
      c1++;
    } else if (c1<count1 && c2<count2 && data1[c1].timeStamp>data2[c2].timeStamp) {
      resdata[i] = data2[c2];
      c2++;
    } else if (c1<count1) {
      resdata[i] = data1[c1];
      c1++;
    } else {
      resdata[i] = data2[c2];
      c2++;
    }
  }
}

void IntegPeriod::sort(IntegPeriod *&data, int count)
{
  IntegPeriod *resdata = new IntegPeriod[count];

  bool flags[count];
  for (int i=0; i<count; i++) flags[i] = false;

  for (int i=0; i<count; i++) {
    //Find the minimum time remaining
    timegen_t besttime = -1;
    int bestind = 0;
    for (int i1=0; i1<count; i1++) {
      if (!flags[i1] && (data[i1].timeStamp<besttime || besttime==-1)) {
	bestind = i1;
        besttime = data[i1].timeStamp;
      }
    }

    flags[bestind] = true;
    resdata[i] = data[bestind];
  }
  delete[] data;
  data = resdata;
}

/*void IntegPeriod::sort(IntegPeriod *&data, int count)
{
  IntegPeriod *resdata = new IntegPeriod[count];

  bool flags[count];
  for (int i=0; i<count; i++) flags[i] = false;

  bool done = false;
  int resindex = 0;

  while (!done) {
    //Find the minimum time remaining
    unsigned long long besttime = 0xFFFFFFFFFFFFFFFFll;
    int bestind = 0;
    for (int i1=0; i1<count; i1++) {
      if (!flags[i1] && (unsigned long long)(data[i1].timeStamp)<besttime) {
	bestind = i1;
        besttime = data[i1].timeStamp;
      }
    }
    if (besttime==0xFFFFFFFFFFFFFFFFll) break;

    //Then count how many after the minimum monotonically increase
    unsigned long long lasttime = besttime;
    int j;
    for (j=bestind+1; j<count; j++) {
      //If time has stopped increasing, break
      if ((unsigned long long)(data[j].timeStamp)<lasttime) break;
      lasttime = data[j].timeStamp;
    }

    for (int i=bestind; i<j; i++) {
      //Mark that we have used each point
      flags[i] = true;
      resdata[resindex] = data[i];
      resindex++;
    }
  }
  delete[] data;
  data = resdata;
}*/

/////////////////////////////////////////////////////////////////
//Erk.
void IntegPeriod::integrate(IntegPeriod *&res, int &reslen,
			    IntegPeriod *data, int size,
			    long long period, bool keeprfi)
{
  res = NULL;
  reslen = 0;
  long long epoch = data[0].timeStamp;
  long long endepoch = data[size-1].timeStamp;

  //This is space inefficient if data is sparse wrt specified period
  int num = (endepoch-epoch)/period + 2;
  if (num<=0) return;

  res = new IntegPeriod[num];

  long long start, stop;
  int counter = 0, i=0;
  bool done = false;

  //Represents the start and stop epochs of "this" integration period
  start = epoch - (epoch%period);
  stop = start+period;

  while (i<size && start<endepoch && !done) {
    int numaccum = 0;
    res[counter].timeStamp = start;
    //Add in all the period that should be averaged together
    for (;i<size&&!done&&epoch<=stop; i++) {
      if (!data[i].RFI || keeprfi) {
        res[counter] += data[i];
	numaccum++;
      }

      if (i+1<size) {
	epoch = data[i+1].timeStamp;
      } else {
	done=true;
      }
    }
    if (numaccum>0) {
      res[counter]/=numaccum;
      //Only advance counter if there were some data
      counter++;
    }
    //Advance to the next integration period
    start += period;
    stop  += period;
    if (stop>endepoch) stop=endepoch;
  }
  reslen = counter;
  if (reslen==0) {
    delete[] res;
    res = NULL;
  }
}


////////////////////////////////////////////////////////////////////////
//Write the given data to a text file
void IntegPeriod::writeASCII(const IntegPeriod *data, int datlen,
			     ofstream &datfile)
{
  for (int i=0; i<datlen; i++) {
    //Time stamp
    if (data[i].timeStamp<864000000000ll)
      datfile << data[i].timeStamp/3600000000.0 << " ";
    else
      datfile << data[i].timeStamp << " ";

    //Both input power levels
    datfile << data[i].power1 << " " << data[i].power2 << " ";
    //Cross power
    datfile << data[i].powerX << endl; //One line per period
  }
}


////////////////////////////////////////////////////////////////////////
//Write the given data to a text file
void IntegPeriod::writeASCII(const IntegPeriod *data, int datlen,
			     const char *fname)
{
  ofstream datfile(fname, ios::out);
  writeASCII(data, datlen, datfile);
  datfile.close();
}


////////////////////////////////////////////////////////////////////////
//Write the given data as a binary file
void IntegPeriod::write(const IntegPeriod *data, int datlen,
			const char *fname)
{
  ofstream datfile(fname, ios::out|ios::binary);
  write(data, datlen, datfile);
  datfile.close();
}


////////////////////////////////////////////////////////////////////////
//Write the given data as a binary file
void IntegPeriod::write(const IntegPeriod *data, int datlen,
			ofstream &datfile)
{
  for (int i=0; i<datlen; i++) datfile << data[i];
}


////////////////////////////////////////////////////////////////////////
//Write the given data to a WAVE audio file
void IntegPeriod::writeWAVE(const IntegPeriod *data, int datlen,
			    int samprate, string fname)
{
  unsigned long fsize, audiosize=0;
  unsigned long tlong;
  unsigned short tshort;

  //We need to count how many audio samples we have all up
  for (int i=0; i<datlen; i++) {
    if (data[i].audioLen!=0) audiosize+=data[i].audioLen;
  }
  audiosize = sizeof(audio_t)*2*audiosize;
  fsize     = audiosize + 16 + 4 + 8;

  //Open the output file for writing
  ofstream datfile(fname.c_str(), ios::out | ios::binary);

  //Write the RIFF header
  datfile << "RIFF";
  datfile.write((char*)&fsize, sizeof(unsigned long));
  datfile << "WAVE";

  //Output the FORMAT chunk
  datfile << "fmt ";
  tlong = 16; //Header size
  datfile.write((char*)&tlong, sizeof(unsigned long));
  tshort = 1; //Uncompressed
  datfile.write((char*)&tshort, sizeof(unsigned short));
  tshort = 2; //Stereo - two channels
  datfile.write((char*)&tshort, sizeof(unsigned short));
  datfile.write((char*)&samprate, sizeof(int)); //Sample rate
  tlong = sizeof(audio_t)*2*samprate; //Bytes per second
  datfile.write((char*)&tlong, sizeof(unsigned long));
  tshort = sizeof(audio_t)*2; //Bytes per block
  datfile.write((char*)&tshort, sizeof(unsigned short));
  tshort = sizeof(audio_t)*8; //Bits per sample
  datfile.write((char*)&tshort, sizeof(unsigned short));
//  tshort = 0; //Extra header bytes
//  datfile.write((char*)&tshort, sizeof(unsigned short));

  //Output the DATA chunk
  datfile << "data";
  datfile.write((char*)&audiosize, sizeof(unsigned long));
  for (int i=0; i<datlen; i++) {
    if (data[i].audioLen!=0) {
      datfile.write((char*)data[i].rawAudio, sizeof(audio_t)*2*data[i].audioLen);
    }
  }

  datfile.close();
}


////////////////////////////////////////////////////////////////////////
//Scale inputs so that they approximate channel 1 of reference
void IntegPeriod::rescale(IntegPeriod *data, int datlen,
			  IntegPeriod *ref, int reflen)
{
  timegen_t xt[datlen];
  timegen_t yt[reflen];
  double m1=0.0, m2=0.0;
  int counter = 0;

  for (int i=0; i<reflen; i++) yt[i] = ref[i].timeStamp;
  for (int i=0; i<datlen; i++) xt[i] = data[i].timeStamp;

  for (int i=0; i<datlen; i++) {
    int j = matchTimes(i, xt, datlen, yt, reflen);
    if (j!=-1) {
      m1 += data[i].power1/ref[j].power1;
      m2 += data[i].power2/ref[j].power1;
      counter++;
    }
  }

  if (counter>0) {
    m1 /= (float)counter;
    m2 /= (float)counter;
    cout << "P1 = P1 * " << m1 << endl;
    cout << "P2 = P2 * " << m2 << endl;
    cout << "PX = PX * " << (m1+m2)/2.0 << endl;
    for (int i=0; i<datlen; i++) {
      data[i].power1 = data[i].power1/m1;
      data[i].power2 = data[i].power2/m2;
      data[i].powerX = data[i].powerX/((m1+m2)/2.0);
      data[i].amplitude = data[i].amplitude/((m1+m2)/2.0);
    }
  }
}


////////////////////////////////////////////////////////////////////////
//Scale and offset inputs so that they approximate channel 1 of reference
void IntegPeriod::translate(IntegPeriod *data, int datlen,
			    IntegPeriod *ref, int reflen)
{
  timegen_t xt[datlen];
  timegen_t yt[reflen];
  float x1[datlen];
  float x2[datlen];
  float y[reflen];

  for (int i=0; i<reflen; i++) {
    yt[i] = ref[i].timeStamp;
    y[i] = ref[i].power1;
  }

  for (int i=0; i<datlen; i++) {
    xt[i] = data[i].timeStamp;
    x1[i] = data[i].power1;
    x2[i] = data[i].power2;
  }

  float m1=0.0, m2=0.0, b1=0.0, b2=0.0;

  regression(m1, b1, x1, xt, datlen, y, yt, reflen);
  regression(m2, b2, x2, xt, datlen, y, yt, reflen);

  for (int i=0; i<datlen; i++) {
    data[i].power1 = data[i].power1*m1 + b1;
    data[i].power2 = data[i].power2*m2 + b2;
    data[i].powerX = data[i].powerX*((m1+m2)/2.0);
    data[i].amplitude = data[i].amplitude/((m1+m2)/2.0);
  }
}


////////////////////////////////////////////////////////////////////////
int IntegPeriod::purgeFlagged(IntegPeriod *&res, int &reslen,
			       IntegPeriod *data, int datalen)
{
  //First, loop through and count how many non-flagged periods
  reslen=0;
  res = NULL;
  for (int i=0; i<datalen; i++) {
    if (!data[i].RFI) reslen++;
  }
  if (reslen==0) return 0;
  //Then go through and copy those ones over
  res = new IntegPeriod[reslen];
  int j=0;
  for (int i=0; i<datalen; i++) {
    if (!data[i].RFI) {
      res[j] = data[i];
      j++;
    }
  }
  return datalen-reslen;
}


////////////////////////////////////////////////////////////////////////
void IntegPeriod::normalise(IntegPeriod *&data, int count)
{
  float max1=data[0].power1;
  float max2=data[0].power2;
  for (int i=0; i<count; i++) {
    if (data[i].power1>max1) max1=data[i].power1;
    if (data[i].power2>max2) max2=data[i].power2;
  }

  for (int i=0; i<count; i++) {
    data[i].power1/=max1;
    data[i].power2/=max2;
    //data[i].powerX/=
    //data[i].amplitude/=
  }
}

