
/*************************************************************************
*  mplex - MPEG/SYSTEMS multiplexer					 *
*  Copyright (C) 1994 1995 Christoph Moar				 *
*  Siemens ZFE ST SN 11 / T SN 6					 *
*									 *
*  moar@informatik.tu-muenchen.de 					 *
*       (Christoph Moar)			 			 *
*  klee@heaven.zfe.siemens.de						 *
*       (Christian Kleegrewe, Siemens only requests)			 *
* Modifications and enhancements (C) 2000, 2001 Andrew Stevens
* andrew.stevens@philips.com
*									 *
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


#include "main.hh"
#include "mjpeg_logging.h"

/*************************************************************************
    Main
*************************************************************************/


int main (int argc, char* argv[])
{
    char 	*audio_file = NULL;
    char        *video_file = NULL;
    char        *multi_file = NULL;	

	int     optargs;
    VideoStream videoStrm(0);
    AudioStream audioStrm(0);
    off_t audio_bytes, video_bytes;
    clockticks first_frame_PTS = 0;

    optargs = intro_and_options (argc, argv, &multi_file);
	// For the moment everything is such a mess that we can't really cleanly
	// handle stills. We'll get there in the end though...
	if(  opt_stills )
	{
		vector<const char *> stills;
		check_stills(argc-optargs, argv+optargs, stills);
		init_stream_syntax_parameters();
		VideoStream stillStrm(0);
		stillStrm.Init( stills[0] );
		mjpeg_error_exit1( "Still stream muxing not (yet) implemented\n");
		//outputstream( stillStrm, audioStrm, multi_file );
	}
	else
	{
		check_files (argc-optargs, argv+optargs,  &audio_file, &video_file);
		init_stream_syntax_parameters();

		if (video_file) {
			videoStrm.Init( video_file);
			videoStrm.SetMuxParams( video_buffer_size );
		}
    
		if (audio_file) {
			audioStrm.Init (audio_file);
			audioStrm.SetMuxParams( audio_buffer_size );
		}

    outputstream (videoStrm,  audioStrm, audio_file,  multi_file );
	videoStrm.Close();
	audioStrm.Close();
	}
    return (0);	
}

			
