/* -*- mode:C; tab-width:8; -*- */
/*
 *  Copyright (C) 2001 Kawamata/Hitoshi <hitoshi.kawamata@nifty.ne.jp>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __YUVFILTER_STATIC_H__
#define __YUVFILTER_STATIC_H__

#include <mjpegtools/yuvfilter.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef USE_DLFCN_H

typedef struct {
  const char *name;
  const yuvfilter_t *yuvfilter;
} yuvfilter_desc_t;

extern const yuvfilter_desc_t yuvfilter_descs[];

#endif

#ifdef __cplusplus
}
#endif

#endif
