
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
#include "format_codes.h"
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
	OutputStream ostrm;

    off_t audio_bytes, video_bytes;
    clockticks first_frame_PTS = 0;

    optargs = intro_and_options (argc, argv, &multi_file);
	// 
	// TODO: Just crude single-stream stills for the momement
	// TODO: Clean up interface to muxing routine...
	//
	if(  MPEG_STILLS_FORMAT(opt_mux_format) )
	{
		mjpeg_debug( "Multiplexing stills stream!\n" );
		check_files (argc-optargs, argv+optargs,  &audio_file, &video_file);
		ostrm.InitSyntaxParameters();
		int stream_num;
		double frame_interval;
		switch( opt_mux_format )
		{
		case MPEG_FORMAT_VCD_STILL :
			stream_num = 1;
			frame_interval = 10; // 10 Frame periods
			break;
		case MPEG_FORMAT_SVCD_STILL :
			stream_num = 1;
			frame_interval = 10;
			break;
		default:
			mjpeg_error_exit1("Only VCD and SVCD stills format for the moment...\n");
		}
		ConstantFrameIntervals intervals( frame_interval );
		StillsStream stillStrm(ostrm,stream_num,  
							   &intervals);
		stillStrm.Init( video_file );
		ostrm.OutputMultiplex( &stillStrm , 
							   NULL, 
							   multi_file);
		
	}
	else
	{
		check_files (argc-optargs, argv+optargs,  &audio_file, &video_file);
		ostrm.InitSyntaxParameters();
        VideoStream videoStrm(ostrm,0);
        AudioStream audioStrm(ostrm,0);

		if (video_file) {
			videoStrm.Init( video_file);
		}
    
		if (audio_file) {
			audioStrm.Init (audio_file);
		}

		ostrm.OutputMultiplex( video_file ? &videoStrm : NULL, 
							   audio_file ? &audioStrm : NULL, 
							   multi_file);
	}
    return (0);	
}

			
