/*
    $Id$

    Copyright (C) 2000 Herbert Valerio Riedel <hvr@gnu.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <glib.h>
#include <string.h>

#include "mpeg.h"

const guint8 mpeg_sync[] = {
  0x00, 0x00, 0x01, 0xba 
};

const guint8 mpeg_seq_start[] = {
  0x00, 0x00, 0x01, 0xb3 
};

const guint8 mpeg_end_code[] = {
 0x00, 0x00, 0x01, 0xb9
};

const gdouble frame_rates[16]= {
   0.00, 23.976, 24.0, 25.0, 
  29.97, 30.0,   50.0, 59.94, 
  60.00, 00.0, 
};

const gdouble aspect_ratios[16]= { 
  0.0000, 1.0000, 0.6735, 0.7031,
  0.7615, 0.8055, 0.8437, 0.8935,
  0.9375, 0.9815, 1.0255, 1.0695,
  1.1250, 1.1575, 1.2015, 0.0000
};

mpeg_type_t 
mpeg_type(gconstpointer mpeg_block)
{
  const guint8 *data = mpeg_block;

  if (memcmp(data, mpeg_end_code, sizeof(mpeg_end_code)) == 0) {
	return MPEG_UNKNOWN;
  }

  if(memcmp(data, mpeg_sync, sizeof(mpeg_sync))) {
printf("mpeg data: %x %x %x %x\n", data[0], data[1], data[2], data[3]);
    return MPEG_INVALID;
  }

  if((data[15] == 0xe0) ||
     (data[15] == 0xbb && data[24] == 0xe0))
    return MPEG_VIDEO;

  if((data[15] == 0xc0) ||
     (data[15] == 0xbb && data[24] == 0xc0))
    return MPEG_AUDIO;

  return MPEG_UNKNOWN;
}

gboolean 
mpeg_analyze_start_seq(gconstpointer packet, mpeg_info_t* info)
{
  const guint8 *pkt = packet;

  g_return_val_if_fail(info != NULL, FALSE);

  if(memcmp(pkt, mpeg_sync, sizeof(mpeg_sync)))
    return FALSE;

  if(!memcmp(pkt+30, mpeg_seq_start, sizeof(mpeg_seq_start)) ) {
    guint hsize, vsize, aratio, frate, brate, bufsize;

    hsize = pkt[34] << 4;
    hsize += pkt[35] >> 4;

    vsize = (pkt[35] & 0x0f) << 8;
    vsize += pkt[36];

    aratio = pkt[37] >> 4;
    frate = pkt[37] & 0x0f;

    brate = pkt[38] << 10;
    brate += pkt[39] << 2;
    brate += pkt[40] >> 6;

    bufsize = (pkt[40] & 0x3f) << 6;
    bufsize += pkt[41] >> 4;
    
    info->hsize = hsize;
    info->vsize = vsize;
    info->aratio = aspect_ratios[aratio];
    info->frate = frame_rates[frate];
    info->bitrate = 400*brate;
    info->vbvsize = bufsize;

    if(hsize == 352 && vsize == 288 && frate == 3)
      info->norm = MPEG_NORM_PAL;
    else if(hsize == 352 && vsize == 240 && frate == 1)
      info->norm = MPEG_NORM_FILM;
    else if(hsize == 352 && vsize == 240 && frate == 4)
      info->norm = MPEG_NORM_NTSC;
    else
      info->norm = MPEG_NORM_OTHER;

    return TRUE;
  }

  return FALSE;

}
