/* libavifile - common includes and defs for lavtools using avifile */
/* Copyright (C) 2002 Shawn Sulma */
/*
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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/**
 * Aside from co-ordinating the import of the appropriate header files,
 * this file does some correction of version numbers 
 *
 * 2002-03-03 initial version.
 *
 */
#ifndef LIBAVIFILE_H
#define LIBAVIFILE_H

#include <avifile.h>
#include <aviplay.h>
#include <avifile/version.h>
#include <videoencoder.h>

// Correct the version numbers
#if AVIFILE_MAJOR_VERSION == 0
#if AVIFILE_MINOR_VERSION == 6
#undef AVIFILE_MINOR_VERSION
#define AVIFILE_MINOR_VERSION 60
#endif /* AVIFILE_MINOR_VERSION == 6 */
#endif /* AVIFILE_MAJOR_VERSION == 0 */

// Import version-dependent headers
#if AVIFILE_MAJOR_VERSION == 0 && AVIFILE_MINOR_VERSION < 60
#include <aviutil.h>
typedef unsigned int framepos_t;
#else
#include <image.h>
#include <fourcc.h>
#include <creators.h>
#endif /* AVIFILE_MAJOR_VERSION, AVIFILE_MINOR_VERSION */

#include "riffinfo.h"

#ifndef min
#define min(a,b) ((a)<(b))?(a):(b)
#endif

#ifndef max
#define max(a,b) ((a)>(b))?(a):(b)
#endif

#endif LIBAVIFILE_H
