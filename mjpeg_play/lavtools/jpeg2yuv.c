/*
jpeg2yuv
========


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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string.h>
//#include <glib.h>
#include <jpeglib.h>
#include <sys/types.h>

#define MAXPIXELS (1280*1024)  /* Maximum size of final image */

static unsigned char outbuffer[MAXPIXELS*3];
static unsigned char outbuffer2[MAXPIXELS*3];
static unsigned char yuvbuffer[MAXPIXELS*3];

static gchar *jpegformatstr   = NULL;

static float framerate = -1.0;
static gint32 begin = 0; /* the video frame start */
static gint32 numframes = -1; /* -1 means: take all frames */

static int verbose = 0;
static int debug = 0;
static int interlaced = 0; /* tells if the YUV4MPEG stream shall be interlaced */

static struct jpeg_decompress_struct dinfo;
/* static struct jpeg_compress_struct newinfo; */
static struct jpeg_error_mgr jerr;

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
    
  fprintf(stderr,
	  "%s pipes a stream of JPEG files to stdout,\n"
	  "making the direct encoding of MPEG files possible under mpeg2enc.\n"
	  "Any JPEG format supported by libjpeg can be read.\n"
	  "stdout will be filled with the YUV4MPEG movie data stream,\n"
	  "so be prepared to pipe it on to mpeg2enc or to write it into a file.\n"
	  "\n"
	  "usage: %s [ options ]\n"
	  "\n"
	  "where options are ([] shows the defaults):\n"
	  "  -v          verbose operation	       \n"
	  "  -d          debug information	       \n"
	  "  -b framenr. start at given frame nr.               [0]\n"
	  "  -n numframes processes the given nr. of frames     [all]\n"
	  "  -j {1}%%{2}d{3} Read JPEG frames with the name components as follows:\n"
	  "               {1} JPEG filename prefix (e g rendered_ )\n"
	  "               {2} Counting placeholder (like in C, printf, eg 06 ))\n"
	  "               {1} JPEG filename postfix (e g .jpeg)\n"
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
   if (-1 == (c = getopt(argc, argv, "dIhvb:j:n:f:")))
     break;
   switch (c) {
   case 'v':
     verbose = 1;
     break;
   case 'd':
     debug = 1;
     break;
   case 'j':
     jpegformatstr = optarg;
     inmovtarname = NULL;
     break;
   case 'b':
     begin = atol(optarg);
     break;
   case 'n':
     numframes = atol(optarg);
     break;
   case 'f':
     framerate = atol(optarg);
     break;
   case 'I':
     interlaced = 1;
     break;
   case 'h':
   default:
     usage(argv[0]);
     exit(1);
   }
 }
 if (jpegformatstr == NULL)
   { printf("%s needs an input filename to proceed. (-j option)\n\n", argv[0]); 
     usage(argv[0]); 
     exit(1);
   }

 if (framerate == -1.0)
   { printf("%s needs the framerate of the forthcoming YUV4MPEG movie. (-f option)\n\n", argv[0]); 
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
  gchar jpegname[255];
  FILE *jpegfile;

  sprintf(jpegname, 254, jpegformatstr, begin);
  if (debug) fprintf(stderr, "Analyzing %s to get the right pic params\n", jpegname);
  jpegfile = fopen(jpegname, "r");

  if (jpegfile == NULL)
    {
      fprintf(stderr, "While opening: \"%s\"\n", jpegname);
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
	{
	  printf("Output components of JPEG image = %d, must be 3\n",dinfo.output_components);
	  exit(1);
	}
      
      fprintf(stderr, "Image dimensions are %dx%d, framerate = %f frames/s\n",
	      outmovtar->mov_width, outmovtar->mov_height, framerate);
      jpeg_destroy_decompress(&dinfo);
      fclose(jpegfile);
    }

  if (verbose) printf("movie frame rate is: %f frames/s\n", framerate);

  return 1;
}

int unify_movtar()
{
  int newpicsavail = 1;
  gint64 read_bytes, read_video_bytes = 0;
  gint64 bytes_to_read;

  guchar videobuffer[300000]; /* ought to be enough */
  gint32 vid_index = begin;
  gchar jpegname[255];
  FILE *jpegfile;

  if (verbose) fprintf(stderr, "Now starting unify process.\n");

  if (debug) printf("input wavsize is %lld\n", wavsize);

  while (newpicsavail)
    {
      sprintf(jpegname, jpegformatstr, vid_index);
      jpegfile = fopen(jpegname, "r");

      if (jpegfile == NULL)
	{ 
	  if (verbose) printf("While opening %s:\n", jpegname);
	  
	  if (!recompress)
	    {
	      if (verbose) perror("Warning: Read from jpeg file failed (non-critical)");
	      if (verbose) printf("(Rewriting latest frame, size %lld, instead)\n", read_video_bytes);
	    }
	  else
	    {
	      if (verbose) printf("Last frame encountered (%s invalid). Stop.\n", jpegname);
	      newpicsavail = 0;
	    }
	} 
      else
	{
	  if (!recompress)
	    {
	      read_video_bytes = fread(videobuffer, 1, 300000, jpegfile);
	      if (verbose) printf("Got %s, size %lld\n", jpegname, read_video_bytes); 
	    }
	  else
	    {
	      if (debug) printf("Preparing frame\n");

	      movtar_write_frame_init(outmovtar);
	      if (!rtjpeg)
		{
		  if (debug) printf("Recompressing frame in JPEG format\n");
		  jpeg_recompress(jpegfile, movtar_get_fd(outmovtar));
		}
	      else
		{
		  if (debug) printf("Recompressing frame in RTJPEG format\n");
		  rtjpeg_recompress(jpegfile, movtar_get_fd(outmovtar));
		}
	      if (debug) printf("Finishing frame\n");
	      movtar_write_frame_end(outmovtar);
	    }
	  fclose(jpegfile);
	}

      if (!recompress)
	{
	  /* The video data should now be in videobuffer, read_bytes contains the read bytes */      
	  if (read_video_bytes != 0)
	    movtar_write_frame(outmovtar, videobuffer, read_video_bytes); /* write it out */
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

  if (debug) fprintf(stderr, "Parsing & checking input files.\n");
  if (init_parse_files() == 0)
  { fprintf(stderr, "* Error processing the JPEG input.\n"); exit(1); }

  if (debug) fprintf("Now generating YUV4MPEG output stream.\n");
  if (unify_movtar() == 0)
  { fprintf(stderr, "* Error processing the input files.\n");
    exit(1);
  }

  return 0;
}










