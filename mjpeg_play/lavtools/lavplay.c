/*
 * lavplay - Linux Audio Video PLAYback
 *      
 * Copyright (C) 2000 Rainer Johanni <Rainer@Johanni.de>
 *
 *
 * Plays back MJPEG AVI, Quicktime or movtar files using the
 * hardware of the zoran card or using SDL (software playback)
 *
 * Usage: lavplay [options] filename [filename ...]
 * where options are as follows:
 *
 *   -h/--h-offset num --- Horizontal offset
 *
 *   -v/--v-offset num --- Vertical offset
 *      You may use that only for quarter resolution videos
 *      unless you remove the following 4 lines of code in the BUZ driver:
 *
 *      if(params->img_x     <0) err0++;
 *      if(params->img_y     <0) err0++;
 *
 *      and
 *
 *      if(params->img_x + params->img_width  > 720) err0++;
 *      if(params->img_y + params->img_height > tvnorms[params->norm].Ha/2) err0++;
 *
 *      These parameters are usefull for videos captured from other sources
 *      which do not appear centered when played with the BUZ
 *
 *   -a/--audio [01] --- audio playback (defaults to on)
 *
 *   -s/--skip num --- skip num seconds before playing
 *
 *   -c/--synchronization [01] --- Sync correction off/on (default on)
 *
 *   -n/--mjpeg-buffers num --- Number of MJPEG buffers
 *               normally no need to change the default
 *
 *   -q/--no-quit --- Do NOT quit at end of video
 *
 *   -g/--gui-mode --- GUI-mode, used by xlav and LVS to control video position
 *
 *   -x/--exchange-fields --- Exchange fields of an interlaced video
 *
 *   -z/--zoom --- "Zoom to fit flag"
 *      If this flag is not set, the video is always displayed in origininal
 *      resolution for interlaced input and in double height and width
 *      for not interlaced input. The aspect ratio of the input file is maintained.
 *
 *      If this flag is set, the program tries to zoom the video in order
 *      to fill the screen as much as possible. The aspect ratio is NOT
 *      maintained, ie. the zoom factors for horizontal and vertical directions
 *      may be different. This might not always yield the results you want!
 *
 *   -p/--playback [SHC] --- playback method
 *      'S': software playback (using SDL)
 *      'H': hardware playback directly on the monitor
 *      'C': hardware playback to the output of the card
 *      For 'C', you need xawtv to see what you're playing
 *
 * Following the options, you may give a optional +p/+n parameter (for forcing
 * the use of PAL or NTSC) and the several AVI files, Quicktime files
 * or Edit Lists arbitrarily mixed (as long as all files have the same
 * paramters like width, height and so on).
 *
 * Forcing a norm does not convert the video from NTSC to PAL or vice versa.
 * It is only intended to be used for foreign AVI/Quicktime files whose
 * norm can not be determined from the framerate encoded in the file.
 * So use with care!
 *
 * The format of an edit list file is as follows:
 *    line 1: "LAV Edit List"
 *    line 2: "PAL" or "NTSC"
 *    line 3: Number of AVI/Quicktime files comprising the edit sequence
 *    next lines: Filenames of AVI/Quicktime files
 *    and then for every sequence
 *    a line consisting of 3 numbers: file-number start-frame end-frame
 *
 * If you are a real command line hardliner, you may try to entering the following
 * commands during playing (normally used by xlav/LVS, just type the command and
 * press enter):
 *    p<num>    Set playback speed, num may also be 0 (=pause) and negative (=reverse)
 *    s<num>    Skip to frame <num>
 *    s+<num>   <num> frames forward
 *    s-<num>   <num> frames backward
 *    +         1 frame forward (makes only sense when paused)
 *    -         1 frame backward (makes only sense when paused)
 *    q         quit
 *    em        Move scene (arg1->arg2) to position (arg3)
 *    eu/eo     Cut (u) or Copy (o) scene (arg1->arg2) into memory
 *    ep        Paste selection into current position of video
 *    ed        Delete scene (arg1->arg2) from video
 *    ea        Add movie (arg1, be sure that it's mov/avi/movtar!!!)
 *                frame arg2-arg3 (if arg2=-1, whole movie) to position arg4
 *    es        Set a lowest/highest possible frame (max/min = trimming)
 *    om        Open movie (arg1) frame arg2-arg3 (arg2=-1 means whole
 *                movie). Watch out, this movie should have the same params
 *                as the current open movie or weird things may happen!!!
 *    wa        Save current movie into edit list (arg1)
 *    ws        Save current selection into memory into edit list (arg1)
 *
 **** Environment variables ***
 *
 * Recognized environment variables
 *    LAV_VIDEO_DEV: Name of video device (default: "/dev/video")
 *    LAV_AUDIO_DEV: Name of audio device (default: "/dev/dsp")
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
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include <inttypes.h>
#include <getopt.h>
#include <SDL/SDL.h>
#include <mjpeg.h>

#include "jpegutils.h" 
#include "audiolib.h"
#include "lav_io.h"
#include "editlist.h"
#include "mjpeg_logging.h"

/************************** DEFINES **************************/
#define LAVPLAY_VSTR "lavplay" LAVPLAY_VERSION  /* Expected version info */

#define LAVPLAY_INTERNAL 0
#define LAVPLAY_DEBUG    1
#define LAVPLAY_INFO     2
#define LAVPLAY_WARNING  3
#define LAVPLAY_ERROR    4

#define stringify( str ) #str



void malloc_error(void);
void sig_cont(int sig);
int queue_next_frame(char *vbuff, int skip_video, int skip_audio, int skip_incr);
void Usage(char *progname);
void lock_screen(void);
void update_screen(uint8_t *jpeg_buffer, int buf_len);
void unlock_screen(void);
void x_shutdown(int a);
void add_new_frames(char *movie, int nc1, int nc2, int dest);
void cut_copy_frames(int nc1, int nc2, char cut_or_copy);
void paste_frames(int dest);
void check_min_max(void);
void initialize_SDL_window(void);
int process_edit_input(char *input_buffer, int length);


