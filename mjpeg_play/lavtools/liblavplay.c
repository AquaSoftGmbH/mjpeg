/*
 * liblavplay - a librarified Linux Audio Video PLAYback
 *
 * Copyright (C) 2000 Rainer Johanni <Rainer@Johanni.de>
 * Extended by:   Gernot Ziegler  <gz@lysator.liu.se>
 *                Ronald Bultje   <rbultje@ronald.bitfreak.net>
 *              & many others
 *
 * A library for playing back MJPEG video via software MJPEG
 * decompression (using SDL) or via hardware MJPEG video
 * devices such as the Pinnacle/Miro DC10(+), Iomega Buz,
 * the Linux Media Labs LML33, the Matrox Marvel G200,
 * Matrox Marvel G400 and the Rainbow Runner G-series.
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <config.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/vfs.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#ifndef IRIX 
#include <linux/videodev.h>
#include <linux/soundcard.h>
#else
#define VIDEO_MODE_PAL		0
#define VIDEO_MODE_NTSC		1
#define VIDEO_MODE_SECAM	2
#define VIDEO_MODE_AUTO		3
#endif

#include <videodev_mjpeg.h>
#include <pthread.h>
#include <SDL/SDL.h>

#include "mjpeg_types.h"
#include "liblavplay.h"
#include "audiolib.h"
//#include "lav_io.h"
//#include "editlist.h"
#include "jpegutils.h"


/* On some systems MAP_FAILED seems to be missing */
#ifndef MAP_FAILED
#define MAP_FAILED ( (caddr_t) -1 )
#endif

#define MJPEG_MAX_BUF 64

#define HZ 100

#define VALUE_NOT_FILLED -10000

typedef struct {
   char   *buff;                              /* the buffer for JPEG frames */
   int    video_fd;                           /* the file descriptor for the video device */
   struct mjpeg_requestbuffers br;            /* buffer requests */
   char   *tmpbuff[2];                        /* buffers for flicker reduction */
   double spvf;                               /* seconds per video frame */
   int    usec_per_frame;                     /* milliseconds per frame */
   int    min_frame_num;                      /* the lowest frame to be played back - normally 0 */
   int    max_frame_num;                      /* the latest frame to be played back - normally num_frames - 1 */
   int    current_frame_num;                  /* the current frame */
   int    current_playback_speed;             /* current playback speed */

   long   old_field_len;
   int    old_buff_no;
   int    currently_processed_frame;          /* changes constantly */
   int    currently_synced_frame;             /* changes constantly */
   int    show_top;                           /* changes constantly */
   int    first_frame;                        /* software sync variable */
   struct timeval lastframe_completion;       /* software sync variable */

   SDL_Surface *screen;                       /* the screen object used for software playback */
   SDL_Rect jpegdims;                         /* a SDL rectangle */
   SDL_Overlay *yuv_overlay;                  /* SDL YUV overlay */

   pthread_t software_playback_thread;        /* the thread for software playback */
   pthread_mutex_t valid_mutex;
   int valid[MJPEG_MAX_BUF];                  /* Non-zero if buffer has been filled - num of frames to be played */
   pthread_cond_t buffer_filled[MJPEG_MAX_BUF];
   pthread_cond_t buffer_done[MJPEG_MAX_BUF];
   pthread_mutex_t syncinfo_mutex;
#if defined(SUPPORT_READ_DV2) || defined(SUPPORT_READ_YUV420)
   int data_format[MJPEG_MAX_BUF];
#endif

   struct mjpeg_sync syncinfo[MJPEG_MAX_BUF]; /* synchronization info */

   unsigned long *save_list;                  /* for editing purposes */
   long save_list_len;                        /* for editing purposes */

   char   abuff[16384];                       /* the audio buffer */
   double spas;                               /* seconds per audio sample */
   long   audio_buffer_size;                  /* audio stream buffer size */
   int    audio_mute;                         /* controls whether to currently play audio or not */

   int    state;                              /* playing, paused or stoppped */
   pthread_t playback_thread;                 /* the thread for the whole playback-library */
} video_playback_setup;


/******************************************************
 * lavplay_msg()
 *   simplicity function which will give messages
 ******************************************************/

static void lavplay_msg(int type, lavplay_t *info, const char format[], ...) GNUC_PRINTF(3,4);
static void lavplay_msg(int type, lavplay_t *info, const char format[], ...)
{
   if (!info)
   {
      /* we can't let errors pass without giving notice */
      char buf[1024];
      va_list args;

      va_start(args, format);
      vsnprintf(buf, sizeof(buf)-1, format, args);

      printf("**ERROR: %s\n", buf);

      va_end(args);
   }
   else if (info->msg_callback)
   {
      char buf[1024];
      va_list args;

      va_start(args, format);
      vsnprintf(buf, sizeof(buf)-1, format, args);

      info->msg_callback(type, buf);

      va_end(args);
   }
   else if (type == LAVPLAY_MSG_ERROR)
   {
      /* we can't let errors pass without giving notice */
      char buf[1024];
      va_list args;

      va_start(args, format);
      vsnprintf(buf, sizeof(buf)-1, format, args);

      printf("**ERROR: %s\n", buf);

      va_end(args);
   }
}


/******************************************************
 * lavplay_change_state()
 *   change the state
 ******************************************************/

static void lavplay_change_state(lavplay_t *info, int new_state)
{
   video_playback_setup *settings = (video_playback_setup *)info->settings;
   //EditList *editlist = info->editlist;

   settings->state = new_state;

   if (info->state_changed)
      info->state_changed(new_state);
}


/******************************************************
 * lavplay_set_speed()
 *   set the playback speed (<0 is play backwards)
 *
 * return value: 1 on success, 0 on error
 ******************************************************/

int lavplay_set_speed(lavplay_t *info, int speed)
{
   video_playback_setup *settings = (video_playback_setup *)info->settings;
   //EditList *editlist = info->editlist;
   int changed = 0;

   if ((settings->current_frame_num == settings->max_frame_num && speed > 0) ||
      (settings->current_frame_num == settings->min_frame_num && speed < 0))
   {
      lavplay_msg(LAVPLAY_MSG_WARNING, info,
         "We are already at the %s", speed<0?"beginning":"end");
      return 0;
   }

   if ((speed==0 && settings->current_playback_speed!=0) ||
      (speed!=0 && settings->current_playback_speed==0))
      changed = 1;

   settings->current_playback_speed = speed;

   if (changed)
   {
      if (speed==0)
         lavplay_change_state(info, LAVPLAY_STATE_PAUSED);
      else
         lavplay_change_state(info, LAVPLAY_STATE_PLAYING);
   }

   return 1;
}


/******************************************************
 * lavplay_increase_frame()
 *   increase (or decrease) a num of frames
 *
 * return value: 1 on succes, 0 if we had to change state
 ******************************************************/

int lavplay_increase_frame(lavplay_t *info, long num)
{
   video_playback_setup *settings = (video_playback_setup *)info->settings;
   //EditList *editlist = info->editlist;

   settings->current_frame_num += num;
   if (settings->current_frame_num < settings->min_frame_num)
   {
      settings->current_frame_num = settings->min_frame_num;
      if (settings->current_playback_speed < 0) lavplay_set_speed(info, 0);
      return 0;
   }
   if (settings->current_frame_num > settings->max_frame_num)
   {
      settings->current_frame_num = settings->max_frame_num;
      if (settings->current_playback_speed > 0) lavplay_set_speed(info, 0);
      return 0;
   }
   return 1;
}


/******************************************************
 * lavplay_set_frame()
 *   set the current framenum
 *
 * return value: 1 on success, 0 if we had to change state
 ******************************************************/

int lavplay_set_frame(lavplay_t *info, long framenum)
{
   video_playback_setup *settings = (video_playback_setup *)info->settings;
   //EditList *editlist = info->editlist;

   return lavplay_increase_frame(info, framenum - settings->current_frame_num);
}


/******************************************************
 * lavplay_get_video()
 *   get video data
 *
 * return value: length of frame on success, -1 on error
 ******************************************************/

static int lavplay_get_video(lavplay_t *info, char *buff, long frame_num)
{
   if (info->get_video_frame)
   {
      int length;
      info->get_video_frame(buff, &length, frame_num);
      return length;
   }
   else
   {
      return el_get_video_frame(buff, frame_num, info->editlist);
   }
}


/******************************************************
 * lavplay_get_audio()
 *   get audio data
 *
 * return value: number of samples on success, -1 on error
 ******************************************************/

static int lavplay_get_audio(lavplay_t *info, char *buff, long frame_num, int mute)
{
   if (info->get_audio_sample)
   {
      int num_samps;
      info->get_audio_sample(buff, &num_samps, frame_num);
      return num_samps;
   }
   else
   {
      return el_get_audio_data(buff, frame_num, info->editlist, mute);
   }
}


/******************************************************
 * lavplay_queue_next_frame()
 *   queues a frame (video + audio)
 *
 * return value: 1 on succes, 0 on error
 ******************************************************/

