
/*
 *  mpegconsts.c:  Video format constants for MPEG and utilities for display
 *                 and conversion to format used for yuv4mpeg
 *
 *  Copyright (C) 2001 Andrew Stevens <andrew.stevens@philips.com>
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

static double 
mpeg_framerates[]=
    {0.0, 24000.0/1001.0,24.0,25.0,30000.0/1001.0,30.0,50.0,60000.0/1001.0,60.0};

#define MPEG_NUM_RATES (sizeof(mpeg_framerates)/sizeof(double))
const mpeg_frame_rate_code_t mpeg_num_frame_rates = MPEG_NUM_RATES;

static const char *
frame_rate_definitions[MPEG_NUM_RATES] =
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

const double 
mpeg_frame_rate( mpeg_frame_rate_code_t code )
{
	if( code <= 0 || code > mpeg_num_frame_rates )
		return 0.0;
	else
		return mpeg_framerates[code];
}

/*
 * Look-up MPEG frame rate code for a frame rate - tolerance
 * is Y4M_FPS_MULT used by YUV4MPEG (see yuv4mpeg_intern.h)
 */

const mpeg_frame_rate_code_t 
mpeg_frame_rate_code( double frame_rate )
{
	mpeg_frame_rate_code_t i;
	for( i = 1; i < mpeg_num_frame_rates; ++i )
	{
		if( (int)(mpeg_framerates[i]*Y4M_FPS_MULT) ==
			(int)(frame_rate*Y4M_FPS_MULT) )
			return i;
	}

	return 0;
			
}


/*
 * Convert MPEG frame-rate code to corresponding frame-rate
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
	mpeg_frame_rate_code_t i;
	if( mpeg_version < 1 || mpeg_version > 2 )
		return 0;

	for( i = 1; i < mpeg_num_aspect_ratios[mpeg_version-1]; ++i )
	{
		if( (int)(mpeg_aspect_ratios[mpeg_version-1][i-1]*Y4M_ASPECT_MULT) ==
			(int)(aspect_ratio*Y4M_FPS_MULT) )
			return i;
	}

	return 0;
			
}

/*
 * Look-up MPEG explanatory definition string for frame rate code
 *
 */


const char *
mpeg_frame_rate_code_definition(   mpeg_frame_rate_code_t code  )
{
	if( code <= 0 || code >=  mpeg_num_frame_rates )
		return "UNDEFINED: illegal/reserved frame-rate ratio code\n";

	return frame_rate_definitions[code];
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
		return "UNDEFINED: illegal aspect ratio code\n";

	return aspect_ratio_definitions[mpeg_version-1][code-1];
}
