//
// Copyright (C) David Brodrick
// Copyright (C) CSIRO Australia Telescope National Facility
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.
//
// $Id: RFI.h,v 1.6 2004/11/15 08:41:34 brodo Exp brodo $

#ifndef _RFI_HDR_
#define _RFI_HDR_

#include <TimeCoord.h>
class IntegPeriod;

//Some hacked together functions from previous code.
//This doesn't contain any great insights into RFI mitigation, rather
//it is a loose collection of some poorly implemented statistical
//processing functions..

//"autoclean" the 'data' argument. Size of 'res' is held in 'rescount'
void cleanData(IntegPeriod *&res, int &rescount,
	       IntegPeriod *data, int count);

//Get average float value over 'len' indices, starting from 'start'
float getAvg(float *v, int start, int len);

//Get standard deviation over 'len' floats starting from 'start'
float getSD(float mean, float *v, int start, int len);

//Flag any data more than 'sigma' standard deviations from the mean
//as calculated over a length of 'len'. If 'sdnazi' is set to true
//any data with a local standard deviation greater
void markOutliers(IntegPeriod *data, int size, float sigma,
		  int len, bool sdnazi=false);

//Flag data more than 'cutoff' times the local mean
void powerClean(IntegPeriod *data, int size, float cutoff, int len);

//Find mapping y=mx+b between datasets and set m and b
void regression(float &m, float &b,
		float *x, timegen_t *xtimes, int xlen,
                float *y, timegen_t *ytimes, int ylen);

int nearest(int i, timegen_t *a, int alen,
	    timegen_t *b, int blen, timegen_t cutoff=0);

int matchTimes(int i, timegen_t *a, int alen,
	       timegen_t *b, int blen);

//Find closest matches between a and b and return new datasets in ref's
//The caller needs to delete the pointers when done.
void matchTimes(float *adata, timegen_t *atimes, int alen,
		float *bdata, timegen_t *btimes, int blen,
		float *&ares, float *&bres,
		timegen_t *&restime, int &reslen);

#endif
