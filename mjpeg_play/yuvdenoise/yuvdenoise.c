
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

uint8_t version[] = "0.0.63\0";

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
uint32_t framenr = 0;

int deinterlace=0;              /* set to 1 if deinterlacing needed */
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
int lum_delta;
int cru_delta;
int crv_delta;
int search_radius = 8;          /* initial search-radius in half-pixels */
int border = -1;
double block_quality;
float sharpen = 0;

y4m_frame_info_t *frameinfo = NULL;
y4m_stream_info_t *streaminfo = NULL;

float a[16];
int i;
float v;

void denoise_image ();
void detect_black_borders ();
void mb_search_88 (int x, int y);
void mb_search (int x, int y);
void subsample_reference_image2 ();
void subsample_averaged_image2 ();
void subsample_reference_image4 ();
void subsample_averaged_image4 ();
void delay_images ();
void time_average_images ();
void display_greeting ();
void display_help ();
void copy_filtered_block (int x, int y);

void deinterlace_frame (unsigned char* yuv[3]);

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
  unsigned char *yuv[3];
  unsigned char *yuv1[3];
  unsigned char *yuv2[3];
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

  while ((c = getopt (argc, argv, "?:d")) != -1)
    {
      switch (c)
        {
        case 'd':
          deinterlace=1;
          mjpeg_log (LOG_INFO, "deinterlacer activated\n");    
          break;
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

/* write the outstream header */

  y4m_write_stream_header (fd_out, streaminfo);

/* read every single frame until the end of the input stream */

  while ((errnr = y4m_read_frame (fd_in, streaminfo, frameinfo, yuv)) == Y4M_OK)
    {
      /* process the frame */

      /* if needed, deinterlace frame */
      if(deinterlace)
        deinterlace_frame(yuv);

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
  mjpeg_log (LOG_INFO, "corropt your movies. \n");
  mjpeg_log (LOG_INFO, "------------------------------------ \n");
}

void
display_help ()
{
  fprintf (stdout, "sorry no help this time\n");
  exit (0);
}

void deinterlace_frame (unsigned char* yuv[3])
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
  for (y=0;y<height;y+=2)
  {
    /* Go through each line by a "block" length of 32 pixels */
    for (x=0;x<width;x+=16)
    {
      /* search best matching block of pixels in other field line */
      min= 65535;
      xpos=0;
      /* search in the range of +/- 12 pixels offset in the line */
      for (xx=-12;xx<12;xx++)
      {
        d=0;
        /* Calculate SAD */
        for (i=0;i<16;i++)
        {
          /* to avoid blocking in ramps we analyse the best match on */
          /* two lines ... */
          d += abs (
                    *(yuv[0]+(x+i)+y*width) -
                    *(yuv[0]+(x+xx+i)+(y+1)*width)
                    );
          d += abs (
                    *(yuv[0]+(x+i)+(y+2)*width) -
                    *(yuv[0]+(x+xx+i)+(y+1)*width)
                    );
        }
        /* if SAD reaches a minimum store the position */
        if (min>d)
        {
          min=d;
          xpos=xx;
          l1=l2=0;
          for (i=0;i<16;i++)
          {
            l1+=*(yuv[0]+(x+i)+y*width);
            l2+=*(yuv[0]+(x+i+xpos)+(y+1)*width);
          }
          l1/=16;
          l2/=16;
          lumadiff=abs(l1-l2);
          lumadiff=(lumadiff<12)? 0:1;
        }
      }

      /* copy pixel-block into the line-buffer */

      /* if lumadiff is small take the fields block, if not */
      /* take the other fields block */
      
      if(lumadiff && min>1024)
        for (i=0;i<16;i++)
        {
          *(line+x+i)=
          *(yuv[0]+(x+i)+((y)*width));
        }          
      else
        for (i=0;i<16;i++)
        {
          *(line+x+i)=
          *(yuv[0]+(x+i+xpos)+((y+1)*width))*0.5+
          *(yuv[0]+(x+i)+((y+0)*width))*0.5;
        }
    }
    
    /* copy the line-buffer into the source-line */
    for(i=0;i<width;i++)
      *(yuv[0]+i+(y+1)*width)=
        *(line+i);
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
subsample_image (uint8_t * dest, uint8_t * source)
{
  int x, y;

  /* subsample Y */
  for (y = 0; y < height; y += 2)
    for (x = 0; x < width; x += 2)
      {
        dest[(x / 2) + (y / 2) * width] =
          (source[(x - 2) + (y - 2) * width] +
           source[(x - 1) + (y - 2) * width] +
           source[(x + 0) + (y - 2) * width] +
           source[(x + 1) + (y - 2) * width] +
           source[(x + 2) + (y - 2) * width] +
           source[(x - 2) + (y - 1) * width] +
           source[(x - 1) + (y - 1) * width] +
           source[(x + 0) + (y - 1) * width] +
           source[(x + 1) + (y - 1) * width] +
           source[(x + 2) + (y - 1) * width] +
           source[(x - 2) + (y + 0) * width] +
           source[(x - 1) + (y + 0) * width] +
           source[(x + 0) + (y + 0) * width] +
           source[(x + 1) + (y + 0) * width] +
           source[(x + 2) + (y + 0) * width] +
           source[(x - 2) + (y + 1) * width] +
           source[(x - 1) + (y + 1) * width] +
           source[(x + 0) + (y + 1) * width] +
           source[(x + 1) + (y + 1) * width] +
           source[(x + 2) + (y + 1) * width] +
           source[(x - 2) + (y + 2) * width] +
           source[(x - 1) + (y + 2) * width] +
           source[(x + 0) + (y + 2) * width] +
           source[(x + 1) + (y + 2) * width] + source[(x + 2) + (y + 2) * width]) / 25;
      }

  for (y = 0; y < uv_height; y += 2)
    for (x = 0; x < uv_width; x += 2)
      {
        /* U */
        dest[(x / 2) + (y / 2) * uv_width + u_offset] =
          (source[(x - 2) + (y - 2) * uv_width + u_offset] +
           source[(x - 1) + (y - 2) * uv_width + u_offset] +
           source[(x + 0) + (y - 2) * uv_width + u_offset] +
           source[(x + 1) + (y - 2) * uv_width + u_offset] +
           source[(x + 2) + (y - 2) * uv_width + u_offset] +
           source[(x - 2) + (y - 1) * uv_width + u_offset] +
           source[(x - 1) + (y - 1) * uv_width + u_offset] +
           source[(x + 0) + (y - 1) * uv_width + u_offset] +
           source[(x + 1) + (y - 1) * uv_width + u_offset] +
           source[(x + 2) + (y - 1) * uv_width + u_offset] +
           source[(x - 2) + (y + 0) * uv_width + u_offset] +
           source[(x - 1) + (y + 0) * uv_width + u_offset] +
           source[(x + 0) + (y + 0) * uv_width + u_offset] +
           source[(x + 1) + (y + 0) * uv_width + u_offset] +
           source[(x + 2) + (y + 0) * uv_width + u_offset] +
           source[(x - 2) + (y + 1) * uv_width + u_offset] +
           source[(x - 1) + (y + 1) * uv_width + u_offset] +
           source[(x + 0) + (y + 1) * uv_width + u_offset] +
           source[(x + 1) + (y + 1) * uv_width + u_offset] +
           source[(x + 2) + (y + 1) * uv_width + u_offset] +
           source[(x - 2) + (y + 2) * uv_width + u_offset] +
           source[(x - 1) + (y + 2) * uv_width + u_offset] +
           source[(x + 0) + (y + 2) * uv_width + u_offset] +
           source[(x + 1) + (y + 2) * uv_width + u_offset] +
           source[(x + 2) + (y + 2) * uv_width + u_offset]) / 25;

        /* V */
        dest[(x / 2) + (y / 2) * uv_width + v_offset] =
          (source[(x - 2) + (y - 2) * uv_width + v_offset] +
           source[(x - 1) + (y - 2) * uv_width + v_offset] +
           source[(x + 0) + (y - 2) * uv_width + v_offset] +
           source[(x + 1) + (y - 2) * uv_width + v_offset] +
           source[(x + 2) + (y - 2) * uv_width + v_offset] +
           source[(x - 2) + (y - 1) * uv_width + v_offset] +
           source[(x - 1) + (y - 1) * uv_width + v_offset] +
           source[(x + 0) + (y - 1) * uv_width + v_offset] +
           source[(x + 1) + (y - 1) * uv_width + v_offset] +
           source[(x + 2) + (y - 1) * uv_width + v_offset] +
           source[(x - 2) + (y + 0) * uv_width + v_offset] +
           source[(x - 1) + (y + 0) * uv_width + v_offset] +
           source[(x + 0) + (y + 0) * uv_width + v_offset] +
           source[(x + 1) + (y + 0) * uv_width + v_offset] +
           source[(x + 2) + (y + 0) * uv_width + v_offset] +
           source[(x - 2) + (y + 1) * uv_width + v_offset] +
           source[(x - 1) + (y + 1) * uv_width + v_offset] +
           source[(x + 0) + (y + 1) * uv_width + v_offset] +
           source[(x + 1) + (y + 1) * uv_width + v_offset] +
           source[(x + 2) + (y + 1) * uv_width + v_offset] +
           source[(x - 2) + (y + 2) * uv_width + v_offset] +
           source[(x - 1) + (y + 2) * uv_width + v_offset] +
           source[(x + 0) + (y + 2) * uv_width + v_offset] +
           source[(x + 1) + (y + 2) * uv_width + v_offset] +
           source[(x + 2) + (y + 2) * uv_width + v_offset]) / 25;
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
calc_SAD (char *frm, char *ref, uint32_t frm_offs, uint32_t ref_offs, int div)
{
#if 0
  int dx, dy, Y;
  uint32_t d = 0;

  for (dy = 0; dy < BLOCKSIZE / div; dy++)
    for (dx = 0; dx < BLOCKSIZE / div; dx++)
      {
        Y =
          *(frm + frm_offs + (dx) + (dy) * width) -
          *(ref + ref_offs + (dx) + (dy) * width);

        d += (Y > 0) ? Y : -Y;
      }
  return d;
#endif
  int dx, dy, Y;
  uint32_t d = 0;
  int f, r;

  f = frm_offs;
  r = ref_offs;

  for (dy = 0; dy < BLOCKSIZE / div; dy++)
    {
      for (dx = 0; dx < BLOCKSIZE / div; dx++)
        {
          Y = *(frm + f++) - *(ref + r++);

          d += (Y > 0) ? Y : -Y;
        }
      f += width - (BLOCKSIZE / div) - 1;
      r += width - (BLOCKSIZE / div) - 1;
    }
  return d;
}

inline uint32_t
calc_SAD_ (char *frm, char *ref, uint32_t frm_offs, uint32_t ref_offs, int div)
{
  int dx, dy, Y;
  uint32_t d = 0;

  for (dy = -BLOCKSIZE; dy < BLOCKSIZE * 2 / div; dy++)
    for (dx = -BLOCKSIZE; dx < BLOCKSIZE * 2 / div; dx++)
      {
        Y =
          *(frm + frm_offs + (dx) + (dy) * width) -
          *(ref + ref_offs + (dx) + (dy) * width);

        d += (Y > 0) ? Y : -Y;
      }
  return d;
}

inline uint32_t
calc_SAD_uv (uint8_t * frm, uint8_t * ref, uint32_t frm_offs, uint32_t ref_offs, int div)
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

            d += (Y > 0) ? Y : -Y;
          }
      old_d = d;
      return d;
    }
  else
    {
      return old_d;
    }
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
mb_search_44 (int x, int y)
{
  int dy, dx, qy, qx;
  uint32_t d;
  int Y, U, V;

  d =
    calc_SAD (SUBA4, SUBO4, (x - BLOCKOFFSET) / 4 + (y - BLOCKOFFSET) / 4 * width,
              (x - BLOCKOFFSET) / 4 + (y - BLOCKOFFSET) / 4 * width, 4);
#if 0
  d += calc_SAD_uv (SUBA4 + u_offset,
                    SUBO4 + u_offset,
                    (x - BLOCKOFFSET) / 8 + (y - BLOCKOFFSET) / 8 * uv_width,
                    (x - BLOCKOFFSET) / 8 + (y - BLOCKOFFSET) / 8 * uv_width, 4);
  d +=
    calc_SAD_uv (SUBA4 + v_offset, SUBO4 + v_offset,
                 (x - BLOCKOFFSET) / 8 + (y - BLOCKOFFSET) / 8 * uv_width,
                 (x - BLOCKOFFSET) / 8 + (y - BLOCKOFFSET) / 8 * uv_width, 4);
#endif
  CSAD = d;

  SAD[0] = 10000000;
  best_match_x[0] = 0;
  best_match_y[0] = 0;

  for (qy = -(search_radius >> 1); qy <= +(search_radius >> 1); qy += 4)
    for (qx = -(search_radius >> 1); qx <= +(search_radius >> 1); qx += 4)
      {
        d = calc_SAD (SUBA4,
                      SUBO4,
                      (x + qx - BLOCKOFFSET) / 4 + (y + qy - BLOCKOFFSET) / 4 * width,
                      (x - BLOCKOFFSET) / 4 + (y - BLOCKOFFSET) / 4 * width, 4);
#if 1
        d += calc_SAD_uv (SUBA4 + u_offset,
                          SUBO4 + u_offset,
                          (x + qx - BLOCKOFFSET) / 8 + (y + qy -
                                                        BLOCKOFFSET) / 8 * uv_width,
                          (x - BLOCKOFFSET) / 8 + (y - BLOCKOFFSET) / 8 * uv_width, 4);

        d += calc_SAD_uv (SUBA4 + v_offset,
                          SUBO4 + v_offset,
                          (x + qx - BLOCKOFFSET) / 8 + (y + qy -
                                                        BLOCKOFFSET) / 8 * uv_width,
                          (x - BLOCKOFFSET) / 8 + (y - BLOCKOFFSET) / 8 * uv_width, 4);
#endif
        if (d < CSAD)
          {
            if (d < SAD[0])
              {
                best_match_x[0] = qx * 2;
                best_match_y[0] = qy * 2;
                SAD[0] = d;
              }
          }
      }
}

void
mb_search_22 (int x, int y)
{
  int dy, dx, qy, qx;
  uint32_t d;
  int Y, U, V;
  int bx;
  int by;
  int i;

  SAD[0] = 10000000;
  bx = best_match_x[0] / 2;
  by = best_match_y[0] / 2;

  for (qy = (by - 8); qy <= (by + 8); qy += 2)
    for (qx = (bx - 8); qx <= (bx + 8); qx += 2)
      {
        d = calc_SAD (SUBA2,
                      SUBO2,
                      (x + qx - BLOCKOFFSET) / 2 + (y + qy - BLOCKOFFSET) / 2 * width,
                      (x - BLOCKOFFSET) / 2 + (y - BLOCKOFFSET) / 2 * width, 2);
#if 1
        d += calc_SAD_uv (SUBA2 + u_offset,
                          SUBO2 + u_offset,
                          (x + qx) / 4 + (y + qy) / 4 * uv_width,
                          (x) / 4 + (y) / 4 * uv_width, 2);

        d += calc_SAD_uv (SUBA2 + v_offset,
                          SUBO2 + v_offset,
                          (x + qx) / 4 + (y + qy) / 4 * uv_width,
                          (x) / 4 + (y) / 4 * uv_width, 2);
#endif
        if (qx == qy == 0)
          {
            CSAD = d;
          }

        if (d < SAD[0])
          {
            best_match_x[0] = qx * 2;
            best_match_y[0] = qy * 2;
            SAD[0] = d;
          }
      }
  if (CSAD <= SAD[0])
    {
      best_match_x[0] = bx * 2;
      best_match_y[0] = by * 2;
      SAD[0] = CSAD;
    }
}

void
mb_search (int x, int y)
{
  int dy, dx, qy, qx;
  uint32_t d, du, dv;
  int Y, U, V;
  int bx;
  int by;

  SAD[0] = 10000000;
  bx = best_match_x[0] / 2;
  by = best_match_y[0] / 2;

  for (qy = (by - 4); qy <= (by + 4); qy++)
    for (qx = (bx - 4); qx <= (bx + 4); qx++)
      {
        d = calc_SAD (AVRG,
                      YUV1,
                      (x + qx - BLOCKOFFSET) + (y + qy - BLOCKOFFSET) * width,
                      (x - BLOCKOFFSET) + (y - BLOCKOFFSET) * width, 1);
#if 1
        d += calc_SAD_uv (AVRG + u_offset,
                          YUV1 + u_offset,
                          (x + qx) / 2 + (y + qy) / 2 * uv_width,
                          (x) / 2 + (y) / 2 * uv_width, 1);

        d += calc_SAD_uv (AVRG + v_offset,
                          YUV1 + v_offset,
                          (x + qx) / 2 + (y + qy) / 2 * uv_width,
                          (x) / 2 + (y) / 2 * uv_width, 1);
#endif
        if (qx == qy == 0)
          {
            CSAD = d;
          }

        if (d < SAD[0])
          {
            best_match_x[0] = qx * 2;
            best_match_y[0] = qy * 2;
            SAD[0] = d;
          }
      }
  if (CSAD <= SAD[0])
    {
      best_match_x[0] = bx * 2;
      best_match_y[0] = by * 2;
      SAD[i] = CSAD;
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
copy_filtered_block (int x, int y)
{
  int dx, dy;
  int qx = best_match_x[0];
  int qy = best_match_y[0];
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

        /* Y */
        if (sx != 0 || sy != 0)
          YUV4[(xx) + (yy) * width] =
            YUV2[(xx) + (yy) * width] =
            (AVRG[(xx + qx - 0) + (yy + qy - 0) * width] * a0 +
             AVRG[(xx + qx - 1) + (yy + qy - 0) * width] * a1 +
             AVRG[(xx + qx - 0) + (yy + qy - 1) * width] * a2 +
             AVRG[(xx + qx - 1) + (yy + qy - 1) * width] * a3) * (1 -
                                                                  block_quality)
            + YUV1[(xx) + (yy) * width] * block_quality;

        else
          YUV4[(xx) + (yy) * width] =
            YUV2[(xx) + (yy) * width] =
            AVRG[(xx + qx) + (yy + qy) * width] * (1 - block_quality) +
            YUV1[(xx) + (yy) * width] * block_quality;

        /* U */
        YUV4[(x + dx) / 2 + (y + dy) / 2 * uv_width + u_offset] =
          YUV2[(x + dx) / 2 + (y + dy) / 2 * uv_width + u_offset] =
          AVRG[(x + dx + qx) / 2 + (y + dy + qy) / 2 * uv_width +
               u_offset] * (1 - block_quality) + YUV1[(x2) + (y2) +
                                                      u_offset] * block_quality;

        /* V */
        YUV4[(x2) + (y2) + v_offset] =
          YUV2[(x2) + (y2) + v_offset] =
          AVRG[(x + dx + qx) / 2 + (y + dy + qy) / 2 * uv_width +
               v_offset] * (1 - block_quality) + YUV1[(x2) + (y2) +
                                                      v_offset] * block_quality;
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
block_change (int x, int y, uint32_t mean)
{
  uint32_t d = 0;

  d = calc_SQD (YUV1, AVRG, (x) / 2 + (y) / 2 * width, (x) / 2 + (y) / 2 * width, 2);

  return (d <= mean * 4) ? 0 : 1;
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
