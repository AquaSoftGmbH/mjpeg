/*
 * y4mcolorbars.c:  Generate real, standard colorbars in POG form 
 *                   (where "POG" == "YUV4MPEG2 stream").
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

#include <yuv4mpeg.h>
#include <mpegconsts.h>

#include "subsample.h"
#include "colorspace.h"


typedef struct _cl_info {
  y4m_ratio_t aspect;
  int interlace;
  y4m_ratio_t framerate;
  int framecount;
  int ss_mode;
  int width;
  int height;
  int verbosity;
} cl_info_t;



static
void usage(const char *progname)
{
  int i;

  fprintf(stdout, "usage: %s [options]\n", progname);
  fprintf(stdout, "\n");
  fprintf(stdout, "Reads RAW PPM image(s) from stdin, and produces YUV4MPEG2 stream on stdout.\n");
  fprintf(stdout, "Converts computer graphics R'G'B' colorspace to digital video Y'CbCr.\n");
  fprintf(stdout, "\n");
  fprintf(stdout, "Options:  (defaults specified in [])\n");
  fprintf(stdout, "\n");
  fprintf(stdout, "  -W w     frame width [720]\n");
  fprintf(stdout, "  -H h     frame height [480]\n");
  fprintf(stdout, "  -F n:d   framerate (as ratio) \n");
  fprintf(stdout, "  -A w:h   pixel aspect ratio [10:11]\n");
  fprintf(stdout, "  -I x     interlacing [p]\n");
  fprintf(stdout, "             p = none/progressive\n");
  fprintf(stdout, "             t = top-field-first\n");
  fprintf(stdout, "             b = bottom-field-first\n");
  fprintf(stdout, "\n");
  fprintf(stdout, "  -n n     frame count (output n frames) [1]\n");
  fprintf(stdout, "\n");
  fprintf(stdout, "  -S mode  chroma subsampling mode [%s]\n",
	  ssm_id[SSM_420_JPEG]);
  for (i = 1; i < SSM_COUNT; i++)
  fprintf(stdout, "            '%s' -> %s\n", ssm_id[i], ssm_description[i]);
  fprintf(stdout, "\n");
  fprintf(stdout, "  -v n     verbosity (0,1,2) [1]\n");
}



static
void parse_args(cl_info_t *cl, int argc, char **argv)
{
  int i;
  int c;

  cl->interlace = Y4M_ILACE_NONE;
  cl->framerate = y4m_fps_NTSC;
  cl->framecount = 1;
  cl->ss_mode = SSM_420_JPEG;
  cl->width = 720;
  cl->height = 480;
  cl->aspect = y4m_sar_NTSC_CCIR601;

  while ((c = getopt(argc, argv, "A:F:I:W:H:n:S:h")) != -1) {
    switch (c) {
    case 'W':
      if ((cl->width = atoi(optarg)) <= 0) {
	mjpeg_error("Invalid width:  '%s'\n", optarg);
	goto ERROR_EXIT;
      }
      break;
    case 'H':
      if ((cl->height = atoi(optarg)) <= 0) {
	mjpeg_error("Invalid height:  '%s'\n", optarg);
	goto ERROR_EXIT;
      }
      break;
    case 'A':
      if (y4m_parse_ratio(&(cl->aspect), optarg) != Y4M_OK) {
	mjpeg_error("Could not parse ratio:  '%s'\n", optarg);
	goto ERROR_EXIT;
      }
      break;
    case 'F':
      if (y4m_parse_ratio(&(cl->framerate), optarg) != Y4M_OK) {
	mjpeg_error("Could not parse framerate:  '%s'\n", optarg);
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
    case 'n':
      if ((cl->framecount = atoi(optarg)) <= 0) {
	mjpeg_error("Invalid frame count:  '%s'\n", optarg);
	goto ERROR_EXIT;
      }
      break;
    case 'S':
      for (i = 0; i < SSM_COUNT; i++) {
	if (!(strcmp(optarg, ssm_id[i]))) break;
      }
      if (i < SSM_COUNT)
	cl->ss_mode = i;
      else {
	mjpeg_error("Unknown subsampling mode option:  %s\n\n", optarg);
	goto ERROR_EXIT;
      }
      break;
    case 'v':
      cl->verbosity = atoi(optarg);
      break;
    case 'h':
      usage(argv[0]);
      exit(0);
      break;
    case '?':
    default:
      mjpeg_error("Unknown option:  '-%c'\n", optopt);
      goto ERROR_EXIT;
      break;
    }
  }

  mjpeg_default_handler_verbosity(cl->verbosity);

  mjpeg_info("Colorbar Command-line Parameters:\n");
  mjpeg_info("            frame size:  %dx%d\n", cl->width, cl->height);
  mjpeg_info("             framerate:  %d:%d\n",
	     cl->framerate.n, cl->framerate.d);
  mjpeg_info("    pixel aspect ratio:  %d:%d\n",
	     cl->aspect.n, cl->aspect.d);
  mjpeg_info("             interlace:  %s\n",
	     mpeg_interlace_code_definition(cl->interlace));
  mjpeg_info("           # of frames:  %d\n", cl->framecount);
  mjpeg_info("    chroma subsampling:  %s\n", ssm_description[cl->ss_mode]);
  return;

 ERROR_EXIT:
  mjpeg_error("For usage hints, use option '-h':  '%s -h'\n", argv[0]);
  exit(1);

}




/*
 * Color Bars:
 *
 *     top 2/3:  100% white, followed by 75% binary combinations
 *                of R', G', and B' with decreasing luma
 *
 * middle 1/12:  reverse order of above, but with 75% white and
 *                alternating black
 *
 *  bottom 1/4:  -I, 100% white, +Q, black, PLUGE, black,
 *                where PLUGE is (black - 4 IRE), black, (black + 4 IRE)
 *
 */

