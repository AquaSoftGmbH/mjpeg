
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
#include "mplexconsts.hh"

vector<LpcmParams *> opt_lpcm_param;
vector<VideoParams *> opt_video_param;

int opt_verbosity = 1;
int opt_data_rate = 0;  /* 3486 = 174300B/sec would be right for VCD */
int opt_video_offset = 0;
int opt_audio_offset = 0;
int opt_sector_size = 2324;
int opt_VBR = 0;
int opt_mpeg = 1;
int opt_mux_format = 0;			/* Generic MPEG-1 stream as default */
int opt_multifile_segment = 0;
int opt_always_system_headers = 0;
int opt_packets_per_pack = 20;
int opt_max_timeouts = 10;
clockticks opt_max_PTS = 0;
int opt_emul_vcdmplex = 0;
bool opt_ignore_underrun = false;
bool opt_split_at_seq_end = true;
off_t opt_max_segment_size = 0;
char *opt_multplex_outfile = 0;

/*************************************************************************
    Startbildschirm und Anzahl der Argumente

    Intro Screen and argument check
*************************************************************************/

    
static void Usage(char *str)
{
    fprintf( stderr,
	"mjpegtools mplex version " VERSION "\n"
	"Usage: %s [params] -o <output filename pattern> <input file>... \n"
	"         %%d in the output file name is by segment count\n"
	"  where possible params are:\n"
	"--verbose|-v num\n"
    "  Level of verbosity. 0 = quiet, 1 = normal 2 = verbose/debug\n"
	"--format|-f fmt\n"
    "  Set defaults for particular MPEG profiles\n"
	"  [0 = Generic MPEG1, 1 = VCD, 2 = user-rate VCD, 3 = Generic MPEG2,\n"
    "   4 = SVCD, 5 = user-rate SVCD\n"
	"   6 = VCD Stills, 7 = SVCD Stills, 8 = DVD]\n"
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

#if defined(HAVE_GETOPT_LONG)
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
     { "system-headers", 1, 0, 'h' },
     { "split-segment", 0, &opt_multifile_segment, 1},
     { "max-segment-size",  1, 0, 'S' },
     { "mux-upto",   	    1, 0, 'l' },
     { "packets-per-pack",  1, 0, 'p' },
     { "sector-size",       1, 0, 's' },
     { "help",              0, 0, '?' },
     { 0,                   0, 0, 0 }
 };
#endif


bool parse_lpcm_opt( char *optarg )
{
    char *endptr, *startptr;
    long number;
    unsigned int samples_sec;
    unsigned int channels;
    unsigned int bits_sample;
    endptr = optarg;
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
        opt_lpcm_param.push_back(params);
        if( *endptr == ',' )
            ++endptr;
    } while( *endptr != '\0' );
    return true;
}


bool parse_video_opt( char *optarg )
{
    char *endptr, *startptr;
    long number;
    unsigned int buffer_size;
    endptr = optarg;
    do 
    {
        startptr = endptr;
        mjpeg_info( "BUFFER: %s", optarg );
        buffer_size = static_cast<unsigned int>(strtol(startptr, &endptr, 10));
        if( startptr == endptr )
            return false;

        VideoParams *params = VideoParams::Checked( buffer_size );
        if( params == 0 )
            return false;
        opt_video_param.push_back(params);
        if( *endptr == ',' )
            ++endptr;
    } 
    while( *endptr != '\0' );
    return true;
}

