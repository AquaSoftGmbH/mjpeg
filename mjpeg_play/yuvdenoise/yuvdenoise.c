
/****************************************************************************
 *                                                                          *
 * Yuv-Denoiser to be used with Andrew Stevens mpeg2enc                     *
 *                                                                          *
 * (C)2001 S.Fendt                                                          *
 *                                                                          *
 * Licensed and protected by the GNU-General-Public-License version 2       *
 * (or if you prefer any later version of that license). See the file       *
 * LICENSE for detailed information.                                        *
 *                                                                          *
 ****************************************************************************/



#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include "mjpeg_types.h"
#include "yuv4mpeg.h"
#include "mjpeg_logging.h"
#include "config.h"



/*****************************************************************************
 * if this is commented out, a lot of messages                               *
 * should apear on the screen...                                             *
 *****************************************************************************/

/* #define DEBUG */



/*****************************************************************************
 * here the search-blocksize is defined. The blocks which will be exchanged  *
 * will have the half size of the search block                               *
 *****************************************************************************/

#define BLOCKSIZE     8
#define BLOCKOFFSET   (BLOCKSIZE-(BLOCKSIZE/2))/2
#define RADIUS        32



/*****************************************************************************
 * Pointer definitions to some used buffers. These are allocated later and   *
 * (hopefully) freed all at the end of the program.                          *
 *****************************************************************************/

uint8_t *yuv[3];
uint8_t *yuv1[3];
uint8_t *yuv2[3];
uint8_t *avrg[3];
uint8_t *sub_r2[3];
uint8_t *sub_t2[3];
uint8_t *sub_r4[3];
uint8_t *sub_t4[3];



/*****************************************************************************
 * a frame counter is needed in calculate_motion_vectors, as we don't like   *
 * continious scene changes ... even if there isn't one. :)                  *
 *****************************************************************************/

uint32_t framenr = 0;



/*****************************************************************************
 * two global flags which control the behaveiour of the denoiser.            *
 *****************************************************************************/

int deinterlace = 0;            /* set to 1 if deinterlacing needed */
int scene_change = 0;           /* set to 1 if a scene change was detected */



/*****************************************************************************
 * some global variables needed for handling the YUV planar frames.          *
 *****************************************************************************/

int width;                      /* width of the images */
int height;                     /* height of the images */
int u_offset;                   /* offset of the U-component from beginning of the image */
int v_offset;                   /* offset of the V-component from beginning of the image */
int uv_width;                   /* width of the UV-components */
int uv_height;                  /* height of the UV-components */



/*****************************************************************************
 * global variables needed for the motion estimation                         *
 *****************************************************************************/

uint32_t mean_SAD;              /* mean sum of absolute differences */
float block_quality;



/*****************************************************************************
 * matrices to store various block-results                                   *
 *****************************************************************************/

int matrix[1024][768][2];
uint32_t SAD_matrix[1024][768];
uint32_t lum_matrix[1024][768];



/*****************************************************************************
 * pointers to stream-info used by Mato's Y4M-library                        *
 *****************************************************************************/

y4m_frame_info_t *frameinfo = NULL;
y4m_stream_info_t *streaminfo = NULL;



/*****************************************************************************
 * declaration of used functions                                             *
 *****************************************************************************/

void display_greeting ();
void display_help ();
void deinterlace_frame (uint8_t *yuv[3]);
void denoise_frame (uint8_t *ref[3]);
void antiflicker_reference (uint8_t *frame[3]);



/*****************************************************************************
 * MAIN                                                                      *
 *****************************************************************************/