/************************** VARIABLES **************************/
int  verbose = 1;
static EditList el;
static int    h_offset = 0;
static int    v_offset = 0;
static int    quit_at_end = 1;
static int    gui_mode = 0;
static int    exchange_fields  = 0;
static int    zoom_to_fit = 0;
static int    MJPG_nbufs = 8;          /* Number of MJPEG buffers */
static int    skip_seconds = 0;
static double test_factor = 1.0;     /* Internal test of synchronization only */
static int    audio_enable = 1;
static int    playback_width = 720;  /* playback_width for hardware */
static int    opt_use_read = 0;

/* gz (Gernot) variables */
static int soft_play = 0; /* software */
int soft_fullscreen = 0;
struct mjpeg_handle *mjpeg;
int screen_output = 0; /* hardware */

/* The following variable enables flicker reduction by doubling/reversing
   the fields. If this leads to problems during play (because of
   increased CPU overhead), just set it to 0 */
static int flicker_reduction = 1;
static int nvcorr = 0; /* Number of frames added/skipped for sync */
static int numca = 0; /* Number of corrections because video ahead audio */
static int numcb = 0; /* Number of corrections because video behind audio */

static long nframe;

static int play_speed=1;
static int audio_mute=0;

static unsigned long *save_list=0;
static long save_list_len = 0;

static char abuff[16384];

static char *tmpbuff[2]; /* buffers for flicker reduction */
static long old_field_len = 0;
static int  old_buff_no = 0;

static double spvf;   /* seconds per video frame */
static double spas;   /* seconds per audio sample */

/* Audio/Video sync correction params */
int sync_corr = 1;           /* Switch for sync correction */

/* The following two params control the way how synchronization is achieved:

   If video is behind audio and "sync_skip_frames", video frames are
   skipped, else audio samples are inserted.

   If video is ahead audio and "sync_ins_frames" is set, video frames
   are inserted (by duplication), else audio samples are skipped.
*/
int sync_skip_frames = 1;
int sync_ins_frames  = 1;

/* These options are for use in Linux Video Studio - they won't do
   much bad in normal lavplay use since I will set them to video
   defaults anyway (0 and el.video_frames-1) */
static int min_frame_num;
static int max_frame_num;

/* SDL parameters (for software-playback) */
SDL_Surface *screen;
SDL_Rect jpegdims;
SDL_Overlay *yuv_overlay;
static uint8_t *jpeg_data;
int soft_width=0, soft_height=0;

/************************** PROGRAM CODE **************************/


/* system_error: report an error from a system call */

static void system_error(char *str1,char *str2)
{
   mjpeg_error_exit1("Error %s (in %s): %s\n",str1,str2,strerror(errno));
}

/* Since we use malloc often, here the error handling */

void malloc_error(void)
{
   mjpeg_error_exit1("Out of memory - malloc failed\n");
}

void sig_cont(int sig)
{
   int res;

   res = fcntl(0,F_SETFL,O_NONBLOCK);
   if (res<0) system_error("making stdin nonblocking","fcntl F_SETFL");
}


static int inc_frames(long num)
{
   nframe += num;
   if(nframe<min_frame_num)
   {
      nframe = min_frame_num;
      play_speed = 0;
      return 0;
   }
   if(nframe>max_frame_num)
   {
      nframe = max_frame_num;
      play_speed = 0;
      return 1;
   }
   return 0;
}

int queue_next_frame(char *vbuff, int skip_video, int skip_audio, int skip_incr)
{
   int res, mute, i, jpeg_len1, jpeg_len2, new_buff_no;
   char hlp[16];

   /* Read next frame */

   if(!skip_video)
   {
      if(flicker_reduction && el.video_inter && play_speed <= 0)
      {
         if(play_speed == 0)
         {
            res = el_get_video_frame(vbuff, nframe, &el);
            if(res<0) return -1;
            jpeg_len1 = lav_get_field_size(vbuff, res);
			/* Found seperate fields? */
			if( jpeg_len1 < res )
			{
				memcpy(vbuff+jpeg_len1, vbuff, jpeg_len1);
				old_field_len = 0;
			}
         }
         else /* play_speed < 0, play old first field + actual second field */
         {
            new_buff_no = 1 - old_buff_no;
            res = el_get_video_frame(tmpbuff[new_buff_no], nframe, &el);
            if(res<0) return -1;
            jpeg_len1 = lav_get_field_size(tmpbuff[new_buff_no], res);
			if( jpeg_len1 < res )
			{
				jpeg_len2 = res - jpeg_len1;
				if(old_field_len==0)
				{
					/* no old first field, duplicate second field */
					memcpy(vbuff,tmpbuff[new_buff_no]+jpeg_len1,jpeg_len2);
					old_field_len = jpeg_len2;
				}
				else
				{
					/* copy old first field into vbuff */
					memcpy(vbuff,tmpbuff[old_buff_no],old_field_len);
				}
				/* copy second field */
				memcpy(vbuff+old_field_len,tmpbuff[new_buff_no]+jpeg_len1,jpeg_len2);
				/* save first field */
				old_field_len = jpeg_len1;
				old_buff_no   = new_buff_no;
			}
         }
      }
      else
      {
         res = el_get_video_frame(vbuff, nframe, &el);
         if(res<0) 
			 return -1;
         old_field_len = 0;
      }

   }

   /* Read audio, if present */

   if(el.has_audio && !skip_audio && audio_enable)
   {
      mute = play_speed==0 ||
             (audio_mute && play_speed!=1 && play_speed!=-1);
      res = el_get_audio_data(abuff, nframe, &el, mute);

      if(play_speed<0)
      {
         /* reverse audio */
         for(i=0;i<res/2;i+=el.audio_bps)
         {
            memcpy(hlp,abuff+i,el.audio_bps);
            memcpy(abuff+i,abuff+res-i-el.audio_bps,el.audio_bps);
            memcpy(abuff+res-i-el.audio_bps,hlp,el.audio_bps);
         }
      }
      res = audio_write(abuff,res,0);
      if(res<0)
      {
         mjpeg_error("Playing audio: %s\n",audio_strerror());
         return -1;
      }
   }

   /* Increment frames */

   if(!skip_incr)
   {
      res = inc_frames(play_speed);
      if(quit_at_end) return res;
   }

   return 0;
}

