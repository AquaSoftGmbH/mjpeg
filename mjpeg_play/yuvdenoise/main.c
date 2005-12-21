/***********************************************************
 * YUVDENOISER for the mjpegtools                          *
 * ------------------------------------------------------- *
 * (C) 2001-2004 Stefan Fendt                              *
 *                                                         *
 * Licensed and protected by the GNU-General-Public-       *
 * License version 2 or if you prefer any later version of *
 * that license). See the file LICENSE for detailed infor- *
 * mation.                                                 *
 *                                                         *
 * FILE: main.c                                            *
 *                                                         *
 ***********************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "config.h"
#include "mjpeg_types.h"
#include "yuv4mpeg.h"
#include "mjpeg_logging.h"
#include "cpu_accel.h"
#include "motionsearch.h"

int verbose = 1;
int width = 0;
int height = 0;
int lwidth = 0;
int lheight = 0;
int cwidth = 0;
int cheight = 0;
int input_chroma_subsampling = 0;
int input_interlaced = 0;

int renoise_Y=0;
int renoise_U=0;
int renoise_V=0;

int temp_Y_thres = 4;
int temp_U_thres = 8;
int temp_V_thres = 8;

int med_Y_thres = 2;
int med_U_thres = 4;
int med_V_thres = 4;

int spat_Y_thres = 2;
int spat_U_thres = 4;
int spat_V_thres = 4;

int gauss_Y = 16;
int gauss_U = 255;
int gauss_V = 255;

uint8_t *frame1[3];
uint8_t *frame2[3];
uint8_t *frame3[3];
uint8_t *frame4[3];
uint8_t *frame5[3];
uint8_t *frame6[3];
uint8_t *frame7[3];
uint8_t *frame8[3];
uint8_t *frame9[3];
uint8_t *framea[3];
uint8_t *frameb[3];
uint8_t *framec[3];
uint8_t *framed[3];
uint8_t *framee[3];
uint8_t *framef[3];
uint8_t *scratchplane;
uint8_t *outframe[3];

int buff_offset;
int buff_size;

uint16_t transform_L16[256];
uint8_t transform_G8[65536];

/***********************************************************
 * helper-functions                                        *
 ***********************************************************/

// if we want any visualy correct threshold to be applied we need to linearize
// the data. As YUV 8Bit is compressed in the shaddows, we would need to uncompress
// this first by applying a gamma-correction of 2.8 for PAL and 2.2 for NTSC...
// we instead actualy use 1.8 (which is incorrect for both but more "real")

#define CMP(a,b) {int temp;if(a<b){temp=a;a=b;b=temp;}}

int median5 ( int a, int b, int c, int d, int e )
{
	CMP(a,b);
	CMP(b,c);
	CMP(c,d);
	CMP(d,e);

	CMP(a,b);
	CMP(b,c);
	CMP(c,d);

	CMP(a,b);
	CMP(b,c);

	return c;
}

int median9 ( int value[9] )
{
	CMP(value[0],value[1]);
	CMP(value[1],value[2]);
	CMP(value[2],value[3]);
	CMP(value[3],value[4]);
	CMP(value[4],value[5]);
	CMP(value[5],value[6]);
	CMP(value[6],value[7]);
	CMP(value[7],value[8]);

	CMP(value[0],value[1]);
	CMP(value[1],value[2]);
	CMP(value[2],value[3]);
	CMP(value[3],value[4]);
	CMP(value[4],value[5]);
	CMP(value[5],value[6]);
	CMP(value[6],value[7]);

	CMP(value[0],value[1]);
	CMP(value[1],value[2]);
	CMP(value[2],value[3]);
	CMP(value[3],value[4]);
	CMP(value[4],value[5]);
	CMP(value[5],value[6]);

	CMP(value[0],value[1]);
	CMP(value[1],value[2]);
	CMP(value[2],value[3]);
	CMP(value[3],value[4]);
	CMP(value[4],value[5]);

	CMP(value[0],value[1]);
	CMP(value[1],value[2]);
	CMP(value[2],value[3]);
	CMP(value[3],value[4]);

	return value[4];
}

