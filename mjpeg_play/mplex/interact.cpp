
/*
 *  interact.cc:  Simple command-line front-end
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


#include <config.h>
#include <stdlib.h>
#include <unistd.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#include <sys/stat.h>

#include <mjpeg_logging.h>
#include <format_codes.h>

#include "interact.hh"
#include "videostrm.hh"
#include "audiostrm.hh"
#include "zalphastrm.hh"
#include "mplexconsts.hh"
#include "aunit.hh"

vector<LpcmParams *> opt_lpcm_param;
vector<VideoParams *> opt_video_param;

MultiplexJob::MultiplexJob()
{
    verbose = 1;
    data_rate = 0;  /* 3486 = 174300B/sec would be right for VCD */
    video_offset = 0;
    audio_offset = 0;
    sector_size = 2048;
    VBR = false;
    mpeg = 1;
    mux_format = MPEG_FORMAT_MPEG1;
    multifile_segment = false;
    always_system_headers = 0;
    packets_per_pack = 20;
    max_timeouts = 10;
    max_PTS = 0;
    emul_vcdmplex = 0;
    max_segment_size = 0;
    outfile_pattern = 0;
    packets_per_pack = 1;
}

/*************************************************************************
    Startbildschirm und Anzahl der Argumente

    Intro Screen and argument check
*************************************************************************/

    
void MultiplexJob::Usage(char *str)
{
    fprintf( stderr,
	"mjpegtools mplex-2 version " VERSION "\n"
	"Usage: %s [params] -o <output filename pattern> <input file>... \n"
	"         %%d in the output file name is by segment count\n"
	"  where possible params are:\n"
	"--verbose|-v num\n"
    "  Level of verbosity. 0 = quiet, 1 = normal 2 = verbose/debug\n"
	"--format|-f fmt\n"
    "  Set defaults for particular MPEG profiles\n"
	"  [0 = Generic MPEG1, 1 = VCD, 2 = user-rate VCD, 3 = Generic MPEG2,\n"
    "   4 = SVCD, 5 = user-rate SVCD\n"
	"   6 = VCD Stills, 7 = SVCD Stills, 8 = DVD with NAV sectors, 9 = DVD]\n"
    "--mux-bitrate|-r num\n"
    "  Specify data rate of output stream in kbit/sec\n"
	"    (default 0=Compute from source streams)\n"
	"--video-buffer|-b num [, num...] \n"
    "  Specifies decoder buffers size in kB.  [ 20...2000]\n"
    "--lpcm-params | -L samppersec:chan:bits [, samppersec:chan:bits]\n"
	"--mux-limit|-l num\n"
    "  Multiplex only num seconds of material (default 0=multiplex all)\n"
	"--sync-offset|-O num\n"
    "  Specify offset of timestamps (video-audio) in mSec\n"
	"--sector-size|-s num\n"
    "  Specify sector size in bytes for generic formats [256..16384]\n"
    "--vbr|-V\n"
    "  Multiplex variable bit-rate video\n"
	"--packets-per-pack|-p num\n"
    "  Number of packets per pack generic formats [1..100]\n"
	"--system-headers|-h\n"
    "  Create System header in every pack in generic formats\n"
	"--max-segment-size|-S size\n"
    "  Maximum size of output file(s) in Mbyte (default: 2000) (0 = no limit)\n"
	"--split-segment|-M\n"
    "  Simply split a sequence across files rather than building run-out/run-in\n"
	"--help|-?\n"
    "  Print this lot out!\n", str);
	exit (1);
}

static const char short_options[] = "o:b:r:O:v:m:f:l:s:S:q:p:L:VXMeh";

#if defined(HAVE_GETLONG)
static struct option long_options[] = 
{
     { "verbose",           1, 0, 'v' },
     { "format",            1, 0, 'f' },
     { "mux-bitrate",       1, 0, 'r' },
     { "video-buffer",      1, 0, 'b' },
     { "lpcm-params",       1, 0, 'L' },
     { "output",            1, 0, 'o' },
     { "sync-offset",    	1, 0, 'O' },
     { "vbr",      	        1, 0, 'V' },
     { "system-headers",    1, 0, 'h' },
     { "split-segment",     0, 0, 'M' },
     { "max-segment-size",  1, 0, 'S' },
     { "mux-upto",   	    1, 0, 'l' },
     { "packets-per-pack",  1, 0, 'p' },
     { "sector-size",       1, 0, 's' },
     { "help",              0, 0, '?' },
     { 0,                   0, 0, 0   }
 };
