/*
 * ppmtoy4m.c:  Generate a YUV4MPEG2 stream from one or more PPM images
 *
 *              Converts R'G'B' to ITU-Rec.601 Y'CbCr colorspace and
 *               performs 4:2:0 chroma subsampling.
 *
 *
 *  Copyright (C) 2001 Matthew J. Marjanovic <maddog@mir.com>
 *
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
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>

#include <yuv4mpeg.h>
#include <mpegconsts.h>
#include "subsample.h"
#include "colorspace.h"



/* command-line parameters */
typedef struct _cl_info {
  y4m_ratio_t aspect;    
  y4m_ratio_t framerate; 
  int interlace;         
  int interleave;
  int offset;
  int framecount;
  int repeatlast;
  int ss_mode;
  int verbosity;
  int fdin;
} cl_info_t;


/* PPM image information */
typedef struct _ppm_info {
  int width;
  int height;
} ppm_info_t;



static
void usage(const char *progname)
{
  int i;
  fprintf(stdout, "\n");
  fprintf(stdout, "usage:  %s [options] [ppm-file]\n", progname);
  fprintf(stdout, "\n");
  fprintf(stdout, "Reads RAW PPM image(s), and produces YUV4MPEG2 stream on stdout.\n");
  fprintf(stdout, "Converts computer graphics R'G'B' colorspace to digital video Y'CbCr,\n");
  fprintf(stdout, " and performs chroma subsampling.\n");
  fprintf(stdout, "\n");
  fprintf(stdout, "If 'ppm-file' is not specified, reads from stdin.\n");
  fprintf(stdout, "\n");
  fprintf(stdout, " options:  (defaults specified in [])\n");
  fprintf(stdout, "\n");
  fprintf(stdout, "  -o n     frame offset (skip n input frames) [0]\n");
  fprintf(stdout, "  -n n     frame count (output n frames; 0 == all of them) [0]\n");
  fprintf(stdout, "  -F n:d   framerate [30000:1001 = NTSC]\n");
  fprintf(stdout, "  -A n:d   display aspect ratio [4:3]\n");
  fprintf(stdout, "  -I x     interlacing [p]\n");
  fprintf(stdout, "             p = none/progressive\n");
  fprintf(stdout, "             t = top-field-first\n");
  fprintf(stdout, "             b = bottom-field-first\n");
  fprintf(stdout, "  -L       treat PPM images as 2-field interleaved\n");
  fprintf(stdout, "  -r       repeat last input frame\n");
  fprintf(stdout, "  -S mode  chroma subsampling mode [%s]\n",
	  ssm_id[SSM_420_JPEG]);
  for (i = 1; i < SSM_COUNT; i++)
  fprintf(stdout, "            '%s' -> %s\n", ssm_id[i], ssm_description[i]);
  /*  fprintf(stdout, "  -R type  subsampling filter type\n");*/
  fprintf(stdout, "  -v n     verbosity (0,1,2) [1]\n");
}



