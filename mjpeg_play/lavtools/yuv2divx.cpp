// yuv2divx
//
// A utility for converting YUV streams (YUV4MPEG) into DIVX files.  It
// was developed against the lav2yuv and yuv4mpeg utilities from the 
// MJPEG lavtools project.  (mjpeg.sourceforge.net).  It also leans
// heavily on the x2divx project of Ulrich Hecht.  As with both of these
// other projects, this utility is licensed under the GPL v2.
//
// (c) 2001/07/13 Shawn Sulma <lavtools@athos.cx>
//      based on very helpful work by the lavtools, mjpeg.sourceforge.net
//      and x2divx (Ulrich Hecht et al, www.emulinks.de/divx).
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
// This utility is a mix of C (from lavtools) and C++ (from x2divx).  I'm not
// a guru at either, but it works for me and is very handy.
//
// YUV scaling or denoising, or whatever is not performed in this
// application.  That's what yuvscaler et al are for. :)
//
// This is very early code.  Expect unusual things, naive assumptions, odd
// behaviour, and just plain Wrongness.  Helpful, constructive criticism is
// encouraged.  Flames... well... you know where you can stick them.
//
// 2001-10-28 v0.0.20
//
// - added right and left border guessing (and top and bottom work!)
// - added ability to take audio from lav files (elists, etc)
// - added ability (-E) to select any available avifile codec
//   (only tested against CVS avifile 0.6 as of this date).
// - code formatting made internally consistent.
// - reduced use of [f]printf wherever possible
// - fixed a rather nasty audio bug that could arise when odd sample
//   rates were created.  The simple solution is to round up to the
//   next block-align of the sound.  However, the audio track will then
//   slooowly get ahead of the video.  Code inserted to try to keep it
//   on track by request enough blocks to keep the audio stream where it
//   _should_ be.
// - added option (-F) to tell yuv2divx how many frames to expect.  This
//   is only used for the display of the time remaining.  If you don't
//   specify it, yuv2divx will try to guess the frames in the stream
//   by the size of the audio input.  If none, or it runs out of audio
//   data, it stops guessing.
//
// 2001-11-25 v0.0.21 - fixed display bug in audio rate output.
//
// 2002-01-21 v0.0.22 - _really_ fixed display of audio rate output :)
//
// 2002-02-16 v0.0.23
//
// - added -L/--listcodecs to display a list of available avifile codecs
//   it is dependent on what avifile thinks is available, and is mostly
//   for troubleshooting.  Warning: it can be a LONG list.
//
// 2002-02-24 v0.0.24 - fixed breakage caused by editlist changes
//
// 2002-03-03 v0.0.25 - now using "libavifile.h" for avifile specific includes.

#define APPNAME "yuv2divx"
#define APPVERSION "0.0.24"
#define LastChanged "2002/02/24"
#define __MODULE__ APPNAME	// Needed for avifile exceptions

#include <iostream.h>
#include <math.h>
#include <sys/time.h>
#include <unistd.h>		// Needed for the access call to check if file exists
#include <getopt.h>		// getopt
#include <stdint.h>		// standard integer types
#include <stdlib.h>		// standard library with integer division
#include <stdio.h>
#include <config.h>
#include <fcntl.h>
#include <string.h>

extern "C"
{
#include "yuv4mpeg.h"
#include "mjpeg_logging.h"
#include "lav_common.h"
}
#include "libavifile.h"



#define BORDER_LSB_DROP 0
#define BORDER_SOFT 16

typedef struct
{
	char riff[4];		// always "RIFF"
	int length;		// Length (little endian)
	char format[4];		// fourcc with the format.  We only want WAVE
}
RiffHeader;

