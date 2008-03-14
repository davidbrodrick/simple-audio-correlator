
extern "C" {
#include <cpgplot.h>
}

#include <pthread.h>
#include <math.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <assert.h>
#include <iostream>
#include <PlotArea.h>

using namespace::std;

//Initialise lock for static info.
pthread_mutex_t PlotArea::theirLock;
bool PlotArea::theirIsInit = false;
int PlotArea::theirPopulation;
PlotArea **PlotArea::theirPlotAreas;

void PlotArea::init()
{
    //Lock static information
    //    pthread_mutex_lock(&theirLock);

    //Check if the class is already initialised
    if (theirIsInit == false) {
	//Now we will be!!
	theirIsInit = true;

	pthread_mutex_init(&theirLock, NULL);

	//At present there are no PlotAreas.
	theirPopulation = 0;

//	cpgenv(0, MAGICN, -2, 2, 0, 1);
    }

    //Release lock
//    pthread_mutex_unlock(&theirLock);
}

PlotArea::PlotArea(int xindex, int yindex)
:itsTitle("PlotArea"),
itsXtitle("X Axis"),
itsYtitle("Y Axis"),
itsXmax(64),
itsXmin(0),
itsXleft(0),
itsXright(0),
itsXauto(true),
itsXindex(xindex),
itsYindex(yindex),
itsYmax(6.0),
itsYmin(-1.0),
itsYtop(0),
itsYbottom(0),
itsYauto(true)
{
    //Ensure static data has been initialised
    init();

    pthread_mutex_init(&itsLock, NULL);

    clear();

    //cpglab(itsXtitle, itsYtitle, itsTitle);
}


int PlotArea::plotLine(int numpoints, float *xvals, float *yvals, int colour)
{
    float xmin, xmax, ymin, ymax;
    if (itsXauto) {
      xmin=xvals[0];
      xmax=xvals[numpoints-1];
    } else {
      xmin=itsXmin;
      xmax=itsXmax;
    }

    if (itsYauto) {
      ymin=ymax=yvals[0];
      for(int ii=0;ii<numpoints;ii++) {
	if(yvals[ii]>ymax) ymax=yvals[ii];
	if(yvals[ii]<ymin) ymin=yvals[ii];
      }
      float temp = ymax - ymin;
      temp/=20;
      ymax+=temp;   ymin-=temp;
    } else {
      ymin = itsYmin;
      ymax = itsYmax;
    }


    //Set focus to our panel
    cpgpanl(itsXindex, itsYindex);

    cpgswin(xmin,xmax,ymin,ymax);
    cpglab(itsXtitle,itsYtitle,itsTitle);
    cpgbox("ABCNPST", 0, 0, "ABCNPST", 0, 0);

    return pl(numpoints, xvals, yvals, colour);;
}


int PlotArea::plotLogLine(int numpoints, float *xvals, float *yvals, int colour)
{
    float xmin, xmax, ymin, ymax;
    if (itsXauto) {
      xmin=xvals[0];
      xmax=xvals[numpoints-1];
    } else {
      xmin=itsXmin;
      xmax=itsXmax;
    }

    if (itsYauto) {
      ymin=ymax=yvals[0];
      for(int ii=0;ii<numpoints;ii++) {
	if(yvals[ii]>ymax) ymax=yvals[ii];
	if(yvals[ii]<ymin) ymin=yvals[ii];
      }
      float temp = ymax - ymin;
      temp/=20;
      ymax+=temp;   ymin-=temp;
    } else {
      ymin = itsYmin;
      ymax = itsYmax;
    }

    //Set focus to our panel
    cpgpanl(itsXindex, itsYindex);

    cpgswin(xmin,xmax,ymin,ymax);
    cpglab(itsXtitle,itsYtitle,itsTitle);
    cpgbox("ABCNPST", 0, 0, "ABCNPSTL", 0, 0);

    return pl(numpoints, xvals, yvals, colour);;
}


int PlotArea::pl(int numpoints, float *xvals, float *yvals, int colour)
{

    //Lock
    pthread_mutex_lock(&itsLock);

#ifdef DEBUG
    cout << "Drawing " << numpoints << " point line in panel ("
 	 << itsXindex << ", " << itsYindex << ")\n";
#endif

    //Record the current colour, we will restore it
    int col;
    cpgqci(&col);
    //Set the colour
    cpgsci(colour);

    cpgslw(2);
    cpgline(numpoints, xvals, yvals);
    cpgslw(1);

    //Restore the original colour
    cpgsci(col);

    //Unlock
    pthread_mutex_unlock(&itsLock);

    return 1;
}


