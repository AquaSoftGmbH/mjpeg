/*
 * lavrec - Linux Audio Video RECord
 *
 * Copyright (C) 2000 Rainer Johanni <Rainer@Johanni.de>
 * Extended by: Gernot Ziegler <gz@lysator.liu.se>
 * Patched '2000 by Wolfgang Scherr <scherr@net4you.net>
 * Works fine with Miro DC10(plus), too.
 *
 *
 * Capture motion video from the IOMEGA Buz to an AVI or Quicktime
 * file together with capturing audio from the soundcard.
 *
 * Usage: lavrec [options] filename [filename ...]
 * where options are as follows:
 *
 *   -f/--format [aAqm] --- Output file format:
 *      'a': AVI (default)
 *      'A': AVI with fields exchanged
 *      'q': quicktime (if compiled with quicktime support)
 *      'm': movtar (if compiled with movtar support)
 *      Hint: If your AVI video looks strange, try 'A' instead 'a'
 *      and vice versa.
 *		 
 *   -i/--input [pPnNsStTa] --- Input Source:
 *      'p': PAL       Composite Input
 *      'P': PAL       SVHS-Input
 *      'n': NTSC      Composite Input
 *      'N': NTSC      SVHS-Input
 *      's': SECAM     Composite Input
 *      'S': SECAM     SVHS-Input
 *      't': PAL/SECAM TV tuner input (if available)
 *      'T': NTSC      TV tuner input (if available)
 *      'a': (or every other letter) Autosense (default)
 *
 *   -d/--decimation num --- Frame recording decimation:
 *      must be either 1, 2 or 4 for identical decimation
 *      in horizontal and vertical direction (mostly used) or a
 *      two digit letter with the first digit specifying horizontal
 *      decimation and the second digit specifying vertical decimation
 *      (more exotic usages).
 *
 *   -g/--geometry WxH+X+Y --- An X-style geometry string (capturing area):
 *      Even if a decimation > 1 is used, these values are always
 *      coordinates in the undecimated frame.  
 *                For DC10: 768x{576 or 480}.
 *                For others: 720x{576 or 480}.
 *      Also, unlike in X-Window, negative values for X and Y
 *      really mean negative offsets (if this feature is enabled
 *      in the driver) which lets you fine tune the position of the
 *      image caught.
 *      The horizontal resolution of the DECIMATED frame must
 *      allways be a multiple of 16, the vertical resolution
 *      of the DECIMATED frame must be a multiple of 16 for decimation 1
 *      and a multiple of 8 for decimations 2 and 4
 *
 *      If not offset (X and Y values) is given, the capture area
 *      is centered in the frame.
 *
 *   -q/--quality num --- quality:
 *      must be between 0 and 100, default is 50
 *
 *   -t/--time num -- capturing time:
 *      Time to capture in seconds, default is unlimited
 *      (use ^C to stop capture!)
 *
 *   -S/--single-frame --- enables single-frame capturing mode
 *
 *   -T/--time-lapse num --- time-lapse mode:
 *      Time lapse factor: Video will be played back <num> times
 *      faster, audio is switched off.
 *      This means that only every <num>th frame is recorded.
 *      If num==1 it is silently ignored.
 *
 *   -w/--wait --- Wait for user confirmation to start
 *
 **** Audio settings ***
 *
 *   -a/--audio-bitsize num --- audio bitsize:
 *      Audio size in bits, must be 0 (no audio), 8 or 16 (default)
 *
 *   -r/--audio_bitrate num --- audio-bitrate:
 *      Audio rate (in Hz), must be a permitted sampling rate for
 *      your soundcard. Default is 22050.
 *
 *   -s/--stereo --- enable stereo (disabled by default)
 *
 *   -l/--audio-volume num --- audio recording level/volume:
 *      Audio level to use for recording, must be between 0 and 100
 *      or -1 (for not touching the mixer settings at all), default
 *      is 100.
 *
 *   -m/--mute --- mute output during recording:
 *      Mute audio output during recording (default is to let it enabled).
 *      This is particularly usefull if you are recording with a
 *      microphone to avoid feedback.
 *
 *   -R/--audio-source [lmc] --- Audio recording source:
 *      'l': line-in
 *      'm': microphone
 *      'c': cdrom
 *
 **** Audio/Video synchronization ***
 *
 *   -c/--synchronization num --- Level of corrections for synchronization:
 *      0: Neither try to replicate lost frames nor any sync correction
 *      1: Replicate frames for lost frames, but no sync correction
 *      2. lost frames replication + sync correction
 *
 **** Special capture settings ***
 *
 *   -n/--mjpeg-buffers num --- Number of MJPEG capture buffers (default 64)
 *
 *   -b/--mjpeg-buffer-size num --- Size of MJPEG buffers in KB (default 256)
 *
 **** Environment variables ***
 *
 * Recognized environment variables:
 *    LAV_VIDEO_DEV: Name of video device (default: "/dev/video")
 *    LAV_AUDIO_DEV: Name of audio device (default: "/dev/dsp")
 *    LAV_MIXER_DEV: Name of mixer device (default: "/dev/mixer")
 *
 * To overcome the AVI (and ext2fs) 2 GB filesize limit, you can:
 *   - give multiple filenames on the command-line
 *   - give one filename which contains a '%' sign. This name is then
 *     interpreted as the format argument to sprintf() to form multiple
 *     file names
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <config.h>
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
#include <sys/resource.h>
#include <sys/vfs.h>
#include <signal.h>
#include <stdlib.h>
#include <getopt.h>
#include <linux/videodev.h>
#include <linux/soundcard.h>
#include <videodev_mjpeg.h>

#include "frequencies.h"
#include "audiolib.h"
#include "lav_io.h"
#include "mjpeg_logging.h"

/************************** DEFINES **************************/
/* On some systems MAP_FAILED seems to be missing */
#ifndef MAP_FAILED
#define MAP_FAILED ( (caddr_t) -1 )
#endif

