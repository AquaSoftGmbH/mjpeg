
/*
 * Yuv-Denoiser to be used with Andrew Stevens mpeg2enc
 *
 * (C)2001 S.Fendt 
 *
 * Licensed and protected by the GNU-General-Public-License version 2 
 * (or if you prefer any later version of that license). See the file 
 * LICENSE for detailed information.
 *
 */

/* I really, really should clean this mess up ... *sigh* */

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include "mjpeg_types.h"
#include "yuv4mpeg.h"
#include "mjpeg_logging.h"
#include "config.h"

/* #define DEBUG */
#define BLOCKSIZE   8

#define BLOCKOFFSET (BLOCKSIZE-(BLOCKSIZE/2))/2

uint8_t YUV1[(int) (768 * 576 * 1.5)];
uint8_t YUV2[(int) (768 * 576 * 1.5)];
uint8_t YUV3[(int) (768 * 576 * 1.5)];
uint8_t YUV4[(int) (768 * 576 * 1.5)];
uint8_t SUBO2[(int) (768 * 576 * 1.5)];
uint8_t SUBD2[(int) (768 * 576 * 1.5)];
uint8_t SUBA2[(int) (768 * 576 * 1.5)];
uint8_t SUBO4[(int) (768 * 576 * 1.5)];
uint8_t SUBD4[(int) (768 * 576 * 1.5)];
uint8_t SUBA4[(int) (768 * 576 * 1.5)];
uint8_t SUBO8[(int) (768 * 576 * 1.5)];
uint8_t SUBA8[(int) (768 * 576 * 1.5)];
uint8_t DEL0[(int) (768 * 576 * 1.5)];
uint8_t DEL1[(int) (768 * 576 * 1.5)];
uint8_t DEL2[(int) (768 * 576 * 1.5)];
uint8_t DEL3[(int) (768 * 576 * 1.5)];
uint8_t DEL4[(int) (768 * 576 * 1.5)];
uint8_t DEL5[(int) (768 * 576 * 1.5)];
uint8_t DEL6[(int) (768 * 576 * 1.5)];
uint8_t DEL7[(int) (768 * 576 * 1.5)];
uint8_t DEL8[(int) (768 * 576 * 1.5)];
uint8_t AVRG[(int) (768 * 576 * 1.5)];

unsigned char *yuv[3];
unsigned char *yuv1[3];
unsigned char *yuv2[3];
unsigned char *avrg[3];
unsigned char *sub_r2[3];
unsigned char *sub_t2[3];
unsigned char *sub_r4[3];
unsigned char *sub_t4[3];

uint32_t framenr = 0;

int deinterlace = 0;            /* set to 1 if deinterlacing needed */
int scene_change = 0;            /* set to 1 if deinterlacing needed */
int width;                      /* width of the images */
int height;                     /* height of the images */
int u_offset;                   /* offset of the U-component from beginning of the image */
int v_offset;                   /* offset of the V-component from beginning of the image */
int uv_width;                   /* width of the UV-components */
int uv_height;                  /* height of the UV-components */
int best_match_x[4];            /* best block-match's X-coordinate in half-pixels */
int best_match_y[4];            /* best block-match's Y-coordinate in half-pixels */
uint32_t SAD[4];                /* best block-match's sum of absolute differences */
uint32_t CSAD;                  /* best block-match's sum of absolute differences */
uint32_t SQD;                   /* best block-match's sum of absolute differences */
uint32_t YSQD;                  /* best block-match's sum of absolute differences */
uint32_t USQD;                  /* best block-match's sum of absolute differences */
uint32_t VSQD;                  /* best block-match's sum of absolute differences */
uint32_t CQD;                   /* center block-match's sum of absolute differences */
uint32_t init_SQD = 0;
uint32_t mean_SAD;
int lum_delta;
int cru_delta;
int crv_delta;
int search_radius = 48;         /* initial search-radius in half-pixels */
int border = -1;
double block_quality;
float sharpen = 0;

/* matrix storing the motion vectors                       */

/*                                                         */

/* REMARK: I don't have a clue why I should use a malloc   */

/*         here... Will we ever encounter frames bigger    */

/*         than this ?!?                                   */

int matrix[1024][768][2];
uint32_t SAD_matrix[1024][768];

