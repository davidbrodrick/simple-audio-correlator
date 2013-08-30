//
// Copyright (C) David Brodrick, 2002-2004
// Copyright (C) CSIRO Australia Telescope National Facility
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.
//
// $Id: StoreMaster.cc,v 1.8 2004/04/16 12:57:20 brodo Exp $

#include <StoreMaster.h>
#include <IntegPeriod.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <assert.h>
#include <string.h>

//Static Members
//Maximum number of integration periods to return for any request. This is to
//prevent bad requests consuming all the memory and crashing the program.
const int StoreMaster::theirMaxResults = 1000000;


///////////////////////////////////////////////////////////////////////
//Constructor
StoreMaster::StoreMaster(char *path, long long maxage,
			 int savebufsize, int storebufsize)
:itsOldest(0),
itsNewest(0),
itsSaveBufSize(savebufsize),
itsSaveBuf(itsSaveBufSize),
itsNumQueued(0),
itsStoreBufSize(storebufsize),
itsStoreBuf(itsStoreBufSize),
itsMaxDataAge(maxage)
{
  //assert(itsStoreBufSize>=itsSaveBufSize);

  //Initialise class lock
  pthread_mutex_init(&itsLock, NULL);
  pthread_mutex_init(&itsDirLock, NULL);

  //Make a copy of the argument save path string
  int len = strlen(path)+1;
  itsSaveDir = new char[len];
  strcpy(itsSaveDir, path);
  //Check if we need to add trailing / to directory name
  if (itsSaveDir[len-2]!='/') itsSaveDir[len-2] = '/';
  itsSaveDir[len-1]='\0';
}


///////////////////////////////////////////////////////////////////////
//Constructor - string
StoreMaster::StoreMaster(string path, long long maxage,
			 int savebufsize, int storebufsize)
:itsOldest(0),
itsNewest(0),
itsSaveBufSize(savebufsize),
itsSaveBuf(itsSaveBufSize),
itsNumQueued(0),
itsStoreBufSize(storebufsize),
itsStoreBuf(itsStoreBufSize),
itsMaxDataAge(maxage)
{
  //assert(itsStoreBufSize>=itsSaveBufSize);

  //Initialise class lock
  pthread_mutex_init(&itsLock, NULL);
  pthread_mutex_init(&itsDirLock, NULL);

  //Make a copy of the argument save path string
  int len = path.length()+2;
  itsSaveDir = new char[len];
  strcpy(itsSaveDir, path.c_str());
  //Check if we need to add trailing / to directory name
  if (itsSaveDir[len-2]!='/') itsSaveDir[len-2] = '/';
  itsSaveDir[len-1]='\0';
}


///////////////////////////////////////////////////////////////////////
//Destructor
StoreMaster::~StoreMaster()
{
  //Ensure all data has been written to file
  flush();
  //Get lock, and then destroy it
  Lock();
  pthread_mutex_destroy(&itsLock);
  //Free all allocated memory
  delete[] itsSaveDir;
}


