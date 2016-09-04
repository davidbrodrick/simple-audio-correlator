//
// Copyright (C) David Brodrick, 2002
// Copyright (C) CSIRO Australia Telescope National Facility
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.
//
// $Id: Buf.h,v 1.4 2004/04/01 09:35:06 brodo Exp brodo $

//Thread safe templated reader/writer circular buffer.

#ifndef _BUF__HDR_
#define _BUF__HDR_

#include <pthread.h>

using namespace std;

//Define DEBUG_BUF to print extra debugging info at runtime
#define DEBUG_BUF


//This is not the right spot for this declaration
typedef struct timed_audio {
  long long timestamp;
  signed short *audio;
} timed_audio;

template <class T> class Buf {
public:
  //Initialises mutex and some other stuff
  Buf(int capacity=1);
  virtual ~Buf();

  //Return the maximum size of our buffer
  int getSize();
  //Return how many entries are currently in the buffer
  int getEntries();

  //Remove all data from the queue
  virtual void makeEmpty();

  //Insert the specified data into the buffer	
  virtual void put(T data);

  //Return data with the requested epoch.
  //The caller is responsible for deleting the returned data.
  //This will return NULL if the requested epoch is not available.
  //if 'epoch' is -1 the latest data will be returned and epoch
  //will be set to hold the epoch for the returned data.
  virtual T get(int &epoch);

  //Return a 'time stamp' for the latest data
  int getEpoch(bool havelock=false);
  //Return a 'time stamp' for the oldest data
  int getOldest();

  //Sleep the calling thread until data for the requested epoch
  //is available. If the requested data is already available this
  //method will return immediately.
  void wait4Epoch(int epoch);

  inline void Lock() {pthread_mutex_lock(&itsLock);}
  inline void Unlock() {pthread_mutex_unlock(&itsLock);}

protected:
  //What element index comes next in the circular queue
  inline int nextE(int elem) {return (elem+1)%itsCapacity;}
  //What is the previous element in the circular queue
  inline int prevE(int elem) {return (((elem-=1)==-1)?itsCapacity-1:elem);}
  //Return the index for data with the given epoch.
  //Will return -1 if that epoch is not in the buffer.
  int epoch2index(int epoch);

  //Template buffer where our data are stored
  T* itsBuffer;

  //Epoch of most recent data
  int itsEpoch;
  int itsCapacity, itsCount;
  int itsHead, itsTail;

  pthread_mutex_t itsLock;
  //Mutex and condition used by wait4Epoch
  pthread_mutex_t itsWaitMutex;
  pthread_cond_t  itsWaitCond;

};

#endif