int
main (int argc, char *argv[])
{
  int fd_in = 0;
  int fd_out = 1;
  int errnr;
  char c;
  int i;

  display_greeting ();

  /* process commandline */
  /* REMARK: at present no commandline options are really taken into account */

  while ((c = getopt (argc, argv, "?")) != -1)
    {
      switch (c)
        {
        case '?':
          display_help ();
          break;
        }
    }

  /* initialize stream-information */

  streaminfo = y4m_init_stream_info (NULL);
  frameinfo = y4m_init_frame_info (NULL);

  if ((errnr = y4m_read_stream_header (fd_in, streaminfo)) != Y4M_OK)
    {
      mjpeg_log (LOG_ERROR, "Couldn't read YUV4MPEG header: %s!\n", y4m_errstr (errnr));
      exit (1);
    }

  /* calculate frame constants used for YUV-processing */

  width = streaminfo->width;
  height = streaminfo->height;
  u_offset = width * height;
  v_offset = width * height * 1.25;
  uv_width = width >> 1;
  uv_height = height >> 1;

  /* allocate memory for buffers */

  yuv[0] = malloc (width * height * sizeof (uint8_t));
  yuv[1] = malloc (width * height * sizeof (uint8_t) / 4);
  yuv[2] = malloc (width * height * sizeof (uint8_t) / 4);

  yuv1[0] = malloc (width * height * sizeof (uint8_t));
  yuv1[1] = malloc (width * height * sizeof (uint8_t) / 4);
  yuv1[2] = malloc (width * height * sizeof (uint8_t) / 4);

  yuv2[0] = malloc (width * height * sizeof (uint8_t));
  yuv2[1] = malloc (width * height * sizeof (uint8_t) / 4);
  yuv2[2] = malloc (width * height * sizeof (uint8_t) / 4);

  avrg[0] = malloc (width * height * sizeof (uint8_t));
  avrg[1] = malloc (width * height * sizeof (uint8_t) / 4);
  avrg[2] = malloc (width * height * sizeof (uint8_t) / 4);

  sub_t2[0] = malloc (width * height * sizeof (uint8_t));
  sub_t2[1] = malloc (width * height * sizeof (uint8_t) / 4);
  sub_t2[2] = malloc (width * height * sizeof (uint8_t) / 4);

  sub_r2[0] = malloc (width * height * sizeof (uint8_t));
  sub_r2[1] = malloc (width * height * sizeof (uint8_t) / 4);
  sub_r2[2] = malloc (width * height * sizeof (uint8_t) / 4);

  sub_t4[0] = malloc (width * height * sizeof (uint8_t));
  sub_t4[1] = malloc (width * height * sizeof (uint8_t) / 4);
  sub_t4[2] = malloc (width * height * sizeof (uint8_t) / 4);

  sub_r4[0] = malloc (width * height * sizeof (uint8_t));
  sub_r4[1] = malloc (width * height * sizeof (uint8_t) / 4);
  sub_r4[2] = malloc (width * height * sizeof (uint8_t) / 4);

/* if needed, deinterlace frame */

  fprintf (stderr, "%d\n", streaminfo->interlace);

  if (streaminfo->interlace != Y4M_ILACE_NONE)
    {
      mjpeg_log (LOG_INFO, "stream read is interlaced: using deinterlacer.\n");
      mjpeg_log (LOG_INFO, "stream written is frame-based.\n");

      /* setting Y4M header to non-interlaced video */
      streaminfo->interlace = Y4M_ILACE_NONE;

      /* turning on deinterlacer */
      deinterlace = 1;
    }

/* write the outstream header */

  y4m_write_stream_header (fd_out, streaminfo);

/* read every single frame until the end of the input stream */

  while ((errnr = y4m_read_frame (fd_in, streaminfo, frameinfo, yuv)) == Y4M_OK)
    {
      /* process the frame */

      /* if needed, deinterlace frame */
      if (deinterlace)
        deinterlace_frame (yuv);

      /* deflicker */
      antiflicker_reference (yuv);

      /* main denoise processing */
      denoise_frame (yuv);

      /* write the frame */
      y4m_write_frame (fd_out, streaminfo, frameinfo, yuv);
    }


  /* free all used buffers and structures */

  y4m_free_stream_info (streaminfo);
  y4m_free_frame_info (frameinfo);

  for (i = 0; i < 3; i++)
    {
      free (yuv[i]);
      free (yuv1[i]);
      free (yuv2[i]);
      free (avrg[i]);
      free (sub_t2[i]);
      free (sub_r2[i]);
      free (sub_t4[i]);
      free (sub_r4[i]);
    }

  /* Exit with zero status */
  return (0);
}



/*****************************************************************************
 * greeting - message                                                        *
 *****************************************************************************/

void
display_greeting ()
{
  mjpeg_log (LOG_INFO, "------------------------------------ \n");
  mjpeg_log (LOG_INFO, "Y4M Motion-Compensating-YUV-Denoiser \n");
  mjpeg_log (LOG_INFO, "------------------------------------ \n");
  mjpeg_log (LOG_INFO, "Version: MjpegTools %s\n", VERSION);
  mjpeg_log (LOG_INFO, "\n");
  mjpeg_log (LOG_INFO, "This is development software. It may \n");
  mjpeg_log (LOG_INFO, "corrupt your movies. \n");
  mjpeg_log (LOG_INFO, "------------------------------------ \n");
}

/*****************************************************************************
 * help - message                                                            *
 *****************************************************************************/

void
display_help ()
{
  fprintf (stdout, "sorry no help this time\n");
  exit (0);
}


/*****************************************************************************
 * Line-Deinterlacer                                                         *
 * 2001 S.Fendt                                                              *
 * it does a better job than just dropping lines but sometimes it            *
 * fails and reduces y-resolution... hmm... but everything else is           *
 * supposed to fail, too, sometimes, and is #much# more time consuming.      *
 *****************************************************************************/

