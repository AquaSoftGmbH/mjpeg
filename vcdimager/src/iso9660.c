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
#include <stdio.h>

#include "iso9660.h"
#include "iso9660_private.h"

#include "bytesex.h"

/* some parameters... */
#define SYSTEM_ID         "CD-RTOS CD-BRIDGE"

#define APPLICATION_ID    "CDI/CDI_VCD.APP;1"
#define VOLUME_SET_ID     ""
#define PREPARER_ID       "VCDImager " VERSION
#define PUBLISHER_ID      ""

static void
pathtable_get_size_and_entries(gconstpointer pt, gsize *size, guint *entries);

static inline void 
strncpy_pad(gchar dst[], const gchar src[], gsize len)
{
  gsize rlen = strlen(src);
  strncpy(dst, src, len);
  if(rlen < len)
    memset(dst+rlen, ' ', len-rlen);
}

void 
set_iso_evd(gpointer pd)
{
  struct iso_volume_descriptor ied;

  g_assert(sizeof(struct iso_volume_descriptor) == ISO_BLOCKSIZE);

  g_return_if_fail(pd != NULL);
  
  memset(&ied, 0, sizeof(ied));

  ied.type = to_711(ISO_VD_END);
  strncpy_pad(ied.id, ISO_STANDARD_ID, sizeof(ied.id));
  ied.version = to_711(ISO_VERSION);

  memcpy(pd, &ied, sizeof(ied));
}
				       
void
set_iso_pvd(gpointer pd,
	    const gchar volume_id[],
	    guint32 iso_size,
	    gconstpointer root_dir,
	    guint32 path_table_l_extent,
	    guint32 path_table_m_extent,
	    guint32 path_table_size)
{
  struct iso_primary_descriptor ipd;

  g_assert(sizeof(struct iso_primary_descriptor) == ISO_BLOCKSIZE);

  g_return_if_fail(pd != NULL);
  g_return_if_fail(volume_id != NULL);

  memset(&ipd,0,sizeof(ipd)); /* paranoia? */

  /* magic stuff ... thatis CD XA marker... */
  strcpy(((void*)&ipd)+ISO_XA_MARKER_OFFSET, ISO_XA_MARKER_STRING);

  ipd.type = to_711(ISO_VD_PRIMARY);
  strncpy_pad(ipd.id, ISO_STANDARD_ID, 5);
  ipd.version = to_711(ISO_VERSION);

  strncpy_pad(ipd.system_id, SYSTEM_ID, 32);
  strncpy_pad(ipd.volume_id, volume_id, 32);

  ipd.volume_space_size = to_733(iso_size);

  ipd.volume_set_size = to_723(1);
  ipd.volume_sequence_number = to_723(1);
  ipd.logical_block_size = to_723(ISO_BLOCKSIZE);

  ipd.path_table_size = to_733(path_table_size);
  ipd.type_l_path_table = to_731(path_table_l_extent); 
  ipd.type_m_path_table = to_732(path_table_m_extent); 
  
  g_assert(sizeof(ipd.root_directory_record) == 34);
  memcpy(ipd.root_directory_record, root_dir, sizeof(ipd.root_directory_record));
  ipd.root_directory_record[0] = 34;

  strncpy_pad(ipd.volume_set_id, VOLUME_SET_ID, 128);
  strncpy_pad(ipd.publisher_id, PUBLISHER_ID, 128);
  strncpy_pad(ipd.preparer_id, PREPARER_ID, 128);
  strncpy_pad(ipd.application_id, APPLICATION_ID, 128); 

  strncpy_pad(ipd.copyright_file_id    , "", 37);
  strncpy_pad(ipd.abstract_file_id     , "", 37);
  strncpy_pad(ipd.bibliographic_file_id, "", 37);

  {
    char iso_time[17];

    snprintf(iso_time, sizeof(iso_time), 
	     "%4.4d%2.2d%2.2d%2.2d%2.2d%2.2d00",
	     1978, 7, 14,  0, 0, 0);
    iso_time[16] = 0;

    memcpy(ipd.creation_date,    iso_time,17);
    memcpy(ipd.modification_date,iso_time,17);
    memcpy(ipd.expiration_date,  "0000000000000000",17);
    memcpy(ipd.effective_date,   "0000000000000000",17);
  }

  ipd.file_structure_version = to_711(1);

  /* we leave ipd.application_data = 0 */

  

  memcpy(pd, &ipd, sizeof(ipd)); /* copy stuff to arg ptr */
}

void
dir_add_entry(gpointer dir,
	      const gchar name[], 
	      guint32 extent,
	      guint32 size,
	      guint8 flags)
{
  dir_add_entry_su(dir, name, extent, size, flags, NULL, 0);
}

guint
dir_calc_record_size(guint namelen, 
		     guint su_len)
{
  guint length;

  length = sizeof(struct iso_directory_record);
  length += namelen;
  if(length % 2) /* pad to word boundary */
    length++;
  length += su_len;
  if(length % 2) /* pad to word boundary again */
    length++;

  return length;
}

