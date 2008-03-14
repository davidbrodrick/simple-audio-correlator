/* This class provides a simple c++ interface to 2D plotting facilities.
 * At present it is built on top of the pgplot graphics library.
 *
 * Copyright (C) David Brodrick 2001-08-04
 * Copyright (C) CSIRO Australia Telescope National Facility
 *
 * This code is licenced under the GPL version 2.
 */

#ifndef _PLOTAREA_HDR_
#define _PLOTAREA_HDR_

//#define DEBUG

#include <pthread.h>

class PlotArea {
public:
    PlotArea(int xindex, int yindex);

    //Destroy the plotarea and free resources.
    ~PlotArea();

    //Draw this line.
    int plotLine(int numpoints, float *xvals, float *yvals, int colour);
    //Draw line with log scale.
    int plotLogLine(int numpoints, float *xvals, float *yvals, int colour);
    //Draw these points.
    int plotPoints(int numpoints, float *xvals, float *yvals, int colour);
    //Draw these points with log scale.
    int plotLogPoints(int numpoints, float *xvals, float *yvals, int colour);


    //Blank all data currently displayed on the PlotArea.
    int clear();

    //Set the title for this particular PlotArea.
    //We copy the string internally, so the caller
    //retains ownership.
    void setTitle(char *title);

    //These two methods allow you to set axis information.
    //max and min values can be nominated, if autoscale is
    //true the axis will automatically scale to suit the
    //variation in the currently displayed data, otherwise
    //the nominated values will be kept indefinately.
    bool setAxisX(char *title, float max, float min, bool autoscale);
    bool setAxisY(char *title, float max, float min, bool autoscale);

    void drawContours(float **data, int len1, int len1,
		      int min1, int min2, int max1, int max2,
		      float off1, float off2, float sca1, float sca2);

    //Print this PlotArea to the nominated file.
    //The output is colour postscript format.
    int printFile(char *file);

    void setPosition(int xindex, int yindex);

    //Set the number of PlotAreas
    //If need be some are deleted, otherwise some are created.
    //All PlotAreas are cleared.
    static bool setPopulation(int num);
    //Return current population
    static int getPopulation();

    //Return a requested plot area, NULL if it doesn't exist
    static PlotArea *getPlotArea(int index);

private:
    //Draw this line.
    int pl(int numpoints, float *xvals, float *yvals, int colour);
    //Draw these points.
    int pp(int numpoints, float *xvals, float *yvals, int colour);


    //This method initialises static data such as the underlaying
    //graphics library has been initialised yet. Normally
    //the first PlotArea created will initialise things.
    static void init();

    //This records if static data has been initialised yet.
    static bool theirIsInit;

    //theirLock controls access to static information
    static pthread_mutex_t theirLock;

    //Total number of PlotAreas
    static int theirPopulation;

    //Array of pointers to all current PlotAreas
    static PlotArea **theirPlotAreas;

    //itsLock controls access to this objects internals
    pthread_mutex_t itsLock;

    //The title for this PlotArea
    char *itsTitle;

    //Title for X-axis
    char *itsXtitle;
    //Title for Y-axis
    char *itsYtitle;

    //Current scales for X-axis
    float itsXmax, itsXmin;
    //Current X positions of window
    int itsXleft, itsXright;
    //Are we autoscaling X?
    bool itsXauto;

    //where are we on the panel?
    int itsXindex, itsYindex;

    //Current scales for Y-axis
    float itsYmax, itsYmin;
    //Current Y positions of window
    int itsYtop, itsYbottom;
    //Are we autoscaling Y?
    bool itsYauto;

};

#endif //_PLOTAREA_HDR_
