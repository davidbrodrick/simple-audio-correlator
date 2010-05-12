#Makefile for sac - the "Simple Audio Correlator" - and associated utilities
#Some tweaking may be required on your system.. ?

INCLUDE = -I.
#CCOPTS  = -O3 -march=pentium -D_REENTRANT -Wall
CCOPTS  = -g -pthread -DREENTRANT -Wall -D_POSIX_REENTRANT_FUNCTIONS
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
	$(LIB) -o sacmkwav  $(SACMKWAVOBJS) $(LIBFLAGS)

SACRIOOBJS = sacriometer.o IntegPeriod.o TimeCoord.o RFI.o PlotArea.o \
	     SolarFlare.o chapman.o TCPstream.o
sacriometer: $(SACRIOOBJS)
	$(LIB) -o sacriometer  $(SACRIOOBJS) $(XLIBFLAGS)

SACIQOBJS = saciq.o IntegPeriod.o TimeCoord.o TCPstream.o PlotArea.o RFI.o
saciq: $(SACIQOBJS)
	$(LIB) -o saciq  $(SACIQOBJS) $(LIBFLAGS) $(XLIBFLAGS)

SACRTOBJS = sacrt.o IntegPeriod.o TimeCoord.o TCPstream.o PlotArea.o RFI.o
sacrt: $(SACRTOBJS)
	$(LIB) -o sacrt  $(SACRTOBJS) $(LIBFLAGS) $(XLIBFLAGS)

SACEDITOBJS = sacedit.o IntegPeriod.o TimeCoord.o PlotArea.o RFI.o TCPstream.o
sacedit: $(SACEDITOBJS)
	$(LIB) -o sacedit  $(SACEDITOBJS) $(LIBFLAGS) $(XLIBFLAGS)

SACMERGEOBJS = sacmerge.o IntegPeriod.o TimeCoord.o RFI.o TCPstream.o
sacmerge: $(SACMERGEOBJS)
	$(LIB) -o sacmerge  $(SACMERGEOBJS) $(LIBFLAGS) $(XLIBFLAGS)

SACMODOBJS = sacmodel.o IntegPeriod.o TimeCoord.o TCPstream.o PlotArea.o RFI.o \
	     Site.o Antenna.o Source.o
sacmodel: $(SACMODOBJS)
	$(LIB) -o sacmodel  $(SACMODOBJS) $(LIBFLAGS) $(XLIBFLAGS)

SACROTOBJS = sacrotate.o IntegPeriod.o TimeCoord.o TCPstream.o PlotArea.o \
	RFI.o Source.o Site.o Antenna.o
sacrotate: $(SACROTOBJS)
	$(LIB) -o sacrotate  $(SACROTOBJS) $(LIBFLAGS) $(XLIBFLAGS)

SACFORWARDOBJS = sacforward.o IntegPeriod.o TimeCoord.o TCPstream.o \
	         DataForwarder.o RFI.o ThreadedObject.o
sacforward: $(SACFORWARDOBJS)
	$(LIB) -o sacforward  $(SACFORWARDOBJS) $(LIBFLAGS)

SACSIMOBJS = sacsim.o IntegPeriod.o TimeCoord.o TCPstream.o PlotArea.o RFI.o \
	     Source.o Site.o Antenna.o
sacsim: $(SACSIMOBJS)
	$(LIB) -o sacsim  $(SACSIMOBJS) $(LIBFLAGS) $(XLIBFLAGS)

sacsim.o: sacsim.cc IntegPeriod.h TimeCoord.h PlotArea.h Source.h \
	  Antenna.h Site.h Makefile 
	$(CC) -c sacsim.cc

sacforward.o: sacforward.cc IntegPeriod.h TimeCoord.h DataForwarder.h Makefile \
		TCPstream.h
	$(CC) -c sacforward.cc

sacmerge.o: sacmerge.cc IntegPeriod.h TimeCoord.h PlotArea.h RFI.h Makefile
	$(CC) -c sacmerge.cc

sacriometer.o: sacriometer.cc IntegPeriod.h TimeCoord.h PlotArea.h RFI.h \
	       Makefile SolarFlare.h
	$(CC) -c sacriometer.cc

sacrt.o: sacrt.cc IntegPeriod.h TimeCoord.h PlotArea.h Makefile
	$(CC) -c sacrt.cc

