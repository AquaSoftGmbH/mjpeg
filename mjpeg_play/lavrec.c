/*
    lavrec - Linux Audio Video RECord

    Copyright (C) 2000 Rainer Johanni <Rainer@Johanni.de>

    Extended by: Gernot Ziegler <gz@lysator.liu.se>


    Capture motion video from the IOMEGA Buz to an AVI or Quicktime
    file together with capturing audio from the soundcard.

    Usage: lavrec [options] filename [filename ...]

    where options are as follows:

    -f [aAqm]    Format, 'a' for AVI (default), 'A' for AVI with fields
                 exchanged, 'q' for quicktime, 'm' for movtar
                 Hint: If your AVI video looks strange, try 'A' instead 'a'
                 and vice versa.
		 
    -i [pPnNta]  Input Source:
                 'p' PAL  Composite Input
                 'P' PAL  SVHS-Input
                 'n' NTSC Composite Input
                 'N' NTSC SVHS-Input
                 't' TV tuner input (if available)
                 'a' (or every other letter) Autosense (default)

   -d num        decimation, must be either 1, 2 or 4 for identical decimation
                 in horizontal and vertical direction (mostly used) or a
                 two digit letter with the first digit specifying horizontal
                 decimation and the second digit specifying vertical decimation
                 (more exotic usages).

   -g WxH+X+Y    An X-style geometry string for capturing subareas.
                 Even if a decimation > 1 is used, these values are always
                 coordinates in the undecimated (720x576 or 720x480) frame.
                 Also, unlike in X-Window, negative values for X and Y
                 really mean negative offsets (if this feature is enabled
                 in the driver) which lets you fine tune the position of the
                 image caught.
                 The horizontal resolution of the DECIMATED frame must
                 allways be a multiple of 16, the vertical resolution
                 of the DECIMATED frame must be a multiple of 16 for decimation 1
                 and a multiple of 8 for decimations 2 and 4

                 If not offset (X and Y values) is given, the capture area
                 is centered in the frame.

   -q num        quality, must be beetween 0 and 100, default is 50

   -t num        Time to capture in seconds, default is unlimited
                 (use ^C to stop capture!)

   -S            Single frame capture mode

   -T num        Time lapse factor: Video will be played back <num> times
                 faster, audio is switched off.
                 If num==1 it is silently ignored.

   -w            Wait for user confirmation to start

   *** Audio settings ***

   -a num        Audio size in bits, must be 0 (no audio), 8 or 16 (default)

   -r num        Audio rate, must be a permitted sampling rate for your soundcard
                 default is 22050.

   -s            enable stereo (disabled by default)

   -l num        Audio level to use for recording, must be beetwen 0 and 100
                 or -1 (for not touching the mixer settings at all), default is 100

                 Only if mixer is used (l != -1):

   -m            Mute audio output during recording (default is to let it enabled).
                 This is particularly usefull if you are recording with a
                 microphone to avoid feedback.

   -R [lmc]      Recording source:
                 l: line
                 m: microphone
                 c: cdrom

   *** Audio/Video synchronization ***

   -c num        Level of corrections for synchronization:
                 0: Neither try to replicate lost frames nor any sync correction
                 1: Replicate frames for lost frames, but no sync correction
                 2. lost frames replication + sync correction

   *** Special capture settings ***

   -n num        Number of MJPEG capture buffers (default 32)

   -b num        Buffersize of MJPEG buffers in KB (default 256)


   There may be given multiple filenames to overcome
   the AVI (and ext2fs) 2 GB limit:

   Either by giving multiple filenames on the command line
   or by giving one filename which contains a '%' sign.
   This name is then interpreted as the format argument to sprintf() 
   to form multiple file names.

   *** Environment variables ***

   LAV_VIDEO_DEV     Name of video device (default: "/dev/video")
   LAV_AUDIO_DEV     Name of audio device (default: "/dev/dsp")
   LAV_MIXER_DEV     Name of mixer device (default: "/dev/mixer")


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <linux/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
//#include <sys/resource.h>
#include <sys/vfs.h>

#include <signal.h>
#include <stdlib.h>

#include <linux/videodev.h>
#include <linux/soundcard.h>
#include <videodev_mjpeg.h>

#include "lav_io.h"

/* These are explicit prototypes for the compiler, to prepare separation of audiolib.c */
void audio_shutdown();
int audio_init(int a_read, int a_stereo, int a_size, int a_rate);
long audio_get_buffer_size();
int audio_read(char *buf, int size, int swap, struct timeval *tmstmp, int *status);

char *audio_strerror();

/* Set the default options here */

static char video_format = 'a';         /* 'a' or 'A' for AVI, 'q' for Quicktime */
static char input_source = 'a';         /* 'a' for Auto detect, others see above */
static int  hordcm       = 4;           /* horizontal decimation */
static int  verdcm       = 4;           /* vertical decimation */
static int  geom_x       = 0;
static int  geom_y       = 0;           /* x geometry string:  "WxH+X+Y" */
static int  geom_width   = 0;
static int  geom_height  = 0;
static int  geom_flags   = 0;           /* flags what has been read */
static int  quality      = 50;          /* determines target size of JPEG code,
                                           100=max. quality */
static int  record_time  = 100000;      /* "unlimited" */

static int  single_frame   = 0;         /* continous capture */
static int  time_lapse     = 1;         /* no time lapse */
static int  wait_for_start = 0;         /* Wait for user confirmation to start */

static int audio_size    = 16;          /* size of an audio sample: 8 or 16 bits,
                                           0 for no audio */
static int audio_rate    = 44100;       /* sampling rate for audio */
static int stereo        = 0;           /* 0: capture mono, 1: capture stereo */
static int audio_lev     = 100;         /* Level of Audio input,
                                            0..100: Recording level
                                           -1:  don't change mixer settings */
static int audio_mute    = 0;           /* Flag for muting audio output */
static int audio_recsrc  = 'l';         /* Audio recording source */
static int sync_corr     = 2;           /* Level of corrections for synchronization */
static int MJPG_nbufs    = 64;          /* Number of MJPEG buffers */
static int MJPG_bufsize  = 256;         /* Size   of MJPEG buffers */

static int interlaced;

static int verbose	= 2;
/* On some systems MAP_FAILED seems to be missing */

#ifndef MAP_FAILED
#define MAP_FAILED ( (caddr_t) -1 )
#endif

static lav_file_t *video_file, *video_file_old;

#define NUM_AUDIO_TRIES 500 /* makes 10 seconds with 20 ms pause beetween tries */

