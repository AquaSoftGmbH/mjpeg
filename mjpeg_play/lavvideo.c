/*
lavvideo (V1.0)
===============

Small tool for showing a realtime video window on the screen,
but (simplification) _ignoring_ all window managing.

Usage: lavvideo [options]
Have a look at the options below. Nothing is guaranteed to be working.

This code is released under the GPL.

Copyright by Gernot Ziegler.
*/

#define DEBUG(x) x
#define VERBOSE(x) x

/* Here you can enter your own framebuffer address, 
 * NULL means autodetect by V4L driver 
 */
#define FRAMEBUFFERADDRESS NULL

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <linux/types.h>
#include <linux/videodev.h>
#include <videodev_mjpeg.h>

#define DEVNAME "/dev/video0"

static char *device   = DEVNAME;

int v4ldev;
int file;
int res;
int frame;
struct video_window vw;
struct video_channel vc;
struct video_mbuf vb;
struct video_mmap vm;
struct video_buffer vv;
char * buff;

static int norm = VIDEO_MODE_PAL;
static int wait = -1;
static int input = 0;
static int xres = 800;
static int pixbits = 32;
static int width = 400;
static int height = 300;
static int xoffset = 5;
static int yoffset = 5;
static int verbose = 0;
static int debug = 0;

void usage(char *prog)
{
    char *h;

    if (NULL != (h = (char *)strrchr(prog,'/')))
	prog = h+1;
    
    fprintf(stderr,
	    "%s shows an overlay video window on the screen.\n"
	    "\n"
	    "usage: %s [ options ]\n"
	    "\n"
	    "options: [defaults]\n"
	    "  -h          help	       \n"
	    "  -v          verbose operation	       \n"
	    "  -d          debug information	       \n"
            "  -c device   specify device              [%s]\n"
            "  -t seconds  the time the video window shows [infinite]\n"
            "  -s size     specify size                [%dx%d]\n"
            "  -o offset   displacement on screen      [%dx%d]\n"
            "  -x xres     screen x resolution          [%d]\n"
            "  -b pixbits  Bits per pixel (15,16,24,32) [%d]\n"
	    "  -n tvnorm   set pal/ntsc/secam          [pal]\n"
	    "  -i input    set input source (int)      [%d]\n"
	    "\n"
	    "NOTE: lavvideo makes your V4L card write _directly_ into the frame buffer, "
            "ignoring\n _everything_ that lies under it. It is far from being as "
	    "comfortable as \ne.g. xawtv is, and just serving as a simple test and "
            "demo program."
	    "\n"
	    "examples:\n"
	    "  %s  | shows a video window with the default config.\n"
	    "  %s -c /dev/v4l0 -n ntsc | utilizes the device v4l0 for V4L I/O\n"
            "  communication, and expects the signal being NTSC standard.\n"
	    "\n"
	    "--\n"
	    "(c) 2000 Gernot Ziegler<gz@lysator.liu.se>\n",
	    prog, prog, device, width, height, xoffset, yoffset, xres, pixbits, 
	    input, prog, prog);
}

int doIt(void)
{ int turnon;

  /* V4l initialization */

  /* open device */
  if (verbose) printf("Opening device %s\n", device);
  v4ldev = open(device, O_RDWR);
  if (v4ldev < 0) { perror("Error opening video device."); exit(1); }

  /* select input */
  if (verbose) printf("Selecting input %d\n", input);
  vc.channel = input;
  vc.norm = norm;
  res = ioctl(v4ldev, VIDIOCSCHAN, &vc);
  if (res < 0) 
    { perror("VIDIOCSCHAN failed."); exit(1); }
  
  if (verbose) printf("Choosing window width [%dx%d] and offset [%dx%d]\n, "
		      "bitdepth %d", 
		      width, height, xoffset, yoffset, pixbits);
  vv.width = width;
  vv.height = height;
  if (pixbits == 15)
    vv.bytesperline = xres*16/8;
  else
    vv.bytesperline = xres*pixbits/8;
  vv.depth = pixbits;
  vv.base = NULL; 
  
  res = ioctl(v4ldev, VIDIOCSFBUF, &vv);
  if (res < 0) { perror("VIDIOCSFBUF failed."); exit(1); }

   /* set up window parameters */
  vw.x = xoffset; 
  vw.y = yoffset; 
  vw.width = width;
  vw.height = height;
  vw.chromakey = 0;
  vw.flags = 0;
  vw.clips = NULL;
  vw.clipcount = 0;
  
  res = ioctl(v4ldev, VIDIOCSWIN, &vw);
  if (res < 0) 
    { 
      perror("VIDIOCSWIN");
      printf("\n(Is your videocard listed in videodev.h in the v4l driver ?\n");  
      exit(1); 
    }
  
  if (verbose) printf("Turning on the video window !\n");
  turnon = 1;
  res = ioctl(v4ldev, VIDIOCCAPTURE, &turnon);
  if (res < 0) { perror("v4l: VIDIOCCAPTURE"); exit(1); }

  if (wait == -1) 
  { if (verbose) printf("Alright, infinite loop (interrupt with Ctrl-C).", 
			time);
    for(;;);
  }
  else
  if (verbose) printf("Alright, waiting %ld seconds.", time);
  usleep(wait*1000000);

  if (verbose) printf("Turning off the video window.\n");
  turnon = 0;
  res = ioctl(v4ldev, VIDIOCCAPTURE, &turnon);
  if (res < 0) { perror("v4l: VIDIOCCAPTURE"); exit(1); }
}

int main(int argc,char *argv[]) 
{ int c;

  /* parse options */
  for (;;) 
  {
    if (-1 == (c = getopt(argc, argv, "Svds:o:m:c:n:i:t:b:x:h")))
      break;

    switch (c) {
    case 'v':
      verbose = 1;
      break;
    case 'd':
      debug = 1;
      break;
    case 's':
      if (2 != sscanf(optarg,"%dx%d",&width,&height))
	{ width = 400; height = 300; }
      break;
    case 'o':
      if (2 != sscanf(optarg,"%dx%d",&xoffset,&yoffset))
	{ width = 5; height = 5; }
      break;
    case 'c':
      device = optarg;
      break;
    case 'n':
      if (0 == strcasecmp(optarg,"pal"))
	norm = VIDEO_MODE_PAL;
      else if (0 == strcasecmp(optarg,"ntsc"))
	norm = VIDEO_MODE_NTSC;
      else if (0 == strcasecmp(optarg,"secam"))
	norm = VIDEO_MODE_SECAM;
      else 
	{
	fprintf(stderr,"unknown tv norm %s\n",optarg);
	exit(1);
	}
      break;
    case 'i':
      input = atoi(optarg);
	    break;
    case 't':
      wait = atoi(optarg);
	    break;
    case 'b':
      pixbits = atoi(optarg);
	    break;
    case 'x':
      xres = atoi(optarg);
	    break;
    case 'h':
    default:
      usage(argv[0]);
      exit(1);
    }
  }
    
  doIt();

  return 0;
}










