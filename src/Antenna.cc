//
// Copyright (C) David Brodrick, 2002
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.
//
// $Id: $

#include <Antenna.h>

///////////////////////////////////////////////////////////////////////
//Empty destructor
Antenna::~Antenna() {};
JoveNSArray::~JoveNSArray() {};

///////////////////////////////////////////////////////////////////////
//Return the gain of some dodgy antenna
double Antenna::getGain(timegen_t time, pair_t azel)
{
  //If the source is set, we cannot see it
  if (azel.c2<0.0) return 0.0;
  else return 1.41*sin(azel.c2);
}


///////////////////////////////////////////////////////////////////////
//Return the gain of a Jove phased-array with N-S oriented dipoles
double JoveNSArray::getGain(timegen_t time, pair_t azel)
{
  //If the source is set, we cannot see it
  if (azel.c2<0.0) return 0.0;

  //Get the projection of each dipole
  //double gain = 1.0 - fabs(cos(azel.c2)*cos(azel.c1));
  double gain = sin(azel.c2) - fabs(cos(azel.c2)*cos(azel.c1));
  //Account for sine of elevation because the light is more spread out?
  gain *= sin(azel.c2);
  //Don't go negative
  if (gain<0.0) gain = 0.0;

  //Root two gain factor for having two dipoles
  return 1.41*gain;
}