#define NUM_AUDIO_TRIES 500 /* makes 10 seconds with 20 ms pause beetween tries */

#define LAVREC_INTERNAL 0
#define LAVREC_DEBUG    1
#define LAVREC_INFO     2
#define LAVREC_WARNING  3
#define LAVREC_ERROR    4
#define LAVREC_PROGRESS 5

#define AUDIO_BUFFER_SIZE 8192

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



char *audio_strerror(void);
void set_mixer(int flag);
void CleanUpAudio(void);
void SigHandler(int sig_num);
void Usage(char *progname);
int XParseGeometry(char *string, int *x, int *y, unsigned int *width, unsigned int *height);
int parse_geometry (char *geom, int *x, int *y, int *width, int *height);


/************************** VARIABLES **************************/
/* defaults */
static char video_format   = 'a';    /* 'a' or 'A' for AVI, 'q' for Quicktime */
static char input_source   = 'a';    /* 'a' for Auto detect, others see above */
static int  hordcm         = 4;      /* horizontal decimation */
static int  verdcm         = 4;      /* vertical decimation */
static int  geom_x         = 0;
static int  geom_y         = 0;      /* x geometry string:  "WxH+X+Y" */
static int  geom_width     = 0;
static int  geom_height    = 0;
static int  geom_flags     = 0;      /* flags what has been read */
static int  quality        = 50;     /* determines target size of JPEG code,
                                        100=max. quality */
static int  record_time    = 100000; /* "unlimited" */
static int  single_frame   = 0;      /* continous capture */
static int  opt_use_read   = 0;      /* Use read instead of mmap for capture */
static int  time_lapse     = 1;      /* no time lapse */
static int  wait_for_start = 0;      /* Wait for user confirmation to start */
static int  audio_size     = 16;     /* Audio sample size: 8/16 bits, 0 = no audio */
static int  audio_rate     = 44100;  /* sampling rate for audio */
static int  stereo         = 0;      /* 0: capture mono, 1: capture stereo */
static int  audio_lev      = 100;    /* Level of Audio input, 0..100: Recording level
                                        -1:  don't change mixer settings */
static int  audio_mute     = 0;      /* Flag for muting audio output */
static int  audio_recsrc   = 'l';    /* Audio recording source */
static int  sync_corr      = 2;      /* Level of corrections for synchronization */
static int  MJPG_nbufs     = 64;     /* Number of MJPEG buffers */
static int  MJPG_bufsize   = 256;    /* Size   of MJPEG buffers */
static int  interlaced;
static int  verbose        = 1;

static lav_file_t *video_file, *video_file_old;
static int width, height;
static int input, norm;
static int audio_bps; /* audio bytes per sample */
static long audio_buffer_size = 0;
struct video_audio vau; /* For setting audio properties when using a tuner */
static char *norm_name[] = {"PAL", "NTSC", "SECAM"};
static int mixer_set = 0;
static int tuner_freq = 0;
static int mixer_volume_saved = 0;
static int mixer_recsrc_saved = 0;
static int mixer_inplev_saved = 0;
static char infostring[4096];

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

/****************** PROGRAM CODE ******************/

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
		mjpeg_warn("Unable to open sound mixer %s\n"
				   "Try setting the sound mixer with another tool!!!",
				   mixer_dev_name);
		return;
	}

	mixer_set = 1;

	switch(audio_recsrc)
	{
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
	case 'l':
	default :
		sound_mixer_read_input  = SOUND_MIXER_READ_LINE;
		sound_mixer_write_input = SOUND_MIXER_WRITE_LINE;
		sound_mask_input        = SOUND_MASK_LINE;
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
			mjpeg_warn("Unable to save sound mixer settings\n");
			mjpeg_warn("Restore your favorite setting with another tool after capture\n");
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
			mjpeg_warn("Unable to set the sound mixer correctly\n");
			mjpeg_warn("Audio capture might not be successfull (try another mixer tool!)\n");
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
			mjpeg_warn( "Unable to restore sound mixer settings\n" );
			mjpeg_warn( "Restore your favorite setting with another tool\n");
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
	fprintf(stderr, "lavtools version " VERSION ": lavrec\n");
	fprintf(stderr, "Usage: %s [options] <filename> [<filename> ...]\n", progname);
	fprintf(stderr, "where options are:\n");
	fprintf(stderr, "  -f/--format [aAqm]          Format AVI/Quicktime/movtar\n");
	fprintf(stderr, "  -i/--input [pPnNsStTa]      Input Source\n");
	fprintf(stderr, "  -d/--decimation num         Decimation (either 1,2,4 or two digit number)\n");
	fprintf(stderr, "  -g/--geometry WxH+X+Y       X-style geometry string (part of 768/720x576/480)\n");
	fprintf(stderr, "  -q/--quality num            Quality [%%]\n");
	fprintf(stderr, "  -t/--time num               Capture time (default: unlimited - Ctrl-C to stop\n");
	fprintf(stderr, "  -S/--single-frame           Single frame capture mode\n");
	fprintf(stderr, "  -T/--time-lapse num         Time lapse, capture only every <num>th frame\n");
	fprintf(stderr, "  -w/--wait                   Wait for user confirmation to start\n");
	fprintf(stderr, "  -a/--audio-bitsize num      Audio size, 0 for no audio, 8 or 16\n");
	fprintf(stderr, "  -r/--audio-bitrate num      Audio rate [Hz]\n");
	fprintf(stderr, "  -s/--stereo                 Stereo (default: mono)\n");
	fprintf(stderr, "  -l/--audio-volume num       Recording level [%%], -1 for mixers not touched\n");
	fprintf(stderr, "  -m/--mute                   Mute audio output during recording\n");
	fprintf(stderr, "  -R/--audio-source [lmc]     Set recording source: (l)ine, (m)icro, (c)d\n");
	fprintf(stderr, "  -c/--synchronization [012]  Level of corrections for synchronization\n");
	fprintf(stderr, "  -n/--mjpeg-buffers num      Number of MJPEG buffers (default: 64)\n");
	fprintf(stderr, "  -b/--mjpeg-buffer-size num  Size of MJPEG buffers [Kb] (default: 256)\n");
	fprintf(stderr, "  -C/--channel LIST:CHAN      When using a TV tuner, channel list/number\n");
	fprintf(stderr, "  -U/--use-read               Use read instead of mmap for recording\n");
	fprintf(stderr, "  -v/--verbose [012]          verbose level (default: 0)\n");
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
	tempWidth = 0;
	if (*strind != '+' && *strind != '-' && *strind != 'x') {
		tempWidth = ReadInteger(strind, &nextCharacter);
		if (strind == nextCharacter) 
		    return (0);
		strind = nextCharacter;
		mask |= WidthValue;
	}

	tempHeight = 0;
	if (*strind == 'x' || *strind == 'X') {	
		strind++;
		tempHeight = ReadInteger(strind, &nextCharacter);
		if (strind == nextCharacter)
		    return (0);
		strind = nextCharacter;
		mask |= HeightValue;
	}

	tempX = tempY = 0;
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

	mjpeg_warn( "Closing file(s) and exiting - "
				"output file(s) my not be readable due to error\n");
}

