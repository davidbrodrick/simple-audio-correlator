# Introduction #

Add your content here.


# Contents #

Add your content here.  Format your content with:
  * Text in **bold** or _italic_
  * Headings, paragraphs, and lists
  * Automatic links to other wiki pages




INTRO:

---

"sac", the "Simple Audio Correlator" is a collection of C++ classes to read
data from a PC sound card, calculate power and power spectra measurements for
each channel and the cross correlation of multiple channels, record the
information to disk, and serve the data collection off disk to client programs
via a simple TCP interface.

All this proves to be very useful for making simple radio observatories: a
single radio can be plugged into the computer; two radios can be plugged into
the computer using the left and right channels of the sound card; or two radios
with phase coherent local oscillators can be used to form a digital cross
correlating radio interferometer.

sac uses a configuration file 'sac.conf' to specify, at runtime, how the
program should behave. By default sac will look in the current directory
for sac.conf but you can specify an alternate configuration file simply by
providing the name of the file as an argument, eg:

./sac /etc/sac.conf

A sample of 'sac.conf', including full documentation should be found in this
directory. You will need to customise it if you plan to use sac to serve
telescope data.


BUILD INSTRUCTIONS:

---

In order to build sac you should (if you have Linux, the right compiler and
dependent libraries) only need to issue the command...

> make

...in the directory containing the sac source code. This produces a binary
executable called 'sac'. Please let me know if you had to alter the Makefile
or the code to get sac to build on your system.

Note for non-Debian users: Steve Heaton has advised that he didn't need
the -lpng library with RedHat. In Debian pgplot depends on it.

Send bug fixes/comments/patches/feature requests to brodrick@fastmail.fm

This software requires g++-3.2 in order to compile successfully.
It will NOT compile with g++-2.95/old libstdc++.

It is certainly NOT ANSI C++ compliant, for lots of reasons, mainly the use of
the  'long long' data type for representation of time. It might be hard to
build this software using compilers other than gcc and operating systems other
than Linux. You are encouraged to try anyway.


LICENSE:

---

Copyright David Brodrick 2001-2004.

<insert pseudolegal phrase here>

sac comes with NO warranty to the extent permitted by law.