int median27 (uint8_t value[27] )
{
	CMP(value[ 0],value[ 1]);
	CMP(value[ 1],value[ 2]);
	CMP(value[ 2],value[ 3]);
	CMP(value[ 3],value[ 4]);
	CMP(value[ 4],value[ 5]);
	CMP(value[ 5],value[ 6]);
	CMP(value[ 6],value[ 7]);
	CMP(value[ 7],value[ 8]);
	CMP(value[ 8],value[ 9]);
	CMP(value[ 9],value[10]);
	CMP(value[10],value[11]);
	CMP(value[11],value[12]);
	CMP(value[12],value[13]);
	CMP(value[13],value[14]);
	CMP(value[14],value[15]);
	CMP(value[15],value[16]);
	CMP(value[16],value[17]);
	CMP(value[17],value[18]);
	CMP(value[18],value[19]);
	CMP(value[19],value[20]);
	CMP(value[20],value[21]);
	CMP(value[21],value[22]);
	CMP(value[22],value[23]);
	CMP(value[23],value[24]);
	CMP(value[24],value[25]);
	CMP(value[25],value[26]);
	CMP(value[26],value[27]);

	CMP(value[ 0],value[ 1]);
	CMP(value[ 1],value[ 2]);
	CMP(value[ 2],value[ 3]);
	CMP(value[ 3],value[ 4]);
	CMP(value[ 4],value[ 5]);
	CMP(value[ 5],value[ 6]);
	CMP(value[ 6],value[ 7]);
	CMP(value[ 7],value[ 8]);
	CMP(value[ 8],value[ 9]);
	CMP(value[ 9],value[10]);
	CMP(value[10],value[11]);
	CMP(value[11],value[12]);
	CMP(value[12],value[13]);
	CMP(value[13],value[14]);
	CMP(value[14],value[15]);
	CMP(value[15],value[16]);
	CMP(value[16],value[17]);
	CMP(value[17],value[18]);
	CMP(value[18],value[19]);
	CMP(value[19],value[20]);
	CMP(value[20],value[21]);
	CMP(value[21],value[22]);
	CMP(value[22],value[23]);
	CMP(value[23],value[24]);
	CMP(value[24],value[25]);
	CMP(value[25],value[26]);

	CMP(value[ 0],value[ 1]);
	CMP(value[ 1],value[ 2]);
	CMP(value[ 2],value[ 3]);
	CMP(value[ 3],value[ 4]);
	CMP(value[ 4],value[ 5]);
	CMP(value[ 5],value[ 6]);
	CMP(value[ 6],value[ 7]);
	CMP(value[ 7],value[ 8]);
	CMP(value[ 8],value[ 9]);
	CMP(value[ 9],value[10]);
	CMP(value[10],value[11]);
	CMP(value[11],value[12]);
	CMP(value[12],value[13]);
	CMP(value[13],value[14]);
	CMP(value[14],value[15]);
	CMP(value[15],value[16]);
	CMP(value[16],value[17]);
	CMP(value[17],value[18]);
	CMP(value[18],value[19]);
	CMP(value[19],value[20]);
	CMP(value[20],value[21]);
	CMP(value[21],value[22]);
	CMP(value[22],value[23]);
	CMP(value[23],value[24]);
	CMP(value[24],value[25]);

	CMP(value[ 0],value[ 1]);
	CMP(value[ 1],value[ 2]);
	CMP(value[ 2],value[ 3]);
	CMP(value[ 3],value[ 4]);
	CMP(value[ 4],value[ 5]);
	CMP(value[ 5],value[ 6]);
	CMP(value[ 6],value[ 7]);
	CMP(value[ 7],value[ 8]);
	CMP(value[ 8],value[ 9]);
	CMP(value[ 9],value[10]);
	CMP(value[10],value[11]);
	CMP(value[11],value[12]);
	CMP(value[12],value[13]);
	CMP(value[13],value[14]);
	CMP(value[14],value[15]);
	CMP(value[15],value[16]);
	CMP(value[16],value[17]);
	CMP(value[17],value[18]);
	CMP(value[18],value[19]);
	CMP(value[19],value[20]);
	CMP(value[20],value[21]);
	CMP(value[21],value[22]);
	CMP(value[22],value[23]);
	CMP(value[23],value[24]);

	CMP(value[ 0],value[ 1]);
	CMP(value[ 1],value[ 2]);
	CMP(value[ 2],value[ 3]);
	CMP(value[ 3],value[ 4]);
	CMP(value[ 4],value[ 5]);
	CMP(value[ 5],value[ 6]);
	CMP(value[ 6],value[ 7]);
	CMP(value[ 7],value[ 8]);
	CMP(value[ 8],value[ 9]);
	CMP(value[ 9],value[10]);
	CMP(value[10],value[11]);
	CMP(value[11],value[12]);
	CMP(value[12],value[13]);
	CMP(value[13],value[14]);
	CMP(value[14],value[15]);
	CMP(value[15],value[16]);
	CMP(value[16],value[17]);
	CMP(value[17],value[18]);
	CMP(value[18],value[19]);
	CMP(value[19],value[20]);
	CMP(value[20],value[21]);
	CMP(value[21],value[22]);

	CMP(value[ 0],value[ 1]);
	CMP(value[ 1],value[ 2]);
	CMP(value[ 2],value[ 3]);
	CMP(value[ 3],value[ 4]);
	CMP(value[ 4],value[ 5]);
	CMP(value[ 5],value[ 6]);
	CMP(value[ 6],value[ 7]);
	CMP(value[ 7],value[ 8]);
	CMP(value[ 8],value[ 9]);
	CMP(value[ 9],value[10]);
	CMP(value[10],value[11]);
	CMP(value[11],value[12]);
	CMP(value[12],value[13]);
	CMP(value[13],value[14]);
	CMP(value[14],value[15]);
	CMP(value[15],value[16]);
	CMP(value[16],value[17]);
	CMP(value[17],value[18]);
	CMP(value[18],value[19]);
	CMP(value[19],value[20]);
	CMP(value[20],value[21]);

	CMP(value[ 0],value[ 1]);
	CMP(value[ 1],value[ 2]);
	CMP(value[ 2],value[ 3]);
	CMP(value[ 3],value[ 4]);
	CMP(value[ 4],value[ 5]);
	CMP(value[ 5],value[ 6]);
	CMP(value[ 6],value[ 7]);
	CMP(value[ 7],value[ 8]);
	CMP(value[ 8],value[ 9]);
	CMP(value[ 9],value[10]);
	CMP(value[10],value[11]);
	CMP(value[11],value[12]);
	CMP(value[12],value[13]);
	CMP(value[13],value[14]);
	CMP(value[14],value[15]);
	CMP(value[15],value[16]);
	CMP(value[16],value[17]);
	CMP(value[17],value[18]);
	CMP(value[18],value[19]);

	CMP(value[ 0],value[ 1]);
	CMP(value[ 1],value[ 2]);
	CMP(value[ 2],value[ 3]);
	CMP(value[ 3],value[ 4]);
	CMP(value[ 4],value[ 5]);
	CMP(value[ 5],value[ 6]);
	CMP(value[ 6],value[ 7]);
	CMP(value[ 7],value[ 8]);
	CMP(value[ 8],value[ 9]);
	CMP(value[ 9],value[10]);
	CMP(value[10],value[11]);
	CMP(value[11],value[12]);
	CMP(value[12],value[13]);
	CMP(value[13],value[14]);
	CMP(value[14],value[15]);
	CMP(value[15],value[16]);
	CMP(value[16],value[17]);
	CMP(value[17],value[18]);

	CMP(value[ 0],value[ 1]);
	CMP(value[ 1],value[ 2]);
	CMP(value[ 2],value[ 3]);
	CMP(value[ 3],value[ 4]);
	CMP(value[ 4],value[ 5]);
	CMP(value[ 5],value[ 6]);
	CMP(value[ 6],value[ 7]);
	CMP(value[ 7],value[ 8]);
	CMP(value[ 8],value[ 9]);
	CMP(value[ 9],value[10]);
	CMP(value[10],value[11]);
	CMP(value[11],value[12]);
	CMP(value[12],value[13]);
	CMP(value[13],value[14]);
	CMP(value[14],value[15]);
	CMP(value[15],value[16]);

	CMP(value[ 0],value[ 1]);
	CMP(value[ 1],value[ 2]);
	CMP(value[ 2],value[ 3]);
	CMP(value[ 3],value[ 4]);
	CMP(value[ 4],value[ 5]);
	CMP(value[ 5],value[ 6]);
	CMP(value[ 6],value[ 7]);
	CMP(value[ 7],value[ 8]);
	CMP(value[ 8],value[ 9]);
	CMP(value[ 9],value[10]);
	CMP(value[10],value[11]);
	CMP(value[11],value[12]);
	CMP(value[12],value[13]);
	CMP(value[13],value[14]);
	CMP(value[14],value[15]);

	CMP(value[ 0],value[ 1]);
	CMP(value[ 1],value[ 2]);
	CMP(value[ 2],value[ 3]);
	CMP(value[ 3],value[ 4]);
	CMP(value[ 4],value[ 5]);
	CMP(value[ 5],value[ 6]);
	CMP(value[ 6],value[ 7]);
	CMP(value[ 7],value[ 8]);
	CMP(value[ 8],value[ 9]);
	CMP(value[ 9],value[10]);
	CMP(value[10],value[11]);
	CMP(value[11],value[12]);
	CMP(value[12],value[13]);
	CMP(value[13],value[14]);


	return value[14];
}
uint8_t get_median ( uint8_t * pix_list, int max_index )
{
	int i;
	int j;
	int n = max_index;

	for(i=0;i<max_index;i++)
	{
		for(j=0;j<n;j++)
		{
			CMP ( pix_list[j],pix_list[j+1] );
		}
		n--;
	}
	return pix_list[max_index/2];
}