///////////////////////////////////////////////////////////////////////
//Flush any waiting data to disk
void StoreMaster::flush(bool havelock)
{
  bool res = true;
  if (!havelock) Lock();
  pthread_mutex_lock(&itsDirLock);

  //Firstly, free up disk space by removing any data which is too old
  if (itsMaxDataAge!=0) removeOldData();

  //Check what range of data we need to save
  int epoch  = itsSaveBuf.getOldest();
  int newest = itsSaveBuf.getEpoch();

  while(res && epoch<=newest) {
    int bufepoch = epoch;
    time_t filetime = itsSaveBuf.get(bufepoch)->timeStamp/1000000;
    struct tm *utc = gmtime(&filetime);
    struct tm fileutc;
    memcpy(&fileutc,utc,sizeof(struct tm));

    //Get the name of the file based on timestamp
    ostringstream oss;
    toFileName(itsSaveBuf.get(bufepoch)->timeStamp, oss);

    //Check that all the directories exist
    if (!checkDirs(oss)) {
      cerr << "Warning, could not make directory for save file\n";
      res = false;
    }
    //Open the file
    ofstream datfile(oss.str().c_str(), ios::app|ios::out|ios::binary);
    //Check if we were successful
    if (datfile.fail()) {
      cerr << "Warning, could not open save file for writing\n"
	<< "\t" << oss.str() << "\n";
      res = false;
    }

    //Save each queued IntegPeriod which shares the same
    //destination file name as that which we determined above
    for (;epoch<=newest && res; epoch++) {
      //Get the next data to be saved from the buffer
      IntegPeriod *saveper = itsSaveBuf.get(epoch);
      time_t thistime = saveper->timeStamp/1000000;
      utc = gmtime(&thistime);
      if (utc->tm_year!=fileutc.tm_year ||
	  utc->tm_mon!=fileutc.tm_mon ||
	  utc->tm_mday!=fileutc.tm_mday ||
	  utc->tm_hour!=fileutc.tm_hour ||
	  utc->tm_min!=fileutc.tm_min)
      {
        //Time to change the destination file
        break;
      }
      //Write the data to disk
      datfile << (*saveper);
    }
    //Flush the data to disk and close the file
    datfile.flush();
    datfile.close();
  }
  //Clear the buffer
  itsNumQueued = 0;
  itsSaveBuf.makeEmpty();
  pthread_mutex_unlock(&itsDirLock);
  if (!havelock) Unlock();
}


///////////////////////////////////////////////////////////////////////
//Take the given data and add it to the store
void StoreMaster::put(IntegPeriod *newper)
{
  Lock();
  //Add this period to the waiting queue
  itsSaveBuf.put(newper);
  itsNumQueued++;
  //If we are now full write all waiting data to disk
  if (itsNumQueued==itsSaveBufSize) flush(true);

  //And add it to the cache of most recent data
  //Check if the buffer is full, we need to remove the oldest
  if (itsStoreBuf.getEntries() == itsStoreBufSize) {
    int epoch = itsStoreBuf.getOldest();
    IntegPeriod *oldper = itsStoreBuf.get(epoch);
    delete oldper;
  }
  //Insert the new data into our storage cache
  itsStoreBuf.put(newper);
  //Get the timestamp of the oldest data in the storage buffer
  //We need this to know what data is available in memory
  int epoch = itsStoreBuf.getOldest();
  IntegPeriod *oldper = itsStoreBuf.get(epoch);
  itsOldest = oldper->timeStamp;
  itsNewest = newper->timeStamp;
  Unlock();
}


///////////////////////////////////////////////////////////////////////
//Get most recent data
IntegPeriod *StoreMaster::getRecent()
{
  IntegPeriod *res = NULL;
  Lock();
  //Only works if data in memory for the moment
  if (itsNewest!=0) {
    int epoch = -1;
    res = new IntegPeriod();
    (*res) = (*itsStoreBuf.get(epoch)); //Clone the data from the buffer
  }
  Unlock();
  return res;
}


///////////////////////////////////////////////////////////////////////
//Get first data with with time stamp after 'epoch'
IntegPeriod *StoreMaster::get(long long epoch)
{
  IntegPeriod *res = NULL;
  Lock();
  //First check if we still have the data in memory
  if (itsOldest!=0 && epoch>=itsOldest && epoch<itsNewest) {
    //We have it in memory, find it. At present this is just a linear
    //search though it probably should use a binary search instead
    int last  = itsStoreBuf.getOldest();
    int newest = itsStoreBuf.getEpoch();
    while (last<=newest) {
      IntegPeriod *loadper = itsStoreBuf.get(last);
      if (loadper->timeStamp>epoch) {
	//We found it, so clone it
	res = new IntegPeriod;
	(*res) = (*loadper);
	break;
      }
      last++;
    }
  } else {
    //It is older than the oldest in memory, we need to check the disk
    ifstream *infile;
    if (findEpoch(epoch, infile)) {
      //We found the requested data, allocate memory and load from file
      res = new IntegPeriod;
      *infile >> (*res);
      //Finished with the file
      infile->close();
      delete infile;
    }
  }
  Unlock();
  return res;
}


