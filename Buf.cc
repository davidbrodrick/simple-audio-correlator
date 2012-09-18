//
// Copyright (C) David Brodrick, 2002
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.
//
// $Id: Buf.cc,v 1.3 2004/04/07 12:39:18 brodo Exp brodo $

#include <Buf.h>
#include <assert.h>
#include <iostream>
#include <IntegPeriod.h>


///////////////////////////////////////////////////////////////////////
//Constructor
template <class T>
Buf<T>::Buf(int size)
  :itsEpoch(-1),
  itsCapacity(size),
  itsCount(0),
  itsHead(0),
  itsTail(0)
{
  assert(itsCapacity>0);

  pthread_mutex_init(&itsLock,NULL);
  pthread_mutex_init(&itsWaitMutex, NULL);
  pthread_cond_init(&itsWaitCond, NULL);
  itsBuffer = new T[itsCapacity];
}


///////////////////////////////////////////////////////////////////////
//Destructor
template <class T>
Buf<T>::~Buf()
{
  pthread_mutex_destroy(&itsLock);
  pthread_mutex_destroy(&itsWaitMutex);
  pthread_cond_destroy(&itsWaitCond);
}


///////////////////////////////////////////////////////////////////////
//Put a new element into the buffer
template <class T>
void Buf<T>::put(T data)
{
  Lock();
  /*cerr << "put: count=" << itsCount << " head=" << itsHead
   << " tail=" << itsTail << endl;*/
  //Insert data at head of the queue
  itsBuffer[itsHead] = data;

  //We need to advance the head pointer to next element
  itsHead = nextE(itsHead);
  if (itsCount==itsCapacity){
    //The buffer is already full, therefore we need to shift
    //the tail pointer as well as the head
    itsTail = nextE(itsTail);
    assert(itsTail==itsHead);
  } else {
    //No need to move tail yet, but we are one fuller than we were!
    itsCount++;
  }

  itsEpoch++;
  Unlock();
  //Alert waiting threads that new data is available
  pthread_mutex_lock(&itsWaitMutex);
  pthread_cond_broadcast(&itsWaitCond);
  pthread_mutex_unlock(&itsWaitMutex);
}


///////////////////////////////////////////////////////////////////////
//Get data from the buffer
template <class T>
T Buf<T>::get(int &epoch)
{
  T res = 0; //Hack initialisation to prevent compiler warning
             //Only works with current template types. Not general.
  Lock();

  //If epoch is -1 we return our latest data
  if (epoch==-1) epoch = itsEpoch;
  //Get the array index for the given epoch
  int index = epoch2index(epoch);

  if (index!=-1) {
    //If the epoch is valid get the result from the buffer
    res = itsBuffer[index];
  } else {
    //Epoch was invalid, adjust epoch to inform caller
    epoch=-1;
  }

  Unlock();
  return res;
}


///////////////////////////////////////////////////////////////////////
//Return the maximum size of our buffer
template <class T>
int Buf<T>::getSize()
{
  return itsCapacity;
}


///////////////////////////////////////////////////////////////////////
//Return the current number of entries
template <class T>
int Buf<T>::getEntries()
{
  return itsCount;
}


///////////////////////////////////////////////////////////////////////
//Remove all data from the queue
template <class T>
void Buf<T>::makeEmpty()
{
  Lock();
  itsHead = itsTail = itsCount = 0;
  itsEpoch=-1;
  Unlock();
}


///////////////////////////////////////////////////////////////////////
//Get the epoch of the latest data available
template <class T>
int Buf<T>::getEpoch(bool havelock)
{
    int res;

    if (!havelock) Lock();
    res = itsEpoch;
    if (!havelock) Unlock();

    return res;
}


///////////////////////////////////////////////////////////////////////
//Get the epoch of the oldest data available
template <class T>
int Buf<T>::getOldest()
{
    int res;

    Lock();
    res = itsEpoch - itsCount + 1;
    if (res<0 || res>itsEpoch) res = 0;
    Unlock();

    return res;
}


///////////////////////////////////////////////////////////////////////
//Wait until the specified epoch
template <class T>
void Buf<T>::wait4Epoch(int epoch)
{
  Lock();
  pthread_mutex_lock(&itsWaitMutex);
  while(epoch>itsEpoch) {
    Unlock();
    //Condition is signalled everytime new data is added
    pthread_cond_wait(&itsWaitCond,&itsWaitMutex);
    Lock();
  }
  pthread_mutex_unlock(&itsWaitMutex);
  Unlock();
}


///////////////////////////////////////////////////////////////////////
//Convert the epoch into an index into our underlaying array
template <class T>
int Buf<T>::epoch2index(int epoch)
{
  int res;

  if (epoch>itsEpoch||epoch<itsEpoch-itsCount||epoch==-1) {
    res = -1;
  } else {
    //diff is how far back to go from our latest data
    int diff = itsEpoch - epoch;
    //Latest data is at itsHead-1
    int latest = prevE(itsHead);

    res = latest-diff;
    if (res<0) res=itsCount+res;
  }

  /*cerr << "epoch2index: epoch=" <<epoch <<" res=" <<res
       << " itsEpoch=" <<itsEpoch <<" itsHead=" <<itsHead
       << " itsCount=" <<itsCount <<endl;*/

  return res;
}


template class Buf<int>;
template class Buf<float>;
template class Buf<timed_audio*>;
template class Buf<IntegPeriod*>;
