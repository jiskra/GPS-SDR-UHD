# See README for compiler options
CFLAGS= -Ofast  -pthread  `pkg-config --cflags librtlsdr`
LDLIBS= -lm -pthread  -luhd `pkg-config --libs librtlsdr`

# Airspy conf 
# CFLAGS= -Ofast -pthread -D WITH_AIR -I.  `pkg-config --cflags libairspy`
# LDLIBS= -lm -pthread  `pkg-config --libs libairspy` -lusb-1.0

# RTL only conf
#CFLAGS= -Ofast -pthread -D WITH_RTL -I.  `pkg-config --cflags librtlsdr`
#LDLIBS= -lm -pthread   -lrtlsdr `pkg-config --libs librtlsdr`


all:	getch.o getopt.o
	$(CC) -Ofast acarsserv.o dbmgn.o -o $@ $(LDLIBS)

getch.o:	getch.c getch.h
getopt.o:	getopt.c getopt.h
gpssim.o:	gpssim.c gpssim.h


clean:
	@\rm -f *.o acarsdec acarsserv
