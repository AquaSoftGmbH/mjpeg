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

#ifndef _ISOFS_FS_H
#define _ISOFS_FS_H

#define MIN_TRACK_SIZE 4*75

#define MIN_ISO_SIZE MIN_TRACK_SIZE

#define ISO_BLOCKSIZE 2048

#define LEN_ISONAME     31
#define MAX_ISONAME     37

#define ISO_FILE        0       
#define ISO_DIRECTORY   2

/* file/dirname's */

/* functions return newly allocated string !! */
gchar*
iso_mkfilename(const gchar fname[]);

gchar*
iso_mkdirname(const gchar dname[]);

/* volume descriptors */

void
set_iso_pvd(gpointer pd, const gchar volume_id[], guint32 iso_size,
	    gconstpointer root_dir, guint32 path_table_l_extent,
	    guint32 path_table_m_extent, guint32 path_table_size);

void 
set_iso_evd(gpointer pd);

/* directory tree */

void
dir_init_new(gpointer dir, guint32 self, guint32 ssize, guint32 parent, 
	     guint32 psize);

void
dir_init_new_su(gpointer dir, guint32 self, guint32 ssize, 
		gconstpointer ssu_data, guint ssu_size, guint32 parent,
		guint32 psize, gconstpointer psu_data, guint psu_size);

void
dir_add_entry(gpointer dir, const gchar name[], guint32 extent,
	      guint32 size, guint8 flags);

void
dir_add_entry_su(gpointer dir, const gchar name[], guint32 extent,
		 guint32 size, guint8 flags, gconstpointer su_data,
		 guint su_size);

guint
dir_calc_record_size(guint namelen, guint su_len);

/* pathtable */

void
pathtable_init(gpointer pt);

gsize
pathtable_get_size(gconstpointer pt);

guint16
pathtable_l_add_entry(gpointer pt, const gchar name[], guint32 extent,
		      guint16 parent);

guint16
pathtable_m_add_entry(gpointer pt, const gchar name[], guint32 extent,
		      guint16 parent);

#endif
