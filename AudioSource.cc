//
// Copyright (C) David Brodrick, 2002
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.
//
// $Id: AudioSource.cc,v 1.3 2003/08/27 13:02:12 bro764 Exp brodo $

//Thread to read raw data from the sound card and write it to an output 
//buffer. This multi-threaded scheme helps to ensure we capture as much
//audio as possible - while the processor thread is busy calculating
//correlation functions and Fourier transforms this thread is already busy 
//recording the next batch of raw audio data.

#include <AudioSource.h>
#include <IntegPeriod.h>
#include <iostream>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/soundcard.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <sys/time.h>
#include <time.h>


///////////////////////////////////////////////////////////////////////
//Constructor
AudioSource::AudioSource(const char *device, Buf<IntegPeriod*> *buf)
:itsOutputBuf(buf),
itsDevice(device),
itsValid(true),
itsSampRate(8000),
itsIntegPeriod(1000)
{
  ThreadedObject();

  if (device==0) {
    itsSimulate = true;
  } else {
    //Try to open the audio device
    if ((itsFD = open(device, O_RDONLY, 0))==-1) {
      //Error, could not open the audio device
      perror(itsDevice);
      itsValid = false;
    }
    //Reset the sound card
    ioctl(itsFD, SNDCTL_DSP_RESET, 0);
    //Set audio format, 16 bits
    int audformat = AFMT_S16_LE;
    int temp = audformat;
    if (ioctl(itsFD, SNDCTL_DSP_SETFMT, &temp)==-1) {
      //Error, could not obtain desired format
      perror(itsDevice);
      itsValid = false;
    }
    if (temp!=audformat) {
      cerr << "AUDIO: ERROR: Audio format is not supported\n";
      itsValid = false;
    }
  }
  //Configure the default sampling rate
  setSampRate(itsSampRate);
  //Set the default integration length (length of audio output)
  setIntegPeriod(itsIntegPeriod);
#ifdef DEBUG_AUDIO
  cerr << "Audio Device Initialised\n";
#endif
  }


///////////////////////////////////////////////////////////////////////
//Destructor
AudioSource::~AudioSource()
{
  //Close the audio device
  close(itsFD);
}


///////////////////////////////////////////////////////////////////////
//Main loop of execution for audio grabbing thread
void AudioSource::run()
{
  bool error = false;

  //On some sound cards the first few reads return rubbish so we
  //do a few dummy reads here before we start collecting data.
  if (!itsSimulate) {
    audio_t splutter[512];
    for (int i=0; i<10 && !error; i++) {
      if (read(itsFD, (void*)splutter, 1024)==-1) {
	perror(itsDevice);
	error = true;
      }
    }
  }

  //Main data collection loop
  while (itsKeepRunning && !error) {
    //Allocate memory to hold the audio for the next period
    audio_t *tempdata = new audio_t[itsLength];
    //Create the next IntegPeriod and configure it
    IntegPeriod *intper = new IntegPeriod();
    intper->audioLen  = itsLength/2; //Because there are 2 channels
    intper->rawAudio  = tempdata;
    intper->timeStamp = getTime();   //Timestamp at start of period

    //Read the actual audio data from the audio device
    if (itsSimulate) {sleep(1);}
    else {
      if (read(itsFD, (void*)tempdata, 2*itsLength)==-1) {
	perror(itsDevice);
	error = true;
      }
    }
    //Insert the new data in our output buffer
    itsOutputBuf->put(intper);
    cout << "." << flush;
  }
  //Kill our thread, we have finished
  itsKeepRunning = false;
  pthread_exit(NULL);
}


///////////////////////////////////////////////////////////////////////
//Configure the sampling rate, returns actual rate obtained
int AudioSource::setSampRate(int hz)
{
  int res = hz;
  if (!itsSimulate) {
    //Configure the sampling rate of the audio device
    if (ioctl(itsFD, SNDCTL_DSP_SPEED, &hz)==-1) {
      perror(itsDevice);
      itsValid = false;
      res = -1;
    } else {
      itsSampRate = hz;
      res = hz;
    }
  }
#ifdef DEBUG_AUDIO
  cerr << "AUDIO: Got " << hz << " Hz sampling rate\n";
#endif
  return res;
}


///////////////////////////////////////////////////////////////////////
//Set the number of channels, 1 or 2
bool AudioSource::setChannels(int num)
{
  bool res = true;
  num -= 1;
  if (!itsSimulate) {
    if (ioctl(itsFD, SNDCTL_DSP_STEREO, &num)==-1) {
      perror(itsDevice);
      res = itsValid = false;
    }
  }
  return res;
}


///////////////////////////////////////////////////////////////////////
//Set the duration, in milliseconds, of each block of audio output
void AudioSource::setIntegPeriod(int ms)
{
  assert(ms>1);
  //If the sampling rate is accurate, each sample takes 1000/samprate ms
  float eachsamp = 1000/(float)itsSampRate;
  //Hence we want this many samples for each output block
  itsLength = 2*static_cast<int>(ms/eachsamp);
  //Ensure even length for stereo capture
  if (itsLength%1) itsLength++;
#ifdef DEBUG_AUDIO
  cerr << "AUDIO: Set output length to " << itsLength << " samples\n";
#endif
}


///////////////////////////////////////////////////////////////////////
//Return true if hardware is configured and the thread is ready to run
bool AudioSource::isValid()
{
  return itsValid;
}


///////////////////////////////////////////////////////////////////////
//Return the time as microseonds since the epoch
long long AudioSource::getTime()
{
  struct timeval tv = {0,0};
  long long res;
  //Get current time from the system
  gettimeofday(&tv, 0);
  //Convert to 64 bit
  res = 1000000*(long long)(tv.tv_sec) + tv.tv_usec;
  return res;
}
