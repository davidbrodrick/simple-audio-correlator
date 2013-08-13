//
// Copyright (C) David Brodrick
// Copyright (C) CSIRO Australia Telescope National Facility
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.
//
// $Id: IntegPeriod.h,v 1.16 2004/11/21 01:56:36 brodo Exp brodo $

//Class to encapsulate the information produced each telescope integration
//period. This wraps up most operations which can be performed on an
//integration period, such as correlating raw data, printing the data
//in a simple ASCII text format, writing and saving data from disk, etc.
//
//This is a part of the original implementation of sac and so doesn't really
//understand about different TeleSystems. Each IntegPeriod assumes that
//it contains data for an interferometer system with two sampled inputs.
//Other classes try to make this appear in a suitable format for the client
//software, but at this level every integration period contains the data
//from two receivers and the cross product of the two.
//
//If you are looking for funky software correlation code, this isn't the
//place to look. The basic correlation/Fourier transformation process is
//pretty straightforward and we just use a few canned routines to 
//accomplish this processing. It would probably be worth implementing
//our own versions however simply to remove the dependence on libdsp.
//Another option is just to incorporate the relevant libdsp code into sac.

#ifndef _INTEGPERIOD_HDR_
#define _INTEGPERIOD_HDR_

#include <TCPstream.h>
#include <fstream>

//Type read in from the sound card
typedef signed short audio_t;

//Enumeration for different types of windowing mode
typedef enum windowing_mode {
  win_none=0,
  win_hanning,
  win_hamming,
  win_costapered,
  win_genericcos,
  win_blackman,
  win_exactblackman,
  win_blackmanharris,
  win_bartlett,
  win_tukey,
  win_flattop
} windowing_mode;


class IntegPeriod {
public:
  IntegPeriod();
  //Destructor frees all subdata if they exist
  ~IntegPeriod();

  //Don't destruct, just reset
  inline void clear() {
    power1 = 0.0;
    power2 = 0.0;
    powerX = 0.0;
    amplitude = 0.0;
    phase = 0.0;
    timeStamp = 0;
  }

  //Number of samples of audio for each channel
  int audioLen;
  //rawAudio holds the raw audio as stereo interleaved samples
  //hence rawAudio is of length 2*audioLen
  audio_t* rawAudio;

  //Start time of the integration period. microseconds since the epoch,
  //00:00:00 UTC January 1 1970
  long long timeStamp;

  //Number of frequency domain channels in the spectral output
  int numBins;

  //Spectra for input 1. Length is given by the 'numBins' field.
  float *input1Spec;
  //Spectra for input 2. Length is given by the 'numBins' field.  
  float *input2Spec;
  //Real cross spectra. Length is given by the 'numBins' field.
  float *crossSpec;
  //Imaginary cross spectra. Length is given by the 'numBins' field.  
  float *phaseSpec; //name is misleading. Not phase - just imaginary.    

  //Detected power level for input 1
  float power1;
  //Detected power level for input 2
  float power2;
  //Detected zero-lag cross power level
  float powerX;  
  
  //The complex amplitude for the cross correlated channel during this period.
  float amplitude;
  //The complex phase for the cross correlated channel during this period.  
  float phase;

  //Flag to indicate if this period is considered to be peturbed by RFI.
  bool RFI;

  //Calculate simple zero lag correlations with the given channel gains
  void doCorrelations(float gain1, float gain2);
  //Calculate simple zero lag correlations with unity gain
  void doCorrelations();
  
  //Discard any data except the indicated data
  void keepOnly(bool cross, bool inputs, bool audio);

  //These methods load a bunch of IntegPeriods from a source
  //data and count are filled in by the methods
  //count may be zero even if true is returned (connection ok but no data)
  static bool load(IntegPeriod *&data, int &count,
                   long long start, long long end,
                   const char *server="localhost",
                   int port=31234, bool preprocess=true);
  static bool load(IntegPeriod *&data, int &count,
                   long long start, long long end,
                   TCPstream &sock, bool preprocess=true);
  //Load 'raw' data (includes audio if available)
  static bool loadraw(IntegPeriod *&data, int &count,
                      long long start, long long end,
                      int &samprate,
                      const char *server="localhost", int port=31234);
  static bool loadraw(IntegPeriod *&data, int &count,
                      long long start, long long end,
                      int &samprate, TCPstream &sock);
  //Load from a file
  static bool load(IntegPeriod *&data, int &count, const char *infile, bool keepaudio=false);
  static bool load(IntegPeriod *&data, int &count, ifstream &infile, bool keepaudio=false);