void
deinterlace_frame (uint8_t *yuv[3])
{

  /* Buffer one line */
  uint8_t line[1024];
  unsigned int d;
  unsigned int min;
  int x;
  int y;
  int xx;
  int i;
  int xpos;
  int l1;
  int l2;
  int lumadiff=0;

  /* Go through the frame by every two lines */
  for (y = 0; y < height; y += 2)
    {
      /* Go through each line by a "block" length of 32 pixels */
      for (x = 0; x < width; x += 8)
        {
          /* search best matching block of pixels in other field line */
          min = 65535;
          xpos = 0;
          /* search in the range of +/- 16 pixels offset in the line */
          for (xx = -16; xx < 16; xx++)
            {
              d = 0;
              /* Calculate SAD */
              for (i = -8; i < 16; i++)
                {
                  /* to avoid blocking in ramps we analyse the best match on */
                  /* two lines ... */
                  d += (int) abs (*(yuv[0] + (x + i) + y * width) -
                                  *(yuv[0] + (x + xx + i) + (y + 1) * width));
                  d += (int) abs (*(yuv[0] + (x + i) + (y + 2) * width) -
                                  *(yuv[0] + (x + xx + i) + (y + 1) * width));
                }
              /* if SAD reaches a minimum store the position */
              if (min > d)
                {
                  min = d;
                  xpos = xx;
                  l1 = l2 = 0;
                  for (i = 0; i < 8; i++)
                    {
                      l1 += *(yuv[0] + (x + i) + y * width);
                      l2 += *(yuv[0] + (x + i + xpos) + (y + 1) * width);
                    }
                  l1 /= 8;
                  l2 /= 8;
                  lumadiff = abs (l1 - l2);
                  lumadiff = (lumadiff < 6) ? 0 : 1;
                }
            }

          /* copy pixel-block into the line-buffer */

          /* if lumadiff is small take the fields block, if not */
          /* take the other fields block */

          if (lumadiff && min > (8 * 24))
            for (i = 0; i < 8; i++)
              {
                *(line + x + i) =
                  *(yuv[0] + (x + i) + ((y) * width)) * 0.5 +
                  *(yuv[0] + (x + i) + ((y + 2) * width)) * 0.5;
              }
          else
            for (i = 0; i < 8; i++)
              {
                *(line + x + i) =
                  *(yuv[0] + (x + i + xpos) + ((y + 1) * width)) * 0.5 +
                  *(yuv[0] + (x + i) + ((y + 0) * width)) * 0.5;
              }
        }

      /* copy the line-buffer into the source-line */
      for (i = 0; i < width; i++)
        *(yuv[0] + i + (y + 1) * width) = *(line + i);
    }
}



/*****************************************************************************
 * takes a frame and blends it into the average                              *
 * buffer.                                                                   *
 * The blend may not be to slow, as noise wouldn't                           *
 * be filtered good than, but it may not be to fast                          *
 * either... I decided to take approx 5 frames into                          *
 * account. Probably need's some finetuning ...                              *
 *****************************************************************************/

void
average_frames (uint8_t *ref[3], uint8_t *avg[3])
{
  int x, y;

  /* blend Y component */

  for (y = 0; y < height; y++)
    for (x = 0; x < width; x++)
      {
        *(avg[0] + x + y * width) =
          (*(avg[0] + x + y * width) * 7 + *(ref[0] + x + y * width) * 1) / 8;
      }

  /* blend U and V components */

  for (y = 0; y < uv_height; y++)
    for (x = 0; x < uv_width; x++)
      {
        *(avg[1] + x + y * uv_width) =
          (*(avg[1] + x + y * uv_width) * 7 + *(ref[1] + x + y * uv_width) * 1) / 8;

        *(avg[2] + x + y * uv_width) =
          (*(avg[2] + x + y * uv_width) * 7 + *(ref[2] + x + y * uv_width) * 1) / 8;
      }
}



/*****************************************************************************
 * generate a lowpassfiltered and subsampled copy                            *
 * of the source image (src) at the destination                              *
 * image location.                                                           *
 * Lowpass-filtering is important, as this subsampled                        *
 * image is used for motion estimation and the result                        *
 * is more reliable when filtered.                                           *
 *****************************************************************************/

