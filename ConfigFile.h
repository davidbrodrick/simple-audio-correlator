//
// Copyright (C) David Brodrick
// Copyright (C) CSIRO Australia Telescope National Facility
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.
//

#ifndef _CONFIGFILE_HDR_
#define _CONFIGFILE_HDR_

#include <string>
#include <sstream>
#include <fstream>

using namespace::std;

class TeleSystem;

//Class to parse a sac configuration file and make the information
//contained in it available to the other bits of the software.
class ConfigFile
{
private:
  //Depicts whether to start the realtime audio processing component
  bool itsDoRealTime;
  //Audio integration time (ms) for realtime processor
  int itsIntegTime;
  //The audio device file to use for capture of raw data
  string itsAudioDev;
  //Sampling rate to use for raw data capture
  int itsSampRate;
  //Directory to use for the data store
  string itsStoreDir;
  //Should the storage subsystem record the raw audio
  bool itsKeepAudio;
  //Should the storage system keep spectral information
  bool itsKeepSpectra;
  //Directory to use for the short-term raw data store
  string itsRawStoreDir;
  //Maximum age (microseconds) of raw data to be kept
  long long itsMaxRawAge;
  //Should the raw data store be used (true) or disabled (false)
  bool itsStoreRaw;
  //Network port for the server to listen on
  int itsServerPort;
  //Maximum number of network clients to run
  int itsMaxClients;
  //Number of spectral channels to calculate
  int itsNumBins;
  //Latitude of the telescope in degrees, North +ve.
  float itsLatitude;
  //Longitude of the telescope in degrees, East +ve.
  float itsLongitude;
  //Gain of the first sound card channel
  float itsGain1;
  //Gain of the second sound card channel
  float itsGain2;
  //Handle to the file we are parsing.
  ifstream itsFile;
  //Record of how many raw lines we have read from the file
  int itsLineNum;  

  //Return the next meaningful line from the file. This strips
  //comments and blank lines and substitues macro values.
  istringstream *nextLine();

  //Parses each line from the file and uses the info to specify
  //values for the appropriate fields.
  void parseFile();

public:
  //Load and parse the specified file
  ConfigFile(const char *fname);

  //Return true if the file says to run the realtime data processor
  inline bool getDoRealTime() {return itsDoRealTime;}

  //Return the requested integration time in milliseconds
  inline int getIntegTime() {return itsIntegTime;}

  //Return the requested integration time in milliseconds
  inline int getNumBins() {return itsNumBins;}

  //Return the audio device to record realtime data from.
  //Should really set this up to support multiple sound cards...
  inline string getAudioDev() {return itsAudioDev;}

  //Return the sampling rate specified by the file
  inline int getSampRate() {return itsSampRate;}

  //Return the base directory for the main data store
  inline string getStoreDir() {return itsStoreDir;}

  //Return true if the main storage system should store spectral information
  inline bool getKeepSpectra() {return itsKeepSpectra;}

  //Return the base directory for the optional raw data store
  inline string getRawStoreDir() {return itsRawStoreDir;}

  //Return if the optional raw data store should be used (true if so)
  inline bool getStoreRaw() {return itsStoreRaw;}

  //Return the maximum age of raw data to be kept - older data will be removed
  inline long long getMaxRawAge() {return itsMaxRawAge;}

  //Return the port to run the network server on
  inline int getServerPort() {return itsServerPort;}

  //Return the maximum number of network clients to start
  inline int getMaxClients() {return itsMaxClients;}

  //Return the server latitude, in degrees, North +ve
  inline float getLatitude() {return itsLatitude;}

  //Return the server longitude, in degrees, East +ve
  inline float getLongitude() {return itsLongitude;}
  
  //Return the required gain factor for sound card channel 1
  inline float getGain1() {return itsGain1;}
  
  //Return the required gain factor for sound card channel 2
  inline float getGain2() {return itsGain2;} 
};

#endif
