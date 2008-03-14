//
// Copyright (C) David Brodrick, 2002
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.
//
// $Id: ThreadedObject.cc,v 1.2 2003/08/27 13:17:40 bro764 Exp brodo $

//Yet another C++ thread wrapper class.
//Create a subclass of this and define a custom 'run' method. When a 
//caller calls the 'start' method a new POSIX thread will be spawned
//and will immediately jump to the subclasses 'run' method.
//Be sure to use 'itsKeepRunning' as a loop condition in the 'run' method.
//When 'stop' is called this will be set false and the thread is expected
//to exit.

#include <ThreadedObject.h>
#include <unistd.h>
#include <iostream>

using namespace::std;

///////////////////////////////////////////////////////////////////////
//Helper function to launch new threads
void *threadedobject_run(void *arg) {
  ((ThreadedObject*)arg)->itsThread = pthread_self();
  ((ThreadedObject*)arg)->run();
  return NULL;
}


///////////////////////////////////////////////////////////////////////
//Constructor
ThreadedObject::ThreadedObject()
:itsKeepRunning(false)
{
  pthread_mutex_init(&itsLock,NULL);
}


///////////////////////////////////////////////////////////////////////
//Destructor
ThreadedObject::~ThreadedObject()
{
  Lock();
  if (itsKeepRunning) {
    itsKeepRunning=false;
    pthread_join(itsThread,NULL);
  }
  pthread_mutex_destroy(&itsLock);
}


///////////////////////////////////////////////////////////////////////
//Spawn a new thread and jump it to the run() method.
void ThreadedObject::start()
{
  Lock();
  //Indicate that we want the thread to run
  itsKeepRunning = true;
  //Spawn a new thread
  pthread_create(&itsThread, NULL, threadedobject_run, (void*)this);
  Unlock();
}


///////////////////////////////////////////////////////////////////////
//Tell the thread to exit and block until it has joined.
bool ThreadedObject::stop()
{
  bool res = false;
  Lock();
  if (itsKeepRunning) {
    //Indicate to thread that it should stop running
    itsKeepRunning = false;
    //Wait for the thread to actually finish up and exit
    pthread_join(itsThread,NULL);
    res = true;
  }
  Unlock();
  return res;
}


///////////////////////////////////////////////////////////////////////
//Main loop of execution for the thread. We just sleep here.
void ThreadedObject::run()
{
  while (itsKeepRunning) {
    cout << "WARNING, running threadedObject::run()\n";
    sleep(10);
  }
  pthread_exit(NULL);
}
