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

#ifndef _DIRECTORY_H_
#define _DIRECTORY_H_

void directory_init(void);

void directory_done(void);

void directory_mkdir(const gchar pathname[]);

void directory_mkfile(const gchar pathname[],
		      guint32 start, 
		      guint32 size,
		      gboolean form2, 
		      guint8 filenum);

guint32 directory_get_size(void);

void directory_dump_entries(gpointer buf, 
			    guint32 extent);

void directory_dump_pathtables(gpointer ptl, 
			       gpointer ptm);

#endif /* _DIRECTORY_H_ */

