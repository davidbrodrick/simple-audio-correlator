#include <SACUtil.h>

extern "C" {
#include <cpgplot.h>
}

//Set a standard palette for pgplot.
void setupPalette()
{
  int foo;

  cpgscrn(0, "black", &foo);
  cpgscrn(1, "white", &foo);

  cpgscrn(2, "green", &foo); //input 1 dots
  cpgscrn(3, "blue", &foo); //input 1 lines

  cpgscrn(4, "violet", &foo); //input 2 dots
  cpgscrn(5, "firebrick", &foo); //input 2 lines

  cpgscrn(6, "red", &foo); //cross correlation dots
  cpgscrn(7, "white", &foo); //cross correlation lines
}

//Set a palette with a white background and appropriate colours
void setupPaletteWhite()
{
  int foo;

  cpgscrn(0, "white", &foo); //background
  cpgscrn(1, "black", &foo); //frames, axis etc

  cpgscrn(2, "green", &foo); //input 1 dots
  cpgscrn(3, "slate blue", &foo); //input 1 lines

  cpgscrn(4, "firebrick", &foo); //input 2 dots
  cpgscrn(5, "violet", &foo); //input 2 lines

  cpgscrn(6, "red", &foo); //cross correlation dots
  cpgscrn(7, "black", &foo); //cross correlation lines
}