static int width, height;
static int input, norm;
static int audio_bps; /* audio bytes per sample */

static char infostring[4096];

#define LAVREC_INTERNAL 0
#define LAVREC_DEBUG    1
#define LAVREC_INFO     2
#define LAVREC_WARNING  3
#define LAVREC_ERROR    4
#define LAVREC_PROGRESS 5

static int need_newline=0;

static void lavrec_msg(int type, char *str1, char *str2)
{
   char *ctype;

   if(type==LAVREC_PROGRESS)
   {
      printf("%s   \r",str1);
      fflush(stdout);
      need_newline=1;
   }
   else
   {
      switch(type)
      {
          case LAVREC_INTERNAL: ctype = "Internal Error"; break;
          case LAVREC_DEBUG:    ctype = "Debug";          break;
          case LAVREC_INFO:     ctype = "Info";           break;
          case LAVREC_WARNING:  ctype = "Warning";        break;
          case LAVREC_ERROR:    ctype = "Error";          break;
          default:              ctype = "Unkown";
      }
      if(need_newline) printf("\n");
      printf("%s: %s\n",ctype,str1);
      if(str2[0]) printf("%s: %s\n",ctype,str2);
      need_newline=0;
   }
}

/* system_error: report an error from a system call */

static void system_error(char *str1,char *str2)
{
   sprintf(infostring,"Error %s (in %s)",str1,str2);
   lavrec_msg(LAVREC_ERROR,infostring,strerror(errno));
   exit(1);
}

static int mixer_set = 0;
static int mixer_volume_saved = 0;
static int mixer_recsrc_saved = 0;
static int mixer_inplev_saved = 0;

/*
   Set the sound mixer:
   flag = 1 : set for recording from the line input
   flag = 0 : restore previously saved values
*/

void set_mixer(int flag)
{
   int fd, recsrc, level, status, numerr;
   int sound_mixer_read_input;
   int sound_mixer_write_input;
   int sound_mask_input;
   char *mixer_dev_name;

   /* Avoid restoring anything when nothing was set */

   if (flag==0 && mixer_set==0) return;

   mixer_dev_name = getenv("LAV_MIXER_DEV");
   if(!mixer_dev_name) mixer_dev_name = "/dev/mixer";
   fd = open(mixer_dev_name, O_RDONLY);
   if (fd == -1)
   {
      sprintf(infostring,"Unable to open sound mixer %s", mixer_dev_name);
      lavrec_msg(LAVREC_WARNING, infostring,
                 "Try setting the sound mixer with another tool!!!");
      return;
   }

   mixer_set = 1;

   switch(audio_recsrc)
   {
      case 'l':
         sound_mixer_read_input  = SOUND_MIXER_READ_LINE;
         sound_mixer_write_input = SOUND_MIXER_WRITE_LINE;
         sound_mask_input        = SOUND_MASK_LINE;
         break;
      case 'm':
         sound_mixer_read_input  = SOUND_MIXER_READ_MIC;
         sound_mixer_write_input = SOUND_MIXER_WRITE_MIC;
         sound_mask_input        = SOUND_MASK_MIC;
         break;
      case 'c':
         sound_mixer_read_input  = SOUND_MIXER_READ_CD;
         sound_mixer_write_input = SOUND_MIXER_WRITE_CD;
         sound_mask_input        = SOUND_MASK_CD;
         break;
   }

   if(flag)
   {
      /* Save the values we are going to change */

      numerr = 0;
      status = ioctl(fd, SOUND_MIXER_READ_VOLUME, &mixer_volume_saved);
      if (status == -1) numerr++;
      status = ioctl(fd, SOUND_MIXER_READ_RECSRC, &mixer_recsrc_saved);
      if (status == -1) numerr++;
      status = ioctl(fd, sound_mixer_read_input , &mixer_inplev_saved);
      if (status == -1) numerr++;
      if (numerr) 
      {
         lavrec_msg(LAVREC_WARNING,
                    "Unable to save sound mixer settings",
                    "Restore your favorite setting with another tool after capture");
         mixer_set = 0; /* Avoid restoring the wrong values */
      }

      /* Set the recording source to the line input, 
         the level of the line input to audio_lev,
         the output volume to zero (to avoid audio feedback
         when using a camera build in microphone */

      numerr = 0;

      recsrc = sound_mask_input;
      status = ioctl(fd, SOUND_MIXER_WRITE_RECSRC, &recsrc);
      if (status == -1) numerr++;

      level = 256*audio_lev + audio_lev; /* left and right channel */
      status = ioctl(fd, sound_mixer_write_input, &level);
      if (status == -1) numerr++;

      if(audio_mute)
      {
         level = 0;
         status = ioctl(fd, SOUND_MIXER_WRITE_VOLUME, &level);
         if (status == -1) numerr++;
      }

      if (numerr) 
      {
         lavrec_msg(LAVREC_WARNING,
                    "Unable to set the sound mixer correctly",
                    "Audio capture might not be successfull (try another mixer tool!)");
      }
   }
   else
   {
      /* Restore previously saved settings */

      numerr = 0;
      status = ioctl(fd, SOUND_MIXER_WRITE_RECSRC, &mixer_recsrc_saved);
      if (status == -1) numerr++;
      status = ioctl(fd, sound_mixer_write_input,  &mixer_inplev_saved);
      if (status == -1) numerr++;
      if(audio_mute)
      {
         status = ioctl(fd, SOUND_MIXER_WRITE_VOLUME, &mixer_volume_saved);
         if (status == -1) numerr++;
      }

      if (numerr) 
      {
         lavrec_msg(LAVREC_WARNING,
                    "Unable to restore sound mixer settings",
                    "Restore your favorite setting with another tool");
      }
   }

   close(fd);

}

/* Shut down the audio task and reset the mixers.
   CleanUpAudio is only called indirectly through atexit()
*/

void CleanUpAudio(void)
{
   if(audio_size) audio_shutdown();
   set_mixer(0);
}

/* The signal handler only sets a flag which makes the main program
   to exit the video recording loop.
   This way we avoid race conditions when ^C is pressed during
   writing to the AVI File
*/

static int VideoExitFlag = 0;

void SigHandler(int sig_num)
{
   VideoExitFlag = 1;
}

