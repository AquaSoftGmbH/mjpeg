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
#include "logging.h"
#include <stdlib.h>

static gboolean _verbose = FALSE;

static void
vcd_log_handler(const gchar *log_domain, GLogLevelFlags log_level,
		const gchar *message, gpointer unused_data)
{
  if(log_level & VCD_LOG_LEVEL_ERROR) {
    g_printerr("**ERROR: %s\n", message);
    exit(EXIT_FAILURE);
  } else if(log_level & VCD_LOG_LEVEL_VERBOSE) {
    if(_verbose) 
      g_print("--DEBUG: %s\n", message);
  } else if(log_level & VCD_LOG_LEVEL_WARNING)
    g_printerr("++ WARN: %s\n", message);
  else if(log_level & VCD_LOG_LEVEL_INFO)
    g_print(   "   INFO: %s\n", message);
  else
    g_assert_not_reached();
}

void
logging_set_verbose(gboolean flag)
{
  _verbose = flag;
}

void
logging_init(void)
{
  g_log_set_handler(VCD_LOG_DOMAIN,
		    VCD_LOG_LEVEL_VERBOSE 
		    | VCD_LOG_LEVEL_ERROR 
		    | VCD_LOG_LEVEL_WARNING 
		    | VCD_LOG_LEVEL_INFO,
		    vcd_log_handler, 
		    NULL);
}

void
logging_done(void)
{
  /* noop */
}
