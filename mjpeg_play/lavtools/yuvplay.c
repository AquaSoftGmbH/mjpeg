/*
 *  yuvplay - play YUV data using SDL
 *
 *  Copyright (C) 2000, Ronald Bultje <rbultje@ronald.bitfreak.net>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include "yuv4mpeg.h"
#include "mjpeg_logging.h"
#include <SDL/SDL.h>
#include <sys/time.h>

/* SDL variables */
SDL_Surface *screen;
SDL_Overlay *yuv_overlay;
SDL_Rect rect;

static int got_sigint = 0;

static void usage () {
   fprintf (stdout, "Usage: lavpipe/lav2yuv... | yuvplay [options]\n"
                    "  -s : size (width x height)\n"
			        "  -f : frame rate (overrides stream rate)\n"
      );
}

static void sigint_handler (int signal) {
   mjpeg_log (LOG_WARN, "Caugth SIGINT, exiting...\n");
   got_sigint = 1;
}

static long get_time_diff(struct timeval time_now) {
   struct timeval time_now2;
   gettimeofday(&time_now2,0);
   return time_now2.tv_sec*1.e6 - time_now.tv_sec*1.e6 + time_now2.tv_usec - time_now.tv_usec;
}

static char *print_status(int frame, double framerate) {
   int m, h, s, f;
   static char temp[256];

   m = frame / (60 * framerate);
   h = m / 60;
   m = m % 60;
   s = (int)(frame/framerate) % 60;
   f = frame % (int)framerate;
   sprintf(temp, "%d:%2.2d:%2.2d.%2.2d", h, m, s, f);
   return temp;
}