///////////////////////////////////////////////////////////////////////
//Get all data newer than the specified epoch
IntegPeriod **StoreMaster::get(long long startepoch,
			       long long endepoch,
			       int &count)
{
  int ressize = 10;
  IntegPeriod **res = new IntegPeriod*[ressize];
  count = 0;
  bool buffull = false;
  bool inmemory = false;
  bool havelock = false;
  bool firstfile = true;
  long long epoch = startepoch;

  Lock();
//  cerr << "Request for " << startepoch << " oldest is " << itsOldest << " newest is " << itsNewest << endl;
  //Grab a timestamp for the oldest data in memory
  long long oldest = itsOldest;
  if (itsOldest!=0 && epoch>=itsOldest && epoch<itsNewest) {
    inmemory = true;
  }
  //Unlock memory storage so new data can be inserted. while we load from disk
  Unlock();

  //We loop through all files between the specified epoch and the
  //present pulling out the requested data
  ifstream *infile;
  while (!buffull&&!inmemory) {
    //Open the appropriate file
    if (firstfile) {
      firstfile = false;
      //Try to open the first file
      if (!findEpoch(epoch, infile)) {
	//File didn't exist, move along to the next
	if (!(epoch = nextFile(epoch, infile))) {
	  if (!havelock) Lock();
	  break;
	}
      }
    } else {
      //Try to open the next file
      if (!(epoch = nextFile(epoch, infile))) {
	if (!havelock) Lock();
	break;
      }
    }

    //Load the data from the current file
    while (!infile->eof()&&!buffull&&!inmemory) {
      res[count] = new IntegPeriod;
      *infile >> (*res[count]);
      if (infile->eof()) {
	//If we had an EOF the last period loaded will be invalid
	delete res[count];
      } else {
	//If we are close to data in the memory cache get the lock
	//so that we can ensure we don't miss any periods
	if (!havelock && oldest!=0 && res[count]->timeStamp>=oldest) {
	  Lock();
	  havelock = true;
	}
	//Check if the current data, and all more recent, is cached
	if (havelock && itsOldest!=0 && res[count]->timeStamp>=itsOldest) {
	  delete res[count];
	  inmemory = true;
	} else if (endepoch!=0 && res[count]->timeStamp>endepoch) {
          //We have reached the last data requested
	  delete res[count];
	  buffull = true;
	} else {
	  count++;
	  //Check if we have exceeded the allowed number of results
	  if (count>theirMaxResults) buffull = true;
	  if (count==ressize && !buffull) {
	    //We have filled the output array, so double it's size
	    ressize*=2;
	    IntegPeriod **newres = new IntegPeriod*[ressize];
	    //Copy all existing data over to new storage array
	    for (int i=0; i<count; i++) newres[i] = res[i];
	    delete[] res;
	    res = newres;
	  }
	}
      }
    }
    //Finished with this file
    infile->close();
  }
  Unlock();

  //We have loaded all data from disk, now add any still in memory
  if (!buffull && itsOldest!=0) {
    int last  = itsStoreBuf.getOldest();
    int newest = itsStoreBuf.getEpoch();

    //Find the first data older than the requested epoch
    assert(last<=newest);
    while (last<=newest) {
      IntegPeriod *loadper = itsStoreBuf.get(last);
      if (loadper->timeStamp>=startepoch) {
	break;
      }
      last++;
    }
    //If not returning all recent data find the last datum we will return
    if (endepoch!=0) {
      //Find the first data older than the requested epoch
      while (newest>last && (*itsStoreBuf.get(newest))>endepoch) {
	newest--;
      }
    }

    //Count how many buffered periods are stored in memory
//    cerr << count + newest-last+1 << " = " << count << "+" <<newest-last+1<<endl;
    count += newest - last + 1;
    //Ensure there is room for the extra data
    if (count>ressize) {
      IntegPeriod **newres = new IntegPeriod*[count];
      //Copy all existing data over to new storage array
      for (int i=0; i<ressize; i++) newres[i] = res[i];
      delete[] res;
      res = newres;
      ressize = count;
    }
    int initval = count-newest+last-1;
    for (int i=initval; i<count; i++) {
      //Clone each of the data we are going to return
      IntegPeriod *loadper = itsStoreBuf.get(last);
      if (loadper==NULL) break;
      res[i] = new IntegPeriod;
      (*res[i]) = (*loadper);
      last++;
    }
  }

//  Unlock();
  //Check if we created the array for nothing
  if (count==0 && res) {
    delete[] res;
    res = NULL;
  }
  return res;
}


