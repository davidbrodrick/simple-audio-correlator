//
// Copyright (C) David Brodrick, 2002-2004
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.
//
// $Id: Processor.cc,v 1.5 2004/04/07 08:37:29 brodo Exp brodo $

//Thread to calculate the correlation functions of recently captured audio
//samples. This reads the raw data from a buffer from the audio capture
//thread and writes processed data to the data storage component. We can 
//strip certain fields from the data before submitting it for storage.

#include <Processor.h>
#include <IntegPeriod.h>
#include <StoreMaster.h>
#include <iostream>
#include <fstream>
#include <iomanip>

///////////////////////////////////////////////////////////////////////
//Constructor
Processor::Processor(Buf<IntegPeriod*> *source,
                     StoreMaster *sinc,
		     StoreMaster *rawsinc)
:itsInBuf(source),
itsOutBuf(sinc),
itsRawOutBuf(rawsinc),
itsLastEpoch(-1),
itsKeepAudio(false)
{
  ThreadedObject();
}


///////////////////////////////////////////////////////////////////////
//Destructor
Processor::~Processor()
{
}


///////////////////////////////////////////////////////////////////////
//Main loop of execution for the processing thread
void Processor::run()
{
  while (itsKeepRunning) {
    //Get the next audio period from our input buffer
    IntegPeriod &intper = getNextInput();
    //Calculate the frequency spectra of the input audio
    intper.doCorrelations();

    if (itsRawOutBuf!=NULL) {
      IntegPeriod *intpernostrip = new IntegPeriod();
      *intpernostrip = intper;
      itsRawOutBuf->put(intpernostrip);
    }
    //Discard all data which we do not wish to write to disk
    strip(&intper);
    //Give the data to the data storage component
    itsOutBuf->put(&intper);
  }
  //Close our thread, we have finished
  itsKeepRunning = false;
}


///////////////////////////////////////////////////////////////////////
//Get the next data from the buffer
IntegPeriod& Processor::getNextInput()
{
  IntegPeriod *res;
  while (1) {
    //Look for the next epoch we can get
    int epoch = itsLastEpoch+1;
    itsLastEpoch++;
    //Wait until our requested data is available
    itsInBuf->wait4Epoch(epoch);
    //Get the data from our input buffer
    res = itsInBuf->get(epoch);
    //If we were successful break from the loop
    if (res!=NULL&&epoch!=-1) break;
    //If we get to this badness has occurred and memory has leaked.
    //The computer cannot keep up with the specified sampling rate
    //or spectral resolution.
    cerr << "Processor: MISSED AUDIO - Memory has been leaked\n";
  }
  return *res;
}


///////////////////////////////////////////////////////////////////////
//Strip selected fields from the data
void Processor::strip(IntegPeriod *arg)
{
  if (arg->rawAudio && !itsKeepAudio) {
    delete[] arg->rawAudio;
    arg->rawAudio = NULL;
  }
  //if (!itsKeepSpectra) {
    if (arg->input1Spec) {
      delete[] arg->input1Spec;
      arg->input1Spec = NULL;
    }
    if (arg->input2Spec) {
      delete[] arg->input2Spec;
      arg->input2Spec = NULL;
    }
    if (arg->phaseSpec) {
      delete[] arg->phaseSpec;
      arg->phaseSpec = NULL;
    }
    if (arg->crossSpec) {
      delete[] arg->crossSpec;
      arg->crossSpec = NULL;
    }
  //}
}