typedef struct
{
	char fmt[4];		// "fmt_" (ASCII characters)
	int length;		// chunk length
	short que;		// wave format
	short channels;		// channels
	int rate;		// samples per second
	int bps;		// bytes per second
	short bytes;		// bytes per sample:    1 = 8 bit Mono, 
	//                      2 = 16 bit Mono/8 bit stereo
	//                      4 = 16 bit Stereo
	short bits;		// bits per sample
}
WaveHeader;

/* The DATA chunk */

typedef struct
{
	char data[4];		// chunk type ("DATA")
	int length;		// length
}
DataHeader;

int got_sigint = 0;

// From yuvplay
static void
sigint_handler ( int signal )
{
	mjpeg_log ( LOG_WARN, "Caught SIGINT, exiting...\n" );
	got_sigint = 1;
}

void
error ( char *text )
{
	mjpeg_error ( text );
}

static int
guesstop ( unsigned char *frame, int width, int height )
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
guessbottom ( unsigned char *frame, int width, int height )
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
guessleft ( unsigned char *frame, int width, int height )
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
guessright ( unsigned char *frame, int width, int height )
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
	mjpeg_info ( "\n" );
	mjpeg_info ( "Usage: %s [OPTION]... -o [output AVI]\n", APPNAME );
	exit ( 0 );
}

static void
print_help ( void )
{
	printf ( "\nUsage: %s [OPTION]... -o [output AVI]\n", APPNAME );
	printf ( "\nOptions:\n" );
	printf ( "  -b --divxbitrate\tDivX ;-) bitrate (default 1800)\n" );
//      printf ("  -f --fast_motion\tuse fast-motion codec (default low-motion)\n");
	printf ( "  -a --mp3bitrate\tMP3 bitrate (kBit, default auto)\n" );
	printf ( "  -e --endframe\t\tnumber of frames to encode (default all)\n" );
	printf ( "  -c --crop\t\tcrop output to \"wxh+x+y\" (default full frame)\n" );
	printf ( "  -w --width\t\toutput width (use when not available in stream)\n" );
	printf ( "  -h --height\t\toutput height (use when not available in stream)\n" );
	printf ( "  -o --outputfile\tOutput filename IS REQUIRED\n" );
	printf ( "  -V --version\t\tVersion and license.\n" );
	printf ( "  -s --forcedaudiorate\taudio sample rate of input file (Hz);\n\t\t\tuse only if avifile gets it wrong\n" );
	printf ( "  -d --droplsb\t\tdrop x least significant bits (0..3, default 0)\n" );
	printf ( "  -n --noise\t\tnoise filter (0..2, default 0)\n" );
	printf ( "  -g --guess\t\tguess values for -c and -z options\n" );
#if AVIFILE_MAJOR_VERSION == 0 && AVIFILE_MINOR_VERSION >= 60
	printf ( "  -k --keyframes\tset keyframes attribute (default 15)\n" );
	printf ( "  -C --crispness\tset crispness attribute (default 20)\n" );
	printf ( "  -L --listcodecs\tshow the list of avifile codecs (LONG!)\n" );
#endif
	printf ( "  -F --frames\t\tnumber of frames to expect in YUV4MPEG stream\n\t\t\t(for display purposes only)\n" );
	printf ( "  -A --audio\t\tspecify audio file\n" );
	printf ( "  -E --encoder\t\tspecify the fourcc of the desired encoder\n" );
	printf ( "     --help\t\tPrint this help list.\n\n" );
	exit ( 0 );
}

static void
display_license ( void )
{
	mjpeg_info ( "\nThis is %s version %s \n", APPNAME, APPVERSION );
	mjpeg_info ( "%s", "Copyright (C) Shawn Sulma <lavtools@athos.cx>\n" );
	mjpeg_info ( "Based on code from Ulricht Hecht and the MJPEG Square. This\n" );
	mjpeg_info ( "program is distributed in the hope that it will be useful, but\n" );
	mjpeg_info ( "WITHOUT ANY WARRANTY; without even the implied warranty of\n" );
	mjpeg_info ( "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n" );
	mjpeg_info ( "See the GNU General Public License for more information.\n" );
	exit ( 0 );
}