void Usage(char *progname)
{
   fprintf(stderr, "Usage: %s [options] <filename> [<filename> ...]\n", progname);
   fprintf(stderr, "where options are:\n");
   fprintf(stderr, "   -f [aAqm]   Format AVI/Quicktime/movtar\n");
   fprintf(stderr, "   -i [pPnNat] Input Source\n");
   fprintf(stderr, "   -d num     Decimation (either 1,2,4 or two digit number)\n");
   fprintf(stderr, "   -g WxH+X+Y X-style geometry string (part of 720x576/480 field)\n");
   fprintf(stderr, "   -q num     Quality [%%]\n");
   fprintf(stderr, "   -t num     Capture time\n");
   fprintf(stderr, "   -S         Single frame capture mode\n");
   fprintf(stderr, "   -T num     Time lapse, capture only every <num>th frame\n");
   fprintf(stderr, "   -w         Wait for user confirmation to start\n");
   fprintf(stderr, "   -a num     Audio size, 0 for no audio, 8 or 16\n");
   fprintf(stderr, "   -r num     Audio rate [Hz]\n");
   fprintf(stderr, "   -s         Stereo\n");
   fprintf(stderr, "   -l num     Recording level [%%], -1 for mixers not touched\n");
   fprintf(stderr, "   -m         Mute audio output during recording\n");
   fprintf(stderr, "   -R [lmc]   Set recording source: (l)ine, (m)icro, (c)d\n");
   fprintf(stderr, "   -c [012]   Level of corrections for synchronization\n");
   fprintf(stderr, "   -n num     # of MJPEG buffers\n");
   fprintf(stderr, "   -b num     Size of MJPEG buffers [Kb]\n");
   fprintf(stderr, "   -v num     verbose level\n");
   fprintf(stderr, "Environment variables recognized:\n");
   fprintf(stderr, "   LAV_VIDEO_DEV, LAV_AUDIO_DEV, LAV_MIXER_DEV\n");
   exit(1);
}

/* RJ: The following stuff thanks to Philipp Zabel: */

/* pH5 - the following was stolen from glut (that stole the code from X):   */

/* 
 * Bitmask returned by XParseGeometry().  Each bit tells if the corresponding
 * value (x, y, width, height) was found in the parsed string.
 */
#define NoValue         0x0000
#define XValue          0x0001
#define YValue          0x0002
#define WidthValue      0x0004
#define HeightValue     0x0008
#define AllValues       0x000F
#define XNegative       0x0010
#define YNegative       0x0020

/* the following function was stolen from the X sources as indicated. */

/* Copyright 	Massachusetts Institute of Technology  1985, 1986, 1987 */
/* $XConsortium: XParseGeom.c,v 11.18 91/02/21 17:23:05 rws Exp $ */

/*
Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation, and that the name of M.I.T. not be used in advertising or
publicity pertaining to distribution of the software without specific,
written prior permission.  M.I.T. makes no representations about the
suitability of this software for any purpose.  It is provided "as is"
without express or implied warranty.
*/

/*
 *    XParseGeometry parses strings of the form
 *   "=<width>x<height>{+-}<xoffset>{+-}<yoffset>", where
 *   width, height, xoffset, and yoffset are unsigned integers.
 *   Example:  "=80x24+300-49"
 *   The equal sign is optional.
 *   It returns a bitmask that indicates which of the four values
 *   were actually found in the string.  For each value found,
 *   the corresponding argument is updated;  for each value
 *   not found, the corresponding argument is left unchanged. 
 */

static int
ReadInteger(char *string, char **NextString)
{
    register int Result = 0;
    int Sign = 1;
    
    if (*string == '+')
	string++;
    else if (*string == '-')
    {
	string++;
	Sign = -1;
    }
    for (; (*string >= '0') && (*string <= '9'); string++)
    {
	Result = (Result * 10) + (*string - '0');
    }
    *NextString = string;
    if (Sign >= 0)
	return (Result);
    else
	return (-Result);
}

int XParseGeometry(char *string, int *x, int *y, unsigned int *width, unsigned int *height)
{
	int mask = NoValue;
	register char *strind;
	unsigned int tempWidth, tempHeight;
	int tempX, tempY;
	char *nextCharacter;

	if ( (string == NULL) || (*string == '\0')) return(mask);
	if (*string == '=')
		string++;  /* ignore possible '=' at beg of geometry spec */

	strind = (char *)string;
	if (*strind != '+' && *strind != '-' && *strind != 'x') {
		tempWidth = ReadInteger(strind, &nextCharacter);
		if (strind == nextCharacter) 
		    return (0);
		strind = nextCharacter;
		mask |= WidthValue;
	}

	if (*strind == 'x' || *strind == 'X') {	
		strind++;
		tempHeight = ReadInteger(strind, &nextCharacter);
		if (strind == nextCharacter)
		    return (0);
		strind = nextCharacter;
		mask |= HeightValue;
	}

	if ((*strind == '+') || (*strind == '-')) {
		if (*strind == '-') {
  			strind++;
			tempX = -ReadInteger(strind, &nextCharacter);
			if (strind == nextCharacter)
			    return (0);
			strind = nextCharacter;
			mask |= XNegative;

		}
		else
		{	strind++;
			tempX = ReadInteger(strind, &nextCharacter);
			if (strind == nextCharacter)
			    return(0);
			strind = nextCharacter;
		}
		mask |= XValue;
		if ((*strind == '+') || (*strind == '-')) {
			if (*strind == '-') {
				strind++;
				tempY = -ReadInteger(strind, &nextCharacter);
				if (strind == nextCharacter)
			    	    return(0);
				strind = nextCharacter;
				mask |= YNegative;

			}
			else
			{
				strind++;
				tempY = ReadInteger(strind, &nextCharacter);
				if (strind == nextCharacter)
			    	    return(0);
				strind = nextCharacter;
			}
			mask |= YValue;
		}
	}
	
	/* If strind isn't at the end of the string the it's an invalid
		geometry specification. */

	if (*strind != '\0') return (0);

	if (mask & XValue)
	    *x = tempX;
 	if (mask & YValue)
	    *y = tempY;
	if (mask & WidthValue)
            *width = tempWidth;
	if (mask & HeightValue)
            *height = tempHeight;
	return (mask);
}

/* ===========================================================================
   Parse the X-style geometry string
*/

int parse_geometry (char *geom, int *x, int *y, int *width, int *height)
{
   return XParseGeometry (geom, x, y, width, height);
}

#define MAX_MBYTES_PER_FILE 1600  /* Maximum number of Mbytes per file.
                                     We make a conservative guess since we
                                     only count the number of bytes output
                                     and don't know how big the control
                                     information will be */
#define MIN_MBYTES_FREE 10        /* Minimum number of Mbytes that should
                                     stay free on the file system, this is also
                                     only a guess */
