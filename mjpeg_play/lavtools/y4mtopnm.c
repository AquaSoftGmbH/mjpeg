/*
 * y4mtopnm.c:  Convert a YUV4MPEG2 stream into one or more PPM/PGM/PAM images.
 *
 *              Converts ITU-Rec.601 Y'CbCr to R'G'B' colorspace
 *               (or Rec.601 Y' to [0,255] grayscale, for PGM).
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

#include "config.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>

#include <yuv4mpeg.h>
#include <mpegconsts.h>
#include "colorspace.h"


/* command-line parameters */
typedef struct _cl_info {
  int make_pam;
  int deinterleave;
  int verbosity;
  FILE *outfp;
} cl_info_t;



static
void usage(const char *progname)
{
  fprintf(stdout, "\n");
  fprintf(stdout, "usage:  %s [options]\n", progname);
  fprintf(stdout, "\n");
  fprintf(stdout, "Reads YUV4MPEG2 stream from stdin and produces RAW PPM, PGM\n");
  fprintf(stdout, " or PAM image(s) on stdout.  Converts digital video Y'CbCr colorspace\n");
  fprintf(stdout, " to computer graphics R'G'B'.\n");
  fprintf(stdout, "\n");
  fprintf(stdout, " options:  (defaults specified in [])\n");
  fprintf(stdout, "\n");
  fprintf(stdout, "  -P       produce PAM output instead of PPM/PGM\n");
  fprintf(stdout, "  -D       de-interleave fields into two PNM images\n");
  fprintf(stdout, "  -v n     verbosity (0,1,2) [1]\n");
  fprintf(stdout, "  -h       print this help message\n");
}