void Usage(char *progname)
{
   fprintf(stderr, "Usage: %s [options] <filename> [<filename> ...]\n", progname);
   fprintf(stderr, "where options are:\n");
   fprintf(stderr, "  -o/--norm [np]             NTSC or PAL (default: guess from framerate)\n");
   fprintf(stderr, "  -h/--h-offset num          Horizontal offset\n");
   fprintf(stderr, "  -v/--v-offset num          Vertical offset\n");
   fprintf(stderr, "  -s/--skip num              skip num seconds before playing\n");
   fprintf(stderr, "  -c/--synchronization [01]  Sync correction off/on (default on)\n");
   fprintf(stderr, "  -n/--mjpeg-buffers num     Number of MJPEG buffers\n");
   fprintf(stderr, "  -q/--no-quit               Do NOT quit at end of video\n");
   fprintf(stderr, "  -g/--gui-mode              GUI-mode (used by xlav and LVS)\n");
   fprintf(stderr, "  -x/--exchange-fields       Exchange fields of an interlaced video\n");
   fprintf(stderr, "  -z/--zoom                  Zoom video to fill screen as much as possible\n");
   fprintf(stderr, "  -Z/--full-screen           Switch to fullscreen\n");
   fprintf(stderr, "  -p/--playback [SHC]        playback: (S)oftware, (H)ardware (screen) or (C)ard\n");
   fprintf(stderr, "  -a/--audio [01]            Enable audio playback\n");
   fprintf(stderr, "  --size NxN                 width X height for SDL window (software)\n");
   exit(1);
}

void lock_screen(void)
{
       /* lock the screen for current decompression */
       if ( SDL_MUSTLOCK(screen) ) 
	 {
	   if ( SDL_LockSurface(screen) < 0 )
	     mjpeg_error_exit1("Error locking output screen: %s\n", SDL_GetError());
	 }
	if (SDL_LockYUVOverlay(yuv_overlay) < 0)
		mjpeg_error_exit1("Error locking yuv overlay: %s\n", SDL_GetError());
}

void unlock_screen(void)
{
	if ( SDL_MUSTLOCK(screen) ) 
	{
		SDL_UnlockSurface(screen);
	}
	SDL_UnlockYUVOverlay(yuv_overlay);
}

void update_screen(uint8_t *jpeg_buffer, int buf_len)
{
	/* get frame into jpeg_data (length len) */
	//len = el_get_video_frame(jpeg_data, nframe, &el);

	SDL_LockYUVOverlay( yuv_overlay );
	/* decode frame to yuv[] */
	decode_jpeg_raw (jpeg_buffer, buf_len, el.video_inter, CHROMA420,
					 el.video_width, el.video_height, 
					 yuv_overlay->pixels[0], 
					 yuv_overlay->pixels[1],
					 yuv_overlay->pixels[2]);

	SDL_UnlockYUVOverlay( yuv_overlay );
	SDL_DisplayYUVOverlay(yuv_overlay, &jpegdims);
}

void initialize_SDL_window()
{
	int i;
	char wintitle[255];
	char *sbuffer;

	mjpeg_info("Initialising SDL\n");
	if (SDL_Init (SDL_INIT_VIDEO) < 0)
	{
		mjpeg_error_exit1( "SDL Failed to initialise...\n" );

	}

	/* Now initialize SDL */
	if (soft_fullscreen)
		screen = SDL_SetVideoMode(soft_width==0?el.video_width:soft_width, 
			soft_height==0?el.video_height:soft_height, 0,
			SDL_HWSURFACE | SDL_FULLSCREEN);
	else
		screen = SDL_SetVideoMode(soft_width==0?el.video_width:soft_width, 
			soft_height==0?el.video_height:soft_height, 0,
			SDL_HWSURFACE);
	if ( screen == NULL )  
	{
		mjpeg_error_exit1("SDL: Output screen error: %s\n", SDL_GetError());
	}

	SDL_EventState(SDL_KEYDOWN, SDL_ENABLE);
	SDL_EventState(SDL_MOUSEMOTION, SDL_IGNORE);

	/* jpeg frames - memory allocation */
	jpeg_data = malloc(3 * el.video_height * el.video_width * sizeof(uint8_t)/2);

	yuv_overlay = SDL_CreateYUVOverlay(el.video_width, el.video_height, 
									   SDL_IYUV_OVERLAY, screen);
	if ( yuv_overlay == NULL )
	{
		mjpeg_error_exit1("SDL: Couldn't create SDL_yuv_overlay: %s\n", SDL_GetError());
	}
	mjpeg_info( "SDL YUV overlay: %s\n",
				yuv_overlay->hw_overlay ? "hardware" : "software" );
	if( yuv_overlay->pitches[0] != yuv_overlay->pitches[1]*2 ||
		yuv_overlay->pitches[0] != yuv_overlay->pitches[2]*2 )
		mjpeg_error_exit1( "SDL returned non-YUV420 overlay!\n");

	jpegdims.x = 0; // This is not going to work with interlaced pics !!
	jpegdims.y = 0;
	jpegdims.w = soft_width==0?el.video_width:soft_width;
	jpegdims.h = soft_height==0?el.video_height:soft_height;

	/* Lock the screen to test, and to be able to access screen->pixels */
	lock_screen();
       
	/* Draw bands of color on the raw surface, as run indicator for debugging */
	sbuffer = (char *)screen->pixels;
	for ( i=0; i < screen->h; ++i ) 
	{
		memset(sbuffer,(i*255)/screen->h,
			   screen->w * screen->format->BytesPerPixel);
		sbuffer += screen->pitch;
	}

	/* Set up the necessary callbacks for the synchronisation thread */
	mjpeg->update_screen_callback = update_screen;

	/* Set the windows title (currently simply the first file name) */
	sprintf(wintitle, "lavplay %s", el.video_file_list[0]);
	SDL_WM_SetCaption(wintitle, "0000000");  

	/* unlock, update and wait for the fun to begin */
	unlock_screen();
	SDL_UpdateRect(screen, 0, 0, jpegdims.w, jpegdims.h);
}