  //Write the array of IntegPeriods to a file
  //Fails silently it seems...
  static void write(const IntegPeriod *data, int count, ofstream &outfile);
  static void write(const IntegPeriod *data, int count, const char *fname);
  static void writeASCII(const IntegPeriod *data, int count,
			 ofstream &outfile);
  static void writeASCII(const IntegPeriod *data, int count,
			 const char *fname);
  //Write any audio data out in a WAVE file format
  static void writeWAVE(const IntegPeriod *data, int count,
			int samprate, string fname);

  //Scale both inputs to the scale of power1 of the reference
  static void rescale(IntegPeriod *data, int datalen,
                      IntegPeriod *ref, int reflen);
  //Scale and offset both inputs to the scale of power1 of the reference
  static void translate(IntegPeriod *data, int datalen,
			IntegPeriod *ref, int reflen);
  //Merge and timesort the given, sorted, integration periods
  static void merge(IntegPeriod *&resdata, int &rescount,
		    IntegPeriod *data1, int count1,
		    IntegPeriod *data2, int count2);
  //Time sort the unordered integration periods
  static void sort(IntegPeriod *&data, int count);
  //Normalise power 1 and power 2 values and then scale cross power
  static void normalise(IntegPeriod *&data, int count);
  //Accumulate integration periods up to the indicated duration
  //Assumes data are sorted
  static void integrate(IntegPeriod *&cleandata, int &cleanlen,
			IntegPeriod *data, int len,
			long long period, bool keeprfi=false);
  //Return a new series by purging the RFI-flagged integration periods
  static int purgeFlagged(IntegPeriod *&res, int &reslen,
			  IntegPeriod *data, int datalen);
  void operator=(const IntegPeriod &rhs);
  //    void operator=(IntegPeriod &rhs);
  //Return the given index from the cross power spectrum
  float operator[](int index);
  //Operators for accumulating
  IntegPeriod operator+(IntegPeriod &rhs);
  IntegPeriod operator/(int divisor);
  void operator+=(IntegPeriod &rhs);
  void operator/=(int divisor);
  //Operators to compare time stamps
  int operator==(long long tstamp);
  int operator<=(long long tstamp);
  int operator>=(long long tstamp);
  int operator<(long long tstamp);
  int operator>(long long tstamp);
  signed long long operator-(long long tstamp);
  int operator==(IntegPeriod &rhs);
  int operator<=(IntegPeriod &rhs);
  int operator>=(IntegPeriod &rhs);
  int operator<(IntegPeriod &rhs);
  int operator>(IntegPeriod &rhs);
  signed long long operator-(IntegPeriod &rhs);
  //For printing short info with cout
  //friend ostream &operator<<(ostream& os, IntegPeriod& per);
  //Saves state of the integperiod to a file
  friend ofstream &operator<<(ofstream& os, const IntegPeriod& per);
  friend TCPstream &operator<<(TCPstream& os, const IntegPeriod& per);
  //Read state of integperiod from a file
  friend istream &operator>>(istream& os, IntegPeriod& per);

private:
  //These hold the signed audio for each channel, each length audioLen
  float *audio1, *audio2;

  //This finds the mean of the audio samples and subtracts
  //this away from each sample to produced signed data.
  //This removes the DC offset terms in the data.
  //The given gains are applied to the audio channels
  void processAudio(float gain1, float gain2);
  //Return the address of the next blocks of audio to integrate
  //Will return NULL for each pointer if insufficient audio
  //data is available.
  void getNextAudio(int blockLen, int blockNum, float *&a1, float *&a2);
  //Calculate the zero lag correlation of ch1 and ch2.
  float correlate(int len, float *ch1, float *ch2);
};

#endif
