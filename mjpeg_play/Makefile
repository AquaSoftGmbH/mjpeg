#################################################
# Makefile for movtar_play
# I have no clue about automake and autoconfig
# so please be merciful to me when complaining 
# about this one ;)
# /Gernot (gz@lysator.liu.se)
#################################################
# config
#gcc -pg -mpentiumpro jpegtestfunk.c -ojpegtestfunk -L.  -L./buz_tools/ -O6 
#-laudio -ljpeg -lmovtar -lasound 
M_OBJS       = movtar_play

# uncomment if you want to use ALSA, 
# the Advanced Linux Sound Architecture (http://alsa.jcu.cz) 
#USE_ALSA = -DUSE_ALSA -lasound
C_FLAGS  = `glib-config glib --cflags`
L_FLAGS  = `glib-config glib --libs` -ljpeg -lpthread -lSDL -L./jpeg-6b-mmx

all:    $(M_OBJS)

movtar_play: movtar.c movtar_play.c
	gcc movtar.c movtar_play.c $(C_FLAGS) $(L_FLAGS) $(USE_ALSA) -o movtar_play

install:
	su -c "cp -v $(M_OBJS) /usr/local/bin/"

clean:
	rm -f $(M_OBJS) .*.o.flags *.o *~