void
subsample_frame (uint8_t *dst[3], uint8_t *src[3])
{
  int x, y;

  /* subsample Y component */

  for (y = 0; y < height; y += 2)
    for (x = 0; x < width; x += 2)
      {
        *(dst[0] + (x / 2) + (y / 2) * width) =
          (*(src[0] + (x - 1) + (y - 1) * width) +
           *(src[0] + (x + 0) + (y - 1) * width) +
           *(src[0] + (x + 1) + (y - 1) * width) +
           *(src[0] + (x - 1) + (y + 0) * width) +
           *(src[0] + (x + 0) + (y + 0) * width) +
           *(src[0] + (x + 1) + (y + 0) * width) +
           *(src[0] + (x - 1) + (y + 1) * width) +
           *(src[0] + (x + 0) + (y + 1) * width) +
           *(src[0] + (x + 1) + (y + 1) * width)) / 9;
      }

  /* subsample U and V components */

  for (y = 0; y < uv_height; y += 2)
    for (x = 0; x < uv_width; x += 2)
      {
        *(dst[1] + (x / 2) + (y / 2) * uv_width) =
          (*(src[1] + (x - 1) + (y - 1) * uv_width) +
           *(src[1] + (x + 0) + (y - 1) * uv_width) +
           *(src[1] + (x + 1) + (y - 1) * uv_width) +
           *(src[1] + (x - 1) + (y + 0) * uv_width) +
           *(src[1] + (x + 0) + (y + 0) * uv_width) +
           *(src[1] + (x + 1) + (y + 0) * uv_width) +
           *(src[1] + (x - 1) + (y + 1) * uv_width) +
           *(src[1] + (x + 0) + (y + 1) * uv_width) +
           *(src[1] + (x + 1) + (y + 1) * uv_width)) / 9;

        *(dst[2] + (x / 2) + (y / 2) * uv_width) =
          (*(src[2] + (x - 1) + (y - 1) * uv_width) +
           *(src[2] + (x + 0) + (y - 1) * uv_width) +
           *(src[2] + (x + 1) + (y - 1) * uv_width) +
           *(src[2] + (x - 1) + (y + 0) * uv_width) +
           *(src[2] + (x + 0) + (y + 0) * uv_width) +
           *(src[2] + (x + 1) + (y + 0) * uv_width) +
           *(src[2] + (x - 1) + (y + 1) * uv_width) +
           *(src[2] + (x + 0) + (y + 1) * uv_width) +
           *(src[2] + (x + 1) + (y + 1) * uv_width)) / 9;
      }
}



/*****************************************************************************
 * SAD-function for Y                                                        *
 *****************************************************************************/

inline uint32_t
calc_SAD (uint8_t * frm, uint8_t * ref, uint32_t frm_offs, uint32_t ref_offs, int div)
{
  int dx, dy, Y;
  uint32_t d = 0;
  uint8_t *fs = frm + frm_offs;
  uint8_t *rs = ref + ref_offs;

  for (dy = 0; dy < BLOCKSIZE / div; dy++)
    {
      for (dx = 0; dx < BLOCKSIZE / div; dx++)
        {
          Y = (int) *(fs + dx) - (int) *(rs + dx);

          d += (Y > 0) ? Y : -Y;
        }
      fs += width;
      rs += width;
    }
  return d;
}

/*****************************************************************************
 * SAD-function for U+V                                                      *
 *****************************************************************************/

inline uint32_t
calc_SAD_uv (uint8_t * frm, uint8_t * ref, uint32_t frm_offs, uint32_t ref_offs, int div)
{
  int dx, dy, Y;
  uint32_t d = 0;
  uint8_t *fs = frm + frm_offs;
  uint8_t *rs = ref + ref_offs;

  for (dy = 0; dy < BLOCKSIZE / 2 / div; dy++)
    {
      for (dx = 0; dx < BLOCKSIZE / 2 / div; dx++)
        {
          Y = (int) *(fs + dx) - (int) *(rs + dx);

          d += (Y > 0) ? Y : -Y;
        }
      fs += uv_width;
      rs += uv_width;
    }
  return d;
}


