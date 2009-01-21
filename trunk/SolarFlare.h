//
// Copyright (C) David Brodrick
// Copyright (C) Swinburne University
// Copyright (C) CSIRO Australia Telescope National Facility
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.
//
// $Id:  $

#ifndef _SOLARFLARE_HDR_
#define _SOLARFLARE_HDR_

#include <TimeCoord.h>
#include <string>
#include <sstream>
#include <fstream>

//Definition of main structure that holds flare related data
class SolarFlare {
public:
  SolarFlare() {
    site.c1 = 0.0;
    site.c2 = 0.0;
    n = 0.85;
    tau = 0;
    alpha = 1;
    final_output = NULL;
  }

  //Get the parameter of fit "N"
  inline float getN() { return n; }
  //Get the parameter of fit alpha
  inline float getAlpha() { return alpha; }
  //Get the parameter of fit Tau
  inline timegen_t getTau() { return tau; }

  //Specify the latitude/longitude of the receiving site
  inline void setSite(float lon, float lat) {
    site.c1 = lon;
    site.c2 = lat;
  }
  inline void setSite(pair_t loc) {
    site = loc;
  }

  //Load the radiometer data having the specified filename
  //channel = 1 or 2 depending which receiver you wish to use
  bool loadRadiometer(string &fname, int channel, float noise);

  //Load the reference data set having the specified filename
  //channel = 1 or 2 depending which receiver you wish to use
  bool loadReference(string &fname, int channel, float noise);

  //Specify one or more ranges of sidereal time where we should calibrate
  //the radiometer data against the reference data. ie consecutive array
  //entires are treated as pairs that together specify a time range
  void setReferenceCalTimes(timegen_t *times, int length);

  //Calibrate the radiometer data to the reference data
  void calibrateReference();

  //Display the reference and radiometer data sets
  void displayRadiometer(int plotarea);

  //Get the absorption measurements for the given radiometer/reference data
  void getAbsorption();

  //Display the absorption data set
  void displayAbsorption(int plotarea);

  //Calculate the solar elevation for each absorption measurement
  void getElevation();

  //Display the solar zenith angle data
  void displayElevation(int plotarea);

  //Correct the absorption for the solar angle using a Chapman function
  void correctChapman();

  //Correct the absorption for the solar angle using a simple model
  void correctSimple();

  //Display the solar radiation cross section data
  void displayCrossSection(int plotarea);

  //Display the corrected data set
  void displayCorrected(int plotarea);

  //Load the 1-minute GOES/SPIDR data with the specified filename
  bool loadGOES(string &fname);

  //Load the 3-second GOES data with the specified filename
  bool loadGOES_3s(string &fname);

  //Display the GOES X-ray data on the given PlotArea
  void displayGOES(int plotarea);

  //Specify the time ranges during that we should use to calibrate the corrected
  //absorption dat against the GOES data. ie consecutive array
  //entires are treated as pairs that together specify a time range
  void setGOESCalTimes(timegen_t *times, int length);

  //Calibrate the absorption data to the GOES data
  void calibrateGOES();

  //Use the specified parameters of fit to create a model of the X-rays
  //Length of returned data is the same as the number of absorption points
  float *applyParameters(float n, float a);

  //Assess how good the given fit is to the GOES data
  //The array order and timestamps correspond to the absorption data
  float assessFit(float *model);

  //Display the given fit
  void displayFit(int plotarea);
  //Display the given fit and the GOES data
  void displayFitAndGOES(int plotarea);

private:
  //Determine the time offset, tau, by searching for best correlation
  //The best number is also added to the absorption timestamps
  timegen_t determineTau();
  //Get the best value for the scalar alpha for the given dataset
  float getAlpha(float *model);
  //Use specified value of n and also apply cross-section
  float *applyN(float n);

  //Make mappings between GOES and absorption timestamps
  void makeMappings();

  pair_t site; //Observing site location

  double internal_noise; //The amount of internal receiver noise

  int num_radiometer; //Number of radiometer data points
  float *radiometer; //The radiometer flux measurements
  timegen_t *radiometer_abs; //Absolute timestamps for radiometer data
  timegen_t *radiometer_lst; //Sidereal timestamps for radiometer data
  timegen_t *radiometer_ut;  //"UT" timestamps for radiometer data

  int num_reference; //Number of points in the reference dataset
  float *reference; //The reference flux measurements "quiet day curve"
  timegen_t *reference_lst; //Sidereal timestamps for reference data

  int num_reference_cals; //Number of time period to use for calibration
  timegen_t *reference_cals; //Sidereal time periods to use to calibrate
                             //the radiometer data to the reference data.
                             //start-stop pairs, length=2*num_reference_cals

  int num_absorption; //Number of absorption measurement points
  float *absorption; //The absorption measurements
  timegen_t *absorption_abs; //Absolute timestamps for absorption measurements
  timegen_t *absorption_lst; //Sidereal timestamps for absorption measurements
  timegen_t *absorption_ut;  //"UT" timestamps for absorption data

  float *solar_elevation;  //Solar elevation for each absorption point
  float *cross_section; //Solar radiation cross-section for absorption points
  float *corrected; //Absorption points corrected for solar angle

  int num_goes; //Number of satellite flux measurements
  float *goes; //GOES X-ray measurements
  timegen_t *goes_abs;  //Absolute timestamps to go with GOES data
  timegen_t *goes_ut;   //UT-like timestamps for the GOES data
  int num_goes_cals;    //Number of time period to use for calibration
  timegen_t *goes_cals; //Absolute time periods to use to calibrate
                        //the absorption data to the GOES data.
                        //In start-stop pairs, length=2*num_goes_cals

  int *nearest_mapping; //Mapping of the nearest GOES data to each absorption
  int *matched_mapping; //Mapping of the optimal time mapping

  double n; //Itkina's "n" parameter
  double alpha; //Itkina's "a" parameter
  timegen_t tau; //Our "tau" parameter

  float *final_output; //Absorption points mapped to X-ray flux
};

#endif