#endif


bool MultiplexJob::ParseLpcmOpt( const char *optarg )
{
    char *endptr, *startptr;
    unsigned int samples_sec;
    unsigned int channels;
    unsigned int bits_sample;
    endptr = const_cast<char *>(optarg);
    do 
    {
        startptr = endptr;
        samples_sec = static_cast<unsigned int>(strtol(startptr, &endptr, 10));
        if( startptr == endptr || *endptr != ':' )
            return false;


        startptr = endptr+1;
        channels = static_cast<unsigned int>(strtol(startptr, &endptr, 10));
        if(startptr == endptr || *endptr != ':' )
            return false;

        startptr = endptr+1;
        bits_sample = static_cast<unsigned int>(strtol(startptr, &endptr, 10));
        if( startptr == endptr )
            return false;

        LpcmParams *params = LpcmParams::Checked( samples_sec,
                                                  channels,
                                                  bits_sample );
        if( params == 0 )
            return false;
        lpcm_param.push_back(params);
        if( *endptr == ',' )
            ++endptr;
    } while( *endptr != '\0' );
    return true;
}


bool MultiplexJob::ParseVideoOpt( const char *optarg )
{
    char *endptr, *startptr;
    unsigned int buffer_size;
    endptr = const_cast<char *>(optarg);
    do 
    {
        startptr = endptr;
        buffer_size = static_cast<unsigned int>(strtol(startptr, &endptr, 10));
        if( startptr == endptr )
            return false;

        VideoParams *params = VideoParams::Checked( buffer_size );
        if( params == 0 )
            return false;
        video_param.push_back(params);
        if( *endptr == ',' )
            ++endptr;
    } 
    while( *endptr != '\0' );
    return true;
}

void MultiplexJob::SetFromCmdLine(unsigned int argc, char *argv[])
{
    int n;
    outfile_pattern = NULL;

#if defined(HAVE_GETLONG)
	while( (n=getlong(argc,argv,short_options,long_options, NULL)) != -1 )
#else
    while( (n=getopt(argc,argv,short_options)) != -1 )
#endif
	{
		switch(n)
		{
        case 0 :
            break;
		case 'o' :
			outfile_pattern = optarg;
			break;
		case 'm' :
			mpeg = atoi(optarg);
			if( mpeg < 1 || mpeg > 2 )
				Usage(argv[0]);
  	
			break;
		case 'v' :
			verbose = atoi(optarg);
			if( verbose < 0 || verbose > 2 )
				Usage(argv[0]);
			break;

		case 'V' :
			VBR = true;
			break;
	  
		case 'h' :
			always_system_headers = 1;
			break;

		case 'b' :
            if( ! ParseVideoOpt( optarg ) )
            {
                mjpeg_error( "Illegal video decoder buffer size(s): %s", 
                             optarg );
                Usage(argv[0]);
            }
            break;
        case 'L':
            if( ! ParseLpcmOpt( optarg ) )
            {
                mjpeg_error( "Illegal LPCM option(s): %s", optarg );
                Usage(argv[0]);
            }
            break;

		case 'r':
			data_rate = atoi(optarg);
			if( data_rate < 0 )
				Usage(argv[0]);
			/* Convert from kbit/sec (user spec) to 50B/sec units... */
			data_rate = (( data_rate * 1000 / 8 + 49) / 50 ) * 50;
			break;

		case 'O':
			video_offset = atoi(optarg);
			if( video_offset < 0 )
			{
				audio_offset = - video_offset;
				video_offset = 0;
			}
			break;
          
		case 'l' : 
 			max_PTS = atoi(optarg);
			if( max_PTS < 1  )
				Usage(argv[0]);
			break;
		
		case 'p' : 
			packets_per_pack = atoi(optarg);
			if( packets_per_pack < 1 || packets_per_pack > 100  )
				Usage(argv[0]);
			break;
		
	  
		case 'f' :
			mux_format = atoi(optarg);
			if( mux_format < MPEG_FORMAT_MPEG1 || mux_format > MPEG_FORMAT_LAST
                )
				Usage(argv[0]);
			break;
		case 's' :
			sector_size = atoi(optarg);
			if( sector_size < 256 || sector_size > 16384 )
				Usage(argv[0]);
			break;
		case 'S' :
			max_segment_size = atoi(optarg);
			if( max_segment_size < 0  )
				Usage(argv[0]);
			max_segment_size *= 1024*1024; 
			break;
		case 'M' :
			multifile_segment = true;
			break;
		case 'e' :
			emul_vcdmplex = 1;
			break;
		case '?' :
		default :
			Usage(argv[0]);
			break;
		}
	}
	if (argc - optind < 1 || outfile_pattern == NULL)
    {	
		Usage(argv[0]);
    }
	(void)mjpeg_default_handler_verbosity(verbose);
	mjpeg_info( "mplex version %s (%s)",MPLEX_VER,MPLEX_DATE );

    InputStreamsFromCmdLine( argc-(optind-1), argv+optind-1);
}

