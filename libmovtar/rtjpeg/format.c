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

#include <string.h>

#include "format.h"

#define SWAP_WORD(a) ( ((a) << 24) | \
                      (((a) << 8) & 0x00ff0000) | \
                      (((a) >> 8) & 0x0000ff00) | \
        ((unsigned long)(a) >>24) )


const static __u8 *__rtj_magic = "RTJPEG20";

void rtj_init_header( struct rtj_header *hdr )
{
  memcpy( hdr->desc, __rtj_magic, 8 );
  hdr->width = 0;
  hdr->height = 0;
  memset( hdr->tbls, 0, 128 );
}


void rtj_write_header( const struct rtj_header* hdr, FILE* f )
{
#ifdef WORDS_BIGENDIAN /* big-endian */

  fwrite( hdr, RTJ_HEADERSIZE, 1, f );

#else /* little-endian */

  __u32 packbuffer[132];
  int i = 2;
  __u32 *ptr2pack = (__u32*) hdr;
  
  /* copy ident, it is already correctly packed */
  memcpy( packbuffer, ptr2pack, 8 );
  
  while ( i < 132 ) {  /* size of width,height and tbls */
    packbuffer[i++] = SWAP_WORD(ptr2pack[i]); /* repack buffer */
  }

  fwrite( packbuffer, 1, RTJ_HEADERSIZE, f );
  
#endif
}

int rtj_read_header( struct rtj_header* hdr, FILE* f )
{
#ifdef WORDS_BIGENDIAN /* big-endian */
  long pos = ftell(f);

  fread( hdr, RTJ_HEADERSIZE, 1, f );

  if ( strncmp( hdr->desc, __rtj_magic, 8 ) != 0 ) {
    fseek(f,pos,SEEK_SET);
    return 0;
  }

  return 1;

#else /* little-endian */

  int tmp;

  __u32 *trgptr = (__u32*) hdr;
  __u32 *endptr;
  
  long pos = ftell(f);
  
  tmp = fread( trgptr, 1, RTJ_HEADERSIZE, f );
  
  if ( tmp != RTJ_HEADERSIZE ) {
    fseek(f,pos,SEEK_SET);
    return 0;
  }
  
  endptr = trgptr+132;

  trgptr += 2; /* skip desc */

  while ( trgptr != endptr ) {
    __u32 tmp = SWAP_WORD( *trgptr ); /* unpack header */
    *trgptr = tmp;
    trgptr++;
  }

  if ( strncmp( hdr->desc, __rtj_magic,8 ) != 0 ) {
    fseek(f,pos,SEEK_SET);
    return 0;
  }

  return 1;
  
#endif
}


