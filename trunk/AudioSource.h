//
// Copyright (C) David Brodrick, 2002
// Copyright (C) CSIRO Australia Telescope National Facility
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.
//
// $Id: AudioSource.h,v 1.5 2004/04/07 12:39:01 brodo Exp $

//Thread to read raw data from the sound card and write it to an output 
//buffer. This multi-threaded scheme helps to ensure we capture as much
//audio as possible - while the processor thread is busy calculating
//correlation functions and Fourier transforms this thread is already busy 
//recording the next batch of raw audio data.

#ifndef _AUDIOSOURCE_HDR_
#define _AUDIOSOURCE_HDR_

//Define DEBUG_AUDIO to print extra debugging information
//#define DEBUG_AUDIO

#include <ThreadedObject.h>
#include <Buf.h>
#include <pthread.h>

//Forward declarations
class IntegPeriod;

class AudioSource : public ThreadedObject {
public:
  AudioSource(const char *device, Buf<IntegPeriod*> *buf);
  AudioSource(const char *device);
  ~AudioSource();

  //Set the buffer to write the output to
  inline void setOutputBuf(Buf<IntegPeriod*> *buf) {
    itsOutputBuf=buf;
  }

  //Set the sampling rate, returns the actual rate obtained or -1 if
  //the requested sampling rate generated an error.
  int setSampRate(int hz);
  //Returns the actual rate sampling rate.
  inline int getSampRate() {return itsSampRate;}
  //Set the number of channels, 1 for mono, 2 for stereo.
  //Returns true if the request was successful.
  bool setChannels(int num);
  //Set integration period as number of milliseconds of audio.
  void setIntegPeriod(int ms);

  //Check if the AudioSource is able to run
  bool isValid();

  //Get the audio block length
  inline int getBlockLen() { return itsLength; }

private:
  //Main loop of execution for the dedicated thread
  void run();

  //Return the current time, as microseconds since the epoch
  long long getTime();

  //The buffer to which we write our output data
  Buf<IntegPeriod*> *itsOutputBuf;
  //File descriptor of the sound card device
  int itsFD;
  //Name of our audio device
  const char *itsDevice;
  //Have we encountered a fatal error, eg, no sound hardware installed
  bool itsValid;
  //Current sampling rate
  int itsSampRate;
  //Number of milliseconds of audio in each output block
  int itsIntegPeriod;
  //Number of samples in each output block
  int itsLength;
  //Number of output channels, 1 for mono, 2 for stereo
  int itsNumChannels;
  //Are we just simulating
  bool itsSimulate;
};

#endif