static
void parse_args(cl_info_t *cl, int argc, char **argv)
{
  int i;
  int c;

  cl->offset = 0;
  cl->framecount = 0;
  cl->aspect = y4m_aspect_4_3;
  cl->interlace = Y4M_ILACE_NONE;
  cl->framerate = y4m_fps_NTSC;
  cl->interleave = 0;
  cl->repeatlast = 0;
  cl->ss_mode = SSM_420_JPEG;
  cl->verbosity = 1;
  cl->fdin = 0; /* default to stdin */

  while ((c = getopt(argc, argv, "A:F:I:Lo:n:rS:v:h")) != -1) {
    switch (c) {
    case 'A':
      if (y4m_parse_ratio(&(cl->aspect), optarg) != Y4M_OK) {
	mjpeg_error("Could not parse ratio:  '%s'\n", optarg);
	goto ERROR_EXIT;
      }
      break;
    case 'F':
      if (y4m_parse_ratio(&(cl->framerate), optarg) != Y4M_OK) {
	mjpeg_error("Could not parse ratio:  '%s'\n", optarg);
	goto ERROR_EXIT;
      }
      break;
    case 'I':
      switch (optarg[0]) {
      case 'p':  cl->interlace = Y4M_ILACE_NONE;  break;
      case 't':  cl->interlace = Y4M_ILACE_TOP_FIRST;  break;
      case 'b':  cl->interlace = Y4M_ILACE_BOTTOM_FIRST;  break;
      default:
	mjpeg_error("Unknown value for interlace: '%c'\n", optarg[0]);
	goto ERROR_EXIT;
	break;
      }
      break;
    case 'L':
      cl->interleave = 1;
      break;
    case 'o':
      if ((cl->offset = atoi(optarg)) < 0)
	mjpeg_error_exit1("Offset must be >= 0:  '%s'\n", optarg);
      break;
    case 'n':
      if ((cl->framecount = atoi(optarg)) < 0)
	mjpeg_error_exit1("Frame count must be >= 0:  '%s'\n", optarg);
      break;
    case 'r':
      cl->repeatlast = 1;
      break;
    case 'S':
      for (i = 0; i < SSM_COUNT; i++) {
	if (!(strcmp(optarg, ssm_id[i]))) break;
      }
      if (i < SSM_COUNT)
	cl->ss_mode = i;
      else {
	mjpeg_error("Unknown subsampling mode option:  %s\n", optarg);
	goto ERROR_EXIT;
      }
      break;
    case 'v':
      cl->verbosity = atoi(optarg);
      if ((cl->verbosity < 0) || (cl->verbosity > 2))
	mjpeg_error("Verbosity must be 0, 1, or 2:  '%s'\n", optarg);
      break;
    case 'h':
      usage(argv[0]);
      exit(0);
      break;
    case '?':
    default:
      goto ERROR_EXIT;
      break;
    }
  }
  /* optional remaining argument is a filename */
  if (optind == (argc - 1)) {
    if ((cl->fdin = open(argv[optind], O_RDONLY)) == -1)
      mjpeg_error_exit1("Failed to open '%s':  %s\n",
			argv[optind], strerror(errno));
  } else if (optind != argc) 
    goto ERROR_EXIT;


  mjpeg_default_handler_verbosity(cl->verbosity);

  mjpeg_info("Command-line Parameters:\n");
  mjpeg_info("             framerate:  %d:%d\n",
	     cl->framerate.n, cl->framerate.d);
  mjpeg_info("  display aspect ratio:  %d:%d\n",
	     cl->aspect.n, cl->aspect.d);
  mjpeg_info("             interlace:  %s%s\n",
	     mpeg_interlace_code_definition(cl->interlace),
	     (cl->interlace == Y4M_ILACE_NONE) ? "" :
	     (cl->interleave) ? " (interleaved PPM input)" :
	     " (field-sequential PPM input)");
  mjpeg_info("        starting frame:  %d\n", cl->offset);
  if (cl->framecount == 0)
    mjpeg_info("           # of frames:  all%s\n",
	       (cl->repeatlast) ? ", repeat last frame forever" :
	       ", until input exhausted");
  else
    mjpeg_info("           # of frames:  %d%s\n",
	       cl->framecount,
	       (cl->repeatlast) ? ", repeat last frame until done" :
	       ", or until input exhausted");
  mjpeg_info("    chroma subsampling:  %s\n", ssm_description[cl->ss_mode]);

  /* DONE! */
  return;

 ERROR_EXIT:
  mjpeg_error("For usage hints, use option '-h'.  Please take a hint.\n");
  exit(1);

}



/*
 * returns:  0 - success, got header
 *           1 - EOF, no new frame
 *          -1 - failure
 */

