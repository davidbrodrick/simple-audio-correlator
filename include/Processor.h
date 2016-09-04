//
// Copyright (C) David Brodrick, 2002-2004
// Copyright (C) CSIRO Australia Telescope National Facility
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.
//

//Thread to calculate the correlation functions of recently captured audio
//samples. This reads the raw data from a buffer from the audio capture
//thread and writes processed data to the data storage component. We can 
//strip certain fields from the data before submitting it for storage.

#ifndef _PROCESSOR_HDR_
#define _PROCESSOR_HDR_

#include <ThreadedObject.h>
#include <Buf.h>
#include <pthread.h>
#include <sstream>

//Forward declarations
class IntegPeriod;
class StoreMaster;

class Processor : public ThreadedObject {
public:
  //Create a new processor. Raw data will be read from 'in' and processed
  //data will be written to the specified store, 'out'. Processed
  //IntegPeriods with attached raw audio data will be stored in the
  //rolling 'rawout' storage buffer if 'rawout' is non-null.
  Processor(Buf<IntegPeriod*> *in, StoreMaster *out,
            StoreMaster *rawout=NULL, float gain1=1.0, float gain2=1.0);
  //Destructor
  ~Processor();

  //Do we keep audio (true) or strip it before saving (false)
  inline void setKeepAudio(bool keep) {itsKeepAudio = keep;}

private:
  //Main loop of execution for the dedicated thread
  void run();
  //Get the next available IntegPeriod from our input source
  IntegPeriod& getNextInput();
  //Strip the IntegPeriod down to only what we want to save
  void strip(IntegPeriod*);

  //Buffer from which we read IntegPeriods with just audio data
  Buf<IntegPeriod*> *itsInBuf;
  //Buffer to which we write processed IntegPeriods
  StoreMaster *itsOutBuf;
  //Buffer to which we write processed IntegPeriods with raw data
  //Can be NULL in which case we don't store a raw audio buffer.
  StoreMaster *itsRawOutBuf;

  //Number of frequency domain spectral channels in our output
  int itsNumBins;
  //Epoch of the last data we obtained from the input buffer
  int itsLastEpoch;
  
  //Gain for channel 1
  float itsGain1;
  //Channel 2
  float itsGain2;

  //Do we keep audio (true) or strip it before saving (false)
  bool itsKeepAudio;
};

#endif
