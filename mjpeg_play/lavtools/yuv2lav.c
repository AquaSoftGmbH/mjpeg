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

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include "mjpeg_logging.h"

#include "yuv4mpeg.h"
#include "jpegutils.h"
#include "lav_io.h"


/* max_file_size stuff */
#define MAX_MBYTES_PER_FILE_64 ((0x3fffff) * 93/100)        // 4Tb.  (Most filesystems have 
                                                            // fundamental limitations around ~Tb)
#define MAX_MBYTES_PER_FILE_32 ((0x7fffffff >> 20) * 85/100)// Is less than 2^31 and 2*10^9
#if _FILE_OFFSET_BITS == 64
#define MAX_MBYTES_PER_FILE MAX_MBYTES_PER_FILE_64
#else
#define MAX_MBYTES_PER_FILE MAX_MBYTES_PER_FILE_32
                                  /* Maximum number of Mbytes per file.
                                     We make a conservative guess since we
                                     only count the number of bytes output
                                     and don't know how big the control
                                     information will be */
#endif


static int   param_quality = 80;
static char  param_format = 'a';
static char *param_output = 0;
static int   param_bufsize = 256*1024; /* 256 kBytes */
static int   param_interlace = -1;
static int   param_maxfilesize = MAX_MBYTES_PER_FILE;

static int got_sigint = 0;

static int verbose = 1;

static void usage() 
{
  fprintf(stderr,
	  "Usage:  yuv2lav [params] -o <filename>\n"
	  "where possible params are:\n"
	  "   -v num      Verbosity [0..2] (default 1)\n"
	  "   -f [aA"
#ifdef HAVE_LIBQUICKTIME
                   "q"
#else
                   " "
#endif
#ifdef HAVE_LIBMOVTAR
                    "m"
#else
                    " "
#endif
                     "]   output format (AVI"
#ifdef HAVE_LIBQUICKTIME
                                           "/Quicktime"
#endif
#ifdef HAVE_LIBMOVTAR
                                                     "/movtar"
#endif
                                                            ") [%c]\n"
	  "   -I num      force output interlacing 0:no 1:top 2:bottom field first\n"
	  "   -q num      JPEG encoding quality [%d%%]\n"
	  "   -b num      size of MJPEG buffer [%d kB]\n"
          "   -m num      maimum size per file [%d MB]\n"
	  "   -o file     output mjpeg file (REQUIRED!)\n",
	  param_format, param_quality, param_bufsize/1024, param_maxfilesize);
}

static void sigint_handler (int signal) {
 
   mjpeg_debug( "Caught SIGINT, exiting...\n");
   got_sigint = 1;
   
}

