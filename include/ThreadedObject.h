//
// Copyright (C) David Brodrick, 2002
// Copyright (C) CSIRO Australia Telescope National Facility
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.
//
// $Id: ThreadedObject.h,v 1.3 2004/02/06 09:19:09 brodo Exp $

//Yet another C++ thread wrapper class.
//Create a subclass of this and define a custom 'run' method. When a 
//caller calls the 'start' method a new POSIX thread will be spawned
//and will immediately jump to the subclasses 'run' method.
//Be sure to use 'itsKeepRunning' as a loop condition in the 'run' method.
//When 'stop' is called this will be set false and the thread is expected
//to exit.

#ifndef _THREADEDOBJECT_HDR_
#define _THREADEDOBJECT_HDR_

#include <pthread.h>

//Forward declarations
//This function is used for spawning new threads. The C function used
//for creating new threads can only launch the new thread to a C function.
//We use this C function to send the thread to our 'run' method.
extern void* threadedobject_run(void*);

class ThreadedObject {
  //Helper function for spawning new threads
  friend void *threadedobject_run(void*);
public:
  ThreadedObject();
  virtual ~ThreadedObject();
  //This will spawn a new thread and jump to run() method. It is
  //the run method which you wish to inherit and customise.
  //Be sure to include 'itsKeepRunning' as a loop condition.
  void start();
  //This will block until the thread has joined. If no thread is
  //currently running false will be returned.
  bool stop();

  inline void Lock()   {pthread_mutex_lock(&itsLock);}
  inline void Unlock() {pthread_mutex_unlock(&itsLock);}

protected:
  //Main loop of execution for the dedicated thread
  virtual void run();

  //Thread ID of the dedicated thread
  pthread_t itsThread;
  //Lock for this object, initialised in constructor
  pthread_mutex_t itsLock;
  //Indicates if the thread should keep running. You should include
  //this as a loop condition in your main loop for the thread.
  bool itsKeepRunning;
};

#endif
