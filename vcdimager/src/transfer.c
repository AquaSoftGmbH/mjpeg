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
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "transfer.h"

#include "cd_sector.h"
#include "directory.h"
#include "logging.h"
#include "salloc.h"

glong 
get_regular_file_size(const gchar file_name[])
{
  struct stat buf;
  
  if(stat(file_name, &buf))
    vcd_error("stat(`%s'): %s", file_name, g_strerror(errno));

  if(!S_ISREG(buf.st_mode))
    return -1;

  return buf.st_size;
}

void
write_mode2_sector(FILE *fd, gconstpointer data, guint32 extent,
		   guint8 fnum, guint8 cnum, guint8 sm, guint8 ci)
{
  gchar buf[CDDA_SIZE] = { 0, };

  g_assert(fd != NULL);
  
  make_mode2(buf, data, extent, fnum, cnum, sm, ci);

  if(fseek(fd, extent*CDDA_SIZE, SEEK_SET))
    vcd_error("fseek(): %s", g_strerror(errno));

  fwrite(buf, CDDA_SIZE, 1, fd);
  
  if(ferror(fd)) 
    vcd_error("fwrite(): %s", g_strerror(errno));
}

void
write_raw_mode2_sector(FILE *fd, gconstpointer data, guint32 extent)
{
  gchar buf[CDDA_SIZE] = { 0, };

  g_assert(fd != NULL);
  
  make_raw_mode2(buf, data, extent);

  if(fseek(fd, extent*CDDA_SIZE, SEEK_SET)) 
    vcd_error("fseek(): %s", g_strerror(errno));

  fwrite(buf, CDDA_SIZE, 1, fd);

  if(ferror(fd)) 
    vcd_error("fwrite(): %s", g_strerror(errno));
}

void
insert_file_raw_mode2(FILE *image_fd, const gchar pathname[], 
		      const gchar iso_pathname[])
{
  FILE *fd = NULL;
  guint32 size = 0, extent = 0, sectors = 0;

  if(!(fd = fopen(pathname, "rb"))) 
    vcd_error("fopen(`%s'): %s", pathname, g_strerror(errno));
  
  size = get_regular_file_size(pathname);

  sectors = size / M2RAW_SIZE;

  if(size % M2RAW_SIZE) 
    vcd_error("raw mode2 file `%s' must have size multiple of %d \n",
	      pathname, M2RAW_SIZE);

  extent = sector_alloc(SECTOR_NIL, sectors);

  directory_mkfile(iso_pathname, extent, size, TRUE, 1);

  while(!feof(fd)) {
    gchar buf[M2RAW_SIZE] = { 0, };

    fread(buf, M2RAW_SIZE, 1, fd);
    write_raw_mode2_sector(image_fd, buf, extent++);
  }
      
  fclose(fd);
}

void 
insert_file_mode2_form1(FILE *image_fd, const gchar pathname[], 
			const gchar iso_pathname[])
{
  FILE *fd = NULL;
  guint32 size = 0, sectors = 0, extent = 0, start_extent = 0;

  if(!(fd = fopen(pathname, "rb"))) 
    vcd_error("fopen(`%s'): %s", pathname, g_strerror(errno));
  
  size = get_regular_file_size(pathname);

  sectors = size / M2F1_SIZE;
  if(size % M2F1_SIZE)
    sectors++;

  start_extent = sector_alloc(SECTOR_NIL, sectors);

  directory_mkfile(iso_pathname, start_extent, size, FALSE, 1);

  for(extent = 0;extent < sectors;extent++) {
    gchar buf[M2F1_SIZE] = { 0, };

    if(feof(fd)) 
      vcd_error("fread(): unexpected EOF encountered...");

    fread(buf, M2F1_SIZE, 1, fd);

    write_mode2_sector(image_fd, buf, start_extent+extent, 1, 0, 
		       (extent < sectors-1) ? SM_DATA : SM_DATA|SM_EOF,
		       0);
  }
      
  fclose(fd);
}

static void
__add_path_recursive(FILE* fd, const gchar path[], const gchar iso_path[])
{
  struct dirent *dent = NULL;
  DIR *dir = opendir(path);
  
  if(!dir)
    vcd_error("opendir(`%s'): %s", path, g_strerror(errno));

  while((dent = readdir(dir))) {
    const gchar *dirname = dent->d_name;
    
    if(strcmp(dirname, ".") && strcmp(dirname, "..")) {
      struct stat buf;
      gchar *iso_newpath;
      gchar *newpath;
      
      if(strlen(iso_path))
	iso_newpath = g_strconcat(iso_path, "/", dirname, NULL);
      else
	iso_newpath = g_strdup(dirname);
      g_strup(iso_newpath);
      
      newpath = g_strconcat(path, "/", dirname, NULL);
      
      if(stat(newpath, &buf)) 
	vcd_error("stat(`%s'): %s", newpath, g_strerror(errno));
      
      if(S_ISDIR(buf.st_mode)) {
	vcd_verbose("adding directory `%s' as `%s' to isofs", newpath, iso_newpath);
	directory_mkdir(iso_newpath);
	__add_path_recursive(fd, newpath, iso_newpath);
      }
      else if(S_ISREG(buf.st_mode)) {
	gchar *tmp = g_strdup_printf("%s;1", iso_newpath); /* fixme */
	vcd_verbose("adding file `%s' as `%s' to isofs", newpath, tmp);
	insert_file_mode2_form1(fd, newpath, tmp);
	g_free(tmp);
      } else 
	vcd_warning("file entry `%s' ignored (not a file or directory)", newpath);

      g_free(newpath);
      g_free(iso_newpath);
    }
  }
  
  closedir(dir);
}

void
insert_tree_mode2_form1(FILE *image_fd, const gchar pathname[])
{
  g_assert(pathname != NULL);

  vcd_warning("no filename iso9660 checks are performed yet when adding user files...!");

  __add_path_recursive(image_fd, pathname, "");
}
