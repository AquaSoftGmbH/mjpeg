#
# Makefile for mplex - the MPEG1/Systems Multiplexer
# Please edit this file according to your development platform.
#

# Source code is clean K&R - Style C, tested and compiled successfully on
# (Hewlett-Packard)  HPux : cc, gcc
# (Sun)              SUNos: cc, gcc
# (Silicon Graphics) IRIX : cc, gcc
# (SiemensNixdorf)   SINIX: ccs

# I guess the code should compile on just about any either K&R-Style or
# ANSI C compiler. Let me know if you got it compiled on further platforms.
# I know of some efforts had been made to compile this program to a DOS
# platform, using a port of GNU's gcc compiler, but don't know how it
# worked out. The program heavily depends on assumptions about size of
# 'unsigned int', but I don't think it should be too hard to get it to work
# under DOS. Let me know if you have such a port.

#CFLAGS =  -g

CFLAGS =  -O1 -g -Wall
LDFLAGS=  -lm
#CC     =  egcs
CC    =  gcc
RM     =  /bin/rm -f

OBJS = main.o bits.o buffer.o inits.o inptstrm.o interact.o multplex.o systems.o timecode.o vector.o

SRCS = main.c bits.c buffer.c inits.c inptstrm.c interact.c multplex.c systems.c timecode.c vector.c

all: mplex

mplex: $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

clean:
	$(RM) $(OBJS) *% *~ core mplex