int PlotArea::plotPoints(int numpoints, float *xvals, float *yvals, int colour)
{
    float xmin, xmax, ymin, ymax;
    if (itsXauto) {
      xmin=xvals[0];
      xmax=xvals[numpoints-1];
    } else {
      xmin=itsXmin;
      xmax=itsXmax;
    }

    if (itsYauto) {
      ymin=ymax=yvals[0];
      for(int ii=0;ii<numpoints;ii++) {
	if(yvals[ii]>ymax) ymax=yvals[ii];
	if(yvals[ii]<ymin) ymin=yvals[ii];
      }
      float temp = ymax - ymin;
      temp/=20;
      ymax+=temp;   ymin-=temp;
    } else {
      ymin = itsYmin;
      ymax = itsYmax;
    }

    //Lock
    pthread_mutex_lock(&itsLock);

    //Set focus to our panel
    cpgpanl(itsXindex, itsYindex);

    cpgswin(xmin,xmax,ymin,ymax);
    cpglab(itsXtitle,itsYtitle,itsTitle);
    cpgbox("ABCNPST", 0, 0, "ABCNPST", 0, 0);

    pp(numpoints, xvals, yvals, colour);

    //Unlock
    pthread_mutex_unlock(&itsLock);

    return 1;
}

int PlotArea::plotLogPoints(int numpoints, float *xvals, float *yvals, int colour)
{
    float xmin, xmax, ymin, ymax;
    if (itsXauto) {
      xmin=xvals[0];
      xmax=xvals[numpoints-1];
    } else {
      xmin=itsXmin;
      xmax=itsXmax;
    }

    if (itsYauto) {
      ymin=ymax=yvals[0];
      for(int ii=0;ii<numpoints;ii++) {
	if(yvals[ii]>ymax) ymax=yvals[ii];
	if(yvals[ii]<ymin) ymin=yvals[ii];
      }
      float temp = ymax - ymin;
      temp/=20;
      ymax+=temp;   ymin-=temp;
    } else {
      ymin = itsYmin;
      ymax = itsYmax;
    }

    //Lock
    pthread_mutex_lock(&itsLock);

    //Set focus to our panel
    cpgpanl(itsXindex, itsYindex);

    cpgswin(xmin,xmax,ymin,ymax);
    cpglab(itsXtitle,itsYtitle,itsTitle);
    cpgbox("ABCNPST", 0, 0, "ABCNPSTL", 0, 0);

    pp(numpoints, xvals, yvals, colour);

    //Unlock
    pthread_mutex_unlock(&itsLock);

    return 1;
}


int PlotArea::pp(int numpoints, float *xvals, float *yvals, int colour)
{
#ifdef DEBUG
    cout << "Drawing " << numpoints << " point line in panel ("
 	 << itsXindex << ", " << itsYindex << ")\n";
#endif

    //Record the current colour, we will restore it
    int col;
    cpgqci(&col);
    //Set the colour
    cpgsci(colour);

    //Set line width
    cpgslw(5);
    cpgpt(numpoints, xvals, yvals, -1);
    cpgslw(1);

    //Restore the original colour
    cpgsci(col);

    return 1;
}


void PlotArea::drawContours(float **data, int len1, int len2,
			    int min1, int min2, int max1, int max2,
			    float off1, float off2, float sca1, float sca2)
{
  int col;

  //Lock
  pthread_mutex_lock(&itsLock);

#ifdef DEBUG
  cout << "Drawing contour plot\n";
#endif

  //cpgbbuf();

  //Set focus to our panel
  cpgpanl(itsXindex, itsYindex);
  cpgenv(0.0, len1, 0.0, len2, 0, 1);
  cpglab(itsXtitle,itsYtitle,itsTitle);
  cpgbox("ABCNPST", 0, 0, "ABCNPST", 0, 0);
  //Record the current colour, we will restore it
  cpgqci(&col);

//  int numcontours = 10;
//  float contours[numcontours];
//  for (int i=0; i<numcontours; i++) {
//    contours[i]=(1/(float)numcontours)*i;
//  }

  //cpgswin(itsXmin,itsXmax,itsYmin,itsYmax);
//  float trans[6] = {off1,sca1,0,off2,0,sca2};
  static float trans[6] = {0.0, 1.0, 0.0, 0.0, 0.0, 1.0};

  float cdata[len1*len2];
  for(int i=0;i<len1;i++) {
    for(int j=0;j<len2;j++) {
      cdata[i*len2+j] = data[i][j];
    }
  }

  for (int i=1; i<21; i++) {
    float alev = 0.0 + i*(2.0-0.0)/20.0;
    int lw = (i%5 == 0) ? 3 : 1;
    int ci = (i < 10)   ? 2 : 3;
    int ls = (i < 10)   ? 2 : 1;
    cpgslw(lw);
    cpgsci(ci);
    cpgsls(ls);
//    cpgcont(f, nx, ny, 1, nx, 1, ny, &alev, -1, tr);
    cpgcont(cdata,len1,len2,max1,min1,max2,min2,&alev,-1,trans);
  }
  cpgslw(1);
  cpgsls(1);
  cpgsci(1);

  cerr << "Drawing "<<len1<<" "<<len2<<" "<<min1<<":"<<max1<<" "<<
    min2<<":"<<max2<<endl;
//  cpggray(cdata,len1,len2,max1,min1,max2,min2,2.0,-0.5,trans);

  //Restore the original colour
  cpgsci(col);
  //Unlock
  pthread_mutex_unlock(&itsLock);
}

