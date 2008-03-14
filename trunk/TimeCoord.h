//
// Copyright (C) David Brodrick
// Copyright (C) CSIRO Australia Telescope National Facility
// Copyright (C) Swinburne University, Australia
//
// Some algorithms are based on Jean Meeus "Astronomical Formulae for
// Calculators", Fourth ed. Wim Brouw of the CSIRO Australia Telescope
// National Facility originally wrote the "refract" function.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.
//
// $Id: TimeCoord.h,v 1.4 2004/03/23 09:50:47 brodo Exp brodo $
//

#ifndef _TIMECOORD_HDR_
#define _TIMECOORD_HDR_

#include <string>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>

using namespace::std;

//This file contains definitions for some useful routines to manipulate
//time and spatial coordinate quantities.

//mutex on system time library calls since these use static output buffers
extern pthread_mutex_t _time_lock_;
//Some nice name-clashy constant definitions
const double PI = 3.141592654;
const double C  = 299792458.0;
const int DUTC = 32;

//Generic time
typedef signed long long timegen_t;
//Absolute time (Abs) (microseconds since 00:00:00 UTC, January 1, 1970)
typedef timegen_t timeAbs_t;
//Local Sidereal Time (LST) (microseconds since 00:00 LST)
typedef timegen_t timeLST_t;

//An angle in radians in radians in radians in radians in radians
typedef double angle_t;
//A pair of angles (in whatever coordinate system)
typedef struct pair_t {
  angle_t c1;
  angle_t c2;
} pair_t;
//An X,Y,Z vector (in whatever coordinate system)
typedef struct vec3d_t {
  double x;
  double y;
  double z;
} vec3d_t;


//Read time in the format YYYY-mm-DD HH:MM[:SS[.S]]
///Has this silly problem where it parses as local time though
timegen_t parseDate(string indate);
//Read time in the format HH:MM or HH.decimal
timegen_t parseTime(string arg);

//Get the current time, microseconds since 00:00:00 UTC, January 1, 1970
inline timeAbs_t getAbs()
{
  struct timeval tv = {0,0};
  timeAbs_t res;
  pthread_mutex_lock(&_time_lock_);
  //Get current time from the system
  gettimeofday(&tv, 0);
  //Convert to 64 bit
  res = 1000000*(unsigned long long)(tv.tv_sec) + tv.tv_usec;
  pthread_mutex_unlock(&_time_lock_);
  return res;
}

//Get the Julian date for the specified time
double Abs2JD(timeAbs_t t);
//Get the Julian date for the specified DAY
double Abs2JD0(timeAbs_t t);

//Get the current Julian time - for convenience
inline double getJD()
{
  timeAbs_t t = getAbs();
  return Abs2JD(t);
}

//Get number of Julian centuries since 1900. "T" in Meeus.
inline double JD2T(double JD) {
  return (JD-2415020.0)/36525.0;

}

//Get number of Julian centuries since 1900. "T" in Meeus.
inline double Abs2T(timeAbs_t t) {
  return JD2T(Abs2JD(t));
}

//Get number of Julian centuries since 1900 for the DAY. Also "T" in Meeus.
inline double Abs2T0(timeAbs_t t) {
  return JD2T(Abs2JD0(t));
}

//Mean sidereal time at Greenwich at 0 hours UT
inline timeLST_t T2GMST0(double T) {
  double GMST0 = fmod(6.6460656 + 2400.051262*T + 0.00002581*T*T, 24.0);
  return (long long)(3600000000.0*GMST0); //To usecs
}

//Mean sidereal time at Greenwich at 0 hours UT
inline timeLST_t Abs2GMST0(timeAbs_t t)
{ return T2GMST0(Abs2T0(t)); }

//Sidereal time at Greenwich at nominated instant
inline timeLST_t Abs2GMST(timeAbs_t t)
{ return Abs2GMST0(t) + (timeLST_t)(1.002737908*(t%86400000000ll)); }

//Convert Absolute time and Longitude to Local Mean Sidereal Time (LMST)
inline timeLST_t Abs2LST(timeAbs_t tm, angle_t loc_long)
{
  timeLST_t t = Abs2GMST(tm);
  t += (timeLST_t)((86400000000.0)*(loc_long/(2*PI)));
  t = t%86400000000ll;
  return t;
}
//Convert Absolute time and Location to Local Mean Sidereal Time (LMST)
inline timeLST_t Abs2LST(timeAbs_t tm, pair_t loc)
{ return Abs2LST(tm, loc.c1); }

