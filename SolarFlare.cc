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

#include <SolarFlare.h>
#include <IntegPeriod.h>
#include <PlotArea.h>
#include <RFI.h>
#include <iostream>
#include <math.h>
#include <cstdlib>

extern "C" {
  double atm8_chapman__(double *X, double *chi0);
}

/////////////////////////////////////////////////////////////////
//Print a measure of X-ray flux in the standard notation
void printFlux(float flux)
{
  cout.precision(3);
  if (flux>1e-4) {
    cout << "X" << (flux/1e-4);
  } else if (flux>1e-5) {
    cout << "M" << (flux/1e-5);
  } else if (flux>1e-6) {
    cout << "C" << (flux/1e-6);
  } else {
    cout << flux;
  }
  cout.precision(7);
}


/////////////////////////////////////////////////////////////////
//Check if the timestamp falls within one of the ranges
static bool inRange(timegen_t t, timegen_t *ranges, int length)
{
  for (int r=0; r<length; r+=2) {
    if (ranges[r]<=t && ranges[r+1]>=t) return true;
    //cout << printTime(t) << " is outside range " << printTime(ranges[r]) << " " << printTime(ranges[r+1]) << endl;
  }
  return false;
}


/////////////////////////////////////////////////////////////////
//Read time in the format HH:MM[:SS[.S]]
/*static timegen_t parseDate(istringstream &indate)
{
  timegen_t bat = 0;
  timegen_t bat2 = 0;
  struct tm brktm;
  char temp;

  int year, month, day, hour, min;

  timezone = 0;

  indate >> year;
  brktm.tm_year = year-1900;
  bat2+=(year-1970)*31556926000000ll)
  indate >> temp;
  if (temp!='-') return -1;

  indate >> month;
  brktm.tm_mon = month-1;


  indate >> temp;
  if (temp!='-') return -1;

  indate >> day;
  brktm.tm_mday = day;

  indate >> hour;
  brktm.tm_hour = hour;

  indate >> temp;
  if (temp!=':') return -1;

  indate >> min;
  brktm.tm_min = min;

  brktm.tm_sec = 0;

  bat = mktime(&brktm);
  bat*=1000000;

  return bat;
}
*/

///////////////////////////////////////////////////////////////////////////
//Load the radiometer data having the specified filename
bool SolarFlare::loadRadiometer(string &fname, int channel, float noise)
{
  IntegPeriod *data = NULL;
  int numdata = 0;

  if (!IntegPeriod::load(data, numdata, fname.c_str()))
    return false;

  num_radiometer = numdata;
  radiometer = new float[num_radiometer];
  radiometer_abs = new timegen_t[num_radiometer];
  radiometer_lst = new timegen_t[num_radiometer];
  radiometer_ut  = new timegen_t[num_radiometer];

  if (channel==1) {
    for (int i=0; i<numdata; i++) radiometer[i] = data[i].power1 - noise;
  } else {
    for (int i=0; i<numdata; i++) radiometer[i] = data[i].power2 - noise;
  }

  timegen_t tmin = 0;
  for (int i=0; i<numdata; i++) {
    radiometer_abs[i] = data[i].timeStamp;
    radiometer_lst[i] = Abs2LST(data[i].timeStamp, site);
    //Check if this is the earliest timestamp
    if (radiometer_abs[i]<tmin || tmin==0) tmin=radiometer_abs[i];
  }

  //Now that we know the minimum, make up a UT like timestamp
  for (int i=0; i<numdata; i++) {
    radiometer_ut[i] = Abs2UT(radiometer_abs[i], tmin);
  }
  return true;
}


///////////////////////////////////////////////////////////////////////////
//Load the reference data set having the specified filename
bool SolarFlare::loadReference(string &fname, int channel, float noise)
{
  IntegPeriod *data = NULL;
  int numdata = 0;

  if (!IntegPeriod::load(data, numdata, fname.c_str()))
    return false;

  num_reference = numdata;
  reference = new float[num_reference];
  reference_lst = new timegen_t[num_reference];

  if (channel==1) {
    for (int i=0; i<numdata; i++) reference[i] = data[i].power1 - noise;
  } else {
    for (int i=0; i<numdata; i++) reference[i] = data[i].power2 - noise;
  }

  for (int i=0; i<numdata; i++) reference_lst[i] = data[i].timeStamp;

  return true;

}