static int reentry = 0;
void x_shutdown(int a)
{
	if( reentry )		/* Not perfect but good enough... */
		return;
	reentry = 1;
	mjpeg_debug("Ctrl-C shutdown!\n");
  
	if (el.has_audio && audio_enable) 
		audio_shutdown();

	mjpeg_close(mjpeg);

	if (soft_play)
	{
		free(jpeg_data);
		SDL_FreeYUVOverlay(yuv_overlay);
		SDL_Quit();
	}

	/* Force direct exit - main thread could be in a wait on the now killed
	   mjpeg thread and suchlike things. */
	_exit(1);
}


/* Moved editing part here to make adding some new functions based on
   these easier. Think about a move a scene (nc1, nc2) to position nc3,
   delete a scene (nc1, nc2), etc. */

void check_min_max()
{
	/* A few simple checks to make sure things don't go wrong */
	if (min_frame_num < 0)
		min_frame_num = 0;
	if (max_frame_num < 0)
		max_frame_num = 0;
	if (min_frame_num >= el.video_frames)
		min_frame_num = el.video_frames-1;
	if (max_frame_num >= el.video_frames)
		max_frame_num = el.video_frames-1;
	if (min_frame_num>max_frame_num)
		max_frame_num = min_frame_num;
}

void add_new_frames(char *movie, int nc1, int nc2, int dest)
{
	/* This will add frames nc1->nc2 from new
	movie 'movie' to the current edit list at dest,
	nc1=-1 (nc2 does not matter) means whole movie */

	if (nc1 < -1 || nc2<nc1 || dest < 0 || dest >= el.video_frames)
	{
		mjpeg_error("Wrong parameters for add movie frames...\n");
	}
	else
	{
		/* Open movie into editlist */
		int i,old_n,n;

		old_n = el.video_frames;

		n = open_video_file(movie,&el);

		el.frame_list = (long*) realloc(el.frame_list,
			(el.video_frames+el.num_frames[n])*sizeof(long));
		if(el.frame_list==0) malloc_error();
		for(i=0;i<el.num_frames[n];i++)
			el.frame_list[el.video_frames++] = EL_ENTRY(n,i);

		if (nc1 == -1)
		{
			nc1 = 0;
			nc2 = el.num_frames[n]-1;
		}

		/* Delete unwanted frames */
		if (nc1>0)
		{
			for(i=old_n;i<old_n+nc1;i++) el.frame_list[i] = el.frame_list[i+nc1];
			el.video_frames -= nc1;
		}
		if (nc2+old_n-nc1 < el.video_frames-1 && nc1 != -1)
		{
			el.video_frames = old_n + nc2 - nc1 + 1;
		}

		/* Move frames we want to the dest position */
		cut_copy_frames(old_n, old_n+nc2-nc1, 'u');
		paste_frames(dest);

		mjpeg_debug("--- Added frames %d - %d from movie %s\n", nc1, nc2, movie);
	}
}

void cut_copy_frames(int nc1, int nc2, char cut_or_copy)
{
	int i,k;

	if(nc1>=0 && nc2>=nc1 && nc2<el.video_frames)
	{
		/* Save Selection */

		if(save_list) free(save_list);
		save_list = (long*) malloc((nc2-nc1+1)*sizeof(long));
		if(save_list==0) malloc_error();
		k = 0;
		for(i=nc1;i<=nc2;i++) save_list[k++] = el.frame_list[i];
		save_list_len = k;

		/* Cut Frames */

		if(cut_or_copy=='u')
		{

			if(nframe>=nc1 && nframe<=nc2) nframe = nc1-1;
			if(nframe>nc2) nframe -= save_list_len;

			for(i=nc2+1;i<el.video_frames;i++)
			{
				el.frame_list[i-save_list_len] = el.frame_list[i];
			}

			/* Update min and max frame_num's to reflect the removed section
			 */
			if( nc1-1 < min_frame_num )
			{
				if( nc2 <= min_frame_num )
					min_frame_num -= (nc2-nc1)+1;
				else
					min_frame_num = nc1-1;
			}
			if( nc1-1 < max_frame_num )
			{
				if( nc2 <= max_frame_num )
					max_frame_num -= (nc2-nc1)+1;
				else
					max_frame_num = nc1-1;
			}

			el.video_frames -= save_list_len;
			check_min_max();
		}
		mjpeg_debug("Cut/Copy done ---- !!!!\n");
	}
	else
	{
		mjpeg_error("Cut/Copy %d %d params are wrong!\n",nc1,nc2);
	}
}

void paste_frames(int dest)
{
	/* We should output a warning if save_list is empty,
	   we just insert nothing */

	int k,i;

	el.frame_list = realloc(el.frame_list,(el.video_frames+save_list_len)*sizeof(long));
	if(el.frame_list==0) malloc_error();
	k = save_list_len;
	for(i=el.video_frames-1;i>=dest;i--) el.frame_list[i+k] = el.frame_list[i];
	k = dest;
	for(i=0;i<save_list_len;i++)
	{
		if(k<=min_frame_num) min_frame_num++;
		if(k<max_frame_num) max_frame_num++;
		el.frame_list[k++] = save_list[i];
	}
	el.video_frames += save_list_len;
	check_min_max();
	mjpeg_debug("Paste done ---- !!!!\n");
	inc_frames(dest - nframe);
}

/* The main editor loop:
 *  'pN'           - sets play speed to N (...,-1,0,1,...)
 *  '+'            - goes to next frame (only makes sense when playspeed=0)
 *  '-'            - goes to previous frame
 *  'aN'           - N = [01], 0 means disable audio, 1 means enable audio
 *  'sX'           - X = num   : goto frame 'num'
 *                   X = +/-num: go 'num' frames back/forward
 *  'eX [options]' - Editing option X with optional options (see below)
 *  'om [options]' - Open movie (arg1) frames (arg2-arg3)
 *  'wa [file]'    - Save current editlist to file [file]
 *  'ws [file]'    - Save current selection to file [file]
 *  'q'            - Quit
 *
 * Editing options ('eX [options]'):
 *  'eu N1 N2'           - cut frame N1-N2 to selection
 *  'eo N1 N2'           - copy frame N1-N2 to selection
 *  'ep'                 - Paste selection-frames in the movie at the current position
 *  'em N1 N2 N3'        - Move frames N1-N2 to position N3 (N3 = position before cutting)
 *  'ed N1 N2'           - Remove frames N1-N2 from the playlist
 *  'ea [file] N1 N2 N3' - Add movie [file] frames N1-N2 to the current playlist at N3
 *                         If N1 is -1, the whole movie is added
 *  'es N1 N2'           - Set playable part of playlist to N1-N2 (for trimming)
 *                         If N1 is -1, the whole movie will be seen
 */