void
mb_search_44 (int x, int y, uint8_t *ref_frame[3], uint8_t *tgt_frame[3])
{
  int qy, qx;
  uint32_t d;
  uint32_t CAD = (256 * BLOCKSIZE * BLOCKSIZE);
  uint32_t SAD = (256 * BLOCKSIZE * BLOCKSIZE);

  /* search_radius has to be devided by 8 as radius is given in */
  /* half-pixels and image is reduced in resolution by 4 ...    */

  for (qy = -RADIUS / 8; qy <= RADIUS / 8; qy += 4)
    for (qx = -RADIUS / 8; qx <= RADIUS / 8; qx += 4)
      {
        d = calc_SAD (tgt_frame[0],
                      ref_frame[0],
                      (x + qx - BLOCKOFFSET) / 4 + (y + qy - BLOCKOFFSET) / 4 * width,
                      (x - BLOCKOFFSET) / 4 + (y - BLOCKOFFSET) / 4 * width, 4);

        d += calc_SAD_uv (tgt_frame[1],
                          ref_frame[1],
                          (x + qx - BLOCKOFFSET) / 8 + (y + qy -
                                                        BLOCKOFFSET) / 8 * uv_width,
                          (x - BLOCKOFFSET) / 8 + (y - BLOCKOFFSET) / 8 * uv_width, 2);

        d += calc_SAD_uv (tgt_frame[2],
                          ref_frame[2],
                          (x + qx - BLOCKOFFSET) / 8 + (y + qy -
                                                        BLOCKOFFSET) / 8 * uv_width,
                          (x - BLOCKOFFSET) / 8 + (y - BLOCKOFFSET) / 8 * uv_width, 2);

        if (d < SAD)
          {
            matrix[x][y][0] = qx * 2;
            matrix[x][y][1] = qy * 2;
            SAD = d;
          }

        if (qx == 0 && qy == 0)
          {
            CAD = d;
          }

      }

  if (SAD > CAD)
    {
      matrix[x][y][0] = 0;
      matrix[x][y][1] = 0;
    }
}

void
mb_search_22 (int x, int y, uint8_t *ref_frame[3], uint8_t *tgt_frame[3])
{
  int qy, qx;
  uint32_t d;
  uint32_t CAD = (256 * BLOCKSIZE * BLOCKSIZE);
  uint32_t SAD = (256 * BLOCKSIZE * BLOCKSIZE);
  int bx;
  int by;

  bx = matrix[x][y][0] / 2;
  by = matrix[x][y][1] / 2;

  for (qy = (by - 2); qy <= (by + 2); qy += 2)
    for (qx = (bx - 2); qx <= (bx + 2); qx += 2)
      {
        d = calc_SAD (tgt_frame[0],
                      ref_frame[0],
                      (x + qx - BLOCKOFFSET) / 2 + (y + qy - BLOCKOFFSET) / 2 * width,
                      (x - BLOCKOFFSET) / 2 + (y - BLOCKOFFSET) / 2 * width, 2);

        d += calc_SAD_uv (tgt_frame[1],
                          ref_frame[1],
                          (x + qx - BLOCKOFFSET) / 4 + (y + qy -
                                                        BLOCKOFFSET) / 4 * uv_width,
                          (x - BLOCKOFFSET) / 4 + (y - BLOCKOFFSET) / 4 * uv_width, 2);

        d += calc_SAD_uv (tgt_frame[2],
                          ref_frame[2],
                          (x + qx - BLOCKOFFSET) / 4 + (y + qy -
                                                        BLOCKOFFSET) / 4 * uv_width,
                          (x - BLOCKOFFSET) / 4 + (y - BLOCKOFFSET) / 4 * uv_width, 2);

        if (d < SAD)
          {
            matrix[x][y][0] = qx * 2;
            matrix[x][y][1] = qy * 2;
            SAD = d;
          }
        if (qx == 0 && qy == 0)
          {
            CAD = d;
          }
      }

  if (SAD > CAD)
    {
      matrix[x][y][0] = bx * 2;
      matrix[x][y][1] = bx * 2;
    }
}

void
mb_search (int x, int y, uint8_t *ref_frame[3], uint8_t *tgt_frame[3])
{
  int  qy, qx;
  uint32_t d;
  uint32_t CAD = (256 * BLOCKSIZE * BLOCKSIZE);
  uint32_t SAD = (256 * BLOCKSIZE * BLOCKSIZE);
  int bx;
  int by;

  bx = matrix[x][y][0] / 2;
  by = matrix[x][y][1] / 2;

  for (qy = (by - 1); qy <= (by + 1); qy++)
    for (qx = (bx - 1); qx <= (bx + 1); qx++)
      {
        d = calc_SAD (tgt_frame[0],
                      ref_frame[0],
                      (x + qx - BLOCKOFFSET) + (y + qy - BLOCKOFFSET) * width,
                      (x - BLOCKOFFSET) + (y - BLOCKOFFSET) * width, 1);

        d += calc_SAD_uv (tgt_frame[1],
                          ref_frame[1],
                          (x + qx - BLOCKOFFSET) / 2 + (y + qy) / 2 * uv_width,
                          (x - BLOCKOFFSET) / 2 + (y) / 2 * uv_width, 1);

        d += calc_SAD_uv (tgt_frame[2],
                          ref_frame[2],
                          (x + qx - BLOCKOFFSET) / 2 + (y + qy -
                                                        BLOCKOFFSET) / 2 * uv_width,
                          (x - BLOCKOFFSET) / 2 + (y - BLOCKOFFSET) / 2 * uv_width, 1);

        if (d < SAD)
          {
            matrix[x][y][0] = qx * 2;
            matrix[x][y][1] = qy * 2;
            SAD = d;
          }

        if (qx == 0 && qy == 0)
          {
            CAD = d;
          }
      }

  if (SAD > CAD)
    {
      matrix[x][y][0] = 0;
      matrix[x][y][1] = 0;
      SAD = CAD;
    }

}

