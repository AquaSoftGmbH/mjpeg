/*
 *  blend.flt - reads two frame-interlaced YUV4MPEG streams from stdin
 *              and writes out a transistion from one to the other
 *
 *  Copyright (C) 2000, pHilipp Zabel <pzabel@gmx.de>
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

#include "y4m12.h"

static void usage () {

   fprintf (stderr, "usage:  blend.flt [params]\n"
                    "params: -o [num] - opacity of second input at the beginning [0-255]\n"
                    "        -O [num] - opacity of second input at the end [0-255]\n"
                    "        -l [num] - duration of transistion in frames (REQUIRED!)\n"
                    "        -v [num] - verbosity [0..2]\n"
                    "        -h       - print out this usage information\n");

}

static void blend (unsigned char *src0[3], unsigned char *src1[3],
                   unsigned int opacity1,
                   unsigned int width,     unsigned int height,
                   unsigned char *dst[3])
{
   register unsigned int i, op0, op1;
   register unsigned int len = width * height;
   op1 = (opacity1 > 255) ? 255 : opacity1;
   op0 = 255 - op1;

   for (i=0; i<len; i++)
      dst[0][i] = (op0 * src0[0][i] + op1 * src1[0][i]) / 255;
      
   len>>=2; /* len = len / 4 */

   for (i=0; i<len; i++) {
      dst[1][i] = (op0 * src0[1][i] + op1 * src1[1][i]) / 255;
      dst[2][i] = (op0 * src0[2][i] + op1 * src1[2][i]) / 255;
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
   int i, opacity, frame;
   unsigned int param_opacity0   = 0;     /* opacity of input1 at the beginning */
   unsigned int param_opacity1   = 255;   /* opacity of input1 at the end */
   unsigned int param_duration   = 0;     /* duration of transistion effect */

   while ((i = getopt(argc, argv, "v:o:O:l:h")) != -1) {
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
      case 'o':
         param_opacity0 = atoi (optarg);
         if (param_opacity0 < 0)
            param_opacity0 = 0;
         if (param_opacity0 > 255)
            param_opacity0 = 255;
         break;
      case 'O':
         param_opacity1 = atoi (optarg);
         if (param_opacity1 < 0)
            param_opacity1 = 0;
         if (param_opacity1 > 255)
            param_opacity1 = 255;
         break;
      case 'l':
         param_duration = atoi (optarg);
         break;
      }
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

      opacity = (1 - frame/((double)param_duration-1)) * param_opacity0
                  + (frame/((double)param_duration-1)) * param_opacity1;
      blend(yuv0, yuv1, opacity,
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
