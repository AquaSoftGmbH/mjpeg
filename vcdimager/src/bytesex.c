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

/* stuff */

#include <glib.h>
#include "bytesex.h"

#if (G_BYTE_ORDER != G_LITTLE_ENDIAN) && (G_BYTE_ORDER != G_BIG_ENDIAN)
#error this hosts byte order not supported yet
#endif

guint8
to_711(guint8 i)
{
  return i; /* yeah it's trivial ... */
}

guint16 
to_721(guint16 i)
{
  return GUINT16_TO_LE(i);
}

guint16
to_722(guint16 i)
{
  return GUINT16_TO_BE(i);
}

guint32
to_723(guint16 i)
{
  return GUINT32_SWAP_LE_BE(i) | i;
}

guint32
to_731(guint32 i)
{
  return GUINT32_TO_LE(i);
}

guint32
to_732(guint32 i)
{
  return GUINT32_TO_BE(i);
}

guint64
to_733(guint32 i)
{
  return GUINT64_SWAP_LE_BE(i) | i;
}


guint8
from_711(guint8  p)
{
  return p;
}


guint32
from_733(guint64 p)
{
  return (0xFFFFFFFF & p);
}


guint8
to_bcd8(guint8 n)
{
  g_return_val_if_fail(n < 100, 0);

  return ((n/10)<<4) | (n%10);
}
