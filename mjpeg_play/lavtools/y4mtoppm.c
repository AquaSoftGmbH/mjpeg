/*
 * y4mtoppm.c:  Generate one or more PPM images from a YUV4MPEG2 stream
 *
 *              Performs 4:2:0->4:4:4 chroma supersampling and then
 *               converts ITU-Rec.601 Y'CbCr to R'G'B' colorspace.
 *
 *
 *  Copyright (C) 2002 Matthew J. Marjanovic <maddog@mir.com>
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
#include <fcntl.h>

#include <yuv4mpeg.h>
#include <mpegconsts.h>
#include "subsample.h"
#include "colorspace.h"


/* command-line parameters */
typedef struct _cl_info {
  int interleave;
  int ss_mode;
  int verbosity;
  FILE *outfp;
} cl_info_t;



static
void usage(const char *progname)
{
  int i;
  fprintf(stdout, "\n");
  fprintf(stdout, "usage:  %s [options]\n", progname);
  fprintf(stdout, "\n");
  fprintf(stdout, "Reads YUV4MPEG2 stream from stdin and produces RAW PPM image(s) on stdout.\n");
  fprintf(stdout, "Converts digital video Y'CbCr colorspace to computer graphics R'G'B',\n");
  fprintf(stdout, " and performs chroma supersampling.\n");
  fprintf(stdout, "\n");
  fprintf(stdout, " options:  (defaults specified in [])\n");
  fprintf(stdout, "\n");
  fprintf(stdout, "  -L       interleave fields into single PPM image\n");
  fprintf(stdout, "  -S mode  chroma subsampling mode [%s]\n",
	  ssm_id[SSM_420_JPEG]);
  for (i = 1; i < SSM_COUNT; i++)
  fprintf(stdout, "            '%s' -> %s\n", ssm_id[i], ssm_description[i]);
  fprintf(stdout, "  -v n     verbosity (0,1,2) [1]\n");
}