int intro_and_options(int argc, char *argv[])
{
    int n;
	char *outfile = NULL;
    long number;
    char *numptr, *endptr;

#if defined(HAVE_GETOPT_LONG)
	while( (n=getopt_long(argc,argv,short_options,long_options, NULL)) != -1 )
#else
    while( (n=getopt(argc,argv,short_options)) != -1 )
    {
    switch(n)
#endif
	{
		switch(n)
		{
        case 0 :
            break;
		case 'o' :
			outfile = optarg;
			break;
		case 'm' :
			opt_mpeg = atoi(optarg);
			if( opt_mpeg < 1 || opt_mpeg > 2 )
				Usage(argv[0]);
  	
			break;
		case 'v' :
			opt_verbosity = atoi(optarg);
			if( opt_verbosity < 0 || opt_verbosity > 2 )
				Usage(argv[0]);
			break;

		case 'V' :
			opt_VBR = 1;
			break;
	  
		case 'h' :
			opt_always_system_headers = 1;
			break;

		case 'b' :
            if( ! parse_video_opt( optarg ) )
            {
                mjpeg_error( "Illegal video decoder buffer size(s): %s", 
                             optarg );
                Usage(argv[0]);
            }
            break;
        case 'L':
            if( ! parse_lpcm_opt( optarg ) )
            {
                mjpeg_error( "Illegal LPCM option(s): %s", optarg );
                Usage(argv[0]);
            }
            break;

		case 'r':
			opt_data_rate = atoi(optarg);
			if( opt_data_rate < 0 )
				Usage(argv[0]);
			/* Convert from kbit/sec (user spec) to 50B/sec units... */
			opt_data_rate = (( opt_data_rate * 1000 / 8 + 49) / 50 ) * 50;
			break;

		case 'O':
			opt_video_offset = atoi(optarg);
			if( opt_video_offset < 0 )
			{
				opt_audio_offset = - opt_video_offset;
				opt_video_offset = 0;
			}
			break;
          
		case 'l' : 
 			opt_max_PTS = static_cast<clockticks>(atoi(optarg)) * CLOCKS;
			if( opt_max_PTS < 1LL  )
				Usage(argv[0]);
			break;
		
		case 'p' : 
			opt_packets_per_pack = atoi(optarg);
			if( opt_packets_per_pack < 1 || opt_packets_per_pack > 100  )
				Usage(argv[0]);
			break;
		
	  
		case 'f' :
			opt_mux_format = atoi(optarg);
			if( opt_mux_format != MPEG_FORMAT_DVD &&
                (opt_mux_format < MPEG_FORMAT_MPEG1 || opt_mux_format > MPEG_FORMAT_LAST)
                )
				Usage(argv[0]);
			break;
		case 's' :
			opt_sector_size = atoi(optarg);
			if( opt_sector_size < 256 || opt_sector_size > 16384 )
				Usage(argv[0]);
			break;
		case 'S' :
			opt_max_segment_size = atoi(optarg);
			if( opt_max_segment_size < 0  )
				Usage(argv[0]);
			opt_max_segment_size *= 1024*1024; 
			break;
		case 'M' :
			opt_multifile_segment = 1;
			break;
		case 'e' :
			opt_emul_vcdmplex = 1;
			break;
		case '?' :
		default :
			Usage(argv[0]);
			break;
		}
	}
	if (argc - optind < 1 || outfile == NULL)
    {	
		Usage(argv[0]);
    }
	(void)mjpeg_default_handler_verbosity(opt_verbosity);
	mjpeg_info( "mplex version %s (%s)",MPLEX_VER,MPLEX_DATE );
	opt_multplex_outfile = outfile;
	return optind-1;
}



/*************************************************************************
    MPEG Streams Kontrolle

    Basic Checks on MPEG Streams
*************************************************************************/




/*************************************************************************
    Check if files are valid MPEG / AC3 streams
*************************************************************************/

void check_files (int argc,
				  char* argv[],
                  vector<IBitStream *> &mpa_files,
                  vector<IBitStream *> &ac3_files,
                  vector<IBitStream *> &lpcm_files,
				  vector<IBitStream *> &video_files
	)
{
    IBitStream *bs;
    BitStreamUndo undo;
    int i;
    bool bad_file = false;
    
	for( i = 1; i < argc; ++i )
    {
        bs = new IBitStream;
        bs->open( argv[i] );
        // Remember the streams initial state...
        bs->prepareundo( undo);
        if( MPAStream::Probe( *bs ) )
        {
            mjpeg_info ("File %s looks like an MPEG Audio stream." ,argv[i]);
            bs->undochanges( undo);
            mpa_files.push_back( bs );
            continue;
        }

        bs->undochanges( undo);
        if( AC3Stream::Probe( *bs ) )
        {
            mjpeg_info ("File %s looks like an AC3 Audio stream.",
                        argv[i]);
            bs->undochanges( undo);
            ac3_files.push_back( bs );
            continue;
        }
        bs->undochanges( undo);
        if( VideoStream::Probe( *bs ) )
        {
            mjpeg_info ("File %s looks like an MPEG Video stream.",
                        argv[i]);
            bs->undochanges( undo);
            video_files.push_back( bs );
            continue;
        }
        bs->undochanges( undo);
        if( LPCMStream::Probe( *bs ) )
        {
            mjpeg_info ("File %s looks like an LPCM Audio stream.",
                        argv[i]);
            bs->undochanges( undo);
            lpcm_files.push_back( bs );
            continue;
        }
        bad_file = true;
        bs->close();
        delete bs;
        mjpeg_error ("File %s unrecogniseable!", argv[i]);
    }
    
    if( bad_file )
    {
        mjpeg_error_exit1( "Unrecogniseable file(s)... exiting.");
    }
        
}


/*************************************************************************
    File vorhanden?

    File found?
*************************************************************************/

bool open_file(const char *name)			
{
    FILE* datei;
	struct stat stb;

    datei=fopen (name, "rw");

    if (datei==NULL)
    {	
		mjpeg_error("File %s not found.", name);
		return (true);
    }
    fclose(datei);
    return (false);
}


/* 
 * Local variables:
 *  c-file-style: "stroustrup"
 *  tab-width: 4
 *  indent-tabs-mode: nil
 * End:
 */
