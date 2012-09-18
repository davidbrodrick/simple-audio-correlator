//
// Copyright (C) David Brodrick, 2002, 2003
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.
//
// $Id: sac.cc,v 1.8 2005/02/17 08:37:25 brodo Exp $

//"sac", the "Simple Audio Correlator" is a collection of classes
//to read data from a PC sound card, calculate power and power spectra
//measurements for each channel and the cross correlation of multiple
//channels, record the information to disk, and serve the data collection
//off disk to client programs via a simple TCP interface.
//
//All this proves to be very useful for making simple radio observatories:
//a single radio can be plugged into the computer; two radios can be
//plugged into the computer using the left and right channels of the
//sound card; or two radios with phase locked local oscillators can be
//used to form a digital cross correlating radio interferometer/
//spectrometer/radiometer.
//
//This file gets the ball rolling by setting the software up according
//to the specifications found in a configuration file.

#include <Buf.h>
#include <AudioSource.h>
#include <Processor.h>
#include <StoreMaster.h>
#include <WebMaster.h>
#include <ConfigFile.h>
#include <iostream>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

//Configure sound card and start the audio thread
void initAudio(ConfigFile &config, Buf<IntegPeriod*> *sink);
//Configure and start the data processing thread
void initProcessor(ConfigFile &config, Buf<IntegPeriod*> *source,
		   StoreMaster *sink, StoreMaster *rawsink);


/////////////////////////////////////////////////////////////////////////////
//Program starts execution here
int main(int argc, char *argv[])
{
  const char *fname = "sac.conf"; //Default configuration file

  if (argc>1) {
    if (strcmp(argv[1], "-h")==0) {
      //Print a useless message
      cerr << "sac: The \"simple audio correlator\"\n";
      cerr << "Usage: sac [config-file, eg, /etc/sac.conf]\n\n";
      exit(1);
    } else {
      //Use the specified configuration file
      fname = argv[1];
    }
  }

  //Set the timezone
  tzset();

  //Parse the configuration file
  ConfigFile theconfig(fname);

  //Create component to manage the saving/retrieval of selected data
  StoreMaster *store = new StoreMaster(theconfig.getStoreDir());

  //Declare StoreMaster which manages saving/retrieval of all raw data
  //This StoreMaster is optional and will only be created if the realtime
  //component is going to run.
  StoreMaster *rawstore = NULL;

  //If requested, start the realtime processing component
  if (theconfig.getDoRealTime()) {
    if (theconfig.getStoreRaw()) {
      //We need to create a rolling store for raw audio data
      rawstore = new StoreMaster(theconfig.getRawStoreDir(),
				 theconfig.getMaxRawAge());
    }

    //Create buffer between audio and data processing threads
    Buf<IntegPeriod*> *audiobuf = new Buf<IntegPeriod*>(30);
    //Start audio thread with specified parameters
    initAudio(theconfig, audiobuf);
    //initAudio(0, _samprate, _integperiod, audiobuf); //Null input source
    //Start the data processing (correlator) thread
    initProcessor(theconfig, audiobuf, store, rawstore);
  }

  //Start the data network server component
  WebMaster *ws = new WebMaster(store, rawstore, &theconfig);
  ws->start();

  while (1) sleep(5000); //Hmmm, probably something better to do

  return 0;
}


/////////////////////////////////////////////////////////////////////////////
//Start the audio thread
void initAudio(ConfigFile &config, Buf<IntegPeriod*> *sink)
{
  //Create the AudioSource
  AudioSource *aud = new AudioSource(config.getAudioDev().c_str(), sink);
  //Configure for stereo operation, even if only using 1 channel...
  aud->setChannels(2);
  //Configure sampling rate
  aud->setSampRate(config.getSampRate());
  //Configure integration period (length of each audio output block)
  aud->setIntegPeriod(config.getIntegTime());
  //Ensure everything worked
  if (!aud->isValid()) {
    cerr << "Could not configure audio device, exiting\n";
    exit(1);
  }
  //Print a reassuring message to the user
  cerr << config.getAudioDev() << " configured: " << aud->getSampRate()
    << " Hz, " << config.getIntegTime() << " ms integration\n";
  //Start the AudioSource
  aud->start();
}


/////////////////////////////////////////////////////////////////////////////
//Start the data processing thread
void initProcessor(ConfigFile &config,
		   Buf<IntegPeriod*> *source,
		   StoreMaster *sink,
		   StoreMaster *rawsink)
{
  //The Processor will do the correlations
  Processor *proc = new Processor(source, sink, rawsink);
  //Determines if raw audio will be saved to disk - space consuming!
  ///Now disabled in preference to the raw data sink
  proc->setKeepAudio(false);
  //Print another reassuring message
  cerr << "Processor configured: raw audio buffer "
      << ((rawsink==NULL)?"disabled\n":"enabled\n");
  //Start the processing thread... the system is away!
  proc->start();
}

