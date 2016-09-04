#Makefile for sac - the "Simple Audio Correlator" - and associated utilities
#Some tweaking may be required on your system.. ?

INCLUDE = -I include
#CCOPTS  = -O3 -march=pentium -D_REENTRANT -Wall
CCOPTS  = -g -pthread -DREENTRANT -Wall -D_POSIX_REENTRANT_FUNCTIONS -Wno-write-strings
CC	= g++ ${INCLUDE} ${CCOPTS}
LIB	= g++
LIBFLAGS= -g -lstdc++ -lpthread -DREENTRANT -pthread -D_POSIX_REENTRANT_FUNCTIONS
XLIBFLAGS = -lstdc++ -lX11 -lcpgplot -lpgplot -lpng \
            -L/usr/X11R6/lib/ -L/usr/lib

all:	sac sacmon sacmkwav saciq sacrt sacedit sacforward sacmerge \
	sacrotate sacsim sacmodel sacriometer

SACOBJS	= sac.o ConfigFile.o AudioSource.o Processor.o StoreMaster.o \
	  WebMaster.o WebHandler.o RFI.o IntegPeriod.o ThreadedObject.o \
	  TCPstream.o Buf.o TimeCoord.o DataForwarder.o
	  
sac:	$(SACOBJS)           	
	$(LIB) -o sac $(SACOBJS) $(LIBFLAGS)

SACMONOBJS = sacmon.o IntegPeriod.o RFI.o TCPstream.o PlotArea.o \
	     TimeCoord.o SACUtil.o
#I think pgplot requires the Fortran linker
sacmon: $(SACMONOBJS)
	$(LIB) -o sacmon $(SACMONOBJS) $(XLIBFLAGS)

SACMKWAVOBJS = sacmkwav.o IntegPeriod.o TimeCoord.o TCPstream.o RFI.o
sacmkwav: $(SACMKWAVOBJS)
	$(LIB) -o sacmkwav $(SACMKWAVOBJS) $(LIBFLAGS)

SACRIOOBJS = sacriometer.o IntegPeriod.o TimeCoord.o RFI.o PlotArea.o \
	     SolarFlare.o chapman.o TCPstream.o
sacriometer: $(SACRIOOBJS)
	$(LIB) -o sacriometer $(SACRIOOBJS) $(XLIBFLAGS)

SACIQOBJS = saciq.o IntegPeriod.o TimeCoord.o TCPstream.o PlotArea.o RFI.o
saciq: $(SACIQOBJS)
	$(LIB) -o saciq $(SACIQOBJS) $(LIBFLAGS) $(XLIBFLAGS)

SACRTOBJS = sacrt.o IntegPeriod.o TimeCoord.o TCPstream.o PlotArea.o RFI.o
sacrt: $(SACRTOBJS)
	$(LIB) -o sacrt $(SACRTOBJS) $(LIBFLAGS) $(XLIBFLAGS)

SACEDITOBJS = sacedit.o IntegPeriod.o TimeCoord.o PlotArea.o RFI.o TCPstream.o
sacedit: $(SACEDITOBJS)
	$(LIB) -o sacedit $(SACEDITOBJS) $(LIBFLAGS) $(XLIBFLAGS)

SACMERGEOBJS = sacmerge.o IntegPeriod.o TimeCoord.o RFI.o TCPstream.o
sacmerge: $(SACMERGEOBJS)
	$(LIB) -o sacmerge $(SACMERGEOBJS) $(LIBFLAGS) $(XLIBFLAGS)

SACMODOBJS = sacmodel.o IntegPeriod.o TimeCoord.o TCPstream.o PlotArea.o RFI.o \
	     Site.o Antenna.o Source.o
sacmodel: $(SACMODOBJS)
	$(LIB) -o sacmodel $(SACMODOBJS) $(LIBFLAGS) $(XLIBFLAGS)

SACROTOBJS = sacrotate.o IntegPeriod.o TimeCoord.o TCPstream.o PlotArea.o \
	RFI.o Source.o Site.o Antenna.o
sacrotate: $(SACROTOBJS)
	$(LIB) -o sacrotate $(SACROTOBJS) $(LIBFLAGS) $(XLIBFLAGS)

SACFORWARDOBJS = sacforward.o IntegPeriod.o TimeCoord.o TCPstream.o \
	         DataForwarder.o RFI.o ThreadedObject.o
sacforward: $(SACFORWARDOBJS)
	$(LIB) -o sacforward $(SACFORWARDOBJS) $(LIBFLAGS)

SACSIMOBJS = sacsim.o IntegPeriod.o TimeCoord.o TCPstream.o PlotArea.o RFI.o \
	     Source.o Site.o Antenna.o
sacsim: $(SACSIMOBJS)
	$(LIB) -o sacsim $(SACSIMOBJS) $(LIBFLAGS) $(XLIBFLAGS)

sacsim.o: src/sacsim.cc Makefile include/IntegPeriod.h include/TimeCoord.h include/PlotArea.h include/Source.h include/Antenna.h include/Site.h
	$(CC) -c src/sacsim.cc

sacforward.o: src/sacforward.cc Makefile include/IntegPeriod.h include/TimeCoord.h include/DataForwarder.h include/TCPstream.h
	$(CC) -c src/sacforward.cc

sacmerge.o: src/sacmerge.cc Makefile include/IntegPeriod.h include/TimeCoord.h include/PlotArea.h include/RFI.h
	$(CC) -c src/sacmerge.cc