y4m_frame_info_t *frameinfo = NULL;
y4m_stream_info_t *streaminfo = NULL;

float a[16];
int i;
float v;

void denoise_image ();
void detect_black_borders ();
void mb_search_88 (int x, int y);

//void mb_search (int x, int y);
void subsample_reference_image2 ();
void subsample_averaged_image2 ();
void subsample_reference_image4 ();
void subsample_averaged_image4 ();
void delay_images ();
void time_average_images ();
void display_greeting ();
void display_help ();

//void copy_filtered_block (int x, int y);

void deinterlace_frame (unsigned char *yuv[3]);
void denoise_frame (unsigned char *ref[3]);

void
test_line ()
{
  static int x;
  int y;

  int xx, yy;

  for (yy = 0; yy < height; yy++)
    for (xx = 0; xx < width; xx++)
      {
        YUV1[xx + yy * width] = 0;
        YUV1[xx / 2 + yy / 2 * uv_width + u_offset] = 128;
        YUV1[xx / 2 + yy / 2 * uv_width + v_offset] = 128;
      }

  for (y = 0; y < (height / 2); y++)
    {
      YUV1[0 + x + y * width] = 155;
      YUV1[3 + x + y * width] = 155;
      YUV1[4 + x + y * width] = 155;
      YUV1[5 + x + y * width] = 155;
      YUV1[8 + x + y * width] = 155;
    }

  x += 2;
  x = (x > (width / 2)) ? 0 : x;
}

