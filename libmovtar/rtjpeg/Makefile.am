
bin_PROGRAMS=pnm2rtj rtjshow

pnm2rtj_SOURCES=pnm2rtj.c 
pnm2rtj_LDADD = @SDL_LIBS@ -lpthread -ljpeg RTjpeg.o format.o 

rtjshow_SOURCES=rtjshow.c 
rtjshow_LDADD = @SDL_LIBS@ -lpthread -ljpeg RTjpeg.o format.o 

CFLAGS=@CFLAGS@ -g

