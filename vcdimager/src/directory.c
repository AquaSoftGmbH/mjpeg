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

#include "directory.h"
#include "iso9660.h"

/* CD-ROM XA */

#define XA_FORM1_DIR    0x8d
#define XA_FORM1_FILE   0x0d
#define XA_FORM2_FILE   0x15

typedef struct {
  guint8 unknown1[4]; /* zero */
  guint8 type;        /* XA_... */
  guint8 magic[3];    /* { 'U', 'X', 'A' } */
  guint8 filenum;     /* filenum */
  guint8 unknown2[5]; /* zero */
} xa_t;

/* tree data structure */

static GNode *_root = NULL;

typedef struct {
  gboolean is_dir;
  gchar *name;
  guint8 xa_type;
  guint8 xa_filenum;
  guint32 extent;
  guint32 size;
  guint pt_id;
} data_t;

#define EXTENT(anode) ((data_t*)((anode)->data))->extent
#define SIZE(anode) ((data_t*)((anode)->data))->size
#define PT_ID(anode) ((data_t*)((anode)->data))->pt_id

/* implementation */

static gboolean 
traverse_get_dirsizes(GNode *node, gpointer data)
{
  data_t *d = node->data;
  guint *sum = data;

  if(d->is_dir)
    *sum += (d->size / ISO_BLOCKSIZE);

  return FALSE;
}

static guint
get_dirsizes(GNode* dirnode)
{
  guint result = 0;

  g_node_traverse(dirnode, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
		  traverse_get_dirsizes, &result);

  return result;
}

static gboolean 
traverse_update_dirextents(GNode *dirnode, gpointer data)
{
  data_t *d = dirnode->data;

  if(d->is_dir) {
    GNode* child = g_node_first_child(dirnode);
    guint dirextent = d->extent;

    dirextent += d->size / ISO_BLOCKSIZE;

    while(child) {
      data_t *d = child->data;


      g_assert(d != NULL);

      if(d->is_dir) {
	d->extent = dirextent;
	dirextent += get_dirsizes(child);
      }

      child = g_node_next_sibling(child);
    }
    
  }

  return FALSE;
}

static void 
update_dirextents(guint32 extent)
{
  EXTENT(_root) = extent;

  g_node_traverse(_root, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
		  traverse_update_dirextents, NULL);
}

static gboolean 
traverse_update_sizes(GNode *node, gpointer data)
{
  data_t *dirdata = node->data;

  if(dirdata->is_dir) {
    GNode* child = g_node_first_child(node);
    guint blocks = 1;
    guint offset = 0;

    offset += dir_calc_record_size(1, sizeof(xa_t)); /* '.' */
    offset += dir_calc_record_size(1, sizeof(xa_t)); /* '..' */

    while(child) {
      data_t *d = child->data;
      guint reclen;

      g_assert(d != NULL);

      reclen = dir_calc_record_size(strlen(d->name), sizeof(xa_t));
    
      if(offset + reclen > ISO_BLOCKSIZE) {
	blocks++;
	offset = reclen;
      } else
	offset += reclen;

      child = g_node_next_sibling(child);
    }
    dirdata->size = blocks*ISO_BLOCKSIZE;
  }

  return FALSE;
}

static void 
update_sizes(void)
{
  g_node_traverse(_root, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
		  traverse_update_sizes, NULL);
}

static gsize
g_strlenv(gchar **str_array)
{
  gsize n = 0;
  while(str_array[n])
    n++;

  return n;
}

/* exported stuff: */

void
directory_init(void)
{
  data_t *data;

  g_return_if_fail(_root == NULL);

  g_assert(sizeof(xa_t) == 14);

  _root = g_node_new(data = g_new0(data_t, 1));

  data->is_dir = TRUE;
  data->name = g_memdup("\0", 2);
  data->xa_type = XA_FORM1_DIR;
  data->xa_filenum = 0x00;
}

static gboolean 
traverse_directory_done(GNode *node, gpointer data)
{
  data_t *nodedata = node->data;
  
  g_free(nodedata->name);

  return FALSE;
}

void
directory_done(void)
{
  g_return_if_fail(_root != NULL);

  g_node_traverse(_root, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
		  traverse_directory_done, NULL);

  g_node_destroy(_root);
}

static GNode* 
lookup_child(GNode* node, const gchar name[])
{
  GNode* child = g_node_first_child(node);

  while(child) {
    data_t *d = child->data;

    if(!strcmp(d->name, name))
      return child;

    child = g_node_next_sibling(child);
  }

  return child; /* NULL */
}