///////////////////////////////////////////////////////////////////////////
void SolarFlare::setReferenceCalTimes(timegen_t *times, int length)
{
  num_reference_cals = length;
  reference_cals = new timegen_t[num_reference_cals];
  for (int i=0; i<num_reference_cals; i++) reference_cals[i] = times[i];
}


///////////////////////////////////////////////////////////////////////////
void SolarFlare::setGOESCalTimes(timegen_t *times, int length)
{
  num_goes_cals = length;
  goes_cals = new timegen_t[num_goes_cals];
  for (int i=0; i<num_goes_cals; i++) goes_cals[i] = times[i];
}


///////////////////////////////////////////////////////////////////////////
//Calibrate the radiometer data to the reference data
void SolarFlare::calibrateReference()
{
  double ratio = 0.0;
  int count = 0;
  for (int rad=0; rad<num_radiometer; rad++) {
    //Don't bother matching if it is outside the time range
    if (!inRange(radiometer_lst[rad], reference_cals, num_reference_cals))
      continue;
    //Get the nearest reference point to this radiometer point
    int ref = matchTimes(rad, radiometer_lst, num_radiometer,
			 reference_lst, num_reference);
    //No suitable match
    if (ref==-1 || !inRange(reference_lst[ref], reference_cals, num_reference_cals))
      continue;
    //cout << "MATCHED " << printTime(radiometer_lst[rad]) << " with " << printTime(reference_lst[ref]) << endl;

    //We can use these points
    ratio += radiometer[rad]/reference[ref];
    count++;
  }
  if (count==0) {
    cerr << "SolarFlare:calibrateReference: No points could be used for calibration!\n";
  } else {
    ratio /= count;
    for (int i=0; i<num_radiometer; i++) radiometer[i]/=ratio;
  }
}


///////////////////////////////////////////////////////////////////////////
//Display the reference and radiometer data sets
void SolarFlare::displayRadiometer(int plotarea)
{
  float radtimes[num_radiometer];
  float reftimes[num_reference];
  float pmax = 0.0;
  float tmin = 24.0;
  float tmax = 0.0;

  for (int i=0; i<num_radiometer; i++) {
    radtimes[i] = radiometer_lst[i]/3600000000.0;
    if (radtimes[i]>tmax) tmax=radtimes[i];
    if (radtimes[i]<tmin) tmin=radtimes[i];
    if (radiometer[i]>pmax) pmax=radiometer[i];
  }
  for (int i=0; i<num_reference; i++) {
    reftimes[i] = reference_lst[i]/3600000000.0;
    if (reference[i]>pmax&&reftimes[i]>=tmin&&reftimes[i]<=tmax)
      pmax=reference[i];
  }

  PlotArea *x = PlotArea::getPlotArea(plotarea);
  x->clear();
  x->setTitle("Radiometer and Reference Data");
  x->setAxisY("Power", pmax + pmax/10.0, 0.0, false);
  x->setAxisX("Local Sidereal Time (Hours)", tmax, tmin, false);

  x->plotPoints(num_radiometer, radtimes, radiometer, 9);
  x->plotPoints(num_reference,  reftimes, reference,  4);
}


///////////////////////////////////////////////////////////////////////////
//Get the absorption measurements for the given radiometer/reference data
void SolarFlare::getAbsorption()
{
  absorption = new float[num_radiometer]; //might be less but not more
  absorption_abs = new timegen_t[num_radiometer];
  absorption_lst = new timegen_t[num_radiometer];
  absorption_ut  = new timegen_t[num_radiometer];
  int count = 0;

  for (int rad=0; rad<num_radiometer; rad++) {
    //Get the nearest reference point to this radiometer point
    int ref = nearest(rad, radiometer_lst, num_radiometer,
		      reference_lst, num_reference, 120000000);
    if (ref==-1) continue; //No nearest match

    //Measure the absorption caused by the flare
    absorption[count] = 10*log(reference[ref]/radiometer[rad]);
    absorption_lst[count] = radiometer_lst[rad];
    absorption_abs[count] = radiometer_abs[rad];
    absorption_ut[count]  = radiometer_ut[rad];

    count++;
  }
  num_absorption = count;
}