static
void parse_args(cl_info_t *cl, int argc, char **argv)
{
  int c;

  cl->make_pam = 0;
  cl->deinterleave = 0;
  cl->verbosity = 1;
  cl->outfp = stdout; /* default to stdout */

  while ((c = getopt(argc, argv, "PDv:h")) != -1) {
    switch (c) {
    case 'P':
      cl->make_pam = 1;
      break;
    case 'D':
      cl->deinterleave = 1;
      break;
    case 'v':
      cl->verbosity = atoi(optarg);
      if ((cl->verbosity < 0) || (cl->verbosity > 2))
	mjpeg_error("Verbosity must be 0, 1, or 2:  '%s'", optarg);
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
  if (optind != argc) 
    goto ERROR_EXIT;


  mjpeg_default_handler_verbosity(cl->verbosity);
  /* DONE! */
  return;

 ERROR_EXIT:
  mjpeg_error("For usage hints, use option '-h'.  Please take a hint.");
  exit(1);

}


static
void write_pam(FILE *fp,
               int chroma,
               uint8_t *buffers[],
               uint8_t *rowbuffer,
               int width, int height) 
{
  mjpeg_debug("write raw PAM image from one buffer, %dx%d", width, height);
  fprintf(fp, "P7\n");
  fprintf(fp, "WIDTH %d\n", width);
  fprintf(fp, "HEIGHT %d\n", height);
  fprintf(fp, "MAXVAL 255\n");
  switch (chroma) {
  case Y4M_CHROMA_444:
    fprintf(fp, "DEPTH 3\n");
    fprintf(fp, "TUPLTYPE RGB\n");
    fprintf(fp, "ENDHDR\n");
    {
      int x, y;
      uint8_t *pixels;
      uint8_t *R = buffers[0];
      uint8_t *G = buffers[1];
      uint8_t *B = buffers[2];
      for (y = 0; y < height; y++) {
        pixels = rowbuffer;
        for (x = 0; x < width; x++) {
          *(pixels++) = *(R++);
          *(pixels++) = *(G++);
          *(pixels++) = *(B++);
        }
        fwrite(rowbuffer, sizeof(rowbuffer[0]), width * 3, fp);
      }
    }
    break;
  case Y4M_CHROMA_444ALPHA:
    fprintf(fp, "DEPTH 4\n");
    fprintf(fp, "TUPLTYPE RGB_ALPHA\n");
    fprintf(fp, "ENDHDR\n");
    {
      int x, y;
      uint8_t *pixels;
      uint8_t *R = buffers[0];
      uint8_t *G = buffers[1];
      uint8_t *B = buffers[2];
      uint8_t *A = buffers[3];
      for (y = 0; y < height; y++) {
        pixels = rowbuffer;
        for (x = 0; x < width; x++) {
          *(pixels++) = *(R++);
          *(pixels++) = *(G++);
          *(pixels++) = *(B++);
          *(pixels++) = *(A++);
        }
        fwrite(rowbuffer, sizeof(rowbuffer[0]), width * 4, fp);
      }
    }
    break;
  case Y4M_CHROMA_MONO:
    fprintf(fp, "DEPTH 1\n");
    fprintf(fp, "TUPLTYPE GRAYSCALE\n");
    fprintf(fp, "ENDHDR\n");
    fwrite(buffers[0], sizeof(buffers[0][0]), width * height, fp);
    break;
  }
  
}


static
void write_ppm(FILE *fp,
               uint8_t *buffers[],
               uint8_t *rowbuffer,
               int width, int height) 
{
  int x, y;
  uint8_t *pixels;
  uint8_t *R = buffers[0];
  uint8_t *G = buffers[1];
  uint8_t *B = buffers[2];

  mjpeg_debug("write raw PPM image from one buffer, %dx%d", width, height);
  fprintf(fp, "P6\n%d %d 255\n", width, height);
  for (y = 0; y < height; y++) {
    pixels = rowbuffer;
    for (x = 0; x < width; x++) {
      *(pixels++) = *(R++);
      *(pixels++) = *(G++);
      *(pixels++) = *(B++);
    }
    fwrite(rowbuffer, sizeof(rowbuffer[0]), width * 3, fp);
  }
}


static
void write_pgm(FILE *fp, uint8_t *buffer, int width, int height) 
{
  mjpeg_debug("write raw PGM image from one buffer, %dx%d", width, height);
  fprintf(fp, "P5\n%d %d 255\n", width, height);
  fwrite(buffer, sizeof(buffer[0]), width * height, fp);
}




static
void write_image(FILE *fp,
                 int make_pam,
                 int chroma,
                 uint8_t *buffers[],
                 uint8_t *rowbuffer,
                 int width, int height) 
{
  if (make_pam) {
    write_pam(fp, chroma, buffers, rowbuffer, width, height);
  } else {
    switch (chroma) {
    case Y4M_CHROMA_444:
      write_ppm(fp, buffers, rowbuffer, width, height);
      break;
    case Y4M_CHROMA_444ALPHA:
      write_ppm(fp, buffers, rowbuffer, width, height);
      write_pgm(fp, buffers[3], width, height);
      break;
    case Y4M_CHROMA_MONO:
      write_pgm(fp, buffers[0], width, height);
      break;
    default:
      assert(0);  break;
    }
  }
}
  


int main(int argc, char **argv)
{
  int in_fd = 0;
  cl_info_t cl;
  y4m_stream_info_t streaminfo;
  y4m_frame_info_t frameinfo;
  uint8_t *buffers[Y4M_MAX_NUM_PLANES];  /* R'G'B' or Y'CbCr */
  uint8_t *buffers2[Y4M_MAX_NUM_PLANES]; /* R'G'B' or Y'CbCr */
  int err, i;
  int width, height;
  int interlace, chroma, planes;
  uint8_t *rowbuffer = NULL;

  y4m_accept_extensions(1); /* allow non-4:2:0 chroma */
  y4m_init_stream_info(&streaminfo);
  y4m_init_frame_info(&frameinfo);

  parse_args(&cl, argc, argv);

  if ((err = y4m_read_stream_header(in_fd, &streaminfo)) != Y4M_OK) {
    mjpeg_error("Couldn't read YUV4MPEG2 header: %s!", y4m_strerr(err));
    exit(1);
  }
  mjpeg_info("Input stream parameters:");
  y4m_log_stream_info(LOG_INFO, "<<<", &streaminfo);

  width = y4m_si_get_width(&streaminfo);
  height = y4m_si_get_height(&streaminfo);
  interlace = y4m_si_get_interlace(&streaminfo);
  chroma = y4m_si_get_chroma(&streaminfo);
  planes = y4m_si_get_plane_count(&streaminfo);

  switch (chroma) {
  case Y4M_CHROMA_444:
  case Y4M_CHROMA_444ALPHA:
  case Y4M_CHROMA_MONO:
    break;
  default:
    mjpeg_error("Cannot handle input stream's chroma mode!");
    mjpeg_error_exit1("Input must be non-subsampled (e.g. 4:4:4).");
  }
    
  mjpeg_info("Output image parameters:");
  mjpeg_info("   format:  %s",
             (cl.make_pam) ? "PAM" :
             (chroma == Y4M_CHROMA_444) ? "PPM" :
             (chroma == Y4M_CHROMA_444ALPHA) ? "PPM+PGM" :
             (chroma == Y4M_CHROMA_MONO) ? "PGM" : "???");
  mjpeg_info("  framing:  %s",
             (cl.deinterleave) ? "two images per frame (deinterleaved)" :
             "one image per frame (interleaved)");
  if (cl.deinterleave) {
    mjpeg_info("    order:  %s",
               (interlace == Y4M_ILACE_BOTTOM_FIRST) ? "bottom field first" :
               "top field first");
  }

  /*** Allocate buffers ***/
  mjpeg_debug("allocating buffers...");
  rowbuffer = malloc(width * planes * sizeof(rowbuffer[0]));
  mjpeg_debug("  rowbuffer %p", rowbuffer);
  /* allocate buffers big enough for 4:4:4 supersampled components */
  for (i = 0; i < planes; i++) {
    switch (interlace) {
    case Y4M_ILACE_NONE:
      buffers[i] = malloc(width * height * sizeof(buffers[i][0]));
      buffers2[i] = NULL;
      break;
    case Y4M_ILACE_TOP_FIRST:
    case Y4M_ILACE_BOTTOM_FIRST:
    case Y4M_ILACE_MIXED:
      /* 'buffers' may hold whole frame or one field... */
      buffers[i] = malloc(width * height * sizeof(buffers[i][0]));
      /* ... but 'buffers2' will never hold more than one field. */
      buffers2[i] = malloc(width * height / 2 * sizeof(buffers[i][0]));
      break;
    default:
      assert(0);
      break;
    }
    mjpeg_debug("  buffers[%d] %p   buffers2[%d] %p", 
                i, buffers[i], i, buffers2[i]);
  }

  /* XXXXXX handle mixed-mode field output properly! */
  /* which fields is written first if 'p' */

  /*** Process frames ***/
  while (1) {
    int temporal;
    err = y4m_read_frame_header(in_fd, &streaminfo, &frameinfo);
    if (err != Y4M_OK) break;
    temporal = y4m_fi_get_temporal(&frameinfo);

    if (!cl.deinterleave) {
      mjpeg_debug("reading whole frame...");
      err = y4m_read_frame_data(in_fd, &streaminfo, &frameinfo, buffers);
      if (err != Y4M_OK) break;
      switch (chroma) {
      case Y4M_CHROMA_444:
        convert_YCbCr_to_RGB(buffers, width * height);
        break;
      case Y4M_CHROMA_444ALPHA:
        convert_YCbCr_to_RGB(buffers, width * height);
        convert_Y219_to_Y255(buffers[3], width * height);
        break;
      case Y4M_CHROMA_MONO:
        convert_Y219_to_Y255(buffers[0], width * height);
        break;
      default:
        assert(0);  break;
      }
      write_image(cl.outfp, cl.make_pam,
                  chroma, buffers, rowbuffer, width, height);
    } else {
      mjpeg_debug("reading separate fields...");
      err = y4m_read_fields_data(in_fd, &streaminfo, &frameinfo, 
                                 buffers, buffers2);
      if (err != Y4M_OK) break;
      switch (chroma) {
      case Y4M_CHROMA_444:
        convert_YCbCr_to_RGB(buffers,  width * height / 2);
        convert_YCbCr_to_RGB(buffers2, width * height / 2);
        break;
      case Y4M_CHROMA_444ALPHA:
        convert_YCbCr_to_RGB(buffers,     width * height / 2);
        convert_YCbCr_to_RGB(buffers2,    width * height / 2);
        convert_Y219_to_Y255(buffers[3],  width * height / 2);
        convert_Y219_to_Y255(buffers2[3], width * height / 2);
        break;
      case Y4M_CHROMA_MONO:
        convert_Y219_to_Y255(buffers[0],  width * height / 2);
        convert_Y219_to_Y255(buffers2[0], width * height / 2);
        break;
      default:
        assert(0);  break;
      }
      switch (interlace) {
      case Y4M_ILACE_NONE:
      case Y4M_ILACE_MIXED:
      case Y4M_ILACE_TOP_FIRST:
        /* write top field first */
        write_image(cl.outfp, cl.make_pam,
                    chroma, buffers, rowbuffer, width, height / 2);
        write_image(cl.outfp, cl.make_pam,
                    chroma, buffers2, rowbuffer, width, height / 2);
        break;
      case Y4M_ILACE_BOTTOM_FIRST:
        /* write bottom field first */
        write_image(cl.outfp, cl.make_pam,
                    chroma, buffers2, rowbuffer, width, height / 2);
        write_image(cl.outfp, cl.make_pam,
                    chroma, buffers, rowbuffer, width, height / 2);
        break;
      default:
        mjpeg_error_exit1("Unknown input interlacing mode (%d).\n", interlace);
        break;
      }
    }
  }     
  if ((err != Y4M_OK) && (err != Y4M_ERR_EOF))
    mjpeg_error("Couldn't read frame:  %s", y4m_strerr(err));

  /*** Clean-up after ourselves ***/
  mjpeg_debug("freeing buffers; cleaning up");
  free(rowbuffer);
  for (i = 0; i < planes; i++) {
    free(buffers[i]);
    if (buffers2[i] != NULL)
      free(buffers2[i]);
  }
  y4m_fini_stream_info(&streaminfo);
  y4m_fini_frame_info(&frameinfo);
  
  mjpeg_debug("Done.");
  return 0;
}


