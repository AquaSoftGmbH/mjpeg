
/*
 *  interact.hh:  Simple command-line front-end
 *
 *  Copyright (C) 2001 Andrew Stevens <andrew.stevens@philips.com>
 *
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU General Public License
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef __INTERACT_H__
#define __INTERACT_H__

#include <config.h>
#include <unistd.h>
#include <vector>
#include "mjpeg_types.h"

#include "aunit.hh"

/*************************************************************************
    Entry points...
*************************************************************************/

    
int intro_and_options( int, char **, char**);

void check_files (int argc, char* argv[],
                  vector<char *> &mpa_file,
                  vector<char *> &ac3_file,                  
                  vector<char *> &video_file
	);
bool open_file(const char *name);






/*************************************************************************
    Command line options and derived parameters
*************************************************************************/



extern int opt_quiet_mode;
extern int opt_interactive_mode;
extern int opt_buffer_size;
extern int opt_data_rate;
extern int opt_video_offset;
extern int opt_audio_offset;
extern int opt_sector_size;
extern int opt_VBR;
extern int opt_mpeg;
extern int opt_mux_format;
extern int opt_multifile_segment;
extern int opt_always_system_headers;
extern int opt_packets_per_pack;
extern clockticks opt_max_PTS;
extern int opt_emul_vcdmplex;
extern bool opt_stills;
extern bool opt_ignore_underrun;
extern int verbose;

extern off_t opt_max_segment_size;

/*************************************************************************
    Program ID
*************************************************************************/
 
#define MPLEX_VER    "2.0.2"
#define MPLEX_DATE   "$Date$"


#endif // __INTERACT_H__


/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
