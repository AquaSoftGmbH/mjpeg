/* 
 *  idct_248.h
 *
 *     Copyright (C) Charles 'Buck' Krasic - May 2000
 *     Copyright (C) Erik Walthinsen - May 2000
 *
 *  This file is part of libdv, a free DV (IEC 61834/SMPTE 314M)
 *  decoder.
 *
 *  libdv is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your
 *  option) any later version.
 *   
 *  libdv is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *   
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 
 *
 *  The libdv homepage is http://libdv.sourceforge.net/.  
 */

#ifndef _IDCT_248_H
#define _IDCT_248_H

#include "dv.h"

extern void dv_dct_248_init(void);
extern void dv_idct_248(dv_248_coeff_t *x248);

#endif // _IDCT_248_H