///////////////////////////////////////////////////////////////////////
//Return the filename where data for the given epoch should be stored
void StoreMaster::toFileName(long long epoch, ostringstream &output)
{
  //Build up the name of the file based on timestamp
  time_t filetime = epoch/1000000;
  struct tm *utc = gmtime(&filetime);
  //Base directory given to the program
  output << itsSaveDir;
  //Year
  output << 1900+utc->tm_year << "/";
  //Month
  int temp = utc->tm_mon+1;
  if (temp<10) output << "0" << temp << "/";
  else output << temp << "/";
  //Day of month
  temp = utc->tm_mday;
  if (temp<10) output << "0" << temp << "/";
  else output << temp << "/";
  //Hour of day
  temp = utc->tm_hour;
  if (temp<10) output << "0" << temp;
  else output << temp;
  //Minute of the hour
  temp = utc->tm_min;
  if (temp<10) output << "0" << temp;
  else output << temp;
}


///////////////////////////////////////////////////////////////////////
//Ensure all directories in the given path exist
bool StoreMaster::checkDirs(ostringstream &arg)
{
  bool res = true;

  string savepath(arg.str());
  int start = savepath.find("/");
  int end = savepath.find("/", start+1);

//  pthread_mutex_lock(&itsDirLock);
  while (start!=-1 && end!= -1 && res) {
    //Get the path for the next directory in the string
    string temppath = savepath.substr(0,end+1);
    //check if the directory already exists
    struct stat buf;
    if (stat(temppath.c_str(),&buf)==-1) {
      //It doesn't already exist, try to make the directory
	if (mkdir(temppath.c_str(), S_IRUSR|S_IXUSR|S_IWUSR
		  |S_IRGRP|S_IXGRP|S_IWGRP|S_IXOTH)==-1)
      {
        //We couldn't create the directory
	res = false;
	perror(temppath.c_str());
      }
    }
    //Advance to the next directory name
    start = end + 1;
    end = savepath.find("/", start+1);
  }
//  pthread_mutex_unlock(&itsDirLock);
  return res;
}


///////////////////////////////////////////////////////////////////////
//
bool StoreMaster::checkFile(long long epoch, ifstream *&infile)
{
  //Get the file name of where to look for the given epoch
  ostringstream filename;
  toFileName(epoch, filename);
  //Try to open the file
  //infile->open(filename.str().c_str(), ios::nocreate|ios::binary);
  infile = new ifstream(filename.str().c_str(), ios::binary);

  bool res = true;
  if (infile->fail()) res = false;
  return res;
}


///////////////////////////////////////////////////////////////////////
//
bool StoreMaster::checkFile(long long epoch)
{
  bool res = false;
  //Get the file name of where to look for the given epoch
  ostringstream filename;
  toFileName(epoch, filename);
  //Try to open the file, close it again if we are successful
  //ifstream testfile(filename.str().c_str(), ios::nocreate|ios::binary);
  ifstream testfile(filename.str().c_str(), ios::binary);
  if (!testfile.fail()) {
    testfile.close();
    res = true;
  }
  return res;
}


///////////////////////////////////////////////////////////////////////
//
bool StoreMaster::findEpoch(long long epoch, ifstream *&infile)
{
  bool res = false;
  long long tstamp;
  int readsize = sizeof(int)+sizeof(long long);

  pthread_mutex_lock(&itsDirLock);
  //First check if a file exists for the specified epoch
  if (checkFile(epoch, infile)) {
    while (!infile->eof()&&!res) {
      //Read just the size and timestamp from the file
      int size;
      infile->read((char*)&size, sizeof(int));
      infile->read((char*)&tstamp, sizeof(long long));
      if (infile->eof()) break;
      if (tstamp<=epoch) {
	//We need to seek to the start of the next period
	infile->seekg(size-readsize,ios::cur);
      } else {
	//The period on disk has timestamp after the argument, this
	//is the period we wish to return, seek back to it's start
	infile->seekg(-readsize,ios::cur);
	res = true;
      }
    }
    //Close the file if we didn't find what we wanted
    if (!res) {
      infile->close();
    }
  }
  pthread_mutex_unlock(&itsDirLock);
  ///TODO: Check start of next file
  return res;
}