void
init_gamma_transform_LUTs (void)
{
  int i;

  for (i = 0; i < 256; i++)
    {
      transform_L16[i] = pow ((float) i / 256.f, 1 / 1.8) * 65536.f;
      //fprintf(stderr,"%i ",transform_L16[i]);
    }

  for (i = 0; i < 65536; i++)
    {
      transform_G8[i] = pow ((float) i / 65536.f, 1.8) * 256.f;
      //fprintf(stderr,"%i ",transform_G8[i]);
    }

  mjpeg_log (LOG_INFO, "16-Bit gamma-transformations initialized...");
}

// 3x3 gauss filter image-plane and overlay this with factor p (0...255) on the source
// p=0       use source-pixels unfiltered
// p=1...254 use mixed gauss and source-pixels
// p=255     use gauss-filtered pixels
void
gauss_filter_plane (uint8_t * plane, int w, int h, int p)
{
  int x, y;
  int g;
  uint8_t *src = plane;
  uint8_t *dst = scratchplane;


// If the gaussian filter is disabled why go thru all the data copying - just
// return early and speed things up.
  if (p == 0)
    return;

// fill first and last line content into out of bounds-region
  memcpy (src - w, src, w);
  memcpy (src + w * h, src + w * h - w, w);

  for (y = 0; y < h; y++)
    for (x = 0; x < w; x++)
      {
	g = *(src + (-1) + (-1) * w);
	g += *(src + (-1) * w) * 2;
	g += *(src + (+1) + (-1) * w);
	g += *(src + (-1)) * 2;
	g += *(src) * 4;
	g += *(src + (+1)) * 2;
	g += *(src + (-1) + (+1) * w);
	g += *(src + (+1) * w) * 2;
	g += *(src + (+1) + (+1) * w);
	g /= 16;

	*(dst) = (g * p + *(src) * (255 - p)) / 255;
	dst++;
	src++;
      }
  memcpy (plane, scratchplane, w * h);
}

