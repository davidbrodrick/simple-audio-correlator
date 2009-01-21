//
// Copyright (C) David Brodrick
// Copyright (C) CSIRO Australia Telescope National Facility
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.
//
// $Id: $

#ifndef _SITE_HDR_
#define _SITE_HDR_

//using namespace::std;

#include <TimeCoord.h>
#include <Source.h>
#include <Antenna.h>
#include <IntegPeriod.h>

class Site {
private:
  //The latitude and longitude of the receiver site in radians.
  pair_t itsSite;
  //The East-West (.c1) North-South (.c2) components of the baseline in
  //metres with East being positive and North being positive. The Vector
  //should point to "antenna 2" from an "antenna 1" origin.
  pair_t itsBaseline;
  //The X,Y,Z components of the baseline
  vec3d_t itsXYZ;
  //Observing frequency, in Hertz
  double itsFreq;
  //The instrumental phase offset. This quantity will be zero for a
  //properly phase-calibrated meridian transit interferometer but a delay
  //(eg length of coaxial cable) can be inserted to point the interferometer
  //fringe pattern on the sky.
  angle_t itsPhaseOffset;
  //An antenna gain model for each antenna
  Antenna *itsAnt1, *itsAnt2;

  void getResponse(IntegPeriod &data, pair_t azel, float flux);

public:
  Site(pair_t location, pair_t bsln, double freq, double phase=0.0);

  //Return the baseline vector. res.c1 contains the East-West component of
  //the baseline, res.c2 contains the North-South component. Both numbers in
  //metres with East being positive and North being positive. The Vector
  //should point to "antenna 2" from an "antenna 1" origin.
  inline pair_t getBaseline() {return itsBaseline;}
  //Return the baseline in X,Y,Z coordinates
  inline vec3d_t getBaselineXYZ() {return itsXYZ;}
  //Return u,v,w coordinate for specified LST and reference position
  inline vec3d_t uvwCoord(timeLST_t t, pair_t radec)
  {return getuvw(getBaselineXYZ(), RA2HA(t, radec), itsFreq);}
  //These return the latitude/longitude/both of the receiving site.
  //Quantities are in radians, East is positive, North is positive.
  inline pair_t getSite() {return itsSite;}
  inline double getSiteLong() {return itsSite.c1;}
  inline double getSiteLat()  {return itsSite.c2;}

  //Get the observing frequency
  inline double getFreq()  {return itsFreq;}

  //Return the instrumental phase offset, in radians.
  inline double getPhaseOffset() {return itsPhaseOffset;}
  //Specify the instrumental phase offset to use, in radians.
  inline void setPhaseOffset(double phase) {itsPhaseOffset = phase;}

  //Return the gain pattern for the given antenna
  inline Antenna* getAnt1() {return itsAnt1;}
  inline Antenna* getAnt2() {return itsAnt2;}
  //Zero gets you antenna 1, otherwise you get antenna 2
  inline Antenna* getAnt(int num)
  {if (num==0) return itsAnt1; else return itsAnt2;}

  //Return the system phase response to a source at the specified
  //az/el position
  inline angle_t getPhaseResponse(pair_t azel)
  { return phaseResponse(azel, itsBaseline, itsFreq, itsPhaseOffset); }

  //Compute the system response to the given source. The response to the source
  //will be calculated at the times specified by the data' timestamps.
  //NOTE: We add the response to the existing values of the data so you will
  //need to ensure you initialise it appropriately..
  void getResponse(IntegPeriod *data, int datalen, Source &src);

  //Parse a Site from a string or return NULL
  static Site *parseSite(string &str);
};

#endif
