// lav2divx
//
// A utility for converting from lavtools generated files (avi, mov or editlist)
// to divx3 or divx4 using avifile 0.53 or higher.
//
// (c) 2001/07/13 Shawn Sulma <lavtools@athos.cx>
//      based on very helpful work by the lavtools, mjpeg.sourceforge.net
//      and x2divx (Ulrich Hecht et al, www.emulinks.de/divx).  It is
//      licensed under the GPL v2.
//
//      This program is free software; you can redistribute it and/or modify
//      it under the terms of the GNU General Public License as published by
//      the Free Software Foundation; either version 2 of the License, or
//      (at your option) any later version.
//
//      This program is distributed in the hope that it will be useful,
//      but WITHOUT ANY WARRANTY; without even the implied warranty of
//      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//      GNU General Public License for more details.
//
//      You should have received a copy of the GNU General Public License
//      along with this program; if not, write to the Free Software
//      Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
//      USA
//
//
// NOTES: the encoding to divx is hardcoded, but there's nothing special
// about it so in theory, you could have it encode to any available avifile
// format.
//
// Additionally, this utility uses the mjpeg/lavtools mjpeg-to-yuv
// conversion utils from lav2yuv (using jpeg-mmx).  This seems to be a bit
// (10-15% on my machine) slower than than the Morgan MJPEG codec.  However,
// the latter is a shareware device that has timeouts (not to mention
// licensing issues).  Additionally, using the lav2yuv/jpeg-mmx libraries
// allows me to add the lsb-drop, noise filter, monochrome, and cropping
// options that would be more difficult to add if done another way.
//
// This utility is a mix of C (from lavtools) and C++ (from x2divx).  I'm not
// a guru at either, but it works for me and is very handy.
//
// Only very limited processing is possible through this application -- a
// simple noise filter, cropping and dropping of lsbs.  Anything more complicated
// and you really should start with lav2yuv, use any of the other yuv tools
// and finish off with yuv2divx.
//
// This is very early code, so expect unusual things to happen.  Also expect
// inefficiencies, naive assumptions, and just plain Wrongness.  Any helpful
// constructive criticism is welcome.
//
// 2001-10-28 v0.0.20
//
// - added right and left border guessing (and top and bottom work!)
// - added ability (-E) to select any available avifile codec
//   (only tested against CVS avifile 0.6 as of this date).  
// - code formatting made internally consistent.
// - reduced use of [f]printf wherever possible
// - added ability (-m) to combine stereo audio to mono
//
// 2002-02-16 v0.0.21
//
// - added -L/--listcodecs to display a list of available avifile codecs
//   it is dependent on what avifile thinks is available, and is mostly
//   for troubleshooting.  Warning: it can be a LONG list.
//
// 2002-02-24 v0.0.22 - fixed breakage caused by editlist changes
//
// 2002-03-03 v0.0.23 - now using "libavifile.h" to hide avifile versioning tricks.
//

#define APPNAME "lav2divx"
#define APPVERSION "0.0.23"
#define LastChanged "2002/03/02"
#define __MODULE__ APPNAME	// Needed for avifile exceptions

#include <iostream>
#include <math.h>		// M_PI is better than included PI constant
#include <sys/time.h>
#include <unistd.h>		// Needed for the access call to check if file exists
#include <getopt.h>		// getopt
#include <stdint.h>		// standard integer types
#include <stdlib.h>		// standard library with integer division
#include <config.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>

extern "C"
{
#include "lav_common.h"
}
#include "libavifile.h"

#define NTSC 'n'

void
error ( char *text )
{
	mjpeg_error ( text );
}

// parameters for the border-finding routines.
#define BORDER_LSB_DROP 0
#define BORDER_SOFT 16

static int
guesstop ( uint8_t *frame, int width, int height )
{
 	long currY;
 	long oldY = 0;
        for ( int y = 0; y < height; y++ )
	{
                currY = 0;
		for ( int x = 0; x < width; x++ )
                {
           		currY += frame[y * width + x];
                }
 		currY /= height;
 		currY = currY>> BORDER_LSB_DROP;
                if ( currY > oldY + BORDER_SOFT)
                {
 			return y;
		}
    		oldY = currY;
        }
 	return 0;
}