int process_edit_input(char *input_buffer, int length)
{
	int i;

	input_buffer[length-1] = 0;
	if(input_buffer[0]=='q') return -1; /* We are done */
	switch(input_buffer[0])
	{
		case 'p': play_speed = atoi(input_buffer+1); break;
		case '+': inc_frames( 1); break;
		case '-': inc_frames(-1); break;
		case 'a': audio_mute = input_buffer[1]=='0'; break;
		case 's':
			if(input_buffer[1]=='+')
			{
				int nskip;
				nskip = atoi(input_buffer+2);
				inc_frames(nskip);
			}
			else if(input_buffer[1]=='-')
			{
				int nskip;
				nskip = atoi(input_buffer+2);
				inc_frames(-nskip);
			}
			else
			{
				int nskip;
				nskip = atoi(input_buffer+1)-nframe;
				inc_frames(nskip);
			}
			break;

		case 'e':
			/* Do some simple editing:
			   next chars are u (for cUt), o (for cOpy) or p (for Paste) */
			if(input_buffer[1]=='u'||input_buffer[1]=='o')
			{
				/* Cut/Copy scene (nc1->nc2) into memory */
				int nc1, nc2;
				sscanf(input_buffer+2,"%d %d",&nc1,&nc2);
				cut_copy_frames(nc1, nc2, input_buffer[1]);
			}

			if(input_buffer[1]=='p')
			{
				/* Paste current selection in current position */
				int k,i;

				el.frame_list = realloc(el.frame_list,
				(el.video_frames+save_list_len)*sizeof(long));
				if(el.frame_list==0) malloc_error();
				k = save_list_len;
				for(i=el.video_frames-1;i>nframe;i--) el.frame_list[i+k] = el.frame_list[i];
				k = nframe+1;
				for(i=0;i<save_list_len;i++) el.frame_list[k++] = save_list[i];
				el.video_frames += save_list_len;
				mjpeg_debug("Paste done ---- !!!!\n");
			}

			if(input_buffer[1]=='m')
			{
				/* Move scene(nc1->nc2) to position (nc3) */
				int nc1, nc2, nc3;
				sscanf(input_buffer+2,"%d %d %d",&nc1,&nc2, &nc3);
				cut_copy_frames(nc1, nc2, 'u');
				paste_frames(nc3);
			}

			if(input_buffer[1]=='d')
			{
				/* Delete scene(nc1->nc2) */
				int k, nc1, nc2;
				sscanf(input_buffer+2,"%d %d",&nc1,&nc2);
				k = nc2 - nc1+1;
				if(nframe>=nc1 && nframe<=nc2) nframe = nc1-1;
				if(nframe>nc2) nframe -= k;
				for(i=nc2+1;i<el.video_frames;i++)
				{
					el.frame_list[i-k] = el.frame_list[i];
					if(i-k < min_frame_num) min_frame_num--;
					if(i-k <= max_frame_num) max_frame_num--;
				}
				el.video_frames -= k;
				check_min_max();
			}

			if(input_buffer[1]=='a')
			{
				/* Add scenes from new movie file...
				arg1=movie.avi/mov/movtar, arg2/arg3=start/stop frames */
				int nc1, nc2, dest;
				char movie[256];
				sscanf(input_buffer+2,"%s %d %d %d",movie,&nc1,&nc2,&dest);
				add_new_frames(movie, nc1, nc2, dest);
			}

			if(input_buffer[1]=='s')
			{
				/* Set lowest/highest frame to display, nc1==-1 means whole movie */
				int nc1, nc2;
				sscanf(input_buffer+2,"%d %d",&nc1,&nc2);
				if(nc1<-1||nc1>nc2||nc2>=el.video_frames)
				{
					mjpeg_error("Wrong parameters for setting frame borders!\n");
				}
				else if (nc1==-1)
				{
					min_frame_num = 0;
					max_frame_num = el.video_frames-1;
				}
				else
				{
					min_frame_num = nc1;
					max_frame_num = nc2;
				}
			}
			break;

		case 'o':
			/* Open a new file - caution!!! There is no checking
			   for sound, equivalent properties or anything!!! */
			if(input_buffer[1]=='m')
			{
				/* Movie, 'om file beginframe endframe',
				   beginframe=-1 means whole file */
				char movie[256];
				char *arguments[1];
				int nc1, nc2, x, nc3, nc4;

				play_speed = 0;
				nframe = 0;
				/* nc1-nc2 = movie, nc3-nc4 is part of movie that should be seen
				   nc3=-1 means just see nc1-nc2, nc3/nc4 can also be omitted */
				x = sscanf(input_buffer+2, "%s %d %d %d %d", movie, &nc1, &nc2, &nc3, &nc4);
				arguments[0] = movie;
				if( nc1 != -1 )
					mjpeg_info("Opening %s (frames %d-%d)\n", movie, nc1,nc2);
				else
					mjpeg_info("Opening %s\n", movie);
				read_video_files(arguments, 1, &el);
				if (nc2>=el.video_frames || nc1 == -1) nc2=el.video_frames-1;
				if (nc1<0) nc1=0;
				if (nc4>nc2) nc4=nc2;
				if (nc3<nc1 && nc3!=-1) nc3=nc1;

				if (x == 3)
				{
					min_frame_num = 0;
					max_frame_num = nc2 - nc1;
				}
				else if (nc3 == -1)
				{
					min_frame_num = 0;
					max_frame_num = nc2 - nc1;
				}
				else
				{
					min_frame_num = nc3 - nc1;
					max_frame_num = el.video_frames - 1 + nc4 - nc2;
				}

				for(i=0;i<nc1;i++) el.frame_list[i] = el.frame_list[i+nc1];
				el.video_frames = nc2-nc1+1;
			}
			break;

		case 'w':
			/* Write edit list */
			if(input_buffer[1]=='a')
			{
				char filename[256];
				sscanf(input_buffer+3,"%s",filename);
				write_edit_list(filename,0,el.video_frames-1,&el);
			}
			if(input_buffer[1]=='s')
			{
				char filename[256];
				int ns1, ns2;
				sscanf(input_buffer+3,"%d %d %s",&ns1,&ns2,filename);
				write_edit_list(filename,ns1,ns2,&el);
			}
			break;
	}
	printf("- play speed =%3d, pos =%6ld/%ld >",play_speed,nframe,el.video_frames);

	return 0;
}


