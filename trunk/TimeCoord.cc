//
// Copyright (C) David Brodrick
// Copyright (C) CSIRO Australia Telescope National Facility
//
// Some algorithms are based on Jean Meeus "Astronomical Formulae for
// Calculators", Fourth ed. Wim Brouw of the CSIRO Australia Telescope
// National Facility originally wrote the "refract" function.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.
//
// $Id: TimeCoord.cc,v 1.3 2004/03/23 09:51:56 brodo Exp brodo $

#include <TimeCoord.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <math.h>

pthread_mutex_t _time_lock_ = PTHREAD_MUTEX_INITIALIZER;

//Read time in the format YYYY-mm-DD HH:MM[:SS[.S]]
timegen_t parseDate(string indate)
{
  struct tm tm;
  memset(&tm, '\0', sizeof (tm));
  strptime(indate.c_str(), "%F %H:%M", &tm);
  timegen_t bat = mktime(&tm) - timezone;
  bat*=1000000;
  return bat;
}


//Read time in the format HH:MM or HH.decimal
timegen_t parseTime(string arg)
{
  timegen_t bat = 0;
  istringstream indate(arg);

  if ((signed int)arg.find(":")!=-1) {
    char temp;
    int hour, min;

    indate >> hour;
    if (indate.fail() || hour<0 || hour>23) return -1;
    bat = 3600*hour;
    indate >> temp;
    indate >> min;
    if (min<0 || min>59) return -1;
    bat += 60*min;
    bat*=1000000;
  } else {
    float temp = 0.0;
    indate >> temp;
    if (indate.fail() || temp<0.0) return -1;
    bat = (timegen_t)(temp*3600000000.0);
  }

  return bat;
}


//Get the Julian date for the specified time
//Based on Jean Meeus "Astronomical Formulae for Calculators", Fourth ed.
double Abs2JD(timeAbs_t a)
{
  time_t t = (time_t)(a/1000000); //From usecs to secs

  pthread_mutex_lock(&_time_lock_);
  struct tm *gm = gmtime(&t);

  long year = gm->tm_year + 1900;
  long month = gm->tm_mon + 1;
  if (month<=2) {
    year--;
    month+=12;
  }

  //Assume Gregorian calendar
  long A = (long)(year/100);
  long B = 2 - A + (long)(A/4);

  double julian_day = (long)(365.25*year) + (long)(30.6001*(month+1))
                      + gm->tm_mday + 1720994.5 + B;
  julian_day += gm->tm_hour/24.0 + gm->tm_min/1440.0 + gm->tm_sec/86400.0;
  pthread_mutex_unlock(&_time_lock_);
  return julian_day;
}

//Get the Julian date for the specified *DAY*
//Based on Jean Meeus "Astronomical Formulae for Calculators", Fourth ed.
double Abs2JD0(timeAbs_t a)
{
  time_t t = (time_t)(a/1000000); //From usecs to secs

  pthread_mutex_lock(&_time_lock_);
  struct tm *gm = gmtime(&t);

  long year = gm->tm_year + 1900;
  long month = gm->tm_mon + 1;
  if (month<=2) {
    year--;
    month+=12;
  }

  //Assume Gregorian calendar
  long A = (long)(year/100);
  long B = 2 - A + (long)(A/4);

  double julian_day = (long)(365.25*year) + (long)(30.6001*(month+1))
                      + gm->tm_mday + 1720994.5 + B;
  pthread_mutex_unlock(&_time_lock_);
  return julian_day;
}


//Convert a HA/Dec pair and Location to Azimuth and Elevation pair
pair_t Eq2Hor(pair_t hadec, pair_t loc)
{
  //cout << "HA=" << 12*hadec.c1/PI << "\t";
  //precalculate some values
  double slat = sin(loc.c2);
  double clat = cos(loc.c2);
  double sdec = sin(hadec.c2);
  double cdec = cos(hadec.c2);
  double cha  = cos(hadec.c1);
  double a = sdec*slat + cdec*clat*cha;
  //Calculate the El position
  pair_t res;
  res.c2 = asin(a);
  //This bother deals with finite math at transit
  double d = (sdec-slat*a)/(clat*cos(res.c2));
  if (d<-1.0) d = -1.0; if (d>1.0) d = 1.0;
  //Get the Az position
  res.c1 = acos(d);
  //Ensure output in right quadrant and range
  if (hadec.c1>0.0) res.c1 = 2*PI - res.c1;
  if (res.c1<0.0) res.c1 = 2*PI + res.c1;
  return res;
}


//Account for atmospheric refraction (adjusts elevation)
//This code taken from ATOMS file ACCPointing.cc by Wim Brouw, CSIRO,
//which is also licensed under GPL.
angle_t refract(angle_t el)
{
  //approximation since atmospheric conditions are unknown
  const double P = 1020.0; //pressure
  const double T = 290.0;  //temperature
  const double e = 8.0;    //humidity

  if (el>0.0) {
    double N = 1 - (7.8e-5 * P + 0.39 * e/T)/T;
    double R_p = (1-N*N)/2*N*N;
    el = el + R_p/tan(el);
  }
  return el;
}


//Print the time in a readable format
string printTime(timegen_t arg)
{
  ostringstream out;
  if (arg<8640000000000ll) {
    //double tstamp = static_cast<double>(arg);
    int day,hrs,min;
    double sec;
    day =  arg/86400000000ll;
    if (day==1) out << "1 day ";
    else if (day!=0) out << day << " days ";
    arg -= day*86400000000ll;
    hrs =  arg/3600000000ll;
    if (hrs<10) out << "0";
    out << hrs <<":";
    arg -= hrs*3600000000ll;
    min =  arg/60000000ll;
    if (min<10) out << "0";
    out << min <<":";
    arg -= min*60000000ll;
    sec = arg/1000000.0;
    if (sec<10.0) out << "0";
    out << sec << endl;
  } else {
    time_t tstamp  = arg/1000000;
    pthread_mutex_lock(&_time_lock_);
    struct tm *utc = gmtime(&tstamp);
    out << asctime(utc);
    pthread_mutex_unlock(&_time_lock_);
  }
  return out.str();
}