static int lavplay_queue_next_frame(lavplay_t *info, char *vbuff,
#if defined(SUPPORT_READ_DV2) || defined(SUPPORT_READ_YUV420)
				    int data_format,
#endif
				    int skip_video, int skip_audio, int skip_incr)
{
   int res, mute, i, jpeg_len1, jpeg_len2, new_buff_no;
   char hlp[16];
   video_playback_setup *settings = (video_playback_setup *)info->settings;
   EditList *editlist = info->editlist;

   /* Read next frame */
   if (!skip_video)
   {
      if (info->flicker_reduction && editlist->video_inter &&
#if defined(SUPPORT_READ_DV2) || defined(SUPPORT_READ_YUV420)
	  data_format == DATAFORMAT_MJPG &&
#endif	  
	  settings->current_playback_speed <= 0)
      {
         if (settings->current_playback_speed == 0)
         {
            if ((res = lavplay_get_video(info, vbuff, settings->current_frame_num)) < 0)
               return 0;
            jpeg_len1 = lav_get_field_size(vbuff, res);

            /* Found seperate fields? */
            if (jpeg_len1 < res)
            {
               memcpy(vbuff+jpeg_len1, vbuff, jpeg_len1);
               settings->old_field_len = 0;
            }
         }
         else /* play_speed < 0, play old first field + actual second field */
         {
            new_buff_no = 1 - settings->old_buff_no;
            if ((res = lavplay_get_video(info, settings->tmpbuff[new_buff_no],
               settings->current_frame_num)) < 0)
               return 0;
            jpeg_len1 = lav_get_field_size(settings->tmpbuff[new_buff_no], res);
            if (jpeg_len1 < res)
            {
               jpeg_len2 = res - jpeg_len1;
               if (settings->old_field_len==0)
               {
                  /* no old first field, duplicate second field */
                  memcpy(vbuff,settings->tmpbuff[new_buff_no]+jpeg_len1,jpeg_len2);
                  settings->old_field_len = jpeg_len2;
               }
               else
               {
                  /* copy old first field into vbuff */
                  memcpy(vbuff,settings->tmpbuff[settings->old_buff_no],settings->old_field_len);
               }
               /* copy second field */
               memcpy(vbuff+settings->old_field_len,settings->tmpbuff[new_buff_no]+jpeg_len1,jpeg_len2);
               /* save first field */
               settings->old_field_len = jpeg_len1;
               settings->old_buff_no = new_buff_no;
            }
         }
      }
      else
      {
         if (lavplay_get_video(info, vbuff, settings->current_frame_num) < 0)
            return 0;
         settings->old_field_len = 0;
      }
   }

   /* Read audio, if present */
   if (editlist->has_audio && !skip_audio && info->audio)
   {
      mute = settings->current_playback_speed==0 || settings->audio_mute;
         //(settings->audio_mute &&
         //settings->current_playback_speed!=1 &&
         //settings->current_playback_speed!=-1);
      res = lavplay_get_audio(info, settings->abuff,
         settings->current_frame_num, mute);

      if (settings->current_playback_speed < 0)
      {
         /* reverse audio */
         for(i=0;i<res/2;i+=editlist->audio_bps)
         {
            memcpy(hlp,settings->abuff+i, editlist->audio_bps);
            memcpy(settings->abuff+i,settings->abuff+res-i-editlist->audio_bps, editlist->audio_bps);
            memcpy(settings->abuff+res-i-editlist->audio_bps,hlp, editlist->audio_bps);
         }
      }
      res = audio_write(settings->abuff,res,0);
      if (res < 0)
      {
         lavplay_msg(LAVPLAY_MSG_ERROR, info,
            "Error playing audio: %s",audio_strerror());
         return 0;
      }
   }

   /* Increment frames */
   if(!skip_incr)
   {
      res = lavplay_increase_frame(info, settings->current_playback_speed);
      if (!info->continuous) return res;
   }

   return 1;
}


/******************************************************
 * lavplay_SDL_lock()
 *   when using software playback - lock the SDL screen
 *
 * return value: 1 on success, 0 on error
 ******************************************************/

static int lavplay_SDL_lock(lavplay_t *info)
{
   video_playback_setup *settings = (video_playback_setup *)info->settings;

   /* lock the screen for current decompression */
   if (SDL_MUSTLOCK(settings->screen)) 
   {
      if (SDL_LockSurface(settings->screen) < 0)
      {
         lavplay_msg(LAVPLAY_MSG_ERROR, info,
            "Error locking output screen: %s", SDL_GetError());
         return 0;
      }
   }
   if (SDL_LockYUVOverlay(settings->yuv_overlay) < 0)
   {
      lavplay_msg(LAVPLAY_MSG_ERROR, info,
         "Error locking yuv overlay: %s", SDL_GetError());
      return 0;
   }

   return 1;
}


/******************************************************
 * lavplay_SDL_unlock()
 *   when using software playback - unlock the SDL screen
 *
 * return value: 1 on success, 0 on error
 ******************************************************/

static int lavplay_SDL_unlock(lavplay_t *info)
{
   video_playback_setup *settings = (video_playback_setup *)info->settings;

   if (SDL_MUSTLOCK(settings->screen)) 
   {
      SDL_UnlockSurface(settings->screen);
   }
   SDL_UnlockYUVOverlay(settings->yuv_overlay);

   return 1;
}


/******************************************************
 * lavplay_SDL_update()
 *   when using software playback - there's a new frame
 *
 * return value: 1 on success, 0 on error
 ******************************************************/

static int lavplay_SDL_update(lavplay_t *info, uint8_t *jpeg_buffer,
#if defined(SUPPORT_READ_DV2) || defined(SUPPORT_READ_YUV420)
			      int data_format,
#endif
			      int buf_len)
{
   video_playback_setup *settings = (video_playback_setup *)info->settings;
   EditList *editlist = info->editlist;

   if (!lavplay_SDL_lock(info)) return 0;

   /* decode frame to yuv */
#if defined(SUPPORT_READ_DV2) || defined(SUPPORT_READ_YUV420)
   switch (data_format) {
   case DATAFORMAT_MJPG:
#endif
   decode_jpeg_raw (jpeg_buffer, buf_len, editlist->video_inter, CHROMA420,
      editlist->video_width, editlist->video_height, 
      settings->yuv_overlay->pixels[0], 
      settings->yuv_overlay->pixels[1],
      settings->yuv_overlay->pixels[2]);
#if defined(SUPPORT_READ_DV2) || defined(SUPPORT_READ_YUV420)
      break;
# ifdef SUPPORT_READ_YUV420
   case DATAFORMAT_YUV420:
      memcpy(settings->yuv_overlay->pixels[0],
	     jpeg_buffer,
	     editlist->video_width * editlist->video_height);
      memcpy(settings->yuv_overlay->pixels[1],
	     jpeg_buffer + (editlist->video_width * editlist->video_height),
	     (editlist->video_width * editlist->video_height / 4));
      memcpy(settings->yuv_overlay->pixels[2],
	     jpeg_buffer + (editlist->video_width * editlist->video_height * 5 / 4),
	     (editlist->video_width * editlist->video_height / 4));
      break;
# endif
# ifdef SUPPORT_READ_DV2
   case DATAFORMAT_DV2:
     /* FIXME: not implemented, do default: */
# endif
   default:
     return 0;
   }
#endif

   if (!lavplay_SDL_unlock(info)) return 0;
   SDL_DisplayYUVOverlay(settings->yuv_overlay, &(settings->jpegdims));

   return 1;
}


/******************************************************
 * lavplay_SDL_init()
 *   when using software playback - initialize SDL
 *
 * return value: 1 on success, 0 on error
 ******************************************************/

static int lavplay_SDL_init(lavplay_t *info)
{
   char *sbuffer;
   int i;
   video_playback_setup *settings = (video_playback_setup *)info->settings;
   EditList *editlist = info->editlist;

   lavplay_msg(LAVPLAY_MSG_INFO, info,
      "Initialising SDL");
   if (SDL_Init (SDL_INIT_VIDEO) < 0)
   {
      lavplay_msg(LAVPLAY_MSG_ERROR, info,
         "SDL Failed to initialise...");
      return 0;
   }

   /* Now initialize SDL */
   if (info->soft_full_screen)
      settings->screen = SDL_SetVideoMode(info->sdl_width, info->sdl_height, 0,
         SDL_HWSURFACE | SDL_FULLSCREEN);
   else
      settings->screen = SDL_SetVideoMode(info->sdl_width, info->sdl_height, 0,
         SDL_HWSURFACE);
   if (!settings->screen)
   {
      lavplay_msg(LAVPLAY_MSG_ERROR, info,
         "SDL: Output screen error: %s", SDL_GetError());
      return 0;
   }

   SDL_EventState(SDL_KEYDOWN, SDL_ENABLE);
   SDL_EventState(SDL_MOUSEMOTION, SDL_IGNORE);

   settings->yuv_overlay = SDL_CreateYUVOverlay(editlist->video_width,
      editlist->video_height, SDL_IYUV_OVERLAY, settings->screen);
   if (!settings->yuv_overlay)
   {
      lavplay_msg(LAVPLAY_MSG_ERROR, info,
         "SDL: Couldn't create SDL_yuv_overlay: %s", SDL_GetError());
      return 0;
   }
   lavplay_msg(LAVPLAY_MSG_INFO, info,
      "SDL YUV overlay: %s", settings->yuv_overlay->hw_overlay ? "hardware" : "software" );
   if(settings->yuv_overlay->pitches[0] != settings->yuv_overlay->pitches[1]*2 ||
      settings->yuv_overlay->pitches[0] != settings->yuv_overlay->pitches[2]*2 )
   {
      lavplay_msg(LAVPLAY_MSG_ERROR, info,
         "SDL returned non-YUV420 overlay!");
      return 0;
   }

   settings->jpegdims.x = 0; /* This is not going to work with interlaced pics !! */
   settings->jpegdims.y = 0;
   settings->jpegdims.w = info->sdl_width;
   settings->jpegdims.h = info->sdl_height;

   /* Lock the screen to test, and to be able to access screen->pixels */
   if (!lavplay_SDL_lock(info)) return 0;
       
   /* Draw bands of color on the raw surface, as run indicator for debugging */
   sbuffer = (char *)settings->screen->pixels;
   for ( i=0; i < settings->screen->h; ++i ) 
   {
      memset(sbuffer,(i*255)/settings->screen->h,
         settings->screen->w * settings->screen->format->BytesPerPixel);
      sbuffer += settings->screen->pitch;
   }

   /* Set the windows title */
   SDL_WM_SetCaption("Lavplay Video Playback", "0000000");  

   /* unlock, update and wait for the fun to begin */
   if (!lavplay_SDL_unlock(info)) return 0;
   SDL_UpdateRect(settings->screen, 0, 0, settings->jpegdims.w, settings->jpegdims.h);

   return 1;
}