/* half pixel search seem's definetly not to be needed ... hmm */
#if 0
void
mb_search_half (int x, int y)
{
  int dy, dx, qy, qx, xx, yy, x0, x1, y0, y1;
  uint32_t d;
  int Y, U, V;
  int bx;
  int by;
  float sx, sy;
  float a0, a1, a2, a3;
  uint32_t old_SQD;
  int i;

  SAD[0] = 10000000;
  SAD[1] = 10000000;
  SAD[2] = 10000000;
  SAD[3] = 10000000;

  for (i = 0; i < 4; i++)
    {
      bx = best_match_x[i];
      by = best_match_y[i];

      for (qy = (by - 1); qy <= (by + 1); qy++)
        for (qx = (bx - 1); qx <= (bx + 1); qx++)
          {
            sx = (qx - (qx & ~1)) * 0.5;
            sy = (qy - (qy & ~1)) * 0.5;

            a0 = (1 - sx) * (1 - sy);
            a1 = (sx) * (1 - sy);
            a2 = (1 - sx) * (sy);
            a3 = (sx) * (sy);

            xx = x + qx / 2;    /* Keeps some calc. out of the MB-search-loop... */
            yy = y + qy / 2;

            d = 0;
            for (dy = -2; dy < 6; dy++)
              for (dx = -2; dx < 6; dx++)
                {
                  x0 = xx + dx;
                  x1 = x0 - 1;
                  y0 = (yy + dy) * width;
                  y1 = (yy + dy - 1) * width;

                  /* we only need Y for half-pels... */
                  Y = (AVRG[(x0) + (y0)] * a0 +
                       AVRG[(x1) + (y0)] * a1 +
                       AVRG[(x0) + (y1)] * a2 +
                       AVRG[(x1) + (y1)] * a3) - YUV1[(x + dx) + (y + dy) * width];

                  d += (Y < 0) ? -Y : Y;        /* doch mal einen SAD... nur testweise ... */
                }

            if (d < SAD[i])
              {
                best_match_x[i] = qx;
                best_match_y[i] = qy;
                SAD[i] = d;
              }
          }
    }
}
#endif

void
copy_filtered_block (int x, int y, uint8_t *dest[3], uint8_t *srce[3])
{
  int dx, dy;
  int qx = matrix[x][y][0];
  int qy = matrix[x][y][1];
  int xx, yy, x2, y2;
  float sx, sy;
  float a0, a1, a2, a3;

  sx = (qx - (qx & ~1)) * 0.5;
  sy = (qy - (qy & ~1)) * 0.5;
  qx /= 2;
  qy /= 2;

  a0 = (1 - sx) * (1 - sy);
  a1 = (sx) * (1 - sy);
  a2 = (1 - sx) * (sy);
  a3 = (sx) * (sy);

  for (dy = 0; dy < (BLOCKSIZE / 2); dy++)
    for (dx = 0; dx < (BLOCKSIZE / 2); dx++)
      {
        xx = x + dx;
        yy = y + dy;
        x2 = xx >> 1;
        y2 = yy / 2 * uv_width;

        if (sx != 0 || sy != 0)

          *(dest[0] + (xx) + (yy) * width) =
            (*(srce[0] + (xx + qx - 0) + (yy + qy - 0) * width) * a0 +
             *(srce[0] + (xx + qx - 1) + (yy + qy - 0) * width) * a1 +
             *(srce[0] + (xx + qx - 0) + (yy + qy - 1) * width) * a2 +
             *(srce[0] + (xx + qx - 1) + (yy + qy - 1) * width) * a3) * (1 -
                                                                         block_quality) +
            *(yuv[0] + (xx) + (yy) * width) * block_quality;

        else
          *(dest[0] + (xx) + (yy) * width) =
            *(srce[0] + (xx + qx) + (yy + qy) * width) * (1 - block_quality) +
            *(yuv[0] + (xx) + (yy) * width) * block_quality;

        /* U */
        *(dest[1] + (xx) / 2 + (yy) / 2 * uv_width) =
          *(srce[1] + (xx + qx) / 2 + (yy + qy) / 2 * uv_width) * (1 - block_quality) +
          *(yuv[1] + (xx) / 2 + (yy) / 2 * uv_width) * block_quality;

        /* V */
        *(dest[2] + (xx) / 2 + (yy) / 2 * uv_width) =
          *(srce[2] + (xx + qx) / 2 + (yy + qy) / 2 * uv_width) * (1 - block_quality) +
          *(yuv[2] + (xx) / 2 + (yy) / 2 * uv_width) * block_quality;
      }
}