#define MIN_MBYTES_FREE_OPEN 20   /* Minimum number of Mbytes that have to be
                                     free in the filesystem when opening a new file */
#define CHECK_INTERVAL 50         /* Interval for checking free space on file system */

static int output_status;       /* 0: no output file opened yet
                                   1: audio and video to the same file
                                      (or no audio present)
                                   2: video to new file, audio fills up old file
                                   3: audio fills up last file, video output disabled */
static int cur_out_file;        /* Number of current output file (for video) */
static int num_out_files;       /* Total number of files to write,
                                   if num_out_files==0 use pattern */
static char **out_file_list;    /* List of output file names */
static char out_filename[1024]; /* Name of current output file (for video) */

static long bytes_output_cur;   /* Bytes output to the current output file */
static long bytes_last_checked; /* Number of bytes that were output when the
                                   free space was last checked */
static long MBytes_fs_free;     /* MBytes free when free space was last checked */

static unsigned long num_frames; /* Number of frames output so far */
static unsigned long num_asamps; /* Number of audio samples output so far */
static unsigned long num_frames_old; /* Number of frames output when we switched
                                        from old to new file  */

static double spvf;   /* seconds per video frame */
static double spas;   /* seconds per audio sample */


static void get_free_space()
{
   int res;
   long blocks_per_MB;
   struct statfs statfs_buf;

   /* check the disk space again */
   res = statfs(out_filename,&statfs_buf);
   if(res)
   {
      /* some error happened */
      MBytes_fs_free = MAX_MBYTES_PER_FILE; /* some fake value */
   }
   else
   {
      blocks_per_MB = (1024*1024) / statfs_buf.f_bsize;
      MBytes_fs_free = statfs_buf.f_bavail/blocks_per_MB;
   }
   bytes_last_checked = bytes_output_cur;
}

/*
   close_files_on_error: Close the output file(s) if an error occured.
                         We don't care about further errors.
 */
                         
static void close_files_on_error()
{
   int res;

   if(output_status > 0) res = lav_close(video_file);
   if(output_status > 1) res = lav_close(video_file_old);

   lavrec_msg(LAVREC_WARNING,"Trying to close output file(s) and exiting",
                             "Output file(s) my not be readable due to error");
}

#define OUTPUT_VIDEO_ERROR_RETURN \
if(output_status==2) \
{ \
   output_status = 3; \
   return 0; \
} \
else \
   return 1;

/* output_video_frame outputs a video frame and does all the file handling
   necessary like opening new files and closing old ones.
   It returns 0 if all is o.k and -1 or 1 if we should exit immediatly.
 */

static int output_video_frame(char *buff, long size, long count)
{
   int res, n;
   int OpenNewFlag=0;

   if(output_status == 3) return 0; /* Only audio is still active */

   /* Check space on filesystem if we have filled it up
      or if we have written more than CHECK_INTERVAL bytes since last check */

   if( output_status>0 )
   {
      n = (bytes_output_cur - bytes_last_checked)>>20; /* in MBytes */
      if( n > CHECK_INTERVAL || n > MBytes_fs_free-MIN_MBYTES_FREE )
         get_free_space();
   }

   /* Check if it is time to exit */

   if(VideoExitFlag) lavrec_msg(LAVREC_INFO,"Signal caught, exiting","");
   if (num_frames*spvf > record_time)
   {
      lavrec_msg(LAVREC_INFO,"Recording time reached, exiting","");
      VideoExitFlag = 1;
   }

   /* Check if we have to open a new output file */

   if( output_status>0 && (bytes_output_cur>>20) > MAX_MBYTES_PER_FILE)
   {
      lavrec_msg(LAVREC_INFO,"Max filesize reached, opening next output file","");
      OpenNewFlag = 1;
   }
   if( output_status>0 && MBytes_fs_free < MIN_MBYTES_FREE)
   {
      lavrec_msg(LAVREC_INFO,"File system is nearly full, "
                             "trying to open next output file","");
      OpenNewFlag = 1;
   }

   /* If a file is open and we should open a new one or exit,
      close current file */

   if(output_status>0 && (OpenNewFlag || VideoExitFlag) )
   {
      if (audio_size)
      {
         /* Audio is running - flag that the old file should be closed */
         if(output_status != 1)
         {
            /* There happened something bad - the old output file from the
               last file change is not closed.
               We try to close all files and exit */
            lavrec_msg(LAVREC_ERROR,"Error: Audio too far behind video",
                                    "Check if audio works correctly!");
            close_files_on_error();
            return -1;
         }
         lavrec_msg(LAVREC_DEBUG,"Closing current output file for video, "
                                 "waiting for audio to be filled","");
         video_file_old = video_file;
         num_frames_old = num_frames;
         if (VideoExitFlag)
         {
            output_status = 3;
            return 0;
         }
         else
            output_status = 2;
      }
      else
      {
         res = lav_close(video_file);
         if(res)
         {
            sprintf(infostring,"Error closing video output file %s, "
                               "may be unuseable due to error",out_filename);
            lavrec_msg(LAVREC_ERROR,infostring,lav_strerror());
            return res;
         }
         if (VideoExitFlag) return 1;
      }
   }

   /* Open new output file if needed */

   if(output_status==0 || OpenNewFlag )
   {
      /* Get next filename */

      if(num_out_files==0)
      {
         sprintf(out_filename,out_file_list[0],++cur_out_file);
      }
      else
      {
         if(cur_out_file>=num_out_files)
         {
            lavrec_msg(LAVREC_WARNING,"Number of given output files reached","");
            OUTPUT_VIDEO_ERROR_RETURN
         }
         strncpy(out_filename,out_file_list[cur_out_file++],sizeof(out_filename));
      }
      sprintf(infostring,"Opening output file %s",out_filename);
      lavrec_msg(LAVREC_INFO,infostring,"");
         
      /* Open next file */

      video_file = lav_open_output_file(out_filename,video_format,
                      width,height,interlaced,
                      (norm==VIDEO_MODE_PAL? 25.0 : 30000.0/1001.0),
                      audio_size,(stereo ? 2 : 1),audio_rate);
      if(!video_file)
      {
         sprintf(infostring,"Error opening output file %s",out_filename);
         lavrec_msg(LAVREC_ERROR,infostring,lav_strerror());
         OUTPUT_VIDEO_ERROR_RETURN
      }

      if(output_status==0) output_status = 1;

      /* Check space on filesystem. Exit if not enough space */

      bytes_output_cur = 0;
      get_free_space();
      if(MBytes_fs_free < MIN_MBYTES_FREE_OPEN)
      {
         lavrec_msg(LAVREC_ERROR,"Not enough space for opening new output file","");
         /* try to close and remove file, don't care about errors */
         res = lav_close(video_file);
         res = remove(out_filename);
         OUTPUT_VIDEO_ERROR_RETURN
      }
   }

   /* Output the frame count times */

   res = lav_write_frame(video_file,buff,size,count);

   /* If an error happened, try to close output files and exit */

   if(res)
   {
      sprintf(infostring,"Error writing to output file %s",out_filename);
      lavrec_msg(LAVREC_ERROR,infostring,lav_strerror());
      close_files_on_error();
      return res;
   }

   /* Update counters. Maybe frame its written only once,
     but size*count is the save guess */

   bytes_output_cur += size*count;+
   num_frames += count;

   return 0;
}