/******************************************************
 * lavplay_mjpeg_software_frame_sync()
 *   Try to keep in sync with nominal frame rate,
 *     timestamp frame with actual completion time
 *     (after any deliberate sleeps etc)
 *
 * return value: 1 on success, 0 on error
 ******************************************************/

static void lavplay_mjpeg_software_frame_sync(lavplay_t *info, int frame_periods)
{
   int usec_since_lastframe;
   struct timeval now;
   struct timespec nsecsleep;
   video_playback_setup *settings = (video_playback_setup *)info->settings;
   //EditList *editlist = info->editlist;

   /* I really *wish* the timer was higher res on x86 Linux... 10mSec
    * is a *pain*.  Sooo wasteful here...
    */
   for (;;)
   {
      gettimeofday( &now, 0 );
      usec_since_lastframe = now.tv_usec - settings->lastframe_completion.tv_usec;
      if ( usec_since_lastframe< 0)
         usec_since_lastframe+= 1000000;
      if( now.tv_sec > settings->lastframe_completion.tv_sec+1 )
         usec_since_lastframe= 1000000;
      if( settings->first_frame ||
         frame_periods*settings->usec_per_frame-usec_since_lastframe < (1000000)/HZ )
         break;

      /* Assume some other process will get a time-slice before
       * we do... and hence the worst-case delay of 1/HZ after
       * sleep timer expiry will apply. Reasonable since X will
       * probably do something...
       */
      nsecsleep.tv_nsec = 
         (frame_periods*settings->usec_per_frame-usec_since_lastframe-1000000/HZ)*1000;
      nsecsleep.tv_sec = 0;
      nanosleep( &nsecsleep, NULL );
   }
   settings->first_frame = 0;

   /* We are done with writing the picture - Now update all surrounding info */
   gettimeofday( &(settings->lastframe_completion), 0 );
   settings->syncinfo[settings->currently_processed_frame].timestamp = settings->lastframe_completion;
}


/******************************************************
 * lavplay_mjpeg_playback_thread()
 *   the main (software) video playback thread
 *
 * return value: 1 on success, 0 on error
 ******************************************************/

static void *lavplay_mjpeg_playback_thread(void * arg)
{
   lavplay_t *info = (lavplay_t *)arg;
   video_playback_setup *settings = (video_playback_setup *)info->settings;
   //EditList *editlist = info->editlist;

   lavplay_msg(LAVPLAY_MSG_DEBUG, info,
      "Starting software playback thread");

   /* Allow easy shutting down by other processes... */
   pthread_setcancelstate( PTHREAD_CANCEL_ENABLE, NULL );
   pthread_setcanceltype( PTHREAD_CANCEL_ASYNCHRONOUS, NULL );

   while (settings->state != LAVPLAY_STATE_STOP)
   {
      pthread_mutex_lock(&(settings->valid_mutex));
      while (settings->valid[settings->currently_processed_frame] == 0)
      {
         lavplay_msg(LAVPLAY_MSG_DEBUG, info,
            "Playback thread: sleeping for new frames (waiting for frame %d)", 
            settings->currently_processed_frame);
         pthread_cond_wait(&(settings->buffer_filled[settings->currently_processed_frame]),
            &(settings->valid_mutex));
         if (settings->state == LAVPLAY_STATE_STOP)
         {
            /* Ok, we shall exit, that's the reason for the wakeup */
            lavplay_msg(LAVPLAY_MSG_DEBUG, info,
               "Playback thread: was told to exit");
            pthread_exit(NULL);
         }
      }
      pthread_mutex_unlock(&(settings->valid_mutex));

      /* There is one buffer to play - get ready to rock ! */
      if (!lavplay_SDL_update(info, settings->buff+settings->currently_processed_frame*settings->br.size,
#if defined(SUPPORT_READ_DV2) || defined(SUPPORT_READ_YUV420)
         settings->data_format[settings->currently_processed_frame],
#endif
         settings->br.size))
      {
         /* something went wrong - give a warning (don't exit yet) */
         lavplay_msg(LAVPLAY_MSG_WARNING, info,
            "Error playing a frame");
      }

      /* Synchronise and timestamp current frame after sync */
      lavplay_mjpeg_software_frame_sync(info, settings->valid[settings->currently_processed_frame]);
      settings->syncinfo[settings->currently_processed_frame].frame = settings->currently_processed_frame;

      pthread_mutex_lock(&(settings->valid_mutex));
      settings->valid[settings->currently_processed_frame] = 0;
      pthread_mutex_unlock(&(settings->valid_mutex));

      /* Broadcast & wake up the waiting processes */
      pthread_cond_broadcast(&(settings->buffer_done[settings->currently_processed_frame]));

      /* Now update the internal variables */
      settings->currently_processed_frame = (settings->currently_processed_frame + 1) % settings->br.count;
      settings->show_top = (settings->show_top) ? 0 : 1;
   }

   lavplay_msg(LAVPLAY_MSG_DEBUG, info,
      "Playback thread: was told to exit");
   pthread_exit(NULL);
   return NULL;
}


/******************************************************
 * lavplay_mjpeg_open()
 *   hardware: opens the device and allocates buffers
 *   software: inits threads and allocates buffers
 *
 * return value: 1 on success, 0 on error
 ******************************************************/

static int lavplay_mjpeg_open(lavplay_t *info)
{
   int i;
   video_playback_setup *settings = (video_playback_setup *)info->settings;
   EditList *editlist = info->editlist;
   int max_frame_size = editlist->max_frame_size;

   lavplay_msg(LAVPLAY_MSG_DEBUG, info,
      "Initializing the %s", info->playback_mode=='S'?"threading system":"video device");

   switch (info->playback_mode)
   {
      case 'H':
      case 'C':
#ifndef IRIX
         /* open video device */
         if ((settings->video_fd = open(info->video_dev, O_RDWR)) < 0)
         {
            lavplay_msg(LAVPLAY_MSG_ERROR, info,
               "Error opening %s: %s", info->video_dev, (char *)sys_errlist[errno]);
            return 0;
         }

         /* Request buffers */
         settings->br.count = info->MJPG_numbufs;
         if (max_frame_size < 140*1024) max_frame_size = 140*1024;
         settings->br.size  = (max_frame_size + 4095)&~4095;
         if (ioctl(settings->video_fd, MJPIOC_REQBUFS, &(settings->br)) < 0)
         {
            lavplay_msg(LAVPLAY_MSG_ERROR, info,
               "Error requesting buffers: %s", (char *)sys_errlist[errno]);
            return 0;
         }

         /* Map the buffers */
         settings->buff = mmap(0, settings->br.count * settings->br.size,
            PROT_READ | PROT_WRITE, MAP_SHARED, settings->video_fd, 0);
         if (settings->buff == MAP_FAILED)
         {
            lavplay_msg(LAVPLAY_MSG_ERROR, info,
               "Error mapping the video buffer: %s", (char *)sys_errlist[errno]);
            return 0;
         }
#else
	 fprintf(stderr, "IRIX doesn't support hardware MJPEG playback!\n");
	 return 0;
#endif
         break;


      case 'S':
         /* Just allocate MJPG_nbuf buffers */
         settings->br.count = info->MJPG_numbufs;
         settings->br.size  = (max_frame_size*2 + 4095)&~4095;
         settings->buff = (char *)malloc(settings->br.count * settings->br.size);
         if (!settings->buff)
         {
            lavplay_msg (LAVPLAY_MSG_ERROR, info,
               "Malloc error, you\'re probably out of memory");
            return 0;
         }

         pthread_mutex_init(&(settings->valid_mutex), NULL);
         pthread_mutex_init(&(settings->syncinfo_mutex), NULL);
       
         /* Invalidate all buffers, and initialize the conditions */
         for (i=0;i<MJPEG_MAX_BUF;i++)
	 {
            settings->valid[i] = 0;
            pthread_cond_init(&(settings->buffer_filled[i]), NULL);
            pthread_cond_init(&(settings->buffer_done[i]), NULL);
            memset(&(settings->syncinfo[i]), 0, sizeof(struct mjpeg_sync));
         }

         /* Now do the thread magic */
         settings->currently_processed_frame = 0;
         settings->show_top = 1; /* start with top frames as default, change with mjpeg_set_params */
       
         if (pthread_create(&(settings->software_playback_thread), NULL, 
            lavplay_mjpeg_playback_thread, (void *)info))
         {
            lavplay_msg(LAVPLAY_MSG_ERROR, info,
               "Could not create software playback thread");
            return 0;
         }

         break;

      default:
         lavplay_msg(LAVPLAY_MSG_ERROR, info,
            "Unknown playback_method!");
         return 0;
   }

   settings->usec_per_frame = 0;

   return 1;
}


