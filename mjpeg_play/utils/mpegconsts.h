
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

#ifndef __MPEGCONSTS_H__
#define __MPEGCONSTS_H__


#include "yuv4mpeg.h"


typedef unsigned int mpeg_framerate_code_t;
typedef unsigned int mpeg_aspect_code_t;

extern const mpeg_framerate_code_t mpeg_num_framerates;
extern const mpeg_aspect_code_t mpeg_num_aspect_ratios[2];

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Convert MPEG frame-rate code to corresponding frame-rate
 *  y4m_fps_UNKNOWN = { 0, 0 } = Undefined/resrerved code.
 */

const y4m_ratio_t
mpeg_framerate( mpeg_framerate_code_t code );


/*
 * Look-up MPEG frame rate code for a (exact) frame rate.
 *  0 = No MPEG code defined for frame-rate
 */

const mpeg_framerate_code_t 
mpeg_framerate_code( y4m_ratio_t framerate );


/*
 * Convert floating-point framerate to an exact ratio.
 *  Uses a standard MPEG rate, if it finds one within MPEG_FPS_TOLERANCE
 *  (see mpegconsts.c), otherwise uses "fps:1000000" as the ratio.
 */

const y4m_ratio_t
mpeg_conform_framerate( double fps );


/*
 * Convert MPEG aspect ratio code to corresponding aspect ratio
 *
 * WARNING: The semantics of aspect ratio coding *changed* between
 * MPEG1 and MPEG2.  In MPEG1 it is the *pixel* aspect ratio. In
 * MPEG2 it is the (far more sensible) aspect ratio of the eventual
 * display.
 *
 */

const double 
mpeg_aspect_ratio( int mpeg_version,  mpeg_aspect_code_t code );

/*
 * Look-up MPEG aspect ratio code for an aspect ratio - tolerance
 * is Y4M_ASPECT_MULT used by YUV4MPEG (see yuv4mpeg_intern.h)
 *
 * WARNING: The semantics of aspect ratio coding *changed* between
 * MPEG1 and MPEG2.  In MPEG1 it is the *pixel* aspect ratio. In
 * MPEG2 it is the (far more sensible) aspect ratio of the eventual
 * display.
 *
 */

const mpeg_aspect_code_t 
mpeg_frame_aspect_code( int mpeg_version, double aspect_ratio );

/*
 * Look-up MPEG explanatory definition string aspect ratio code for an
 * aspect ratio code
 *
 */

const char *
mpeg_aspect_code_definition( int mpeg_version,  mpeg_aspect_code_t code  );

/*
 * Look-up MPEG explanatory definition string aspect ratio code for an
 * frame rate code
 *
 */

const char *
mpeg_framerate_code_definition( mpeg_framerate_code_t code  );

#ifdef __cplusplus
};
#endif



#endif /* __MPEGCONSTS_H__ */