static int output_audio_to_file(char *buff, long samps, int old)
{
   int res;

   if(samps==0) return 0;

   /* Output data */

   res = lav_write_audio(old?video_file_old:video_file,buff,samps);

   /* If an error happened, try to close output files and exit */

   if(res)
   {
      lavrec_msg(LAVREC_ERROR,"Error writing to output file",lav_strerror());
      close_files_on_error();
      return res;
   }

   /* update counters */

   num_asamps += samps;
   if(!old) bytes_output_cur += samps*audio_bps;

   return 0;
}

static int output_audio_samples(char *buff, long samps)
{
   int res;
   long diff;

   /* Safety first */

   if(!output_status)
   {
      lavrec_msg(LAVREC_INTERNAL,"Output audio but no file open","");
      return -1;
   }

   if(output_status<2)
   {
      /* Normal mode, just output the sample */
      res = output_audio_to_file(buff,samps,0);
      return res;
   }

   /* if we come here, we have to fill up the old file first */

   diff  = (num_frames_old*spvf - num_asamps*spas)*audio_rate;
   
   if(diff<0)
   {
      lavrec_msg(LAVREC_INTERNAL,"Audio output ahead video output","");
      return -1;
   }

   if(diff >= samps)
   {
      /* All goes to old file */
      res = output_audio_to_file(buff,samps,1);
      return res;
   }

   /* diff samples go to old file */

   res = output_audio_to_file(buff,diff,1);
   if(res) return res;

   /* close old file */

   lavrec_msg(LAVREC_DEBUG,"Audio is filled","");
   res = lav_close(video_file_old);

   if(res)
   {
      lavrec_msg(LAVREC_ERROR,
                "Error closing video output file, may be unuseable due to error",
                lav_strerror());
      return res;
   }

   /* Check if we are ready */

   if(output_status==3) return 1;

   /* remaining samples go to new file */

   output_status = 1;
   res = output_audio_to_file(buff+diff*audio_bps,samps-diff,0);
   return res;
}