//Print an RA/Dec position into a string
string printRADec(pair_t radec)
{
  ostringstream out;
  out << 12.0*radec.c1/PI << "h " << 180.0*radec.c2/PI << "d";
  return out.str();
}


//Print an HA/Dec position into a string
string printHADec(pair_t hadec)
{
  ostringstream out;
  out << 12.0*hadec.c1/PI << "HA " << 180.0*hadec.c2/PI << "d";
  return out.str();
}


//Print an Az/El position into a string
string printAzEl(pair_t azel)
{
  ostringstream out;
  out << "Az=" << 180.0*azel.c1/PI << ", El=" << 180.0*azel.c2/PI;
  return out.str();
}


//Print a longitude and latitude into a string
string printLongLat(pair_t latlong)
{
  ostringstream out;
  out << 180.0*latlong.c1/PI << " " << (latlong.c1>0.0?"E ":"W ");
  out << 180.0*latlong.c2/PI << " " << (latlong.c2>=0.0?"N":"S");
  return out.str();
}


//Get the RA/Dec of the Sun at the given instant
//Based on Jean Meeus "Astronomical Formulae for Calculators", Fourth ed.
//Realised too late all constants were in degrees so there's a few compile
//time conversions from degrees to radians... D'oh!
pair_t solarPosition(timeAbs_t t)
{
  //Get "T" in Meeus
  double T = Abs2T(t);

  //Calculate geometric mean longitude of the Sun
  double L = fmod(279.69668 + 36000.76892*T + 0.0003025*T*T, 360.0);
  //Calculate mean anomaly
  double M = fmod(358.47583 + 35999.04975*T - 0.000150*T*T - 0.0000033*T*T*T, 360.0);
  //Calculate the Sun's equation of centre
  double c = (1.919460 - 0.004789*T - 0.000014*T*T)*sin(M*(2*PI/360.0))
           + (0.020094 - 0.000100*T)*sin(2*M*(2*PI/360.0))
           +  0.000293*sin(3*M*(2*PI/360.0));
  //Calculate the true longitude of the Sun
  double longTrue = L + c;
  //Get apparrent position by approximating nutation and aberration
  double omega = (259.18 - 1934.142*T)*(2*PI/360.0);
  double longApp = (2*PI/360.0)*(longTrue - 0.00569 - 0.00479*sin(omega));
  //Get obliquity of the ecliptic
  double e = (2*PI/360.0)*(23.452294 - 0.0130125*T - 0.00000164*T*T + 0.000000503*T*T*T);
  e += 0.00004468*cos(omega);
  //Finally we can calculate the RA/Dec position of the Sun
  pair_t res;
  res.c1 = atan2(cos(e)*sin(longApp), cos(longApp));
  res.c2 = asin(sin(e)*sin(longApp));
  return res;
}


//Get the decimated phase for given source angle, baseline, frequency
//and phase offset
angle_t phaseResponse(pair_t azel, pair_t bsln, double freq, double Pi)
{
  //Get the delay between when a wavefront reaches the elements
  double delay = geometricDelay(azel, bsln);
  //Get the period of one cycle at the given frequency
  double period = 1.0/freq;
  //Get the delay as a fraction of a wavelength
  double fracwave = fmod(delay, period)/period;
  //Get it as a phase
  double phase = 2*PI*fracwave;
  //Apply the instrumental phase offset
  phase+=Pi;
  //Get it as a phase between PI and -PI
  phase = phase>=PI?(-2*PI+phase):phase;
  phase = phase<=-PI?(2*PI+phase):phase;
  return phase;
}


//Convert baseline from East-West, North-South form to X,Y,Z system
//Thanks to Maxim A. Voronkov for help here
vec3d_t Ground2XYZ(pair_t bsln, pair_t loc)
{
  //Precalculate some values
  double cl = cos(loc.c1);
  double sl = sin(loc.c1);

  vec3d_t res;
  res.x = bsln.c1*cl - bsln.c2*sl;
  res.y = bsln.c1*sl + bsln.c2*cl;
  res.z = 0.0;
  return res;
}


//Get u,v,w for the given XYZ baseline and source Hour Angle / Dec
vec3d_t getuvw(vec3d_t bsln, pair_t hadec, double freq)
{
  //Precalculate some quantities
  double cha  = cos(hadec.c1);
  double sha  = sin(hadec.c1);
  double cdec = cos(hadec.c2);
  double sdec = sin(hadec.c2);
  //From "Synthesis Imaging in Radio Astronomy II",
  //Chapter 2, by Richard Thompson.
  vec3d_t res;
  res.x = sha*bsln.x + cha*bsln.y;
  res.y = -sdec*cha*bsln.x + sdec*sha*bsln.y + cdec*bsln.z;
  res.z = cdec*cha*bsln.x - cdec*sha*bsln.y + sdec*bsln.z;
  //u,v plane coordinates are measured in wavelengths
  double lambda = C/freq;
  res.x/=lambda;
  res.y/=lambda;
  res.z/=lambda;
  return res;
}

/*int main() {
  timeAbs_t now=getAbs();
  pair_t radec = solarPosition(now);
  pair_t pos={PI*150/180.0, PI*-30/180.0};
  timeLST_t lst = Abs2LST(now, pos);
  pair_t azel = Eq2Hor(lst, radec, pos);
  cout << printAzEl(azel) << endl;
}*/
