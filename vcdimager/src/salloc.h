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

/* sector allocation management */

#ifndef _SALLOC_H_
#define _SALLOC_H_

#include <glib.h>

#define SECTOR_NIL ((guint32)(-1))

void salloc_init(void);

void salloc_done(void);

guint32 sector_alloc(guint32 hint,
		     guint32 size);

guint32 salloc_get_highest(void);

#endif /* _SALLOC_H_ */