void
adaptive_filter_plane (uint8_t * ref, int w, int h, uint16_t t)
{
  uint8_t *ff = ref;		// reference buffer

  uint32_t x, m;
  int32_t d;

  t *= 256;

  if (w == lwidth)
    for (x = 0; x < (w * h); x++)
      {

	m = transform_L16[*(ff - 2 - w * 2)];
	m += transform_L16[*(ff - 1 - w * 2)];
	m += transform_L16[*(ff - w * 2)];
	m += transform_L16[*(ff + 1 - w * 2)];
	m += transform_L16[*(ff + 2 - w * 2)];
	m += transform_L16[*(ff - 2 - w)];
	m += transform_L16[*(ff - 1 - w)];
	m += transform_L16[*(ff - w)] * 2;
	m += transform_L16[*(ff + 1 - w)];
	m += transform_L16[*(ff + 2 - w)];
	m += transform_L16[*(ff - 2)];
	m += transform_L16[*(ff - 1)] * 2;
	m += transform_L16[*(ff)] * 4;
	m += transform_L16[*(ff + 1)] * 2;
	m += transform_L16[*(ff + 2)];
	m += transform_L16[*(ff - 2 + w)];
	m += transform_L16[*(ff - 1 + w)];
	m += transform_L16[*(ff + w)] * 2;
	m += transform_L16[*(ff + 1 + w)];
	m += transform_L16[*(ff + 2 + w)];
	m += transform_L16[*(ff - 2 + w * 2)];
	m += transform_L16[*(ff - 1 + w * 2)];
	m += transform_L16[*(ff + w * 2)];
	m += transform_L16[*(ff + 1 + w * 2)];
	m += transform_L16[*(ff + 2 + w * 2)];
	m /= 32;

	d = t - abs (transform_L16[*(ff)] - m);
	d = d < 0 ? 0 : d;

	m *= d;
	m += transform_L16[*(ff)];
	m /= d + 1;

	*(ff) = transform_G8[m];

	ff++;
      }
  else
    for (x = 0; x < (w * h); x++)
      {

	m = 256 * *(ff - 2 - w * 2);
	m += 256 * *(ff - 1 - w * 2);
	m += 256 * *(ff - w * 2);
	m += 256 * *(ff + 1 - w * 2);
	m += 256 * *(ff + 2 - w * 2);
	m += 256 * *(ff - 2 - w);
	m += 256 * *(ff - 1 - w);
	m += 256 * *(ff - w) * 2;
	m += 256 * *(ff + 1 - w);
	m += 256 * *(ff + 2 - w);
	m += 256 * *(ff - 2);
	m += 256 * *(ff - 1) * 2;
	m += 256 * *(ff) * 4;
	m += 256 * *(ff + 1) * 2;
	m += 256 * *(ff + 2);
	m += 256 * *(ff - 2 + w);
	m += 256 * *(ff - 1 + w);
	m += 256 * *(ff + w) * 2;
	m += 256 * *(ff + 1 + w);
	m += 256 * *(ff + 2 + w);
	m += 256 * *(ff - 2 + w * 2);
	m += 256 * *(ff - 1 + w * 2);
	m += 256 * *(ff + w * 2);
	m += 256 * *(ff + 1 + w * 2);
	m += 256 * *(ff + 2 + w * 2);
	m /= 32;
#if 1
	d = t - abs ((256 * *(ff)) - m);
	d = d < 0 ? 0 : d;

	m = m * d;
	m += *(ff) * 256;
	m /= (d + 1);
#endif
	*(ff) = m / 256;

	ff++;
      }
}

