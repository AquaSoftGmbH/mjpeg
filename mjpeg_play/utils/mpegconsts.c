
/*
 *  mpegconsts.c:  Video format constants for MPEG and utilities for display
 *                 and conversion to format used for yuv4mpeg
 *
 *  Copyright (C) 2001 Andrew Stevens <andrew.stevens@philips.com>
 *  Copyright (C) 2001 Matthew Marjanovic <maddog@mir.com>
 *
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU General Public License
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <config.h>
#include "mpegconsts.h"
#include "yuv4mpeg.h"
#include "yuv4mpeg_intern.h"


#define Y4M_ASPECT_MULT (1<<20)   /* temporary! until aspect fixed up (mdog) */

static y4m_ratio_t
mpeg_framerates[] = {
  Y4M_FPS_UNKNOWN,
  Y4M_FPS_NTSC_FILM,
  Y4M_FPS_FILM,
  Y4M_FPS_PAL,
  Y4M_FPS_NTSC,
  Y4M_FPS_30,
  Y4M_FPS_PAL_FIELD,
  Y4M_FPS_NTSC_FIELD,
  Y4M_FPS_60
};


#define MPEG_NUM_RATES (sizeof(mpeg_framerates)/sizeof(mpeg_framerates[0]))
const mpeg_framerate_code_t mpeg_num_framerates = MPEG_NUM_RATES;

static const char *
framerate_definitions[MPEG_NUM_RATES] =
{
  "illegal", 
  "24000.0/1001.0 (NTSC 3:2 pulldown converted FILM)",
  "24.0 (NATIVE FILM)",
  "25.0 (PAL/SECAM VIDEO / converted FILM)",
  "30000.0/1001.0 (NTSC VIDEO)",
  "30.0",
  "50.0 (PAL FIELD RATE)",
  "60000.0/1001.0 (NTSC FIELD RATE)",
  "60.0"
};


static const char *mpeg1_aspect_ratio_definitions[] =
{
	"1:1 (square pixels)",
	"1:0.6735",
	"1:0.7031 (16:9 Anamorphic PAL/SECAM for 720x578/352x288 images)",
    "1:0.7615",
	"1:0.8055",
	"1:0.8437 (16:9 Anamorphic NTSC for 720x480/352x240 images)",
	"1:0.8935",
	"1:0.9375 (4:3 PAL/SECAM for 720x578/352x288 images)",
	"1:0.9815",
	"1:1.0255",
	"1:1:0695",
	"1:1.125  (4:3 NTSC for 720x480/352x240 images)",
	"1:1575",
	"1:1.2015"
};

static const double mpeg1_aspect_ratios[] =
{
	1.0,
	0.6735,
	0.7031,
	0.7615,
	0.8055,
	0.8437,
	0.8935,
	0.9375,
	0.9815,
	1.0255,
	1.0695,
	1.125,
	1.1575,
	1.2015
};


static const char *mpeg2_aspect_ratio_definitions[] = 
{
	"1:1 display",
	"4:3 display",
	"16:9 display",
	"2.21:1 display"
};


static const double mpeg2_aspect_ratios[] =
{
	1.0,
	4.0/3.0,
	16.0/9.0,
	2.21/1.0
};

static const char **aspect_ratio_definitions[2] = 
{
	mpeg1_aspect_ratio_definitions,
	mpeg2_aspect_ratio_definitions
};

static const double *mpeg_aspect_ratios[2] = 
{
	mpeg1_aspect_ratios,
	mpeg2_aspect_ratios
};

const mpeg_aspect_code_t mpeg_num_aspect_ratios[2] = 
{
	sizeof(mpeg1_aspect_ratios)/sizeof(double),
    sizeof(mpeg2_aspect_ratios)/sizeof(double)
};

/*
 * Convert MPEG frame-rate code to corresponding frame-rate
 */

const y4m_ratio_t
mpeg_framerate( mpeg_framerate_code_t code )
{
	if( code <= 0 || code > mpeg_num_framerates )
		return y4m_fps_UNKNOWN;
	else
		return mpeg_framerates[code];
}

/*
 * Look-up MPEG frame rate code for a (exact) frame rate.
 */


