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

#ifndef VCDFILES_H
#define VCDFILES_H

#include <glib.h>
#include "mpeg.h"

typedef struct {
  gchar *fname;
  guint32 extent;
  guint32 length;
  mpeg_info_t mpeg_info;
} track_info_t;

/* dest must be a ISO_BLOCKSIZE malloc'ed buffer */
void
set_entries_vcd(void *dest, guint ntracks, const track_info_t tracks[]);

void 
set_info_vcd(void *dest, guint ntracks, const track_info_t tracks[]);

/* TODO: optional lot.vcd & psd.vcd files */

#endif /* VCDFILES_H */
