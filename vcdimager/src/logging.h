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

#ifndef _LOGGING_H_
#define _LOGGING_H_

#include <glib.h>

#define VCD_LOG_DOMAIN "VCDImager"

#define VCD_LOG_LEVEL_VERBOSE (1 << (G_LOG_LEVEL_USER_SHIFT+0))
#define VCD_LOG_LEVEL_ERROR   (1 << (G_LOG_LEVEL_USER_SHIFT+1))
#define VCD_LOG_LEVEL_WARNING (1 << (G_LOG_LEVEL_USER_SHIFT+2))
#define VCD_LOG_LEVEL_INFO    (1 << (G_LOG_LEVEL_USER_SHIFT+3))

#define vcd_verbose(format, args...)     g_log (VCD_LOG_DOMAIN, \
                                               VCD_LOG_LEVEL_VERBOSE, \
                                               format, ##args)

#define vcd_error(format, args...)       g_log (VCD_LOG_DOMAIN, \
                                               VCD_LOG_LEVEL_ERROR, \
                                               format, ##args)

#define vcd_warning(format, args...)     g_log (VCD_LOG_DOMAIN, \
                                               VCD_LOG_LEVEL_WARNING, \
                                               format, ##args)

#define vcd_info(format, args...)        g_log (VCD_LOG_DOMAIN, \
                                               VCD_LOG_LEVEL_INFO, \
                                               format, ##args)


void logging_set_verbose(gboolean flag);

void logging_init(void);

void logging_done(void);

#endif _LOGGING_H_