static
int read_ppm_header(int fd, int *width, int *height)
{
  char s[6];
  int n;
  int maxval = 0;
  
  *width = 0;
  *height = 0;

  /* look for "P6" */
  n = y4m_read(fd, s, 3);
  if (n > 0) 
    return 1;  /* EOF */

  if ((n < 0) || (strncmp(s, "P6", 2)))
    mjpeg_error_exit1("Bad Raw PPM magic!\n");

  /* skip whitespace */
  while (((n = read(fd, s, 1)) == 1) && (isspace(s[0]))) {}
  if (n <= 0) return -1;

  /* read width */
  do {
    if (!isdigit(s[0]))
      mjpeg_error_exit1("PPM read error:  bad char\n");
    *width = (*width * 10) + (s[0] - '0');
  } while (((n = read(fd, s, 1)) == 1) && (!isspace(s[0])));

  /* skip whitespace */
  while (((n = read(fd, s, 1)) == 1) && (isspace(s[0]))) {}
  if (n <= 0) return -1;

  /* read height */
  do {
    if (!isdigit(s[0]))
      mjpeg_error_exit1("PPM read error:  bad char\n");
    *height = (*height * 10) + (s[0] - '0');
  } while (((n = read(fd, s, 1)) == 1) && (!isspace(s[0])));

  /* skip whitespace */
  while (((n = read(fd, s, 1)) == 1) && (isspace(s[0]))) {}
  if (n <= 0) return -1;

  /* read maxval */
  do {
    if (!isdigit(s[0]))
      mjpeg_error_exit1("PPM read error:  bad char\n");
    maxval = (maxval * 10) + (s[0] - '0');
  } while (((n = read(fd, s, 1)) == 1) && (!isspace(s[0])));

  if (maxval != 255)
    mjpeg_error_exit1("Expecting maxval == 255, not %d!\n", maxval);

  return 0;
}



static
void alloc_buffers(unsigned char *buffers[], int width, int height)
{
  mjpeg_debug("Alloc'ing buffers\n");
  buffers[0] = malloc(width * height * 2 * sizeof(buffers[0][0]));
  buffers[1] = malloc(width * height * 2 * sizeof(buffers[1][0]));
  buffers[2] = malloc(width * height * 2 * sizeof(buffers[2][0]));
}






static
void read_ppm_into_two_buffers(int fd,
			       unsigned char *buffers[], 
			       unsigned char *buffers2[], 
			       unsigned char *rowbuffer,
			       int width, int height)
{
  int x, y;
  unsigned char *pixels;
  unsigned char *R = buffers[0];
  unsigned char *G = buffers[1];
  unsigned char *B = buffers[2];
  unsigned char *R2 = buffers2[0];
  unsigned char *G2 = buffers2[1];
  unsigned char *B2 = buffers2[2];

  mjpeg_debug("read into two buffers, %dx%d\n", width, height);
  height /= 2;
  for (y = 0; y < height; y++) {
    pixels = rowbuffer;
    if (y4m_read(fd, pixels, width * 3))
      mjpeg_error_exit1("read error A  y=%d\n", y);
    for (x = 0; x < width; x++) {
      *(R++) = *(pixels++);
      *(G++) = *(pixels++);
      *(B++) = *(pixels++);
    }
    pixels = rowbuffer;
    if (y4m_read(fd, pixels, width * 3))
      mjpeg_error_exit1("read error B  y=%d\n", y);
    for (x = 0; x < width; x++) {
      *(R2++) = *(pixels++);
      *(G2++) = *(pixels++);
      *(B2++) = *(pixels++);
    }
  }
}



static
void read_ppm_into_one_buffer(int fd,
			      unsigned char *buffers[],
			      unsigned char *rowbuffer,
			      int width, int height) 
{
  int x, y;
  unsigned char *pixels;
  unsigned char *R = buffers[0];
  unsigned char *G = buffers[1];
  unsigned char *B = buffers[2];

  for (y = 0; y < height; y++) {
    pixels = rowbuffer;
    y4m_read(fd, pixels, width * 3);
    for (x = 0; x < width; x++) {
      *(R++) = *(pixels++);
      *(G++) = *(pixels++);
      *(B++) = *(pixels++);
    }
  }
}