int main(int argc, char ** argv)
{
   int video_dev;
   int res;
   struct mjpeg_status bstat;
   struct mjpeg_params bparm;
   struct mjpeg_requestbuffers breq;
   struct mjpeg_sync bsync;
   struct timeval first_time;
   struct timeval audio_t0;
   struct timeval audio_tmstmp;
   char * MJPG_buff;
   char AUDIO_buff[8192];
   long audio_offset, nb;
   int write_frame;
   char input_buffer[256];
   int i, n, nerr, nfout;
   unsigned long first_lost, num_lost;
   unsigned long num_syncs;
   unsigned long num_ins, num_del;
   unsigned long num_aerr;
   int astat;
   long audio_buffer_size;
   double time;
   double tdiff1, tdiff2;
   char *video_dev_name;

   /* check usage */
   if (argc < 2)  Usage(argv[0]);

   /* Get options */

   nerr = 0;
   while( (n=getopt(argc,argv,"v:f:i:d:g:q:t:ST:wa:r:sl:mR:c:n:b:")) != EOF)
   {
      switch(n) {
         case 'f':
            video_format = optarg[0];
            if(optarg[0]!='a' && optarg[0]!='A' && optarg[0]!='q' && optarg[0]!='m')
            {
               fprintf(stderr,"Format (-f option) must be a, A, q or m\n");
               nerr++;
            }
            break;

         case 'i':
            input_source = optarg[0];
            break;

         case 'd':
            i = atoi(optarg);
            if(i<10)
            {
               hordcm = verdcm = i;
            }
            else
            {
               hordcm = i/10;
               verdcm = i%10;
            }
            if( (hordcm != 1 && hordcm != 2 && hordcm != 4) ||
                (verdcm != 1 && verdcm != 2 && verdcm != 4) )
            {
               fprintf(stderr,"decimation = %d invalid\n",i);
               fprintf(stderr,"   must be one of 1,2,4,11,12,14,21,22,24,41,42,44\n");
               nerr++;
            }
            break;

         case 'g':
            geom_flags = parse_geometry (optarg, &geom_x, &geom_y, &geom_width, &geom_height);
            /* RJ: We check for errors later since the allowed range of params
               dependes on other settings not yet known */
            break;

         case 'q':
            quality = atoi(optarg);
            if(quality<0 || quality>100)
            {
               fprintf(stderr,"quality = %d invalid (must be 0 ... 100)\n",quality);
               nerr++;
            }
            break;

         case 't':
            record_time = atoi(optarg);
            if(record_time<=0)
            {
               fprintf(stderr,"record_time = %d invalid\n",record_time);
               nerr++;
            }
            break;

         case 'S':
            single_frame = 1;
            break;

         case 'T':
            time_lapse = atoi(optarg);
            if(time_lapse<=1) time_lapse = 1;
            break;

         case 'w':
            wait_for_start = 1;
            break;

         case 'a':
            audio_size = atoi(optarg);
            if(audio_size != 0 && audio_size != 8 && audio_size != 16)
            {
               fprintf(stderr,"audio_size = %d invalid (must be 0, 8 or 16)\n",
                              audio_size);
               nerr++;
            }
            break;

         case 'r':
            audio_rate = atoi(optarg);
            if(audio_rate<=0)
            {
               fprintf(stderr,"audio_rate = %d invalid\n",audio_rate);
               nerr++;
            }
            break;

         case 's':
            stereo = 1;
            break;

         case 'l':
            audio_lev = atoi(optarg);
            if(audio_lev<-1 || audio_lev>100)
            {
               fprintf(stderr,"recording level = %d invalid (must be 0 ... 100 or -1)\n",
                              audio_lev);
               nerr++;
            }
            break;

         case 'm':
            audio_mute = 1;
            break;

         case 'R':
            audio_recsrc = optarg[0];
            if(audio_recsrc!='l' && audio_recsrc!='m' && audio_recsrc!='c')
            {
               fprintf(stderr,"Recording source (-R param) must be l,m or c\n");
               nerr++;
            }
            break;

         case 'c':
            sync_corr = atoi(optarg);
            if(sync_corr<0 || sync_corr>2)
            {
               fprintf(stderr,"parameter -c %d invalid (must be 0, 1 or 2)\n",sync_corr);
               nerr++;
            }
            break;

         case 'n':
            MJPG_nbufs = atoi(optarg);
            break;

         case 'b':
            MJPG_bufsize = atoi(optarg);
            break;

	case 'v':
	  verbose = atoi(optarg);
	  break;

         default:
            nerr++;
            break;
      }
   }

   if(optind>=argc) nerr++;

   if(nerr) Usage(argv[0]);

   num_out_files = argc - optind;
   out_file_list = argv + optind;
   cur_out_file  = 0; /* Not yet opened */
   output_status = 0;

   /* If the first filename contains a '%', the user wants file patterns */

   if(strstr(argv[optind],"%")) num_out_files = 0;

   /* Special settings for single frame captures */

   if(single_frame)
   {
      audio_size = 0;
      MJPG_nbufs = 4;
   }

   /* time lapse also doesn't want audio */

   if(time_lapse>1) audio_size = 0;

   if (verbose > 1) {
     printf("\nRecording parameters:\n\n");
     printf("Output format:      %s\n",video_format=='q'?"Quicktime":"AVI");
     printf("Input Source:       ");
     switch(input_source)
     {
        case 'p': printf("Composite PAL\n"); break;
        case 'P': printf("S-VHS PAL\n"); break;
        case 'n': printf("Composite NTSC\n"); break;
        case 'N': printf("S-VHS NTSC\n"); break;
        case 't': printf("TV tuner\n"); break;
        default:  printf("Auto detect\n");
     }
     if(hordcm==verdcm)
        printf("Decimation:         %d\n",hordcm);
     else
        printf("Decimation:         %d (hor) x %d (ver)\n",hordcm,verdcm);
     printf("Quality:            %d\n",quality);
     printf("Recording time:     %d sec\n",record_time);
     if(time_lapse>1)
        printf("Time lapse factor:  %d\n",time_lapse);

     printf("\n");
     printf("MJPEG buffer size:  %d KB\n",MJPG_bufsize);
     printf("# of MJPEG buffers: %d\n",MJPG_nbufs);
     if(audio_size)
     {
        printf("\nAudio parameters:\n\n");
        printf("Audio sample size:           %d bit\n",audio_size);
        printf("Audio sampling rate:         %d Hz\n",audio_rate);
        printf("Audio is %s\n",stereo ? "STEREO" : "MONO");
        if(audio_lev!=-1)
        {
           printf("Audio input recording level: %d %%\n",audio_lev);
           printf("%s audio output during recording\n",audio_mute?"Mute":"Don\'t mute");
           printf("Recording source: %c\n",audio_recsrc);
        }
        else
           printf("Audio input recording level: Use mixer setting\n");
        printf("Level of correction for Audio/Video synchronization:\n");
        switch(sync_corr)
        {
           case 0: printf("No lost frame compensation, No frame drop/insert\n"); break;
           case 1: printf("Lost frame compensation, No frame drop/insert\n"); break;
           case 2: printf("Lost frame compensation and frame drop/insert\n"); break;
        }
     }
     else
        printf("\nAudio disabled\n\n");
     printf("\n");
   }
   /* Flush the Linux File buffers to disk */

   sync();

   /* set the sound mixer */

   if(audio_size && audio_lev>=0) set_mixer(1);

   /* Initialize the audio system if audio is wanted.
      This involves a fork of the audio task and is done before
      the video device and the output file is opened */

   audio_bps = 0;
   if (audio_size)
   {
      res = audio_init(1,stereo,audio_size,audio_rate);
      if(res)
      {
         set_mixer(0);
         lavrec_msg(LAVREC_ERROR,"Error initializing Audio",audio_strerror());
         exit(1);
      }
      audio_bps = audio_size/8;
      if(stereo) audio_bps *= 2;
      audio_buffer_size = audio_get_buffer_size();
   }

   /* The audio system needs a exit processing (audio_shutdown()),
      the mixers also should be reset at exit. */

   atexit(CleanUpAudio);

   /* open the video device */

   video_dev_name = getenv("LAV_VIDEO_DEV");
   if(!video_dev_name) video_dev_name = "/dev/video";
   video_dev = open(video_dev_name, O_RDONLY);
   if (video_dev < 0) system_error(video_dev_name,"open");

   /* Set input and norm according to input_source,
      do an auto detect if neccessary */

   switch(input_source)
   {
      case 'p': input = 0; norm = 0; break;
      case 'P': input = 1; norm = 0; break;
      case 'n': input = 0; norm = 1; break;
      case 'N': input = 1; norm = 1; break;
      case 't': input = 2; norm = 0; break;
      default:
         n = 0;
	 if (verbose > 1)
           lavrec_msg(LAVREC_INFO,"Auto detecting input and norm ...","");
         for(i=0;i<2;i++)
         {
	    if (verbose > 1) {
              sprintf(infostring,"Trying %s ...",(i==2) ? "TV tuner" : (i==0?"Composite":"S-Video"));
              lavrec_msg(LAVREC_INFO,infostring,"");
	    }
            bstat.input = i;
            res = ioctl(video_dev,MJPIOC_G_STATUS,&bstat);
            if(res<0) system_error("getting input status","ioctl MJPIOC_G_STATUS");
            if(bstat.signal)
            {
		if (verbose > 1) {
                  sprintf(infostring,"input present: %s %s",bstat.norm?"NTSC":"PAL",
                                                         bstat.color?"color":"no color");
                  lavrec_msg(LAVREC_INFO,infostring,"");
		}
               input = i;
               norm = bstat.norm;
               n++;
            }
            else
               lavrec_msg(LAVREC_INFO,"NO signal","");
         }
         switch(n)
         {
            case 0:
               lavrec_msg(LAVREC_ERROR,"No input signal ... exiting","");
               exit(1);
            case 1:
	       if (verbose > 1) {	
                 sprintf(infostring,"Detected %s %s",norm?"NTSC":"PAL",
                                                   input==0?"Composite":"S-Video");
                 lavrec_msg(LAVREC_INFO,infostring,"");
	       }
               break;
            case 2:
               lavrec_msg(LAVREC_ERROR,"Input signal on Composite AND S-Video ... exiting","");
               exit(1);
         }
   }


   /* Query and set params for capture */

   res = ioctl(video_dev, MJPIOC_G_PARAMS, &bparm);
   if(res<0) system_error("getting video parameters","ioctl MJPIOC_G_PARAMS");

   bparm.input      = input;
   bparm.norm       = norm;
   bparm.decimation = 0;
   bparm.quality    = quality;

   /* Set decimation and image geometry params */

   bparm.HorDcm         = hordcm;
   bparm.VerDcm         = (verdcm==4) ? 2 : 1;
   bparm.TmpDcm         = (verdcm==1) ? 1 : 2;
   bparm.field_per_buff = (verdcm==1) ? 2 : 1;

   bparm.img_width      = (hordcm==1) ? 720 : 704;
   bparm.img_height     = (norm==1)   ? 240 : 288;

   if (geom_width>720) {
      lavrec_msg(LAVREC_ERROR,"Image width too big! Exiting.","");
      exit(1);
   }
   if ((geom_width%(bparm.HorDcm*16))!=0) {
      sprintf(infostring,"Image width not multiple of %d! Exiting",bparm.HorDcm*16);
      lavrec_msg(LAVREC_ERROR,infostring,"");
      exit(1);
   }
   if (geom_height>(norm==1 ? 480 : 576)) {
      lavrec_msg(LAVREC_ERROR,"Image height too big! Exiting.","");
      exit(1);
   }
   /* RJ: Image height must only be a multiple of 8, but geom_height
          is double the field height */
   if ((geom_height%(bparm.VerDcm*16))!=0) {
      sprintf(infostring,"Image height not multiple of %d! Exiting",bparm.VerDcm*16);
      lavrec_msg(LAVREC_ERROR,infostring,"");
      exit(1);
   }

   if(geom_flags&WidthValue)  bparm.img_width  = geom_width;
   if(geom_flags&HeightValue) bparm.img_height = geom_height/2;

   if(geom_flags&XValue)
      bparm.img_x = geom_x;
   else
      bparm.img_x = (720 - bparm.img_width)/2;

   if(geom_flags&YValue)
      bparm.img_y = geom_y/2;
   else
      bparm.img_y = ( (norm==1 ? 240 : 288) - bparm.img_height)/2;


   /* Care about field polarity and APP Markers which are needed for AVI
      and Quicktime and may be for other video formats as well */

   if(verdcm > 1)
   {
      /* for vertical decimation > 1 no known video format needs app markers,
         we need also not to care about field polarity */

      bparm.APP_len = 0; /* No markers */
   }
   else
   {
      bparm.APPn     = lav_query_APP_marker(video_format);
      bparm.APP_len  = lav_query_APP_length(video_format);
      /* There seems to be some confusion about what is the even and odd field ... */
      bparm.odd_even = lav_query_polarity(video_format) == LAV_INTER_EVEN_FIRST;
      for(n=0; n<bparm.APP_len && n<60; n++) bparm.APP_data[n] = 0;
   }

   res = ioctl(video_dev, MJPIOC_S_PARAMS, &bparm);
   if(res<0) system_error("setting video parameters","ioctl MJPIOC_S_PARAMS");

   width  = bparm.img_width/bparm.HorDcm;
   height = bparm.img_height/bparm.VerDcm*bparm.field_per_buff;
   interlaced = (bparm.field_per_buff>1);

   if (verbose > 1) {
     sprintf(infostring,"Image size will be %dx%d, %d field(s) per buffer",
                      width, height, bparm.field_per_buff);
     lavrec_msg(LAVREC_INFO,infostring,"");
   }

   /* Request buffers */

   breq.count = MJPG_nbufs;
   breq.size  = MJPG_bufsize*1024;
   res = ioctl(video_dev, MJPIOC_REQBUFS,&breq);
   if(res<0) system_error("requesting video buffers","ioctl MJPIOC_REQBUFS");

   if (verbose > 1) { 
     sprintf(infostring,"Got %ld buffers of size %ld KB",breq.count,breq.size/1024);
     lavrec_msg(LAVREC_INFO,infostring,"");
   }

   /* Map the buffers */

   MJPG_buff = mmap(0, breq.count*breq.size, PROT_READ, MAP_SHARED, video_dev, 0);
   if (MJPG_buff == MAP_FAILED) system_error("mapping video buffers","mmap");

   /* Assure proper exit handling if the user presses ^C during recording */

   signal(SIGINT,SigHandler);

   /* Try to get a reliable timestamp for Audio */

   if (audio_size && sync_corr>1)
   {
     if (verbose > 1) {
       printf("Getting audio ... \n");
     }
      for(n=0;;n++)
      {
         if(n>NUM_AUDIO_TRIES)
         {
            lavrec_msg(LAVREC_ERROR,"Unable to get audio - exiting ....","");
            exit(1);
         }
         res = audio_read(AUDIO_buff,sizeof(AUDIO_buff),0,&audio_t0,&astat);
         if(res<0)
         {
            lavrec_msg(LAVREC_ERROR,"Error reading audio",audio_strerror());
            exit(1);
         }
         if(res && audio_t0.tv_sec ) break;
         usleep(20000);
      }
   }
         
   /* For single frame recording: Make stdin nonblocking */

   if(single_frame || wait_for_start)
   {
      res = fcntl(0,F_SETFL,O_NONBLOCK);
      if (res<0) system_error("making stdin nonblocking","fcntl F_SETFL");
   }

   /* We are set up now - wait for user confirmation if wanted */

   if(wait_for_start)
   {
      printf("\nPress enter to start recording>");
      fflush(stdout);
      while(1)
      {
         usleep(20000);
         res = read(0,input_buffer,256);
         if(res>0) break; /* Got Return */
         if(VideoExitFlag) exit(0); /* caught signal */

         /* Audio (if on) is allready running, empty buffer to avoid overflow */

         if (audio_size)
         {
            while( (res=audio_read(AUDIO_buff,sizeof(AUDIO_buff),0,&audio_t0,&astat)) >0 ) /*noop*/;
            if(res==0) continue;
            if(res<0)
            {
               lavrec_msg(LAVREC_ERROR,"Error reading audio",audio_strerror());
               exit(1);
            }
         }
      }
   }

   /* Queue all buffers, this also starts streaming capture */

   for(n=0;n<breq.count;n++)
   {
      res = ioctl(video_dev, MJPIOC_QBUF_CAPT, &n);
      if (res<0) system_error("queuing buffers","ioctl MJPIOC_QBUF_CAPT");
   }

   /* The video capture loop */

   write_frame = 1;

   num_syncs  = 0; /* Number of MJPIOC_SYNC ioctls            */
   num_lost   = 0; /* Number of frames lost                   */
   num_frames = 0; /* Number of frames written to file        */
   num_asamps = 0; /* Number of audio samples written to file */
   num_ins    = 0; /* Number of frames inserted for sync      */
   num_del    = 0; /* Number of frames deleted for sync       */
   num_aerr   = 0; /* Number of audio buffers in error        */

   /* Seconds per video frame: */
   spvf = (norm==VIDEO_MODE_PAL) ? 0.040 : 1001./30000.;

   /* Seconds per audio sample: */
   if(audio_size)
      spas = 1.0/audio_rate;
   else
      spas = 0.;

   tdiff1 = 0.;
   tdiff2 = 0.;

   while (1)
   {
      /* sync on a frame */
      res = ioctl(video_dev, MJPIOC_SYNC, &bsync);
      if (res < 0)
      {
         close_files_on_error();
         system_error("syncing on a buffer","ioctl MJPIOC_SYNC");
      }
      num_syncs++;

      if(num_syncs==1)
      {
         first_time = bsync.timestamp;
         first_lost = bsync.seq;
         if(audio_size && sync_corr>1)
         {
            /* Get time difference beetween audio and video in bytes */
            audio_offset  = ((first_time.tv_usec-audio_t0.tv_usec)*1.e-6 +
                             first_time.tv_sec-audio_t0.tv_sec - spvf)*audio_rate;
            audio_offset *= audio_bps;   /* convert to bytes */
         }
         else
            audio_offset = 0;
      }

      time = bsync.timestamp.tv_sec - first_time.tv_sec
           + 1.e-6*(bsync.timestamp.tv_usec - first_time.tv_usec)
           + spvf; /* for first frame */

      if(single_frame)
      {
         if(write_frame==1) /* first time here or frame written in last loop cycle */
         {
            printf("%6ld frames, press enter>",num_frames);
            fflush(stdout);
         }
         res = read(0,input_buffer,256);
         if(res>0)
            write_frame = 1;
         else
            write_frame = 0;
         nfout = 1; /* always output frame only once */
      }
      else if(time_lapse>1)
      {
         write_frame = (num_syncs % time_lapse) == 0;
         nfout = 1; /* always output frame only once */
      }
      else /* normal capture */
      {
         nfout = 1;
         n = bsync.seq - num_syncs - first_lost + 1; /* total lost frames */
         if(sync_corr>0) nfout += n - num_lost; /* lost since last sync */
         num_lost = n;

         /* Check if we have to insert/delete frames to stay in sync */

         if(sync_corr>1)
         {
            if( tdiff1-tdiff2 < -spvf)
            {
               nfout++;
               num_ins++;
               tdiff1 += spvf;
            }
            if( tdiff1-tdiff2 > spvf)
            {
               nfout--;
               num_del++;
               tdiff1 -= spvf;
            }
         }
      }

      /* write it out */

      if(write_frame && nfout>0)
      {
         res = output_video_frame(MJPG_buff+bsync.frame*breq.size, bsync.length, nfout);
         if(res) break; /* Done or error occured */
      }

      /* Re-queue the buffer */

      res = ioctl(video_dev, MJPIOC_QBUF_CAPT, &bsync.frame);
      if (res < 0)
      {
         close_files_on_error();
         system_error("re-queuing buffer","ioctl MJPIOC_QBUF_CAPT");
      }

      /* Output statistics */

      if(!single_frame && output_status<3)
      {
         int nf, ns, nm, nh;
         if(norm==VIDEO_MODE_PAL)
         {
            nf = num_frames % 25;
            ns = num_frames / 25;
         }
         else
         {
            nf = num_frames % 30;
            ns = num_frames / 30;
         }
         nm = ns / 60;
         ns = ns % 60;
         nh = nm / 60;
         nm = nm % 60;
	 if (verbose > 0) {
           sprintf(infostring,"time:%2d.%2.2d.%2.2d:%2.2d lost:%4lu ins:%3lu del:%3lu "
                            "audio errs:%3lu tdiff=%10.6f",
                nh, nm, ns, nf, num_lost, num_ins, num_del, num_aerr, tdiff1-tdiff2);
           lavrec_msg(LAVREC_PROGRESS,infostring,"");
	 }
      }


      /* Care about audio */

      if (!audio_size) continue;

      res = 0;

      while(1)
      {

         /* Only try to read a audio sample if video is ahead - else we might
            get into difficulties when writing the last samples */

         if(output_status < 3 && 
            num_frames*spvf < (num_asamps+audio_buffer_size/audio_bps)*spas) break;

         nb=audio_read(AUDIO_buff,sizeof(AUDIO_buff),0,&audio_tmstmp,&astat);
         if(nb==0) break;

         if(nb<0)
         {
            lavrec_msg(LAVREC_ERROR,"Error reading audio",audio_strerror());
            close_files_on_error();
            res = -1;
            break;
         }

         if(!astat) num_aerr++;

         /* Adjust for difference at start */

         if(audio_offset>=nb) { audio_offset -= nb; continue; }
         nb -= audio_offset;

         /* Got an audio sample, write it out */

         res = output_audio_samples(AUDIO_buff+audio_offset,nb/audio_bps);
         if(res) break; /* Done or error occured */
         audio_offset = 0;

         /* calculate time differences beetween audio and video
            tdiff1 is the difference according to the number of frames/samples written
            tdiff2 is the difference according to the timestamps
           (only if audio timestamp is not zero) */

         if(audio_tmstmp.tv_sec)
         {
            tdiff1 = num_frames*spvf - num_asamps*spas;
            tdiff2 = (bsync.timestamp.tv_sec -audio_tmstmp.tv_sec )
                   + (bsync.timestamp.tv_usec-audio_tmstmp.tv_usec)*1.e-6;
         }
      }
      if (res) break;
   }

   /* Audio and mixer exit processing is done with atexit() */

   if(res>=0) {
      if (verbose > 1) {
        lavrec_msg(LAVREC_INFO,"Clean exit ...","");
      }
   } else
      lavrec_msg(LAVREC_INFO,"Error exit ...","");
   exit(0);
}