/* 100% white   */
/*  75% yellow  */
/*  75% cyan    */
/*  75% green   */
/*  75% magenta */
/*  75% red     */
/*  75% blue    */
static unsigned char rainbowRGB[][7] = {
  { 255, 191,   0,   0, 191, 191,   0 },
  { 255, 191, 191, 191,   0,   0,   0 },
  { 255,   0, 191,   0, 191,   0, 191 }
};
static unsigned char *rainbow[3] = {
  rainbowRGB[0], rainbowRGB[1], rainbowRGB[2]
};


/*  75% blue    */
/*      black   */
/*  75% magenta */
/*      black   */
/*  75% cyan    */
/*      black   */
/*  75% white   */
static unsigned char wobnairRGB[][7] = {
  {   0,   0, 191,   0,   0,   0, 191 },
  {   0,   0,   0,   0, 191,   0, 191 },
  { 191,   0, 191,   0, 191,   0, 191 }
};
static unsigned char *wobnair[3] = {
  wobnairRGB[0], wobnairRGB[1], wobnairRGB[2]
};


static
void create_bars(unsigned char *ycbcr[], int width, int height)
{
  int i, x, y, w;
  int bnb_start;
  int pluge_start;
  int stripe_width;
  int pl_width;
  unsigned char *lineY;
  unsigned char *lineCb;
  unsigned char *lineCr;

  unsigned char *Y = ycbcr[0];
  unsigned char *Cb = ycbcr[1];
  unsigned char *Cr = ycbcr[2];

  convert_RGB_to_YCbCr(rainbow, 7);
  convert_RGB_to_YCbCr(wobnair, 7);

  bnb_start = height * 2 / 3;
  pluge_start = height * 3 / 4;
  stripe_width = (width + 6) / 7;
  lineY = malloc(width * sizeof(lineY[0]));
  lineCb = malloc(width * sizeof(lineCb[0]));
  lineCr = malloc(width * sizeof(lineCr[0]));

  /* Top:  Rainbow */
  for (x = 0, i = 0, w = 0; x < width; i++) {
    for (w = 0; w < stripe_width; w++, x++) {
      lineY[x] = rainbow[0][i];
      lineCb[x] = rainbow[1][i];
      lineCr[x] = rainbow[2][i];
    }
  }
  for (y = 0; y < bnb_start; y++) {
    memcpy(Y, lineY, width);
    memcpy(Cb, lineCb, width);
    memcpy(Cr, lineCr, width);
    Y += width;
    Cb += width;
    Cr += width;
  }
  
  /* Middle:  Wobnair */
  for (x = 0, i = 0, w = 0; x < width; i++) {
    for (w = 0; w < stripe_width; w++, x++) {
      lineY[x] = wobnair[0][i];
      lineCb[x] = wobnair[1][i];
      lineCr[x] = wobnair[2][i];
    }
  }
  for (; y < pluge_start; y++) {
    memcpy(Y, lineY, width);
    memcpy(Cb, lineCb, width);
    memcpy(Cr, lineCr, width);
    Y += width;
    Cb += width;
    Cr += width;
  }


  /* Bottom:  PLUGE */
  pl_width = 5 * stripe_width / 4;
  /* -I -- well, we use -Cb here */
  for (x = 0; x < pl_width; x++) {
    lineY[x] =    0;
    lineCb[x] =  16;
    lineCr[x] = 128;
  }
  /* white */
  for (; x < (2 * pl_width); x++) {
    lineY[x] =  235;
    lineCb[x] = 128;
    lineCr[x] = 128;
  }
  /* +Q -- well, we use +Cr here */
  for (; x < (3 * pl_width); x++) {
    lineY[x] =    0;
    lineCb[x] = 128;
    lineCr[x] = 240;
  }
  /* black */
  for (; x < (5 * stripe_width); x++) {
    lineY[x] =   16;
    lineCb[x] = 128;
    lineCr[x] = 128;
  }
  /* black - 8 (3.75IRE) ; black ; black + 8  */
  for (; x < (5 * stripe_width) + (stripe_width / 3); x++) {
    lineY[x] =    8;
    lineCb[x] = 128;
    lineCr[x] = 128;
  }
  for (; x < (5 * stripe_width) + (2 * (stripe_width / 3)); x++) {
    lineY[x] =    16;
    lineCb[x] = 128;
    lineCr[x] = 128;
  }
  for (; x < (6 * stripe_width); x++) {
    lineY[x] =   24;
    lineCb[x] = 128;
    lineCr[x] = 128;
  }
  /* black */
  for (; x < width; x++) {
    lineY[x] =   16;
    lineCb[x] = 128;
    lineCr[x] = 128;
  }
  for (; y < height; y++) {
    memcpy(Y, lineY, width);
    memcpy(Cb, lineCb, width);
    memcpy(Cr, lineCr, width);
    Y += width;
    Cb += width;
    Cr += width;
  }

  free(lineY);
  free(lineCb);
  free(lineCr);
}