///////////////////////////////////////////////////////////////////////////
//Display the absorption data set
void SolarFlare::displayAbsorption(int plotarea)
{
  float times[num_absorption];
  float amax = 0.0;
  float tmin = 24.0;
  float tmax = 0.0;

  for (int i=0; i<num_absorption; i++) {
    times[i] = Abs2UT(absorption_abs[i])/3600000000.0;
    if (times[i]>tmax) tmax=times[i];
    if (times[i]<tmin) tmin=times[i];
    if (absorption[i]>amax) amax=absorption[i];
  }

  PlotArea *x = PlotArea::getPlotArea(plotarea);
  x->clear();
  x->setTitle("Ionospheric Absorption");
  x->setAxisY("Absorption (dB)", amax + amax/10.0, 0.0, false);
  x->setAxisX("Universal Time (Hours)", tmax, tmin, false);

  x->plotPoints(num_absorption, times, absorption, 9);
}


///////////////////////////////////////////////////////////////////////////
//Display the corrected absorption data set
void SolarFlare::displayCorrected(int plotarea)
{
  float times[num_absorption];
  float amax = 0.0;
  float tmin = 24.0;
  float tmax = 0.0;

  for (int i=0; i<num_absorption; i++) {
    times[i] = Abs2UT(absorption_abs[i])/3600000000.0;
    if (times[i]>tmax) tmax=times[i];
    if (times[i]<tmin) tmin=times[i];
    if (corrected[i]>amax) amax=corrected[i];
  }

  PlotArea *x = PlotArea::getPlotArea(plotarea);
  x->clear();
  x->setTitle("Corrected Absorption");
  x->setAxisY("Absorption (dB)", amax + amax/10.0, 0.0, false);
  x->setAxisX("Universal Time (Hours)", tmax, tmin, false);

  x->plotPoints(num_absorption, times, corrected, 9);
}


///////////////////////////////////////////////////////////////////////////
//Calculate the solar elevation for each absorption measurement
void SolarFlare::getElevation()
{
  solar_elevation = new float[num_absorption];

  for (int i=0; i<num_absorption; i++) {
    //Get the RA/Dec of the sun at this time
    pair_t radec = solarPosition(absorption_abs[i]);
    //Get the Az/El of the sun from our site at the time
    pair_t azel = Eq2Hor(absorption_lst[i], radec, site);
    //Save the elevation of the sun at the site at this time
    solar_elevation[i] = azel.c2;
  }
}


///////////////////////////////////////////////////////////////////////////
//Display the solar elevation
void SolarFlare::displayElevation(int plotarea)
{
  float times[num_absorption];
  float tmin = 24.0;
  float tmax = 0.0;
  float indegrees[num_absorption];

  for (int i=0; i<num_absorption; i++) {
    indegrees[i] = 180.0*solar_elevation[i]/PI;
    times[i] = Abs2UT(absorption_abs[i])/3600000000.0;
    if (times[i]>tmax) tmax=times[i];
    if (times[i]<tmin) tmin=times[i];
  }

  PlotArea *x = PlotArea::getPlotArea(plotarea);
  x->clear();
  x->setTitle("Solar Elevation");
  x->setAxisY("Elevation (degrees)", 90.0, 0.0, false);
  x->setAxisX("Universal Time (Hours)", tmax, tmin, false);

  x->plotPoints(num_absorption, times, indegrees, 9);
}


