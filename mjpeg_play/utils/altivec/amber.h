/* amber.h, this file is part of the
 * AltiVec optimized library for MJPEG tools MPEG-1/2 Video Encoder
 * Copyright (C) 2002  James Klicman <james@klicman.org>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef AMBER_ENABLE

#ifndef AMBER_MAX_TRACES
#define AMBER_MAX_TRACES 1
#endif

#ifndef AMBER_MAX_EXIT
#define AMBER_MAX_EXIT 1
#endif

long amber_start(const char *file, int line,
                 int *trace_count, int max_traces);
long amber_stop(const char *file, int line,
                int *trace_count, int max_traces, int max_exit);

static int amber_trace_count = 0;

#define AMBER_START amber_start(__FILE__, __LINE__, &amber_trace_count, \
                                AMBER_MAX_TRACES)
#define AMBER_STOP  amber_stop(__FILE__, __LINE__, &amber_trace_count, \
                               AMBER_MAX_TRACES, AMBER_MAX_EXIT)
#else

#define AMBER_START
#define AMBER_STOP

#endif
