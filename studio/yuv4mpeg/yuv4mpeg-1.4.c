/*
 *  yuv4mpeg.c: reading to and writing from YUV4MPEG streams
 *              possibly part of lavtools in near future ;-)
 *
 *  Copyright (C) 2001 pHilipp Zabel  <pzabel@gmx.de>
 *
 *  based on code from mpeg2enc and lav2yuv
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
#include <unistd.h>
#include <string.h>
#include <math.h>

#include "yuv4mpeg.h"

const int num_mpeg2_framerates = 9;
static double mpeg2_framerates[] = { 00.000, 23.976,
   24.000,                      /* 24fps movie */
   25.000,                      /* PAL */
   29.970,                      /* NTSC */
   30.000, 50.000,
   59.940, 60.000
};

int yuv_fps2mpegcode (double fps)
{
   int i;

   for (i = 0; i < num_mpeg2_framerates; i++)
      if (fabs (mpeg2_framerates[i] - fps) / mpeg2_framerates[i] < 0.001)
         return i;
   return 0;
}

double yuv_mpegcode2fps (unsigned int code)
{
   if (code < num_mpeg2_framerates)
      return mpeg2_framerates[code];
   return 0.0;
}

size_t piperead (int fd, char *buf, size_t len)
{
   size_t n = 0;
   size_t r = 0;

   while (r < len) {
      n = read (fd, buf + r, len - r);
      if (n <= 0)
         return r;
      r += n;
   }
   return r;
}

/*
  Raw write does *not* guarantee to write the entire buffer load if it
  becomes possible to do so.  This does...
 */

size_t pipewrite (int fd, char *buf, size_t len)
{
   size_t n = 0;
   size_t r = 0;

   while (r < len) {
      n = write (fd, buf + r, len - r);
      if (n < 0)
         return n;
      r += n;
   }
   return r;
}


int 
yuv_read_frame (int fd, unsigned char *yuv[3], int width, int height)
{

   int v, h, i;
   unsigned char magic[7];

   if (piperead (fd, magic, 6) != 6)
      return 0;
   if (strncmp (magic, "FRAME\n", 6)) {
      magic[6] = '\0';
      printf("Error (yuv4mpeg.c): \nStart of frame is not \"FRAME\\n\"\n");
      return 0;
      /* should we return -1 and set errno = EBADMSG instead? */
   }

   h = width;
   v = height;

   /* Read luminance scanlines */

   for (i = 0; i < v; i++)
      if (piperead (fd, yuv[0] + i * h, h) != h)
         return 0;

   v /= 2;
   h /= 2;

   /* Read chrominance scanlines */

   for (i = 0; i < v; i++)
      if (piperead (fd, yuv[1] + i * h, h) != h)
         return 0;
   for (i = 0; i < v; i++)
      if (piperead (fd, yuv[2] + i * h, h) != h)
         return 0;
   return 1;
}

#define PARAM_LINE_MAX 256

int 
yuv_read_header (int fd_in, int *horizontal_size, int *vertical_size,
				 int *frame_rate_code)
{
   int n, nerr;
   char param_line[PARAM_LINE_MAX];

   for (n = 0; n < PARAM_LINE_MAX; n++) {
      if ((nerr = read (fd_in, param_line + n, 1)) < 1) {
         printf("Error (yuv4mpeg.c): Error reading header from stdin\n");
         /* set errno if nerr == 0 ? */
         return -1;
      }
      if (param_line[n] == '\n')
         break;
   }
   if (n == PARAM_LINE_MAX) {
      printf("Error (yuv4mpeg.c): Didn't get linefeed in first %d characters of data\n",
               PARAM_LINE_MAX);
      /* set errno to EBADMSG? */
      return -1;
   }
   param_line[n] = 0;           /* Replace linefeed by end of string */

   if (strncmp (param_line, "YUV4MPEG", 8)) {
      printf("Error (yuv4mpeg.c): Input does not start with \"YUV4MPEG\"\n");
      printf("Error (yuv4mpeg.c): This is not a valid input for me\n");
      /* set errno to EBADMSG? */
      return -1;
   }

   sscanf (param_line + 8, "%d %d %d", horizontal_size, vertical_size,
           frame_rate_code);

   nerr = 0;
   if (*horizontal_size <= 0) {
      printf("Error (yuv4mpeg.c): Horizontal size illegal\n");
      nerr++;
   }
   if (*vertical_size <= 0) {
      printf("Error (yuv4mpeg.c): Vertical size illegal\n");
      nerr++;
   }

   /* Ignore frame rate code */
   return nerr ? -1 : 0;
}

void
yuv_write_header (int fd, int width, int height, int frame_rate_code)
{
   char str[256];

   snprintf (str, sizeof (str), "YUV4MPEG %d %d %d\n",
             width, height, frame_rate_code);
   pipewrite (fd, str, strlen (str));
}

void yuv_write_frame (int fd, unsigned char *yuv[], int width,
                      int height)
{

   pipewrite (fd, "FRAME\n", 6);

/*FIXME! this is subject to be extended to work with lav2yuv: */

   pipewrite (fd, yuv[0], width*height);
   pipewrite (fd, yuv[1], width*height/4);
   pipewrite (fd, yuv[2], width*height/4);
   
}