///////////////////////////////////////////////////////////////////////////
//Correct the absorption for the solar angle using a Chapman function
void SolarFlare::correctChapman()
{
  double X = 1000.0;
  //double X = 200.0;

  cross_section = new float[num_absorption];
  corrected = new float[num_absorption];

  for (int i=0; i<num_absorption; i++) {
    if (solar_elevation[i]>0.0) {
      double zenith_angle = 90.0 - solar_elevation[i]*57.2957795130823208768;
      cross_section[i] = atm8_chapman__(&X, &zenith_angle);
    } else {
      cross_section[i] = 0.0;
    }
    corrected[i] = absorption[i]*cross_section[i];
  }
}


///////////////////////////////////////////////////////////////////////////
//Correct the absorption for the solar angle using a simple model
void SolarFlare::correctSimple()
{
  cross_section = new float[num_absorption];
  corrected = new float[num_absorption];

  for (int i=0; i<num_absorption; i++) {
    if (solar_elevation[i]>0.0)
      cross_section[i] = 1.0/sin(solar_elevation[i]);
    else
      cross_section[i] = 0.0;

    corrected[i] = absorption[i]*cross_section[i];
  }
}


///////////////////////////////////////////////////////////////////////////
//Display the solar radiation cross-section
void SolarFlare::displayCrossSection(int plotarea)
{
  float times[num_absorption];
  float tmin = 24.0;
  float tmax = 0.0;
  float amax = 0.0;

  for (int i=0; i<num_absorption; i++) {
    times[i] = Abs2UT(absorption_abs[i])/3600000000.0;
    if (times[i]>tmax) tmax=times[i];
    if (times[i]<tmin) tmin=times[i];
    if (cross_section[i]>amax) amax=corrected[i];
  }

  PlotArea *x = PlotArea::getPlotArea(plotarea);
  x->clear();
  x->setTitle("Solar Radiation Cross-section");
  x->setAxisY("Cross-section", PI/2.0, 0.0, false);
  x->setAxisX("Universal Time (Hours)", tmax, tmin, false);

  x->plotPoints(num_absorption, times, cross_section, 9);
}


///////////////////////////////////////////////////////////////////////////
//Load the 1-minute GOES data with the specified filename
bool SolarFlare::loadGOES(string &fname)
{
  ifstream tfile(fname.c_str(), ios::in);
  if (!tfile.good()) {
    cerr << "ERROR opening " << fname << endl;
    return false;
  }

  const int magicsize = 200000; //Dumb - a guess at max file length
  goes     = new float[magicsize];
  goes_abs = new timegen_t[magicsize];
  goes_ut  = new timegen_t[magicsize];

  timegen_t tmin = 0;
  int i = 0;
  while (tfile.good()) {
    if (i==magicsize) {
      cerr << "\nERROR: TOO MANY LINES IN GOES DATA FILE\n\n";
      exit(1);
    }
    char line[1001];
    line[1000] = '\0';
    tfile.getline(line, 1000);
    if (line[0]=='#' || line[0]==':' || !tfile.good()) continue;
    string lstr(line);
    timegen_t tdate = parseDate(lstr.substr(0, 16));
    if (tdate==-1) continue;
    //cout << lstr.substr(0, 16) << "\t" << printTime(tdate);
    goes_abs[i] = tdate;
    istringstream l(lstr.substr(17,7));
    //string foo;
    l >> goes[i];
    //cout << lstr.substr(17,7) << "\t" << goes[i] << endl;
    if (isnan(goes[i])) { cerr << "XRAY_WAS_NAN.."; continue; }
    if (fabs(goes[i])>1) continue;
    if (goes_abs[i]<tmin || tmin==0) tmin=goes_abs[i];
    i++;
  }
  num_goes = i;
  tfile.close();

  //Then calculate the UT-like timestamps
  for (int i=0; i<num_goes; i++) {
    goes_ut[i] = Abs2UT(goes_abs[i], tmin);
  }

  makeMappings();

  return true;
}


