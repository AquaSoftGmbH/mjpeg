/*
jpeg2yuv
========

  Converts a collection of JPEG images to a YUV4MPEG stream.
  (see jpeg2yuv -h for help (or have a look at the function "usage"))
  
  Copyright (C) 1999 Gernot Ziegler (gz@lysator.liu.se)

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string.h>
#include <jpeglib.h>
#include "jpegutils.h"
#include <sys/types.h>

#include "mjpeg_logging.h"
#include "mjpeg_types.h"

#include "oldyuv4mpeg.h"

#define MAXPIXELS (1280*1024)  /* Maximum size of final image */

static unsigned char *jpegformatstr   = NULL;

static float framerate = -1.0;
static int fpscode = 0;
static uint32_t begin = 0; /* the video frame start */
static uint32_t numframes = -1; /* -1 means: take all frames */

static int verbose = 1; /* the verbosity of the program (see mjpeg_logging.h) */
static int interlaced = -1; /* tells if the YUV4MPEG stream shall be interlaced */
static int interlaced_top_first = -1; /* tells if the YUV4MPEG's frames are bottom first or top first */

static int image_width = 0;
static int image_height = 0;

static struct jpeg_decompress_struct dinfo;
static struct jpeg_error_mgr jerr;

void usage(char *prog);
void parse_commandline(int argc, char ** argv);
int generate_YUV4MPEG(void);
int init_parse_files(void);

/*
 * The User Interface parts 
 */

/* usage
 * Prints a short description of the program, including default values 
 * in: prog: The name of the program 
 */
void usage(char *prog)
{
    char *h;

    if (NULL != (h = (char *)strrchr(prog,'/')))
	prog = h+1;
    
  mjpeg_error(
	  "%s pipes a stream of JPEG files to stdout,\n"
	  "making the direct encoding of MPEG files possible under mpeg2enc.\n"
	  "Any JPEG format supported by libjpeg can be read.\n"
	  "stdout will be filled with the YUV4MPEG movie data stream,\n"
	  "so be prepared to pipe it on to mpeg2enc or to write it into a file.\n"
	  "\n"
	  "usage: %s [ options ]\n"
	  "\n"
	  "where options are ([] shows the defaults):\n"
	  "  -v num      Verbosity [0..2] (default 1)\n"
	  "  -b framenr. start at given frame nr.               [0]\n"
	  "  -n numframes processes the given nr. of frames     [all]\n"
	  "  -j {1}%%{2}d{3} Read JPEG frames with the name components as follows:\n"
	  "               {1} JPEG filename prefix (e g rendered_ )\n"
	  "               {2} Counting placeholder (like in C, printf, eg 06 ))\n"
	  "  -I force interlaced mode (default:autodetect)\n"
	  "     ONLY for concattenated JPEGs, like from MJPEG hardware !\n"
	  "  -T x : (only for Interlaced) x=1: Top picture first, x=0: Bottom picture first\n"
	  "         (can NOT be autodetected !)\n"
	  "\n"
	  "\n"
	  "examples:\n"
	  "  %s -j in_%%06d.jpeg -b 100000 > result.yuv\n"
          "  | combines all the available JPEGs that match \n"
          "    in_??????.jpeg, starting with 100000 (in_100000.jpeg, \n"
          "    in_100001.jpeg, etc...) into the uncompressed YUV4MPEG videofile result.yuv\n"
	  "  %s -j abc_%%04d.jpeg | mpeg2enc -f3 -o out.m2v\n"
          "  | combines all the available JPEGs that match \n"
          "    abc_??????.jpeg, starting with 0000 (abc_0000.jpeg, \n"
          "    abc_0001.jpeg, etc...) and pipes it to mpeg2enc which encodes\n"
          "    an MPEG2-file called out.m2v out of it\n"
	  "\n"
	  "--\n"
	  "(c) 2001 Gernot Ziegler <gz@lysator.liu.se>\n",
	  prog, prog, prog, prog);
}

/* parse_commandline
 * Parses the commandline for the supplied parameters.
 * in: argc, argv: the classic commandline parameters
 */