static int
guessbottom ( uint8_t *frame, int width, int height )
{
 	long currY;
 	long oldY = 0;
 	for ( int y = height - 1; y > 0; y-- )
 	{
 	 	currY = 0;
 	 	for ( int x = 0; x < width; x++ )
 	 	{
 	 	 	currY += frame[y * width + x];
 	 	}
 	 	currY /= height;
 	 	currY = currY>> BORDER_LSB_DROP;
 	 	if ( currY > oldY + BORDER_SOFT) 
 	 	{
 	 	 	return y;
 	 	}
 	 	oldY = currY;
 	}
 	return 0;
}

static int
guessleft ( uint8_t *frame, int width, int height )
{
 	long currY;
 	long oldY = 0;
 	for ( int x = 0; x < width; x++ )
 	{
 	 	currY = 0;
 	 	for ( int y = 0; y < height; y++ )
 	 	{
 	 	 	currY += frame[y * width + x];
 	 	}
 	 	currY /= height;
 	 	currY = currY>> BORDER_LSB_DROP;
 	 	if ( currY > oldY + BORDER_SOFT)
 	 	{
 	 	 	return x;
 	 	}
 	 	oldY = currY;
 	}
 	return 0;
}

static int
guessright ( uint8_t *frame, int width, int height )
{
 	long currY;
 	long oldY = 0;
 	for ( int x = width - 1; x >= 0; x-- )
 	{
 	 	currY = 0;
 	 	for ( int y = 0; y < height; y++ )
 	 	{
 	 	 	currY += frame[y * width + x];
 	 	}
 	 	currY /= height;
 	 	currY = currY>> BORDER_LSB_DROP;
 	 	if ( currY > oldY + BORDER_SOFT)
 	 	{
 	 	 	return x;
 	 	}
 	 	oldY = currY;
 	}
 	return 0;
}

static void
print_usage ( void )
{
	mjpeg_info ( " " );
	mjpeg_info ( "Usage: %s [OPTION]... [input AVI]... -o [output AVI]", APPNAME );
	exit ( 0 );
}

static void
print_help ( void )
{
	printf ( "\nUsage: %s [OPTION]... [input AVI]... -o [output AVI]\n", APPNAME );
	printf ( "\nOptions:\n" );
	printf ( "  -b --divxbitrate\tDivX ;-) bitrate (default 1800)\n" );
//	printf ( "  -f --fast_motion\tuse fast-motion codec (default low-motion)\n" );
	printf ( "  -a --mp3bitrate\tMP3 bitrate (kBit, default auto)\n" );
	printf ( "  -e --endframe\t\tnumber of frames to encode (default all)\n" );
	printf ( "  -c --crop\t\tcrop output to \"wxh+x+y\" (default full frame)\n" );
	printf ( "  -o --outputfile\tOutput filename IS REQUIRED\n" );
	printf ( "  -v --version\t\tVersion and license.\n" );
	printf ( "  -s --forcedaudiorate\taudio sample rate of input file (Hz);\n\t\t\tuse only if avifile gets it wrong\n" );
	printf ( "  -n --noise\t\tnoise filter (0..2, default 0)\n" );
	printf ( "  -g --guess\t\tguess values for -c and -z options\n" );
#if AVIFILE_MAJOR_VERSION == 0 && AVIFILE_MINOR_VERSION >= 60
	printf ( "  -L --listcodecs\tdisplay available avifile codecs (LONG)\n" );
	printf ( "  -k --keyframes\tset keyframes attribute (default 15)\n" );
	printf ( "  -C --crispness\tset crispness attribute (default 20)\n" );
#endif
        printf ( "  -E --encoder\t\tspecify the fourcc of the desired encoder\n");
	printf ( "  -m --mono\t\tcovert stereo input to mono output\n" );
	printf ( "     --help\t\tPrint this help list.\n\n" );
	exit ( 0 );
};