/* 
 * read one whole frame
 * if field-sequential interlaced, this requires reading two PPM images
 *
 * if interlaced, fields are written into separate buffers
 *
 * ppm.width/height refer to image dimensions
 */

static
int read_ppm_frame(int fd, ppm_info_t *ppm,
		   unsigned char *buffers[], unsigned char *buffers2[],
		   int ilace, int ileave)
{
  int width, height;
  static unsigned char *rowbuffer = NULL;
  int err;

  err = read_ppm_header(fd, &width, &height);
  if (err > 0) return 1;  /* EOF */
  if (err < 0) return -1; /* error */
  mjpeg_debug("Got PPM header:  %dx%d\n", width, height);

  if (ppm->width == 0) {
    /* first time */
    mjpeg_debug("Initializing PPM read_frame\n");
    ppm->width = width;
    ppm->height = height;
    rowbuffer = malloc(width * 3 * sizeof(rowbuffer[0]));
  } else {
    /* make sure everything matches */
    if ( (ppm->width != width) ||
	 (ppm->height != height) )
      mjpeg_error_exit1("One of these frames is not like the others!\n");
  }
  if (buffers[0] == NULL) 
    alloc_buffers(buffers, width, height);
  if ((buffers2[0] == NULL) && (ilace != Y4M_ILACE_NONE))
    alloc_buffers(buffers2, width, height);

  mjpeg_debug("Reading rows\n");

  if ((ilace != Y4M_ILACE_NONE) && (ileave)) {
    /* Interlaced and Interleaved:
       --> read image and deinterleave fields at same time */
    if (ilace == Y4M_ILACE_TOP_FIRST) {
      /* 1st buff arg == top field == temporally first == "buffers" */
      read_ppm_into_two_buffers(fd, buffers, buffers2,
				rowbuffer, width, height);
    } else {
      /* bottom-field-first */
      /* 1st buff art == top field == temporally second == "buffers2" */
      read_ppm_into_two_buffers(fd, buffers2, buffers,
				rowbuffer, width, height);
    }      
  } else if ((ilace == Y4M_ILACE_NONE) || (!ileave)) {
    /* Not Interlaced, or Not Interleaved:
       --> read image into first buffer... */
    read_ppm_into_one_buffer(fd, buffers, rowbuffer, width, height);
    if ((ilace != Y4M_ILACE_NONE) && (!ileave)) {
      /* ...Actually Interlaced:
	 --> read the second image/field into second buffer */
      err = read_ppm_header(fd, &width, &height);
      if (err > 0) return 1;  /* EOF */
      if (err < 0) return -1; /* error */
      mjpeg_debug("Got PPM header:  %dx%d\n", width, height);
      
      /* make sure everything matches */
      if ( (ppm->width != width) ||
	   (ppm->height != height) )
	mjpeg_error_exit1("One of these frames is not like the others!\n");
      read_ppm_into_one_buffer(fd, buffers2, rowbuffer, width, height);
    }
  }
  return 0;
}



static
void setup_output_stream(int fdout, cl_info_t *cl,
			 y4m_stream_info_t *sinfo, ppm_info_t *ppm,
			 int *field_height) 
{
  y4m_si_set_width(sinfo, ppm->width);
  if (cl->interlace == Y4M_ILACE_NONE) {
    y4m_si_set_height(sinfo, ppm->height);
    *field_height = ppm->height;
  } else if (cl->interleave) {
    y4m_si_set_height(sinfo, ppm->height);
    *field_height = ppm->height / 2;
  } else {
    y4m_si_set_height(sinfo, ppm->height * 2);
    *field_height = ppm->height;
  }
  y4m_si_set_aspectratio(sinfo, cl->aspect);
  y4m_si_set_interlace(sinfo, cl->interlace);
  y4m_si_set_framerate(sinfo, cl->framerate);

  y4m_write_stream_header(fdout, sinfo);

  mjpeg_info("Output Stream parameters:\n");
  y4m_log_stream_info(LOG_INFO, "  ", sinfo);
}