void
directory_mkdir(const gchar pathname[])
{
  gchar **splitpath;
  guint level, n;
  GNode* pdir = _root;

  g_return_if_fail(pathname != NULL);
  g_assert(_root != NULL);

  splitpath = g_strsplit(pathname, "/", 0);

  level = g_strlenv(splitpath);

  for(n = 0; n < level-1; n++) 
    if(!(pdir = lookup_child(pdir, splitpath[n]))) {
      g_printerr("parent dir %s (%d) missing!\n", splitpath[n], n);
      g_assert_not_reached();
    }

  if(lookup_child(pdir, splitpath[level-1])) {
    g_printerr("directory_mkdir: `%s' already exists\n", pathname);
    g_assert_not_reached();
  }

  {
    data_t *data = g_new0(data_t, 1);

    g_node_append_data(pdir, data);

    data->is_dir = TRUE;
    data->name = g_strdup(splitpath[level-1]);
    data->xa_type = XA_FORM1_DIR;
    data->xa_filenum = 0x00;
    /* .. */
  }

  g_strfreev(splitpath);
}

void
directory_mkfile(const gchar pathname[], guint32 start, guint32 size,
		 gboolean form2, guint8 filenum)
{
  gchar **splitpath;
  guint level, n;
  GNode* pdir = _root;

  g_return_if_fail(pathname != NULL);
  g_assert(_root != NULL);

  splitpath = g_strsplit(pathname, "/", 0);

  level = g_strlenv(splitpath);

  for(n = 0; n < level-1; n++) 
    if(!(pdir = lookup_child(pdir, splitpath[n]))) {
      g_printerr("parent dir %s (%d) missing!\n", splitpath[n], n);
      g_assert_not_reached();
    }

  if(lookup_child(pdir, splitpath[level-1])) {
    g_printerr("directory_mkfile: `%s' already exists\n", pathname);
    g_assert_not_reached();
  }

  {
    data_t *data = g_new0(data_t, 1);

    g_node_append_data(pdir, data);

    data->is_dir = FALSE;
    data->name = g_strdup(splitpath[level-1]);
    data->xa_type = form2 ? XA_FORM2_FILE : XA_FORM1_FILE;
    data->xa_filenum = filenum;
    data->size = size;
    data->extent = start;
    /* .. */
  }

  g_strfreev(splitpath);
}

guint32
directory_get_size(void)
{
  g_assert(_root != NULL);

  update_sizes();
  return get_dirsizes(_root);
}

static gboolean 
traverse_directory_dump_entries(GNode *node, gpointer data)
{
  data_t *d = node->data;
  xa_t tmp = { { 0, }, 0, { 'U', 'X', 'A' }, 0, { 0, } };
  
  guint32 root_extent = EXTENT(g_node_get_root(node));
  guint32 parent_extent = 
    G_NODE_IS_ROOT(node) 
    ? EXTENT(node)
    : EXTENT(node->parent);
  guint32 parent_size = 
    G_NODE_IS_ROOT(node) 
    ? SIZE(node)
    : SIZE(node->parent);

  gpointer dirbufp = data+ISO_BLOCKSIZE*(parent_extent - root_extent);


  tmp.type = d->xa_type;
  tmp.filenum = d->xa_filenum;

  if(!G_NODE_IS_ROOT(node))
    dir_add_entry_su(dirbufp, d->name, d->extent, d->size, 
		     d->is_dir ? ISO_DIRECTORY : ISO_FILE,
		     &tmp, sizeof(tmp));
  
  if(d->is_dir) {
    gpointer dirbuf = data+ISO_BLOCKSIZE*(d->extent - root_extent);
    xa_t tmp2 = { { 0, }, XA_FORM1_DIR, { 'U', 'X', 'A' }, 0, { 0, } };
  
    dir_init_new_su(dirbuf, 
		    d->extent, d->size, &tmp2, sizeof(tmp2),
		    parent_extent, parent_size, &tmp2, sizeof(tmp2));
  }

  return FALSE;
}

void
directory_dump_entries(gpointer buf, guint32 extent)
{
  g_assert(_root != NULL);

  update_sizes(); /* better call it one time more than one less */
  update_dirextents(extent);

  g_node_traverse(_root, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
		  traverse_directory_dump_entries, buf); 
}

typedef struct {
  gpointer ptl;
  gpointer ptm;
} directory_dump_pathtables_t;

static gboolean 
traverse_directory_dump_pathtables(GNode *node, gpointer data)
{
  data_t *d = node->data;
  directory_dump_pathtables_t *args = data;

  if(d->is_dir) {
    guint parent_id = G_NODE_IS_ROOT(node) ? 1 : PT_ID(node->parent);

    pathtable_l_add_entry(args->ptl, d->name, d->extent, parent_id);
    d->pt_id =
      pathtable_m_add_entry(args->ptm, d->name, d->extent, parent_id);
  }

  return FALSE;
}

void
directory_dump_pathtables(gpointer ptl, gpointer ptm)
{
  directory_dump_pathtables_t args;

  pathtable_init(ptl);
  pathtable_init(ptm);

  args.ptl = ptl;
  args.ptm = ptm;

  g_node_traverse(_root, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
		  traverse_directory_dump_pathtables, &args); 
}