static void
display_license ( void )
{
 	mjpeg_info ( "This is %s version %s ", APPNAME, APPVERSION );
	mjpeg_info ( "%s", "Copyright (C) Shawn Sulma <lavtools@athos.cx>" );
	mjpeg_info ( "Based on code from Ulricht Hecht and the MJPEG Square. This" );
	mjpeg_info ( "program is distributed in the hope that it will be useful, but" );
	mjpeg_info ( "WITHOUT ANY WARRANTY; without even the implied warranty of" );
	mjpeg_info ( "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE." );
	mjpeg_info ( "See the GNU General Public License for more information." );
	exit ( 0 );
};

static void
displayGreeting (  )
{
	mjpeg_info ( "===========================================" );
	mjpeg_info ( "%s", APPNAME );
        mjpeg_info ( "-----------------------------" );
	mjpeg_info ( "MJPEGTools version %s", VERSION );
	mjpeg_info ( "%s version %s (%s)", APPNAME, APPVERSION, LastChanged );
	mjpeg_info ( " " );
      	mjpeg_info ( "This utility is development software.  It may eat your" );
	mjpeg_info ( "movies or let the smoke out of your computer." );
	mjpeg_info ( "-----------------------------" );
}

#if AVIFILE_MAJOR_VERSION == 0 && AVIFILE_MINOR_VERSION >= 60
static void
listCodecs ( )
{
	BITMAPINFOHEADER bih;
	bih.biCompression = 0xffffffff;
	// just to fill video_codecs list
	Creators::CreateVideoDecoder(bih, 0, 0);

	fprintf ( stderr, "List of available codecs:\n" );
	avm::vector<CodecInfo>::iterator it;
	for ( it = video_codecs.begin (); it != video_codecs.end (); it++ )
	{
		fprintf ( stderr, "Name: %s\n", it->GetName() );
		fprintf ( stderr, "   Module: library %s\n", it->modulename.c_str() );
		fprintf ( stderr, "   Codecs: ");
		avm::vector<fourcc_t>::iterator ic;
		for ( ic = it->fourcc_array.begin () ; ic != it->fourcc_array.end () ; ic++ )
		{
			fprintf ( stderr, "%.4s ", ic );
		}
		fprintf ( stderr, "\n" );
	}
}
#endif

