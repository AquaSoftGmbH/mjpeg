BIN_DIR = /usr/local/bin
LIB_DIR = /usr/lib/site-python

all: ioctl V4L.py

ioctl: ioctl.c
	gcc -o ioctl ioctl.c

V4L.py: ioctl
	./ioctl

install:
	install -m 755 v4l.py ${LIB_DIR}
	install -m 755 V4L.py ${LIB_DIR}
