//
// Copyright (C) David Brodrick, 2002-2004
// Copyright (C) CSIRO Australia Telescope National Facility
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.
//
// $Id: StoreMaster.h,v 1.6 2004/02/06 09:37:49 brodo Exp $

#ifndef _STOREMASTER_HDR_
#define _STOREMASTER_HDR_

//Uncomment this to enable some further debugging
//#define DEBUG_STORE

#include <pthread.h>
#include <TimeCoord.h>
#include <Buf.h>
#include <sstream>
#include <fstream>

class IntegPeriod;

class StoreMaster {
public:
  StoreMaster(char *path, long long maxage=0,
	      int savebufsize=5, int storebufsize=5);
  StoreMaster(string path, long long maxage=0,
	      int savebufsize=5, int storebufsize=5);

  ~StoreMaster();

  //Flush any unsaved data to disk
  void flush(bool havelock = false);

  //Take ownership of the given data and add it to the store
  void put(IntegPeriod *newper);

  //Get first data with time stamp after 'epoch'
  IntegPeriod *get(long long epoch);

  //Get the most recent data available
  IntegPeriod *getRecent();

  //Get all data between the given range of times
  //'count' is set to contain the number of periods returned
  //set end to zero to get all data since the start epoch
  IntegPeriod **get(long long start,
		    long long end,
		    int &count);

private:
  //Translate the given epoch into a filename. The filename is
  //added to the given ostringstream.
  void toFileName(long long epoch, ostringstream &output);
  //Translate the given filename into an epoch.
  //Returns -1 if fname is not understood.
  long long fromFileName(string fname);
  //Open a handle to which ever file immediately chronologically precedes
  //the file that 'epoch' would be stored in. Will return an epoch for
  //the returned file, or 0 if none found.
  long long prevFile(long long epoch, ifstream *&fhandle);
  //Open a handle to which ever file chronologically follows the
  //file that 'epoch' would be stored in. If the file immediately
  //after cannot be found we will search forward until we find one.
  //Will return an epoch for the returned file, or 0 if none found.
  long long nextFile(long long epoch, ifstream *&fhandle);
  //Recurse the given directory to find the first file containing
  //data after the target file 'epoch' would be written to. The
  //resulting filename is returned in 'output' if a file is found.
  bool fileAfter(string dir, long long epoch, ostringstream &output);
  //Recurse the main storage directory to find the first file containing
  //data after the target file 'epoch' would be written to. The
  //resulting filename is returned in 'output' if a file is found.
  bool searchForward(long long epoch, ostringstream &output);

  //Ensure all directories in the given path exist
  bool checkDirs(ostringstream &savepath);
  //Check if the file which would, by name, contain the data for
  //the specified epoch exists. Returns true if it exists.
  bool checkFile(long long epoch);
  //Check if the file which would, by name, contain the data for
  //the specified epoch exists. Returns true and opens if it exists.
  bool checkFile(long long epoch, ifstream *&infile);

  //Locate the right file for the specified 'epoch',  open it,
  //seek within a file to locate the first integration period with
  //timestamp after 'epoch'. Then returns the open file handle.
  //Will return false if no data exists for that period.
  bool findEpoch(long long epoch, ifstream *&infile);

  //Delete any data more than itsMaxDataAge old.
  //This won't delete data more than a week older than the expiry period.
  //This will probably save your hard earned data collection one day!
  ///TODO: At present the directories are not removed
  void removeOldData();

  //MutEx and locking functions
  pthread_mutex_t itsLock, itsDirLock;
  inline void Lock() {pthread_mutex_lock(&itsLock);}
  inline void Unlock() {pthread_mutex_unlock(&itsLock);}

  //Timestamps for the oldest and newest data to which we have access
  long long itsOldest;
  long long itsNewest;

  //Number of periods that we accumulate before flushing to disk
  int itsSaveBufSize;
  //Buffer where we hold data waiting to be written to disk
  Buf<IntegPeriod*> itsSaveBuf;
  //Number of periods currently waiting in the buffer
  int itsNumQueued;

  //Directory to which we save our data
  char *itsSaveDir;

  //Size of our memory cache for the most recent data
  int itsStoreBufSize;
  //Buffer where we cache recent data to save disk I/O
  Buf<IntegPeriod*> itsStoreBuf;

  //Holds the maximum number of periods to return for any given request
  static const int theirMaxResults;

  //Records the maximum age of data in our store - data older than this
  //will be automatically removed. An epoch of zero means that we should
  //never remove data, no matter how old it may be.
  timegen_t itsMaxDataAge;
};

#endif