void
dir_add_entry_su(gpointer dir,
		 const gchar name[], 
		 guint32 extent,
		 guint32 size,
		 guint8 flags,
		 gconstpointer su_data,
		 guint su_size)
{
  struct iso_directory_record *idr = dir;
  guint8 *dir8 = dir, *tmp8 = dir;
  gsize offset = 0;
  guint32 dsize = from_733(idr->size);

  gint length, su_offset;

  g_assert(sizeof(struct iso_directory_record) == 33);

  if(!dsize && !idr->length)
    dsize = ISO_BLOCKSIZE; /* for when dir lacks '.' entry */
  
  g_return_if_fail(dsize > 0 && !(dsize % ISO_BLOCKSIZE));
  g_return_if_fail(dir != NULL);
  g_return_if_fail(extent > 17);
  g_return_if_fail(name != NULL);
  g_return_if_fail(strlen(name) <= MAX_ISONAME);

  length = sizeof(struct iso_directory_record);
  length += strlen(name);
  if(length % 2) /* pad to word boundary */
    length++;
  su_offset = length;
  length += su_size;
  if(length % 2) /* pad to word boundary again */
    length++;

  /* fixme -- the following looks quite ugly, although it seems to work */

  while(offset < dsize) {
    tmp8 = dir8 + offset;
      
    if(!*tmp8) {
      if(offset)
	offset-=ISO_BLOCKSIZE;
      break;
    }

    offset+=ISO_BLOCKSIZE;

    if(offset == dsize) {
      offset-=ISO_BLOCKSIZE;
      break;
    }
  }
  
  tmp8 = dir8 + offset;

  while(*tmp8) {
    offset += *tmp8;
    tmp8 = dir8 + offset;
  }

  if( ((offset+length) % ISO_BLOCKSIZE) < (offset % ISO_BLOCKSIZE) )
    offset += ISO_BLOCKSIZE-(offset % ISO_BLOCKSIZE);

  tmp8 = dir8 + offset;

  /* g_printerr("using offset %d (for len %d of '%s')\n", offset, length, name); */

  g_assert(offset+length < dsize);

  idr = (struct iso_directory_record*)tmp8;
  
  g_return_if_fail(offset+length < dsize); 
  
  memset(idr, 0, length);
  idr->length = to_711(length);
  idr->extent = to_733(extent);
  idr->size = to_733(size);
  
  idr->date[0] = 1978-1900;
  idr->date[1] = 7;
  idr->date[2] = 14;
  /* ... [6] */

  idr->flags = to_711(flags);

  idr->volume_sequence_number = to_723(1);

  idr->name_len = to_711(strlen(name) ? strlen(name) : 1); /* working hack! */

  memcpy(idr->name, name, from_711(idr->name_len));
  memcpy(tmp8+su_offset, su_data, su_size);
}

void
dir_init_new(gpointer dir,
	     guint32 self,
	     guint32 ssize,
	     guint32 parent,
	     guint32 psize)
{
  dir_init_new_su(dir, self, ssize, NULL, 0, parent, psize, NULL, 0);
}

void dir_init_new_su(gpointer dir,
		     guint32 self,
		     guint32 ssize,
		     gconstpointer ssu_data,
		     guint ssu_size,
		     guint32 parent,
		     guint32 psize,
		     gconstpointer psu_data,
		     guint psu_size)
{
  g_return_if_fail(ssize > 0 && !(ssize % ISO_BLOCKSIZE));
  g_return_if_fail(psize > 0 && !(psize % ISO_BLOCKSIZE));
  g_return_if_fail(dir != NULL);

  memset(dir, 0, ssize);

  /* "\0" -- working hack due to padding  */
  dir_add_entry_su(dir, "\0", self, ssize, ISO_DIRECTORY, ssu_data, ssu_size); 

  dir_add_entry_su(dir, "\1", parent, psize, ISO_DIRECTORY, psu_data, psu_size);
}

void 
pathtable_init(gpointer pt)
{
  g_assert(sizeof(struct iso_path_table) == 8);

  g_return_if_fail(pt != NULL);
  
  memset(pt, 0, ISO_BLOCKSIZE); /* fixme */
}

void
pathtable_get_size_and_entries(gconstpointer pt, 
			       gsize *size,
			       guint *entries)
{
  const guint8 *tmp = pt;
  gsize offset = 0;
  guint count = 0;

  g_return_if_fail(pt != NULL);

  while(from_711(*tmp)) {
    offset += sizeof(struct iso_path_table);
    offset += from_711(*tmp);
    if(offset % 2)
      offset++;
    tmp = pt + offset;
    count++;
  }

  if(size)
    *size = offset;

  if(entries)
    *entries = count;
}

gsize
pathtable_get_size(gconstpointer pt)
{
  gsize size = 0;
  pathtable_get_size_and_entries(pt, &size, NULL);
  return size;
}

guint16 
pathtable_l_add_entry(gpointer pt, 
		      const gchar name[], 
		      guint32 extent, 
		      guint16 parent)
{
  struct iso_path_table *ipt = pt + pathtable_get_size(pt);
  gsize name_len = strlen(name) ? strlen(name) : 1;
  guint entrynum = 0;

  g_assert(pathtable_get_size(pt) < ISO_BLOCKSIZE); /* fixme */

  memset(ipt, 0, sizeof(struct iso_path_table)+name_len); /* paranoia */

  ipt->name_len = to_711(name_len);
  ipt->extent = to_731(extent);
  ipt->parent = to_721(parent);
  memcpy(ipt->name, name, name_len);

  pathtable_get_size_and_entries(pt, NULL, &entrynum);
  return entrynum;
}

guint16 
pathtable_m_add_entry(gpointer pt, 
		      const gchar name[], 
		      guint32 extent, 
		      guint16 parent)
{
  struct iso_path_table *ipt = pt + pathtable_get_size(pt);
  gsize name_len = strlen(name) ? strlen(name) : 1;
  guint entrynum = 0;

  g_assert(pathtable_get_size(pt) < ISO_BLOCKSIZE); /* fixme */

  memset(ipt, 0, sizeof(struct iso_path_table)+name_len); /* paranoia */

  ipt->name_len = to_711(name_len);
  ipt->extent = to_732(extent);
  ipt->parent = to_722(parent);
  memcpy(ipt->name, name, name_len);

  pathtable_get_size_and_entries(pt, NULL, &entrynum);
  return entrynum;
}

