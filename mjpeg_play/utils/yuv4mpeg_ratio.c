/*
 *  yuv4mpeg_ratio.c:  Functions for dealing with y4m_ratio_t datatype.
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

#include <config.h>

#include <string.h>
#include "yuv4mpeg.h"
#include "yuv4mpeg_intern.h"


/* useful list of standard framerates */
const y4m_ratio_t y4m_fps_UNKNOWN    = Y4M_FPS_UNKNOWN;
const y4m_ratio_t y4m_fps_NTSC_FILM  = Y4M_FPS_NTSC_FILM;
const y4m_ratio_t y4m_fps_FILM       = Y4M_FPS_FILM;
const y4m_ratio_t y4m_fps_PAL        = Y4M_FPS_PAL;
const y4m_ratio_t y4m_fps_NTSC       = Y4M_FPS_NTSC;
const y4m_ratio_t y4m_fps_30         = Y4M_FPS_30;
const y4m_ratio_t y4m_fps_PAL_FIELD  = Y4M_FPS_PAL_FIELD;
const y4m_ratio_t y4m_fps_NTSC_FIELD = Y4M_FPS_NTSC_FIELD;
const y4m_ratio_t y4m_fps_60         = Y4M_FPS_60;

/* useful list of standard pixel aspect ratios */
const y4m_ratio_t y4m_sar_UNKNOWN        = Y4M_SAR_UNKNOWN;
const y4m_ratio_t y4m_sar_SQUARE         = Y4M_SAR_SQUARE;
const y4m_ratio_t y4m_sar_SQANA_16_9     = Y4M_SAR_SQANA_16_9;
const y4m_ratio_t y4m_sar_NTSC_CCIR601   = Y4M_SAR_NTSC_CCIR601;
const y4m_ratio_t y4m_sar_NTSC_16_9      = Y4M_SAR_NTSC_16_9;
const y4m_ratio_t y4m_sar_NTSC_SVCD_4_3  = Y4M_SAR_NTSC_SVCD_4_3;
const y4m_ratio_t y4m_sar_NTSC_SVCD_16_9 = Y4M_SAR_NTSC_SVCD_16_9;
const y4m_ratio_t y4m_sar_PAL_CCIR601    = Y4M_SAR_PAL_CCIR601;
const y4m_ratio_t y4m_sar_PAL_16_9       = Y4M_SAR_PAL_16_9;
const y4m_ratio_t y4m_sar_PAL_SVCD_4_3   = Y4M_SAR_PAL_SVCD_4_3;
const y4m_ratio_t y4m_sar_PAL_SVCD_16_9  = Y4M_SAR_PAL_SVCD_16_9;


/*
 *  Euler's algorithm for greatest common divisor
 */

static int gcd(int a, int b)
{
  a = (a >= 0) ? a : -a;
  b = (b >= 0) ? b : -b;

  while (b > 0) {
    int x = b;
    b = a % b;
    a = x;
  }
  return a;
}
    

/*************************************************************************
 *
 * Remove common factors from a ratio
 *
 *************************************************************************/


void y4m_ratio_reduce(y4m_ratio_t *r)
{
  int d;
  if ((r->n == 0) && (r->d == 0)) return;  /* "unknown" */
  d = gcd(r->n, r->d);
  r->n /= d;
  r->d /= d;
}



/*************************************************************************
 *
 * Parse "nnn:ddd" into a ratio
 *
 * returns:         Y4M_OK  - success
 *           Y4M_ERR_RANGE  - range error 
 *
 *************************************************************************/

int y4m_parse_ratio(y4m_ratio_t *r, const char *s)
{
  char *t = strchr(s, ':');
  if (t == NULL) return Y4M_ERR_RANGE;
  r->n = atoi(s);
  r->d = atoi(t+1);
  if (r->d < 0) return Y4M_ERR_RANGE;
  /* 0:0 == unknown, so that is ok, otherwise zero denominator is bad */
  if ((r->d == 0) && (r->n != 0)) return Y4M_ERR_RANGE;
  y4m_ratio_reduce(r);
  return Y4M_OK;
}


/********************************************************************
 *
 *  Take a wild guess at the SAR (sample aspect ratio) by checking the
 *  frame width and height for common values.  
 *  There is, of course, no way to tell a 16:9 anamorphic frame from a
 *  good old 4:3 frame.  So we have top rely on a code set by the user :-(
 *  For familiarity we use the MPEG2 codes.
 *
 *********************************************************************/

y4m_ratio_t y4m_guess_sample_ratio(int width, 
								   int height, 
								   mpeg_aspect_code_t image_aspect_code)
{
	y4m_ratio_t sar;
	y4m_ratio_t ccir601_ntsc;
	y4m_ratio_t ccir601_pal;
	y4m_ratio_t square;
	y4m_ratio_t svcd_pal;
	y4m_ratio_t svcd_ntsc;
	
	if( image_aspect_code == 1)	/* 1:1 Image*/
	{
		sar.n = width;
		sar.d = height;
		y4m_ratio_reduce( &sar );
		return sar;
	} else if( image_aspect_code == 2 ) /* 4:3 image */
	{
		square =  y4m_sar_SQUARE;
		ccir601_ntsc = y4m_sar_NTSC_CCIR601;
		ccir601_pal = y4m_sar_PAL_CCIR601;
		svcd_ntsc = y4m_sar_NTSC_SVCD_4_3;
		svcd_pal = y4m_sar_PAL_SVCD_4_3;
	}
	else if( image_aspect_code == 3 ) /* 16:9 image */
	{
		square =  y4m_sar_SQANA_16_9;
		ccir601_ntsc = y4m_sar_NTSC_16_9;
		ccir601_pal = y4m_sar_PAL_16_9;
		svcd_ntsc = y4m_sar_NTSC_SVCD_16_9;
		svcd_pal = y4m_sar_PAL_SVCD_16_9;
	}
	else
		return y4m_sar_UNKNOWN;

	sar = y4m_sar_UNKNOWN;
	if ((height > 470) && (height < 490)) {
		if ((width > 700) && (width < 724))       /* 704x480 or 720x480 */
			sar = ccir601_ntsc;
		else if ((width > 630) && (width < 650))  /* 640x480 */
			sar = square;
		else if ((width > 470) && (width < 490))  /* 480x480 */
			sar = svcd_ntsc;
	} else if ((height > 230) && (height < 250)) {
		if ((width > 350) && (width < 362))       /* 352x240 or 360x240 */
			sar =  ccir601_ntsc;
		else if ((width > 310) && (width < 330))
			sar =  square;                  /* 320x240 */
	} else if ((height > 565) && (height < 585)) {
		if ((width > 700) && (width < 724))       /* 704x576 or 720x576 */
			sar =  ccir601_pal;
		else if ((width > 760) && (width < 780))  /* 768x576 */
			sar =  square;
		else if ((width > 470) && (width < 490))  /* 480x576 */
			sar = svcd_pal;
	} else if ((height > 280) && (height < 300)) {
		if ((width > 350) && (width < 362))       /* 352x288 */
			sar =  ccir601_pal;
		else if ((width > 380) && (width < 390))  /* 386x288 */
			sar = square;
	}

	return sar;
}