void
antiflicker_reference (uint8_t *frame[3])
{
int x,y;

  for(y=0;y<(uv_height-1);y++)
    for(x=0;x<(uv_width-1);x++)
    {
      *(frame[1]+x+y*uv_width)=
        (
          *(frame[1]+(x+0)+(y+0)*uv_width)+
          *(frame[1]+(x+1)+(y+0)*uv_width)+
          *(frame[1]+(x+0)+(y+1)*uv_width)+
          *(frame[1]+(x+1)+(y+1)*uv_width)
        )/4;      

      *(frame[2]+x+y*uv_width)=
        (
          *(frame[2]+(x+0)+(y+0)*uv_width)+
          *(frame[2]+(x+1)+(y+0)*uv_width)+
          *(frame[2]+(x+0)+(y+1)*uv_width)+
          *(frame[2]+(x+1)+(y+1)*uv_width)
        )/4;      

    }  
}


void
line (uint8_t *image, int x0, int y0, int x1, int y1)
{
  int x, y;
  float sx, sy;

  sx = ((x1 - x0) > 0) ? 1 : -1;
  sy = (float) (y1 - y0) / (float) (x1 - x0);

  for (x = x0; x != x1; x += sx)
    {
      y = sy * (x - x0) + y0;
      *(image + x + y * width) = 200;
    }
}


/*********************************************************** 
 *                                                         * 
 * Hirarchical Motion-Search-Algorithm                     * 
 *                                                         * 
 * The resulting motion vectors are stored in the global   * 
 * array matrix[x][y][d].                                  * 
 *                                                         * 
 * Hirarchical means, that a so called full-search is per- * 
 * formed on one or more subsampled frames first, to re-   * 
 * duce the computational effort. The result is known to   * 
 * be as good as if a real full-search was done.           * 
 *                                                         * 
 ***********************************************************/

void
calculate_motion_vectors (uint8_t *ref_frame[3], uint8_t *target[3])
{
  int x, y, dx, dy;
  uint32_t center_SAD;
  uint32_t vector_SAD;
  uint32_t sum_of_SADs;
  uint32_t avrg_SAD;
  int nr_of_blocks = 0;
  static uint32_t last_mean_SAD;

  /* subsample reference frame by 2 and by 4                 */
  /* store subsampled frame in sub_r[3]                      */
  subsample_frame (sub_r2, ref_frame);
  subsample_frame (sub_r4, sub_r2);

  /* subsample target frame by 2 and by 4                    */
  /* store subsampled frame in sub_t[3]                      */
  subsample_frame (sub_t2, target);
  subsample_frame (sub_t4, sub_t2);

 /***********************************************************
  * finally we calculate the motion-vectors                 *
  *                                                         *
  * the search is performed with a searchblock of BLOCKSIZE *
  * and is overlapped by a factor of 2 (BLOCKSIZE/2)        *
  ***********************************************************/

  /* reset SAD accumulator ... */
  sum_of_SADs = 0;

  /* devide the frame in blocks ... */
  for (y = 0; y < height; y += (BLOCKSIZE / 2))
    for (x = 0; x < width; x += (BLOCKSIZE / 2))
      {
        nr_of_blocks++;

        /* search best matching block in the 4x4 subsampled
         * image and store the result in matrix[x][y][d]
         */
        mb_search_44 (x, y, sub_r4, sub_t4);

        /* search best matching block in the 2x2 subsampled
         * image and store the result in matrix[x][y][d]
         */
        mb_search_22 (x, y, sub_r2, sub_t2);

        /* based on that 2x2 search we start a search in the
         * real frame and store the result in matrix[x][y][d]
         */
        mb_search (x, y, ref_frame, target);

        /* now, we need(!!) to check if the vector is valid...  
         * to do so, the center-SAD is calculated and compared  
         * with the vectors-SAD. If it's not better by at least 
         * a factor of 2 it's discarded...                      
         */

        dx = matrix[x][y][0] / 2;
        dy = matrix[x][y][1] / 2;

        center_SAD = calc_SAD (target[0],
                               ref_frame[0], (x) + (y) * width, (x) + (y) * width, 2);


        center_SAD += calc_SAD_uv (target[1],
                                   ref_frame[1],
                                   (x) / 2 + (y) / 2 * uv_width,
                                   (x) / 2 + (y) / 2 * uv_width, 2);

        center_SAD += calc_SAD_uv (target[2],
                                   ref_frame[2],
                                   (x) / 2 + (y) / 2 * uv_width,
                                   (x) / 2 + (y) / 2 * uv_width, 2);


        vector_SAD = calc_SAD (target[0],
                               ref_frame[0],
                               (x + dx) + (y + dy) * width, (x) + (y) * width, 2);

        vector_SAD += calc_SAD_uv (target[1],
                                   ref_frame[1],
                                   (x + dx) / 2 + (y + dy) / 2 * uv_width,
                                   (x) / 2 + (y) / 2 * uv_width, 2);

        vector_SAD += calc_SAD_uv (target[2],
                                   ref_frame[2],
                                   (x + dx) / 2 + (y + dy) / 2 * uv_width,
                                   (x) / 2 + (y) / 2 * uv_width, 2);


        if (vector_SAD > (center_SAD * 0.85))
          {
            vector_SAD = center_SAD;
            matrix[x][y][0] = 0;
            matrix[x][y][1] = 0;
          }

        SAD_matrix[x][y] = vector_SAD;
        sum_of_SADs += vector_SAD;
      }

  if (framenr == 0)
    {
      last_mean_SAD = 100;
      mean_SAD = 100;
    }

  avrg_SAD = sum_of_SADs / nr_of_blocks;

  last_mean_SAD = mean_SAD;
  mean_SAD = mean_SAD * 0.99 + avrg_SAD * 0.01;

  if (avrg_SAD > (mean_SAD * 2) && framenr++ > 12)
    {
      mean_SAD = last_mean_SAD;
      scene_change = 1;
      mjpeg_log (LOG_INFO, " *** Scene Change ***\n");
    }
  else
    {
      scene_change = 0;
    }

}