int
main (int argc, char *argv[])
{
  float q;
  int fd_in = 0;
  int fd_out = 1;
  int errnr;
  int norm;
  uint8_t magic[256];
  char c;
  int x, y, v1, v2;

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

  yuv[0] = malloc (width * height * sizeof (unsigned char));
  yuv[1] = malloc (width * height * sizeof (unsigned char) / 4);
  yuv[2] = malloc (width * height * sizeof (unsigned char) / 4);

  yuv1[0] = malloc (width * height * sizeof (unsigned char));
  yuv1[1] = malloc (width * height * sizeof (unsigned char) / 4);
  yuv1[2] = malloc (width * height * sizeof (unsigned char) / 4);

  yuv2[0] = malloc (width * height * sizeof (unsigned char));
  yuv2[1] = malloc (width * height * sizeof (unsigned char) / 4);
  yuv2[2] = malloc (width * height * sizeof (unsigned char) / 4);

  avrg[0] = malloc (width * height * sizeof (unsigned char));
  avrg[1] = malloc (width * height * sizeof (unsigned char) / 4);
  avrg[2] = malloc (width * height * sizeof (unsigned char) / 4);

  sub_t2[0] = malloc (width * height * sizeof (unsigned char));
  sub_t2[1] = malloc (width * height * sizeof (unsigned char) / 4);
  sub_t2[2] = malloc (width * height * sizeof (unsigned char) / 4);

  sub_r2[0] = malloc (width * height * sizeof (unsigned char));
  sub_r2[1] = malloc (width * height * sizeof (unsigned char) / 4);
  sub_r2[2] = malloc (width * height * sizeof (unsigned char) / 4);

  sub_t4[0] = malloc (width * height * sizeof (unsigned char));
  sub_t4[1] = malloc (width * height * sizeof (unsigned char) / 4);
  sub_t4[2] = malloc (width * height * sizeof (unsigned char) / 4);

  sub_r4[0] = malloc (width * height * sizeof (unsigned char));
  sub_r4[1] = malloc (width * height * sizeof (unsigned char) / 4);
  sub_r4[2] = malloc (width * height * sizeof (unsigned char) / 4);

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

void
display_help ()
{
  fprintf (stdout, "sorry no help this time\n");
  exit (0);
}

void
deinterlace_frame (unsigned char *yuv[3])
{

  /***********************************************************************/
  /* Line-Deinterlacer                                                   */
  /* 2001 S.Fendt                                                        */
  /* it does a better job than just dropping lines but sometimes it      */
  /* fails and reduces y-resolution... hmm... but everything else is     */
  /* supposed to fail, too, sometimes, and is much more time consuming.  */

  /***********************************************************************/

  /* Buffer one line */
  unsigned char line[1024];
  unsigned int d;
  unsigned int min;
  int x;
  int y;
  int xx;
  int i;
  int xpos;
  int l1;
  int l2;
  int lumadiff;

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

/* OLD */
#if 0
void
time_average_images ()
{
  int x, y;

  for (y = 0; y < height; y++)
    for (x = 0; x < width; x++)
      {
        AVRG[x + y * width] =
          (uint16_t) (DEL1[x + y * width] +
                      DEL2[x + y * width] +
                      DEL3[x + y * width] +
                      DEL4[x + y * width] +
                      DEL5[x + y * width] +
                      DEL6[x + y * width] +
                      DEL7[x + y * width] + DEL8[x + y * width]) >> 3;
      }
  for (y = 0; y < uv_height; y++)
    for (x = 0; x < uv_width; x++)
      {
        AVRG[x + y * uv_width + u_offset] =
          (uint16_t) (DEL1[x + y * uv_width + u_offset] +
                      DEL2[x + y * uv_width + u_offset] +
                      DEL3[x + y * uv_width + u_offset] +
                      DEL4[x + y * uv_width + u_offset] +
                      DEL5[x + y * uv_width + u_offset] +
                      DEL6[x + y * uv_width + u_offset] +
                      DEL7[x + y * uv_width + u_offset] +
                      DEL8[x + y * uv_width + u_offset]) >> 3;
        AVRG[x + y * uv_width + v_offset] =
          (uint16_t) (DEL1[x + y * uv_width + v_offset] +
                      DEL2[x + y * uv_width + v_offset] +
                      DEL3[x + y * uv_width + v_offset] +
                      DEL4[x + y * uv_width + v_offset] +
                      DEL5[x + y * uv_width + v_offset] +
                      DEL6[x + y * uv_width + v_offset] +
                      DEL7[x + y * uv_width + v_offset] +
                      DEL8[x + y * uv_width + v_offset]) >> 3;
      }
}
#endif

void
average_frames (unsigned char *ref[3], unsigned char *avg[3])
{

  /****************************************************/
  /* takes a frame and blends it into the average     */
  /* buffer.                                          */
  /* The blend may not be to slow, as noise wouldn't  */
  /* be filtered good than, but it may not be to fast */
  /* either... I decided to take approx 5 frames into */
  /* account. Probably need's some finetuning ...     */

  /****************************************************/

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

void
time_average_images ()
{
  int x, y;

  for (y = 0; y < height; y++)
    for (x = 0; x < width; x++)
      {
        AVRG[x + y * width] = (YUV2[x + y * width] * 3 + YUV1[x + y * width] * 1) / 4;
      }
  for (y = 0; y < uv_height; y++)
    for (x = 0; x < uv_width; x++)
      {
        AVRG[x + y * uv_width + u_offset] =
          (YUV2[x + y * uv_width + u_offset] * 3 +
           YUV1[x + y * uv_width + u_offset] * 1) / 4;

        AVRG[x + y * uv_width + v_offset] =
          (YUV2[x + y * uv_width + v_offset] * 3 +
           YUV1[x + y * uv_width + v_offset] * 1) / 4;
      }
}

void
subsample_frame (unsigned char *dst[3], unsigned char *src[3])
{

  /******************************************************/
  /* generate a lowpassfiltered and subsampled copy     */
  /* of the source image (src) at the destination       */
  /* image location.                                    */
  /* Lowpass-filtering is important, as this subsampled */
  /* image is used for motion estimation and the result */
  /* is more reliable when filtered.                    */

  /******************************************************/

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

#if 0
subsample_image4 (uint8_t * dest, uint8_t * source)
{
  int x, y;

  /* subsample Y */
  for (y = 0; y < height; y += 4)
    for (x = 0; x < width; x += 4)
      {
        dest[(x / 4) + (y / 4) * width] =
          (source[(x + 0) + (y + 0) * width] +
           source[(x + 1) + (y + 0) * width] +
           source[(x + 2) + (y + 0) * width] +
           source[(x + 3) + (y + 0) * width] +
           source[(x + 0) + (y + 1) * width] +
           source[(x + 1) + (y + 1) * width] +
           source[(x + 2) + (y + 1) * width] +
           source[(x + 3) + (y + 1) * width] +
           source[(x + 0) + (y + 2) * width] +
           source[(x + 1) + (y + 2) * width] +
           source[(x + 2) + (y + 2) * width] +
           source[(x + 3) + (y + 2) * width] +
           source[(x + 0) + (y + 3) * width] +
           source[(x + 1) + (y + 3) * width] +
           source[(x + 2) + (y + 3) * width] + source[(x + 3) + (y + 3) * width]) >> 4;
      }

  for (y = 0; y < uv_height; y += 4)
    for (x = 0; x < uv_width; x += 4)
      {
        /* U */
        dest[(x / 4) + (y / 4) * uv_width + u_offset] =
          (source[(x + 0) + (y + 0) * uv_width + u_offset] +
           source[(x + 1) + (y + 0) * uv_width + u_offset] +
           source[(x + 2) + (y + 0) * uv_width + u_offset] +
           source[(x + 3) + (y + 0) * uv_width + u_offset] +
           source[(x + 0) + (y + 1) * uv_width + u_offset] +
           source[(x + 1) + (y + 1) * uv_width + u_offset] +
           source[(x + 2) + (y + 1) * uv_width + u_offset] +
           source[(x + 3) + (y + 1) * uv_width + u_offset] +
           source[(x + 0) + (y + 2) * uv_width + u_offset] +
           source[(x + 1) + (y + 2) * uv_width + u_offset] +
           source[(x + 2) + (y + 2) * uv_width + u_offset] +
           source[(x + 3) + (y + 2) * uv_width + u_offset] +
           source[(x + 0) + (y + 3) * uv_width + u_offset] +
           source[(x + 1) + (y + 3) * uv_width + u_offset] +
           source[(x + 2) + (y + 3) * uv_width + u_offset] +
           source[(x + 3) + (y + 3) * uv_width + u_offset]) >> 4;

        /* V */
        dest[(x / 4) + (y / 4) * uv_width + v_offset] =
          (source[(x + 0) + (y + 0) * uv_width + v_offset] +
           source[(x + 1) + (y + 0) * uv_width + v_offset] +
           source[(x + 2) + (y + 0) * uv_width + v_offset] +
           source[(x + 3) + (y + 0) * uv_width + v_offset] +
           source[(x + 0) + (y + 1) * uv_width + v_offset] +
           source[(x + 1) + (y + 1) * uv_width + v_offset] +
           source[(x + 2) + (y + 1) * uv_width + v_offset] +
           source[(x + 3) + (y + 1) * uv_width + v_offset] +
           source[(x + 0) + (y + 2) * uv_width + v_offset] +
           source[(x + 1) + (y + 2) * uv_width + v_offset] +
           source[(x + 2) + (y + 2) * uv_width + v_offset] +
           source[(x + 3) + (y + 2) * uv_width + v_offset] +
           source[(x + 0) + (y + 3) * uv_width + v_offset] +
           source[(x + 1) + (y + 3) * uv_width + v_offset] +
           source[(x + 2) + (y + 3) * uv_width + v_offset] +
           source[(x + 3) + (y + 3) * uv_width + v_offset]) >> 4;

      }
}
#endif

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

uint32_t
calc_SQD (char *frm, char *ref, uint32_t frm_offs, uint32_t ref_offs, int div)
{
  int dx, dy, Y;
  uint32_t d = 0;

  for (dy = 0; dy < BLOCKSIZE / div; dy++)
    for (dx = 0; dx < BLOCKSIZE / div; dx++)
      {
        Y =
          *(frm + frm_offs + (dx) + (dy) * width) -
          *(ref + ref_offs + (dx) + (dy) * width);

        d += Y * Y;
      }
  return d;
}

uint32_t
calc_SQD_uv (uint8_t * frm, uint8_t * ref, uint32_t frm_offs, uint32_t ref_offs, int div)
{
  int dx = 0, dy = 0, Y;
  uint32_t d = 0;
  static uint32_t old_frm_offs;
  static uint32_t old_d;

  if (old_frm_offs != frm_offs)
    {
      for (dy = 0; dy < (BLOCKSIZE / 2) / div; dy++)
        for (dx = 0; dx < (BLOCKSIZE / 2) / div; dx++)
          {
            Y =
              *(frm + frm_offs + (dx) + (dy) * uv_width) -
              *(ref + ref_offs + (dx) + (dy) * uv_width);

            d += Y * Y;
          }
      old_d = d;
      return d;
    }
  else
    {
      return old_d;
    }
}


void
mb_search_44 (int x, int y, unsigned char *ref_frame[3], unsigned char *tgt_frame[3])
{
  int dy, dx, qy, qx;
  uint32_t d;
  uint32_t CAD;
  int Y, U, V;
  int bx;
  int by;
  int i;

  SAD[0] = (256 * BLOCKSIZE * BLOCKSIZE);

  /* search_radius has to be devided by 8 as radius is given in */
  /* half-pixels and image is reduced in resolution by 4 ...    */

  for (qy = -search_radius / 8; qy <= search_radius / 8; qy += 4)
    for (qx = -search_radius / 8; qx <= search_radius / 8; qx += 4)
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

        if (d < SAD[0])
          {
            matrix[x][y][0] = qx * 2;
            matrix[x][y][1] = qy * 2;
            SAD[0] = d;
          }

        if (qx == 0 && qy == 0)
          {
            CAD = d;
          }

      }

  if (SAD[0] > CAD)
    {
      matrix[x][y][0] = 0;
      matrix[x][y][1] = 0;
    }
}

void
mb_search_22 (int x, int y, unsigned char *ref_frame[3], unsigned char *tgt_frame[3])
{
  int dy, dx, qy, qx;
  uint32_t d;
  uint32_t CAD;
  int Y, U, V;
  int bx;
  int by;
  int i;

  SAD[0] = (256 * BLOCKSIZE * BLOCKSIZE);
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

        if (d < SAD[0])
          {
            matrix[x][y][0] = qx * 2;
            matrix[x][y][1] = qy * 2;
            SAD[0] = d;
          }
        if (qx == 0 && qy == 0)
          {
            CAD = d;
          }
      }

  if (SAD[0] > CAD)
    {
      matrix[x][y][0] = bx * 2;
      matrix[x][y][1] = bx * 2;
//        SAD[0]=CAD;
    }
}

void
mb_search (int x, int y, unsigned char *ref_frame[3], unsigned char *tgt_frame[3])
{
  int dy, dx, qy, qx;
  uint32_t d, du, dv;
  uint32_t CAD;
  int Y, U, V;
  int bx;
  int by;

  SAD[0] = (256 * BLOCKSIZE * BLOCKSIZE);
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

        if (d < SAD[0])
          {
            matrix[x][y][0] = qx * 2;
            matrix[x][y][1] = qy * 2;
            SAD[0] = d;
          }

        if (qx == 0 && qy == 0)
          {
            CAD = d;
          }
      }

  if (SAD[0] > CAD)
    {
      matrix[x][y][0] = 0;
      matrix[x][y][1] = 0;
      SAD[0] = CAD;
    }

}

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

void
copy_filtered_block (int x, int y, unsigned char *dest[3], unsigned char *srce[3])
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

#if 0
void
copy_reference_block (int x, int y)
{
  int dx, dy;
  int xx, yy;

  for (dy = 0; dy < (BLOCKSIZE); dy++)
    for (dx = 0; dx < (BLOCKSIZE); dx++)
      {
        xx = x + dx;
        yy = (y + dy) * width;

        /* Y */
        YUV2[(xx) + (yy)] = YUV1[(xx) + (yy)];

        /* U */
        xx = (x + dx) / 2 + u_offset;
        yy = (y + dy) / 2 * uv_width;

        YUV2[(xx) + (yy)] = YUV1[(xx) + (yy)];

        xx += u_offset >> 2;

        YUV2[(xx) + (yy)] = YUV1[(xx) + (yy)];
      }
}
#endif
void
copy_reference_block (int x, int y)
{
  int dx, dy;
  int xx, yy;

  for (dy = 0; dy < (BLOCKSIZE / 2); dy++)
    {
      /* Y */
      memcpy (YUV2 + x + (y + dy) * width, YUV1 + x + (y + dy) * width, BLOCKSIZE / 2);

      xx = x / 2;
      yy = (y + dy) / 2;

      /* U */
      memcpy (YUV2 + u_offset + xx + yy * uv_width,
              YUV1 + u_offset + xx + yy * uv_width, BLOCKSIZE / 4);

      /* V */
      memcpy (YUV2 + v_offset + xx + yy * uv_width,
              YUV1 + v_offset + xx + yy * uv_width, BLOCKSIZE / 4);
    }
}

void
detect_black_borders ()
{
  int x, y, z;
  static int tb = 0;

  {
    if (border != -1)
      tb = border;
    if (border == 0)
      tb = 0;

    if (tb != 0)
      for (y = 0; y < tb; y++)
        for (x = 0; x < width; x++)
          {
            YUV2[x + y * width] = 16;
            YUV2[x / 2 + y / 2 * uv_width + u_offset] = 128;
            YUV2[x / 2 + y / 2 * uv_width + v_offset] = 128;

            z = height - y;
            YUV2[x + z * width] = 16;
            YUV2[x / 2 + z / 2 * uv_width + u_offset] = 128;
            YUV2[x / 2 + z / 2 * uv_width + v_offset] = 128;
          }
  }
}


void
antiflicker_reference ()
{
  int x, y;
  int v1, v2, v3, v4;

  for (y = 0; y < (height); y++)
    for (x = 0; x < (width); x++)
      {
        v1 = YUV1[x / 2 + y / 2 * uv_width + u_offset];
        v2 = YUV1[x / 2 + y / 2 * uv_width + v_offset];
        v3 = YUV3[x / 2 + y / 2 * uv_width + u_offset];
        v4 = YUV3[x / 2 + y / 2 * uv_width + v_offset];

        YUV1[x / 2 + y / 2 * uv_width + u_offset] = (v1 + v3) / 2;
        YUV1[x / 2 + y / 2 * uv_width + v_offset] = (v2 + v4) / 2;
        YUV3[x / 2 + y / 2 * uv_width + u_offset] = (v1);
        YUV3[x / 2 + y / 2 * uv_width + v_offset] = (v2);

      }

}

int
block_change (int x, int y)
{
  uint32_t d = 0;

  d = calc_SQD (yuv[0], avrg[0], (x) + (y) * width, (x) + (y) * width, 2);

  return (d <= (4 * BLOCKSIZE * BLOCKSIZE)) ? 0 : 1;
}

void
delay_image ()
{
  memcpy (DEL0, YUV1, (size_t) width * height * 1.5);
  //memcpy (AVRG, YUV1, (size_t) width * height * 1.5);
}

void
line (unsigned char *image, int x0, int y0, int x1, int y1)
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

int
good_contrast_block (x, y)
{
  int xx, yy;
  int v1 = 0, v2 = 255;

  for (yy = -BLOCKSIZE; yy < BLOCKSIZE * 2; yy++)
    for (xx = -BLOCKSIZE; xx < BLOCKSIZE * 2; xx++)
      {
        v1 = (v1 < *(YUV1 + x + xx + (y + yy) * width)) ?
          *(YUV1 + x + xx + (y + yy) * width) : v1;
        v2 = (v2 > *(YUV1 + x + xx + (y + yy) * width)) ?
          *(YUV1 + x + xx + (y + yy) * width) : v1;
      }
  v1 = fabs (v1 - v2);
  return (v1 > 32) ? 1 : 0;
}

void
calculate_motion_vectors (unsigned char *ref_frame[3], unsigned char *target[3])
{

  /***********************************************************/
  /*                                                         */
  /* Hirarchical Motion-Search-Algorithm                     */
  /*                                                         */
  /* The resulting motion vectors are stored in the global   */
  /* array matrix[x][y][d].                                  */
  /*                                                         */
  /* Hirarchical means, that a so called full-search is per- */
  /* formed on one or more subsampled frames first, to re-   */
  /* duce the computational effort. The result is known to   */
  /* be as good as if a real full-search was done.           */
  /*                                                         */

  /***********************************************************/

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

  /***********************************************************/
  /* finally we calculate the motion-vectors                 */
  /*                                                         */
  /* the search is performed with a searchblock of BLOCKSIZE */
  /* and is overlapped by a factor of 2 (BLOCKSIZE/2)        */

  /***********************************************************/

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


        if (vector_SAD > (center_SAD * 0.90))
          {
            vector_SAD = center_SAD;
            matrix[x][y][0] = 0;
            matrix[x][y][1] = 0;
          }

        SAD_matrix[x][y] = vector_SAD;
        sum_of_SADs += vector_SAD;
      }
      
  avrg_SAD = sum_of_SADs / nr_of_blocks;

  last_mean_SAD=mean_SAD;
  mean_SAD = avrg_SAD;
  
  if((mean_SAD/3)>last_mean_SAD && last_mean_SAD!=0)
  {
    mean_SAD=last_mean_SAD;
    scene_change = 1;
    mjpeg_log (LOG_INFO, " *** Scene Change ***\n");    
  }
  else
  {
    scene_change=0;
  }
  
  if(mean_SAD>(last_mean_SAD*1.5) && last_mean_SAD!=0)
    mean_SAD=last_mean_SAD;
  
}