///////////////////////////////////////////////////////////////////////////
//Load the 3-second GOES data with the specified filename
bool SolarFlare::loadGOES_3s(string &fname)
{
  ifstream tfile(fname.c_str(), ios::in);
  if (!tfile.good()) {
    cerr << "ERROR opening " << fname << endl;
    return false;
  }

  const int magicsize = 200000; //Dumb - a guess at max file length
  goes     = new float[magicsize];
  goes_abs = new timegen_t[magicsize];

  //__timezone can't help us here?
  signed int dumbquestion = 100;
  while (true) {
    cout << "I need to know YOUR machines timezone offset at the time of the flare.\n";
    cout << "eg, \"10\" or \"11\" for Narrabri, \"0\" for Greenwich\n";
    cin >> dumbquestion;
    if (dumbquestion<14 && dumbquestion>-14) break;
  }

  int i = 0;
  while (tfile.good()) {
    if (i==magicsize) {
      cerr << "\nERROR: TOO MANY LINES IN GOES DATA FILE\n\n";
      exit(1);
    }
    char line[1001];
    line[1000] = '\0';
    tfile.getline(line, 1000);
    if (line[0]=='#' || line[0]==':' || !tfile.good()) continue;
    istringstream l(line);

    struct tm brktm;
    brktm.tm_min = 0;
    brktm.tm_mday = 0;
    brktm.tm_hour = 0;
    brktm.tm_mon = 0;
    brktm.tm_wday = 0;

    char temp;
    int year, day, sec;

    string foo;
    l>>foo;
    l>>year;
    l>>temp;
    brktm.tm_year = year-1900;

    l>>day;
    l>>temp;
    l>>sec;
    l>>temp;

    timegen_t tdate = mktime(&brktm);
    tdate+=86400*day + sec;
    tdate*=1000000;
    if (tdate==-1) continue;
    tdate += dumbquestion*3600000000ll;

    goes_abs[i] = tdate;
    l >> goes[i]; //But it also has the other wavelength band???!!

    if (isnan(goes[i])) { cerr << "XRAY_WAS_NAN.."; continue; }
    if (fabs(goes[i])>1) continue;
    i++;
  }
  num_goes = i;
  tfile.close();

  makeMappings();

  return true;
}


///////////////////////////////////////////////////////////////////////////
//Display the GOES X-ray data on the given PlotArea
void SolarFlare::displayGOES(int plotarea)
{
  float times[num_goes];
  float xmax = 0.0;
  float tmin = 0.0;
  float tmax = 0.0;
  bool first = true;

  //Set the time scale the same as the absorption graph
  for (int i=0; i<num_absorption; i++) {
    float thistime = absorption_ut[i]/3600000000.0;
    if (thistime>tmax || first) tmax=thistime;
    if (thistime<tmin || first) tmin=thistime;
    first = false;
  }

  for (int i=0; i<num_goes; i++) {
    times[i] = goes_ut[i]/3600000000.0;
    if (goes[i]>xmax) xmax=goes[i];
  }

  PlotArea *x = PlotArea::getPlotArea(plotarea);
  x->clear();
  x->setTitle("GOES X-ray Flux");
  x->setAxisY("Flux (mW/m2)", xmax + xmax/10.0, 0.0, false);
  x->setAxisX("Universal Time (Hours)", tmax, tmin, false);

  x->plotLine(num_goes, times, goes, 5);
}


///////////////////////////////////////////////////////////////////////////
//Assess how good the given fit is to the GOES data
//The array order and timestamps correspond to the absorption data
float SolarFlare::assessFit(float *model)
{
  double sum = 0.0;
  int count = 0;
  for (int a=0; a<num_absorption; a++) {
    if (isnan(model[a])) continue;
    //Don't bother matching if it is outside the time range
    if (!inRange(absorption_ut[a], goes_cals, num_goes_cals)) continue;
    //Get the nearest reference point to this radiometer point
    int ref = nearest_mapping[a];
    //No suitable match
    if (ref==-1||!inRange(goes_ut[ref], goes_cals, num_goes_cals)) continue;

    //We can use these points
    float e = model[a]-goes[ref];
    sum += e*e;
    count++;
  }
  if (count==0) {
    cerr << "SolarFlare:calibrateGOES: No points could be used for calibration!\n";
    return 0;
  } else {
    return sqrt(sum/count);
  }
}