/* Functions for command-line options, supporting
 * getopt() and getopt_long()
 */

static int set_option(char *name, char *value)
{
	/* return 1 means error, return 0 means okay */
	int nerr = 0;
	if (strcmp(name, "verbose")==0 || strcmp(name, "v")==0)
	{
		verbose = atoi(optarg);
	}
	else if (strcmp(name, "audio")==0 || strcmp(name, "a")==0)
	{
		audio_enable = atoi(optarg);
	}
	else if (strcmp(name, "H-offset")==0 || strcmp(name, "H")==0)
	{
		h_offset = atoi(optarg);
	}
	else if (strcmp(name, "V-offset")==0 || strcmp(name, "V")==0)
	{
		v_offset = atoi(optarg);
	}
	else if (strcmp(name, "skip")==0 || strcmp(name, "s")==0)
	{
		skip_seconds = atoi(optarg);
		if(skip_seconds<0) skip_seconds = 0;
	}
	else if (strcmp(name, "synchronization")==0 || strcmp(name, "c")==0)
	{
		sync_corr = atoi(optarg);
	}
	else if (strcmp(name, "mjpeg-buffers")==0 || strcmp(name, "n")==0)
	{
		MJPG_nbufs = atoi(optarg);
		if(MJPG_nbufs<4) MJPG_nbufs = 4;
		if(MJPG_nbufs>256) MJPG_nbufs = 256;
	}
	else if (strcmp(name, "t")==0)
	{
		test_factor = atof(optarg);
		if(test_factor<=0) test_factor = 1.0;
	}
	else if (strcmp(name, "no-quit")==0 || strcmp(name, "q")==0)
	{
		quit_at_end = 0;
	}
	else if (strcmp(name, "exchange-fields")==0 || strcmp(name, "x")==0)
	{
		exchange_fields = 1;
	}
	else if (strcmp(name, "zoom")==0 || strcmp(name, "z")==0)
	{
		zoom_to_fit = 1;
	}
	else if (strcmp(name, "gui-mode")==0 || strcmp(name, "g")==0)
	{
		gui_mode = 1;
	}
	else if (strcmp(name, "full-screen")==0 || strcmp(name, "Z")==0)
	{
		soft_fullscreen = 1;
	}
	else if (strcmp(name, "playback")==0 || strcmp(name, "p")==0)
	{
		switch (value[0])
		{
			case 'S':
				mjpeg_info("Choosing software MJPEG playback\n");
				screen_output = 0;
				soft_play = 1;
				break;
			case 'H':
				mjpeg_info("Choosing hardware MJPEG playback (on-screen)\n");
				screen_output = 1;
				soft_play = 0;
				break;
			case 'C':
				mjpeg_info("Choosing hardware MJPEG playback (on-screen)\n");
				screen_output = 0;
				soft_play = 0;
				break;
		}
	}
	else if (strcmp(name, "size")==0)
	{
		if (sscanf(value, "%dx%d", &soft_width, &soft_height)!=2)
		{
			mjpeg_error( "--size parameter requires NxN argument\n");
			nerr++;
		}
	}
	else nerr++; /* unknown option - error */

	return nerr;
}

