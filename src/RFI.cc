//
// Copyright (C) David Brodrick
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.
//
// $Id: RFI.cc,v 1.8 2004/11/15 11:19:36 brodo Exp brodo $

//Some dubious functions for identifying RFI peturbed data and doing other
//statistical processing.

#include <RFI.h>
#include <IntegPeriod.h>
#include <math.h>
#include <assert.h>


/////////////////////////////////////////////////////////////////
//Applies an automated cleaning suite to the specified data
void cleanData(IntegPeriod *&res, int &rescount,
	       IntegPeriod *data, int count)
{
  if (data==NULL || count==0) return;

  res = NULL;
  rescount = 0;
  IntegPeriod *tempdata = NULL, *tempdata2 = NULL;
  int tempcount = 0, tempcount2 = 0;

  markOutliers(data, count, 2.0, 10);
  IntegPeriod::purgeFlagged(tempdata, tempcount, data, count);
  if (tempdata==NULL || tempcount==0) return;


  powerClean(tempdata, tempcount, 1.15, 10);
  IntegPeriod::purgeFlagged(tempdata2, tempcount2, tempdata, tempcount);
  delete[] tempdata;
  if (tempdata2==NULL || tempcount2==0) return;

  //Perform second pass
  markOutliers(tempdata2, tempcount2, 5.0, 40, false);
  //IntegPeriod::purgeFlagged(res, rescount, tempdata2, tempcount2);
  IntegPeriod::integrate(res, rescount, tempdata2, tempcount2, 10000000);
  delete[] tempdata2;
}


/////////////////////////////////////////////////////////////////
//Get average over 'len' floats from 'start'
float getAvg(float *v, int start, int len)
{
    assert(len>0);

    int end = start+len;
    double mean = 0.0;

    for (int i=start; i<end; i++) {
	mean+=v[i];
    }
    mean = mean/(float)len;

    return mean;
}


/////////////////////////////////////////////////////////////////
//Return standard deviation over 'len' floats from 'start'
float getSD(float mean, float *v, int start, int len)
{
    assert(len>0);

    //calculate variance
    double sd = 0.0;
    int endblock = start+len;
    for (int pnt=start; pnt<endblock; pnt++) {
	double temp=v[pnt] - mean;
	sd+=temp*temp;
    }
    sd/=(float)len;
    //standard dev
    sd=sqrt(sd);

    return sd;
}


/////////////////////////////////////////////////////////////////
//Flag outlayers and maybe any noisy periods
void markOutliers(IntegPeriod *data, int size, float sigma,
		  int len, bool sdnazi)
{
  if (len>=size) {
    //Don't bother flagging any if there's not much data
    return;
  }
  int halflen = len/2;
  double tempmax, mean, sd;
  int end = size-halflen;
  int count = 0;
  float magic = 20;

  float ref[size];

  //Look at the first input channel
  for (int i=0; i<size; i++) ref[i] = data[i].power1;
  //Process the leading data
  mean = getAvg(ref, 0, len);
  sd   = getSD(mean, ref, 0, len);
  tempmax = mean+sigma*sd;
  for (int i=0; i<halflen; i++) {
    if (!data[i].RFI && (ref[i]>tempmax || (sdnazi && sd>mean/magic))) {
      data[i].RFI = true;
      count++;
    }
  }
  //Process the bulk middle section of the samples
  for (int i=halflen; i<end; i++) {
    //Calculate stddev and mean for data centered on this point
    mean = getAvg(ref, i-halflen, len);
    sd   = getSD(mean, ref, i-halflen, len);
    tempmax = mean+sigma*sd;

    if (!data[i].RFI && (ref[i]>tempmax || (sdnazi && sd>mean/magic))) {
      data[i].RFI = true;
      count++;
    }
  }
  //And use that last value for the remaining data
  for (int i=end; i<size; i++) {
    if (!data[i].RFI && (ref[i]>tempmax || (sdnazi && sd>mean/magic))) {
      data[i].RFI = true;
      count++;
    }
  }

  //Look at the second input channel
  for (int i=0; i<size; i++) ref[i] = data[i].power2;
  //Process the leading data
  mean = getAvg(ref, 0, len);
  sd   = getSD(mean, ref, 0, len);
  tempmax = mean+sigma*sd;
  for (int i=0; i<halflen; i++) {
    if (!data[i].RFI && (ref[i]>tempmax || (sdnazi && sd>mean/magic))) {
      data[i].RFI = true;
      count++;
    }
  }
  //Process the bulk middle section of the samples
  for (int i=halflen; i<end; i++) {
    //Calculate stddev and mean for data centered on this point
    mean = getAvg(ref, i-halflen, len);
    sd   = getSD(mean, ref, i-halflen, len);
    tempmax = mean+sigma*sd;

    if (!data[i].RFI && (ref[i]>tempmax || (sdnazi && sd>mean/magic))) {
      if (!data[i].RFI) {
	data[i].RFI = true;
	count++;
      }
    }
  }
  //And use that last value for the remaining data
  for (int i=end; i<size; i++) {
    if (!data[i].RFI && (ref[i]>tempmax || (sdnazi && sd>mean/magic))) {
      data[i].RFI = true;
      count++;
    }
  }

  if (count>0) cerr << "Removed " << count << " of " << size << " periods possibly contaminated by RFI\n";
}


