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

#ifndef _BYTESEX_H_
#define _BYTESEX_H_

#include <glib.h>

guint8  to_711(guint8  i);

guint16 to_721(guint16 i);
guint16 to_722(guint16 i);
guint32 to_723(guint16 i);

guint32 to_731(guint32 i);
guint32 to_732(guint32 i);
guint64 to_733(guint32 i);

guint8  from_711(guint8  p);
guint32 from_733(guint64 p);

guint8  to_bcd8(guint8 n);

#endif /* _BYTESEX_H_ */