static void check_command_line_options(int argc, char *argv[])
{
	int nerr,n,option_index=0;
	char option[2];

	/* getopt_long options */
	static struct option long_options[]={
        {"verbose"       ,1,0,0},     /* -v/--verbose         */
		{"norm"            ,1,0,0},   /* -o/--norm            */
		{"h-offset"        ,1,0,0},   /* -H/--H-offset        */
		{"v-offset"        ,1,0,0},   /* -V/--V-offset        */
		{"skip"            ,1,0,0},   /* -s/--skip            */
		{"synchronization" ,1,0,0},   /* -c/--synchronization */
		{"mjpeg-buffers"   ,1,0,0},   /* -n/--mjpeg-buffers   */
		{"no-quit"         ,0,0,0},   /* -q/--no-quit         */
		{"exchange-fields" ,0,0,0},   /* -x/--exchange-fields */
		{"zoom"            ,0,0,0},   /* -z/--zoom            */
		{"full-screen"     ,0,0,0},   /* -Z/--full-screen     */
		{"playback"        ,1,0,0},   /* -p/--playback [SHC]  */
		{"audio"           ,1,0,0},   /* -a/--audio [01]      */
		{"gui-mode"        ,1,0,0},   /* -g/--gui-mode        */
		{"size"            ,1,0,0},   /* --size               */
		{0,0,0,0}
	};

	if(argc < 2) Usage(argv[0]);

	/* Get options */
	nerr = 0;
	while( (n=getopt_long(argc,argv,"a:v:H:V:s:c:n:t:qZp:xrzg",
		long_options, &option_index)) != EOF)
	{
		switch(n)
		{
			/* getopt_long values */
			case 0:
				nerr += set_option((char *)long_options[option_index].name,
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

	/* Get and open input files */
	read_video_files(argv + optind, argc - optind, &el);

	/* Set min/max options so that it runs like it should */
	min_frame_num = 0;
	max_frame_num = el.video_frames-1;
}

int main(int argc, char ** argv)
{
	int res, frame, hn;
	long nqueue, nsync, first_free;
	struct timeval audio_tmstmp;
	struct timeval time_now;
	int nb_out, nb_err;
	long audio_buffer_size = 0;
	uint8_t * buff;
	int n, skipv, skipa, skipi;
	double tdiff, tdiff1, tdiff2;
	char input_buffer[256];
	long frame_number[256]; /* Must be at least as big as the number of buffers used */
	struct mjpeg_params bp;
	struct mjpeg_sync bs;
	struct sigaction action, old_action;
   	struct video_capability vc;

	/* Output Version information - Used by xlav to check for
	 * consistency. 
	 */
	printf( LAVPLAY_VSTR "\n" );
	fflush(stdout);
	printf( "lavtools version " VERSION "\n" );

	/* command-line options */
        check_command_line_options(argc, argv);

	/* Seconds per video frame: */
	spvf = 1.0 / el.video_fps;
	mjpeg_debug( "1.0/SPVF = %4.4f\n", 1.0 / spvf );

	/* Seconds per audio sample: */
	if(el.has_audio && audio_enable)
		spas = 1.0/el.audio_rate;
	else
		spas = 0.;

	/* Seek starting position */
	nframe = 0;
	n = skip_seconds/spvf;
	res = inc_frames(n);
	if(res)
	{
		mjpeg_error_exit1("Start position is behind all files\n");
	}

	/* allocate auxiliary video buffer for flicker reduction */
	tmpbuff[0] = (char*) malloc(el.max_frame_size);
	tmpbuff[1] = (char*) malloc(el.max_frame_size);
	if(tmpbuff[0]==0 || tmpbuff[1]==0) malloc_error();

	if (soft_play)
		mjpeg = mjpeg_open(MJPEG_OUTPUT_SOFTWARE, MJPG_nbufs, el.max_frame_size);
	else 
		if (screen_output)
			mjpeg = mjpeg_open(MJPEG_OUTPUT_HARDWARE_SCREEN, MJPG_nbufs, el.max_frame_size);
		else
			mjpeg = mjpeg_open(MJPEG_OUTPUT_HARDWARE_VIDEO, MJPG_nbufs, el.max_frame_size);

	buff = mjpeg_get_io_buffer(mjpeg);

	/* initialize SDL */
	if (soft_play)
		initialize_SDL_window();
	if (el.has_audio && audio_enable)
	{
		res = audio_init(0,
						 opt_use_read,
						 (el.audio_chans>1),el.audio_bits,
						 (int)(el.audio_rate*test_factor));
		if(res)
		{
			mjpeg_error_exit1("Error initializing Audio: %s\n",audio_strerror());
		}

		audio_buffer_size = audio_get_buffer_size();

		/* Ensure proper exit processing for audio */
		atexit(audio_shutdown);
	}

	signal(SIGINT, x_shutdown);

   /* After we have fired up the audio and video threads system (which
	  are assisted if we're installed setuid root, we want to set the
	  effective user id to the real user id */
   if( seteuid( getuid() ) < 0 )
	   system_error("Can't set effective user-id: %s\n",
					(char *)sys_errlist[errno]);

   /* Fill all buffers first */

	for(nqueue = 0; nqueue < mjpeg->br.count; nqueue++)
	{
		frame_number[nqueue] = nframe;
		if(queue_next_frame(buff+nqueue* mjpeg->br.size,0,0,0)) break;
	}

	/* Choose the correct parameters for playback */

	mjpeg_get_params(mjpeg, &bp);
   
	bp.input = 0;

   /* Set norm */       
	bp.norm = (el.video_norm == 'n') ? VIDEO_MODE_NTSC : VIDEO_MODE_PAL;
	mjpeg_info("Output norm: %s\n",bp.norm?"NTSC":"PAL");
   
	hn = bp.norm ? 480 : 576;  /* Height of norm */
   
	bp.decimation = 0; /* we will set proper params ourselves */

	/* Check dimensions of video, select decimation factors */
	if (!soft_play && !screen_output)
	{		
		/* set correct width of device for hardware
		   DC10(+): 768 (PAL/SECAM) or 640 (NTSC), Buz/LML33: 720*/
		res = ioctl(mjpeg->dev, VIDIOCGCAP,&vc);
		if (res < 0) 
			mjpeg_error_exit1("getting device capabilities: %s\n",sys_errlist[errno]);
		playback_width = vc.maxwidth;

		if( el.video_width > playback_width || el.video_height > hn )
		{
			/* This is definitely too large */
	 
			mjpeg_error_exit1("Video dimensions too large: %ld x %ld\n",
					el.video_width,el.video_height);
		}
	}
       
	/* if zoom_to_fit is set, HorDcm is independent of interlacing */
       
	if(zoom_to_fit)
	{
		if ( el.video_width < playback_width/4 )
			bp.HorDcm = 4;
		else if ( el.video_width < playback_width/2 )
			bp.HorDcm = 2;
		else
			bp.HorDcm = 1;
	}
   
	if(el.video_inter)
	{
		/* Interlaced video, 2 fields per buffer */
       
		bp.field_per_buff = 2;
		bp.TmpDcm = 1;
	   
		if(zoom_to_fit)
		{
			if( el.video_height <= hn/2 )
				bp.VerDcm = 2;
			else
				bp.VerDcm = 1;
		}
		else
		{
			/* if zoom_to_fit is not set, we always use decimation 1 */
			bp.HorDcm = 1;
			bp.VerDcm = 1;
		}
	}
	else
	{
		/* Not interlaced, 1 field per buffer */
       
		bp.field_per_buff = 1;
		bp.TmpDcm = 2;
       
		if (!soft_play && !screen_output &&  
			( el.video_height > hn/2 || (!zoom_to_fit && el.video_width>playback_width/2) ))
		{

			mjpeg_error("Video dimensions (not interlaced) too large: %ld x %ld\n",
						  el.video_width,el.video_height);
			if(el.video_width>playback_width/2) 
				mjpeg_error("Try -z option !!!!\n");
			exit(1);
		}
       
		if(zoom_to_fit)
		{
			if( el.video_height <= hn/4 )
				bp.VerDcm = 2;
			else
				bp.VerDcm = 1;
		}
		else
		{
			/* the following is equivalent to decimation 2 in lavrec: */
			bp.HorDcm = 2;
			bp.VerDcm = 1;
		}
	}
   
	/* calculate height, width and offsets from the above settings */
   
	bp.quality = 100;
	bp.img_width  = bp.HorDcm*el.video_width;
	bp.img_height = bp.VerDcm*el.video_height/bp.field_per_buff;
	bp.img_x = (playback_width  - bp.img_width )/2 + h_offset;
	bp.img_y = (hn/2 - bp.img_height)/2 + v_offset/2;
   
	mjpeg_info("Output dimensions: %dx%d+%d+%d\n",
			   bp.img_width,bp.img_height*2,bp.img_x,bp.img_y*2);
	mjpeg_info("Output zoom factors: %d (hor) %d (ver)\n",
			bp.HorDcm,bp.VerDcm*bp.TmpDcm);
   
   /* Set field polarity for interlaced video */
   
	bp.odd_even = (el.video_inter==LAV_INTER_ODD_FIRST);
	if(exchange_fields) bp.odd_even = !bp.odd_even;

	mjpeg_set_params(mjpeg, &bp);

	/* Set target play-back frame-rate */
	mjpeg_set_playback_rate( mjpeg, el.video_fps );

	/* Make stdin nonblocking */
	memset(&action, 0, sizeof(action));

	action.sa_handler = sig_cont;
	action.sa_flags = SA_RESTART;


	sigaction(SIGCONT, &action, &old_action);

	sig_cont(0);

	/* Queue the buffers read, this triggers video playback */

	if (el.has_audio && audio_enable) audio_start();


	for(n = 0; n < nqueue; n++)
	{
		mjpeg_queue_buf(mjpeg, n, 1);
	}

	/* Playback loop */

	nsync  = 0;
	tdiff1 = 0.0;
	tdiff2 = 0.0;
	tdiff  = 0.0;
	play_speed = 1;				/* In case we already reached end seq */

	while(1)
	{
		/* Sync to get a free buffer. We want to have all free buffers,
		   which have been played so far. Normally there should be a function
		   in the kernel API to get the number of all free buffers.
		   I don't want to change this API at the moment, therefore
		   we look on the clock to see if there are more buffers ready */

		first_free = nsync;
		do
		{
			mjpeg_sync_buf(mjpeg, &bs);

			frame = bs.frame;
			/* Since we queue the frames in order, we have to get them back in order */
			if(frame != nsync % mjpeg->br.count)
			{
				mjpeg_error("INTERNAL: Bad frame order on sync: frame = %d, nsync = %ld, mjpeg->br.count = %ld\n", 
							frame, nsync, mjpeg->br.count);
				x_shutdown(1);
				exit(1);
			}
			nsync++;
			/* Look on clock */
			gettimeofday(&time_now,0);
			tdiff =  time_now.tv_sec  - bs.timestamp.tv_sec +
				(time_now.tv_usec - bs.timestamp.tv_usec)*1.e-6;
		}
		while(tdiff>spvf && (nsync-first_free)<mjpeg->br.count-1);

		if(gui_mode) 
			printf("@%c%ld/%ld/%d\n",(unsigned int)el.video_norm,frame_number[frame],
				   el.video_frames,play_speed);

		if((nsync-first_free)> mjpeg->br.count-3)
			mjpeg_warn("Disk too slow, can not keep pace!\n");

		if(el.has_audio && audio_enable)
		{
			audio_get_output_status(&audio_tmstmp,&nb_out,&nb_err);
			if(audio_tmstmp.tv_sec)
			{
				tdiff1 = spvf*(nsync-nvcorr) - spas*audio_buffer_size/el.audio_bps*nb_out;
				tdiff2 = (bs.timestamp.tv_sec -audio_tmstmp.tv_sec )
					+ (bs.timestamp.tv_usec-audio_tmstmp.tv_usec)*1.e-6;
			}
		}

		tdiff = tdiff1-tdiff2;
		//mjpeg_warn( "ns =%ld TD =%f\n", nsync, tdiff );
      /* Fill and queue free buffers again */

		res = 0;
		for(n=first_free;n<nsync;)
		{
			/* Audio/Video sync correction */

			skipv = 0;
			skipa = 0;
			skipi = 0;

			if(sync_corr)
			{
				if(tdiff>spvf)
				{
					/* Video is ahead audio */
					skipa = 1;
					if(sync_ins_frames) skipi = 1;
					nvcorr++;
					numca++;
					tdiff-=spvf;
				}
				if(tdiff<-spvf)
				{
					/* Video is behind audio */
					skipv = 1;
					if(!sync_skip_frames) skipi = 1;
					nvcorr--;
					numcb++;
					tdiff+=spvf;
				}
			}

			/* Read one frame, break if EOF is reached */

			frame = n%mjpeg->br.count;
			frame_number[frame] = nframe;
			res = queue_next_frame(buff+frame*mjpeg->br.size,skipv,skipa,skipi);
			if(res) break;
			if(skipv) continue; /* no frame has been read */

			/* Queue the frame */

			mjpeg_queue_buf(mjpeg, frame, 1);

			nqueue++;
			n++;
		}
		if (res) break;

      /* See if we got input */

		res = read(0,input_buffer,256);
		if(res>0)
			if (process_edit_input(input_buffer, res) < 0)
				break;
		fflush(stdout);
		fflush(stderr);
	}

	/* All buffers are queued, sync on the outstanding buffers */
	/* Never try to sync on the last buffer, it is a hostage of
	   the codec since it is played over and over again */

	while(nqueue > nsync + 1)
	{
		mjpeg_sync_buf(mjpeg, &bs);
		nsync++;
	}

	/* Stop streaming playback */
	mjpeg_close(mjpeg);
	if (soft_play)
	{
		free(jpeg_data);
		SDL_FreeYUVOverlay(yuv_overlay);
		SDL_Quit();
	}

	if(sync_corr)
	{
		mjpeg_info("Corrections because video ahead  audio: %4d\n",numca);
		mjpeg_info("Corrections because video behind audio: %4d\n",numcb);
	}

	exit(0);
}