/////////////////////////////////////////////////////////////////
//Flag data more than 'cutoff' times the local mean
void powerClean(IntegPeriod *data, int size, float cutoff, int len)
{
  if (len>=size) {
    //Don't bother flagging any if there's not much data
    return;
  }
  int halflen = len/2;
  double mean;
  int end = size-halflen;
  int count = 0;

  float ref[size];

  //Look at the first input channel
  for (int i=0; i<size; i++) ref[i] = data[i].power1;
  //Process the leading data
  mean = getAvg(ref, 0, len);
  for (int i=0; i<halflen; i++) {
    if (ref[i]/mean>cutoff) {
      data[i].RFI = true;
      count++;
    }
  }
  //Process the bulk middle section of the samples
  for (int i=halflen; i<end; i++) {
    //Calculate stddev and mean for data centered on this point
    mean = getAvg(ref, i-halflen, len);
    if (ref[i]/mean>cutoff) {
      data[i].RFI = true;
      count++;
    }
  }
  //And use that last value for the remaining data
  for (int i=end; i<size; i++) {
    if (ref[i]/mean>cutoff) {
      data[i].RFI = true;
      count++;
    }
  }

  //Look at the second input channel
  for (int i=0; i<size; i++) ref[i] = data[i].power2;
  //Process the leading data
  mean = getAvg(ref, 0, len);
  for (int i=0; i<halflen; i++) {
    if (ref[i]/mean>cutoff && !data[i].RFI) {
      data[i].RFI = true;
      count++;
    }
  }
  //Process the bulk middle section of the samples
  for (int i=halflen; i<end; i++) {
    //Calculate stddev and mean for data centered on this point
    mean = getAvg(ref, i-halflen, len);
    if (ref[i]/mean>cutoff && !data[i].RFI) {
	data[i].RFI = true;
	count++;
    }
  }
  //And use that last value for the remaining data
  for (int i=end; i<size; i++) {
    if (ref[i]/mean>cutoff && !data[i].RFI) {
      data[i].RFI = true;
      count++;
    }
  }

  if (count>0) cerr << "Removed " << count << " of " << size << " too far from local mean\n";
}


/////////////////////////////////////////////////////////////////
//Find closest match or return -1
int nearest(int i, timegen_t *a, int alen,
	    timegen_t *b, int blen, timegen_t cutoff)
{
  if (a==NULL || alen==0 || b==NULL || blen==0) return -1;

  //Need to match up appropriate samples
  //Dodgy slow implementation too
  int b2=-1;
  bool first = true;
  long long bestdiff = 0;
  for (int b1=0; b1<blen; b1++) {
    long long thisdiff = a[i] - b[b1];
    if (thisdiff<0) thisdiff = -thisdiff; //Get absolute
    //Compare this with best
    if (thisdiff<bestdiff || first) {
      bestdiff = thisdiff;
      b2 = b1;
      first = false;
    }
  }
  if (cutoff!=0 && bestdiff>cutoff) return -1;
  return b2;
}