int main(int argc, char *argv[])
{
   double time_between_frames = 0.0;
   double frame_rate = 0.0;
   struct timeval time_now;
   int n, frame;
   unsigned char *yuv[3];
   int in_fd = 0;
   int screenwidth=0, screenheight=0;
   y4m_stream_info_t streaminfo;
   y4m_frame_info_t frameinfo;
   int frame_width;
   int frame_height;

   while ((n = getopt(argc, argv, "hs:f:")) != EOF) {
      switch (n) {
         case 's':
            if (sscanf(optarg, "%dx%d", &screenwidth, &screenheight) != 2) {
               mjpeg_error_exit1( "-s option needs two arguments: -s 10x10\n");
               exit(1);
            }
            break;

	  case 'f':
		  frame_rate = atof(optarg);
		  if( frame_rate <= 0.0 || frame_rate > 200.0 )
			  mjpeg_error_exit1( "-f option needs argument > 0.0 and < 200.0\n");
		  break;
	  case 'h':
	  case '?':
            usage();
            exit(1);
            break;
         default:
            usage();
            exit(1);
      }
   }

   y4m_init_stream_info(&streaminfo);
   y4m_init_frame_info(&frameinfo);
   if ((n = y4m_read_stream_header(in_fd, &streaminfo)) != Y4M_OK) {
      mjpeg_log (LOG_ERROR, "Couldn't read YUV4MPEG header: %s!\n",
         y4m_strerr(n));
      exit (1);
   }
   frame_width = y4m_si_get_width(&streaminfo);
   frame_height = y4m_si_get_height(&streaminfo);

   if ((screenwidth <= 0) || (screenheight <= 0)) {
     /* no user supplied screen size, so let's use the stream info */
     y4m_ratio_t aspect = y4m_si_get_aspectratio(&streaminfo);
       
     if (!(Y4M_RATIO_EQL(aspect, y4m_aspect_UNKNOWN))) {
       /* if aspect ratio supplied, use it */
       if ((frame_width * aspect.d) < (frame_height * aspect.n)) {
	 screenwidth = frame_width;
	 screenheight = frame_width * aspect.d / aspect.n;
       } else {
	 screenheight = frame_height;
	 screenwidth = frame_height * aspect.n / aspect.d;
       }
     } else {
       /* unknown aspect ratio -- assume square pixels */
       screenwidth = frame_width;
       screenheight = frame_height;
     }
   }

   /* Initialize the SDL library */
   if( SDL_Init(SDL_INIT_VIDEO) < 0 ) {
      mjpeg_log(LOG_ERROR, "Couldn't initialize SDL: %s\n", SDL_GetError());
      exit(1);
   }

   /* yuv params */
   yuv[0] = malloc(frame_width * frame_height * sizeof(unsigned char));
   yuv[1] = malloc(frame_width * frame_height / 4 * sizeof(unsigned char));
   yuv[2] = malloc(frame_width * frame_height / 4 * sizeof(unsigned char));

   screen = SDL_SetVideoMode(screenwidth, screenheight, 0, SDL_SWSURFACE);
   if ( screen == NULL ) {
      mjpeg_log(LOG_ERROR, "SDL: Couldn't set %dx%d: %s\n", screenwidth,
         screenheight, SDL_GetError());
      exit(1);
   }
   else {
      mjpeg_log(LOG_DEBUG, "SDL: Set %dx%d @ %d bpp\n", screenwidth,
         screenheight, screen->format->BitsPerPixel);
   }

   yuv_overlay = SDL_CreateYUVOverlay(frame_width, frame_height,
				      SDL_IYUV_OVERLAY, screen);
   if ( yuv_overlay == NULL ) {
      mjpeg_log(LOG_ERROR, "SDL: Couldn't create SDL_yuv_overlay: %s\n", SDL_GetError());
      exit(1);
   }

   rect.x = 0;
   rect.y = 0;
   rect.w = screenwidth;
   rect.h = screenheight;

   SDL_DisplayYUVOverlay(yuv_overlay, &rect);

   signal (SIGINT, sigint_handler);

   frame = 0;
   if ( frame_rate == 0.0 ) {
     /* frame rate has not been set from command-line... */
     if (Y4M_RATIO_EQL(y4m_fps_UNKNOWN, y4m_si_get_framerate(&streaminfo))) {
       mjpeg_info( "Frame-rate undefined in stream... assuming 25Hz!\n" );
       frame_rate = 25.0;
     } else {
       frame_rate = Y4M_RATIO_DBL(y4m_si_get_framerate(&streaminfo));
     }
   }
   time_between_frames = 1.e6 / frame_rate;

   gettimeofday(&time_now,0);

   while ((n = y4m_read_frame(in_fd, &streaminfo, &frameinfo, yuv)) == Y4M_OK && (!got_sigint)) {

      /* Lock SDL_yuv_overlay */
      if ( SDL_MUSTLOCK(screen) ) {
         if ( SDL_LockSurface(screen) < 0 ) break;
      }
      if (SDL_LockYUVOverlay(yuv_overlay) < 0) break;

      /* let's draw the data (*yuv[3]) on a SDL screen (*screen) */
      yuv_overlay->pixels = yuv;

      /* Unlock SDL_yuv_overlay */
      if ( SDL_MUSTLOCK(screen) ) {
         SDL_UnlockSurface(screen);
      }
      SDL_UnlockYUVOverlay(yuv_overlay);

      /* Show, baby, show! */
      SDL_DisplayYUVOverlay(yuv_overlay, &rect);
      SDL_UpdateRect(screen, 0, 0, frame_width, frame_height);

      fprintf(stdout, "Playing frame %4.4d - %s\r", frame,
			  print_status(frame, frame_rate));
      fflush(stdout);

      while(get_time_diff(time_now) < time_between_frames) {
         usleep(1000);
      }
      frame++;
      gettimeofday(&time_now,0);
   }

   if (n != Y4M_OK)
      mjpeg_error("Couldn't read frame: %s\n",
         y4m_strerr(n));

   for (n=0; n<3; n++) {
      free(yuv[n]);
   }

   mjpeg_log(LOG_INFO, "Played %4.4d frames (%s)\n",
	     frame, print_status(frame, frame_rate));

   SDL_FreeYUVOverlay(yuv_overlay);
   SDL_Quit();

   y4m_fini_frame_info(&frameinfo);
   y4m_fini_stream_info(&streaminfo);
   return 0;
}

