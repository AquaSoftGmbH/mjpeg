
/*************************************************************************
*
*  mplex - General-purpose MPEG-1/2 multiplexer.
* (C) 2000, 2001 Andrew Stevens <andrew.stevens@philips.com>
* 
*  Constructed using mplex - MPEG-1/SYSTEMS multiplexer as starting point
*  Copyright (C) 1994 1995 Christoph Moar
*  Siemens ZFE ST SN 11 / T SN 6
*
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

#include "videostrm.hh"
#include "stillsstream.hh"
#include "audiostrm.hh"

#include "outputstream.hh"
/*************************************************************************
    Main
*************************************************************************/


int main (int argc, char* argv[])
{
    vector<IBitStream *> mpa_files;
    vector<IBitStream *> ac3_files;
    vector<IBitStream *> video_files;
    char    *multi_file = NULL;	
	int     i;
	int     optargs;
	OutputStream ostrm;
	vector<ElementaryStream *> strms;
    off_t audio_bytes, video_bytes;
    clockticks first_frame_PTS = 0;

    optargs = intro_and_options (argc, argv, &multi_file);
	check_files (argc-optargs, argv+optargs,  
				 mpa_files,
				 ac3_files,
				 video_files);
	mjpeg_info( "Found %d video streams %d MPEG audio streams and %d AC3 streams\n",
				video_files.size(),
				mpa_files.size(),
				ac3_files.size());


	if( MPEG_STILLS_FORMAT(opt_mux_format) )
	{
		mjpeg_info( "Multiplexing stills stream!\n" );
		ostrm.InitSyntaxParameters();
		unsigned int frame_interval;

		switch( opt_mux_format )
		{
		case MPEG_FORMAT_VCD_STILL :
			frame_interval = 30; // 30 Frame periods
			if( mpa_files.size() > 0 && video_files.size() > 2  )
				mjpeg_error_exit1("VCD stills: no more than two streams (one normal one hi-res) possible\n");
			{
				VCDStillsStream *str[2];
				ConstantFrameIntervals *intervals[2];
			
				for( i = 0; i<video_files.size(); ++i )
				{
					intervals[i] = new ConstantFrameIntervals( frame_interval );
					str[i] = new VCDStillsStream( *video_files[i],
												  ostrm, intervals[i] );
					strms.push_back( str[i] );
					str[i]->Init();
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

			if( video_files.size() > 1 )
			{
				mjpeg_error_exit1("SVCD stills streams may only contain a single video stream\n");
			}
			else if(  video_files.size() > 0 )
			{
				ConstantFrameIntervals *intervals;
				StillsStream *str;
				intervals = new ConstantFrameIntervals( frame_interval );
				str = new StillsStream( *video_files[0],ostrm, intervals );
				strms.push_back( str );
				str->Init();
			}
			else 
			for( i = 0 ; i < mpa_files.size() ; ++i )
			{
				AudioStream *audioStrm = new MPAStream( *mpa_files[i], ostrm);
				audioStrm->Init ( i);
				strms.push_back(audioStrm);
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

		if( video_files.size() < 1 	&& opt_mux_format == MPEG_FORMAT_VCD )
		{
			mjpeg_warn( "Multiplexing audio-only for a standard VCD is very inefficient\n");
		}

		for( i = 0 ; i < video_files.size() ; ++i )
		{
			VideoStream *videoStrm;
			//
			// The first DVD video stream is made the master stream...
			//
			if( i == 0 && opt_mux_format ==  MPEG_FORMAT_DVD )
				videoStrm = new DVDVideoStream( *video_files[i], ostrm);
			else
				videoStrm = new VideoStream( *video_files[i],ostrm);
			videoStrm->Init( i );
			strms.push_back( videoStrm );
		}
		for( i = 0 ; i < mpa_files.size() ; ++i )
		{
			AudioStream *audioStrm = new MPAStream( *mpa_files[i], ostrm);
			audioStrm->Init ( i );
			strms.push_back(audioStrm);
		}
		for( i = 0 ; i < ac3_files.size() ; ++i )
		{
			AudioStream *audioStrm = new AC3Stream( *ac3_files[i], ostrm);
			audioStrm->Init ( i );
			strms.push_back(audioStrm);
		}
		
		ostrm.OutputMultiplex( &strms,  multi_file);
	}
	for( i = 0; i < strms.size(); ++i )
		delete strms[i];
    return (0);	
}

			
