/*
 *  lavpipe - combines several input streams and pipes them trough
 *            arbitrary filters in order to finally output a resulting
 *            video stream based on a given "recipe" (pipe list)
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
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>

#include "mjpeg_logging.h"

#include "pipelist.h"
#include "yuv4mpeg.h"

int verbose = 1;

static int param_offset = 0;
static int param_frames = 0;

static void usage () {
 
   fprintf (stderr, "Usage: lavpipe [options] <pipe list>\n");
   fprintf (stderr, "Options: -o num   Frame offset - skip num frames in the beginning\n");
   fprintf (stderr, "                  if num is negative, all but the last num frames are skipped\n");
   fprintf (stderr, "         -n num   Only num frames are processed (0 means all frames)\n");
   fprintf (stderr, "         -v num  Verbosity of output [0..2]\n");
   
}

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


static pid_t fork_child_sub (char *child, int *fd_in, int *fd_out) {

   int n;
   int pipe_in[2];
   int pipe_out[2];
   char **myargv;
   pid_t pid;

   if (fd_in) {
      if (pipe (pipe_in)) {
         fprintf (stderr, "Couldn't create input pipe from %s\n", child);
         exit (1);
      }
   }
   if (fd_out) {
      if (pipe (pipe_out)) {
         fprintf (stderr, "Couldn't create output pipe to %s\n", child);
         exit (1);
      }
   }         
   if ((pid = fork ())<0) {
      fprintf (stderr, "Couldn't fork %s\n", child);
      exit (1);
   }
   
   if (pid == 0) {
      /* child redirects stdout to pipe_in */
      if (fd_in) {
         close (pipe_in[0]);
         close(1);
         n = dup (pipe_in[1]);
         if(n!=1) exit (1);
      }
      /* child redirects stdin to pipe_out */
      if (fd_out) {
         close (pipe_out[1]);
         close (0);
         n = dup (pipe_out[0]);
         if(n!=0) exit (1);
      }
      myargv = parse_cmdline (child);
      execvp (myargv[0], myargv);
      return -1;
   } else {
      /* parent */
      if (fd_in) {
         close (pipe_in[1]);
         *fd_in = pipe_in[0];
      }
      if (fd_out) {
         close (pipe_out[0]);
         *fd_out = pipe_out[1];
      }
   /* fcntl (mypipe[0], F_SETFL, O_NONBLOCK); */
      return pid;
   }
}


static pid_t fork_child (char *cmd, int ofs, int num, int *fd_in, int *fd_out)
{
   char tmp1[1024], tmp2[1024];
   char *p1 = tmp1, *p2;

   /* replace $o by offset */

   strncpy (p1, cmd, 1024);
   p2 = strstr(p1, "$o");
   if (p2) {
      p2[0] = '\0';
      p2 += 2;
      snprintf (tmp2, 1024, "%s%d%s", p1, ofs, p2);
      p1 = tmp2;
   }
   
   /* replace $n by number of frames */
   
   p2 = strstr(p1, "$n");
   if (p2) {
      p2[0] = '\0';
      p2 += 2;
      snprintf ((char *)((int)tmp1+(int)tmp2-(int)p1), 1024, "%s%d%s", p1, num, p2);
      p1 = (char *)((int)tmp1+(int)tmp2-(int)p1);
   }
   
   mjpeg_debug( "Executing: \"%s\"\n", p1);
   return fork_child_sub (p1, fd_in, fd_out);
}