///////////////////////////////////////////////////////////////////////
//
long long StoreMaster::prevFile(long long epoch, ifstream *&infile)
{
  ostringstream filename;
  pthread_mutex_lock(&itsDirLock);

  //Take 60 seconds from the given epoch
  long long testepoch = epoch-60000000;
  //check if the previous (consecutive) file exists
  if (checkFile(testepoch)) {
    //Consecutive file DOES exists, open it and finish
    toFileName(testepoch, filename);
    infile->open(filename.str().c_str(), ios::binary);
    //infile.open(filename.str().c_str(), ios::nocreate|ios::binary);
    epoch = testepoch;
  } else {
    //No prev file was found
    epoch = 0;
  }
  pthread_mutex_unlock(&itsDirLock);

  return epoch;
}


///////////////////////////////////////////////////////////////////////
//
long long StoreMaster::nextFile(long long epoch, ifstream *&infile)
{
  ostringstream filename;
  pthread_mutex_lock(&itsDirLock);

  //Add 60 seconds to the given epoch
  long long testepoch = epoch+60000000;
  //check if the next (consecutive) file exists
  if (checkFile(testepoch)) {
    //Consecutive file DOES exists, open it and finish
    toFileName(testepoch, filename);
    //infile->open(filename.str().c_str(), ios::nocreate|ios::binary);
    if (infile!=NULL) delete infile;
    infile = new ifstream(filename.str().c_str(), ios::binary);
    epoch = testepoch;
  } else {
    //The next file didn't exist, we have to search to see which is
    //the next data saved to disk.
    if (searchForward(epoch, filename)) {
      epoch = 0;
      if (infile!=NULL) delete infile;
      infile = new ifstream(filename.str().c_str(), ios::binary);
      infile->seekg(sizeof(int));
      if (*infile) {
	//If the file is ok, read a timestamp
	infile->read((char*)&epoch, sizeof(long long));
	//And then return to the start of the file
	infile->seekg(0);
      } else {
cerr << "FILE IS BAD";
	//Could not open the discovered file
	epoch = 0;
      }
    } else {
      //No next file was found
      epoch = 0;
    }
  }
  pthread_mutex_unlock(&itsDirLock);

  return epoch;
}


///////////////////////////////////////////////////////////////////////
//For use with scandir, selects entries of the form [0-9][0-9][0-9][0-9]
int quaddigit(const dirent *entry)
{
  const char *entryname = entry->d_name;
  if (isdigit(entryname[0]) && isdigit(entryname[1]) &&
      isdigit(entryname[2]) && isdigit(entryname[3]) && entryname[4]=='\0')
    return 1;
  else return 0;
}

///////////////////////////////////////////////////////////////////////
//For use with scandir, selects entries of the form [0-9][0-9]
int bidigit(const dirent *entry)
{
  const char *entryname = entry->d_name;
  if (isdigit(entryname[0]) && isdigit(entryname[1]) && entryname[2]=='\0')
    return 1;
  else return 0;
}


///////////////////////////////////////////////////////////////////////
//Recurse the directories and find first data file after 'epoch's
//This is one hell of a routine!
bool StoreMaster::searchForward(long long epoch, ostringstream &output)
{
  return fileAfter(itsSaveDir, epoch, output);
}