void
displayGreeting (  )
{
	mjpeg_info ( "===========================================\n" );
	mjpeg_info ( "%s\n", APPNAME );
	mjpeg_info ( "-----------------------------\n" );
	mjpeg_info ( "MJPEGTools version %s\n", VERSION );
        mjpeg_info ( "%s version %s (%s)\n", APPNAME, APPVERSION, LastChanged );
	mjpeg_info ( "\n" );
	mjpeg_info ( "This utility is development software.  It may eat your\n" );
	mjpeg_info ( "movies or let the smoke out of your computer.\n" );
	mjpeg_info ( "-----------------------------\n" );
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

	displayGreeting();

	if ( GetAvifileVersion (  ) != AVIFILE_VERSION )
	{
		mjpeg_error_exit1 ( "This binary was compiled for Avifile version %s but the library is %s\n"
			, AVIFILE_VERSION
			, GetAvifileVersion (  ) );
	}

	( void ) mjpeg_default_handler_verbosity ( 1 );

	int copt;		// getopt switch variable

	char *outputfile = NULL;	// gcc howled at some point so I put it here
	char *audiofile = NULL;
	int opt_divxbitrate = 1800;
	int opt_expectframes = 0;
	int opt_mp3bitrate = -1;
	int opt_codec = RIFFINFO_DIV3;
	char opt_codec_str[] = "DIV3";
	int opt_endframe = -1;
	int opt_outputwidth = 0;
	int opt_outputheight = 0;
	int opt_forcedaudiorate = 0;
	int opt_guess = 0;
	int flip = 1;

	int opt_x = 0;
	int opt_y = 0;
	int opt_w = 0;
	int opt_h = 0;

	FILE *fd_audio;
	EditList el_audio;
	char *audio_buffer;
	int audioexist = 0;
	float audio_perframe = 0.0;
	float audio_shouldhaveread = 0.0;
	float audio_thisframe = 0.0;
	int audio_totalbytesread = 0;
	int audio_bytesthisframe = 0;

	int audio_bytes;

#if AVIFILE_MAJOR_VERSION == 0 && AVIFILE_MINOR_VERSION >= 60
	int opt_keyframes = 15;
	int opt_crispness = 20;
#endif
	char *arg_geom;
	char *arg_end;

	/* for the new YUV4MPEG streaming */
	y4m_frame_info_t frameinfo;
	y4m_stream_info_t streaminfo;

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
			{"fast_motion", no_argument, NULL, 'f'},
			{"mp3bitrate", required_argument, NULL, 'a'},
			{"endframe", required_argument, NULL, 'e'},
			{"crop", required_argument, NULL, 'c'},
			{"forcedaudiorate", required_argument, NULL, 's'},
			{"width", required_argument, NULL, 'w'},
			{"height", required_argument, NULL, 'h'},
			{"audio", required_argument, NULL, 'A'},
			{"number_cpus", required_argument, NULL, 'n'},
			{"outputfile", required_argument, NULL, 'o'},
			{"droplsb", required_argument, NULL, 'd'},
			{"noise", required_argument, NULL, 'n'},
#if AVIFILE_MAJOR_VERSION == 0 && AVIFILE_MINOR_VERSION >= 60
			{"keyframes", required_argument, NULL, 'k'},
			{"crispness", required_argument, NULL, 'C'},
			{"listcodecs", no_argument, NULL, 'L' },
#endif
			{"guess", no_argument, NULL, 'g'},
			{"encoder", required_argument, NULL, 'E'},
			{"expectframes", required_argument, NULL, 'F'},
			{0, 0, 0, 0}
		};

		copt =
#if AVIFILE_MAJOR_VERSION == 0 && AVIFILE_MINOR_VERSION >= 60
			getopt_long ( argc, argv, "F:E:A:a:w:h:e:c:b:o:s:n:d:gvVLk:C:", long_options, &option_index );