//Convert the absolute time into a time-of-day in UT
inline timegen_t Abs2UT(timegen_t abs)
{ return abs%86400000000ll - DUTC; }
//To UT but continue past 24:00 if its more than a day since "ref"
inline timegen_t Abs2UT(timegen_t abs, timegen_t ref)
{ timegen_t ut = abs%86400000000ll - DUTC;
  return ut + 86400000000ll*((abs-ref)/86400000000ll);
}

//Get the current time as LST, given a location (longitude)
inline timeLST_t getLST(angle_t loc_long)
{ timeAbs_t bat = getAbs(); return Abs2LST(bat, loc_long); }
//Get the current time as LST, given a location
inline timeLST_t getLST(pair_t loc)
{ timeAbs_t bat = getAbs(); return Abs2LST(bat, loc); }

//Print the time in a readable format
string printTime(timegen_t arg);
//Print an RA/Dec position into a string
string printRADec(pair_t radec);
//Print an HA/Dec position into a string
string printHADec(pair_t hadec);
//Print an Az/El position into a string
string printAzEl(pair_t azel);
//Print a longitude and latitude into a string
string printLongLat(pair_t latlong);

//Convert LST and a RA to HA
inline angle_t RA2HA(timeLST_t lst, angle_t ra)
{ return fmod(2*PI*lst/86400000000.0 - ra, 2*PI); }
//Convert LST and a RA/Dec pair to HA/Dec pair
inline pair_t RA2HA(timeLST_t lst, pair_t radec)
{ angle_t ha = 2*PI*lst/86400000000.0 - radec.c1;
  if (ha<-PI) ha=2*PI+ha;
  if (ha>PI) ha=-2*PI+ha;
  //ha = fmod(ha, 2*PI);
  pair_t res;
  res.c1 = ha;
  res.c2 = radec.c2;
  return res;
}

//Convert LST and an HA to RA
//(taking advantage of sweet natural symmetry [and method overloading :-)])
inline pair_t  HA2RA(timeLST_t n, pair_t m)  {return RA2HA(n,m);}
inline angle_t HA2RA(timeLST_t n, angle_t m) {return RA2HA(n,m);}

//Convert a HA/Dec pair and Location to Azimuth/Elevation pair
pair_t Eq2Hor(pair_t hadec, pair_t loc);
//Convert a RA/Dec pair, LST and Location to Azimuth/Elevation pair
inline pair_t Eq2Hor(timeLST_t lst, pair_t radec, pair_t loc)
{
  pair_t res; pair_t hadec;
  hadec = RA2HA(lst, radec); res = Eq2Hor(hadec, loc);
  return res;
}

//Convert an Azimuth/Elevation pair and Location to a HA/Dec pair
inline pair_t Hor2Eq(pair_t azel, pair_t loc)
{ pair_t hadec = Eq2Hor(azel,loc);
  return hadec;
}
//Convert an Azimuth/Elevation pair, LST and Location to RA/Dec pair
inline pair_t  Hor2Eq(timeLST_t lst, pair_t azel, pair_t loc)
{ pair_t hadec = Hor2Eq(azel,loc); return HA2RA(lst, hadec); }

//Account (approximately) for atmospheric correction (adjusts el up)
angle_t refract(angle_t el);
inline pair_t refract(pair_t azel)
{ pair_t r={azel.c1,0};double el=refract(azel.c2);r.c2=el;return r; }

//Get the RA/Dec of the Sun at the given instant
pair_t solarPosition(timeAbs_t t);

//Get the geometric delay for given details, in seconds
//Baseline.c1 is EW distance in metres, .c2 is NS distance
inline double geometricDelay(pair_t azel, pair_t baseline)
{return cos(azel.c2)*(baseline.c2*cos(azel.c1) + baseline.c1*sin(azel.c1))/C;}
//Get the decimated phase for given source angle, baseline, frequency and
//instrumental phase offset
angle_t phaseResponse(pair_t azel, pair_t bsln, double freq, double Pi=0.0);

//Convert baseline from East-West/North-South form to X,Y,Z system
vec3d_t Ground2XYZ(pair_t bsln, pair_t loc);
//Convert baseline from East-West/North-South form to X,Y,Z system
inline vec3d_t Ground2XYZ(vec3d_t bsln, pair_t loc)
{pair_t b={bsln.x,bsln.y}; vec3d_t res=Ground2XYZ(b,loc);
res.z=bsln.z; return res;}

//Get u,v,w for the given XYZ baseline, source HA/Dec and central frequency
vec3d_t getuvw(vec3d_t bsln, pair_t hadec, double freq);

#endif //_TIMECOORD_HDR_