int main(int argc, char **argv)
{
  cl_info_t cl;
  y4m_stream_info_t sinfo;
  y4m_frame_info_t finfo;
  unsigned char *planes[3];  /* Y'CbCr frame buffer */
  int fdout = 1;  /* stdout file descriptor */
  int i;

  y4m_init_stream_info(&sinfo);
  y4m_init_frame_info(&finfo);

  parse_args(&cl, argc, argv);

  /* Setup streaminfo and write output header */
  y4m_si_set_width(&sinfo, cl.width);
  y4m_si_set_height(&sinfo, cl.height);
  y4m_si_set_sampleaspect(&sinfo, cl.aspect);
  y4m_si_set_interlace(&sinfo, cl.interlace);
  y4m_si_set_framerate(&sinfo, cl.framerate);
  y4m_write_stream_header(fdout, &sinfo);
  mjpeg_info("Colorbar Stream parameters:\n");
  y4m_log_stream_info(LOG_INFO, "  ", &sinfo);

  /* Create the colorbars frame */
  for (i = 0; i < 3; i++)
    planes[i] = malloc(cl.width * cl.height * sizeof(planes[i][0]));
  create_bars(planes, cl.width, cl.height);
  chroma_subsample(cl.ss_mode, planes, cl.width, cl.height);

  /* We're on the air! */
  for (i = 0; i < cl.framecount; i++)
      y4m_write_frame(fdout, &sinfo, &finfo, planes);

  /* We're off the air. :( */
  for (i = 0; i < 3; i++)
    free(planes[i]);
  y4m_fini_stream_info(&sinfo);
  y4m_fini_frame_info(&finfo);
  return 0;
}
