/*
 *  yuv4mpeg_intern.h:  Internal constants for "new" YUV4MPEG streams
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

#ifndef __YUV4MPEG_INTERN_H__
#define __YUV4MPEG_INTERN_H__


#define Y4M_MAGIC "YUV4MPEG2"
#define Y4M_FRAME_MAGIC "FRAME"

#define Y4M_DELIM " "  /* single-character(space) separating tagged fields */

#define Y4M_LINE_MAX 256   /* max number of characters in a header line
                               (including the '\n', but not the '\0') */


/* standard framerate ratios */
#define Y4M_FPS_UNKNOWN    { 0, 0 }
#define Y4M_FPS_NTSC_FILM  { 24000, 1001 }
#define Y4M_FPS_FILM       { 24, 1 }
#define Y4M_FPS_PAL        { 25, 1 }
#define Y4M_FPS_NTSC       { 30000, 1001 }
#define Y4M_FPS_30         { 30, 1 }
#define Y4M_FPS_PAL_FIELD  { 50, 1 }
#define Y4M_FPS_NTSC_FIELD { 60000, 1001 }
#define Y4M_FPS_60         { 60, 1 }

/* standard (MPEG-2) display aspect ratios */
#define Y4M_ASPECT_UNKNOWN { 0, 0 }
#define Y4M_ASPECT_1_1     { 1, 1 }
#define Y4M_ASPECT_4_3     { 4, 3 }
#define Y4M_ASPECT_16_9    { 16, 9 }
#define Y4M_ASPECT_221_100 { 221, 100 }


#endif __YUV4MPEG_INTERN_H__