#else
			getopt_long ( argc, argv, "F:E:A:a:w:h:e:c:b:o:s:n:d:gvV", long_options, &option_index );
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

//      // replaced by -E <fcc>, use -E DIV4 or -E DIV6 to use fast motion
//      case 'f':
//	opt_codec = RIFFINFO_DIV4;
//	break;

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

		case 'A':
			audiofile = optarg;
			audioexist = 1;
			break;

#if AVIFILE_MAJOR_VERSION == 0 && AVIFILE_MINOR_VERSION >= 60
		case 'k':
			opt_keyframes = atoi ( optarg );
			break;

		case 'C':
			opt_crispness = atoi ( optarg );
			break;

		case 'L':
			listCodecs ();
			exit ( 0 );
			break;
#endif

		case 'c':	// crop
			arg_geom = optarg;
			opt_w = strtol ( arg_geom, &arg_end, 10 );
			if ( *arg_end != 'x' || opt_w < 100 )
			{
				mjpeg_error_exit1 ( "Bad width parameter\n" );
				// nerr++;
				break;
			}

			arg_geom = arg_end + 1;
			opt_h = strtol ( arg_geom, &arg_end, 10 );
			if ( ( *arg_end != '+' && *arg_end != '\0' ) || opt_h < 100 )
			{
				mjpeg_error_exit1 ( "Bad height parameter\n" );
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
				mjpeg_error_exit1 ( "Bad x parameter\n" );
				// nerr++;
				break;
			}

			arg_geom = arg_end + 1;
			opt_y = strtol ( arg_geom, &arg_end, 10 );
			if ( *arg_end != '\0' || opt_y > 240 )
			{
				mjpeg_error_exit1 ( "Bad y parameter\n" );
				// nerr++;
				break;
			}
			break;

		case 'V':
			display_license (  );
			break;

		case 'E':
			if ( strlen ( optarg ) != 4 )
			{
				mjpeg_error_exit1 ( "encoder argument requires a four character fcc.\n" );
			}
			else
			{
				memcpy ( opt_codec_str, optarg, 4 );
			}
			break;

		case 'F':
			opt_expectframes = atoi ( optarg );
			if ( opt_expectframes < 1 )
			{
				mjpeg_warn ( "Invalid 'expectframes', ignoring\n" );
			}
			break;

		case '?':
			print_usage (  );
			break;

		case ':':
			mjpeg_warn ( "You missed giving something an argument.\n" );	// since we have non-options
			break;	// never gets called
		}
	}
	opt_codec = mmioFOURCC ( opt_codec_str[0], opt_codec_str[1], opt_codec_str[2], opt_codec_str[3] );
	mjpeg_info ( "VIDEO: Using codec %s for encoding.\n", opt_codec_str );

	int fd_in = 0;		// stdin

	// check on output filename here since it 
	// stops a lot of avifile spewing.
	// could also check if file exists and 
	// not allow overwrite...could  
	// add a switch to allow overwrite...nah

	if ( outputfile == NULL )
	{
		mjpeg_error_exit1 ( "\nOutput filename IS REQUIRED. Use -o <filename>\n" );
	}

	int width = 0;
	int height = 0;
	double framerate = 0;
	int frame_rate_code = 0;

	y4m_init_frame_info ( &frameinfo );
	y4m_init_stream_info ( &streaminfo );
	if ( y4m_read_stream_header ( fd_in, &streaminfo ) != Y4M_OK )
	{
		mjpeg_error_exit1 ( "Couldn't read YUV4MPEG header!\n" );
	}

	width = ( opt_outputwidth > 0 ) ? opt_outputwidth : y4m_si_get_width ( &streaminfo );
	height = ( opt_outputheight > 0 ) ? opt_outputheight : y4m_si_get_height ( &streaminfo );
	double time_between_frames;

	time_between_frames = 1000000 * ( 1.0 / Y4M_RATIO_DBL ( y4m_si_get_framerate ( &streaminfo ) ) );

	// do the read video file thing from el.
	// get the format information from the el.  Set it up in the destination avi.

	double framespersec = Y4M_RATIO_DBL ( y4m_si_get_framerate ( &streaminfo ) );

	BITMAPINFOHEADER bh;
	memset ( &bh, 0, sizeof ( BITMAPINFOHEADER ) );
	bh.biSize = sizeof ( BITMAPINFOHEADER );
	bh.biPlanes = 1;
	bh.biBitCount = 24;
	bh.biCompression = RIFFINFO_YV12;
	int output_height = height;
	int output_width = width;

	mjpeg_info ("VIDEO: input height: %i\n", height);
	mjpeg_info ("VIDEO: input width: %i\n", width);
	mjpeg_info ("VIDEO: frames per second: %.5f\n", framespersec);

	int origwidth = bh.biWidth = abs ( width );
	int origheight = bh.biHeight = abs ( height );

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

	unsigned char *framebuffer = new unsigned char[bh.biWidth * bh.biHeight * 3];
	CImage imtarget ( ( BitmapInfo * ) & bh, framebuffer, false );

	WAVEFORMATEX format;
	memset ( &format, 0, sizeof ( WAVEFORMATEX ) );
	int audiosampsperframe = 0;

	if ( audioexist > 0 )
	{
		RiffHeader riffheader;
		WaveHeader waveheader;
		DataHeader dataheader;

		el_audio.has_audio = 0;

		fd_audio = fopen ( audiofile, "r" );
		if ( fd_audio == 0 )
		{
			mjpeg_error_exit1 ( "Audio file could not be opened\n" );
		}

		size_t bytesread;
		bytesread = fread ( &riffheader, 1, sizeof ( RiffHeader ), fd_audio );
		if ( bytesread != sizeof ( RiffHeader ) )
		{		//error
			fclose ( fd_audio );
			mjpeg_error_exit1 ( "Audio file too short\n" );
		}

		if ( ( strncmp ( riffheader.riff, "RIFF", 4 ) != 0 ) || ( strncmp ( riffheader.format, "WAVE", 4 ) != 0 ) )
		{		// error, not a wave file.
			// but it might be a editlist file.
			fclose ( fd_audio );

			char *inputfile[1];
			inputfile[0] = audiofile;
			read_video_files ( inputfile, 1, &el_audio, false );

			audioexist = el_audio.has_audio;
			if ( audioexist < 1 )
			{
				mjpeg_error_exit1 ( "No compatible audio found in %s", audiofile );
			}
			format.wFormatTag = 1;
			format.nChannels = el_audio.audio_chans;
			format.nSamplesPerSec = el_audio.audio_rate;
			format.nBlockAlign = ( el_audio.audio_chans * el_audio.audio_bits ) / 8;
			format.nAvgBytesPerSec = el_audio.audio_bits * el_audio.audio_chans * el_audio.audio_bits / 8;
			format.wBitsPerSample = el_audio.audio_bits;
			format.cbSize = sizeof ( WAVEFORMATEX );
			audio_perframe = ( float ) ( ( ( ( double ) format.nSamplesPerSec ) / framespersec )
						     * ( ( double ) ( format.wBitsPerSample / 8 * format.nChannels ) ) );
			// make a guess as to the number of frames based on the number
			// of frames in the audio edit list.
			if ( opt_expectframes < 1 )
			{
				opt_expectframes = el_audio.video_frames;
			}
		}
		else
		{
			bytesread = fread ( &waveheader, 1, sizeof ( WaveHeader ), fd_audio );
			if ( bytesread != sizeof ( WaveHeader ) )
			{	// error
				fclose ( fd_audio );
				mjpeg_error_exit1 ( "No Wave Header in audio file\n" );
			}

			bytesread = fread ( &dataheader, 1, sizeof ( DataHeader ), fd_audio );
			if ( bytesread != sizeof ( DataHeader ) )
			{	// error
				fclose ( fd_audio );
				mjpeg_error_exit1 ( "No Data in audio file\n" );
			}
			mjpeg_info ( "Audio file is %d bytes long.\n", dataheader.length );
			format.wFormatTag = waveheader.que;
			format.nChannels = waveheader.channels;
			format.nSamplesPerSec = waveheader.rate;
			format.nBlockAlign = ( waveheader.channels * waveheader.bits ) / 8;
			format.nAvgBytesPerSec = waveheader.bps;
			format.wBitsPerSample = waveheader.bits;
			format.cbSize = sizeof ( WAVEFORMATEX );

			audio_perframe = ( float ) ( ( ( ( double ) format.nSamplesPerSec ) / framespersec )
						     * ( ( double ) ( format.wBitsPerSample / 8 * format.nChannels ) ) );
			// make a guess at the number of frames based on the number
			// of audio samples in the wave file.
			if ( opt_expectframes < 1 )
			{
				opt_expectframes = ( int ) ( ( ( float ) dataheader.length ) / audio_perframe );
			}
		}
		audio_buffer = ( char * ) malloc ( ( ( int ) audio_perframe ) * 2 );
		memset ( audio_buffer, 0, ( ( int ) audio_perframe ) * 2 );

		mjpeg_info ( "AUDIO: FormatTag = %i\n", format.wFormatTag );
		mjpeg_info ( "AUDIO: Channels = %i\n", format.nChannels );
		mjpeg_info ( "AUDIO: Samples per Sec = %i\n", format.nSamplesPerSec );
		mjpeg_info ( "AUDIO: Block Align = %i\n", format.nBlockAlign );
		mjpeg_info ( "AUDIO: Avg Bytes per Sec = %i\n", format.nAvgBytesPerSec );
		mjpeg_info ( "AUDIO: Bits Per Sample = %i\n", format.wBitsPerSample );
		mjpeg_info ( "AUDIO: cbSize = %i\n", format.cbSize );
		mjpeg_info ( "AUDIO: Avg Bytes Per Frame = %f\n", audio_perframe );
	}

	IAviWriteFile *avifile;

	avifile = CreateIAviWriteFile ( outputfile );