void
transform_frame (unsigned char *avg[3])
{
  int x, y;

  mjpeg_log (LOG_INFO, " MSAD : %d \n",mean_SAD);

  for (y = 0; y < height; y += (BLOCKSIZE / 2))
    for (x = 0; x < width; x += (BLOCKSIZE / 2))
      {
        if (SAD_matrix[x][y] < (mean_SAD))
          block_quality=0;
        else        
          if (SAD_matrix[x][y] < (mean_SAD * 2))
            block_quality = (float)SAD_matrix[x][y] / (float)(mean_SAD)-1;
          else
            block_quality = 1;

          copy_filtered_block (x, y, yuv1, avg);

            #if DEBUG
            mjpeg_log (LOG_DEBUG, "block_quality : %f \n",block_quality);
            #endif
      }

  memcpy (avg[0], yuv1[0], width * height);
  memcpy (avg[1], yuv1[1], width * height / 4);
  memcpy (avg[2], yuv1[2], width * height / 4);
}

void
denoise_frame (unsigned char *frame[3])
{

  /***********************************************************/
  /*                                                         */
  /* ! MAIN PROCESSING LOOP !                                */
  /*                                                         */
  /* The denoising process is done the following way:        */
  /*                                                         */
  /* 1. Take the reference frame and calculate the motion-   */
  /*    vectors to the corresponding previously filtered     */
  /*    frame blocks in avrg[x].                             */
  /*                                                         */
  /* 2. Transform the previous filtered image with these     */
  /*    motion vectors to match the current reference frame. */
  /*                                                         */
  /* 3. Blend over the transformed image with the actual     */
  /*    reference frame.                                     */
  /*                                                         */

  /***********************************************************/

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

/* old routine */
#if 0
void
denoise_image ()
{
  int sy, sx, bx, by;
  uint32_t div;
  int x, y;
  uint32_t min_SQD = 0;
  uint32_t CQD = 0;
  uint32_t avg_SQD = 0;
  uint32_t min_Y_SQD = 0;
  uint32_t min_U_SQD = 0;
  uint32_t min_V_SQD = 0;
  uint32_t Smin = 0;
  static uint32_t mean_SQD = 0;
  static uint32_t mean_Y_SQD = -1;
  static uint32_t mean_U_SQD = -1;
  static uint32_t mean_V_SQD = -1;
  int y_start, y_end;

  int matrix[768][576][2];

  if (framenr == 0)
    {
      memcpy (AVRG, YUV1, (size_t) width * height * 1.5);
      memcpy (DEL0, YUV1, (size_t) width * height * 1.5);
      memcpy (YUV2, YUV1, (size_t) width * height * 1.5);
      memcpy (YUV3, YUV1, (size_t) width * height * 1.5);
      memcpy (YUV4, YUV1, (size_t) width * height * 1.5);
    }
  else
    {
      //antiflicker_reference();

      for (y = 0; y < height; y++)
        for (x = 0; x < width; x++)
          YUV4[x + y * width] = (DEL0[x + y * width] + YUV1[x + y * width]) / 2;

      /* subsample the reference image */
      subsample_image (SUBO2, YUV1);
      subsample_image (SUBO4, SUBO2);

      /* subsample the averaged image */
      subsample_image (SUBA2, AVRG);
      subsample_image (SUBA4, SUBA2);

      /* reset the minimum SQD-counter */
      min_SQD = 10000;
      avg_SQD = 0;
      div = 0;

      for (y = 0; y < height; y += (BLOCKSIZE))
        for (x = 0; x < width; x += (BLOCKSIZE))
          {
            if (1)
              {
                mb_search_44 (x, y);
                mb_search_22 (x, y);
                mb_search (x, y);
                matrix[x][y][0] = best_match_x[0];
                matrix[x][y][1] = best_match_y[0];
              }
            else
              {
                best_match_x[0] = 0;
                best_match_y[0] = 0;
              }
          }

      for (y = 0; y < height; y += (BLOCKSIZE))
        for (x = 0; x < width; x += (BLOCKSIZE))
          {
            SQD = calc_SAD (AVRG,
                            YUV1,
                            (x + matrix[x][y][0] / 2) +
                            (y + matrix[x][y][1] / 2) * width, (x) + (y) * width, 2);
#if 1
            SQD += calc_SAD_uv (AVRG + u_offset,
                                YUV1 + u_offset,
                                (x + matrix[x][y][0] / 2) / 2 +
                                (y + matrix[x][y][1] / 2) / 2 * uv_width,
                                (x) / 2 + (y) / 2 * uv_width, 2);

            SQD += calc_SAD_uv (AVRG + v_offset,
                                YUV1 + v_offset,
                                (x + matrix[x][y][0] / 2) / 2 +
                                (y + matrix[x][y][1] / 2) / 2 * uv_width,
                                (x) / 2 + (y) / 2 * uv_width, 2);
#endif
            CQD = calc_SAD (AVRG, YUV1, (x) + (y) * width, (x) + (y) * width, 2);
#if 1
            CQD += calc_SAD_uv (AVRG + u_offset,
                                YUV1 + u_offset,
                                (x) / 2 +
                                (y) / 2 * uv_width, (x) / 2 + (y) / 2 * uv_width, 2);

            CQD += calc_SAD_uv (AVRG + v_offset,
                                YUV1 + v_offset,
                                (x) / 2 +
                                (y) / 2 * uv_width, (x) / 2 + (y) / 2 * uv_width, 2);
#endif
            if (SQD < (2 * CQD))
              {
                best_match_x[0] = matrix[x][y][0];
                best_match_y[0] = matrix[x][y][1];
              }
            else
              {
                best_match_x[0] = 0;
                best_match_y[0] = 0;
              }


            if (SQD < (128 * BLOCKSIZE * BLOCKSIZE / 2 / 2))
              {
                div++;
                avg_SQD += SQD;
              }

            if (SQD <= (mean_SQD * 4))
              {
                //block_quality = SQD/(mean_SQD*4);
                block_quality = 0.0;
                copy_filtered_block (x, y);
              }
            else
              {
                if (SQD >
                    (calc_SQD (AVRG, YUV4, (x) + (y) * width, (x) + (y) * width, 2) +
                     1 * calc_SQD_uv (AVRG + u_offset, YUV4 + u_offset,
                                      (x) / 2 + (y) / 2 * uv_width,
                                      (x) / 2 + (y) / 2 * uv_width,
                                      2) + 1 * calc_SQD_uv (AVRG + v_offset,
                                                            YUV4 + v_offset,
                                                            (x) / 2 + (y) / 2 * uv_width,
                                                            (x) / 2 + (y) / 2 * uv_width,
                                                            2)))
                  {
                    best_match_x[0] = 0;
                    best_match_y[0] = 0;
                    avg_SQD += SQD;
                    div++;
                    copy_filtered_block (x, y);
                  }
                else
                  {
                    copy_reference_block (x, y);
                  }
              }
          }
    }
  fprintf (stderr, "---  mean_SQD   :%i -- %f \n", mean_SQD,
           (float) mean_SQD / (BLOCKSIZE * BLOCKSIZE * BLOCKSIZE * BLOCKSIZE / 4) / 3);

  avg_SQD /= div + 1;
  mean_SQD = mean_SQD * 0.5 + avg_SQD * 0.5;

  if (avg_SQD < mean_SQD)
    mean_SQD = avg_SQD;

  if (mean_SQD < 20)
    mean_SQD = 20;

  /* calculate the time averaged image */
  delay_image ();

  for (y = 0; y < height; y++)
    for (x = 0; x < width; x++)
      YUV4[x + y * width] = (DEL0[x + y * width] + YUV1[x + y * width]) / 2;

  time_average_images ();

}
#endif