void parse_commandline(int argc, char ** argv)
{ int c;

/* parse options */
 for (;;) {
   if (-1 == (c = getopt(argc, argv, "Ihv:T:b:j:n:f:")))
     break;
   switch (c) {
   case 'j':
     jpegformatstr = optarg;
     break;
   case 'b':
     begin = atol(optarg);
     break;
   case 'n':
     numframes = atol(optarg);
     break;
   case 'f':
     framerate = atof(optarg);
     break;
   case 'I':
     interlaced = 1;
     break;
   case 'T':
     interlaced_top_first = atoi(optarg);
     break;
   case 'v':
     verbose = atoi(optarg);
     if (verbose < 0 ||verbose > 2) 
       mjpeg_error_exit1( "-v option requires arg 0, 1, or 2\n");    
     break;     
   case 'h':
   default:
     usage(argv[0]);
     exit(1);
   }
 }
 if (jpegformatstr == NULL)
   { 
     mjpeg_error("%s needs an input filename to proceed. (-j option)\n\n", argv[0]); 
     usage(argv[0]); 
     exit(1);
   }

 if (framerate == -1.0)
   { mjpeg_error("%s needs the framerate of the forthcoming YUV4MPEG movie. (-f option)\n\n", argv[0]); 
     usage(argv[0]); 
     exit(1);
   }
}

/*
 * The file handling parts 
 */

/** init_parse_files
 * Verifies the JPEG input files and prepares YUV4MPEG header information.
 * @returns 1 on success
 */
int init_parse_files()
{ 
  unsigned char jpegname[255];
  FILE *jpegfile;

  snprintf(jpegname, sizeof(jpegname), jpegformatstr, begin);
  mjpeg_debug("Analyzing %s to get the right pic params\n", jpegname);
  jpegfile = fopen(jpegname, "r");

  if (jpegfile == NULL)
    {
      mjpeg_error("While opening: \"%s\"\n", jpegname);
      perror("Fatal: Couldn't open first given JPEG frame"); 
      exit(1);
    }
  else
    {
      /* Now open this JPEG file, and examine its header to retrieve the 
	 YUV4MPEG info that shall be written */
      dinfo.err = jpeg_std_error(&jerr);
      jpeg_create_decompress(&dinfo);
      jpeg_stdio_src(&dinfo, jpegfile);
      
      jpeg_read_header(&dinfo, 1);
      dinfo.out_color_space = JCS_YCbCr;      
      jpeg_start_decompress(&dinfo);
      
      if(dinfo.output_components != 3)
	mjpeg_error_exit1("Output components of JPEG image = %d, must be 3\n", dinfo.output_components);
      
      mjpeg_info("Image dimensions are %dx%d, framerate = %f frames/s\n",
	      dinfo.image_width, dinfo.image_height, framerate);
      image_width = dinfo.image_width;
      image_height = dinfo.image_height;

      jpeg_destroy_decompress(&dinfo);
      fclose(jpegfile);
    }

  fpscode = yuv_fps2mpegcode (framerate);
  if (fpscode == 0) 
    {
      mjpeg_error_exit1("%f frames/s is an unsupported framerate !\n"
	      "use: 23.976, or\n"
	      "24.000, /* 24fps movie */\n"
	      "25.000, /* PAL */\n"
	      "29.970, /* NTSC */\n"
	      "30.000, 50.000, 59.940, 60.000\n"
	      "\n", framerate);
    }
  else
    {
      mjpeg_info("movie frame rate is: %f frames/s\n", framerate);
    }

  if (interlaced == -1)
    {
      if (image_height / image_width >= 2)
	{
	  mjpeg_info("INTERLACED format detected, doubling frame height to %d\n", image_height*2);      
	  interlaced = 1;
	}
      else
	{
	  mjpeg_info("(non_interlaced input assumed)\n");      
	  interlaced = 0;
	}
    }

  if (interlaced)
    {
      switch(interlaced_top_first)
	{
	case -1:
	  mjpeg_error_exit1("You have not specified the order of the frames (use the -T option)\n");      
	  break;
	case 0:
	  mjpeg_info("Interlaced frame order: Bottom frame first.\n");      
	  break;
	case 1:
	  mjpeg_info("Interlaced frame order: Top frame first.\n");      
	  break;
	default:
	  mjpeg_error_exit1("Invalid Top/Bottom paramter (only 0 or 1 allowed for -T)\n");      
	}
    }

  return 1;
}

