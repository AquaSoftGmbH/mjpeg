
/*************************************************************************
*  mplex - MPEG/SYSTEMS multiplexer
*  Copyright (C) 1994 1995 Christoph Moar
*  Siemens ZFE ST SN 11 / T SN 6
*  moar@informatik.tu-muenchen.de 
*       (Christoph Moar)
*
*  Copyright (C) 2000, 2001, Andrew Stevens <andrew.stevens@philips.com>
*
*  This program is free software; you can redistribute it and/or modify	 *
*  it under the terms of the GNU General Public License as published by	 *
*  the Free Software Foundation; either version 2 of the License, or	 *
*  (at your option) any later version.					 *
*									 *
*  This program is distributed in the hope that it will be useful,	 *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of	 *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the	 *
*  GNU General Public License for more details.				 *
*									 *
*  You should have received a copy of the GNU General Public License	 *
*  along with this program; if not, write to the Free Software		 *
*  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.		 *
*************************************************************************/

#include <config.h>
#include <stdio.h>
#include <inttypes.h>
#include "bits.hh"
#include "inputstrm.hh"
#include "outputstream.hh"
#include "mjpeg_logging.h"


/*************************************************************************
    Definitionen
*************************************************************************/
 
#define MPLEX_VER    "1.5.1"
#define MPLEX_DATE   "$Date$"



	/* 
	   A "safety margin" for the audio buffer just in case the
	   player doesn't *quite* clear it as quickly as we guess.
	   We don't send a new audio packet until there ought to be at least this
	   much space.
	*/
#define AUDIO_BUFFER_FILL_MARGIN 300

/* The following values for sys_header_length & size are only valid for */
/* System streams consisting of two basic streams. When wrapping around */
/* the system layer on a single video or a single audio stream, those   */
/* values get decreased by 3.                                           */



#define VIDEOCD_SECTOR_SIZE	2324	        /* VideoCD sector size */
#define SVCD_SECTOR_SIZE        2324            /* Super VideoCD sector size */
#define DVD_SECTOR_SIZE         2048            /* DVD sector size     */

#ifdef REDUNDANT
#define STREAMS_VIDEO           1
#define STREAMS_AUDIO           2
#define STREAMS_BOTH            3
#endif


#define STATUS_AUDIO_TIME_OUT	2		/* Statusmessage A out	*/
#define STATUS_VIDEO_TIME_OUT	3		/* Statusmessage V out	*/


    
    
/*************************************************************************
    Funktionsprototypen, keine Argumente, K&R Style
*************************************************************************/

int intro_and_options( int, char **, char**);


/* Initialisation of syntax paramters 	*/
/* based on (checked) options 			*/

void init_stills_syntax_parameters();
void init_stream_syntax_parameters();	

void check_stills( const int argc, char *argv[], 
				   vector<const char *>stills );

void check_files (int argc,
				  char* argv[],
				  char**audio_file,
				  char**video_file
	);
bool open_file(const char *name);

// TODO Get rid of the ugly use of access unit structs with 0 length
// fields a means of signalling "Null AU/beyond end of AU's".


void offset_timecode      (clockticks *time1,clockticks *time2,clockticks *offset);	/* Rechnet Offset zwischen zwei TimeC.	*/
void copy_timecode        (clockticks *,clockticks *);	/* setzt 2tes TimeC. dem 1ten gleich	*/
void bytepos_timecode(long long bytepos, clockticks *ts);
void make_timecode        (double, clockticks *);	/* rechnet aus double einen TimeC.	*/
				/* und schreibt ihn in clockticks   */
void add_to_timecode      (clockticks *,clockticks *);	/* addiert 1tes TimeC. zum 2ten		*/ 
void buffer_dtspts_mpeg1scr_timecode (clockticks timecode,
									 unsigned char  marker,
									 unsigned char **buffer);
void buffer_mpeg2scr_timecode( clockticks timecode,
								unsigned char **buffer
							 );

int  comp_timecode        (clockticks *,clockticks *);	/* 1tes TimeC. <= 2tes TimeC. ?		*/

void status_info (	unsigned int nsectors_a,
					unsigned int nsectors_v,
					unsigned int nsectors_p,
					unsigned long long nbytes,
					unsigned int buf_v,
					unsigned int buf_a,
					log_level_t level
				 );	/* Status line update	*/

void status_header (log_level_t level);
void status_footer (log_level_t level);
void timeout_error	  (int what, int decode_number);





/*************************************************************************
    Statische Arrays
*************************************************************************/

extern unsigned int bitrate_index [4][3][16];

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
extern int verbose;

extern unsigned int audio_buffer_size;
extern unsigned int video_buffer_size;


extern off_t opt_max_segment_size;
