/*
 *  wipe.flt - reads two frame-interlaced YUV4MPEG streams from stdin
 *             and writes out a transistion from one to the other
 *
 *  Copyright (C) 2001, Ronald Bultje
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

/* meaning of numbers:
 * LEFT_TO_RIGHT = 1
 * RIGHT_TO_LEFT = 2
 * TOP_TO_BOTTOM = 3
 * BOTTOM_TO_TOP = 4
 */

#include "y4m12.h"

static void usage () {

   fprintf (stderr, "usage:  wipe.flt [params]\n"
                    "params: -l [num] - duration of transistion in frames (REQUIRED!)\n"
                    "        -n [num] - number of wipe columns/rows (default: 1)\n"
                    "        -d [num] - wiping direction [1..4] (REQUIRED!)\n"
                    "            1: left-to-right    2: right-to-left\n"
                    "            3: top-to-bottom    4: bottom-to-top\n"
                    "        -r [num] - disable/enable reposition\n"
                    "            0: disable          1: only scene 1\n"
                    "            2: only scene 2     3: both\n"
                    "        -v [num] - verbosity [0..2]\n"
                    "        -h       - print out this usage information\n");

}

static void wipe (unsigned char *src0[3], unsigned char *src1[3],
                   unsigned int direction, unsigned int num_columns,
                   int reposition, double progress_phase,
                   unsigned int width,     unsigned int height,
                   unsigned char *dst[3])
{
   register unsigned int width_per_column;
   register unsigned int len = width * height;
   register unsigned int i,a1,a2;

   if (direction  >= 3) {
      width_per_column = height / num_columns;
      a1 = (int)(width_per_column * progress_phase) * width;
      a2 = (int)(width_per_column * (1-progress_phase)) * width;
   } else {
      width_per_column = width / num_columns;
      a1 = width_per_column * progress_phase;
      a2 = width_per_column * (1-progress_phase);
   }
   if (direction % 2 == 0) progress_phase = 1 - progress_phase;

   for (i=0;i<len;i++) {
      if ((direction>2?i/width:i%width) % width_per_column < width_per_column * progress_phase)
         dst[0][i] = direction%2==1?src1[0][reposition>=2?i+a2:i]:src0[0][reposition%2==1?i+a1:i];
      else
         dst[0][i] = direction%2==1?src0[0][reposition%2==1?i-a1:i]:src1[0][reposition>=2?i-a2:i];
   }

   len/=4;
   width/=2;
   height/=2;
   width_per_column/=2;
   if(direction<=2) {
      a1/=2; a2/=2;
   } else {
      a1 = (int)(width_per_column * (1-progress_phase)) * width;
      a2 = (int)(width_per_column * progress_phase) * width;
   }

   for (i=0;i<len;i++) {
      if ((direction>2?i/width:i%width) % width_per_column < width_per_column * progress_phase) {
         dst[1][i] = direction%2==1?src1[1][reposition>=2?i+a2:i]:src0[1][reposition%2==1?i+a1:i];
         dst[2][i] = direction%2==1?src1[2][reposition>=2?i+a2:i]:src0[2][reposition%2==1?i+a1:i];
      } else {
         dst[1][i] = direction%2==1?src0[1][reposition%2==1?i-a1:i]:src1[1][reposition>=2?i-a2:i];
         dst[2][i] = direction%2==1?src0[2][reposition%2==1?i-a1:i]:src1[2][reposition>=2?i-a2:i];
      }
   }
}

int main (int argc, char *argv[])
{
   int verbose = 1;
   int in_fd  = 0;         /* stdin */
   int out_fd = 1;         /* stdout */
   unsigned char *yuv0[3]; /* input 0 */
   unsigned char *yuv1[3]; /* input 1 */
   unsigned char *yuv[3];  /* output */
   y4m12_t *y4m12;
   int i, frame;
   unsigned int param_duration       = 0;     /* duration of transistion effect */
   unsigned int param_num_columns    = 1;     /* number of columns */
   unsigned int param_wipe_direction = 0;     /* direction of the wipe */
   int param_reposition              = 0;     /* reposition */

   while ((i = getopt(argc, argv, "v:l:n:d:r:h")) != -1) {
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
      case 'l':
         param_duration = atoi (optarg);
         break;
      case 'n':
         param_num_columns = atoi(optarg);
         if (param_num_columns <=0) {
            fprintf(stderr, "Error: param_num_columns <= 0\n");
            usage();
            exit(1);
         }
         break;
      case 'd':
         param_wipe_direction = atoi(optarg);
         break;
      case 'r':
         param_reposition = atoi(optarg);
         break;
      }
   }

   if (param_wipe_direction > 4 || param_wipe_direction <= 0) {
      fprintf(stderr, "Error: invalid wipe direction\n");
      usage();
      exit(1);
   }
   if (param_duration <= 0) {
      fprintf(stderr, "Error: duration <= 0\n");
      usage();
      exit(1);
   }

   y4m12 = y4m12_malloc();
   i = y4m12_read_header (y4m12, in_fd);

   yuv[0] = (char *)malloc (y4m12->width * y4m12->height);
   yuv0[0] = (char *)malloc (y4m12->width * y4m12->height);
   yuv1[0] = (char *)malloc (y4m12->width * y4m12->height);

   yuv[1] = (char *)malloc (y4m12->width * y4m12->height/4);
   yuv0[1] = (char *)malloc (y4m12->width * y4m12->height/4);
   yuv1[1] = (char *)malloc (y4m12->width * y4m12->height/4);

   yuv[2] = (char *)malloc (y4m12->width * y4m12->height/4);
   yuv0[2] = (char *)malloc (y4m12->width * y4m12->height/4);
   yuv1[2] = (char *)malloc (y4m12->width * y4m12->height/4);

   y4m12_write_header (y4m12, out_fd);

   for (frame=0;frame<param_duration;frame++)
   {
      y4m12->buffer[0] = yuv0[0];
      y4m12->buffer[1] = yuv0[1];
      y4m12->buffer[2] = yuv0[2];
      i = y4m12_read_frame(y4m12, in_fd);
      if (i<0) exit (1);

      y4m12->buffer[0] = yuv1[0];
      y4m12->buffer[1] = yuv1[1];
      y4m12->buffer[2] = yuv1[2];
      i = y4m12_read_frame(y4m12, in_fd);
      if (i<0) exit (1);

      wipe (yuv0, yuv1, param_wipe_direction, param_num_columns,
         param_reposition, frame/(double)param_duration,
         y4m12->width, y4m12->height, yuv);

      y4m12->buffer[0] = yuv[0];
      y4m12->buffer[1] = yuv[1];
      y4m12->buffer[2] = yuv[2];
      y4m12_write_frame (y4m12, out_fd);
      if (i<0) exit(1);
   }

   y4m12_free(y4m12);

   return 0;
}