void
transform_frame (uint8_t *avg[3])
{
  int x, y;

  mjpeg_log (LOG_INFO, " MSAD : %d \n", mean_SAD);

  for (y = 0; y < height; y += (BLOCKSIZE / 2))
    for (x = 0; x < width; x += (BLOCKSIZE / 2))
      {
        if (SAD_matrix[x][y] < (mean_SAD))
          block_quality = 0;
        else if (SAD_matrix[x][y] < (mean_SAD * 3))
          block_quality = (float) (mean_SAD+SAD_matrix[x][y]) / (float) (mean_SAD*2) - 1;
        else
          block_quality = 1;

        copy_filtered_block (x, y, yuv1, avg);

#if DEBUG
        mjpeg_log (LOG_DEBUG, "block_quality : %f \n", block_quality);
#endif
      }

  memcpy (avg[0], yuv1[0], width * height);
  memcpy (avg[1], yuv1[1], width * height / 4);
  memcpy (avg[2], yuv1[2], width * height / 4);
}

void
denoise_frame (uint8_t *frame[3])
{

 /***********************************************************
  *                                                         *
  * ! MAIN PROCESSING LOOP !                                *
  *                                                         *
  * The denoising process is done the following way:        *
  *                                                         *
  * 1. Take the reference frame and calculate the motion-   *
  *    vectors to the corresponding previously filtered     *
  *    frame blocks in avrg[x].                             *
  *                                                         *
  * 2. Transform the previous filtered image with these     *
  *    motion vectors to match the current reference frame. *
  *                                                         *
  * 3. Blend over the transformed image with the actual     *
  *    reference frame.                                     *
  *                                                         *
  ***********************************************************/

  static int uninitialized = 1;

  /* if uninitialized copy reference frame into average buffer */
  if (uninitialized)
    {
      uninitialized = 0;

      memcpy (avrg[0], frame[0], width * height);
      memcpy (avrg[1], frame[1], width * height / 4);
      memcpy (avrg[2], frame[2], width * height / 4);
    }

  /* Find the motion vectors between reference frame (ref[x]) */
  /* and previous averaged frame (avrg[x]) and store the     */
  /* resulting vectors in matrix[x][y][d].                   */

  calculate_motion_vectors (frame, avrg);

  /* Transform the previously filtered image to match the    */
  /* reference frame                                         */

  transform_frame (avrg);

  /* Blend reference frame with average frame                */
  average_frames (frame, avrg);

  /* Scene Change detected ? */
  if (scene_change)
    {
      memcpy (avrg[0], frame[0], width * height);
      memcpy (avrg[1], frame[1], width * height / 4);
      memcpy (avrg[2], frame[2], width * height / 4);
    }
  else
    {
      /* Copy filtered frame into frame[x] ... */
      memcpy (frame[0], avrg[0], width * height);
      memcpy (frame[1], avrg[1], width * height / 4);
      memcpy (frame[2], avrg[2], width * height / 4);
    }
}