///////////////////////////////////////////////////////////////////////////
//Use the specified parameters of fit to create a model of the X-rays
//Length of returned data is the same as the number of absorption points
float *SolarFlare::applyParameters(float n, float a)
{
  float *res = new float[num_absorption];
  for (int i=0; i<num_absorption; i++) {
    res[i] = a*(pow(absorption[i], n)*cross_section[i]);
    //old way
    //res[i] = a*pow(absorption[i]*cross_section[i], n);
  }
  return res;
}


///////////////////////////////////////////////////////////////////////////
//Determine the time offset, tau, by searching for best correlation
timegen_t SolarFlare::determineTau()
{
  timegen_t thistau = -180000000;
  timegen_t besttau = 0;
  double bestcorr = 0.0;
  bool first = true;

  while (thistau<=180000000) {
    //Need to measure the correlation for this time offset
    timegen_t thesetimes[num_absorption];
    for (int i=0; i<num_absorption; i++)
      thesetimes[i] = absorption_ut[i]+thistau;
    double thiscorr = 0.0;
    double asum = 0.0;
    double gsum = 0.0;
    int count = 0;
    for (int a=0; a<num_absorption; a++) {
      if (!isnan(absorption[a]) && absorption[a]>0
	  && inRange(thesetimes[a], goes_cals, num_goes_cals)) {
	//Get the nearest reference point to this radiometer point
	int ref = nearest(a, thesetimes, num_absorption,
			  goes_ut, num_goes, 31000000);
	//Check if there was a suitable match
	if (ref!=-1 && inRange(goes_ut[ref], goes_cals, num_goes_cals)) {
	  asum += absorption[a];
	  gsum += goes[ref];
	  //double t1 = absorption[a]-goes[ref];
	  thiscorr += absorption[a]*goes[ref]; //t1*t1;
	  count++;
	}
      }
    }
    if (count!=0) {
      //cout << thiscorr <<"\t"<< count <<"\t"<< asum <<"\t"<< gsum;
      //thiscorr = (thiscorr/count)/((asum/count)*(gsum/count));
      thiscorr /= count;
      //thiscorr = sqrt(thiscorr/count);
      //cout << "\t" << thiscorr << endl;
      if (thiscorr>bestcorr || first) {
	//cout << thiscorr << "\t" << printTime(thistau);
	besttau = thistau;
        bestcorr = thiscorr;
	first = false;
      }
    }
    thistau+=5000000;
  }

  tau = besttau;
  for (int a=0; a<num_absorption; a++) {
    absorption_lst[a]+=tau;
    absorption_abs[a]+=tau;
    absorption_ut[a]+=tau;
  }
  return besttau;
}


///////////////////////////////////////////////////////////////////////////
//Use specified value of n and also apply cross-section
float *SolarFlare::applyN(float n)
{
  float *res = new float[num_absorption];
  for (int i=0; i<num_absorption; i++) {
    //res[i] = std::pow(absorption[i], n)*cross_section[i];
    //old way?
    res[i] = pow(absorption[i]*cross_section[i], n);
  }
  return res;
}


///////////////////////////////////////////////////////////////////////////
//Calibrate the absorption data to the GOES data
void SolarFlare::calibrateGOES()
{
  determineTau();

  float bestRMS = 0.0;
  bool first = true;
  for (double thisn=0.3; thisn<1.0; thisn+=0.01) {
    //Try this value of N and also correct for location of the sun
    float *thisfit = applyN(1.0/thisn);
    //Get the scaling parameter and apply it
    float thisalpha = getAlpha(thisfit);
    for (int i=0; i<num_absorption; i++) thisfit[i]/=thisalpha;
    //Test how good this fit is
    float thisRMS = assessFit(thisfit);
    if (thisRMS<bestRMS || first) {
      n = thisn;
      alpha = thisalpha;
      bestRMS = thisRMS;
      if (final_output!=NULL) delete[] final_output;
      final_output = thisfit;
      first = false;
    } else {
      delete[] thisfit;
    }
  }
}


