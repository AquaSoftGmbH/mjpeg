
/*************************************************************************
*  mplex - MPEG/SYSTEMS multiplexer
*  Copyright (C) 1994 1995 Christoph Moar
*  Siemens ZFE ST SN 11 / T SN 6
*
* Modifications and enhancements (C) 2000, 2001 Andrew Stevens
* <andrew.stevens@philips.com>
*									 *
*  This program is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 2 of the License, or
*  (at your option) any later version.					 *
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.	
*
*  You should have received a copy of the GNU General Public License
*  along with this program; if not, write to the Free Software	
*  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*************************************************************************/

#include <config.h>
#include <stdio.h>

#include "mjpeg_types.h"
#include "mjpeg_logging.h"
#include "mpegconsts.h"

#include "interact.hh"
#include "bits.hh"
#include "inputstrm.hh"
#include "outputstream.hh"
#include "stillsstream.hh"

/*************************************************************************
    Main
*************************************************************************/


int main (int argc, char* argv[])
{
    vector<char *> audio_files;
    vector<char *> video_files;
    char    *multi_file = NULL;	
	int     i;
	int     optargs;
	OutputStream ostrm;
	vector<ElementaryStream *> strms;
    off_t audio_bytes, video_bytes;
    clockticks first_frame_PTS = 0;

    optargs = intro_and_options (argc, argv, &multi_file);
	check_files (argc-optargs, argv+optargs,  audio_files, video_files);

	if( video_files.size() != 1 )
	{
		mjpeg_debug( "Multiplexing currently only tested with exactly one (1) video stream\n");
		mjpeg_debug( "If you want to try it you'll need to modify the source code!\n" );
		exit(1);
	}

	if( MPEG_STILLS_FORMAT(opt_mux_format) )
	{
		mjpeg_info( "Multiplexing stills stream!\n" );
		ostrm.InitSyntaxParameters();
		unsigned int frame_interval;

		switch( opt_mux_format )
		{

		case MPEG_FORMAT_VCD_STILL :
			frame_interval = 30; // 30 Frame periods
			if( audio_files.size() > 0 && video_files.size() > 2  )
				mjpeg_error_exit1("VCD stills stream: no audio and no more than two video streams\n");
			{
				VCDStillsStream *str[2];
				ConstantFrameIntervals *intervals[2];
			
				for( i = 0; i<video_files.size(); ++i )
				{
					intervals[i] = new ConstantFrameIntervals( frame_interval );
					str[i] = new VCDStillsStream(ostrm, intervals[i] );
					strms.push_back( str[i] );
					str[i]->Init( video_files[i] );
				}
				if( video_files.size() == 2 )
				{
					str[0]->SetSibling(str[1]);
					str[1]->SetSibling(str[0]);
				}
			}
			break;
		case MPEG_FORMAT_SVCD_STILL :
			frame_interval = 30;
			if(  video_files.size() > 1 )
				mjpeg_error_exit1("SVCD stills streams may only contain a single video stream\n");
			{
				ConstantFrameIntervals *intervals;
				StillsStream *str;
				intervals = new ConstantFrameIntervals( frame_interval );
				str = new StillsStream(ostrm, intervals );
				strms.push_back( str );
				str->Init( video_files[0] );
			}
			break;
		default:
			mjpeg_error_exit1("Only VCD and SVCD stills format for the moment...\n");
		}

		ostrm.OutputMultiplex( &strms,  multi_file);
		
	}
	else
	{
		ostrm.InitSyntaxParameters();
		mjpeg_info( "Found %d video streams and %d audio streams\n",
					video_files.size(),
					audio_files.size() );
		for( i = 0 ; i < video_files.size() ; ++i )
		{
			VideoStream *videoStrm = new VideoStream(ostrm);
			videoStrm->Init( i, video_files[i] );
			strms.push_back( videoStrm );
		}
		for( i = 0 ; i < audio_files.size() ; ++i )
		{
			AudioStream *audioStrm = new AudioStream(ostrm);
			audioStrm->Init ( i, audio_files[i]);
			strms.push_back(audioStrm);
		}
		
		ostrm.OutputMultiplex( &strms,  multi_file);
	}
	for( i = 0; i < strms.size(); ++i )
		delete strms[i];
    return (0);	
}

			