void
temporal_filter_planes (int idx, int w, int h, int t)
{
  uint32_t r, i, c, m;
  int32_t d;
  int x;

  uint8_t *f1 = frame1[idx];
  uint8_t *f2 = frame2[idx];
  uint8_t *f3 = frame3[idx];
  uint8_t *f4 = frame4[idx];
  uint8_t *f5 = frame5[idx];
  uint8_t *f6 = frame6[idx];
  uint8_t *f7 = frame7[idx];
  uint8_t *of = outframe[idx];

  if (t == 0)			// shortcircuit filter if t = 0...
    {
      memcpy (of, f4, w * h);
      return;
    }

  if (w == lwidth)		/* we process the luma-plane */
    {
      t *= 256;
      for (x = 0; x < (w * h); x++)
	{
	  r = transform_L16[*(f4 - 1 - w)];
	  r += transform_L16[*(f4 - w)];
	  r += transform_L16[*(f4 + 1 - w)];
	  r += transform_L16[*(f4 - 1)];
	  r += transform_L16[*(f4)];
	  r += transform_L16[*(f4 + 1)];
	  r += transform_L16[*(f4 - 1 + w)];
	  r += transform_L16[*(f4 + w)];
	  r += transform_L16[*(f4 + 1 + w)];
	  r /= 9;

	  i = transform_L16[*(f4)] * (t + 1);
	  c = (t + 1);

	  d = t - abs (r - transform_L16[*(f1)]);
	  d = d < 0 ? 0 : d;
	  c = c + d;
	  i = i + d * transform_L16[*(f1)];

	  d = t - abs (r - transform_L16[*(f2)]);
	  d = d < 0 ? 0 : d;
	  c = c + d;
	  i = i + d * transform_L16[*(f2)];

	  d = t - abs (r - transform_L16[*(f3)]);
	  d = d < 0 ? 0 : d;
	  c = c + d;
	  i = i + d * transform_L16[*(f3)];

	  d = t - abs (r - transform_L16[*(f5)]);
	  d = d < 0 ? 0 : d;
	  c = c + d;
	  i = i + d * transform_L16[*(f5)];

	  d = t - abs (r - transform_L16[*(f6)]);
	  d = d < 0 ? 0 : d;
	  c = c + d;
	  i = i + d * transform_L16[*(f6)];

	  d = t - abs (r - transform_L16[*(f7)]);
	  d = d < 0 ? 0 : d;
	  c = c + d;
	  i = i + d * transform_L16[*(f7)];

	  m = (i / c) + 1;	// The "+1" is a needed correction as its allways rounded down here ...
	  m = m > 65535 ? 65535 : m;
	  *(of) = transform_G8[m];

	  f1++;
	  f2++;
	  f3++;
	  f4++;
	  f5++;
	  f6++;
	  f7++;
	  of++;
	}
    }
  else				/* we process one of the chroma-planes */
    for (x = 0; x < (w * h); x++)
      {
	r = *(f4 - 1 - w);
	r += *(f4 - w);
	r += *(f4 + 1 - w);
	r += *(f4 - 1);
	r += *(f4);
	r += *(f4 + 1);
	r += *(f4 - 1 + w);
	r += *(f4 + w);
	r += *(f4 + 1 + w);
	r /= 9;

	i = *(f4) * (t + 1);
	c = (t + 1);

	d = t - abs (r - *(f1));
	d = d < 0 ? 0 : d;
	c = c + d;
	i = i + d * *(f1);

	d = t - abs (r - *(f2));
	d = d < 0 ? 0 : d;
	c = c + d;
	i = i + d * *(f2);

	d = t - abs (r - *(f3));
	d = d < 0 ? 0 : d;
	c = c + d;
	i = i + d * *(f3);

	d = t - abs (r - *(f5));
	d = d < 0 ? 0 : d;
	c = c + d;
	i = i + d * *(f5);

	d = t - abs (r - *(f6));
	d = d < 0 ? 0 : d;
	c = c + d;
	i = i + d * *(f6);

	d = t - abs (r - *(f7));
	d = d < 0 ? 0 : d;
	c = c + d;
	i = i + d * *(f7);

	m = (i / c) + 1;	// The "+1" is a needed correction as its allways rounded down here ...
	m = m > 255 ? 255 : m;
	*(of) = m;

	f1++;
	f2++;
	f3++;
	f4++;
	f5++;
	f6++;
	f7++;
	of++;
      }
}

void renoise (uint8_t * frame, int w, int h, int level )
{
static int initialized=0;
static uint8_t random[8192];
int i;

if(level==0) return;

if(initialized!=1)
{
	for(i=0;i<8192;i++)
		random[i]=(i+i+i+i*i*i+1-random[i-1]+random[i-20]*random[i-5]*random[i-25])&255;
	initialized=1;
}

for(i=0;i<(w*h);i++)
	*(frame+i)=(*(frame+i)*(255-level)+random[i&8191]*level)/255;
}

void filter_plane_median ( uint8_t * plane, int w, int h, int level)
{
	int x,y;
	int sx,sy;
	int i;
	uint32_t median;
	int max_index;
	uint8_t t=level;
	uint8_t pix_list[255];
	uint8_t ref_pixel;
	uint8_t chk_pixel;

	for(y=0;y<h;y++)
		for(x=0;x<w;x++)
		{
		ref_pixel = *(plane+(x)+(y)*w);

		max_index = 0;
		for(sy=-6;sy<=6;sy++)
			for(sx=-6;sx<=6;sx++)
			{
			chk_pixel = *(plane+(x+sx)+(y+sy)*w);
			if(abs(chk_pixel-ref_pixel)<t)
				{
				pix_list[max_index]=chk_pixel;
				max_index++;
				}
			}

		// median-approximation
		median = 0;
		for(i=0;i<max_index;i++)
		{
		median += pix_list[i];
		}
		if(max_index!=0)
			median /= max_index;
		else
			median = ref_pixel;
 
		*(plane+x+y*w) = median;

		}
}

/***********************************************************
 * Main Loop                                               *
 ***********************************************************/