#define OUTPUT_VIDEO_ERROR_RETURN \
if(output_status==2) \
{ \
   output_status = 3; \
   return 0; \
} \
else \
   return 1

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

	if(VideoExitFlag) 
		mjpeg_info("Signal caught, exiting\n");
	if (num_frames*spvf > record_time)
	{
		mjpeg_info("Recording time reached, exiting\n");
		VideoExitFlag = 1;
	}

	/* Check if we have to open a new output file */

	if( output_status>0 && (bytes_output_cur>>20) > MAX_MBYTES_PER_FILE)
	{
		mjpeg_info("Max filesize reached, opening next output file\n");
		OpenNewFlag = 1;
	}
	if( output_status>0 && MBytes_fs_free < MIN_MBYTES_FREE)
	{
		mjpeg_info("File system is nearly full, "
				   "trying to open next output file\n");
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
				mjpeg_error("Audio too far behind video"
							"Check if audio works correctly!\n");
				close_files_on_error();
				return -1;
			}
			mjpeg_debug("Closing current output file for video, "
						"waiting for audio to be filled\n");
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
				mjpeg_error("Closing video output file %s, "
							"may be unuseable due to error\n",out_filename);
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
				mjpeg_warn("Number of given output files reached\n");
				OUTPUT_VIDEO_ERROR_RETURN;
			}
			strncpy(out_filename,out_file_list[cur_out_file++],sizeof(out_filename));
		}
		mjpeg_info("Opening output file %s\n",out_filename);
         
		/* Open next file */

		video_file = lav_open_output_file(out_filename,video_format,
			width,height,interlaced,
			(norm==VIDEO_MODE_NTSC? 30000.0/1001.0 : 25.0),
			audio_size,(stereo ? 2 : 1),audio_rate);
		if(!video_file)
		{
			mjpeg_error("Opening output file %s: %s\n",out_filename,lav_strerror());
			OUTPUT_VIDEO_ERROR_RETURN;
		}


		if(output_status==0) output_status = 1;

		/* Check space on filesystem. Exit if not enough space */

		bytes_output_cur = 0;
		get_free_space();
		if(MBytes_fs_free < MIN_MBYTES_FREE_OPEN)
		{
			mjpeg_error("Not enough space for opening new output file\n");
			/* try to close and remove file, don't care about errors */
			res = lav_close(video_file);
			res = remove(out_filename);
			OUTPUT_VIDEO_ERROR_RETURN;
		}
	}

	/* Output the frame count times */

	res = lav_write_frame(video_file,buff,size,count);

	/* If an error happened, try to close output files and exit */

	if(res)
	{
		mjpeg_error("Error writing to output file %s: %s\n",
					out_filename,lav_strerror());
		close_files_on_error();
		return res;
	}

	/* Update counters. Maybe frame its written only once,
	   but size*count is the save guess */

	bytes_output_cur += size*count; 
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
		mjpeg_error("Error writing to output file: %s\n",lav_strerror());
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
		mjpeg_error("INTERNAL:Output audio but no file open\n");
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
		mjpeg_error("INTERNAL:Audio output ahead video output\n");
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

	mjpeg_debug("Audio is filled\n");
	res = lav_close(video_file_old);

	if(res)
	{
		mjpeg_error(
				   "Error closing video output file, may be unuseable due to error: %s\n",
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


/* (Ronald) The following functions are all (almost directly) copied
   from main(), taken out of there in order to:
   - Make main() smaller
   - Make main() smaller
*/
static int set_option(const char *name, char *value)
{
	/* return 1 means error, return 0 means okay */
	int nerr = 0;

	if (strcmp(name, "format")==0 || strcmp(name, "f")==0)
	{
		video_format = value[0];
		if(value[0]!='a' && value[0]!='A' && value[0]!='q' && value[0]!='m')
		{
			mjpeg_error("Format (-f/--format) must be a, A, q or m\n");
			nerr++;
		}
	}
	else if (strcmp(name, "input")==0 || strcmp(name, "i")==0)
	{
		input_source = value[0];
	}
	else if (strcmp(name, "decimation")==0 || strcmp(name, "d")==0)
	{
		int i = atoi(value);
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
			mjpeg_error("decimation (-d/--decimation) = %d invalid\n",i);
			mjpeg_error("   must be one of 1,2,4,11,12,14,21,22,24,41,42,44\n");
			nerr++;
		}
	}
	else if (strcmp(name, "verbose")==0 || strcmp(name, "v")==0)
	{
		verbose = atoi(value);
	}
	else if (strcmp(name, "quality")==0 || strcmp(name, "q")==0)
	{
		quality = atoi(value);
		if(quality<0 || quality>100)
		{
			mjpeg_error("quality (-q/--quality) = %d invalid (must be 0 ... 100)\n",quality);
			nerr++;
		}
	}
	else if (strcmp(name, "geometry")==0 || strcmp(name, "g")==0)
	{
		geom_flags = parse_geometry (value,
			&geom_x, &geom_y, &geom_width, &geom_height);
	}
	else if (strcmp(name, "time")==0 || strcmp(name, "t")==0)
	{
		record_time = atoi(value);
		if(record_time<=0)
		{
			mjpeg_error("record_time (-t/--time) = %d invalid\n",record_time);
			nerr++;
		}
	}
	else if (strcmp(name, "single-frame")==0 || strcmp(name, "S")==0)
	{
		single_frame = 1;
	}
	else if (strcmp(name, "use-read")==0 || strcmp(name, "U")==0)
	{
		opt_use_read = 1;
	}
	else if (strcmp(name, "time-lapse")==0 || strcmp(name, "T")==0)
	{
		time_lapse = atoi(value);
		if(time_lapse<=1) time_lapse = 1;
	}
	else if (strcmp(name, "wait")==0 || strcmp(name, "w")==0)
	{
		wait_for_start = 1;
	}
	else if (strcmp(name, "audio-bitsize")==0 || strcmp(name, "a")==0)
	{
		audio_size = atoi(value);
		if(audio_size != 0 && audio_size != 8 && audio_size != 16)
		{
			mjpeg_error("audio_size (-a/--audio-bitsize) ="
				" %d invalid (must be 0, 8 or 16)\n",
				audio_size);
			nerr++;
		}
	}
	else if (strcmp(name, "audio-bitrate")==0 || strcmp(name, "r")==0)
	{
		audio_rate = atoi(value);
		if(audio_rate<=0)
		{
			mjpeg_error("audio_rate (-r/--audio-bitrate) = %d invalid\n",audio_rate);
			nerr++;
		}
	}
	else if (strcmp(name, "stereo")==0 || strcmp(name, "s")==0)
	{
		stereo = 1;
	}
	else if (strcmp(name, "audio-volume")==0 || strcmp(name, "l")==0)
	{
		audio_lev = atoi(value);
		if(audio_lev<-1 || audio_lev>100)
		{
			mjpeg_error("recording level (-l/--audio-volume)"
				" = %d invalid (must be 0 ... 100 or -1)\n",
				audio_lev);
			nerr++;
		}
	}
	else if (strcmp(name, "mute")==0 || strcmp(name, "m")==0)
	{
		audio_mute = 1;
	}
	else if (strcmp(name, "audio-source")==0 || strcmp(name, "R")==0)
	{
		audio_recsrc = value[0];
		if(audio_recsrc!='l' && audio_recsrc!='m' && audio_recsrc!='c')
		{
			mjpeg_error("Recording source (-R/--audio-source)"
				" must be l,m or c\n");
			nerr++;
		}
	}
	else if (strcmp(name, "synchronization")==0 || strcmp(name, "c")==0)
	{
		sync_corr = atoi(value);
		if(sync_corr<0 || sync_corr>2)
		{
			mjpeg_error("Synchronization (-c/--synchronization) ="
				" %d invalid (must be 0, 1 or 2)\n",sync_corr);
			nerr++;
		}
	}
	else if (strcmp(name, "mjpeg-buffers")==0 || strcmp(name, "n")==0)
	{
		MJPG_nbufs = atoi(value);
	}
	else if (strcmp(name, "mjpeg-buffer-size")==0 || strcmp(name, "b")==0)
	{
		MJPG_bufsize = atoi(value);
	}
	else if (strcmp(name, "channel")==0 || strcmp(name,"C")==0)
	{
		int colin=0; int chlist=0; int chan=0;
		while ( value[colin] && value[colin]!=':') colin++;
		if (value[colin]==0) /* didn't find the colin */
		{
			mjpeg_error("malformed channel parameter (-C/--channel)\n");
			nerr++;
		}
		if (nerr==0) 
			while (strncmp(chanlists[chlist].name,value,colin)!=0)
			{
				if (chanlists[chlist++].name==NULL)
				{
					mjpeg_error("bad frequency map\n");
					nerr++;
				}
			}
		if (nerr==0) 
			while (strcmp((chanlists[chlist].list)[chan].name,  &(value[colin+1]))!=0)
			{
				if ((chanlists[chlist].list)[chan++].name==NULL)
				{
					mjpeg_error("bad channel spec\n");
					nerr++;
				}
			}
		if (nerr==0) tuner_freq=(chanlists[chlist].list)[chan].freq;
	}
	else nerr++; /* unknown option - error */

	return nerr;
}

static void check_command_line_options(int argc, char *argv[])
{
	int n, nerr, option_index = 0;
	char option[2];

	/* getopt_long options */
	static struct option long_options[]={
		{"format"           ,1,0,0},   /* -f/--format            */
		{"input"            ,1,0,0},   /* -i/--input             */
		{"decimation"       ,1,0,0},   /* -d/--decimation        */
		{"verbose"          ,1,0,0},   /* -v/--verbose           */
		{"quality"          ,1,0,0},   /* -q/--quality           */
		{"geometry"         ,1,0,0},   /* -g/--geometry          */
		{"time"             ,1,0,0},   /* -t/--time              */
		{"single-frame"     ,1,0,0},   /* -S/--single-frame      */
		{"time-lapse"       ,1,0,0},   /* -T/--time-lapse        */
		{"wait"             ,0,0,0},   /* -w/--wait              */
		{"audio-bitsize"    ,1,0,0},   /* -a/--audio-size        */
		{"audio-bitrate"    ,1,0,0},   /* -r/--audio-rate        */
		{"stereo"           ,0,0,0},   /* -s/--stereo            */
		{"audio-volume"     ,1,0,0},   /* -l/--audio-volume      */
		{"mute"             ,0,0,0},   /* -m/--mute              */
		{"audio-source"     ,1,0,0},   /* -R/--audio-source      */
		{"synchronization"  ,1,0,0},   /* -c/--synchronization   */
		{"mjpeg-buffers"    ,1,0,0},   /* -n/--mjpeg_buffers     */
		{"mjpeg-buffer-size",1,0,0},   /* -b/--mjpeg-buffer-size */
		{"channel"          ,1,0,0},   /* -C/--channel           */
		{"use-read"         ,0,0,0},   /* --use-read           */
		{0,0,0,0}
	};

	/* check usage */
	if (argc < 2)  Usage(argv[0]);

	/* Get options */
	nerr = 0;
	while( (n=getopt_long(argc,argv,"v:f:i:d:g:q:t:ST:wa:r:sl:mUR:c:n:b:C:",
		long_options, &option_index)) != EOF)
	{
		switch(n)
		{
			/* getopt_long values */
			case 0:
				nerr += set_option(long_options[option_index].name,
					optarg);
				break;
			/* These are the old getopt-values (non-long) */
			default:
				sprintf(option, "%c", n);
				nerr += set_option(option, optarg);
				break;
		}
	}
	if(optind>=argc) nerr++;
	if(nerr) Usage(argv[0]);

	mjpeg_default_handler_verbosity(verbose);
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
	if(time_lapse > 1) audio_size = 0;
}


/* Simply prints recording parameters */
static void lavrec_print_properties()
{
	char *source;
	mjpeg_info("Recording parameters:\n\n");
	mjpeg_info("Output format:      %s\n",video_format=='q'?"Quicktime":"AVI");
	switch(input_source)
	{
	case 'p': source = "Composite PAL\n"; break;
	case 'P': source = "S-Video PAL\n"; break;
	case 'n': source = "Composite NTSC\n"; break;
        case 'N': source = "S-Video NTSC\n"; break;
	case 's': source = "Composite SECAM\n"; break;
	case 'S': source = "S-Video SECAM\n"; break;
	case 't': source = "PAL TV tuner\n"; break;
	case 'T': source = "NTSC TV tuner\n"; break;
	default:  source = "Auto detect\n";
	}
	mjpeg_info("Input Source:       %s\n", source);

	if(hordcm==verdcm)
		mjpeg_info("Decimation:         %d\n",hordcm);
	else
		mjpeg_info("Decimation:         %d (hor) x %d (ver)\n",hordcm,verdcm);
	
	mjpeg_info("Quality:            %d\n",quality);
	mjpeg_info("Recording time:     %d sec\n",record_time);
	if(time_lapse>1)
		mjpeg_info("Time lapse factor:  %d\n",time_lapse);
	
	mjpeg_info("\n");
	mjpeg_info("MJPEG buffer size:  %d KB\n",MJPG_bufsize);
	mjpeg_info("# of MJPEG buffers: %d\n",MJPG_nbufs);
	if(audio_size)
	{
		mjpeg_info("Audio parameters:\n\n");
		mjpeg_info("Audio sample size:           %d bit\n",audio_size);
		mjpeg_info("Audio sampling rate:         %d Hz\n",audio_rate);
		mjpeg_info("Audio is %s\n",stereo ? "STEREO" : "MONO");
		if(audio_lev!=-1)
		{
			mjpeg_info("Audio input recording level: %d %%\n",audio_lev);
			mjpeg_info("%s audio output during recording\n",
					   audio_mute?"Mute":"Don\'t mute");
			mjpeg_info("Recording source: %c\n",audio_recsrc);
		}
			else
				mjpeg_info("Audio input recording level: Use mixer setting\n");
		mjpeg_info("Level of correction for Audio/Video synchronization:\n");
		switch(sync_corr)
		{
		case 0: mjpeg_info("No lost frame compensation, No frame drop/insert\n");
			break;
		case 1: mjpeg_info("Lost frame compensation, No frame drop/insert\n"); 
			break;
		case 2: mjpeg_info("Lost frame compensation and frame drop/insert\n"); 
			break;
		}
	}
	else
		mjpeg_info("Audio disabled\n\n");
}

static void lavrec_setup(int *p_video_dev, struct mjpeg_requestbuffers *breq,
	char **MJPG_buff, char *AUDIO_buff, struct timeval *audio_t0, int *astat)
{
	struct mjpeg_status bstat;
	struct video_capability vc;
	struct video_channel vch;
	struct mjpeg_params bparm;
	int device_width,i,n,res;
	char *video_dev_name;
	int video_dev;

	/* set the sound mixer */
	if(audio_size && audio_lev>=0) set_mixer(1);

	/* Initialize the audio system if audio is wanted.
	   This involves a fork of the audio task and is done before
	   the video device and the output file is opened */
	audio_bps = 0;
	if (audio_size)
	{
		res = audio_init(1,opt_use_read,stereo,audio_size,audio_rate);
		if(res)
		{
			set_mixer(0);
			mjpeg_error_exit1("Error initializing Audio: %s\n",audio_strerror());
		}
		audio_bps = audio_size/8;
		if(stereo) audio_bps *= 2;
		audio_buffer_size = audio_get_buffer_size();
	}

	/* After we have fired up the audio system (which is assisted if we're
	   installed setuid root, we want to set the effective user id to the
	   real user id */
	if( seteuid( getuid() ) < 0 )
	mjpeg_error("Can't set effective user-id: %s\n ",
				(char *)sys_errlist[errno]);

	/* The audio system needs a exit processing (audio_shutdown()),
	   the mixers also should be reset at exit. */
	atexit(CleanUpAudio);

	/* open the video device */
	video_dev_name = getenv("LAV_VIDEO_DEV");
	if(!video_dev_name) video_dev_name = "/dev/video";
	video_dev = open(video_dev_name, O_RDONLY);
	if (video_dev < 0) mjpeg_error_exit1("opening videodev : %s\n",
										 sys_errlist[errno]);

	/* Set input and norm according to input_source,
	   do an auto detect if neccessary */
	switch(input_source)
	{
	/* Maybe we should switch these over */
	case 'p': input = 0; norm = VIDEO_MODE_PAL; break;
	case 'P': input = 1; norm = VIDEO_MODE_PAL; break;
	case 'n': input = 0; norm = VIDEO_MODE_NTSC; break;
	case 'N': input = 1; norm = VIDEO_MODE_NTSC; break;
	case 's': input = 0; norm = VIDEO_MODE_SECAM; break;
	case 'S': input = 1; norm = VIDEO_MODE_SECAM; break;
	case 't': input = 2; norm = VIDEO_MODE_PAL; break;
	case 'T': input = 2; norm = VIDEO_MODE_NTSC; break;
	default:
		n = 0;
		mjpeg_info("Auto detecting input and norm ...\n");
		for(i=0;i<2;i++)
		{
            mjpeg_info("Trying %s ...\n",(i==2) ? "TV tuner" : (i==0?"Composite":"S-Video"));
            bstat.input = i;
            res = ioctl(video_dev,MJPIOC_G_STATUS,&bstat);
            if(res<0) mjpeg_error_exit1("Getting video input status:%s\n",sys_errlist[errno]);
            if(bstat.signal)
            {
                mjpeg_info("input present: %s %s\n",
						   norm_name[bstat.norm],
						   bstat.color?"color":"no color");
				input = i;
				norm = bstat.norm;
				n++;
            }
            else
				mjpeg_info("NO signal ion specified input");
		}
		switch(n)
		{
		case 0:
			mjpeg_error_exit1("No input signal ... exiting\n");
		case 1:
			mjpeg_info("Detected %s %s\n",
					   norm_name[norm],
					   input==0?"Composite":"S-Video");
			break;
		case 2:
			mjpeg_error_exit1("Input signal on Composite AND S-Video ... exiting\n");
		}
	}

	/* Set input and norm first so we can determine width*/
	vch.channel = input;
	vch.norm    = norm;
	res = ioctl(video_dev, VIDIOCSCHAN,&vch);
	if(res<0) mjpeg_error("Setting channel: %s\n",sys_errlist[errno]);

	/* set channel if we're tuning */
	if (input ==2 && tuner_freq!=0)
	{
		unsigned long outfreq;
		outfreq = tuner_freq*16/1000;
		res = ioctl(video_dev, VIDIOCSFREQ,&outfreq);
		if(res<0) mjpeg_error("setting tuner frequency: %s\n",sys_errlist[errno]);
	}

	/* Set up tuner audio if this is a tuner. I think this should be done
	 * AFTER the tuner device is selected */
	if (input == 2) 
	{
		/* get current */
		res = ioctl(video_dev,VIDIOCGAUDIO,&vau);
		if(res<0) mjpeg_error("getting tuner audio params:%s\n",sys_errlist[errno]);
		/* unmute so we get sound to record */
		/* this is done without checking current state because the
		 * current mga driver doesn't report mute state accurately */
		mjpeg_info("Unmuting tuner audio...\n");
		vau.flags &= (~VIDEO_AUDIO_MUTE);
		res = ioctl(video_dev,VIDIOCSAUDIO,&vau);
		if(res<0) mjpeg_error("setting tuner audio params:%s\n",sys_errlist[errno]);
	}

   
	/* Determine device pixel width (DC10=768, BUZ=720 for PAL/SECAM, DC10=640, BUZ=720) */
   
	res = ioctl(video_dev, VIDIOCGCAP,&vc);
	if (res < 0) 
		mjpeg_error_exit1("getting device capabilities: %s\n",sys_errlist[errno]);
	device_width=vc.maxwidth;

	/* Query and set params for capture */

	res = ioctl(video_dev, MJPIOC_G_PARAMS, &bparm);
	if(res<0) 
		mjpeg_error_exit1("getting video parameters: %s\n",sys_errlist[errno]);
	bparm.input = input;
	bparm.norm = norm;

	bparm.decimation = 0;
	bparm.quality    = quality;

	/* Set decimation and image geometry params */
	bparm.HorDcm         = hordcm;
	bparm.VerDcm         = (verdcm==4) ? 2 : 1;
	bparm.TmpDcm         = (verdcm==1) ? 1 : 2;
	bparm.field_per_buff = (verdcm==1) ? 2 : 1;

	/* If device is DC10 then do handle special else do the the
	 * normal thing
	 */
	if (device_width == 768 || device_width == 640)
		bparm.img_width = (hordcm==1) ? device_width : (device_width-64);
	else
	{
		bparm.img_width = (hordcm==1) ? 720 : 704;
		device_width = 720;
	}
	
	bparm.img_height     = (norm==1)   ? 240 : 288;

	if (geom_width>device_width) 
	{
		mjpeg_error_exit1("Image width too big! Exiting.");
	}
	if ((geom_width%(bparm.HorDcm*16))!=0) 
	{
		mjpeg_error_exit1("Image width not multiple of %d! Exiting",bparm.HorDcm*16);
	}
	if (geom_height>(norm==1 ? 480 : 576)) 
	{
		mjpeg_error_exit1("Image height too big! Exiting.");
	}

	/* RJ: Image height must only be a multiple of 8, but geom_height
	   is double the field height */
	if ((geom_height%(bparm.VerDcm*16))!=0) 
	{
		mjpeg_error_exit1("Image height not multiple of %d! Exiting",bparm.VerDcm*16);
	}

	if(geom_flags&WidthValue)  bparm.img_width  = geom_width;
	if(geom_flags&HeightValue) bparm.img_height = geom_height/2;

	if(geom_flags&XValue)
		bparm.img_x = geom_x;
	else
		bparm.img_x = (device_width - bparm.img_width)/2;

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
		bparm.odd_even = lav_query_polarity(video_format) == LAV_INTER_ODD_FIRST;
		for(n=0; n<bparm.APP_len && n<60; n++) bparm.APP_data[n] = 0;
	}

	res = ioctl(video_dev, MJPIOC_S_PARAMS, &bparm);
	if(res<0) 
		mjpeg_error_exit1("setting video parameters:%s\n",sys_errlist[errno]);

	width  = bparm.img_width/bparm.HorDcm;
	height = bparm.img_height/bparm.VerDcm*bparm.field_per_buff;
	interlaced = (bparm.field_per_buff>1);

	mjpeg_info("Image size will be %dx%d, %d field(s) per buffer\n",
			   width, height, bparm.field_per_buff);

	/* Request buffers */
	breq->count = MJPG_nbufs;
	breq->size  = MJPG_bufsize*1024;
	res = ioctl(video_dev, MJPIOC_REQBUFS,breq);
	if(res<0) 
		mjpeg_error_exit1("requesting video buffers:%s\n", sys_errlist[errno]);

	mjpeg_info("Got %ld buffers of size %ld KB\n",breq->count,breq->size/1024);


	/* Map the buffers */

	*MJPG_buff = mmap(0, breq->count*breq->size, 
					 PROT_READ, MAP_SHARED, video_dev, 0);
	if (*MJPG_buff == MAP_FAILED) 
		mjpeg_error("mapping video buffers: %s\n", sys_errlist[errno]);


	/* Assure proper exit handling if the user presses ^C during recording */
	signal(SIGINT,SigHandler);

	/* Try to get a reliable timestamp for Audio */
	if (audio_size && sync_corr>1)
	{
		mjpeg_info("Getting audio ...\n");

		for(n=0;;n++)
		{
			if(n>NUM_AUDIO_TRIES)
			{
				mjpeg_error_exit1("Unable to get audio - exiting ....");
			}
			res = audio_read(AUDIO_buff,AUDIO_BUFFER_SIZE,0,audio_t0,astat);
			if(res<0)
			{
				mjpeg_error_exit1("Error reading audio: %s\n",audio_strerror());
			}
			if(res && audio_t0->tv_sec ) break;
			usleep(20000);
		}
	}

	/* For single frame recording: Make stdin nonblocking */
	if(single_frame || wait_for_start)
	{
		res = fcntl(0,F_SETFL,O_NONBLOCK);
		if (res<0) mjpeg_error("making stdin nonblocking:%s\n",sys_errlist[errno]);
	}

	/* If we can increase process priority ... no need for R/T though... */
	/* This is mainly useful for running using "at" which otherwise drops the
	   priority which causes sporadic audio buffer over-runs */
	if( getpriority(PRIO_PROCESS, 0) > -5 )
		setpriority(PRIO_PROCESS, 0, -5 );

	*p_video_dev = video_dev;
}