///////////////////////////////////////////////////////////////////////////
//Get the best value for the scalar alpha for the given dataset
float SolarFlare::getAlpha(float *model)
{
  double ratio = 0.0;
  int count = 0;
  for (int a=0; a<num_absorption; a++) {
    if (isnan(model[a])) continue;
    //Don't bother matching if it is outside the time range
    if (!inRange(absorption_ut[a], goes_cals, num_goes_cals)) continue;
    //Get the nearest GOES point to this absorption point
    int ref = nearest_mapping[a];
    //No suitable match
    if (ref==-1||!inRange(goes_ut[ref], goes_cals, num_goes_cals)) continue;

    //We can use these points
    ratio += model[a]/goes[ref];
    count++;
  }
  if (count==0) {
    cerr << "SolarFlare:getAlpha: No points could be used for calibration!\n";
    return 0.0;
  } else {
    //cout << ratio << " " << count << endl;
    return ratio/count;
  }
}


///////////////////////////////////////////////////////////////////////////
//Display the fit
void SolarFlare::displayFit(int plotarea)
{
  float times[num_absorption];
  float xmax = 0.0;
  float tmin = 0.0;
  float tmax = 0.0;
  bool first = true;

  //Set the time scale the same as the absorption graph
  for (int i=0; i<num_absorption; i++) {
    float thistime = absorption_ut[i]/3600000000.0;
    times[i] = thistime;
    if (thistime>tmax || first) tmax=thistime;
    if (thistime<tmin || first) tmin=thistime;
    if (final_output[i]>xmax) xmax=final_output[i];
    first = false;
  }

  PlotArea *x = PlotArea::getPlotArea(plotarea);
  x->clear();
  x->setTitle("Estimated X-ray Flux");
  x->setAxisY("Flux (mW/m2)", xmax + xmax/10.0, 0.0, false);
  x->setAxisX("Universal Time (Hours)", tmax, tmin, false);

  x->plotPoints(num_absorption, times, final_output, 9);
}


///////////////////////////////////////////////////////////////////////////
//Display the fit and GOES data together
void SolarFlare::displayFitAndGOES(int plotarea)
{
  float ftimes[num_absorption];
  float gtimes[num_goes];
  float xmax = 0.0;
  float tmin = 0.0;
  float tmax = 0.0;
  bool first = true;

  //Set the time scale the same as the absorption graph
  for (int i=0; i<num_absorption; i++) {
    float thistime = absorption_ut[i]/3600000000.0;
    ftimes[i] = thistime;
    if (thistime>tmax || first) tmax=thistime;
    if (thistime<tmin || first) tmin=thistime;
    if (final_output[i]>xmax) xmax=final_output[i];
    first = false;
  }

  for (int i=0; i<num_goes; i++) {
    gtimes[i] = goes_ut[i]/3600000000.0;
    if (goes[i]>xmax) xmax = goes[i];
  }

  PlotArea *x = PlotArea::getPlotArea(plotarea);
  x->clear();
  x->setTitle("Estimated and GOES X-ray Fluxes");
  x->setAxisY("Flux (mW/m2)", xmax + xmax/10.0, 0.0, false);
  x->setAxisX("Universal Time (Hours)", tmax, tmin, false);
  x->plotLine(num_goes, gtimes, goes, 5);
  x->plotPoints(num_absorption, ftimes, final_output, 9);
}


///////////////////////////////////////////////////////////////////////////
//Make mappings between GOES and absorption timestamps
void SolarFlare::makeMappings()
{
  nearest_mapping = new int[num_absorption];
  matched_mapping = new int[num_absorption];

  for (int a=0; a<num_absorption; a++) {
    //Get the nearest reference point to this radiometer point
    nearest_mapping[a] = nearest(a, absorption_ut, num_absorption, goes_ut, num_goes, 90000000);
    matched_mapping[a] = matchTimes(a, absorption_ut, num_absorption, goes_ut, num_goes);
  }
}