int
main (int argc, char *argv[])
{
  char c;
  int fd_in = 0;
  int fd_out = 1;
  int errno = 0;
  char *msg = NULL;
  y4m_ratio_t rx, ry;
  y4m_frame_info_t iframeinfo;
  y4m_stream_info_t istreaminfo;
  y4m_frame_info_t oframeinfo;
  y4m_stream_info_t ostreaminfo;

  mjpeg_log (LOG_INFO,
	     "-----------------------------------------------------");
  mjpeg_log (LOG_INFO, "mjpeg-tools yuvdenoise version %s", VERSION);
  mjpeg_log (LOG_INFO,
	     "-----------------------------------------------------");

  while ((c = getopt (argc, argv, "hvs:t:g:m:r:")) != -1)
    {
      switch (c)
	{
	case 'h':
	  {
	    mjpeg_log (LOG_INFO, " Usage");
	    mjpeg_log (LOG_INFO, " =====\n");
	    mjpeg_log (LOG_INFO,
		       " This is a spatio-temporal noise-filter for Y4M-video-streams. You can");
	    mjpeg_log (LOG_INFO,
		       " control its behaviour with the following options:\n");
	    mjpeg_log (LOG_INFO,
		       " -s y,u,v     This sets the thresholds [0..255] for the spatial noise-");
	    mjpeg_log (LOG_INFO,
		       "              filter. If you set this too high, expect blurring your");
	    mjpeg_log (LOG_INFO, "              images.\n");
	    mjpeg_log (LOG_INFO,
		       " -g y,u,v     This sets the mixing-level [0..255] for the gauss-filter.");
	    mjpeg_log (LOG_INFO,
		       "              The default-values for the chroma-planes are sane. Believe");
	    mjpeg_log (LOG_INFO,
		       "              me. You only should change them, if you have noise-free");
	    mjpeg_log (LOG_INFO,
		       "              chroma-planes... It sometimes may be usefull, to smooth");
	    mjpeg_log (LOG_INFO,
		       "              the luma-plane, too. Default is to only smooth it very very");
	    mjpeg_log (LOG_INFO,"              little, just to get rid of jaggies.\n");
	    mjpeg_log (LOG_INFO," -t y,u,v     This sets the thresholds for the temporal noise-filter.");
	    mjpeg_log (LOG_INFO,"              Values above 12 may introduce ghosts. But usually you can't");
	    mjpeg_log (LOG_INFO,"              see them in a sequence of moving frames until you pass 18.");
	    mjpeg_log (LOG_INFO,"              This is due to the fact that our brain supresses these.");
	    mjpeg_log (LOG_INFO," -m y,u,v     This sets the thresholds for the (pseudo) median-noise-filter.\n");
	    mjpeg_log (LOG_INFO," -r y,u,v     add this amount of static noise to the denoised image.\n");

	    exit (0);
	    break;
	  }
	case 'v':
	  {
	    verbose = 1;
	    break;
	  }
	case 's':
	  {
	    sscanf (optarg, "%i,%i,%i", &spat_Y_thres, &spat_U_thres,
		    &spat_V_thres);
	    break;
	  }
	case 't':
	  {
	    sscanf (optarg, "%i,%i,%i", &temp_Y_thres, &temp_U_thres,
		    &temp_V_thres);
	    break;
	  }
	case 'g':
	  {
	    sscanf (optarg, "%i,%i,%i", &gauss_Y, &gauss_U, &gauss_V);
	    break;
	  }
	case 'm':
	  {
	    sscanf (optarg, "%i,%i,%i", &med_Y_thres, &med_U_thres, &med_V_thres);
	    break;
	  }
	case 'r':
	  {
	    sscanf (optarg, "%i,%i,%i", &renoise_Y, &renoise_U, &renoise_V);
	    break;
	  }
	case '?':
	default:
	  exit (1);
	}
    }

  mjpeg_log (LOG_INFO, "Using the following thresholds:");
  mjpeg_log (LOG_INFO, " 3D-Median-Filter     [Y,U,V] : [%i,%i,%i]",
	     med_Y_thres, med_U_thres, med_V_thres);
  mjpeg_log (LOG_INFO, " Spatial-Noise-Filter [Y,U,V] : [%i,%i,%i]",
	     spat_Y_thres, spat_U_thres, spat_V_thres);
  mjpeg_log (LOG_INFO, " Gauss-Lowpass-Filter [Y,U,V] : [%i,%i,%i]", gauss_Y,
	     gauss_U, gauss_V);
  mjpeg_log (LOG_INFO, "Temporal-Noise-Filter [Y,U,V] : [%i,%i,%i]",
	     temp_Y_thres, temp_U_thres, temp_V_thres);
  mjpeg_log (LOG_INFO, "Renoise               [Y,U,V] : [%i,%i,%i]",
	     renoise_Y, renoise_U, renoise_V);

  /* initialize stream-information */
  y4m_accept_extensions (1);
  y4m_init_stream_info (&istreaminfo);
  y4m_init_frame_info (&iframeinfo);
  y4m_init_stream_info (&ostreaminfo);
  y4m_init_frame_info (&oframeinfo);

  /* open input stream */
  if ((errno = y4m_read_stream_header (fd_in, &istreaminfo)) != Y4M_OK)
    {
      mjpeg_log (LOG_ERROR, "Couldn't read YUV4MPEG header: %s!",
		 y4m_strerr (errno));
      exit (1);
    }

  /* get format information */
  width = y4m_si_get_width (&istreaminfo);
  height = y4m_si_get_height (&istreaminfo);
  input_chroma_subsampling = y4m_si_get_chroma (&istreaminfo);
  input_interlaced = y4m_si_get_interlace (&istreaminfo);
  mjpeg_log (LOG_INFO, "Y4M-Stream %ix%i(%s)",
	     width,
	     height, y4m_chroma_description (input_chroma_subsampling));

  lwidth = width;
  lheight = height;

  // Setup the denoiser to use the appropriate chroma processing
  if (input_chroma_subsampling == Y4M_CHROMA_420JPEG ||
      input_chroma_subsampling == Y4M_CHROMA_420MPEG2 ||
      input_chroma_subsampling == Y4M_CHROMA_420PALDV)
    msg = "Processing Mode : 4:2:0";
  else if (input_chroma_subsampling == Y4M_CHROMA_411)
    msg = "Processing Mode : 4:1:1";
  else if (input_chroma_subsampling == Y4M_CHROMA_422)
    msg = "Processing Mode : 4:2:2";
  else if (input_chroma_subsampling == Y4M_CHROMA_444)
    msg = "Processing Mode : 4:4:4";
  else
    mjpeg_error_exit1 (" ### Unsupported Y4M Chroma sampling ### ");

  rx = y4m_chroma_ss_x_ratio (input_chroma_subsampling);
  ry = y4m_chroma_ss_y_ratio (input_chroma_subsampling);
  cwidth = width / rx.d;
  cheight = height / ry.d;

  mjpeg_log (LOG_INFO, "%s %s", msg,
	     (input_interlaced ==
	      Y4M_ILACE_NONE) ? "progressive" : "interlaced");
  mjpeg_log (LOG_INFO, "Luma-Plane      : %ix%i pixels", lwidth, lheight);
  mjpeg_log (LOG_INFO, "Chroma-Plane    : %ix%i pixels", cwidth, cheight);

  if (input_interlaced != Y4M_ILACE_NONE)
    {
      // process the fields as images side by side
      lwidth *= 2;
      cwidth *= 2;
      lheight /= 2;
      cheight /= 2;
    }

  y4m_si_set_interlace (&ostreaminfo, y4m_si_get_interlace (&istreaminfo));
  y4m_si_set_chroma (&ostreaminfo, y4m_si_get_chroma (&istreaminfo));
  y4m_si_set_width (&ostreaminfo, width);
  y4m_si_set_height (&ostreaminfo, height);
  y4m_si_set_framerate (&ostreaminfo, y4m_si_get_framerate (&istreaminfo));
  y4m_si_set_sampleaspect (&ostreaminfo,
			   y4m_si_get_sampleaspect (&istreaminfo));

  /* write the outstream header */
  y4m_write_stream_header (fd_out, &ostreaminfo);

  /* now allocate the needed buffers */
  {
    /* calculate the memory offset needed to allow the processing
     * functions to overshot. The biggest overshot is needed for the
     * MC-functions, so we'll use 8*width...
     */
    buff_offset = lwidth * 8;
    buff_size = buff_offset * 2 + lwidth * lheight;

    frame1[0] = buff_offset + (uint8_t *) malloc (buff_size);
    frame1[1] = buff_offset + (uint8_t *) malloc (buff_size);
    frame1[2] = buff_offset + (uint8_t *) malloc (buff_size);

    frame2[0] = buff_offset + (uint8_t *) malloc (buff_size);
    frame2[1] = buff_offset + (uint8_t *) malloc (buff_size);
    frame2[2] = buff_offset + (uint8_t *) malloc (buff_size);

    frame3[0] = buff_offset + (uint8_t *) malloc (buff_size);
    frame3[1] = buff_offset + (uint8_t *) malloc (buff_size);
    frame3[2] = buff_offset + (uint8_t *) malloc (buff_size);

    frame4[0] = buff_offset + (uint8_t *) malloc (buff_size);
    frame4[1] = buff_offset + (uint8_t *) malloc (buff_size);
    frame4[2] = buff_offset + (uint8_t *) malloc (buff_size);

    frame5[0] = buff_offset + (uint8_t *) malloc (buff_size);
    frame5[1] = buff_offset + (uint8_t *) malloc (buff_size);
    frame5[2] = buff_offset + (uint8_t *) malloc (buff_size);

    frame6[0] = buff_offset + (uint8_t *) malloc (buff_size);
    frame6[1] = buff_offset + (uint8_t *) malloc (buff_size);
    frame6[2] = buff_offset + (uint8_t *) malloc (buff_size);

    frame7[0] = buff_offset + (uint8_t *) malloc (buff_size);
    frame7[1] = buff_offset + (uint8_t *) malloc (buff_size);
    frame7[2] = buff_offset + (uint8_t *) malloc (buff_size);

    frame8[0] = buff_offset + (uint8_t *) malloc (buff_size);
    frame8[1] = buff_offset + (uint8_t *) malloc (buff_size);
    frame8[2] = buff_offset + (uint8_t *) malloc (buff_size);

    frame9[0] = buff_offset + (uint8_t *) malloc (buff_size);
    frame9[1] = buff_offset + (uint8_t *) malloc (buff_size);
    frame9[2] = buff_offset + (uint8_t *) malloc (buff_size);

    framea[0] = buff_offset + (uint8_t *) malloc (buff_size);
    framea[1] = buff_offset + (uint8_t *) malloc (buff_size);
    framea[2] = buff_offset + (uint8_t *) malloc (buff_size);

    frameb[0] = buff_offset + (uint8_t *) malloc (buff_size);
    frameb[1] = buff_offset + (uint8_t *) malloc (buff_size);
    frameb[2] = buff_offset + (uint8_t *) malloc (buff_size);

    framec[0] = buff_offset + (uint8_t *) malloc (buff_size);
    framec[1] = buff_offset + (uint8_t *) malloc (buff_size);
    framec[2] = buff_offset + (uint8_t *) malloc (buff_size);

    framed[0] = buff_offset + (uint8_t *) malloc (buff_size);
    framed[1] = buff_offset + (uint8_t *) malloc (buff_size);
    framed[2] = buff_offset + (uint8_t *) malloc (buff_size);

    framee[0] = buff_offset + (uint8_t *) malloc (buff_size);
    framee[1] = buff_offset + (uint8_t *) malloc (buff_size);
    framee[2] = buff_offset + (uint8_t *) malloc (buff_size);

    framef[0] = buff_offset + (uint8_t *) malloc (buff_size);
    framef[1] = buff_offset + (uint8_t *) malloc (buff_size);
    framef[2] = buff_offset + (uint8_t *) malloc (buff_size);

    outframe[0] = buff_offset + (uint8_t *) malloc (buff_size);
    outframe[1] = buff_offset + (uint8_t *) malloc (buff_size);
    outframe[2] = buff_offset + (uint8_t *) malloc (buff_size);

    scratchplane = buff_offset + (uint8_t *) malloc (buff_size);

    mjpeg_log (LOG_INFO, "Buffers allocated.");
  }

  /* initialize motion_library */
  init_motion_search ();

  /* init gamma transfer functions */
  init_gamma_transform_LUTs ();

  /* read every frame until the end of the input stream and process it */
  while (Y4M_OK == (errno = y4m_read_frame (fd_in,
					    &istreaminfo,
					    &iframeinfo, frame1)))
    {
      static uint32_t frame_nr = 0;
      uint8_t *temp[3];

      frame_nr++;

      	gauss_filter_plane (frame1[0], lwidth, lheight, gauss_Y);
      	gauss_filter_plane (frame1[1], cwidth, cheight, gauss_U);
      	gauss_filter_plane (frame1[2], cwidth, cheight, gauss_V);

	filter_plane_median (frame1[0], lwidth, lheight, med_Y_thres);
	filter_plane_median (frame1[1], cwidth, cheight, med_U_thres);
	filter_plane_median (frame1[2], cwidth, cheight, med_V_thres);

	adaptive_filter_plane (frame1[0], lwidth, lheight, spat_Y_thres);
	adaptive_filter_plane (frame1[1], cwidth, cheight, spat_U_thres);
	adaptive_filter_plane (frame1[2], cwidth, cheight, spat_V_thres);

	temporal_filter_planes (0, lwidth, lheight, temp_Y_thres);
      	temporal_filter_planes (1, cwidth, cheight, temp_U_thres);
      	temporal_filter_planes (2, cwidth, cheight, temp_V_thres);

      	renoise (outframe[0], lwidth, lheight, renoise_Y );
      	renoise (outframe[1], cwidth, cheight, renoise_U );
      	renoise (outframe[2], cwidth, cheight, renoise_V );

      if (frame_nr >= 4)
	y4m_write_frame (fd_out, &ostreaminfo, &oframeinfo, outframe);

      // rotate buffer pointers to rotate input-buffers
      temp[0] = framef[0];
      temp[1] = framef[1];
      temp[2] = framef[2];

      framef[0] = framee[0];
      framef[1] = framee[1];
      framef[2] = framee[2];

      framee[0] = framed[0];
      framee[1] = framed[1];
      framee[2] = framed[2];

      framed[0] = framec[0];
      framed[1] = framec[1];
      framed[2] = framec[2];

      framec[0] = frameb[0];
      framec[1] = frameb[1];
      framec[2] = frameb[2];

      frameb[0] = framea[0];
      frameb[1] = framea[1];
      frameb[2] = framea[2];

      framea[0] = frame9[0];
      framea[1] = frame9[1];
      framea[2] = frame9[2];

      frame9[0] = frame8[0];
      frame9[1] = frame8[1];
      frame9[2] = frame8[2];

      frame8[0] = frame7[0];
      frame8[1] = frame7[1];
      frame8[2] = frame7[2];

      frame7[0] = frame6[0];
      frame7[1] = frame6[1];
      frame7[2] = frame6[2];

      frame6[0] = frame5[0];
      frame6[1] = frame5[1];
      frame6[2] = frame5[2];

      frame5[0] = frame4[0];
      frame5[1] = frame4[1];
      frame5[2] = frame4[2];

      frame4[0] = frame3[0];
      frame4[1] = frame3[1];
      frame4[2] = frame3[2];

      frame3[0] = frame2[0];
      frame3[1] = frame2[1];
      frame3[2] = frame2[2];

      frame2[0] = frame1[0];
      frame2[1] = frame1[1];
      frame2[2] = frame1[2];

      frame1[0] = temp[0];
      frame1[1] = temp[1];
      frame1[2] = temp[2];

    }
	// write out the left frames...
	y4m_write_frame (fd_out, &ostreaminfo, &oframeinfo, frame4);
	y4m_write_frame (fd_out, &ostreaminfo, &oframeinfo, frame3);
	y4m_write_frame (fd_out, &ostreaminfo, &oframeinfo, frame2);
	y4m_write_frame (fd_out, &ostreaminfo, &oframeinfo, frame1);

  /* free allocated buffers */
  {
    free (frame1[0] - buff_offset);
    free (frame1[1] - buff_offset);
    free (frame1[2] - buff_offset);

    free (frame2[0] - buff_offset);
    free (frame2[1] - buff_offset);
    free (frame2[2] - buff_offset);

    free (frame3[0] - buff_offset);
    free (frame3[1] - buff_offset);
    free (frame3[2] - buff_offset);

    free (frame4[0] - buff_offset);
    free (frame4[1] - buff_offset);
    free (frame4[2] - buff_offset);

    free (frame5[0] - buff_offset);
    free (frame5[1] - buff_offset);
    free (frame5[2] - buff_offset);

    free (frame6[0] - buff_offset);
    free (frame6[1] - buff_offset);
    free (frame6[2] - buff_offset);

    free (frame7[0] - buff_offset);
    free (frame7[1] - buff_offset);
    free (frame7[2] - buff_offset);

    free (outframe[0] - buff_offset);
    free (outframe[1] - buff_offset);
    free (outframe[2] - buff_offset);

	free (scratchplane - buff_offset);

    mjpeg_log (LOG_INFO, "Buffers freed.");
  }

  /* did stream end unexpectedly ? */
  if (errno != Y4M_ERR_EOF)
    mjpeg_error_exit1 ("%s", y4m_strerr (errno));

  /* Exit gently */
  return (0);
}