/******************************************************
 * lavplay_mjpeg_get_params()
 *   get default parameters
 *
 * return value: 1 on success, 0 on error
 ******************************************************/

static int lavplay_mjpeg_get_params(lavplay_t *info, struct mjpeg_params *bp)
{
   int i;
   video_playback_setup *settings = (video_playback_setup *)info->settings;
   //EditList *editlist = info->editlist;

   if (info->playback_mode == 'H' || info->playback_mode == 'C')
   {
#ifndef IRIX 
      /* do a MJPIOC_G_PARAMS ioctl to get proper default values */
      if (ioctl(settings->video_fd, MJPIOC_G_PARAMS, bp) < 0)
      {
         lavplay_msg(LAVPLAY_MSG_ERROR, info,
            "Error getting video parameters: %s", (char *)sys_errlist[errno]);
         return 0;
      }
#else
	 fprintf(stderr, "IRIX doesn't support hardware MJPEG playback!\n");
	 return 0;
#endif
   }
   else
   {
      /* Set some necessary params */
      bp->decimation = 1;
      bp->quality = 50; /* default compression factor 8 */
      bp->odd_even = 1;
      bp->APPn = 0;
      bp->APP_len = 0; /* No APPn marker */
      for(i=0;i<60;i++) bp->APP_data[i] = 0;
      bp->COM_len = 0; /* No COM marker */
      for(i=0;i<60;i++) bp->COM_data[i] = 0;
      bp->VFIFO_FB = 1;
      memset(bp->reserved,0,sizeof(bp->reserved));
   }

   return 1;
}


/******************************************************
 * lavplay_mjpeg_set_params()
 *   set the parameters
 *
 * return value: 1 on success, 0 on error
 ******************************************************/

static int lavplay_mjpeg_set_params(lavplay_t *info, struct mjpeg_params *bp)
{
   video_playback_setup *settings = (video_playback_setup *)info->settings;
   //EditList *editlist = info->editlist;
  
   if (info->playback_mode == 'H') /* only when doing on-screen (hardware-decoded) output */
    {
#ifndef IRIX
      struct video_window vw;
      XWindowAttributes wts;
      Display *dpy;
      int n;

      if (NULL == (dpy = XOpenDisplay(strchr(info->display, ':'))))
      {
         lavplay_msg(LAVPLAY_MSG_ERROR, info, "Can't open X11 display %s\n", info->display);
         return 0;
      }
      XGetWindowAttributes(dpy, DefaultRootWindow(dpy), &wts);
      vw.x = info->soft_full_screen ? 0: (wts.width-704)/2;
      vw.y = info->soft_full_screen ? 0: (wts.height-((bp->norm == 0) ? 576 : 480))/2;
      vw.width = info->soft_full_screen ? 1024 : 704;
      vw.height = info->soft_full_screen ? 768 : (bp->norm == 0) ? 576 : 480;
      vw.clips = NULL;
      vw.clipcount = 0;
      vw.chromakey = -1;
      
      if (ioctl(settings->video_fd, VIDIOCSWIN, &vw) < 0)
      {
         lavplay_msg(LAVPLAY_MSG_ERROR, info,
            "Could not set on-screen window parameters (check framebuffer-params with v4l-conf): %s",
            (char *)sys_errlist[errno]);
         return 0;
      }
      n = 1;
      if (ioctl(settings->video_fd, VIDIOCCAPTURE, &n) < 0)
      {
         lavplay_msg(LAVPLAY_MSG_ERROR, info,
            "Could not activate on-screen window: %s", (char *)sys_errlist[errno]);
         return 0;
      }

      bp->VFIFO_FB = 1;
#else
	 fprintf(stderr, "IRIX doesn't support hardware MJPEG playback!\n");
	 return 0;
#endif
   }

   if (info->playback_mode == 'H' || info->playback_mode == 'C')
   {
#ifndef IRIX 
      /* All should be set up now, set the parameters */
      lavplay_msg(LAVPLAY_MSG_DEBUG, info,
         "Hardware video settings: input=%d, norm=%d, fields_per_buf=%d, "
         "x=%d, y=%d, width=%d, height=%d, quality=%d",
         bp->input, bp->norm, bp->field_per_buff, bp->img_x, bp->img_y,
         bp->img_width, bp->img_height, bp->quality);
      if (ioctl(settings->video_fd, MJPIOC_S_PARAMS, bp) < 0)
      {
         lavplay_msg(LAVPLAY_MSG_ERROR, info,
            "Error setting video parameters: %s", (char *)sys_errlist[errno]);
         return 0;
      }
#else
	 fprintf(stderr, "IRIX doesn't support hardware MJPEG playback!\n");
	 return 0;
#endif
   }
   else /* software */
   {
      settings->show_top = (bp->odd_even) ? 1 : 0; /* odd_even = 1: show top field first */
   }

   return 1;
}


/******************************************************
 * lavplay_mjpeg_set_frame_rate()
 *   set the frame rate
 *
 * return value: 1 on success, 0 on error
 ******************************************************/

static int lavplay_mjpeg_set_playback_rate(lavplay_t *info, double video_fps, int norm)
{
   int norm_usec_per_frame = 0;
   int target_usec_per_frame;
   video_playback_setup *settings = (video_playback_setup *)info->settings;
   //EditList *editlist = info->editlist;

   switch (norm)
   {
      case VIDEO_MODE_PAL:
      case VIDEO_MODE_SECAM:
         norm_usec_per_frame = 1000000/25; /* 25Hz */
         break;
      case VIDEO_MODE_NTSC:
         norm_usec_per_frame = 1001000/30;  /* 30ish Hz */
         break;
      default:
         if (info->playback_mode != 'S')
         {
            lavplay_msg(LAVPLAY_MSG_ERROR, info,
               "Hardware playback impossible: unknown video norm!");
            return 0;
         }
   }

   if( video_fps != 0.0 )
      target_usec_per_frame = (int)(1000000.0 / video_fps);
   else
      target_usec_per_frame = norm_usec_per_frame;

   if (info->playback_mode != 'S' &&
      abs(target_usec_per_frame - norm_usec_per_frame) > 50)
   {
      lavplay_msg(LAVPLAY_MSG_ERROR, info,
         "Specified frame-rate doesn't match in mode in hardware playback");
      return 0;
   }

   settings->usec_per_frame = target_usec_per_frame;

   return 1;
}


/******************************************************
 * lavplay_mjpeg_queue_buf()
 *   queue a buffer
 *
 * return value: 1 on success, 0 on error
 ******************************************************/

static int lavplay_mjpeg_queue_buf(lavplay_t *info, int frame, int frame_periods)
{
   video_playback_setup *settings = (video_playback_setup *)info->settings;
   //EditList *editlist = info->editlist;

   if (info->playback_mode == 'H' || info->playback_mode == 'C')
   {
#ifndef IRIX 
      if (ioctl(settings->video_fd, MJPIOC_QBUF_PLAY, &frame) < 0)
      {
         lavplay_msg(LAVPLAY_MSG_ERROR, info,
            "Error queueing buffer: %s", (char *)sys_errlist[errno]);
         return 0;
      }
#else
	 fprintf(stderr, "IRIX doesn't support hardware MJPEG playback!\n");
	 return 0;
#endif
   }
   else
   {
      /* mark this buffer as playable and tell the software playback thread to wake up if it sleeps */
      pthread_mutex_lock(&(settings->valid_mutex));
      settings->valid[frame] = frame_periods;
      pthread_cond_broadcast(&(settings->buffer_filled[frame]));
      pthread_mutex_unlock(&(settings->valid_mutex));
   }

   return 1;
}


/******************************************************
 * lavplay_mjpeg_sync_buf()
 *   sync on a buffer
 *
 * return value: 1 on success, 0 on error
 ******************************************************/

