/*
 *  yuv2lav - write YUV4MPEG data stream from stdin to mjpeg file
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
#include <signal.h>
#include "yuv4mpeg.h"
#include "jpegutils.h"
#include "lav_io.h"

static int   param_quality = 80;
static char  param_format = 'a';
static char *param_output = 0;
static int   param_bufsize = 256*1024; /* 256 kBytes */

static int got_sigint = 0;

static void usage (){
  
   fprintf (stderr, "Usage:  yuv2lav [params] -o <filename>\n"
                    "where possible params are:\n"
                    "   -f [aAqm]   output format (AVI/Quicktime/movtar) [%c]\n"
                    "   -q num      JPEG encoding quality [%d%%]\n"
                    "   -b num      size of MJPEG buffer [%d kB]\n"
                    "   -o file     output mjpeg file (REQUIRED!) \n",
                    param_format, param_quality, param_bufsize/1024);

}

static void sigint_handler (int signal) {
 
   fprintf (stderr, "Caught SIGINT, exiting...\n");
   got_sigint = 1;
   
}

int main(int argc, char *argv[])
{

   int frame;
   int fd_in;
   int n, width, height;
   lav_file_t *output = 0;
   int frame_rate_code;
   
   char *jpeg;
   int   jpegsize = 0;
   unsigned char *yuv[3];


   while ((n = getopt(argc, argv, "f:q:b:o:")) != -1) {
      switch (n) {
      case 'f':
         switch (param_format = optarg[0]) {
         case 'a':
         case 'A':
         case 'q':
         case 'm':
            /* do interlace setting here? */
            continue;
         default:
            fprintf (stderr, "-f parameter must be one of [aAqm]\n");
            usage ();
            exit (1);
         }
         break;
      case 'q':
         param_quality = atoi (optarg);
         if ((param_quality<24)||(param_quality>100)) {
            fprintf (stderr, "quality parameter (%d) out of range [24..100]!\n", param_quality);
            exit (1);
         }
         break;
      case 'b':
         param_bufsize = atoi (optarg) * 1024;
         /* constraints? */
         break;
      case 'o':
         param_output = optarg;
         break;
      default:
         usage();
         exit(1);
      }
   }
   
   if (!param_output) {
      fprintf (stderr, "yuv2lav needs an output filename\n");
      usage ();
      exit (1);
   }
   
   fd_in = 0;                   /* stdin */

   if (yuv_read_header(fd_in, &width, &height, &frame_rate_code)) {
      fprintf (stderr, "Could'nt read YUV4MPEG header!\n");
      exit (1);
   }
   
   /* how to determine if input is interlaced? at the moment, we can do this
      via the input frame height, but this relies on PAL/NTSC standard input: */
   if (((height >  288) && (frame_rate_code == 3)) ||  /* PAL */
       ((height >  240) && (frame_rate_code == 4))) {  /* NTSC */
       n = (param_format == 'A') ? LAV_INTER_EVEN_FIRST : LAV_INTER_ODD_FIRST;
   } else                                              /* 24fps movie, etc. */
       n = LAV_NOT_INTERLACED;

   output = lav_open_output_file (param_output, param_format, width, height, n,
                                  yuv_mpegcode2fps (frame_rate_code),
                                  0, 0, 0);
//                                audio_bits, audio_chans, audio_rate);
   if (!output) {
      fprintf (stderr, "Error opening output file %s: %s\n", param_output, lav_strerror ());
      exit(1);
   }

   yuv[0] = malloc(width * height * sizeof(unsigned char));
   yuv[1] = malloc(width * height * sizeof(unsigned char) / 4);
   yuv[2] = malloc(width * height * sizeof(unsigned char) / 4);
   jpeg = malloc(param_bufsize);
   
   signal (SIGINT, sigint_handler);

   frame = 0;
   while ((yuv_read_frame(fd_in, yuv, width, height)) && (!got_sigint)) {

      fprintf (stdout, "frame %d\r", frame);
      fflush (stdout);
      jpegsize = encode_jpeg_raw (jpeg, param_bufsize, param_quality,
                                  n /* itype */, 0 /* ctype */,
                                  width, height, yuv[0], yuv[1], yuv[2]);
      if (jpegsize==-1) {
         fprintf (stderr, "Couldn't compress YUV to JPEG\n");
         exit(1);
      }
      if (lav_write_frame (output, jpeg, jpegsize, 1)) {
         fprintf (stderr, "Error writing output: %s\n", lav_strerror());
         exit(1);
      }
      frame++;

   }

   if (lav_close (output)) {
      fprintf(stderr,"Error closing output file: %s\n",lav_strerror());
      exit(1);
   }
      
   for (n=0; n<3; n++) {
      free(yuv[n]);
   }

   return 0;
}