#if AVIFILE_MAJOR_VERSION == 0 && AVIFILE_MINOR_VERSION >= 60
	const CodecInfo *codecInfo = CodecInfo::match ( opt_codec );
	if ( codecInfo == NULL )
	{
		mjpeg_error_exit1 ( "Avifile could not find codec '%s'\n", opt_codec_str );
	}
	Creators::SetCodecAttr ( *codecInfo, "BitRate", opt_divxbitrate );
	Creators::SetCodecAttr ( *codecInfo, "Crispness", opt_crispness );
	Creators::SetCodecAttr ( *codecInfo, "KeyFrames", opt_keyframes );
#else
	IVideoEncoder::SetExtendedAttr ( opt_codec, "BitRate", opt_divxbitrate );
	// not sure how to detect a codec-not-found error in this case.
#endif

	IAviVideoWriteStream *stream = avifile->AddVideoStream ( opt_codec, &bh, ( int ) time_between_frames );

	stream->SetQuality ( 65535 );

	stream->Start (  );

	IAviAudioWriteStream *astream;

	if ( audioexist > 0 )
	{
		if ( opt_mp3bitrate == -1 )
		{
			switch ( format.nSamplesPerSec )
			{
			case 48000:
				opt_mp3bitrate = 80 * format.nChannels;
				break;
			case 44100:
				opt_mp3bitrate = ( ( format.nChannels > 1 ) ? 64 : 56 ) * format.nChannels;
				break;
			case 22050:
				opt_mp3bitrate = 32 * format.nChannels;
				break;
			}
		}
		mjpeg_info ( "AUDIO: MP3 rate: %i kilobits/second, %i Bytes/second\n" 
			, opt_mp3bitrate	   
			, ( (opt_mp3bitrate * 1000) / 8 ) );

		astream = avifile->AddAudioStream ( 0x55, &format, ( opt_mp3bitrate * 1000 ) / 8 );
		astream->Start (  );
	}

	struct timeval tv;
	struct timezone tz = { 0, 0 };
	gettimeofday ( &tv, &tz );
	long startsec = tv.tv_sec;
	long startusec = tv.tv_usec;
	double fps = 0;

	int lineguesstop = 0;
	int lineguessbottom = 0;
	int lineguesstopcount = 0;
	int lineguessbottomcount = 0;
	int colguessleft = 0;
	int colguessright = 0;
	int colguessleftcount = 0;
	int colguessrightcount = 0;

	int yplane = bh.biWidth * bh.biHeight;
	int vplane = yplane / 4;
	int uplane = yplane / 4;

	int asis = 0;
	char progress[128];

	if ( ( opt_x > 0 ) || ( opt_y > 0 ) || ( opt_h > 0 ) || ( opt_w > 0 ) )
	{
		asis = 1;
		mjpeg_info ( "VIDEO: output cropped to: top: %i, left: %i, height: %i, width: %i\n"
			, opt_x, opt_y, opt_h, opt_w );
	}

	long oldtime = 0;

	unsigned char *yuv[3];
	yuv[0] = ( unsigned char * ) malloc ( width * height * sizeof ( unsigned char ) );
	yuv[1] = ( unsigned char * ) malloc ( width * height * sizeof ( unsigned char ) / 4 );
	yuv[2] = ( unsigned char * ) malloc ( width * height * sizeof ( unsigned char ) / 4 );

	int currentframe = 0;

	if ( opt_endframe > 0 )
	{
		opt_expectframes = opt_endframe;
		mjpeg_info ( "Encoding only %i frames.\n", opt_endframe );
	}
	else if ( opt_expectframes > 0 )
	{
		mjpeg_info ( "Expecting %i frames of data.\n", opt_expectframes );
	}

	while ( y4m_read_frame ( fd_in, &streaminfo, &frameinfo, yuv ) == Y4M_OK && ( !got_sigint ) )
	{
		if ( opt_endframe > 0 && currentframe >= opt_endframe )
		{
			break;
		}

		gettimeofday ( &tv, &tz );
		if ( ( tv.tv_sec != oldtime ) && ( !opt_guess ) )
		{
			fps = currentframe / ( ( tv.tv_sec + tv.tv_usec / 1000000.0 ) 
						- ( startsec + startusec / 1000000.0 ) );
			oldtime = tv.tv_sec;
			snprintf ( progress
				, sizeof ( progress )
				, "Encoding frame %i, %.1f frames per second"
				, currentframe
				, fps 
				);
			if ( currentframe < opt_expectframes )
			{
				mjpeg_info ( "%s, %.1f seconds left.   \r"
					, progress
					, ( ( double ) ( opt_expectframes - currentframe ) ) / fps );
			}
			else
			{
				mjpeg_info ( "%s.                      \r", progress );
			}
			fflush ( stderr );
		}

		// CLIPPING
		//
		// If there's no clipping, use a faster way of copying the frame data.
		//
		if ( asis < 1 )	//faster
		{
			memcpy ( framebuffer, yuv[0], yplane );
			memcpy ( framebuffer + yplane, yuv[2], vplane );
			memcpy ( framebuffer + yplane + uplane, yuv[1], uplane );
		}
		else		// slower, but handles cropping.
		{
			int chromaw = opt_w / 2;
			int chromax = opt_x / 2;
			int chromawidth = output_width / 2;
			for ( int yy = 0; yy < opt_h; yy++ )
			{
				int chromaoffset = ( yy / 2 ) * chromaw;
				int chromaoffsetsrc = ( ( yy + opt_y ) / 2 ) * chromawidth;
				memcpy ( framebuffer + yy * opt_w
					, yuv[0] + ( yy + opt_y ) * output_width + opt_x
					, opt_w );
				memcpy ( framebuffer + yplane + chromaoffset
					, yuv[2] + chromaoffsetsrc + chromax, chromaw );
				memcpy ( framebuffer + yplane + vplane + chromaoffset
					, yuv[1] + chromaoffsetsrc + chromax
					, chromaw );
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
		else
		{
			stream->AddFrame ( &imtarget );
			if ( audioexist )
			{
				if ( el_audio.has_audio )	// edit-list audio is easy.
				{
					audio_bytes = el_get_audio_data ( audio_buffer, currentframe, &el_audio, 0 );
				}
				else	// wave audio is tougher.
				{
					// get next audio buffer.
					audio_thisframe = audio_perframe + audio_shouldhaveread - ( float ) audio_totalbytesread;
					audio_bytesthisframe = ( int ) audio_thisframe;
					if ( audio_thisframe <= audio_perframe )
					{
						audio_bytesthisframe +=
							( audio_bytesthisframe % format.nBlockAlign ) 
							? ( format.nBlockAlign -  ( audio_bytesthisframe % format.nBlockAlign ) ) 
							: 0;
					}
					else
					{
						audio_bytesthisframe -= ( audio_bytesthisframe % format.nBlockAlign );
					}

					audio_bytes = fread ( audio_buffer, 1, audio_bytesthisframe, fd_audio );
					if ( audio_bytesthisframe != audio_bytes )
					{
						mjpeg_info ( "\nShort audio read. %i\n", audio_bytes );
					}
					audio_totalbytesread += audio_bytes;
					audio_shouldhaveread += audio_perframe;
				}
				astream->AddData ( audio_buffer, audio_bytes );
			}
		}

		currentframe++;
	}

	stream->Stop (  );
	delete avifile;
	free ( yuv[0] );
	free ( yuv[1] );
	free ( yuv[2] );
	mjpeg_info ( "\n" );
	mjpeg_info ( "Done.\n" );

	y4m_fini_frame_info ( &frameinfo );
	y4m_fini_stream_info ( &streaminfo );

	if ( audioexist > 0 )
	{
		free ( audio_buffer );
		if ( !el_audio.has_audio )
		{
			fclose ( fd_audio );
			mjpeg_info ( "Expected to read %i bytes of audio data, and read %i.\n"
				, ( int ) audio_shouldhaveread
				, audio_totalbytesread );
		}
	}

	if ( opt_guess )
	{
		int avgtop = lineguesstop / ( max ( lineguesstopcount, 1 ) );
		int avgbottom = lineguessbottom / ( max ( lineguessbottomcount, 1 ) );
		int avgleft = colguessleft / ( max ( colguessleftcount, 1 ) );
		int avgright = colguessright / ( max ( colguessrightcount, 1 ) );

		avgright = (avgright==0) ? bh.biWidth : avgright;
		avgbottom = (avgbottom==0) ? bh.biHeight : avgbottom;
		mjpeg_info ( "avg top border: %i\n", avgtop );
		mjpeg_info ( "avg bottom border: %i\n", avgbottom );
		mjpeg_info ( "avg left border: %i\n", avgleft );
		mjpeg_info ( "avg right border: %i\n", avgright );

		// cropping window only like multiples of two in each direction.
		avgtop += avgtop % 4;
		avgbottom -= avgbottom % 4;
		avgleft += avgleft % 4;
		avgright -= avgright % 4;
		mjpeg_info ( "suggested options: -c %dx%d+%d+%d\n"
			, ( avgright - avgleft )
			, ( avgbottom - avgtop )
			, avgleft
			, avgtop );
	}
}
