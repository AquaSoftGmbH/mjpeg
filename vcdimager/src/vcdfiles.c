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

#include "vcdfiles.h"
#include "bytesex.h"
 
/* random note: most stuff is big endian here */

/* this was easy.... */

#define ENTRIES_ID "ENTRYVCD"
#define ENTRIES_VERSION 0x0200

typedef struct {
  gchar ID[8]      __attribute__((packed)); /* "ENTRYVCD" */
  guint16 version  __attribute__((packed)); /* const 0x0200 -- VCD2.0? */
  guint16 tracks   __attribute__((packed));
  struct { /* all in BCD ! */
    guint8 n       __attribute__((packed));
    guint8 m       __attribute__((packed));
    guint8 s       __attribute__((packed));
    guint8 f       __attribute__((packed));
  } entry[509]     __attribute__((packed));
} EntriesVcd; /* sector 00:04:01 */

void
set_entries_vcd(void *dest, guint ntracks, const track_info_t tracks[])
{
  int n;
  EntriesVcd entries_vcd;

  g_assert(sizeof(EntriesVcd) == 2048);

  g_assert(ntracks <= 509);
  g_assert(ntracks > 0);

  memset(&entries_vcd, 0, sizeof(entries_vcd)); /* paranoia / fixme */
  strncpy(entries_vcd.ID, ENTRIES_ID, 8);
  entries_vcd.version = GUINT16_TO_BE(ENTRIES_VERSION);
  entries_vcd.tracks = GUINT16_TO_BE(ntracks); /* maybe bcd?? / fixme */

  for(n = 0;n < ntracks;n++) {
    guint lsect = tracks[n].extent;

    entries_vcd.entry[n].n = to_bcd8(n+2);
    entries_vcd.entry[n].m = to_bcd8(lsect / (60*75));
    entries_vcd.entry[n].s = to_bcd8(((lsect / 75) % 60)+2);
    entries_vcd.entry[n].f = to_bcd8(lsect % 75);
  }

  memcpy(dest, &entries_vcd, sizeof(entries_vcd));
}

/* thiz waz quite annoying to reverse engineer to the point it is now */

#define INFO_ID       "VIDEO_CD"
#define INFO_VERSION  0x0200
#define INFO_UNKNOWN3 0x08

typedef struct {
  gchar   ID[8]          __attribute__((packed));  /* const "VIDEO_CD" */
  guint16 version        __attribute__((packed));  /* const 0x0200 -- VCD2.0 ? */
  gchar album_desc[16]   __attribute__((packed));  /* album identification/description */
  guint16 vol_count      __attribute__((packed));  /* number of volumes in album */
  guint16 vol_id         __attribute__((packed));  /* number id of this volume in album */
  guint8  pal_flags[13]  __attribute__((packed));  /* bitset of 98 PAL(=set)/NTSC flags */
  guint8  flags          __attribute__((packed));  /* viewing restrictions seems in here */
  gchar   unknown1[4]    __attribute__((packed));  /* has something to do with PSD */
  gchar   unknown2[3]    __attribute__((packed));  /* maybe something in BCD format? */
  guint8  unknown3       __attribute__((packed));  /* const 0x08 */
  gchar   unknown4[2]    __attribute__((packed));  /* ?? */
  gchar   unknown5[2]    __attribute__((packed));  /* ?? */
  
  gchar   unknown6[1992] __attribute__((packed)); /* maybe here are other fields... */
} InfoVcd;

static void
_set_bit(guint8 bitset[], guint bitnum)
{
  guint _byte = bitnum / 8;
  guint _bit  = bitnum % 8;

  bitset[_byte] |= (1 << _bit);
}

void
set_info_vcd(void *dest, guint ntracks, const track_info_t tracks[])
{
  InfoVcd info_vcd;
  gint n;

  g_assert(sizeof(InfoVcd) == 2048);
  g_assert(ntracks <= 98);
  
  memset(&info_vcd, 0, sizeof(info_vcd));
  strncpy(info_vcd.ID, INFO_ID, sizeof(info_vcd.ID));
  info_vcd.version = GUINT16_TO_BE(INFO_VERSION);
  
  strncpy(info_vcd.album_desc, 
	  "by GNU VCDImager",
	  sizeof(info_vcd.album_desc));

  info_vcd.vol_count = GUINT16_TO_BE(0x0001);
  info_vcd.vol_id = GUINT16_TO_BE(0x0001);

  for(n = 0; n < ntracks;n++)
    if(tracks[n].mpeg_info.norm == MPEG_NORM_PAL)
      _set_bit(info_vcd.pal_flags, n);
  
  info_vcd.unknown3 = INFO_UNKNOWN3;

  memcpy(dest, &info_vcd, sizeof(info_vcd));
}

/* eof */