int
main ( int argc, char **argv )
{
	displayGreeting (  );

	if ( GetAvifileVersion (  ) != AVIFILE_VERSION )
	{
		mjpeg_error_exit1 ( "This binary was compiled for Avifile version %s but the library is %s"
			, AVIFILE_VERSION
			, GetAvifileVersion (  ) );
	}

	( void ) mjpeg_default_handler_verbosity ( 1 );

	int copt;		// getopt switch variable

	unsigned int opt_codec = RIFFINFO_DIV3;
	char opt_codec_str[] = "DIV3";
	char *outputfile = NULL;	// gcc howled at some point so I put it here
	int opt_divxbitrate = 1800;
	int opt_mp3bitrate = -1;
	int opt_endframe = -1;
	int opt_outputwidth = 0;
	int opt_outputheight = 0;
	int opt_forcedaudiorate = 0;
	int opt_guess = 0;

	int opt_x = 0;
	int opt_y = 0;
	int opt_w = 0;
	int opt_h = 0;

	int opt_mono = 0;
#if AVIFILE_MAJOR_VERSION == 0 && AVIFILE_MINOR_VERSION >= 60
	int opt_keyframes = 15;
	int opt_crispness = 20;
#endif
	char *arg_geom;
	char *arg_end;
//#endif

	EditList el;
	// LavParam param = { 0, 0, 0, 0, 0, 0, NULL, 0, 0, 440, 220, -1, 4, 2, 0, 0 };
	LavParam param;// = { 0, 0, 0, 0, NULL, 0, 0, 440, 220, -1, 4, 2, 0, 0, 0 };
	uint8_t *lavframe[3];

	param.offset = 0;
	param.frames = 0;
	param.mono = 0;
	param.scenefile = NULL;
	param.delta_lum_threshold = 4;
	param.scene_detection_decimation = 2;
	param.output_width = 0;
	param.output_height = 0;
	param.interlace = -1;
	param.sar = y4m_sar_UNKNOWN;
	param.dar = y4m_dar_4_3;

	// double PI      = 3.14159265359f;    
	// M_PI is the long version in 
	// <math.h> and is more accurate

	// Ok I've redone this whole section for getopt
	// Pretty easy I had to change the options bit above 
	// To steal -o for outputfile and make all options one character
	// The snarly mess with the input files took a while and 
	// Could probably be improved on but this puts paid to
	// core dumps on non-existant or mistyped filenames
	// Since getopt howls when there is no arg to an option 
	// I didn't bother providing my own howling.

	while ( 1 )
	{
		int option_index = 0;

		// Ok a few piddly long option but my gawd there has
		// to be a better way to parse this
		// This example weasels out by just using the
		// short options. Except for help.
		// For real horror, look at wgets options list
		// yea may the gawds smile upon those poor
		// sway backed, downtrodden, programmers.

		static struct option long_options[] = {
			{"help", 0, 0, 0},
			{"version", no_argument, NULL, 'V'},
			{"license", no_argument, NULL, 'V'},
			{"divxbitrate", required_argument, NULL, 'b'},
//			{"fast_motion", no_argument, NULL, 'f'},
			{"mp3bitrate", required_argument, NULL, 'a'},
			{"endframe", required_argument, NULL, 'e'},
			{"crop", required_argument, NULL, 'c'},
			{"forcedaudiorate", required_argument, NULL, 's'},
			{"audio_stream", required_argument, NULL, 'A'},
			{"video_stream", required_argument, NULL, 'V'},
			{"number_cpus", required_argument, NULL, 'U'},
			{"outputfile", required_argument, NULL, 'o'},
#if AVIFILE_MAJOR_VERSION == 0 && AVIFILE_MINOR_VERSION >= 60
			{"keyframes", required_argument, NULL, 'k'},
			{"crispness", required_argument, NULL, 'C'},
			{"listcodecs", no_argument, NULL, 'L'},
#endif
			{"guess", no_argument, NULL, 'g'},
			{"mono", no_argument, NULL, 'm'},
			{"encoder", required_argument, NULL, 'E'},
			{0, 0, 0, 0}
		};

		copt =
#if AVIFILE_MAJOR_VERSION == 0 && AVIFILE_MINOR_VERSION >= 60
			getopt_long ( argc, argv, "LE:fa:e:c:b:o:s:gmvk:C:", long_options, &option_index );
#else
			getopt_long ( argc, argv, "E:fa:e:c:b:o:s:gmv", long_options, &option_index );
#endif
		if ( copt == -1 )
		{
			break;
		}

		switch ( copt )
		{
		case 0:
			if ( long_options[option_index].name == "help" )
			{
				print_help (  );
			}
			break;

		case 'b':
			opt_divxbitrate = atoi ( optarg );
			break;

// -f is deprecated.  use -E DIV4 or -E DIV6 for the fastmotion codec.
//		case 'f':
//			opt_codec = RIFFINFO_DIV4;
//			mjpeg_info ( "Using fast codec." );
//			break;

		case 'a':
			opt_mp3bitrate = atoi ( optarg );
			break;

		case 'e':
			opt_endframe = atoi ( optarg );
			break;

		case 's':
			opt_forcedaudiorate = atoi ( optarg );
			break;

		case 'w':
			opt_outputwidth = atoi ( optarg );
			break;

		case 'h':
			opt_outputheight = atoi ( optarg );
			break;

		case 'g':
			opt_guess = 1;
			opt_endframe = 100;
			break;

		case 'o':
			outputfile = optarg;
			break;

#if AVIFILE_MAJOR_VERSION == 0 && AVIFILE_MINOR_VERSION >= 60
		case 'k':
			opt_keyframes = atoi ( optarg );
			break;

		case 'C':
			opt_crispness = atoi ( optarg );
			break;

		case 'L':
			listCodecs ( );
			exit ( 0 );
			break;
#endif

		case 'c':	// crop
			arg_geom = optarg;
			opt_w = strtol ( arg_geom, &arg_end, 10 );
			if ( *arg_end != 'x' || opt_w < 100 )
			{
				mjpeg_error_exit1 ( "Bad width parameter" );
				// nerr++;
				break;
			}

			arg_geom = arg_end + 1;
			opt_h = strtol ( arg_geom, &arg_end, 10 );
			if ( ( *arg_end != '+' && *arg_end != '\0' ) || opt_h < 100 )
			{
				mjpeg_error_exit1 ( "Bad height parameter" );
				// nerr++;
				break;
			}
			if ( *arg_end == '\0' )
			{
				break;
			}

			arg_geom = arg_end + 1;
			opt_x = strtol ( arg_geom, &arg_end, 10 );
			if ( ( *arg_end != '+' && *arg_end != '\0' ) || opt_x > 720 )
			{
				mjpeg_error_exit1 ( "Bad x parameter" );
				// nerr++;
				break;
			}

			arg_geom = arg_end + 1;
			opt_y = strtol ( arg_geom, &arg_end, 10 );
			if ( *arg_end != '\0' || opt_y > 240 )
			{
				mjpeg_error_exit1 ( "Bad y parameter" );
				// nerr++;
				break;
			}
			break;

		case 'V':
			display_license (  );
			break;

		case '?':
			print_usage (  );
			break;

		case 'm':
			opt_mono = 1;
			break;

		case 'E':
			if ( strlen ( optarg ) != 4 )
			{
				mjpeg_error_exit1 ( "encoder argument requires a four character fcc." );
			}
			else
			{
				memcpy (opt_codec_str, optarg, 4);
			}
			break;

		case ':':
			mjpeg_error_exit1 ( "You missed giving something an argument." );
		}
	}

	opt_codec = mmioFOURCC ( opt_codec_str[0], opt_codec_str[1], opt_codec_str[2], opt_codec_str[3] );
	mjpeg_info ( "VIDEO: using codec %s for encoding.", opt_codec_str );

	int numfiles = argc - optind;
	if ( numfiles <= 0 )
	{
		mjpeg_error ( "I count the number of input files as %i.  I need at least one.", numfiles );
		print_usage (  );
		exit ( 1 );
	}

	int tempor;
	char *inputfiles[numfiles];
	int i = 0;

	for ( ; optind < argc; optind++ )
	{

		inputfiles[i] = argv[optind];

		// first filename may be norm parameter, so don't check
		// access on them.
		if ( !( strcmp ( inputfiles[i], "+p" ) == 0 || strcmp ( inputfiles[i], "+n" ) == 0 ) )
		{

			tempor = access ( inputfiles[i], R_OK );
			if ( tempor < 0 )
			{
				mjpeg_error_exit1 ( "Sorry there is a problem with file: %s", inputfiles[i] );
			}
		}
		++i;
	}

	// check on output filename here since it 
	// stops a lot of avifile spewing.
	// could also check if file exists and 
	// not allow overwrite...could  
	// add a switch to allow overwrite...nah

	if ( outputfile == NULL )
	{
		mjpeg_error_exit1 ( "Output filename IS REQUIRED use -o <filename>" );
		exit ( 1 );
	}

	// open files with EDITLIST library.
	read_video_files ( inputfiles, numfiles, &el, false );

	// do the read video file thing from el.
	// get the format information from the el.  Set it up in the destination avi.

	double framespersec = ( ( el.video_norm == NTSC ) ? ( 30000.0 / 1001.0 ) : ( 25.0 ) );

	BITMAPINFOHEADER bh;
	memset ( &bh, 0, sizeof ( BITMAPINFOHEADER ) );
	bh.biSize = sizeof ( BITMAPINFOHEADER );
	bh.biPlanes = 1;
	bh.biBitCount = 24;
	bh.biCompression = RIFFINFO_YV12;
	param.output_height = el.video_height;
	param.output_width = el.video_width;

	int origwidth = bh.biWidth = abs ( el.video_width );
	int origheight = bh.biHeight = abs ( el.video_height );

	mjpeg_info ( "VIDEO: input height: %i", el.video_height );
	mjpeg_info ( "VIDEO: input width: %i", el.video_width );
	mjpeg_info ( "VIDEO: frames per second %.5f", framespersec );

	if ( opt_h > 0 )
	{
		opt_h = min ( opt_h, ( bh.biHeight - opt_y ) );
		bh.biHeight = opt_h;
	}
	if ( opt_w > 0 )
	{
		opt_w = min ( opt_w, ( bh.biWidth - opt_x ) );
		bh.biWidth = opt_w;
	}

	bh.biSizeImage = bh.biWidth * bh.biHeight * 3;

	uint8_t *framebuffer = new uint8_t[bh.biWidth * bh.biHeight * 3];
	CImage imtarget ( ( BitmapInfo * ) & bh, framebuffer, false );

	int audioexist = el.has_audio;

	// set output format
	int audio_chan = el.audio_chans;
	int audio_origchan = el.audio_chans;
	int audio_rate = el.audio_rate;
	int audio_bpersamp = el.audio_bits;
	int bytespersamp = audio_bpersamp / 8;
	uint8_t *audio_buffer;
	int audiolen = 0;

	if ( opt_mono )
	{
		audio_chan = 1;
	}

	if ( audioexist <= 0 )
	{
		audio_chan = 0;
		audio_rate = 0;
		audio_bpersamp = 0;
	}

	if ( opt_forcedaudiorate )
	{
		audio_rate = opt_forcedaudiorate;
	}

	WAVEFORMATEX format;
	int audio_sampsperframe = 0;

	if (audioexist)
	{
		memset ( &format, 0, sizeof ( WAVEFORMATEX ) );
		format.cbSize = sizeof ( WAVEFORMATEX );
		format.wFormatTag = 1;
		format.nChannels = audio_chan;
		format.nSamplesPerSec = audio_rate;
		format.wBitsPerSample = audio_bpersamp;
		format.nAvgBytesPerSec = audio_rate * audio_chan * audio_bpersamp / 8;
		format.nBlockAlign = ( audio_bpersamp * audio_chan ) / 8;

		audio_sampsperframe = ( int ) ( ( ( double ) audio_rate ) / ( framespersec ) );

	 	mjpeg_info ( "AUDIO: FormatTag = %i", format.wFormatTag );
		mjpeg_info ( "AUDIO: Channels = %i", format.nChannels );
		mjpeg_info ( "AUDIO: Samples per Sec = %i", format.nSamplesPerSec );
	 	mjpeg_info ( "AUDIO: Block Align = %i", format.nBlockAlign );
		mjpeg_info ( "AUDIO: Avg Bytes per Sec = %i", format.nAvgBytesPerSec );
		mjpeg_info ( "AUDIO: Bits Per Sample = %i", format.wBitsPerSample );
		mjpeg_info ( "AUDIO: cbSize = %i", format.cbSize );

	        audio_buffer = ( uint8_t * ) malloc ( audio_sampsperframe * format.nBlockAlign * 2);

	}

	unsigned int fccHandler = opt_codec;
	IAviWriteFile *avifile;

	avifile = CreateIAviWriteFile ( outputfile );

#if AVIFILE_MAJOR_VERSION == 0 && AVIFILE_MINOR_VERSION >= 60
	const CodecInfo *codecInfo = CodecInfo::match ( fccHandler );
	if (codecInfo == NULL)
	{
         	mjpeg_error_exit1 ( "Avifile could not find codec '%s'", opt_codec_str );
	}
	Creators::SetCodecAttr ( *codecInfo, "BitRate", opt_divxbitrate );
	Creators::SetCodecAttr ( *codecInfo, "Crispness", opt_crispness );
	Creators::SetCodecAttr ( *codecInfo, "KeyFrames", opt_keyframes );
#else
	IVideoEncoder::SetExtendedAttr ( fccHandler, "BitRate", opt_divxbitrate );
#endif
	IAviVideoWriteStream *stream =
		avifile->AddVideoStream ( fccHandler, &bh, ( ( int ) ( ( 1.0 / framespersec ) * 1000000.0 ) ) );

	stream->SetQuality ( 65535 );
	stream->Start (  );

	IAviAudioWriteStream *astream;

	if ( audioexist > 0 )
	{
		if ( opt_mp3bitrate == -1 )
		{
			switch ( audio_rate )
			{
			case 48000:
				opt_mp3bitrate = 80 * audio_chan;
				break;
			case 44100:
				opt_mp3bitrate = ( ( audio_chan < 2 ) ? 56 : 64 ) * audio_chan;
				break;
			case 22050:
				opt_mp3bitrate = 32 * audio_chan;
				break;
			default:
				opt_mp3bitrate = 16 * audio_chan;
				mjpeg_warn ( "No mp3 bitrate available for audio rate %d, defaulting to %d"
					, audio_rate
					, opt_mp3bitrate );
			}
		}
		mjpeg_info ( "AUDIO: MP3 rate: %i kilobits/second, %i Bytes/second"
			, opt_mp3bitrate
			, ( opt_mp3bitrate / 125 ) );
		astream = avifile->AddAudioStream ( 0x55, &format, ( opt_mp3bitrate * 1000 ) / 8 );
		astream->Start (  );
	}

	struct timeval tv;
	struct timezone tz = { 0, 0 };
	gettimeofday ( &tv, &tz );
	long startsec = tv.tv_sec;
	long startusec = tv.tv_usec;
	double fps = 0;

	// used for determining a possible clipping window
	int lineguesstop = 0;
	int lineguessbottom = 0;
	int lineguesstopcount = 0;
	int lineguessbottomcount = 0;
	int colguessleft = 0;
	int colguessright = 0;
	int colguessleftcount = 0;
	int colguessrightcount = 0;

	init(&param, lavframe);	// initialize lav2yuv code.

#ifdef HAVE_LIBDV
	lav_init_dv_decoder();
#endif

	int yplane = bh.biWidth * bh.biHeight;
	int vplane = yplane / 4;
	int uplane = yplane / 4;

	int asis = 0;

	if ( ( opt_x > 0 ) || ( opt_y > 0 ) || ( opt_h > 0 ) || ( opt_w > 0 ) )
	{
		asis = 1;
		mjpeg_info ( "VIDEO: output cropped to: top: %i, left: %i, height: %i, width: %i"
			, opt_x, opt_y, opt_h, opt_w );
	}

	long oldtime = 0;
	double secsleft = 0;

	if ( ( opt_endframe < 1 ) || ( opt_endframe > el.video_frames ) )
	{
		opt_endframe = el.video_frames;
	}

	for ( int currentframe = 0; currentframe < opt_endframe; currentframe++ )
	{
		readframe(currentframe, lavframe, &param, el);

		gettimeofday ( &tv, &tz );
		if ( ( tv.tv_sec != oldtime ) && ( !opt_guess ) )
		{
			fps = currentframe / ( ( tv.tv_sec + tv.tv_usec / 1000000.0 ) - ( startsec + startusec / 1000000.0 ) );
			oldtime = tv.tv_sec;
			secsleft = ( ( double ) ( opt_endframe - currentframe ) ) / fps;
			mjpeg_info ( "Encoding frame %i of %i, %.1f frames per sec, %.1f seconds left.    "
				, currentframe
				, opt_endframe
				, fps
				, secsleft );
			fflush ( stdout );
		}

		// CLIPPING
		//
		// If there's no clipping, use a faster way of copying the frame data.
		//
		if ( asis < 1 )	//faster
		{
			memcpy ( framebuffer, lavframe[0], yplane );
			memcpy ( framebuffer + yplane, lavframe[2], vplane );
			memcpy ( framebuffer + yplane + vplane, lavframe[1], uplane );
		}
		else		// slower, but handles cropping.
		{
			int chromaw = opt_w / 2;
			int chromax = opt_x / 2;
			int chromawidth = param.output_width / 2;
			for ( int yy = 0; yy < opt_h; yy++ )
			{
				int chromaoffset = ( yy / 2 ) * chromaw;
				int chromaoffsetsrc = ( ( yy + opt_y ) / 2 ) * chromawidth;
				memcpy ( framebuffer + yy * opt_w,
					 lavframe[0] + ( yy + opt_y ) * param.output_width + opt_x, opt_w );
				memcpy ( framebuffer + yplane + chromaoffset, lavframe[2] + chromaoffsetsrc + chromax,
					 chromaw );
				memcpy ( framebuffer + yplane + vplane + chromaoffset,
					 lavframe[1] + chromaoffsetsrc + chromax, chromaw );
			}
		}

		if ( opt_guess )
		{
		 	int border = guesstop ( framebuffer, bh.biWidth, bh.biHeight );
		 	lineguesstopcount += ( border > 0 ) ? 1 : 0;
		 	lineguesstop += border;
		 	border = guessbottom ( framebuffer, bh.biWidth, bh.biHeight );
		 	lineguessbottomcount += ( border > 0 ) ? 1 : 0;
		 	lineguessbottom += border;
		 	border = guessleft ( framebuffer, bh.biWidth, bh.biHeight );
		 	colguessleftcount += ( border > 0 ) ? 1 : 0;
		 	colguessleft += border;
		 	border = guessright ( framebuffer, bh.biWidth, bh.biHeight );
		 	colguessrightcount += ( border > 0 ) ? 1 : 0;
		 	colguessright += border;
		}

		stream->AddFrame ( &imtarget );

		if ( audioexist )
		{
			audiolen = el_get_audio_data ( audio_buffer, currentframe, &el, 0 );
			if ( opt_mono )
			{
				// collapse to mono from stereo.
				//
				// I have no doubts this is a very naive solution.
				// improvements welcome.
				//
				for ( int samp = 0; samp < audiolen; samp++ )
				{
					short value = 0;
					int offset = samp * bytespersamp * audio_origchan;
					for ( int sampchan = 0; sampchan < audio_origchan; sampchan++ )
					{
						int chanoffset = offset + sampchan * bytespersamp;
						int sampleval = 0;
						void *ptr = audio_buffer + chanoffset;
						if ( bytespersamp == 1 )
						{
							unsigned char *sample = ( unsigned char * ) ptr;
							sampleval = sample[0];
						}
						else if ( bytespersamp == 2 )
						{
							short *sample = ( short * ) ptr;
							sampleval = sample[0];
						}
						value += ( sampleval / 2 );
					}
					//value /= audio_origchan;
					if ( bytespersamp == 1 )
					{
						audio_buffer[samp] = ( uint8_t ) value;
					}
					else if ( bytespersamp == 2 )
					{
						short *destptr = ( short * ) audio_buffer + samp;
						destptr[0] = ( short ) value;
					}
					// audio_buffer[offset/audio_origchan] = value;
				}
			}
			astream->AddData ( audio_buffer, audiolen );
		}
	}

	stream->Stop (  );
	delete avifile;

	mjpeg_info ( " ");
	mjpeg_info ( "Done." );

	if ( opt_guess )
	{
	 	int avgtop = lineguesstop / ( max ( lineguesstopcount, 1 ) );
	 	int avgbottom = lineguessbottom / ( max ( lineguessbottomcount, 1 ) );
	 	int avgleft = colguessleft / ( max ( colguessleftcount, 1 ) );
	 	int avgright = colguessright / ( max ( colguessrightcount, 1 ) );

		avgright = (avgright==0) ? bh.biWidth : avgright;
		avgbottom = (avgbottom==0) ? bh.biHeight : avgbottom;
	 	mjpeg_info ( "avg top border: %i", avgtop );
	 	mjpeg_info ( "avg bottom border: %i", avgbottom );
	 	mjpeg_info ( "avg left border: %i", avgleft );
	 	mjpeg_info ( "avg right border: %i", avgright );

		// cropping window only like multiples of four in each direction.
		avgtop += avgtop % 4;
		avgbottom -= avgbottom % 4;
		avgleft += avgleft % 4;
		avgright -= avgright % 4;
		mjpeg_info ( "suggested options: -c %dx%d+%d+%d"
		        , ( avgright - avgleft )
		        , ( avgbottom - avgtop )
		        , avgleft
		        , avgtop );
	}

}