sacedit.o: sacedit.cc IntegPeriod.h TimeCoord.h PlotArea.h RFI.h Makefile
	$(CC) -c sacedit.cc

sacmodel.o: sacmodel.cc IntegPeriod.h TCPstream.h RFI.h PlotArea.h TimeCoord.h \
	    Makefile Site.h Antenna.h
	$(CC) -c sacmodel.cc

sacmon.o: sacmon.cc IntegPeriod.h TCPstream.h RFI.h PlotArea.h TimeCoord.h \
	  Makefile SACUtil.h
	$(CC) -c sacmon.cc

sacmkwav.o: sacmkwav.cc IntegPeriod.h TimeCoord.h Makefile
	$(CC) -c sacmkwav.cc

saciq.o: saciq.cc IntegPeriod.h PlotArea.h TimeCoord.h Makefile
	$(CC) -c saciq.cc

sacrotate.o: sacrotate.cc IntegPeriod.h TimeCoord.h PlotArea.h Makefile \
	Antenna.h Site.h Source.h
	$(CC) -c sacrotate.cc

sac.o: sac.cc Buf.h AudioSource.h Processor.h StoreMaster.h WebMaster.h \
	ConfigFile.h ThreadedObject.h Makefile
	$(CC) -c sac.cc

Buf.o: Buf.h Buf.cc IntegPeriod.h Makefile
	$(CC) -c Buf.cc
        
SACUtil.o: SACUtil.h SACUtil.cc
	$(CC) -c SACUtil.cc

IntegPeriod.o: IntegPeriod.h IntegPeriod.cc Makefile RFI.h TimeCoord.h
	$(CC) -c IntegPeriod.cc
        
AudioSource.o: AudioSource.h AudioSource.cc Buf.h ThreadedObject.h \
	       IntegPeriod.h Makefile
	$(CC) -c AudioSource.cc
        
ThreadedObject.o: ThreadedObject.h ThreadedObject.cc Makefile
	$(CC) -c ThreadedObject.cc

Processor.o: Processor.cc Processor.h Buf.h ThreadedObject.h IntegPeriod.h \
	     Makefile 
	$(CC) -c Processor.cc

StoreMaster.o: StoreMaster.cc StoreMaster.h Buf.h IntegPeriod.h \
		TimeCoord.h Makefile 
	$(CC) -c StoreMaster.cc
        
WebMaster.o: WebMaster.h WebMaster.cc TCPstream.h ConfigFile.h \
	     ThreadedObject.h Makefile
	$(CC) -c WebMaster.cc

WebHandler.o: WebHandler.h WebHandler.cc WebMaster.h TCPstream.h \
	      StoreMaster.h IntegPeriod.h ConfigFile.h \
              ThreadedObject.h Makefile
	$(CC) -c WebHandler.cc

DataForwarder.o: DataForwarder.h DataForwarder.cc TCPstream.h \
	      StoreMaster.h IntegPeriod.h ConfigFile.h \
              ThreadedObject.h Makefile
	$(CC) -c DataForwarder.cc

RFI.o: RFI.h RFI.cc IntegPeriod.h Makefile
	$(CC) -c RFI.cc

ConfigFile.o: ConfigFile.cc ConfigFile.h Makefile
	$(CC) -c ConfigFile.cc

TCPstream.o: TCPstream.h TCPstream.cc Makefile
	$(CC) -c TCPstream.cc

PlotArea.o: PlotArea.h PlotArea.cc Makefile
	$(CC) -c PlotArea.cc
        
TimeCoord.o: TimeCoord.h TimeCoord.cc Makefile
	$(CC) -c TimeCoord.cc

Antenna.o: Antenna.cc TimeCoord.h
	$(CC) -c Antenna.cc
	
Site.o: Site.h Site.cc IntegPeriod.h TimeCoord.h Antenna.h Source.h 
	$(CC) -c Site.cc
	
Source.o: Source.h Source.cc TimeCoord.h
	$(CC) -c Source.cc

SolarFlare.o: SolarFlare.h SolarFlare.cc TimeCoord.h RFI.h IntegPeriod.h
	$(CC) -c SolarFlare.cc

chapman.o: Makefile chapman.for
	f77 -g -c chapman.for
        
clean:
	rm -f sac sacmon sacmkwav saciq sacxray sacrt sacmodel sacmerge \
	      sacedit sacsim *.o core *~ data.out data.wav data.txt nohup.out \
	      complex.out complex.txt sacriometer sacforward sacrotate