static void wait_for_enter(char *AUDIO_buff, struct timeval *audio_t0, int *astat)
{
	int res;
	char input_buffer[256];

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
			while( (res=audio_read(AUDIO_buff,AUDIO_BUFFER_SIZE,
				0,audio_t0,astat)) >0 ) /*noop*/;
			if(res==0) continue;
			if(res<0)
			{
				mjpeg_error_exit1("Error reading audio: %s\n",audio_strerror());
			}
		}
	}
}

#ifdef REC_SYNC_DEBUG
static void output_stats(unsigned int num_ins, unsigned int num_lost,
						 unsigned int num_del, unsigned int num_aerr, 
						 double tdiff1, double tdiff2,
						 struct timeval *prev_sync, struct timeval *cur_sync)
#else
static void output_stats(unsigned int num_ins, unsigned int num_lost,
							 unsigned int num_del, unsigned int num_aerr)
#endif
{
	int nf, ns, nm, nh;

	if(norm!=VIDEO_MODE_NTSC)
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
#ifdef REC_SYNC_DEBUG
	if( prev_sync->tv_usec > cur_sync->tv_usec )
		prev_sync->tv_usec -= 1000000;
	sprintf(infostring,
			"%2d.%2.2d.%2.2d:%2.2d int: %05d lst:%4u ins:%3u del:%3u "
			"ae:%3u td1=%.3f td2=%.3f\r",
			nh, nm, ns, nf, 
			(cur_sync->tv_usec-prev_sync.tv_usec)/1000, num_lost,
			num_ins, num_del, num_aerr, tdiff1,tdiff2);
#else
	sprintf(infostring,
			"%2d.%2.2d.%2.2d:%2.2d lst:%4u ins:%3u del:%3u ae:%3u\r",
			nh, nm, ns, nf, 
			num_lost, num_ins, num_del, num_aerr );
#endif
	printf(infostring);
    fflush(stdout);
}

