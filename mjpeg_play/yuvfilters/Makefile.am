O		= .$(if $(OBJEXT),$(OBJEXT),o)
X		= $(EXEEXT)

READERS		=
WRITERS		=
FILTERS		= yuvycsnoise$X yuvkineco$X
COREOBJS	= addtask$O
STDOBJS		= $(COREOBJS) alloctask$O initframe$O putframe$O yuvstdin$O yuvstdout$O timecode$O
CORECFLAGS	= $(DEBUG) -O2 -Wall -Wpointer-arith -Wcast-align -Wwrite-strings
CFLAGS		= $(CORECFLAGS) -Wcast-qual
L		= libyuvfilters.a
BINARIES	= $(READERS) $(WRITERS) $(FILTERS)
READERMAINS	= $(READERS:$X=_main$O)
WRITERMAINS	= $(WRITERS:$X=_main$O)
FILTERMAINS	= $(FILTERS:$X=_main$O)
MAINS		= $(BINARIES:$X=_main$O)

bin_SCRIPTS	= $(BINARIES)
CLEANFILES	= $(BINARIES) $(MAINS) $L
SOURCES		= main.c $(BINARIES:$X=.c) $(STDOBJS:$O=.c) yuvfilters.h timecode.h timecode.h
EXTRA_DIST	= README.yuvkineco

%$X: %_main$O $L(%$O $(STDOBJS))
	$(CC) $(LDFLAGS) -o $@ $< $L
yuvkineco$X: $L(yuvycsnoise$O)

$(MAINS): main.c yuvfilters.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ -c $<
$(READERMAINS): override CPPFLAGS += -DREADER=$(@:_main$O=)
$(WRITERMAINS): override CPPFLAGS += -DWRITER=$(@:_main$O=)
$(FILTERMAINS): override CPPFLAGS += -DFILTER=$(@:_main$O=)

$L(%$O): %.c yuvfilters.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $*$O -c $<
	$(AR) r $L $*$O
	$(RM) $*$O
$L($(COREOBJS)): override CFLAGS = $(CORECFLAGS)
$L(timecode$O yuvctrlbytty$O yuvkineco$O): timecode.h
