/*
 * y4mcolorbars.c:  Generate real, standard colorbars in POG form 
 *                   (where "POG" == "YUV4MPEG2 stream").
 *
 *
 *  Copyright (C) 2004 Matthew J. Marjanovic <maddog@mir.com>
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

#include <config.h>

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include <yuv4mpeg.h>
#include <mpegconsts.h>

#include "subsample.h"
#include "colorspace.h"

#define DEFAULT_CHROMA_MODE Y4M_CHROMA_444


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
  fprintf(stdout, "usage: %s [options]\n", progname);
  fprintf(stdout, "\n");
  fprintf(stdout, "Creates a YUV4MPEG2 stream consisting of frames containing a standard\n");
  fprintf(stdout, " colorbar test pattern.\n");
  fprintf(stdout, "\n");
  fprintf(stdout, "Options:  (defaults specified in [])\n");
  fprintf(stdout, "\n");
  fprintf(stdout, "  -n n     frame count (output n frames) [1]\n");
  fprintf(stdout, "\n");
  fprintf(stdout, "  -W w     frame width [720]\n");
  fprintf(stdout, "  -H h     frame height [480]\n");
  fprintf(stdout, "  -F n:d   framerate (as ratio) [30000:1001]\n");
  fprintf(stdout, "  -A w:h   pixel aspect ratio [10:11]\n");
  fprintf(stdout, "  -I x     interlacing [p]\n");
  fprintf(stdout, "             p = none/progressive\n");
  fprintf(stdout, "             t = top-field-first\n");
  fprintf(stdout, "             b = bottom-field-first\n");
  fprintf(stdout, "\n");
  {
    int m;
    const char *keyword;

    fprintf(stdout, "  -S mode  chroma subsampling mode [%s]\n",
	    y4m_chroma_keyword(DEFAULT_CHROMA_MODE));
    for (m = 0;
	 (keyword = y4m_chroma_keyword(m)) != NULL;
	 m++)
      if (chroma_sub_implemented(m))
	fprintf(stdout, "            '%s' -> %s\n",
		keyword, y4m_chroma_description(m));
  }
  fprintf(stdout, "\n");
  fprintf(stdout, "  -v n     verbosity (0,1,2) [1]\n");
}



static
void parse_args(cl_info_t *cl, int argc, char **argv)
{
  int c;

  cl->interlace = Y4M_ILACE_NONE;
  cl->framerate = y4m_fps_NTSC;
  cl->framecount = 1;
  cl->ss_mode = DEFAULT_CHROMA_MODE;
  cl->width = 720;
  cl->height = 480;
  cl->aspect = y4m_sar_NTSC_CCIR601;
  cl->verbosity = 1;

  while ((c = getopt(argc, argv, "A:F:I:W:H:n:S:v:h")) != -1) {
    switch (c) {
    case 'W':
      if ((cl->width = atoi(optarg)) <= 0) {
	mjpeg_error("Invalid width:  '%s'", optarg);
	goto ERROR_EXIT;
      }
      break;
    case 'H':
      if ((cl->height = atoi(optarg)) <= 0) {
	mjpeg_error("Invalid height:  '%s'", optarg);
	goto ERROR_EXIT;
      }
      break;
    case 'A':
      if (y4m_parse_ratio(&(cl->aspect), optarg) != Y4M_OK) {
	mjpeg_error("Could not parse ratio:  '%s'", optarg);
	goto ERROR_EXIT;
      }
      break;
    case 'F':
      if (y4m_parse_ratio(&(cl->framerate), optarg) != Y4M_OK) {
	mjpeg_error("Could not parse framerate:  '%s'", optarg);
	goto ERROR_EXIT;
      }
      break;
    case 'I':
      switch (optarg[0]) {
      case 'p':  cl->interlace = Y4M_ILACE_NONE;  break;
      case 't':  cl->interlace = Y4M_ILACE_TOP_FIRST;  break;
      case 'b':  cl->interlace = Y4M_ILACE_BOTTOM_FIRST;  break;
      default:
	mjpeg_error("Unknown value for interlace: '%c'", optarg[0]);
	goto ERROR_EXIT;
	break;
      }
      break;
    case 'n':
      if ((cl->framecount = atoi(optarg)) <= 0) {
	mjpeg_error("Invalid frame count:  '%s'", optarg);
	goto ERROR_EXIT;
      }
      break;
    case 'S':
      cl->ss_mode = y4m_chroma_parse_keyword(optarg);
      if (cl->ss_mode == Y4M_UNKNOWN) {
	mjpeg_error("Unknown subsampling mode option:  %s", optarg);
	goto ERROR_EXIT;
      } else if (!chroma_sub_implemented(cl->ss_mode)) {
	mjpeg_error("Unsupported subsampling mode option:  %s", optarg);
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
      mjpeg_error("Unknown option:  '-%c'", optopt);
      goto ERROR_EXIT;
      break;
    }
  }

  mjpeg_default_handler_verbosity(cl->verbosity);

  mjpeg_info("Colorbar Command-line Parameters:");
  mjpeg_info("            frame size:  %dx%d", cl->width, cl->height);
  mjpeg_info("             framerate:  %d:%d",
	     cl->framerate.n, cl->framerate.d);
  mjpeg_info("    pixel aspect ratio:  %d:%d",
	     cl->aspect.n, cl->aspect.d);
  mjpeg_info("             interlace:  %s",
	     mpeg_interlace_code_definition(cl->interlace));
  mjpeg_info("           # of frames:  %d", cl->framecount);
  mjpeg_info("    chroma subsampling:  %s",
	     y4m_chroma_description(cl->ss_mode));
  return;

 ERROR_EXIT:
  mjpeg_error("For usage hints, use option '-h':  '%s -h'", argv[0]);
  exit(1);

}




/*
 * Color Bars:
 *
 *     top 2/3:  75% white, followed by 75% binary combinations
 *                of R', G', and B' with decreasing luma
 *
 * middle 1/12:  reverse order of above, but with 75% white and
 *                alternating black
 *
 *  bottom 1/4:  -I, 100% white, +Q, black, PLUGE, black,
 *                where PLUGE is (black - 4 IRE), black, (black + 4 IRE)
 *
 */

