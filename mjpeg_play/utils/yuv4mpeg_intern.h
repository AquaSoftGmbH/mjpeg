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


#define Y4M_MAGIC "YUV4MPEG"
#define Y4M_FRAME_MAGIC "FRAME"

#define Y4M_DELIM " "  /* single-character(space) separating tagged fields */

#define Y4M_LINE_MAX 256   /* max number of characters in a header line
                               (including the '\n', but not the '\0') */


#define Y4M_FPS_MULT (1<<20)	/* Nice exact binary multiplier    */
#define Y4M_ASPECT_MULT (1<<20)	/* To ensure no rounding errors    */



#endif /* __YUV4MPEG_INTERN_H__ */