static
void parse_args(cl_info_t *cl, int argc, char **argv)
{
  int i;
  int c;

  cl->interleave = 0;
  cl->ss_mode = SSM_420_JPEG;
  cl->verbosity = 1;
  cl->outfp = stdout; /* default to stdout */

  while ((c = getopt(argc, argv, "LS:v:h")) != -1) {
    switch (c) {
    case 'L':
      cl->interleave = 1;
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
#if 0
  if (optind == (argc - 1)) {
    if ((cl->fdin = open(argv[optind], O_RDONLY | O_BINARY)) == -1)
      mjpeg_error_exit1("Failed to open '%s':  %s\n",
			argv[optind], strerror(errno));
  } else 
#endif
    if (optind != argc) 
    goto ERROR_EXIT;


  mjpeg_default_handler_verbosity(cl->verbosity);
  /* DONE! */
  return;

 ERROR_EXIT:
  mjpeg_error("For usage hints, use option '-h'.  Please take a hint.\n");
  exit(1);

}






static
void write_ppm_from_two_buffers(FILE *fp,
				uint8_t *buffers[], 
				uint8_t *buffers2[], 
				uint8_t *rowbuffer,
				int width, int height)
{
  int x, y;
  uint8_t *pixels;
  uint8_t *R = buffers[0];
  uint8_t *G = buffers[1];
  uint8_t *B = buffers[2];
  uint8_t *R2 = buffers2[0];
  uint8_t *G2 = buffers2[1];
  uint8_t *B2 = buffers2[2];

  mjpeg_debug("write PPM image from two buffers, %dx%d\n", width, height);
  fprintf(fp, "P6\n%d %d 255\n", width, height);
  for (y = 0; y < height; y += 2) {
    pixels = rowbuffer;
    for (x = 0; x < width; x++) {
      *(pixels++) = *(R++);
      *(pixels++) = *(G++);
      *(pixels++) = *(B++);
    }
    fwrite(rowbuffer, sizeof(rowbuffer[0]), width * 3, fp);
    pixels = rowbuffer;
    for (x = 0; x < width; x++) {
      *(pixels++) = *(R2++);
      *(pixels++) = *(G2++);
      *(pixels++) = *(B2++);
    }
    fwrite(rowbuffer, sizeof(rowbuffer[0]), width * 3, fp);
  }
}



static
void write_ppm_from_one_buffer(FILE *fp,
			       uint8_t *buffers[],
			       uint8_t *rowbuffer,
			       int width, int height) 
{
  int x, y;
  uint8_t *pixels;
  uint8_t *R = buffers[0];
  uint8_t *G = buffers[1];
  uint8_t *B = buffers[2];

  mjpeg_debug("write PPM image from one buffer, %dx%d\n", width, height);
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




int main(int argc, char **argv)
{
  int in_fd = 0;
  cl_info_t cl;
  y4m_stream_info_t streaminfo;
  y4m_frame_info_t frameinfo;
  uint8_t *buffers[3];  /* R'G'B' or Y'CbCr */
  uint8_t *buffers2[3]; /* R'G'B' or Y'CbCr */
  int err, i;
  int width, height;
  int interlace;
  uint8_t *rowbuffer;

  y4m_init_stream_info(&streaminfo);
  y4m_init_frame_info(&frameinfo);

  parse_args(&cl, argc, argv);

  if ((err = y4m_read_stream_header(in_fd, &streaminfo)) != Y4M_OK) {
    mjpeg_error("Couldn't read YUV4MPEG2 header: %s!\n", y4m_strerr(err));
    exit(1);
  }
  mjpeg_info("input stream parameters:\n");
  y4m_log_stream_info(LOG_INFO, "<<<", &streaminfo);

  width = y4m_si_get_width(&streaminfo);
  height = y4m_si_get_height(&streaminfo);
  interlace = y4m_si_get_interlace(&streaminfo);

  /*** Allocate buffers ***/
  mjpeg_debug("allocating buffers...\n");
  rowbuffer = malloc(width * 3 * sizeof(rowbuffer[0]));
  mjpeg_debug("  rowbuffer %p\n", rowbuffer);
  /* allocate buffers big enough for 4:4:4 supersampled components */
  for (i = 0; i < 3; i++) {
    if (interlace == Y4M_ILACE_NONE) {
      buffers[i] = malloc(width * height * sizeof(buffers[i][0]));
      buffers2[i] = NULL;
    } else {
      buffers[i] = malloc(width * height / 2 * sizeof(buffers[i][0]));
      buffers2[i] = malloc(width * height / 2 * sizeof(buffers[i][0]));
    }
    mjpeg_debug("  buffers[%d] %p   buffers2[%d] %p\n", 
		i, buffers[i], i, buffers2[i]);
  }

  /*** Process frames ***/
  while (1) {
    
    if (interlace == Y4M_ILACE_NONE) {
      mjpeg_debug("reading noninterlaced frame...\n");
      err = y4m_read_frame(in_fd, &streaminfo, &frameinfo, buffers);
      if (err != Y4M_OK) break;
      mjpeg_debug("supersampling noninterlaced frame...\n");
      chroma_supersample(cl.ss_mode, buffers, width, height);
      mjpeg_debug("color converting noninterlaced frame...\n");
      convert_YCbCr_to_RGB(buffers, width * height);
      write_ppm_from_one_buffer(cl.outfp, buffers, rowbuffer, width, height);
    } else {
      err = y4m_read_fields(in_fd, &streaminfo, &frameinfo, 
			    buffers, buffers2);
      if (err != Y4M_OK) break;
      mjpeg_debug("supersampling top field...\n");
      chroma_supersample(cl.ss_mode, buffers, width, height / 2);
      mjpeg_debug("supersampling bottom field...\n");
      chroma_supersample(cl.ss_mode, buffers2, width, height / 2);
      mjpeg_debug("color converting top field...\n");
      convert_YCbCr_to_RGB(buffers, width * height / 2);
      mjpeg_debug("color converting bottom field...\n");
      convert_YCbCr_to_RGB(buffers2, width * height / 2);
      if (cl.interleave) {
	write_ppm_from_two_buffers(cl.outfp, buffers, buffers2, 
				   rowbuffer, width, height);
      } else if (interlace == Y4M_ILACE_TOP_FIRST) {
	write_ppm_from_one_buffer(cl.outfp, buffers,
				  rowbuffer, width, height / 2);
	write_ppm_from_one_buffer(cl.outfp, buffers2,
				  rowbuffer, width, height / 2);
      } else { /* ilace == Y4M_ILACE_BOTTOM_FIRST */
	write_ppm_from_one_buffer(cl.outfp, buffers2,
				  rowbuffer, width, height / 2);
	write_ppm_from_one_buffer(cl.outfp, buffers,
				  rowbuffer, width, height / 2);
      }
    }
  }     
  if ((err != Y4M_OK) && (err != Y4M_ERR_EOF))
    mjpeg_error("Couldn't read frame:  %s\n", y4m_strerr(err));

  /*** Clean-up after ourselves ***/
  mjpeg_debug("freeing buffers; cleaning up\n");
  free(rowbuffer);
  for (i = 0; i < 3; i++) {
    free(buffers[i]);
    free(buffers2[i]);
  }
  y4m_fini_stream_info(&streaminfo);
  y4m_fini_frame_info(&frameinfo);
  
  mjpeg_debug("Done.\n");
  return 0;
}