int main(int argc, char **argv)
{
  cl_info_t cl;
  y4m_stream_info_t sinfo;
  y4m_frame_info_t finfo;
  unsigned char *buffers[3];  /* R'G'B' or Y'CbCr */
  unsigned char *buffers2[3]; /* R'G'B' or Y'CbCr */
  ppm_info_t ppm;
  int field_height;

  int fdout = 1;
  int err, i, count, repeating_last;

  y4m_init_stream_info(&sinfo);
  y4m_init_frame_info(&finfo);

  parse_args(&cl, argc, argv);

  ppm.width = 0;
  ppm.height = 0;
  for (i = 0; i < 3; i++) {
    buffers[i] = NULL;
    buffers2[i] = NULL;
  }

  /* Read first PPM frame/field-pair, to get dimensions */
  if (read_ppm_frame(cl.fdin, &ppm, buffers, buffers2, 
		     cl.interlace, cl.interleave))
    mjpeg_error_exit1("Failed to read first frame.\n");

  /* Setup streaminfo and write output header */
  setup_output_stream(fdout, &cl, &sinfo, &ppm, &field_height);

  /* Loop 'framecount' times, or possibly forever... */
  for (count = 0, repeating_last = 0;
       (count < (cl.offset + cl.framecount)) || (cl.framecount == 0);
       count++) {

    if (repeating_last) goto WRITE_FRAME;

    /* Read PPM frame/field */
    /* ...but skip reading very first frame, already read prior to loop */
    if (count > 0) {
      err = read_ppm_frame(cl.fdin, &ppm, buffers, buffers2, 
			   cl.interlace, cl.interleave);
      if ((err == 1) && (cl.repeatlast)) {      /* EOF */
	repeating_last = 1;
	goto WRITE_FRAME;
      } else if (err)
	mjpeg_error_exit1("Error reading ppm frame\n");
    }
    
    /* ...skip transforms if we are just going to skip this frame anyway.
       BUT, if 'cl.repeatlast' is on, we must process/buffer every frame,
       because we don't know when we will see the last one. */
    if ((count >= cl.offset) || (cl.repeatlast)) {
      /* Transform colorspace, then subsample (in place) */
      convert_RGB_to_YCbCr(buffers, ppm.width * field_height);
      chroma_subsample(cl.ss_mode, buffers, ppm.width, field_height);
      if (cl.interlace != Y4M_ILACE_NONE) {
	convert_RGB_to_YCbCr(buffers2, ppm.width * field_height);
	chroma_subsample(cl.ss_mode, buffers2, ppm.width, field_height);
      }
    }

  WRITE_FRAME:
    /* Write converted frame to output */
    if (count >= cl.offset) {
      switch (cl.interlace) {
      case Y4M_ILACE_NONE:
	y4m_write_frame(fdout, &sinfo, &finfo, buffers);
	break;
      case Y4M_ILACE_TOP_FIRST:
	y4m_write_fields(fdout, &sinfo, &finfo, buffers, buffers2);
	break;
      case Y4M_ILACE_BOTTOM_FIRST:
	y4m_write_fields(fdout, &sinfo, &finfo, buffers2, buffers);
	break;
      default:
	mjpeg_error_exit1("Unknown ilace type!   %d\n", cl.interlace);
	break;
      }
    }
  } 


  for (i = 0; i < 3; i++) {
    free(buffers[i]);
    free(buffers2[i]);
  }
  y4m_fini_stream_info(&sinfo);
  y4m_fini_frame_info(&finfo);

  mjpeg_debug("Done.\n");
  return 0;
}
