/*
 *  fade.flt - reads a stream from stdin and fades in/out from/to
 *             black/white. Writes to stadout
 *
 *  Copyright (C) 2004, Michael Hanke
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#include "y4m12.h"

#define ubw 128
#define vbw 128
#define yw  235
#define yb   16
#define in_fd 0
#define out_fd 1

#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))

static void usage () {

   fprintf (stderr, "usage:  fade.flt [params]\n"
                    "params: -s [num] - frame for starting fade in/out [0]\n"
                    "        -d [num] - duration of transition in frames (REQUIRED)\n"
                    "        -t [num] - type of transition\n"
	            "             1   - fade out to black\n"
	            "             2   - fade in from black\n"
	            "             3   - fade out to white\n"
 	            "             4   - fade in from white\n"
                    "        -v [num] - verbosity [0..2]\n"
                    "        -h       - print out this usage information\n");

}

int main (int argc, char *argv[])
{
   int verbose = 1;        /* verbosity */
   int type = 1;            /* type of transition */
   int start = 0;           /* start frame */
   int duration = 0;        /* duration of fading */
   unsigned char *yuv[3];   /* input */
   unsigned char *yuvbw[3]; /* black/white frame */
   y4m12_t *y4m12;
   int i, len, len4, ybw, inout, frame;
   float logbase, facc, mx;

   while ((i = getopt(argc, argv, "v:s:d:t:h")) != -1) {
      switch (i) {
      case 'h':
         usage();
         exit(1);
         break;
      case 'v':
         verbose = atoi (optarg);
         if( verbose < 0 || verbose >2 ) {
            usage ();
            exit (1);
         }
         break;		  
      case 's':
         start = atoi (optarg);
         if (start < 0) {
	     fprintf(stderr, "Error: start < 0\n");
	     exit(1);
	 }
         break;
      case 'd':
         duration = atoi (optarg);
         if (duration <= 0) {
	     fprintf(stderr, "Error: duration <= 0\n");
	     exit(1);
	 }
         break;
      case 't':
         type = atoi (optarg);
         if (type < 0 || type > 4) {
	     fprintf(stderr, "Error: type out of range\n");
	     exit(1);
	 }
         break;
      }
   }

   if (duration <= 0) {
      fprintf(stderr, "Error: duration <= 0\n");
      usage();
      exit(1);
   }

   mx = 256.0;
   logbase = -log(mx)/(duration+1.0);
   if (type < 3) ybw = yb;
   else ybw = yw;
   inout = type & 1;

   y4m12 = y4m12_malloc();
   i = y4m12_read_header (y4m12, in_fd);
   len = y4m12->width * y4m12->height;
   len4 = len >> 2;
   
   yuv[0] = (char *) malloc(len);
   yuvbw[0] = (char *) malloc(len);

   yuv[1] = (char *) malloc(len4);
   yuvbw[1] = (char *) malloc(len4);

   yuv[2] = (char *) malloc(len4);
   yuvbw[2] = (char *) malloc(len4);

   for (i = 0; i< len; i++) yuvbw[0][i] = ybw;
   for (i = 0; i< len4; i++) {
       yuvbw[1][i] = ubw;
       yuvbw[2][i] = vbw;
   }

   y4m12_write_header (y4m12, out_fd);

   for (frame = 0; frame < start; frame++) {
      y4m12->buffer[0] = yuv[0];
      y4m12->buffer[1] = yuv[1];
      y4m12->buffer[2] = yuv[2];
      i = y4m12_read_frame(y4m12, in_fd);
      if (i < 0) {
	  fprintf(stderr, "Error: Unexpected end of input stream\n");
	  exit(1);
      }
      if (inout == 0) {
	  y4m12->buffer[0] = yuvbw[0];
	  y4m12->buffer[1] = yuvbw[1];
	  y4m12->buffer[2] = yuvbw[2];
      }
      i = y4m12_write_frame (y4m12, out_fd);
      if (i < 0) {
	  fprintf(stderr, "Error: Write error\n");
	  exit(1);
      }
   }

   y4m12->buffer[0] = yuv[0];
   y4m12->buffer[1] = yuv[1];
   y4m12->buffer[2] = yuv[2];
   for (frame = 1; frame <= duration; frame++) {
      i = y4m12_read_frame(y4m12, in_fd);
      if (i < 0) return 0;
      facc = exp(logbase*frame);
      if (inout == 0) facc = MIN(1.0/(facc*mx),1.0);

      for (i = 0; i < len; i++)
	  yuv[0][i] = (1-facc)*ybw+facc*yuv[0][i];
      for (i = 0; i < len4; i++) {
	  yuv[1][i] = (1-facc)*ubw+facc*yuv[1][i];
	  yuv[2][i] = (1-facc)*vbw+facc*yuv[2][i];
      }

      i = y4m12_write_frame (y4m12, out_fd);
      if (i < 0) {
	  fprintf(stderr, "Error: Write error\n");
	  exit(1);
      }
   }

   for (;;) {
      y4m12->buffer[0] = yuv[0];
      y4m12->buffer[1] = yuv[1];
      y4m12->buffer[2] = yuv[2];
      i = y4m12_read_frame(y4m12, in_fd);
      if (i < 0) return 0;
      if (inout) {
	  y4m12->buffer[0] = yuvbw[0];
	  y4m12->buffer[1] = yuvbw[1];
	  y4m12->buffer[2] = yuvbw[2];
      }
      i = y4m12_write_frame (y4m12, out_fd);
      if (i < 0) {
	  fprintf(stderr, "Error: Write error\n");
	  exit(1);
      }
   }

   return 0;
}
