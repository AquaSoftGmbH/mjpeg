/*
 *  matteblend - reads three frame-interlaced YUV4MPEG streams from stdin
 *               and blends the second over the first using the third's
 *               luminance channel as a matte
 *
 *  Copyright (C) 2001, pHilipp Zabel <pzabel@gmx.de>
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

#include "yuv4mpeg.h"

static void usage () {

   fprintf (stderr, "usage:  matteblend.flt\n"
                    "no params at the moment - color saturation falloff or such has to be implemented\n");

}

static void blend (unsigned char *src0[3], unsigned char *src1[3], unsigned char *matte[3],
                   unsigned int width,     unsigned int height,
                   unsigned char *dst[3])
{
   register unsigned int i,j;
   register unsigned int len = width * height;

   for (i=0; i<len; i+=4) {
      dst[0][i]   = ((255 - matte[0][i])   * src0[0][i]   + matte[0][i]   * src1[0][i])   / 255;
      dst[0][i+1] = ((255 - matte[0][i+1]) * src0[0][i+1] + matte[0][i+1] * src1[0][i+1]) / 255;
      dst[0][i+2] = ((255 - matte[0][i+2]) * src0[0][i+2] + matte[0][i+2] * src1[0][i+2]) / 255;
      dst[0][i+3] = ((255 - matte[0][i+3]) * src0[0][i+3] + matte[0][i+3] * src1[0][i+3]) / 255;            
   }

   len>>=2; /* len = len / 4 */

   /* do we really have to "downscale" matte here? */
   for (i=0,j=0; i<len; i++, j+=2) {
      int m = (matte[0][j] + matte[0][j+1] + matte[0][j+width] + matte[0][j+width+1]) >> 2;
      if ((j % width) == (width - 2)) j += width;
      dst[1][i] = ((255-m) * src0[1][i] + m * src1[1][i]) / 255;
      dst[2][i] = ((255-m) * src0[2][i] + m * src1[2][i]) / 255;
   }
}

int main (int argc, char *argv[])
{
   int in_fd  = 0;         /* stdin */
   int out_fd = 1;         /* stdout */
   unsigned char *yuv0[3]; /* input 0 */
   unsigned char *yuv1[3]; /* input 1 */
   unsigned char *yuv2[3]; /* input 2 */
   unsigned char *yuv[3];  /* output */
   int w, h, rate;
   int i;

   if (argc > 1) {
      usage ();
      exit (1);
   }

   i = yuv_read_header (in_fd, &w, &h, &rate);
   if (i != 0) {
      fprintf (stderr, "%s: input stream error\n", argv[0]);
      exit (1);
   }

   yuv[0] = (char *)malloc (w*h);   (char *)yuv0[0] = malloc (w*h);   (char *)yuv1[0] = malloc (w*h);   (char *)yuv2[0] = malloc (w*h);
   yuv[1] = (char *)malloc (w*h/4); (char *)yuv0[1] = malloc (w*h/4); (char *)yuv1[1] = malloc (w*h/4); (char *)yuv2[1] = malloc (w*h/4);
   yuv[2] = (char *)malloc (w*h/4); (char *)yuv0[2] = malloc (w*h/4); (char *)yuv1[2] = malloc (w*h/4); (char *)yuv2[2] = malloc (w*h/4);

   yuv_write_header (out_fd, w, h, rate);

   while (1) {
      i = yuv_read_frame(in_fd, yuv0, w, h);
      if (i<=0)
         exit (0);
      i = yuv_read_frame(in_fd, yuv1, w, h);
      if (i<=0)
         exit (1);
      i = yuv_read_frame(in_fd, yuv2, w, h);
      if (i<=0)
         exit (1);

      blend (yuv0, yuv1, yuv2, w, h, yuv);

      yuv_write_frame (out_fd, yuv, w, h);

   }

}