//
// Copyright (C) David Brodrick
// Copyright (C) CSIRO Australia Telescope National Facility
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.
//
// $Id: $

//Represent a source. A source has a particular flux, angular size, shape
//and a location in space. The parent class is abstract, the idea is to
//implement polymorphic sub-classes, eg for astronomical radio blobs,
//satellites, etc.

//Actually, at the minute there is no shape. Only point sources.

#ifndef _SOURCE_HDR_
#define _SOURCE_HDR_

#include <TimeCoord.h>

//////////////////////////////////////////////////////////////////////
class Source {
public:
  //Virtual destructor
  virtual ~Source();

  virtual void getFootprint(vec3d_t *&resp, int &resplen,
			    pair_t site, timegen_t time,
			    float res) = 0;

  //Return the flux of the source at the specified time
  //NOTE: The time may be either an absolute or LST value!
  virtual double getFlux(timegen_t time) = 0;

  //Return the Az/El of the source at the given time and location.
  //NOTE: The time may be either an absolute or LST value!
  virtual pair_t getAzEl(timegen_t time, pair_t location) = 0;

  //Parse a Source from a string or return NULL
  static Source *parseSource(string &str);
};


//////////////////////////////////////////////////////////////////////
//A simple astronomical point source
class AstroPointSource : public Source {
protected:
  //The flux of the source
  double itsFlux;
  //The RA/Dec position of the source
  pair_t itsPosition;
  //The catalogue ID of the source
  string itsID;
  //The name of the source
  string itsName;
  //Any notes about the source
  string itsNotes;

public:
  //Virtual destructor
  virtual ~AstroPointSource();

  //Constructor with astro position and source flux
  AstroPointSource(pair_t radec, double flux);

  //Return the RA/Dec position of the source
  pair_t getPosition() {return itsPosition;}

  //Return the flux of the source at the specified time
  //NOTE: The time may be either an absolute or LST value!
  virtual inline double getFlux(timegen_t time=0) {return itsFlux;}
  virtual inline void setFlux(float flux) {itsFlux = flux;}
  //Return the Az/El of the source at the given time and location.
  //NOTE: The time may be either an absolute or LST value!
  virtual pair_t getAzEl(timegen_t time, pair_t location);

  virtual void getFootprint(vec3d_t *&resp, int &resplen,
			    pair_t site, timegen_t time,
			    float res);
};


//////////////////////////////////////////////////////////////////////
//An extended astronomical source
class ExtendedSource : public AstroPointSource {
protected:
  angle_t itsRadius;

public:
  //Virtual destructor
  virtual ~ExtendedSource();

  //Constructor with astro position and source flux
  ExtendedSource(pair_t radec, double flux, double size);

  virtual void getFootprint(vec3d_t *&resp, int &resplen,
			    pair_t site, timegen_t time, float res);

  virtual inline double getRadius() { return itsRadius; }
  virtual inline void setRadius(double size) { itsRadius = size; }

  //void getBoundingBox(int &ramax, int &ramin, int &decmax, int &decmin,
  //      	      double ramin, double ramax, int ranum,
  //      	      double decmin, double decmax, int decnum);

};

#endif