int generate_YUV4MPEG()
{
  int newpicsavail = 1;

  uint32_t vid_index = begin;
  uint32_t jpegsize;
  unsigned char jpegname[FILENAME_MAX];
  FILE *jpegfile;
  static unsigned char ybuffer[MAXPIXELS]; // the YUV-buffers for the decoded JPEG content
  static unsigned char ubuffer[MAXPIXELS];
  static unsigned char vbuffer[MAXPIXELS];
  static unsigned char jpegdata[MAXPIXELS]; // that ought to be enough
  unsigned char *yuvbufptr[3];

  mjpeg_info("Now generating YUV4MPEG stream.\n");

  yuvbufptr[0] = ybuffer; 
  yuvbufptr[1] = ubuffer; 
  yuvbufptr[2] = vbuffer; 

  if (!interlaced)
    yuv_write_header(STDOUT_FILENO, image_width, image_height, fpscode);
  else
    yuv_write_header(STDOUT_FILENO, image_width, image_height * 2, fpscode);
  
  while (newpicsavail)
    {
      sprintf(jpegname, jpegformatstr, vid_index);
      jpegfile = fopen(jpegname, "r");

      if (jpegfile == NULL)
	{ 
	  mjpeg_info("While opening %s:\n", jpegname);
	  
	  if (numframes != -1)
	    {
	      if (verbose) perror("Warning: Read from jpeg file failed (non-critical)");
	      mjpeg_info("(Rewriting latest frame instead)\n");

	      mjpeg_debug("Old frame recycled, now writing to output stream.\n");
	      yuv_write_frame (STDOUT_FILENO, yuvbufptr, image_width, image_height);
	    }
	  else
	    {
	      mjpeg_info("Last frame encountered (%s invalid). Stop.\n", jpegname);
	      newpicsavail = 0;
	    }
	} 
      else
	{
	  mjpeg_debug("Preparing frame\n");
	  
	  jpegsize = fread(jpegdata, sizeof(unsigned char), MAXPIXELS, jpegfile); 
	  fclose(jpegfile);
	  
	  /* decode_jpeg_raw:s parameters from 20010826
	   * jpeg_data:       buffer with input / output jpeg
	   * len:             Length of jpeg buffer
	   * itype:           0: Not interlaced
	   *                  1: Interlaced, Top field first
	   *                  2: Interlaced, Bottom field first
	   * ctype            Chroma format for decompression.
	   *                  Currently always 420 and hence ignored.
	   * raw0             buffer with input / output raw Y channel
	   * raw1             buffer with input / output raw U/Cb channel
	   * raw2             buffer with input / output raw V/Cr channel
	   * width            width of Y channel (width of U/V is width/2)
	   * height           height of Y channel (height of U/V is height/2)
	   */
	  if (interlaced == 0)
	    {
	      mjpeg_info("Processing non-interlaced %s, size %d.\n", jpegname, jpegsize);
	      decode_jpeg_raw (jpegdata, jpegsize,
			       0, 420, image_width, image_height,
			       ybuffer, ubuffer, vbuffer);
	    }
	  else
	    {
	      if (interlaced_top_first)
		{
		  mjpeg_info("Processing interlaced, top-first %s, size %d.\n", jpegname, jpegsize);
		  decode_jpeg_raw (jpegdata, jpegsize,
				   1, 420, image_width, image_height,
				   ybuffer, ubuffer, vbuffer);
		}
	      else
		{
		  mjpeg_info("Processing interlaced, bottom-first %s, size %d.\n", jpegname, jpegsize);
		  decode_jpeg_raw (jpegdata, jpegsize,
				   2, 420, image_width, image_height,
				   ybuffer, ubuffer, vbuffer);
		}
	    }

	  mjpeg_debug("Frame decoded, now writing to output stream.\n");
	  yuv_write_frame (STDOUT_FILENO, yuvbufptr, image_width, image_height);
	}

      vid_index++; /* reached the end of the file, or the desired number of frames ? */
      if ((numframes != -1 && vid_index == numframes + begin))
	newpicsavail = 0;
    }

  return 1;
}

/* main
 * in: argc, argv:  Classic commandline parameters. 
 * returns: int: 0: success, !0: !success :-)
 */
int main(int argc, char ** argv)
{ 
  parse_commandline(argc, argv);

  mjpeg_default_handler_verbosity(verbose);

  mjpeg_info("Parsing & checking input files.\n");
  if (init_parse_files() == 0)
  { mjpeg_error_exit1("* Error processing the JPEG input.\n"); }

  mjpeg_debug("Now generating YUV4MPEG output stream.\n");
  if (generate_YUV4MPEG() == 0)
  { 
    mjpeg_error_exit1("* Error processing the input files.\n");
  }

  return 0;
}










