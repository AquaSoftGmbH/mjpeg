/*
 *  ypipe - simple test program that blends the output of two
 *          YUV4MPEG-emitting programs together into one output stream
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
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>

#include "yuv4mpeg.h"

static char **parse_cmdline (char *cmdline) {

   char **argv;
   char *p = cmdline;
   int i, argc = 0;

   if (!p) return 0;
   if (!*p) return 0;

   while (*p) {    
      while (!isspace(*p)) {
         p++;
         if (!*p) {
            argc++;
            goto EOL_HANDLE;
         }
      }
      argc++;     
      while (isspace(*p)) {
         p++;
         if (!*p) goto EOL_HANDLE;
      }
   }
EOL_HANDLE:
   argv = (char **) malloc (argc+1 * sizeof (char *));
   for (p=cmdline, i=0; i<argc; i++) {
      argv[i] = p;
      while (!isspace(*(++p)));
      p[0] = '\0';
      while (isspace(*(++p)))
         if (!p[0]) break;
   }
   argv[argc] = NULL;

   return argv;

}

static int fork_child (char *child) {

   int n;
   int pid;
   int mypipe[2];
   char **myargv;
   
   if (pipe (mypipe)) {
      fprintf (stderr, "Couldn't create pipe to %s\n", child);
      exit (1);
   }
   if ((pid = fork ())<0) {
      fprintf (stderr, "Couldn't fork %s\n", child);
      exit (1);
   }
   
   if (pid) {
      /* child redirects stdout to pipe */
      close (mypipe[0]);
      close(1);
      n = dup(mypipe[1]);
      if(n!=1) exit(1);
      myargv = parse_cmdline (child);
      execvp (myargv[0], myargv);
      return -1;
   } else {
      /* parent */
      close (mypipe[1]);
   /* fcntl (mypipe[0], F_SETFL, O_NONBLOCK); */
      return mypipe[0];
   }
}

int main (int argc, char *argv[]) {

   int stream0;   /* read from input 1 */
   int stream1;   /* read from input 2 */
   int w0, h0, w1, h1;
   int rate0, rate1;
   int outstream = 1;
   int n, i,j, x,y;
   unsigned char *yuv[3];
   unsigned char *yuv0[3];
   unsigned char *yuv1[3];

   if (argc != 3) {
      fprintf (stderr, "usage:   ypipe <input1> <input2>\n");
      fprintf (stderr, "example: ypipe \"lav2yuv test1.el\" \"lav2yuv -n1 test2.avi\"\n");
      exit (1);
   }

   stream0 = fork_child (argv[1]);
   stream1 = fork_child (argv[2]);

   i = yuv_read_header (stream0, &w0, &h0, &rate0);
   j = yuv_read_header (stream1, &w1, &h1, &rate1);
   if (i || j) {
      fprintf (stderr, "Couldn't read YUV4MPEG header (%d/%d)\n", !!i, !!j);
      exit (1);
   }
   if ((w0 != w1)||(h0 != h1)) {
      fprintf (stderr, "Different dimensions: (%dx%d) contra (%dx%d)\n", w0, h0, w1, h1);
      exit (1);
   }
   if (rate0 != rate1) {
      fprintf (stderr, "warning: not the same frame_rate_code - different frame rates!\n");
   }
   
   yuv[0] = (char *)malloc (w0*h0);   (char *)yuv0[0] = malloc (w0*h0);   (char *)yuv1[0] = malloc (w0*h0);
   yuv[1] = (char *)malloc (w0*h0/4); (char *)yuv0[1] = malloc (w0*h0/4); (char *)yuv1[1] = malloc (w0*h0/4);
   yuv[2] = (char *)malloc (w0*h0/4); (char *)yuv0[2] = malloc (w0*h0/4); (char *)yuv1[2] = malloc (w0*h0/4);

   yuv_write_header (outstream, w0, h0, rate0);
   
   while ((i = yuv_read_frame(stream0, yuv0, w0, h0)) &&
          (j = yuv_read_frame(stream1, yuv1, w1, h1))) {
      if ((i<0)||(j<0)) exit (1);
      
      /* PROCESS YUV FRAMES HERE */
      /* test: Simple 50% / 50% blending.*/
      
      for (i=0; i<w0*h0; i++)
         yuv[0][i] = (yuv0[0][i] + yuv1[0][i])/2;
         
      for (i=0; i<w0*h0/4; i++) {
         yuv[1][i] = (yuv0[1][i] + yuv1[1][i])/2;
         yuv[2][i] = (yuv0[2][i] + yuv1[2][i])/2;
      }         
      
      yuv_write_frame (outstream, yuv, w0, h0);    
   }
   fflush (stderr);
   fflush (stdout);

}