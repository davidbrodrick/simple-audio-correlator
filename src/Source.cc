//
// Copyright (C) David Brodrick
// Copyright (C) CSIRO Australia Telescope National Facility
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.
//
// $Id: $

#include <Source.h>
#include <iostream>
#include <sstream>

Source::~Source() { }
AstroPointSource::~AstroPointSource() { }
ExtendedSource::~ExtendedSource() { }

//Constructor with astro position and source flux
AstroPointSource::AstroPointSource(pair_t radec, double flux)
  :itsFlux(flux),
  itsPosition(radec)
{

}


//Return the Az/El of the source at the given time and location.
//NOTE: The time may be either an absolute or LST value!
pair_t AstroPointSource::getAzEl(timegen_t lst, pair_t location)
{
  //If time is absolute we need to calculate the sidereal time
  if (lst>86400000000ll) {
    lst = Abs2LST(lst, location);
  }
  return Eq2Hor(lst, itsPosition, location);
}


void AstroPointSource::getFootprint(vec3d_t *&resp, int &resplen,
				    pair_t site, timegen_t time, float res)
{
  //Being a point source, we only ever occupy one pixel
  resplen = 1;
  resp = new vec3d_t[1];

  pair_t azel = getAzEl(time, site);
  resp[0].x = azel.c1;
  resp[0].y = azel.c2;
  resp[0].z = itsFlux;
}


//Parse a source from a string or return NULL
Source *Source::parseSource(string &str)
{
  istringstream l(str);
  //Pull out the RA, Dec, and flux
  pair_t radec;
  double flux;
  l >> radec.c1 >> radec.c2 >> flux;
  //Check the parse did not fail
  //Check the RA was parsed correctly
  //Check the declination is properly understood
  if (l.fail() ||
      radec.c1<0.0 || radec.c1>24.0 ||
      radec.c2<-90.0 || radec.c1>90.0) {
    cerr << "\nPARSE ERROR reading \"" << str << "\":\n";
    cerr << "Expect: \"RA Dec Flux\" one source per line\n";
    cerr << "RA is in decimal hours, between 0.0 and 24.0\n";
    cerr << "Dec is in decimal degrees, -90.0 to 90.0 degrees, North positive\n";
    cerr << "Flux in Jansky's or whatever units you're using\n\n";
    cerr << "eg,\n";
    cerr << "13.4 -45.2 28000\n\n";
    return NULL;
  }
  //Do unit conversions to radians
  radec.c1=PI*radec.c1/12.0;
  radec.c2=PI*radec.c2/180.0;

  //See if we are able to extract a size, maybe it's an extended source
  double size;
  l >> size;
  if (l.fail()) {
    return new AstroPointSource(radec, flux);
  } else if (size<0 || size>180) {
    cerr << "\nPARSE ERROR reading the size of the source\n";
    cerr << "arg 4 in \"" << str << "\"\n";
    cerr << "should be a radius in degrees (0 to 180)\n\n";
    return NULL;
  } else {
    return new ExtendedSource(radec, flux, PI*size/180.0);
  }
}


ExtendedSource::ExtendedSource(pair_t radec, double flux, double size)
:AstroPointSource(radec, flux),
itsRadius(size)
{
}


void ExtendedSource::getFootprint(vec3d_t *&resp, int &resplen,
				  pair_t site, timegen_t time, float res)
{
  resplen = 0;
  resp = NULL;
  //Check if we're even above the horizon at the site
  pair_t azel = getAzEl(time, site);
  if (azel.c2+itsRadius < 0.0) return;

  //Work out the size of the bounding box
  int numpix = 1 + 2*(int)(itsRadius/res);
  //if (numpix<1) numpix = 1;
  int midpixel = numpix/2;
  resplen = numpix*numpix;

  //Allocate the response array
  resp = new vec3d_t[resplen];

  //Pretend we are a square for now
  for (int i=0; i<numpix; i++) {
    float ra = itsPosition.c1 + (i-midpixel)*res;
    for (int j=0; j<numpix; j++) {
      float dec = itsPosition.c2 + (j-midpixel)*res;
      float radist = itsPosition.c1-ra;
      float decdist = itsPosition.c2 - dec;
      if (sqrt(radist*radist + decdist*decdist)>itsRadius) continue;
      pair_t radec = {ra, dec};
      pair_t azel = Eq2Hor(time, radec, site);
      resp[i*numpix+j].x = azel.c1;
      resp[i*numpix+j].y = azel.c2;
      resp[i*numpix+j].z = 1.2*itsFlux/resplen;
    }
  }
}

