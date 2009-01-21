//
// Copyright (C) David Brodrick
// Copyright (C) CSIRO Australia Telescope National Facility
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.
//
// $Id: Antenna.h,v 1.1 2004/05/03 09:34:52 brodo Exp brodo $

//This class encapsulates the beam pattern of a receiving antenna.
//Methods may be overridden by polymorphic derivations of this class.

//At the moment the following antennas are approximated to some extent:
//Antenna: Isotropic antenna
//JoveNSArray: Two North-South oriented dipoles one half wave apart

#ifndef _ANTENNA_HDR_
#define _ANTENNA_HDR_

#include <TimeCoord.h>

class Antenna {
public:
  virtual ~Antenna();

  //Return the gain of the antenna in the given Az/El direction
  virtual double getGain(timegen_t time, pair_t azel);
};


class JoveNSArray: public Antenna {
public:
  virtual ~JoveNSArray();
  //Return the gain of the antenna in the given Az/El direction
  virtual double getGain(timegen_t time, pair_t azel);
};

#endif
