/*
 * Copyright (C) 2001 Calle Lejdfors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Please send all bugreports to: Calle Lejdfors <calle@nsc.liu.se>
 */

#include <stdio.h>
#include "RTjpeg.h"

#ifndef __format_h_
#define __format_h_

/*! 
 * This file defines the format of the RTJ files. This format is
 * an adaption from Justin Schoeman's RTJpeg code. It only includes
 * the quantization info and width/height of the picture.
 *
 * This header *should* be stored in big-endian format.
 *
 */



#define RTJ_HEADERSIZE sizeof(struct rtj_header)
#define RTJ_TBLSIZE 128

struct rtj_header {
  __u8 desc[8]; /* RTJPEG20 */
  __u32 width;
  __u32 height;
  __u32 tbls[RTJ_TBLSIZE];
};


/* initializeses header fields */
void rtj_init_header( struct rtj_header* );

/* write header on FILE* */
void rtj_write_header( const struct rtj_header*, FILE* );

/* 
 * on failure: return 0 on and resets file position.
 * on success: return 1 and leaves file at end of header 
*/
int rtj_read_header( struct rtj_header*, FILE*  ); 


#endif /* __format_h_ */







