//
// Copyright (C) David Brodrick
// Copyright (C) CSIRO Australia Telescope National Facility
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.
//
// $Id: $

#include <Site.h>
#include <iostream>
#include <sstream>
#include <math.h>

////////////////////////////////////////////////////////////////////////
Site::Site(pair_t location, pair_t bsln, double freq, double phase)
:itsSite(location),
itsBaseline(bsln),
itsFreq(freq),
itsPhaseOffset(phase),
itsAnt1(new JoveNSArray()),
//itsAnt1(new Antenna()),
itsAnt2(new JoveNSArray())
{
  //Get the baseline in XYZ coordinates
  itsXYZ = Ground2XYZ(itsBaseline, itsSite);
}


//Compute the system response to the given source. The response to the source
//will be calculated at the times specified by the data' timestamps.
//NOTE: We add the response to the existing values of the data so you will
//need to ensure you initialise it appropriately..
void Site::getResponse(IntegPeriod *data, int datalen, Source &src)
{
  for (int i=0; i<datalen; i++) {
    vec3d_t *footprint = NULL;
    int footprintlen = 0;

    //Get the current az/el footprint of the source
    src.getFootprint(footprint, footprintlen,
		     itsSite, data[i].timeStamp, PI*1.0/180.0);
    if (footprint==NULL || footprintlen==0) continue;

    //process each az/el pixel individually
    for (int j=0; j<footprintlen; j++) {
      //don't process set sources
      if (footprint[j].y<0.0 || footprint[j].z==0.0) continue;

      pair_t azel = {footprint[j].x, footprint[j].y};
      getResponse(data[i], azel, footprint[j].z);
    }

    delete[] footprint;
  }
}


void Site::getResponse(IntegPeriod &data, pair_t azel, float flux)
{
  //Get the antenna responses for the calculated position
  float ant1 = itsAnt1->getGain(data.timeStamp, azel);
  float ant2 = itsAnt2->getGain(data.timeStamp, azel);
  //If both antennas have zero response there's nothing further to do
  if (ant1==0.0 && ant2==0.0) return;
  //I'm not sure if this is the right way to combine the antenna gains
  float antX = ant1*ant2;
  //Get the interferometer phase response to the source
  float phase = getPhaseResponse(azel);
  //Add all the numbers to the existing data values
  data.power1 += flux*ant1; //Would be easy to model bandwidth here too
  data.power2 += flux*ant2;
  //Need complex addition for the visibilities
  double x = data.amplitude*cos(data.phase) + flux*antX*cos(phase);
  double y = data.amplitude*sin(data.phase) + flux*antX*sin(phase);
  data.amplitude = sqrt(x*x + y*y);
  data.phase = atan2(y, x);
  data.powerX = data.amplitude*cos(data.phase);
}


void _siteparseerror() {
  cerr << "longitude must be in degrees, East positive, between -180 and 180\n";
  cerr << "To define a system you need:\n";
  cerr << "\tlongitude latitude baselineEW baselineNS frequency phaseoffset\n";
  cerr << "longitude must be in degrees, East positive, between -180 and 180\n";
  cerr << "latitude must be in degrees, North positive, between -90 and 90\n";
  cerr << "East-West baseline component in metres\n";
  cerr << "North-South baseline component in metres\n";
  cerr << "Frequency in MHz\n";
  cerr << "Phase offset in degrees, between -180 and 180\n";
}


//Parse a Site from a string or return NULL
Site *Site::parseSite(string &str)
{
  istringstream l(str);
  //First read the system location
  pair_t loc;
  l >> loc.c1;
  if (l.fail() || loc.c1>180.0 || loc.c1<-180.0) {
    cerr << "PARSE ERROR reading \"" << str << "\"\n";
    cerr << "The longitude argument was not understood\n";
    _siteparseerror();
    return NULL;
  }
  loc.c1 = PI*loc.c1/180.0;
  //Read the latitude
  l >> loc.c2;
  if (l.fail() || loc.c2>90.0 || loc.c2<-90.0) {
    cerr << "PARSE ERROR reading \"" << str << "\"\n";
    cerr << "The latitude argument was not understood\n";
    _siteparseerror();
    return NULL;
  }
  loc.c2 = PI*loc.c2/180.0;
  //Next is meant to come the E-W part of the baseline
  pair_t bsln;
  l >> bsln.c1;
  if (l.fail() || bsln.c1<-10000.0 || bsln.c1>10000.0) {
    cerr << "PARSE ERROR reading \"" << str << "\"\n";
    cerr << "The East-West baseline argument was not understood\n";
    _siteparseerror();
    return NULL;
  }
  //Next is meant to come the N-S part of the baseline
  l >> bsln.c2;
  if (l.fail() || bsln.c2<-10000.0 || bsln.c2>10000.0) {
    cerr << "PARSE ERROR reading \"" << str << "\"\n";
    cerr << "The North-South baseline argument was not understood\n";
    _siteparseerror();
    return NULL;
  }
  //The frequency is next
  double freq;
  l >> freq;
  if (l.fail() || freq<=0.0 || freq>35000.0) {
    cerr << "PARSE ERROR reading \"" << str << "\"\n";
    cerr << "Frequency argument not understood. Must be in MHz\n";
    _siteparseerror();
    return NULL;
  }
  freq*=1000000.0; //Convert to cycles per second
  //The phase offset is next
  double offset;
  l >> offset;
  if (l.fail() || offset>180.0 || offset<-180.0) {
    cerr << "PARSE ERROR reading \"" << str << "\"\n";
    cerr << "The phase offset argument was not understood\n";
    _siteparseerror();
    return NULL;
  }
  offset = PI*offset/180.0;
  return new Site(loc, bsln, freq, offset);
}