int main (int argc, char *argv[]) {

   PipeList pl;
   int      sequence = 0;
   int      frame, frame_total = 0;
   int      i, j, iteration = 0;
   int      width = 0, height = 0, rate = 0, w, h, r;

   int *input_in;
   int output_in, output_out;
   pid_t *input_pid, output_pid = 0;
   
   unsigned char *yuv[3];

   while ((i = getopt(argc, argv, "o:n:v:")) != EOF) {
      switch (i) {
      case 'o':
         param_offset = atoi (optarg);
         break;
      case 'n':
         param_frames = atoi (optarg);
         break;
      case 'v':
         verbose = atoi (optarg);
		 if( verbose < 0 || verbose >2 )
		 {
			 usage ();
			 exit (1);
		 }
         break;

      default:
         iteration++;
      }
   }
   if ((optind >= argc) || (iteration)) {
      usage ();
      exit (1);
   }

   (void)mjpeg_default_handler_verbosity(verbose);

   if (open_pipe_list (argv[optind], &pl) <0) {
      mjpeg_error_exit1( "lavpipe: couldn't open \"%s\"\n", argv[optind]);
   }

   if (param_offset < 0) {
      param_offset = pl.frame_num + param_offset;
   }   
   if (param_offset >= pl.frame_num) {
      mjpeg_error_exit1( "error: offset greater than # of frames in input\n");
   }
   if ((param_offset + param_frames) > pl.frame_num) {
      mjpeg_warn( "input too short for -n %d\n", param_frames);
      param_frames = pl.frame_num - param_offset;
   }
   if (param_frames == 0) {
      param_frames = pl.frame_num - param_offset;
   }

   /* init input stream fds */
   
   input_in  = (int *)   malloc (pl.input_num * sizeof(int));
   input_pid = (pid_t *) malloc (pl.input_num * sizeof(pid_t));
   for (i=0; i<pl.input_num; i++) input_in[i] = -1;

   /* find start sequence */

   frame = param_offset;
   while (frame >= pl.seq_ptr[sequence]->frame_num) {
      frame -= pl.seq_ptr[sequence]->frame_num;
      sequence++;
   }
   
   mjpeg_debug("starting sequence %d, frame %d\n", sequence, frame);

   while (sequence < pl.seq_num) {

      PipeSequence *seq = pl.seq_ptr[sequence];

      /* open sequence input (if not already open) */

      for (i=0; i<seq->input_num; i++) {
         if (input_in[seq->input_ptr[i]] < 0) {
            /* calculate # of frames we want to get from this stream */
            /* need to look if we can use this in successive sequences and what param_frames is */

            int ofs = seq->input_ofs[i] + frame;
            int k, num = seq->frame_num - frame; /* until end of sequence */

            for (j=sequence+1; j<pl.seq_num; j++) {
               for (k=0; k<pl.seq_ptr[j]->input_num; k++) {
                  if (seq->input_ptr[i] == pl.seq_ptr[j]->input_ptr[k]) {
                     if ((ofs + num) == pl.seq_ptr[j]->input_ofs[k]) {
                        num += pl.seq_ptr[j]->frame_num; /* add another sequence */
                     } else
                        goto FINISH_CHECK; /* need to reopen with other offset */
                  } else
                     goto FINISH_CHECK; /* stream will not be used in sequence j anymore */
               }
            }
FINISH_CHECK:
            if (num > param_frames - frame_total) {
               num = param_frames - frame_total;
            }

            input_pid[seq->input_ptr[i]] = fork_child (pl.input_cmd[seq->input_ptr[i]],
                                                       ofs, num,
                                                       &input_in[seq->input_ptr[i]], NULL);
            yuv_read_header (input_in[seq->input_ptr[i]], &w, &h, &r);
            if (width == 0) {
               width = w; height = h; rate = r;
            } else {
               if ((width != w) || (height != h) /* || (rate != r) */) {
                  mjpeg_error_exit1( "input dimensions differ - please use lavscale or input streams with the same width/height\n");
               }
            }
         }
      }
      
      if (strcmp (seq->output_cmd, "-")) {
         output_pid = fork_child (seq->output_cmd, frame, seq->frame_num - frame, &output_in, &output_out);
      } else {
         output_out = output_in = -1;
      }

      if (output_out >= 0) {
         yuv_write_header (output_out, width, height, rate);
         if (output_in >= 0) {
            yuv_read_header (output_in, &w, &h, &r);
         } else {
            w = width;
            h = height;
            r = rate;
         }
      } else {
         w = width;
         h = height;
         r = rate;
      }

      /* write out header and malloc yuv buffers once*/

      if (iteration == 0) {
         yuv_write_header (1, w, h, r);
         
         /* could be that the filter rescales the frames */

         if (w*h > width*height) {
            yuv[0] = (char *) malloc (w * h);
            yuv[1] = (char *) malloc (w * h / 4);
            yuv[2] = (char *) malloc (w * h / 4);
         } else {
            yuv[0] = (char *) malloc (width * height);
            yuv[1] = (char *) malloc (width * height / 4);
            yuv[2] = (char *) malloc (width * height / 4);
         }
      }

      while (frame < seq->frame_num) {
         
         for (i=0; i<seq->input_num; i++) {
            if (yuv_read_frame (input_in[seq->input_ptr[i]], yuv, width, height) == 0) {
               mjpeg_error_exit1( "lavpipe: input stream error in stream %d, sequence %d, frame %d\n", i, sequence, frame);
            }
            if (output_out >= 0) yuv_write_frame (output_out, yuv, width, height);
         }
         if (output_in >= 0) {
            if (yuv_read_frame (output_in, yuv, w, h) == 0) {
               mjpeg_error_exit1( "lavpipe: filter stream error in sequence %d, frame %d\n", sequence, frame);
            }
         }

         /* output result */

         yuv_write_frame (1, yuv, w, h);

         frame++;
         if (++frame_total == param_frames) {
            sequence = pl.seq_num - 1; /* = close all input files below */
            break;
         }
      }

      if (output_in >= 0) {
         close (output_in);
         output_in = -1;
      }
      if (output_out >= 0) {
         close (output_out);
         output_out = -1;
      }
      if (output_pid > 0) {
         kill (output_pid, SIGINT);
         output_pid = 0;
      }

      /* prepare for next sequence */

      sequence++;
      iteration++;
      
      for (i=0; i<pl.input_num; i++) {
         if (input_in[i] >= 0) {
            if (sequence == pl.seq_num) {
               mjpeg_info( "closing input %d: \"%s\"\n", i, pl.input_cmd[i]);
               close (input_in[i]);
               kill (input_pid[i], SIGINT);
               input_in[i] = -1;
            } else {
               for (j=0; j<pl.seq_ptr[sequence]->input_num; j++)
                  if (i == pl.seq_ptr[sequence]->input_ptr[j])
                     if (pl.seq_ptr[sequence]->input_ofs[j] == seq->input_ofs[i] + frame) {
                        mjpeg_info( "using input %d (\"%s\") further\n", i, pl.input_cmd[i]);
                        goto DO_NOT_CLOSE;
                     }
               mjpeg_info( "closing input %d: \"%s\"\n", i, pl.input_cmd[i]);
               close (input_in[i]);
               kill (input_pid[i], SIGINT);
               input_in[i] = -1;
DO_NOT_CLOSE:
            }
         }
      }

      frame = 0;
      
      if (frame_total == param_frames)
         break;

   }
   
   free (yuv[0]); free (yuv[1]); free (yuv[2]);

   return 0;

}