int PlotArea::clear()
{
  int col;

  //lock
  pthread_mutex_lock(&itsLock);

#ifdef DEBUG
  cout << "Clearing panel (" << itsXindex << ", " << itsYindex << ")\n";
#endif


  //Change focus to our panel
  cpgpanl(itsXindex, itsYindex);

  //Record the current colour, we will restore it
  cpgqci(&col);
  //Set the colour
  cpgsci(1);

  //Blank the panel
  cpgeras();

  //Draw labels
  //    cpglab(itsXtitle, itsYtitle, itsTitle);
  //Draw box and other stuff
  //    cpgbox("ABCNPST", 0, 0, "ABCNPST", 0, 0);

  //    cpgenv(itsXmin, itsXmax, itsYmin, itsYmax, 0, 1);

  //Restore the original colour
  cpgsci(col);

  //Unlock
  pthread_mutex_unlock(&itsLock);

  return 1;
}


bool PlotArea::setPopulation(int num)
{
    //Is the number valid?
    bool status = true;

    //How many panels x and how many y?
    int xnum=1, ynum=1;

    init();

    //Lock static data
    pthread_mutex_lock(&theirLock);

    //Tell pgplot how many panels we want
    switch(num) {
    case 1:
	break;
    case 2:
	ynum=2;
	break;
    case 3:
        xnum=1;
	ynum=3;
	break;
    case 4:
	xnum = ynum = 2;
        break;
    case 5:
    case 6:
	xnum=3;
        ynum=2;
	break;
    default:
        assert(false);
	status = false;
	}

    cpgsubp(1,1);
    cpgeras();
    //Do the actual pgplot call
    cpgsubp(xnum,ynum);

    if (num>theirPopulation) {
	//We need to create some more PlotAreas, but
        //first create a larger array to reference them all
	PlotArea **newpop = new PlotArea*[num];

        //Copy pointers to the old ones to the new array
	for (int i=0; i<theirPopulation; i++) {
	    newpop[i] = theirPlotAreas[i];
	    int ypos = i%ynum + 1;
	    int xpos = i/xnum + 1;
#ifdef DEBUG
	    cout << "Moving PlotArea to (" << xpos << ", "
		<< ypos << ")\n";
#endif

	    newpop[i]->setPosition(xpos, ypos);
	}

	//Now create some new ones
	for (int i=theirPopulation; i<num; i++) {
	    int ypos = (i%ynum) + 1;
	    int xpos = (i-ypos+1)/xnum + 1;

#ifdef DEBUG
	    cout << "Creating new PlotArea(" << xpos << ", "
		<< ypos << ")\n";
#endif
	    newpop[i] = new PlotArea(xpos, ypos);
	}

        //Now make the new array the active one
	delete[] theirPlotAreas;
	theirPlotAreas = newpop;
	theirPopulation = num;

    }

    //Unlock static data
    pthread_mutex_unlock(&theirLock);

    return status;
}

PlotArea *PlotArea::getPlotArea(int plot)
{
    PlotArea *p;

    //Lock static data
    pthread_mutex_lock(&theirLock);

#ifdef DEBUG
    cout << "Request for plot " << plot
	<< ", population = " << theirPopulation << endl;
#endif

    assert(plot<theirPopulation && plot>=0);

    p = theirPlotAreas[plot];

    //Unlock static data
    pthread_mutex_unlock(&theirLock);

    return p;
}


int PlotArea::getPopulation()
{
    int pop;

    //Lock static data
    pthread_mutex_lock(&theirLock);

    pop = theirPopulation;

    //Unlock static data
    pthread_mutex_unlock(&theirLock);

    return pop;
}

bool PlotArea::setAxisX(char *title, float max, float min, bool autoscale)
{
    bool status = true;

    //Lock
    pthread_mutex_lock(&itsLock);

    itsXtitle = title;
    itsXmax = max;
    itsXmin = min;
    itsXauto = autoscale;

    //Unlock
    pthread_mutex_unlock(&itsLock);

    clear();

    return status;
}

bool PlotArea::setAxisY(char *title, float max, float min, bool autoscale)
{
    bool status=true;

    //Lock
    pthread_mutex_lock(&itsLock);

    itsYtitle = title;
    itsYmax = max;
    itsYmin = min;
    itsYauto = autoscale;

    //Unlock
    pthread_mutex_unlock(&itsLock);

    return status;
}


void PlotArea::setPosition(int xindex, int yindex)
{
    assert(xindex>0 && yindex>0);

    pthread_mutex_lock(&itsLock);

    itsXindex = xindex;
    itsYindex = yindex;

    pthread_mutex_unlock(&itsLock);
}

void PlotArea::setTitle(char *title)
{
    pthread_mutex_lock(&itsLock);

    itsTitle = title;

    pthread_mutex_unlock(&itsLock);
}