///////////////////////////////////////////////////////////////////////
//Recurse the specified directory, find first data file after 'epoch's
bool StoreMaster::fileAfter(string basedir,
			    long long epoch,
			    ostringstream &output)
{
  bool done = false;
  time_t searchtime = epoch/1000000;
  struct tm *searchutc = gmtime(&searchtime);
  bool sameday = true, samemonth = true, sameyear = true;

  //These hold a string form of the argument time
  ostringstream argyear, argmonth, argday, argfile;
  argyear << 1900+searchutc->tm_year;
  int temp = searchutc->tm_mon+1;
  if (temp<10) argmonth << "0" << temp;
  else argmonth << temp;
  temp = searchutc->tm_mday;
  if (temp<10) argday << "0" << temp;
  else argday << temp;
  temp = searchutc->tm_hour;
  if (temp<10) argfile << "0" << temp;
  else argfile << temp;
  temp = searchutc->tm_min;
  if (temp<10) argfile << "0" << temp;
  else argfile << temp;


  ostringstream dirname;
  //Get a list of all year directories
  dirname << basedir;
  dirent **yearslist;
  int numyears = scandir(dirname.str().c_str(), &yearslist,
			 quaddigit, alphasort);
  if (numyears==0) {
    //No data in the archive
    return false;
  }
  int startyear;
  for (startyear=0; startyear<numyears; startyear++) {
    //We don't want to look backward in time so seek through the list
    //until we find the same, or a later, year than the argument epoch
    if (strcmp(yearslist[startyear]->d_name, argyear.str().c_str())==0) {
      //cerr << "Found the same year\n";
      break;
    }
    if (strcmp(yearslist[startyear]->d_name, argyear.str().c_str())>0) {
      sameyear = samemonth = sameday = false;
      break;
    }
  }
  //Ensure we found at least one suitable directory to search
  if (startyear==numyears) {
    for (int rm=0;rm<numyears;rm++) delete yearslist[rm];
    delete[] yearslist;
    done = true;
  }

  //Loop over each year
  for (int year=startyear; year<numyears && !done; year++) {
    ostringstream yeardir;
    yeardir << itsSaveDir << yearslist[year]->d_name << "/";
    //cerr << "yeardir is " << yeardir.str() <<endl;
    //Next we need to get a listing of what months have data for this year
    dirent **monthslist;
    int nummonths = scandir(yeardir.str().c_str(), &monthslist,
			    bidigit, alphasort);
    int startmonth = 0;
    if (samemonth) {
      //We need to seek through and find the same, or a later, month
      for (startmonth=0; startmonth<nummonths; startmonth++) {
	if (strcmp(monthslist[startmonth]->d_name, argmonth.str().c_str())==0){
	  //cerr << "Found the same month\n";
	  break;
	}
	if (strcmp(monthslist[startmonth]->d_name, argmonth.str().c_str())>0){
	  samemonth = sameday = false;
	  break;
	}
      }
    }
    //Loop over each month
    for (int month=startmonth; month<nummonths && !done; month++) {
      ostringstream monthdir;
      monthdir << yeardir.str() << monthslist[month]->d_name << "/";
      //cerr << "monthdir is " << monthdir.str() << endl;

      //Next get a listing of what days are available for this month
      dirent **dayslist;
      int numdays = scandir(monthdir.str().c_str(), &dayslist,
			    bidigit, alphasort);
      int startday = 0;
      if (sameday) {
	//We need to seek through and find the same, or a later, day
	for (startday=0; startday<numdays; startday++) {
	  if (strcmp(dayslist[startday]->d_name, argday.str().c_str())==0){
	    //cerr << "Found the same day\n";
	    break;
	  }
	  if (strcmp(dayslist[startday]->d_name, argday.str().c_str())>0){
	    sameday = false;
	    break;
	  }
	}
      }
      //Loop over each day
      for (int day=startday; day<numdays && !done; day++) {
	ostringstream daydir;
	daydir << monthdir.str() << dayslist[day]->d_name << "/";
	//cerr << "daydir is " << daydir.str() << endl;

	//Finally get a listing of what files are present
	dirent **fileslist;
	int numfiles = scandir(daydir.str().c_str(), &fileslist,
			       quaddigit, alphasort);

        int startfile = 0;
	if (sameday) {
	  //We are checking on the same day, need to skip past argument file
	  for (startfile=0; startfile<numfiles; startfile++) {
	    if (strcmp(fileslist[startfile]->d_name, argfile.str().c_str())>0){
	      break;
	    }
	  }
	}
	//If there are any files left we have found the answer
	if (startfile<numfiles) {
	  output << daydir.str() << fileslist[startfile]->d_name;
	  ostringstream foo;
	  //cerr << "file is " << output.str() << " files=" << numfiles<<endl;
          done = true;
	}
	sameday = false;
	for (int rm=0;rm<numfiles;rm++) delete fileslist[rm];
	delete[] fileslist;
      } //End "for each day"
      samemonth = sameday = false;
      for (int rm=0;rm<numdays;rm++) delete dayslist[rm];
      delete[] dayslist;
    } //End "for each month"
    sameyear = samemonth = sameday = false;

    for (int rm=0;rm<nummonths;rm++) delete monthslist[rm];
    delete[] monthslist;

  } //End "for each year"
  for (int rm=0;rm<numyears;rm++) delete yearslist[rm];
  delete[] yearslist;

  return done;
}