sacriometer.o: src/sacriometer.cc Makefile include/IntegPeriod.h include/TimeCoord.h include/PlotArea.h include/RFI.h include/SolarFlare.h
	$(CC) -c src/sacriometer.cc

sacrt.o: src/sacrt.cc Makefile include/IntegPeriod.h include/TimeCoord.h include/PlotArea.h
	$(CC) -c src/sacrt.cc

sacedit.o: src/sacedit.cc Makefile include/IntegPeriod.h include/TimeCoord.h include/PlotArea.h include/RFI.h
	$(CC) -c src/sacedit.cc

sacmodel.o: src/sacmodel.cc Makefile include/IntegPeriod.h include/TCPstream.h include/RFI.h include/PlotArea.h include/TimeCoord.h include/Site.h include/Antenna.h
	$(CC) -c src/sacmodel.cc

sacmon.o: src/sacmon.cc Makefile include/IntegPeriod.h include/TCPstream.h include/RFI.h include/PlotArea.h include/TimeCoord.h include/SACUtil.h
	$(CC) -c src/sacmon.cc

sacmkwav.o: src/sacmkwav.cc Makefile include/IntegPeriod.h include/TimeCoord.h
	$(CC) -c src/sacmkwav.cc

saciq.o: src/saciq.cc Makefile include/IntegPeriod.h include/PlotArea.h include/TimeCoord.h
	$(CC) -c src/saciq.cc

sacrotate.o: src/sacrotate.cc Makefile include/IntegPeriod.h include/TimeCoord.h include/PlotArea.h include/Antenna.h include/Site.h include/Source.h
	$(CC) -c src/sacrotate.cc

sac.o: src/sac.cc Makefile include/Buf.h include/AudioSource.h include/Processor.h include/StoreMaster.h include/WebMaster.h include/ConfigFile.h include/ThreadedObject.h
	$(CC) -c src/sac.cc

Buf.o: src/Buf.cc Makefile include/Buf.h include/IntegPeriod.h
	$(CC) -c src/Buf.cc
        
SACUtil.o: src/SACUtil.cc Makefile include/SACUtil.h 
	$(CC) -c src/SACUtil.cc

IntegPeriod.o: src/IntegPeriod.cc Makefile include/IntegPeriod.h include/RFI.h include/TimeCoord.h
	$(CC) -c src/IntegPeriod.cc
        
AudioSource.o: src/AudioSource.cc Makefile include/AudioSource.h include/Buf.h include/ThreadedObject.h include/IntegPeriod.h
	$(CC) -c src/AudioSource.cc
        
ThreadedObject.o: src/ThreadedObject.cc Makefile include/ThreadedObject.h
	$(CC) -c src/ThreadedObject.cc

Processor.o: src/Processor.cc Makefile include/Processor.h include/Buf.h include/ThreadedObject.h include/IntegPeriod.h 
	$(CC) -c src/Processor.cc

StoreMaster.o: src/StoreMaster.cc Makefile include/StoreMaster.h include/Buf.h include/IntegPeriod.h include/TimeCoord.h
	$(CC) -c src/StoreMaster.cc
        
WebMaster.o: src/WebMaster.cc Makefile include/WebMaster.h include/TCPstream.h include/ConfigFile.h include/ThreadedObject.h
	$(CC) -c src/WebMaster.cc

WebHandler.o: src/WebHandler.cc Makefile include/WebHandler.h include/WebMaster.h include/TCPstream.h include/StoreMaster.h include/IntegPeriod.h include/ConfigFile.h include/ThreadedObject.h
	$(CC) -c src/WebHandler.cc

DataForwarder.o: src/DataForwarder.cc Makefile include/DataForwarder.h include/TCPstream.h include/StoreMaster.h include/IntegPeriod.h include/ConfigFile.h include/ThreadedObject.h
	$(CC) -c src/DataForwarder.cc

RFI.o: src/RFI.cc Makefile include/RFI.h include/IntegPeriod.h
	$(CC) -c src/RFI.cc

ConfigFile.o: src/ConfigFile.cc Makefile include/ConfigFile.h
	$(CC) -c src/ConfigFile.cc

TCPstream.o: src/TCPstream.cc Makefile include/TCPstream.h
	$(CC) -c src/TCPstream.cc

PlotArea.o: src/PlotArea.cc Makefile include/PlotArea.h
	$(CC) -c src/PlotArea.cc
        
TimeCoord.o: src/TimeCoord.cc Makefile include/TimeCoord.h
	$(CC) -c src/TimeCoord.cc

Antenna.o: src/Antenna.cc Makefile include/TimeCoord.h include/Antenna.h
	$(CC) -c src/Antenna.cc
	
Site.o: src/Site.cc Makefile include/Site.h include/IntegPeriod.h include/TimeCoord.h include/Antenna.h include/Source.h
	$(CC) -c src/Site.cc
	
Source.o: src/Source.cc Makefile include/Source.h include/TimeCoord.h
	$(CC) -c src/Source.cc

SolarFlare.o: src/SolarFlare.cc Makefile include/SolarFlare.h include/TimeCoord.h include/RFI.h include/IntegPeriod.h
	$(CC) -c src/SolarFlare.cc

chapman.o: src/chapman.for Makefile
	f77 -g -c src/chapman.for
        
clean:
	rm -f sac sacmon sacmkwav saciq sacxray sacrt sacmodel sacmerge \
	      sacedit sacsim *.o core *~ data.out data.wav data.txt nohup.out \
	      complex.out complex.txt sacriometer sacforward sacrotate
