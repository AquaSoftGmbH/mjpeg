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

#ifndef _TRANSFER_H_
#define _TRANSFER_H_

#include <glib.h>
#include <stdio.h>

glong 
get_regular_file_size(const gchar file_name[]);

void
write_mode2_sector(FILE *fd, gconstpointer data, guint32 extent,
		   guint8 fnum, guint8 cnum, guint8 sm, guint8 ci);

void
write_raw_mode2_sector(FILE *fd, gconstpointer data, guint32 extent);

void
insert_file_raw_mode2(FILE *image_fd, const gchar pathname[], 
		      const gchar iso_pathname[]);

void 
insert_file_mode2_form1(FILE *image_fd, const gchar pathname[], 
			const gchar iso_pathname[]);

void
insert_tree_mode2_form1(FILE *image_fd, const gchar pathname[]);

#endif /* _TRANSFER_H_ */