///////////////////////////////////////////////////////////////////////
//
long long StoreMaster::fromFileName(string fname)
{
  string d(fname.substr(strlen(itsSaveDir)));
  istringstream t(d);

  timegen_t bat = 0;
  struct tm brktm;

  int year, month, day, hour, min;
  char temp;

  t >> year;
  t >> temp;
  if (temp!='/' || year<1900 || year>2200) return -1;
  brktm.tm_year = year-1900;

  t >> month;
  t >> temp;
  if (temp!='/' || month<1 || month>12) return -1;
  brktm.tm_mon = month-1;

  t >> day;
  t >> temp;
  if (temp!='/' || day<1 || day>31) return -1;
  brktm.tm_mday = day;

  //t should now be a quaddigit fname like '0341'
  char c1, c2, c3, c4;
  t >> c1 >> c2 >> c3 >> c4;
  //cerr << c1 << c2 << c3 << c4 << endl;
  if (!isdigit(c1) || !isdigit(c2) || !isdigit(c3)|| !isdigit(c4)) return -1;

  //Reconstruct hour and minute from the characters
  hour = 10*(c1-'0') + (c2-'0');
  min  = 10*(c3-'0') + (c4-'0');
  if (hour<0 || hour>23) return -1;
  if (min<0  || min>59) return -1;
  brktm.tm_hour = hour;
  brktm.tm_min = min;
  brktm.tm_sec = 0;

  bat = mktime(&brktm);
  //mktime converts local time but we've got UTC so compensate by timezone
#ifndef __CYGWIN__ //Must be a POSIX way of doing this
  bat -= __timezone;
  if (__daylight) bat += 3600;
#else
  bat -= _timezone;
  if (_daylight) bat += 3600;
#endif
  //if daylight savings is in place, need to compensate.
  //this will probably be a curse.

  bat*=1000000;

  return bat;
}


///////////////////////////////////////////////////////////////////////
// Delete any data which is older than itsMaxDataAge: but, to prevent
// catastrophe we don't delete data more than a week old.
void StoreMaster::removeOldData()
{
  //Get current time
  timeAbs_t now = getAbs();
  timeAbs_t expiry = now-itsMaxDataAge;
  timeAbs_t latest = expiry - 7*86400000000ll;

#ifdef DEBUG_STORE
  cerr << "now:\t" << printTime(now);
  cerr << "expiry:\t" << printTime(expiry);
  cerr << "latest:\t" << printTime(latest);
#endif

  //Our objective is now to remove all files between t and itsMaxDataAge
  while (true) {
    ostringstream fname;
    if (!searchForward(latest, fname)) break;
    //cerr << "Found File: " << fname.str() << endl;

    //We found a file. Now find out what epoch it represents
    timeAbs_t fage = fromFileName(fname.str());
    if (fage==-1) {
      cerr << "StoreMaster:removeOldData: WARNING: filename not understood:\n"
	<< "\t" << fname.str() << endl
	<< "NO DATA FILES COULD BE UNLINKED\n";
      break;
    }

    if (fage>latest && fage<expiry) {
#ifdef DEBUG_STORE
      cerr << "will delete file:\t" << fname.str() << endl;
      cerr << "file time:       \t" << printTime(fage);
#endif
      if (unlink(fname.str().c_str())!=0) {
	perror(("StoreMaster:removeOldData:"+fname.str()).c_str());
      }

      //This is where we need to check for directories which
      //are now empty and in need of removal.

    } else break; //No files need deleting at the moment
  }
}