static int lavplay_mjpeg_sync_buf(lavplay_t *info, struct mjpeg_sync *bs)
{
   video_playback_setup *settings = (video_playback_setup *)info->settings;
   //EditList *editlist = info->editlist;

   if (info->playback_mode == 'H' || info->playback_mode == 'C')
   {
#ifndef IRIX 
      if (ioctl(settings->video_fd, MJPIOC_SYNC, bs) < 0)
      {
         lavplay_msg(LAVPLAY_MSG_ERROR, info,
            "Error syncing on a buffer: %s", (char *)sys_errlist[errno]);
         return 0;
      }
      lavplay_msg(LAVPLAY_MSG_DEBUG, info,
         "frame=%ld, length=%ld, seq=%ld", bs->frame, bs->length, bs->seq);
#else
	 fprintf(stderr, "IRIX doesn't support hardware MJPEG playback!\n");
	 return 0;
#endif
   }
   else
   {
      /* Wait until this buffer has been played */
      pthread_mutex_lock(&(settings->valid_mutex));
      while (settings->valid[settings->currently_synced_frame] != 0)
      {
         pthread_cond_wait(&(settings->buffer_done[settings->currently_synced_frame]),
            &(settings->valid_mutex));
      }
      pthread_mutex_unlock(&(settings->valid_mutex));

      /* copy the relevant sync information */
      memcpy(bs, &(settings->syncinfo[settings->currently_synced_frame]),
         sizeof(struct mjpeg_sync));
      settings->currently_synced_frame = (settings->currently_synced_frame + 1) % settings->br.count;
   }

   return 1;
}


/******************************************************
 * lavplay_mjpeg_close()
 *   close down
 *
 * return value: 1 on success, 0 on error
 ******************************************************/

static int lavplay_mjpeg_close(lavplay_t *info)
{
   int n;
   video_playback_setup *settings = (video_playback_setup *)info->settings;
   //EditList *editlist = info->editlist;

   lavplay_msg(LAVPLAY_MSG_DEBUG, info,
      "Closing down the %s", info->playback_mode=='S'?"threading system":"video device");

   if (info->playback_mode == 'H' || info->playback_mode == 'C')
   {
#ifndef IRIX
      n = -1;
      if (ioctl(settings->video_fd, MJPIOC_QBUF_PLAY, &n) < 0)
      {
         lavplay_msg(LAVPLAY_MSG_ERROR, info,
            "Error de-queueing the buffers: %s", (char *)sys_errlist[errno]);
         return 0;
      }
      if (info->playback_mode == 'H')
      {
         n = 0;
         if (ioctl(settings->video_fd, VIDIOCCAPTURE, &n) < 0)
         {
            lavplay_msg(LAVPLAY_MSG_ERROR, info,
               "Could not deactivate on-screen window: %s", sys_errlist[errno]);
            return 0;
         }
      }
#else
	 fprintf(stderr, "IRIX doesn't support hardware MJPEG playback!\n");
	 return 0;
#endif
   }
   else
   {
      pthread_cancel(settings->software_playback_thread);
      if (pthread_join(settings->software_playback_thread, NULL)) 
      {
         lavplay_msg(LAVPLAY_MSG_ERROR, info,
            "Failure deleting software playback thread");
         return 0;
      }
   }

   return 1;
}


/******************************************************
 * lavplay_init()
 *   check the given settings and initialize everything
 *
 * return value: 1 on success, 0 on error
 ******************************************************/