/*  75% white   */
/*  75% yellow  */
/*  75% cyan    */
/*  75% green   */
/*  75% magenta */
/*  75% red     */
/*  75% blue    */
static uint8_t rainbowRGB[][7] = {
  { 191, 191,   0,   0, 191, 191,   0 },
  { 191, 191, 191, 191,   0,   0,   0 },
  { 191,   0, 191,   0, 191,   0, 191 }
};
static uint8_t *rainbow[3] = {
  rainbowRGB[0], rainbowRGB[1], rainbowRGB[2]
};


/*  75% blue    */
/*      black   */
/*  75% magenta */
/*      black   */
/*  75% cyan    */
/*      black   */
/*  75% white   */
static uint8_t wobnairRGB[][7] = {
  {   0,   0, 191,   0,   0,   0, 191 },
  {   0,   0,   0,   0, 191,   0, 191 },
  { 191,   0, 191,   0, 191,   0, 191 }
};
static uint8_t *wobnair[3] = {
  wobnairRGB[0], wobnairRGB[1], wobnairRGB[2]
};


static
void create_bars(uint8_t *ycbcr[], int width, int height)
{
  int i, x, y, w;
  int bnb_start;
  int pluge_start;
  int stripe_width;
  int pl_width;
  uint8_t *lineY;
  uint8_t *lineCb;
  uint8_t *lineCr;

  uint8_t *Y = ycbcr[0];
  uint8_t *Cb = ycbcr[1];
  uint8_t *Cr = ycbcr[2];

  convert_RGB_to_YCbCr(rainbow, 7);
  convert_RGB_to_YCbCr(wobnair, 7);

  bnb_start = height * 2 / 3;
  pluge_start = height * 3 / 4;
  stripe_width = (width + 6) / 7;
  lineY = malloc(width * sizeof(lineY[0]));
  lineCb = malloc(width * sizeof(lineCb[0]));
  lineCr = malloc(width * sizeof(lineCr[0]));

  /* Top:  Rainbow */
  for (i = 0, x = 0; i < 7; i++) {
    for (w = 0; (w < stripe_width) && (x < width); w++, x++) {
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
  for (i = 0, x = 0; i < 7; i++) {
    for (w = 0; (w < stripe_width) && (x < width); w++, x++) {
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
  /* black - 8 (3.75IRE) | black | black + 8  */
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
  uint8_t *planes[Y4M_MAX_NUM_PLANES];  /* Y'CbCr frame buffer */
  int fdout = 1;  /* stdout file descriptor */
  int i;
  int err;

  y4m_accept_extensions(1);
  y4m_init_stream_info(&sinfo);
  y4m_init_frame_info(&finfo);

  parse_args(&cl, argc, argv);

  /* Setup streaminfo and write output header */
  y4m_si_set_width(&sinfo, cl.width);
  y4m_si_set_height(&sinfo, cl.height);
  y4m_si_set_sampleaspect(&sinfo, cl.aspect);
  y4m_si_set_interlace(&sinfo, cl.interlace);
  y4m_si_set_framerate(&sinfo, cl.framerate);
  y4m_si_set_chroma(&sinfo, cl.ss_mode);
  if ((err = y4m_write_stream_header(fdout, &sinfo)) != Y4M_OK)
    mjpeg_error_exit1("Write header failed: %s", y4m_strerr(err));
  mjpeg_info("Colorbar Stream parameters:");
  y4m_log_stream_info(LOG_INFO, "  ", &sinfo);

  /* Create the colorbars frame */
  for (i = 0; i < 3; i++)
    planes[i] = malloc(cl.width * cl.height * sizeof(planes[i][0]));
  create_bars(planes, cl.width, cl.height);
  chroma_subsample(cl.ss_mode, planes, cl.width, cl.height);

  /* We're on the air! */
  for (i = 0; i < cl.framecount; i++) {
    if ((err = y4m_write_frame(fdout, &sinfo, &finfo, planes)) != Y4M_OK)
      mjpeg_error_exit1("Write frame failed: %s", y4m_strerr(err));
  }

  /* We're off the air. :( */
  for (i = 0; i < 3; i++)
    free(planes[i]);
  y4m_fini_stream_info(&sinfo);
  y4m_fini_frame_info(&finfo);
  return 0;
}
