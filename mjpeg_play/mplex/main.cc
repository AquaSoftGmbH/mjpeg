
/*************************************************************************
* @mainpage
*  mplex - General-purpose MPEG-1/2 multiplexer.
* (C) 2000, 2001 Andrew Stevens <andrew.stevens@philips.com>
* 
* Doxygen documentation and MPEG Z/Alpha multiplexing part by
* Gernot Ziegler <gz@lysator.liu.se>
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
Very high-level. Parses the command line, tries out different modules
on the input files. If all input files were recognized by the modules, 
it initializes the chosen output module. Tells the output module about
the available input files, and makes the first video stream the master
substream in the output file. 

@param argc number of command line parameters
@param argv content of command line parameters
*************************************************************************/


int main (int argc, char* argv[])
{
    vector<IBitStream *> mpa_files;
    vector<IBitStream *> ac3_files;
    vector<IBitStream *> lpcm_files;
    vector<IBitStream *> video_files;
	int     i;
	int     optargs;
	OutputStream ostrm;
	vector<ElementaryStream *> strms;
    off_t audio_bytes, video_bytes;
    clockticks first_frame_PTS = 0;

    optargs = intro_and_options (argc, argv);
	check_files (argc-optargs, argv+optargs,  
				 mpa_files,
				 ac3_files,
				 lpcm_files,
				 video_files);
	mjpeg_info( "Found %d video streams %d MPEG audio streams %d LPCM audio streams and %d AC3 streams",
				video_files.size(),
				mpa_files.size(),
				lpcm_files.size(),
				ac3_files.size());

	//
	// Where no parameters for streams have been specified
	// simply set the default values (these will depend on the format
	// we're muxing of course...)
	//

	for( i = opt_video_param.size(); i < video_files.size(); ++i )
	{
		opt_video_param.push_back(VideoParams::Default( opt_mux_format ));
	}
	for( i = opt_lpcm_param.size(); i < lpcm_files.size(); ++i )
	{
		opt_lpcm_param.push_back(LpcmParams::Default(opt_mux_format));
	}

	//
	// Set standard values if the selected profile implies this...
	//
	for( i = 0; i < video_files.size(); ++i )
	{
		if( opt_video_param[i]->Force(opt_mux_format) )
		{
			mjpeg_info( "Video stream %d: profile %d selected - ignoring non-srtandard options!", i, opt_mux_format );
		}
	}

	vector<VideoParams *>::iterator vidparm = opt_video_param.begin();
	vector<LpcmParams *>::iterator lpcmparm = opt_lpcm_param.begin();
	if( MPEG_STILLS_FORMAT(opt_mux_format) )
	{
		mjpeg_info( "Multiplexing stills stream!" );
		ostrm.InitSyntaxParameters();
		unsigned int frame_interval;

		switch( opt_mux_format )
		{
		case MPEG_FORMAT_VCD_STILL :
			frame_interval = 30; // 30 Frame periods
			if( mpa_files.size() > 0 && video_files.size() > 2  )
				mjpeg_error_exit1("VCD stills: no more than two streams (one normal one hi-res) possible");
			{
				VCDStillsStream *str[2];
				ConstantFrameIntervals *intervals[2];

				for( i = 0; i<video_files.size(); ++i )
				{
					FrameIntervals *ints = 
						new ConstantFrameIntervals( frame_interval );
					str[i] = 
						new VCDStillsStream( *video_files[i],
											 new StillsParams( *vidparm, ints),
											 ostrm );
					strms.push_back( str[i] );
					str[i]->Init();
					++vidparm;
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
				mjpeg_error_exit1("SVCD stills streams may only contain a single video stream");
			}
			else if(  video_files.size() > 0 )
			{
				ConstantFrameIntervals *intervals;
				StillsStream *str;
				intervals = new ConstantFrameIntervals( frame_interval );
				str = new StillsStream( *video_files[0],
										new StillsParams( *vidparm, intervals ),
										ostrm );
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
			mjpeg_error_exit1("Only VCD and SVCD stills format for the moment...");
		}

		ostrm.OutputMultiplex( &strms,  opt_multplex_outfile);
		
	}
	else
	{
		ostrm.InitSyntaxParameters();

		if( video_files.size() < 1 	&& opt_mux_format == MPEG_FORMAT_VCD )
		{
			mjpeg_warn( "Multiplexing audio-only for a standard VCD is very inefficient");
		}

		for( i = 0 ; i < video_files.size() ; ++i )
		{
			VideoStream *videoStrm;
			//
			// The first DVD video stream is made the master stream...
			//
			if( i == 0 && opt_mux_format ==  MPEG_FORMAT_DVD )
				videoStrm = new DVDVideoStream( *video_files[i], 
												*vidparm,
												ostrm);
			else
				videoStrm = new VideoStream( *video_files[i],
											 *vidparm,
											 ostrm);
			videoStrm->Init( i );
			strms.push_back( videoStrm );
			++vidparm;

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
		for( i = 0 ; i < lpcm_files.size() ; ++i )
		{
			AudioStream *audioStrm = 
				new LPCMStream( *lpcm_files[i], 
								*lpcmparm,
								ostrm);
			audioStrm->Init ( i );
			strms.push_back(audioStrm);
			++lpcmparm;
		}
		
		ostrm.OutputMultiplex( &strms,  opt_multplex_outfile);
	}
	for( i = 0; i < strms.size(); ++i )
		delete strms[i];
    return (0);	
}

			