const mpeg_framerate_code_t 
mpeg_framerate_code( y4m_ratio_t framerate )
{
  mpeg_framerate_code_t i;
  
  y4m_ratio_reduce(&framerate);
  for (i = 1; i < mpeg_num_framerates; ++i) {
    if (Y4M_RATIO_EQL(framerate, mpeg_framerates[i]))
      return i;
  }
  return 0;
}


/* small enough to distinguish 1/1000 from 1/1001 */
#define MPEG_FPS_TOLERANCE 0.0001


const y4m_ratio_t
mpeg_conform_framerate( double fps )
{
  mpeg_framerate_code_t i;
  y4m_ratio_t result;

  /* try to match it to a standard frame rate */
  for (i = 1; i < mpeg_num_framerates; i++) {
    double deviation = 1.0 - (Y4M_RATIO_DBL(mpeg_framerates[i]) / fps);
    if ( (deviation > -MPEG_FPS_TOLERANCE) &&
	 (deviation < +MPEG_FPS_TOLERANCE) )
      return mpeg_framerates[i];
  }
  /* no luck?  just turn it into a ratio (6 decimal place accuracy) */
  result.n = (fps * 1000000.0) + 0.5;
  result.d = 1000000;
  y4m_ratio_reduce(&result);
  return result;
}

  

/*
 * Convert MPEG aspect-ratio code to corresponding aspect-ratio
 */

const double 
mpeg_aspect_ratio( int mpeg_version,  mpeg_aspect_code_t code )
{
	if( mpeg_version < 1 || mpeg_version > 2 )
		return 0.0;
	if( code <= 0 || code > mpeg_num_aspect_ratios[mpeg_version-1] )
		return 0.0;
	else
		return mpeg_aspect_ratios[mpeg_version-1][code-1];
}

/*
 * Look-up MPEG frame rate code for a frame rate - tolerance
 * is Y4M_FPS_MULT used by YUV4MPEG (see yuv4mpeg_intern.h)
 *
 * WARNING: The semantics of aspect ratio coding *changed* between
 * MPEG1 and MPEG2.  In MPEG1 it is the *pixel* aspect ratio. In
 * MPEG2 it is the (far more sensible) aspect ratio of the eventual
 * display.
 *
 */

const mpeg_aspect_code_t 
mpeg_frame_aspect_code( int mpeg_version, double aspect_ratio )
{
	mpeg_aspect_code_t i;
	if( mpeg_version < 1 || mpeg_version > 2 )
		return 0;

	for( i = 1; i < mpeg_num_aspect_ratios[mpeg_version-1]; ++i )
	{
		if( (int)(mpeg_aspect_ratios[mpeg_version-1][i-1]*Y4M_ASPECT_MULT) ==
			(int)(aspect_ratio*Y4M_ASPECT_MULT) )
			return i;
	}

	return 0;
			
}

/*
 * Look-up MPEG explanatory definition string for frame rate code
 *
 */


const char *
mpeg_framerate_code_definition(   mpeg_framerate_code_t code  )
{
	if( code <= 0 || code >=  mpeg_num_framerates )
		return "UNDEFINED: illegal/reserved frame-rate ratio code";

	return framerate_definitions[code];
}

/*
 * Look-up MPEG explanatory definition string aspect ratio code for an
 * aspect ratio code
 *
 */

const char *
mpeg_aspect_code_definition( int mpeg_version,  mpeg_aspect_code_t code  )
{
	if( mpeg_version < 1 || mpeg_version > 2 )
		return "UNDEFINED: illegal MPEG version";
	
	if( code < 1 || code >  mpeg_num_aspect_ratios[mpeg_version-1] )
		return "UNDEFINED: illegal aspect ratio code";

	return aspect_ratio_definitions[mpeg_version-1][code-1];
}


/*
 * Look-up explanatory definition of interlace field order code
 *
 */

const char *
mpeg_interlace_code_definition( int yuv4m_interlace_code )
{
	char *def;
	switch( yuv4m_interlace_code )
	{
	case Y4M_ILACE_NONE :
		def = "unknown/progressive";
		break;
	case Y4M_ILACE_TOP_FIRST :
		def = "interlaced, top field first";
		break;
	case Y4M_ILACE_BOTTOM_FIRST :
		def = "interlaced, bottom field first";
		break;
	default :
		def = "UNDEFINED: illegal video interlacing type-code!";
		break;
	}
	return def;
}