int main(int argc, char ** argv)
{
	int video_dev;
	struct timeval audio_t0;
	struct mjpeg_requestbuffers breq;
	struct mjpeg_sync bsync;
	struct timeval first_time;
	struct timeval audio_tmstmp;
	struct timeval prev_sync, cur_sync;
	char *MJPG_buff = 0;
	char AUDIO_buff[AUDIO_BUFFER_SIZE];
	long audio_offset = 0, nb;
	int write_frame;
	char input_buffer[256];
	int n, res, nfout;
	unsigned int first_lost; 
	unsigned int num_lost, num_syncs, num_ins, num_del, num_aerr;
	int stats_changed, astat;
	double time, tdiff1, tdiff2;
	double sync_lim;

	/* command-line options */
	check_command_line_options(argc, argv);

	/* Care about printing what we're actually doing */
	if (verbose > 1) 
		lavrec_print_properties();

	/* Flush the Linux File buffers to disk */
	sync();

	/* setup the properties, device et all */
	lavrec_setup(&video_dev, &breq, &MJPG_buff, AUDIO_buff,
				 &audio_t0, &astat);

	/* We are set up now - wait for user confirmation if wanted */
	if(wait_for_start) wait_for_enter(AUDIO_buff, &audio_t0, &astat);

	/* Queue all buffers, this also starts streaming capture */
	for(n=0;n<breq.count;n++)
	{
		res = ioctl(video_dev, MJPIOC_QBUF_CAPT, &n);
		if (res<0) 
			mjpeg_error_exit1("queuing buffers: %s\n", sys_errlist[errno]);
	}

	/* The video capture loop */
	write_frame = 1;
	first_lost = 0;
	stats_changed = 0;
	num_syncs  = 0; /* Number of MJPIOC_SYNC ioctls            */
	num_lost   = 0; /* Number of frames lost                   */
	num_frames = 0; /* Number of frames written to file        */
	num_asamps = 0; /* Number of audio samples written to file */
	num_ins    = 0; /* Number of frames inserted for sync      */
	num_del    = 0; /* Number of frames deleted for sync       */
	num_aerr   = 0; /* Number of audio buffers in error        */

	/* Seconds per video frame: */
	spvf = (norm==VIDEO_MODE_NTSC) ? 1001./30000. : 0.040;
	sync_lim = spvf*1.5;
	/* Seconds per audio sample: */
	if(audio_size)
		spas = 1.0/audio_rate;
	else
		spas = 0.;

	tdiff1 = 0.;
	tdiff2 = 0.;
	gettimeofday( &prev_sync, NULL );

	/* This is the get-the-frame-from-the-device-and-safe-to-file-cycle */
	while (1)
	{
		/* sync on a frame */
		res = ioctl(video_dev, MJPIOC_SYNC, &bsync);
		if (res < 0)
		{
			close_files_on_error();
			mjpeg_error("syncing on a buffer:%s\n", sys_errlist[errno]);
		}
		num_syncs++;
		gettimeofday( &cur_sync, NULL );
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
			if(sync_corr>0) 
				nfout += n - num_lost; /* lost since last sync */
			stats_changed = (num_lost != n);
			num_lost = n;

			/* Check if we have to insert/delete frames to stay in sync */

			if(sync_corr>1)
			{
				if( tdiff1-tdiff2 < -sync_lim)
				{
					nfout++;
					num_ins++;
					stats_changed = 1;
					tdiff1 += spvf;
				}
				if( tdiff1-tdiff2 > sync_lim)
				{
					nfout--;
					num_del++;
					stats_changed = 1;
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
			mjpeg_error("re-queuing buffer:%s\n",sys_errlist[errno]);
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
				mjpeg_error("Reading audio: %s\n",audio_strerror());
				close_files_on_error();
				res = -1;
				break;
			}

			if(!astat) 
			{
				num_aerr++;
				stats_changed = 1;
			}

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

		/* Output statistics */

		if(!single_frame && output_status<3 && (verbose > 0 || stats_changed))
		{
#ifdef REC_SYNC_DEBUG
			output_stats(num_ins, num_lost, num_del, num_aerr,
				tdiff1, tdiff2, &prev_sync, &cur_sync);
#else
			output_stats(num_ins, num_lost, num_del, num_aerr);
#endif

			if(stats_changed )
				printf("\n");
			stats_changed = 0;
		}

		prev_sync = cur_sync;

		if (res) break;
	}

	/* Audio and mixer exit processing is done with atexit() */

	/* Re-mute tuner audio if this is a tuner */
	if (input == 2) {
		mjpeg_info("Re-muting tuner audio...\n");
		vau.flags |= VIDEO_AUDIO_MUTE;
		res = ioctl(video_dev,VIDIOCSAUDIO,&vau);
		if(res<0) mjpeg_error("setting tuner audio params:%s\n",
							  sys_errlist[errno]);
	}

	if(res>=0) {
		mjpeg_info("Clean exit ...\n");
	} else
		mjpeg_info("Error exit ...\n");
	exit(0);
}