void MultiplexJob::InputStreamsFromCmdLine (unsigned int argc, char* argv[] )
{
    IFileBitStream *bs;
    IBitStreamUndo undo;
    unsigned int i;
    bool bad_file = false;
    
	for( i = 1; i < argc; ++i )
    {
        bs = new IFileBitStream( argv[i] );
        // Remember the streams initial state...
        bs->PrepareUndo( undo);
        if( MPAStream::Probe( *bs ) )
        {
            mjpeg_info ("File %s looks like an MPEG Audio stream." ,argv[i]);
            bs->UndoChanges( undo);
            mpa_files.push_back( bs );
            continue;
        }

        bs->UndoChanges( undo);
        if( AC3Stream::Probe( *bs ) )
        {
            mjpeg_info ("File %s looks like an AC3 Audio stream.",
                        argv[i]);
            bs->UndoChanges( undo);
            ac3_files.push_back( bs );
            continue;
        }
        bs->UndoChanges( undo);
        if( VideoStream::Probe( *bs ) )
        {
            mjpeg_info ("File %s looks like an MPEG Video stream.",
                        argv[i]);
            bs->UndoChanges( undo);
            video_files.push_back( bs );
            continue;
        }
        bs->UndoChanges( undo);
        if( ZAlphaStream::Probe( *bs ) )
        {
            mjpeg_info ("File %s looks like an Z/Alpha Video stream.",
                        argv[i]);
            bs->UndoChanges( undo);
            z_alpha_files.push_back( bs );
            continue;
        }
        bs->UndoChanges( undo);
        if( LPCMStream::Probe( *bs ) )
        {
            mjpeg_info ("File %s looks like an LPCM Audio stream.",
                        argv[i]);
            bs->UndoChanges( undo);
            lpcm_files.push_back( bs );
            continue;
        }
        bad_file = true;
        delete bs;
        mjpeg_error ("File %s unrecogniseable!", argv[i]);
    }
    
    if( bad_file )
    {
        mjpeg_error_exit1( "Unrecogniseable file(s)... exiting.");
    }

	//
	// Where no parameters for streams have been specified
	// simply set the default values (these will depend on the format
	// we're muxing of course...)
	//

	for( i = video_param.size(); i < video_files.size(); ++i )
	{
		video_param.push_back(VideoParams::Default( mux_format ));
	}
	for( i = lpcm_param.size(); i < lpcm_files.size(); ++i )
	{
		lpcm_param.push_back(LpcmParams::Default(mux_format));
	}

	//
	// Set standard values if the selected profile implies this...
	//
	for( i = 0; i < video_files.size(); ++i )
	{
		if( video_param[i]->Force(mux_format) )
		{
			mjpeg_info( "Video stream %d: profile %d selected - ignoring non-srtandard options!", i, mux_format );
		}
	}
        
}



/* 
 * Local variables:
 *  c-file-style: "stroustrup"
 *  tab-width: 4
 *  indent-tabs-mode: nil
 * End:
 */