static int lavplay_init(lavplay_t *info)
{
   long nqueue;
   struct mjpeg_params bp;

#ifndef IRIX 
   struct video_capability vc;
#endif 

   video_playback_setup *settings = (video_playback_setup *)info->settings;
   EditList *editlist = info->editlist;
   int hn;

   if (editlist->video_frames == 0 && !info->get_video_frame)
   {
      lavplay_msg(LAVPLAY_MSG_ERROR, info,
         "No video source!");
      return 0;
   }
   if (editlist->video_frames == 0 && info->editlist->has_audio &&
      info->audio && !info->get_audio_sample)
   {
      lavplay_msg(LAVPLAY_MSG_ERROR, info,
         "Audio turned on but no audio source!");
      return 0;
   }
   if (editlist->video_frames > 0 && (info->get_video_frame ||
      info->get_audio_sample))
   {
      lavplay_msg(LAVPLAY_MSG_ERROR, info,
         "Manual input as well as file input!");
      return 0;
   }

   /* Set min/max options so that it runs like it should */
   settings->min_frame_num = 0;
   settings->max_frame_num = editlist->video_frames - 1;
   settings->current_frame_num = settings->min_frame_num; /* start with frame 0 */

   /* Seconds per video frame: */
   settings->spvf = 1.0 / editlist->video_fps;
   lavplay_msg(LAVPLAY_MSG_DEBUG, info,
      "1.0/SPVF = %4.4f", 1.0 / settings->spvf );

   /* Seconds per audio sample: */
   if(editlist->has_audio && info->audio)
      settings->spas = 1.0 / editlist->audio_rate;
   else
      settings->spas = 0.;

   if (info->flicker_reduction)
   {
      /* allocate auxiliary video buffer for flicker reduction */
      settings->tmpbuff[0] = (char *)malloc(editlist->max_frame_size);
      settings->tmpbuff[1] = (char *)malloc(editlist->max_frame_size);
      if (!settings->tmpbuff[0] || !settings->tmpbuff[1])
      {
         lavplay_msg (LAVPLAY_MSG_ERROR, info,
            "Malloc error, you\'re probably out of memory");
         return 0;
      }
   }

   /* initialize the playback threading system (used to be libmjpeg) */
   lavplay_mjpeg_open(info);

   /* init SDL if we want SDL */
   if (info->playback_mode == 'S')
   {
      if (!info->sdl_width) info->sdl_width = editlist->video_width;
      if (!info->sdl_height) info->sdl_height = editlist->video_height;
      if (!lavplay_SDL_init(info))
         return 0;
   }

   if (editlist->has_audio && info->audio)
   {
      if (audio_init(0, 0, (editlist->audio_chans>1), editlist->audio_bits,
         editlist->audio_rate)) /* increase this last argument to test sync */
      {
         lavplay_msg(LAVPLAY_MSG_ERROR, info,
            "Error initializing Audio: %s",audio_strerror());
         return 0;
      }
      settings->audio_buffer_size = audio_get_buffer_size();
   }

   /* After we have fired up the audio and video threads system (which
    * are assisted if we're installed setuid root, we want to set the
    * effective user id to the real user id
    */
   if(seteuid(getuid()) < 0)
   {
      lavplay_msg(LAVPLAY_MSG_ERROR, info,
         "Can't set effective user-id: %s", (char *)sys_errlist[errno]);
      return 0;
   }

   /* Fill all buffers first */
   for(nqueue=0;nqueue<settings->br.count;nqueue++)
   {
      if (!lavplay_queue_next_frame(info, settings->buff+nqueue* settings->br.size,
#if defined(SUPPORT_READ_DV2) || defined(SUPPORT_READ_YUV420)
				    (settings->data_format[nqueue] =
				     el_video_frame_data_format(settings->current_frame_num, editlist)),
#endif
				    0,0,0)) break;
   }

   /* Choose the correct parameters for playback */
   if (!lavplay_mjpeg_get_params(info, &bp))
      return 0;

   /* set options */
   bp.input = 0;
   bp.norm = (editlist->video_norm == 'n') ? VIDEO_MODE_NTSC : VIDEO_MODE_PAL;
   lavplay_msg(LAVPLAY_MSG_INFO, info,
      "Output norm: %s", bp.norm==VIDEO_MODE_NTSC?"NTSC":"PAL");
   hn = bp.norm==VIDEO_MODE_NTSC?480:576; /* Height of norm */

   if (info->playback_mode != 'S')
   {
#ifndef IRIX
      /* set correct width of device for hardware
       * DC10(+): 768 (PAL/SECAM) or 640 (NTSC), Buz/LML33: 720
       */
      if (ioctl(settings->video_fd, VIDIOCGCAP, &vc) < 0)
      {
         lavplay_msg(LAVPLAY_MSG_ERROR, info,
            "Error getting device capabilities: %s", (char *)sys_errlist[errno]);
         return 0;
      }
      /* vc.maxwidth is often reported wrong - let's just keep it broken (sigh) */
      if (vc.maxwidth != 768 && vc.maxwidth != 640) vc.maxwidth = 720;

      bp.decimation = 0; /* we will set proper params ourselves later on */

      if (editlist->video_width > vc.maxwidth || editlist->video_height > hn )
      {
         /* the video is too big */
         lavplay_msg(LAVPLAY_MSG_ERROR, info,
            "Video dimensions too large: %ld x %ld, device max = %dx%d",
            editlist->video_width, editlist->video_height, vc.maxwidth, hn);
         return 0;
      }

      /* if zoom_to_fit is set, HorDcm is independent of interlacing */
      if (info->zoom_to_fit)
      {
         if (editlist->video_width <= vc.maxwidth/4 )
            bp.HorDcm = 4;
         else if (editlist->video_width <= vc.maxwidth/2)
            bp.HorDcm = 2;
         else
            bp.HorDcm = 1;
      }

      if (editlist->video_inter)
      {
         /* Interlaced video, 2 fields per buffer */
         bp.field_per_buff = 2;
         bp.TmpDcm = 1;

         if (info->zoom_to_fit)
         {
            if (editlist->video_height <= hn/2)
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

         if (editlist->video_height > hn/2 ||
            (!info->zoom_to_fit && editlist->video_width > vc.maxwidth/2))
         {
            lavplay_msg(LAVPLAY_MSG_ERROR, info,
               "Video dimensions (not interlaced) too large: %ld x %ld",
               editlist->video_width, editlist->video_height);
            if (editlist->video_width > vc.maxwidth/2) 
               lavplay_msg(LAVPLAY_MSG_ERROR, info,
               "Try using the \'zoom-to-fit\'-option");
            return 0;
         }

         if(info->zoom_to_fit)
         {
            if (editlist->video_height <= hn/4 )
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
      bp.img_width  = bp.HorDcm * editlist->video_width;
      bp.img_height = bp.VerDcm * editlist->video_height/bp.field_per_buff;

      if (info->horizontal_offset == VALUE_NOT_FILLED)
         bp.img_x = (vc.maxwidth - bp.img_width)/2;
      else
         bp.img_x = info->horizontal_offset;

      if (info->vertical_offset == VALUE_NOT_FILLED)
         bp.img_y = (hn/2 - bp.img_height)/2;
      else
         bp.img_y = info->vertical_offset/2;

      lavplay_msg(LAVPLAY_MSG_INFO, info,
         "Output dimensions: %dx%d+%d+%d",
         bp.img_width, bp.img_height*2, bp.img_x, bp.img_y*2);
      lavplay_msg(LAVPLAY_MSG_INFO, info,
         "Output zoom factors: %d (hor) %d (ver)",
         bp.HorDcm,bp.VerDcm*bp.TmpDcm);

#else
      fprintf(stderr, "IRIX doesn't support hardware MJPEG playback!\n");
      return 0;
#endif
   }
   else
   {
      /* software playback */
      lavplay_msg(LAVPLAY_MSG_INFO, info,
         "Output dimensions: %ldx%ld",
         editlist->video_width, editlist->video_height);
   }
   
   /* Set field polarity for interlaced video */
   bp.odd_even = (editlist->video_inter==LAV_INTER_TOP_FIRST);
   if (info->exchange_fields) bp.odd_even = !bp.odd_even;

   /* Set these settings */
   if (!lavplay_mjpeg_set_params(info, &bp))
      return 0;

   if (!lavplay_mjpeg_set_playback_rate(info, editlist->video_fps, bp.norm))
      return 0;

   return 1;
}


/******************************************************
 * lavplay_playback_cycle()
 *   the playback cycle
 ******************************************************/

static void lavplay_playback_cycle(lavplay_t *info)
{
   video_playback_stats stats;
   video_playback_setup *settings = (video_playback_setup *)info->settings;
   EditList *editlist = info->editlist;
   struct mjpeg_sync bs;
   struct timeval audio_tmstmp;
   struct timeval time_now;
   double tdiff1, tdiff2;
   int n;
   int first_free, frame, skipv, skipa, skipi, nvcorr;
   long frame_number[256]; /* Must be at least as big as the number of buffers used */

   stats.stats_changed = 0;
   stats.num_corrs_a = 0;
   stats.num_corrs_b = 0;
   stats.nqueue = 0;
   stats.nsync = 0;
   stats.audio = 0;
   stats.norm = editlist->video_norm=='n'?1:0;

   tdiff1 = 0.;
   tdiff2 = 0.;
   nvcorr = 0;

   /* Queue the buffers read, this triggers video playback */
   if (editlist->has_audio && info->audio)
   {
      audio_start();
      stats.audio = 1;
   }
   for(n=0;n<settings->br.count;n++) /* TODO: maybe br.count-1? */
   {
      frame_number[n] = settings->current_frame_num;
      lavplay_mjpeg_queue_buf(info, n, 1);
   }
   stats.nqueue = settings->br.count;

   while (settings->state != LAVPLAY_STATE_STOP)
   {

      /* Sync to get a free buffer. We want to have all free buffers,
       * which have been played so far. Normally there should be a function
       * in the kernel API to get the number of all free buffers.
       * I don't want to change this API at the moment, therefore
       * we look on the clock to see if there are more buffers ready
       */
      first_free = stats.nsync;

      do
      {
         if (settings->state == LAVPLAY_STATE_STOP)
            goto FINISH;

         if (!lavplay_mjpeg_sync_buf(info, &bs))
         {
            lavplay_change_state(info, LAVPLAY_STATE_STOP);
            goto FINISH;
         }
         frame = bs.frame;

         /* Since we queue the frames in order, we have to get them back in order */
         if (frame != stats.nsync % settings->br.count)
         {
            lavplay_msg(LAVPLAY_MSG_ERROR, info,
               "**INTERNAL ERROR: Bad frame order on sync: frame = %d, nsync = %d, br.count = %ld",
               frame, stats.nsync, settings->br.count);
            lavplay_change_state(info, LAVPLAY_STATE_STOP);
            goto FINISH;
         }
         stats.nsync++;

         /* Look on clock */
         gettimeofday(&time_now, 0);
         stats.tdiff = time_now.tv_sec - bs.timestamp.tv_sec +
            (time_now.tv_usec - bs.timestamp.tv_usec)*1.e-6;
      }
      while (stats.tdiff > settings->spvf && (stats.nsync - first_free) < settings->br.count - 1);

      if ((stats.nsync - first_free) > settings->br.count - 3)
         lavplay_msg(LAVPLAY_MSG_WARNING, info,
            "Disk too slow, can not keep pace!");

      if (editlist->has_audio && info->audio)
      {
         audio_get_output_status(&audio_tmstmp, &(stats.num_asamps), &(stats.num_aerr));
         if (audio_tmstmp.tv_sec)
         {
            tdiff1 = settings->spvf * (stats.nsync - nvcorr) -
               settings->spas * settings->audio_buffer_size / editlist->audio_bps * stats.num_asamps;
            tdiff2 = (bs.timestamp.tv_sec - audio_tmstmp.tv_sec) +
               (bs.timestamp.tv_usec - audio_tmstmp.tv_usec) * 1.e-6;
         }
      }
      stats.tdiff = tdiff1 - tdiff2;

      /* Fill and queue free buffers again */
      for (n=first_free;n<stats.nsync;)
      {
         /* Audio/Video sync correction */
         skipv = 0;
         skipa = 0;
         skipi = 0;

         if (info->sync_correction)
         {
            if (stats.tdiff > settings->spvf)
            {
               /* Video is ahead audio */
               skipa = 1;
               if (info->sync_ins_frames) skipi = 1;
               nvcorr++;
               stats.num_corrs_a++;
               stats.tdiff -= settings->spvf;
               stats.stats_changed = 1;
            }
            if (stats.tdiff < -settings->spvf)
            {
               /* Video is behind audio */
               skipv = 1;
               if (!info->sync_skip_frames) skipi = 1;
               nvcorr--;
               stats.num_corrs_b++;
               stats.tdiff += settings->spvf;
               stats.stats_changed = 1;
            }
         }

         /* Read one frame, break if EOF is reached */
         frame = n % settings->br.count;
         frame_number[frame] = settings->current_frame_num;
         if (!lavplay_queue_next_frame(info, settings->buff+frame*settings->br.size,
#if defined(SUPPORT_READ_DV2) || defined(SUPPORT_READ_YUV420)
				       (settings->data_format[frame] =
					el_video_frame_data_format(settings->current_frame_num, editlist)),
#endif
				       skipv,skipa,skipi))
         {
            lavplay_change_state(info, LAVPLAY_STATE_STOP);
            goto FINISH;
         }
         if (skipv) continue; /* no frame has been read */

         /* Queue the frame */
         if (!lavplay_mjpeg_queue_buf(info, frame, 1))
         {
            lavplay_change_state(info, LAVPLAY_STATE_STOP);
            goto FINISH;
         }
         stats.nqueue++;
         n++;
      }

      /* output statistics */
      if (editlist->has_audio && info->audio) stats.audio = settings->audio_mute?0:1;
      stats.play_speed = settings->current_playback_speed;
      stats.frame = settings->current_frame_num;
      if (info->output_statistics)
         info->output_statistics(&stats);
      stats.stats_changed = 0;
   }

FINISH:

   /* All buffers are queued, sync on the outstanding buffers
    * Never try to sync on the last buffer, it is a hostage of
    * the codec since it is played over and over again
    */
   /* NOTE: this causes lockup for some reason - so just leave it */
   //while (stats.nqueue > stats.nsync + 1)
   //{
   //   lavplay_mjpeg_sync_buf(info, &bs);
   //   stats.nsync++;
   //}
   /* and we also don't need audio anymore */
   if (editlist->has_audio && info->audio) 
      audio_shutdown();
}


/******************************************************
 * lavplay_playback_thread()
 *   The main playback thread
 ******************************************************/

static void *lavplay_playback_thread(void *data)
{
   lavplay_t *info = (lavplay_t *)data;
   video_playback_setup *settings = (video_playback_setup *)info->settings;
   //EditList *editlist = info->editlist;

   lavplay_playback_cycle(info);

   if (info->flicker_reduction)
   {
      free(settings->tmpbuff[0]);
      free(settings->tmpbuff[1]);
   }
   lavplay_mjpeg_close(info);

   if (info->playback_mode == 'S')
   {
      SDL_FreeYUVOverlay(settings->yuv_overlay);
      SDL_Quit();
   }

   pthread_exit(NULL);
}


/******************************************************
 * lavplay_malloc()
 *   malloc the pointer and set default options
 *
 * return value: a pointer to an allocated lavplay_t
 ******************************************************/

lavplay_t *lavplay_malloc()
{
   lavplay_t *info;
   video_playback_setup *settings;

   info = (lavplay_t *)malloc(sizeof(lavplay_t));
   if (!info)
   {
      lavplay_msg (LAVPLAY_MSG_ERROR, NULL,
         "Malloc error, you\'re probably out of memory");
      return NULL;
   }
   info->playback_mode = 'S';
   info->horizontal_offset = VALUE_NOT_FILLED;
   info->vertical_offset = VALUE_NOT_FILLED;
   info->exchange_fields = 0;
   info->zoom_to_fit = 0;
   info->flicker_reduction = 1;
   info->sdl_width = 0; /* use video size */
   info->sdl_height = 0; /* use video size */
   info->soft_full_screen = 0;
   info->video_dev = "/dev/video";
   info->display = ":0.0";

   info->audio = 1;
   info->audio_dev = "/dev/dsp";

   info->continuous = 0;
   info->sync_correction = 1;
   info->sync_ins_frames = 1;
   info->sync_skip_frames = 1;
   info->MJPG_numbufs = 8;

   info->output_statistics = NULL;
   info->msg_callback = NULL;
   info->state_changed = NULL;
   info->get_video_frame = NULL;
   info->get_audio_sample = NULL;

   settings = (video_playback_setup *)malloc(sizeof(video_playback_setup));
   info->settings = (void *)settings;
   if (!(info->settings))
   {
      lavplay_msg (LAVPLAY_MSG_ERROR, NULL,
         "Malloc error, you\'re probably out of memory");
      return NULL;
   }

   info->editlist = (EditList *)malloc(sizeof(EditList));
   if (!(info->editlist))
   {
      lavplay_msg (LAVPLAY_MSG_ERROR, NULL,
         "Malloc error, you\'re probably out of memory");
      return NULL;
   }

   info->editlist->video_frames = 0;

   settings->current_playback_speed = 0;
   settings->currently_synced_frame = 0;
   settings->current_frame_num = 0;
   settings->old_field_len = 0;
   settings->old_buff_no = 0;
   settings->first_frame = 1;
   settings->buff = NULL;
   settings->save_list = NULL;
   settings->save_list_len = 0;

   return info;
}


/******************************************************
 * lavplay_main()
 *   the whole video-playback cycle
 *
 * Basic setup:
 *   * this function initializes the devices,
 *       sets up the whole thing and then forks
 *       the main task and returns control to the
 *       main app. It can then start playing by
 *       setting playback speed and such. Stop
 *       by calling lavplay_stop()
 *
 * return value: 1 on succes, 0 on error
 ******************************************************/

int lavplay_main(lavplay_t *info)
{
   video_playback_setup *settings = (video_playback_setup *)info->settings;

   /* Flush the Linux File buffers to disk */
   sync();

   lavplay_change_state(info, LAVPLAY_STATE_PAUSED);

   /* start with initing */
   if (!lavplay_init(info))
      return 0;

   /* fork ourselves to return control to the main app */
   if( pthread_create( &(settings->playback_thread), NULL,
      lavplay_playback_thread, (void*)info) )
   {
      lavplay_msg(LAVPLAY_MSG_ERROR, info,
         "Failed to create thread");
      return 0;
   }

   return 1;
}


/******************************************************
 * lavplay_stop()
 *   stop playing
 *
 * return value: 1 on succes, 0 on error
 ******************************************************/

int lavplay_stop(lavplay_t *info)
{
   video_playback_setup *settings = (video_playback_setup *)info->settings;
   //EditList *editlist = info->editlist;

   if (settings->state == LAVPLAY_STATE_STOP)
   {
      lavplay_msg(LAVPLAY_MSG_WARNING, info,
         "We weren't even initialized!");
      return 0;
   }

   lavplay_change_state(info, LAVPLAY_STATE_STOP);

   //pthread_cancel( settings->playback_thread );
   pthread_join( settings->playback_thread, NULL );

   return 1;
}


/******************************************************
 * lavplay_free()
 *   free() the struct
 *
 * return value: 1 on succes, 0 on error
 ******************************************************/

int lavplay_free(lavplay_t *info)
{
   video_playback_setup *settings = (video_playback_setup *)info->settings;

   if (settings->state != LAVPLAY_STATE_STOP)
   {
      lavplay_msg(LAVPLAY_MSG_WARNING, info,
         "We're not stopped yet, use lavplay_stop() first!");
      return 0;
   }

   free(info->editlist);
   free(settings);
   free(info);
   return 1;
}


/*** Methods for simple video editing (cut/paste) ***/

/******************************************************
 * lavplay_edit_copy()
 *   copy a number of frames into a buffer
 *
 * return value: 1 on succes, 0 on error
 ******************************************************/

int lavplay_edit_copy(lavplay_t *info, long start, long end)
{
   video_playback_setup *settings = (video_playback_setup *)info->settings;
   EditList *editlist = info->editlist;
   int k, i;

   if (settings->save_list) free(settings->save_list);
   settings->save_list = (long *)malloc((end - start + 1) * sizeof(long));
   if (!settings->save_list)
   {
      lavplay_msg (LAVPLAY_MSG_ERROR, info,
         "Malloc error, you\'re probably out of memory");
      lavplay_change_state(info, LAVPLAY_STATE_STOP);
      return 0;
   }
   k = 0;
   for (i=start;i<=end;i++)
      settings->save_list[k++] = editlist->frame_list[i];
   settings->save_list_len = k;

   lavplay_msg(LAVPLAY_MSG_DEBUG, info,
      "Copied frames %ld-%ld into buffer", start, end);

   return 1;
}


/******************************************************
 * lavplay_edit_delete()
 *   delete a number of frames from the current movie
 *
 * return value: 1 on succes, 0 on error
 ******************************************************/

int lavplay_edit_delete(lavplay_t *info, long start, long end)
{
   video_playback_setup *settings = (video_playback_setup *)info->settings;
   EditList *editlist = info->editlist;
   int i;

   if (end < start || start > editlist->video_frames || end >= editlist->video_frames ||
      end < 0 || start < 0)
   {
      lavplay_msg(LAVPLAY_MSG_WARNING, info,
         "Incorrect parameters for deleting frames");
      return 0;
   }

   for(i=end+1;i<editlist->video_frames;i++)
      editlist->frame_list[i-(end-start+1)] = editlist->frame_list[i];

   /* Update min and max frame_num's to reflect the removed section */
   if (start - 1 < settings->min_frame_num)
   {
      if (end < settings->min_frame_num)
         settings->min_frame_num -= (end - start + 1);
      else
         settings->min_frame_num = start;
   }
   if (start - 1 < settings->max_frame_num)
   {
      if (end <= settings->max_frame_num)
         settings->max_frame_num -= (end - start + 1);
      else
         settings->max_frame_num = start - 1;
   }
   if (start <= settings->current_frame_num) {
      if (settings->current_frame_num <= end) {
         settings->current_frame_num = start;
      } else {
         settings->current_frame_num -= (end - start + 1);
      }
   }

   editlist->video_frames -= (end - start + 1);

   lavplay_msg(LAVPLAY_MSG_DEBUG, info,
      "Deleted frames %ld-%ld", start, end);

   return 1;
}


/******************************************************
 * lavplay_edit_cut()
 *   cut a number of frames into a buffer
 *
 * return value: 1 on succes, 0 on error
 ******************************************************/

int lavplay_edit_cut(lavplay_t *info, long start, long end)
{
   if (!lavplay_edit_copy(info, start, end))
      return 0;
   if (!lavplay_edit_delete(info, start, end))
      return 0;

   return 1;
}


/******************************************************
 * lavplay_edit_paste()
 *   paste frames from the buffer into a certain position
 *
 * return value: 1 on succes, 0 on error
 ******************************************************/

int lavplay_edit_paste(lavplay_t *info, long destination)
{
   video_playback_setup *settings = (video_playback_setup *)info->settings;
   EditList *editlist = info->editlist;
   int i, k;

   /* there should be something in the buffer */
   if (!settings->save_list_len || !settings->save_list)
   {
      lavplay_msg(LAVPLAY_MSG_WARNING, info,
         "No frames in the buffer to paste");
      return 0;
   }

   if (destination < 0 || destination >= editlist->video_frames)
   {
      lavplay_msg(LAVPLAY_MSG_WARNING, info,
         "Incorrect parameters for pasting frames");
      return 0;
   }

   editlist->frame_list = realloc(editlist->frame_list,
      (editlist->video_frames+settings->save_list_len)*sizeof(long));
   if (!editlist->frame_list)
   {
      lavplay_msg (LAVPLAY_MSG_ERROR, info,
         "Malloc error, you\'re probably out of memory");
      lavplay_change_state(info, LAVPLAY_STATE_STOP);
      return 0;
   }

   k = settings->save_list_len;
   for (i=editlist->video_frames-1;i>=destination;i--)
      editlist->frame_list[i+k] = editlist->frame_list[i];
   k = destination;
   for (i=0;i<settings->save_list_len;i++)
   {
      if (k<=settings->min_frame_num)
         settings->min_frame_num++;
      if (k<settings->max_frame_num)
         settings->max_frame_num++;

      editlist->frame_list[k++] = settings->save_list[i];
   }
   editlist->video_frames += settings->save_list_len;

   i = lavplay_increase_frame(info, 0);
   if (!info->continuous) return i;

   lavplay_msg(LAVPLAY_MSG_DEBUG, info,
      "Pasted %ld frames from buffer into position %ld in movie",
      settings->save_list_len, destination);

   return 1;
}


/******************************************************
 * lavplay_edit_move()
 *   move a number of frames to a different position
 *
 * return value: 1 on succes, 0 on error
 ******************************************************/

int lavplay_edit_move(lavplay_t *info, long start, long end, long destination)
{
   //video_playback_setup *settings = (video_playback_setup *)info->settings;
   EditList *editlist = info->editlist;
   long dest_real;

   if (destination >= editlist->video_frames || destination < 0 || start < 0 ||
      end < 0 || start >= editlist->video_frames || end >= editlist->video_frames ||
      end < start)
   {
      lavplay_msg(LAVPLAY_MSG_WARNING, info,
         "Incorrect parameters for moving frames");
      return 0;
   }

   if (destination < start)
      dest_real = destination;
   else if (destination > end)
      dest_real = destination - (end - start + 1);
   else
      dest_real = start;

   if (!lavplay_edit_cut(info, start, end))
      return 0;
   if (!lavplay_edit_paste(info, dest_real))
      return 0;

   return 1;
}


/******************************************************
 * lavplay_edit_addmovie()
 *   add a number of frames from a new movie to a
 *     certain position in the current movie
 *
 * return value: 1 on succes, 0 on error
 ******************************************************/

int lavplay_edit_addmovie(lavplay_t *info, char *movie, long start, long end, long destination)
{
   video_playback_setup *settings = (video_playback_setup *)info->settings;
   EditList *editlist = info->editlist;
   int n, i;

   n = open_video_file(movie, editlist);

   if (start < 0)
   {
      start = 0;
      end = editlist->num_frames[n] - 1;
   }

   if (end < 0 || start < 0 || start > end || start > editlist->num_frames[n] ||
      end >= editlist->num_frames[n] || destination < 0 ||
      destination >= editlist->video_frames)
   {
      lavplay_msg(LAVPLAY_MSG_WARNING, info,
         "Wrong parameters for adding a new movie");
      return 0;
   }

   editlist->frame_list = (long *)realloc(editlist->frame_list,
      (editlist->video_frames + (end - start + 1)) * sizeof(long));
   if (!editlist->frame_list)
   {
      lavplay_msg (LAVPLAY_MSG_ERROR, info,
         "Malloc error, you\'re probably out of memory");
      lavplay_change_state(info, LAVPLAY_STATE_STOP);
      return 0;
   }

   if (destination <= settings->max_frame_num)
      settings->max_frame_num += (end - start + 1);
   if (destination < settings->min_frame_num)
      settings->min_frame_num += (end - start + 1);

   for (i=start;i<=end;i++)
   {
      editlist->frame_list[editlist->video_frames++] = editlist->frame_list[destination+i-start];
      editlist->frame_list[destination+i-start] = EL_ENTRY(n, i);
   }

   lavplay_msg(LAVPLAY_MSG_DEBUG, info,
      "Added frames %ld-%ld from %s into position %ld in movie",
      start, end, movie, destination);

   return 1;
}


/******************************************************
 * lavplay_edit_set_playable()
 *   set the part of the movie that will actually
 *     be played, start<0 means whole movie
 *
 * return value: 1 on succes, 0 on error
 ******************************************************/

int lavplay_edit_set_playable(lavplay_t *info, long start, long end)
{
   video_playback_setup *settings = (video_playback_setup *)info->settings;
   EditList *editlist = info->editlist;
   int need_change_frame = 0;

   if (start < 0)
   {
      start = 0;
      end = editlist->video_frames - 1;
   }

   if (end < start || end >= editlist->video_frames || start >= editlist->video_frames)
   {
      lavplay_msg(LAVPLAY_MSG_WARNING, info,
         "Incorrect frame play range!");
      return 0;
   }

   if (settings->current_frame_num < start || settings->current_frame_num > end)
      need_change_frame = 1;

   settings->min_frame_num = start;
   settings->max_frame_num = end;

   if (need_change_frame)
   {
      int res;
      res = lavplay_increase_frame(info, 0);
      if (!info->continuous) return res;
   }

   return 1;
}


/*** Control sound during video playback */

/******************************************************
 * lavplay_toggle_audio()
 *   mutes or unmutes audio (1 = on, 0 = off)
 *
 * return value: 1 on succes, 0 on error
 ******************************************************/

int lavplay_toggle_audio(lavplay_t *info, int audio)
{
   video_playback_setup *settings = (video_playback_setup *)info->settings;
   EditList *editlist = info->editlist;

   if (!(info->audio && editlist->has_audio))
   {
      lavplay_msg(LAVPLAY_MSG_WARNING, info,
         "Audio playback has not been enabled");
      return 0;
   }

   settings->audio_mute = audio==0?1:0;

   lavplay_msg(LAVPLAY_MSG_DEBUG, info,
      "Audio playback was %s", audio==0?"muted":"unmuted");

   return 1;
}


/*** Methods for saving the currently played movie to editlists or open new movies */

/******************************************************
 * lavplay_save_selection()
 *   save a certain range of frames to an editlist
 *
 * return value: 1 on succes, 0 on error
 ******************************************************/

int lavplay_save_selection(lavplay_t *info, char *filename, long start, long end)
{
   //video_playback_setup *settings = (video_playback_setup *)info->settings;
   EditList *editlist = info->editlist;

   if (write_edit_list(filename, start, end, editlist))
      return 0;

   lavplay_msg(LAVPLAY_MSG_DEBUG, info,
      "Saved frames %ld-%ld to editlist %s", start, end, filename);

   return 1;
}


/******************************************************
 * lavplay_save_all()
 *   save the whole current movie to an editlist
 *
 * return value: 1 on succes, 0 on error
 ******************************************************/

int lavplay_save_all(lavplay_t *info, char *filename)
{
   //video_playback_setup *settings = (video_playback_setup *)info->settings;
   EditList *editlist = info->editlist;

   if (write_edit_list(filename, 0, editlist->video_frames - 1, editlist))
      return 0;

   lavplay_msg(LAVPLAY_MSG_DEBUG, info,
      "Saved all frames to editlist %s", filename);

   return 1;
}


/******************************************************
 * lavplay_open()
 *   open a new (series of) movie
 *
 * return value: 1 on succes, 0 on error
 ******************************************************/

int lavplay_open(lavplay_t *info, char **files, int num_files)
{
   video_playback_setup *settings = (video_playback_setup *)info->settings;
   EditList *editlist = info->editlist;
   EditList *new_eli = NULL; /* the new to-be-opened-editlist */
   int i;

   if (num_files <= 0)
   {
      lavplay_msg(LAVPLAY_MSG_WARNING, info,
         "That's not a valid number of files");
      return 0;
   }

   new_eli = (EditList *)malloc(sizeof(EditList));
   if (!new_eli)
   {
      lavplay_msg (LAVPLAY_MSG_ERROR, NULL,
         "Malloc error, you\'re probably out of memory");
      lavplay_change_state(info, LAVPLAY_STATE_STOP);
      return 0;
   }

   /* open the new movie(s) */
   read_video_files(files, num_files, new_eli);

   if (settings->state == LAVPLAY_STATE_STOP)
   {
      /* we're not running yet, yay! */
      info->editlist = new_eli;
      free (editlist);
   }
   else if (editlist->video_width == new_eli->video_width &&
      editlist->video_height == new_eli->video_height &&
      editlist->video_inter == new_eli->video_inter &&
      abs(editlist->video_fps - new_eli->video_fps) < 0.0000001 &&
      editlist->has_audio == new_eli->has_audio &&
      editlist->audio_rate == new_eli->audio_rate &&
      editlist->audio_chans == new_eli->audio_chans &&
      editlist->audio_bits == new_eli->audio_bits)
   {
      /* the movie-properties are the same - just open it */
      info->editlist = new_eli;
      free(editlist);
      editlist = new_eli;
      settings->min_frame_num = 0;
      settings->max_frame_num = editlist->video_frames - 1;
   }
   else
   {
      /* okay, the properties are different, we could re-init but for now, let's just bail out */
      /* TODO: a better thing */
      lavplay_msg(LAVPLAY_MSG_WARNING, info,
         "Editlists are different");
      free(new_eli);
      return 0;
   }

   /* if everything went well et all */
   i = lavplay_increase_frame(info, 0);
   if (!info->continuous) return i;

   return 1;
}


/******************************************************
 * lavplay_busy()
 *   Wait until playback is finished
 ******************************************************/

void lavplay_busy(lavplay_t *info)
{
   pthread_join( ((video_playback_setup*)(info->settings))->playback_thread, NULL );
}
