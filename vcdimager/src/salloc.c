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

#include "salloc.h"
#include "logging.h"

static GByteArray* _bitmap = NULL;

static gboolean
sector_is_set(guint32 sector)
{
  guint _byte = sector / 8;
  guint _bit  = sector % 8;

  if(_byte < _bitmap->len)
    return _bitmap->data[_byte] & (1 << _bit);
  else
    return FALSE;
}

static void
sector_set(guint32 sector)
{
  guint _byte = sector / 8;
  guint _bit  = sector % 8;

  if(_byte >= _bitmap->len) {
    guint oldlen = _bitmap->len;
    g_byte_array_set_size(_bitmap, _byte+1);
    memset(_bitmap->data+oldlen, 0x00, _byte+1-oldlen);
  }

  _bitmap->data[_byte] |= (1 << _bit);
}

/* exported */

guint32
sector_alloc(guint32 hint, guint32 size)
{
  if(!size) {
    size++;
    vcd_warning("request of 0 sectors allocment fixed up to 1 sector (this is harmless)");
  }

  g_return_val_if_fail(size > 0, SECTOR_NIL);

  if(hint != SECTOR_NIL) {
    guint32 i;
    for(i = 0;i < size;i++) 
      if(sector_is_set(hint+i))
	return SECTOR_NIL;
    
    /* everything's ok for allocing */

    i = size;
    while(i)
      sector_set(hint+(--i));
    /* we begin with highest byte, in order to minimizing
       realloc's in sector_set */

    return hint;
  }

  /* find the lowest possible ... */

  hint = 0;

  while(sector_alloc(hint, size) == SECTOR_NIL)
    hint++;

  return hint;
}

void 
salloc_init(void)
{
  g_return_if_fail(_bitmap == NULL);

  _bitmap = g_byte_array_new();
}

void
salloc_done(void)
{
  g_return_if_fail(_bitmap != NULL);

  g_byte_array_free(_bitmap, TRUE);
  _bitmap = NULL;
}

guint32
salloc_get_highest(void)
{
  guint8 last = _bitmap->data[_bitmap->len-1];
  guint n;

  g_return_val_if_fail(_bitmap != NULL, 0);

  g_assert(last != 0);

  n=8;
  while(n)
    if((1 << --n) & last)
      break;

  return (_bitmap->len-1)*8 + n+1;
}