int main(int argc, char *argv[])
{

   int frame;
   int fd_in;
   int n;
   lav_file_t *output = 0;
   int filenum = 0;
   unsigned long filesize_cur = 0;
   
   char *jpeg;
   int   jpegsize = 0;
   unsigned char *yuv[3];

   y4m_frame_info_t frameinfo;
   y4m_stream_info_t streaminfo;

   while ((n = getopt(argc, argv, "v:f:I:q:b:m:o:")) != -1) {
      switch (n) {
      case 'v':
         verbose = atoi(optarg);
         if (verbose < 0 || verbose > 2) {
            mjpeg_error( "-v option requires arg 0, 1, or 2\n");
			usage();
         }
         break;

      case 'f':
         switch (param_format = optarg[0]) {
         case 'a':
         case 'A':
#ifdef HAVE_LIBQUICKTIME
         case 'q':
#endif
#ifdef HAVE_LIBMOVTAR
         case 'm':
#endif
            /* do interlace setting here? */
            continue;
         default:
            mjpeg_error( "-f parameter must be one of [aA"
#ifdef HAVE_LIBQUICKTIME
                                                        "q"
#endif
#ifdef HAVE_LIBMOVTAR
                                                         "m"
#endif
                                                          "]\n");
            usage ();
            exit (1);
         }
         break;
      case 'I':
	 param_interlace = atoi(optarg);
         if (2 < param_interlace) {
            mjpeg_error("-I parameter must be one of 0,1,2\n");
            exit (1);
         }
	 break;
      case 'q':
         param_quality = atoi (optarg);
         if ((param_quality<24)||(param_quality>100)) {
            mjpeg_error( "quality parameter (%d) out of range [24..100]!\n", param_quality);
            exit (1);
         }
         break;
      case 'b':
         param_bufsize = atoi (optarg) * 1024;
         /* constraints? */
         break;
      case 'm':
         param_maxfilesize = atoi(optarg);
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
      mjpeg_error( "yuv2lav needs an output filename\n");
      usage ();
      exit (1);
   }
   if (param_interlace == 2 &&
       (param_format == 'q' || param_format == 'm')) {
      mjpeg_error("cannot use -I 2 with -f %c\n", param_format);
      usage ();
      exit (1);
   }

   (void)mjpeg_default_handler_verbosity(verbose);   
   fd_in = 0;                   /* stdin */

   y4m_init_stream_info(&streaminfo);
   y4m_init_frame_info(&frameinfo);
   if (y4m_read_stream_header(fd_in, &streaminfo) != Y4M_OK) {
      mjpeg_error( "Couldn't read YUV4MPEG header!\n");
      exit (1);
   }

   if (param_interlace < 0) {
       int ylace = y4m_si_get_interlace(&streaminfo);
       param_interlace = 
	 (ylace == Y4M_ILACE_NONE) ? LAV_NOT_INTERLACED :
	 (ylace == Y4M_ILACE_TOP_FIRST) ? LAV_INTER_TOP_FIRST :
	 (ylace == Y4M_ILACE_BOTTOM_FIRST) ? LAV_INTER_BOTTOM_FIRST :
	 LAV_INTER_UNKNOWN;
   }
   if (param_interlace == LAV_INTER_TOP_FIRST  && param_format == 'A')
      param_format = 'a';
   else if (param_interlace == LAV_INTER_BOTTOM_FIRST && param_format == 'a')
      param_format = 'A';

   if (strstr(param_output,"%"))
   {
      char buff[256];
      sprintf(buff, param_output, filenum++);
      output = lav_open_output_file (buff, param_format,
                                  y4m_si_get_width(&streaminfo),
				  y4m_si_get_height(&streaminfo),
				  param_interlace,
                                  Y4M_RATIO_DBL(y4m_si_get_framerate(&streaminfo)),
                                  0, 0, 0);
      if (!output) {
         mjpeg_error( "Error opening output file %s: %s\n", buff, lav_strerror ());
         exit(1);
      }
   }
   else
   {
      output = lav_open_output_file (param_output, param_format,
                                  y4m_si_get_width(&streaminfo),
				  y4m_si_get_height(&streaminfo),
				  param_interlace,
                                  Y4M_RATIO_DBL(y4m_si_get_framerate(&streaminfo)),
                                  0, 0, 0);
//                                audio_bits, audio_chans, audio_rate);
      if (!output) {
         mjpeg_error( "Error opening output file %s: %s\n", param_output, lav_strerror ());
         exit(1);
      }
   }

   yuv[0] = malloc(y4m_si_get_width(&streaminfo) *
		   y4m_si_get_height(&streaminfo) *
		   sizeof(unsigned char));
   yuv[1] = malloc(y4m_si_get_width(&streaminfo) * 
		   y4m_si_get_height(&streaminfo) *
		   sizeof(unsigned char) / 4);
   yuv[2] = malloc(y4m_si_get_width(&streaminfo) * 
		   y4m_si_get_height(&streaminfo) *
		   sizeof(unsigned char) / 4);
   jpeg = malloc(param_bufsize);
   
   signal (SIGINT, sigint_handler);

   frame = 0;
   while (y4m_read_frame(fd_in, &streaminfo, &frameinfo, yuv)==Y4M_OK && (!got_sigint)) {

      fprintf (stdout, "frame %d\r", frame);
      fflush (stdout);
      jpegsize = encode_jpeg_raw (jpeg, param_bufsize, param_quality,
                                  param_interlace,
				  0 /* ctype */,
                                  y4m_si_get_width(&streaminfo),
				  y4m_si_get_height(&streaminfo),
                                  yuv[0], yuv[1], yuv[2]);
      if (jpegsize==-1) {
         mjpeg_error( "Couldn't compress YUV to JPEG\n");
         exit(1);
      }

      if ((filesize_cur + jpegsize)>>20 > param_maxfilesize)
      {
         /* max file size reached, open a new one if possible */
         if (strstr(param_output,"%"))
         {
            char buff[256];
            if (lav_close (output)) {
               mjpeg_error("Closing output file: %s\n",lav_strerror());
               exit(1);
            }
            sprintf(buff, param_output, filenum++);
            output = lav_open_output_file (buff, param_format,
                                        y4m_si_get_width(&streaminfo),
                                        y4m_si_get_height(&streaminfo),
                                        param_interlace,
                                        Y4M_RATIO_DBL(y4m_si_get_framerate(&streaminfo)),
                                        0, 0, 0);
            if (!output) {
               mjpeg_error( "Error opening output file %s: %s\n", buff, lav_strerror ());
               exit(1);
            }
            mjpeg_info("Maximum file size reached (%d MB), opened new file: %s\n",
               param_maxfilesize, buff);
            filesize_cur = 0;
         }
         else
         {
            mjpeg_warn("Maximum file size reached\n");
            break;
         }
      }

      if (lav_write_frame (output, jpeg, jpegsize, 1)) {
         mjpeg_error( "Writing output: %s\n", lav_strerror());
         exit(1);
      }
      filesize_cur += jpegsize;
      frame++;

   }

   if (lav_close (output)) {
      mjpeg_error("Closing output file: %s\n",lav_strerror());
      exit(1);
   }
      
   for (n=0; n<3; n++) {
      free(yuv[n]);
   }

   y4m_fini_frame_info(&frameinfo);
   y4m_fini_stream_info(&streaminfo);

   return 0;
}
