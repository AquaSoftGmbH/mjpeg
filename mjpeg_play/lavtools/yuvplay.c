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

char *print_status(int frame, double framerate) {
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
   double time_between_frames;
   struct timeval time_now;
   int n, width, height;
   int frame;
   int frame_rate_code;
   double framerate;
   unsigned char *yuv[3];
   int fd_in = 0;
   int screenwidth=0, screenheight=0;

   while ((n = getopt(argc, argv, "hs:")) != EOF) {
      switch (n) {
         case 's':
            if (sscanf(optarg, "%dx%d", &screenwidth, &screenheight) != 2) {
               fprintf(stdout, "-s option needs two arguments: -s 10x10\n");
               fflush(stdout);
               exit(1);
            }
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

   if (yuv_read_header(fd_in, &width, &height, &frame_rate_code)) {
      mjpeg_log (LOG_ERROR, "Couldn't read YUV4MPEG header!\n");
      exit (1);
   }

   if (screenwidth <= 0) screenwidth = width;
   if (screenheight <= 0) screenheight = height;

   switch (frame_rate_code) {
      case 1:
         time_between_frames = 1000000 * 1.0 / 23.976;
         framerate = 23.976;
         break;
      case 2:
         time_between_frames = 1000000 * 1.0 / 24.0;
         framerate = 24.0;
         break;
      case 3:
         time_between_frames = 1000000 * 1.0 / 25.0;
         framerate = 25.0;
         break;
      case 4:
         time_between_frames = 1000000 * 1.0 / 29.97;
         framerate = 29.97;
         break;
     default:
         mjpeg_log(LOG_ERROR, "Unknown format!!\n");
         exit(1);
   }

   /* Initialize the SDL library */
   if( SDL_Init(SDL_INIT_VIDEO) < 0 ) {
      mjpeg_log(LOG_ERROR, "Couldn't initialize SDL: %s\n", SDL_GetError());
      exit(1);
   }

   /* yuv params */
   yuv[0] = malloc(width * height * sizeof(unsigned char));
   yuv[1] = malloc(width * height * sizeof(unsigned char) / 4);
   yuv[2] = malloc(width * height * sizeof(unsigned char) / 4);

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

   yuv_overlay = SDL_CreateYUVOverlay(width, height, SDL_IYUV_OVERLAY, screen);
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

   gettimeofday(&time_now,0);

   while ((yuv_read_frame(fd_in, yuv, width, height)) && (!got_sigint)) {

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
      SDL_UpdateRect(screen, 0, 0, width, height);

      //mjpeg_log(LOG_INFO, "Playing frame %4.4d - %s\r", frame, 
         //print_status(frame, framerate));
      fprintf(stdout, "Playing frame %4.4d - %s\r", frame,
         print_status(frame, framerate));
      fflush(stdout);

      while(get_time_diff(time_now) < time_between_frames) {
         usleep(1000);
      }
      frame++;
      gettimeofday(&time_now,0);
   }

   for (n=0; n<3; n++) {
      free(yuv[n]);
   }

   mjpeg_log(LOG_INFO, "Played %4.4d frames (%s)\n", frame, print_status(frame, framerate));

   SDL_FreeYUVOverlay(yuv_overlay);
   SDL_Quit();

   return 0;
}