/////////////////////////////////////////////////////////////////
//Find bidirectional closest match or return -1
int matchTimes(int i, timegen_t *a, int alen,
	              timegen_t *b, int blen)
{
  if (a==NULL || alen==0 || b==NULL || blen==0) return -1;

  //Need to match up appropriate samples
  //Dodgy slow implementation too
  int b2=-1, a1=-1;

  long long bestdiff = a[i] - b[0];
  for (int b1=0; b1<blen; b1++) {
    long long thisdiff = a[i] - b[b1];
    if (thisdiff<0) thisdiff = -thisdiff; //Get absolute
    //Compare this with best
    if (thisdiff<=bestdiff) {
      bestdiff = thisdiff;
      b2 = b1;
    }
  }
  if (b2==-1) return -1;

  //Then also check if this ref epoch is closest to the data
  long long backdiff = a[0] - b[b2];
  if (backdiff<0) backdiff = -backdiff; //Get absolute
  for (int a2=0; a2<alen; a2++) {
    long long thisdiff = a[a2] - b[b2];
      if (thisdiff<0) thisdiff = -thisdiff; //Get absolute
      //Compare this with best
      if (thisdiff<=backdiff) {
	backdiff = thisdiff;
	a1 = a2;
      }
  }

  if (i!=a1) {
    //Not the closest match in both directions
    //cerr << "failed " << a[a1].timeStamp << "\t" << b[b1].timeStamp << endl;
    return -1;
  }
  return b2;
}


/////////////////////////////////////////////////////////////////
//Find bidirectional closest match or return -1
void matchTimes(float *adata, timegen_t *atimes, int alen,
		float *bdata, timegen_t *btimes, int blen,
		float *&ares, float *&bres,
		timegen_t *&restime, int &reslen)
{
  if (adata==NULL || alen==0 || bdata==NULL || blen==0) return;

  reslen = 0;
  int maxlen = (alen>blen)?alen:blen;
  ares = new float[maxlen];
  bres = new float[maxlen];
  restime = new timegen_t[maxlen];

  for (int i=0; i<alen; i++) {
    int j = matchTimes(i, atimes, alen, btimes, blen);
    if (j==-1) continue;
    ares[reslen] = adata[i];
    bres[reslen] = bdata[j];
    restime[reslen] = atimes[i];
    reslen++;
  }
}


//Find mapping y=mx+b between datasets and set m and b
void regression(float &m, float &b,
		float *x, timegen_t *xtimes, int xlen,
                float *y, timegen_t *ytimes, int ylen)
{
  float *xdata=NULL, *ydata=NULL;
  timegen_t *times=NULL;
  int dlen=0;

  matchTimes(x, xtimes, xlen, y, ytimes, ylen, xdata, ydata, times, dlen);

  double xsum = 0.0, ysum=0.0, xsq=0.0, xyprod=0.0;
  for (int i=0; i<dlen; i++) {
    xsum += xdata[i];
    ysum += ydata[i];
    xyprod += xdata[i]*ydata[i];
    xsq += xdata[i]*xdata[i];
  }
  double xmean = xsum/double(dlen);
  double ymean = ysum/double(dlen);
/*  double st2 = 0.0, bt=0.0;
  for (int i=0; i<dlen; i++) {
    double t = xdata[i]-xmean;
    st2 += t*t;
    bt += t*ydata[i];
  }*/

  double Sxy = xyprod - (xsum*ysum)/double(dlen);
  double Sxx = xsq - (xsum*xsum)/double(dlen);

  m = Sxy/Sxx;
  b = ymean - m*xmean;
  //m = bt/st2;
  //b = (ysum - xsum*b)/float(dlen);

  delete[] xdata;
  delete[] ydata;
  delete[] times;
}

