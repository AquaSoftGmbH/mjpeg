// yuv2divx
//
// A utility for converting YUV streams (YUV4MPEG) into DIVX files.  It
// was developed against the lav2yuv and yuv4mpeg utilities from the 
// MJPEG lavtools project.  (mjpeg.sourceforge.net).  It also leans
// heavily on the x2divx project of Ulrich Hecht.  As with both of these
// other projects, this utility is licensed under the GPL v2.
//
// (c) 2001/07/13 Shawn Sulma <lavtools@athos.cx>
//	based on very helpful work by the lavtools, mjpeg.sourceforge.net
//	and x2divx (Ulrich Hecht et al, www.emulinks.de/divx).
//
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version.
//
//	This program is distributed in the hope that it will be useful,
//	but WITHOUT ANY WARRANTY; without even the implied warranty of
//	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//	GNU General Public License for more details.
//
//	You should have received a copy of the GNU General Public License
//	along with this program; if not, write to the Free Software
//	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
//	USA
//
//
// NOTES: the encoding to divx is hardcoded, but there's nothing special
// about it so in theory, you could have it encode to any available avifile
// format.
//
// This utility is a mix of C (from lavtools) and C++ (from x2divx).  I'm not
// a guru at either, but it works for me and is very handy.
//
// One desired additional feature would be YUV scaling (as opposed to
// decimation). This should be added RSN.
//
// This is very early code.  Expect unusual things, naive assumptions, odd
// behaviour, and just plain Wrongness.  Helpful, constructive criticism is
// encouraged.  Flames... well... you know where you can stick them.
//
#define APPNAME "yuv2divx"
#define APPVERSION "0.0.13"
#define LastChanged "2001/07/13"

#include <iostream.h>
#include <videoencoder.h>
#include <avifile.h>
#include <aviplay.h>
#include <avifile/version.h>
#if AVIFILE_MAJOR_VERSION == 0 && AVIFILE_MINOR_VERSION < 50
#include <image.h>
#include <fourcc.h>
#else
#include <aviutil.h>
#endif
#include <avifile/except.h>
#include <math.h>		// M_PI is better than included PI constant
#include <sys/time.h>
#include <unistd.h>		// Needed for the access call to check if file exists
#include <getopt.h>		// getopt
#include <stdint.h>		// standard integer types
#include <stdlib.h>		// standard library with integer division
#define __MODULE__ APPNAME	// Needed for avifile exceptions
#include <stdio.h>
#if AVIFILE_MAJOR_VERSION == 0 && AVIFILE_MINOR_VERSION < 50
#include <creators.h>
#endif
#include <config.h>
#include <fcntl.h>
#include <string.h>

extern "C" 
{
#include "yuv4mpeg.h"
#include "mjpeg_logging.h"
}

typedef struct {
	char riff[4] ;	// always "RIFF"
	int  length ;	// Length (little endian)
	char format[4] ; // fourcc with the format.  We only want WAVE
} RiffHeader ;

typedef struct {
	char  fmt[4] ;	// "fmt_" (ASCII characters)
	int   length ;	// chunk length
	short que ;	// ??? - always 1
	short channels;	// channels
	int   rate ;	// samples per second
	int   bps ;	// bytes per second
	short bytes ;	// bytes per sample:	1 = 8 bit Mono, 
			//			2 = 16 bit Mono/8 bit stereo
			//			4 = 16 bit Stereo
	short bits ;	// bits per sample
} WaveHeader ;

/* The DATA chunk */

typedef struct {
	char data[4] ;	// chunk type ("DATA")
	int  length ;	// length
} DataHeader ;

int got_sigint = 0;

// From yuvplay
static void sigint_handler (int signal)
{
	mjpeg_log (LOG_WARN, "Caugth SIGINT, exiting...\n");
	got_sigint = 1;
}

void error(char *text)
{
	mjpeg_error(text);
}

static void
print_usage (void)
{
  printf ("\nUsage: %s [OPTION]... [input AVI]... -o [output AVI]\n",
	  APPNAME);
  exit (0);
}

static void
print_help (void)
{
  printf ("\nUsage: %s [OPTION]... [input AVI]... -o [output AVI]\n",
	  APPNAME);
  printf ("\nOptions:\n");
  printf ("  -b --divxbitrate\tDivX ;-) bitrate (default 1800)\n");
  printf ("  -f --fast_motion\tuse fast-motion codec (default low-motion)\n");
  printf ("  -a --mp3bitrate\tMP3 bitrate (kBit, default auto)\n");
  printf ("  -e --endframe\t\tnumber of frames to encode (default all)\n");
  printf ("  -c --crop\t\tcrop output to \"wxh+x+y\" (default full frame)\n");
//  printf ("  -l --blur\t\tradius for blur filter (default off)\n");
  printf ("  -w --width\t\toutput width (use when not available in stream)\n");
  printf ("  -h --height\t\toutput height (use when not available in stream)\n");
  printf ("  -o --outputfile\tOutput filename IS REQUIRED\n");
  printf ("  -v --version\t\tVersion and license.\n");
  printf
    ("  -s --forcedaudiorate\taudio sample rate of input file (Hz);\n\t\t\tuse only if avifile gets it wrong\n");
  printf ("  -d --droplsb\t\tdrop x least significant bits (0..3, default 0)\n");
  printf ("  -n --noise\t\tnoise filter (0..2, default 0)\n");
  printf ("  -g --guess\t\tguess values for -c and -z options\n");
#if AVIFILE_MAJOR_VERSION == 0 && AVIFILE_MINOR_VERSION < 50
  printf ("  -k --keyframes\tset keyframes attribute (default 15)\n");
  printf ("  -C --crispness\tset crispness attribute (default 20)\n");
  printf ("  -A --audio\t\tspecify audio file\n");
#endif
//  printf
//    ("  -u --flip_video\tflip picture (use if output video stream is upside-down,\n\t\t\tdefault off)\n");
  printf ("     --help\t\tPrint this help list.\n");
  cout << "\n" << APPNAME << " version " << APPVERSION << " by Ulrich Hecht <uli@emulinks.de>\n" ;
  printf ("Last changed on " LastChanged "\n");
  exit (0);
}

static void
display_license (void)
{
  printf ("\nThis is %s version %s \n", APPNAME, APPVERSION);
  printf ("%s", "Copyright (C) Shawn Sulma <lavtools@athos.cx> \n\
Based on code from Ulricht Hecht and the MJPEG Square. \n\
This program is distributed in the hope that it will be useful,\n\
but WITHOUT ANY WARRANTY; without even the implied warranty of\n\
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n\
GNU General Public License for more details.\n");
  exit (0);
}

int
main (int argc, char **argv)
{

  if (GetAvifileVersion () != AVIFILE_VERSION)
    {
      cout << "This binary was compiled for Avifile ver. " << AVIFILE_VERSION
	<< ", but the library is ver. " << GetAvifileVersion () <<
	". Aborting." << endl;
      return 0;
    }

   (void)mjpeg_default_handler_verbosity(1);

  int copt;			// getopt switch variable

  char *outputfile = NULL;	// gcc howled at some point so I put it here
  char *audiofile = NULL;
  int opt_divxbitrate = 1800;
  int opt_mp3bitrate = -1;
  int opt_codec = fccDIV3;
  int opt_endframe = -1;
//  int opt_blur = 0;
  int opt_outputwidth = 0;
  int opt_outputheight = 0;
  int opt_forcedaudiorate = 0;
  int opt_guess = 0;
  int flip = 1;

  int opt_x = 0;
  int opt_y = 0;
  int opt_w = 0;
  int opt_h = 0;

  FILE* fd_audio;
  char* audio_buffer;
  int audioexist=0;
  int audio_bytesperframe;
  int audio_bytes;

#if AVIFILE_MAJOR_VERSION == 0 && AVIFILE_MINOR_VERSION < 50
  int opt_keyframes=15;
  int opt_crispness=20;
#endif
  char* arg_geom;
  char* arg_end;
//#endif

   /* for the new YUV4MPEG streaming */
   y4m_frame_info_t *frameinfo = NULL;
   y4m_stream_info_t *streaminfo = NULL;

//  static char x1, x2;
//  static unsigned int NewY1, NewY2, NewX1, NewX2, Size, x, y, c;
//  static double ScaleX, ScaleY, t1, t2, t3, t4, f, ft, NewX;

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

  while (1)
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
//	{"blur", required_argument, NULL, 'l'},
	{"forcedaudiorate", required_argument, NULL, 's'},
	{"width", required_argument, NULL, 'w'},
	{"height", required_argument, NULL, 'h'},
	{"audio", required_argument, NULL, 'A'},
	{"video", required_argument, NULL, 'V'},
	{"number_cpus", required_argument, NULL, 'n'},
	{"outputfile", required_argument, NULL, 'o'},
	{"droplsb", required_argument, NULL, 'd'},
	{"noise", required_argument, NULL, 'n'},
#if AVIFILE_MAJOR_VERSION == 0 && AVIFILE_MINOR_VERSION < 50
        {"keyframes", required_argument, NULL, 'k'},
        {"crispness", required_argument, NULL, 'C'},
#endif
//	{"flip_video", no_argument, NULL, 'u'},
	{"guess", no_argument, NULL, 'g'},
	{0, 0, 0, 0}
      };

// w,h removed as yuv scaling is a real pain.
// b removed as it doesn't seem to work properly with this input.
      copt =
#if AVIFILE_MAJOR_VERSION == 0 && AVIFILE_MINOR_VERSION < 50
	getopt_long (argc, argv, "A:fa:w:h:e:c:b:o:s:n:d:gvk:C:", long_options,
		     &option_index);
#else
	getopt_long (argc, argv, "A:fa:w:h:e:c:b:o:s:n:d:gv", long_options,
		     &option_index);
#endif
      if (copt == -1)
	break;

      switch (copt)
	{
	case 0:
	  if (long_options[option_index].name == "help")
	    print_help ();
	  break;

	case 'b':
	  opt_divxbitrate = atoi (optarg);
	  break;

	case 'f':
	  opt_codec = fccDIV4;
	  break;

	case 'a':
	  opt_mp3bitrate = atoi (optarg);
	  break;

	case 'e':
	  opt_endframe = atoi (optarg);
	  break;

//	case 'l':
//	  opt_blur = atoi (optarg);
//	  break;

	case 's':
	  opt_forcedaudiorate = atoi (optarg);
	  break;

	case 'w':
	  opt_outputwidth = atoi (optarg);
	  break;

	case 'h':
	  opt_outputheight = atoi (optarg);
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

#if AVIFILE_MAJOR_VERSION == 0 && AVIFILE_MINOR_VERSION < 50
	case 'k':
	  opt_keyframes = atoi(optarg);
	  break;
	  
	case 'C':
	  opt_crispness = atoi(optarg);
	  break;
#endif

	case 'c': // crop
		arg_geom = optarg;
		opt_w = strtol(arg_geom, &arg_end, 10);
		if (*arg_end != 'x' || opt_w < 100) 
		{
                        mjpeg_error_exit1("Bad width parameter\n");
			// nerr++;
			break;
		}

		arg_geom = arg_end + 1;
		opt_h = strtol(arg_geom, &arg_end, 10);
		if ((*arg_end != '+' && *arg_end != '\0') || opt_h < 100)
		{
			mjpeg_error_exit1( "Bad height parameter\n");
			// nerr++;
			break;
		}
		if (*arg_end == '\0')
		{
			break;
		}

		arg_geom = arg_end + 1;
		opt_x = strtol(arg_geom, &arg_end, 10);
		if ((*arg_end != '+' && *arg_end != '\0') || opt_x > 720) 
		{
			mjpeg_error_exit1( "Bad x parameter\n");
			// nerr++;
			break;
		}

		arg_geom = arg_end + 1;
		opt_y = strtol(arg_geom, &arg_end, 10);
		if (*arg_end != '\0' || opt_y > 240) 
		{
			mjpeg_error_exit1( "Bad y parameter\n");
			// nerr++;
			break;
		}
		break;

//	case 'u':
//	  flip = 0;
//	  break;

//	case 'd': // drop-lsb
//		param_drop_lsb = atoi(optarg);
//		if (param_drop_lsb < 0 || param_drop_lsb > 3) 
//		{
//			mjpeg_error_exit1( "-d option requires arg 0..3\n");
//			//nerr++;
//		}
//		break;

//	case 'n':
//		param_noise_filt = atoi(optarg);
//		if (param_noise_filt < 0 || param_noise_filt > 2) 
//		{
//			mjpeg_error_exit1( "-n option requires arg 0..2\n");
//			//nerr++;
//		}
//		break;

	case 'V':
	  display_license ();
	  break;

	case '?':
	  print_usage ();
	  break;

	case ':':
	  printf ("You missed giving something an argument.\n");	// since we have non-options
	  break;							// never gets called
	}
    }

	int fd_in = 0; // stdin

	// check on output filename here since it 
	// stops a lot of avifile spewing.
	// could also check if file exists and 
	// not allow overwrite...could  
	// add a switch to allow overwrite...nah

	if (outputfile == NULL)
  	{
		printf ("\nOutput filename IS REQUIRED use -o filename\n");
		exit (1);
	}

	int width = 0;
	int height = 0;
	double framerate = 0;
	int frame_rate_code = 0;

	streaminfo = y4m_init_stream_info(NULL);
	if (y4m_read_stream_header(fd_in, streaminfo) != Y4M_OK)
	{
		mjpeg_error_exit1("Couldn't read YUV4MPEG header!\n");
	}
	frameinfo = y4m_init_frame_info(NULL);

	width = ( opt_outputwidth > 0) ? opt_outputwidth : streaminfo->width ;
	height = ( opt_outputheight > 0) ? opt_outputheight : streaminfo->height ;
	double time_between_frames ;

	time_between_frames = 1000000 * (1.0 / streaminfo->framerate);

	// do the read video file thing from el.
	// get the format information from the el.  Set it up in the destination avi.

	double framespersec = framerate;

	BITMAPINFOHEADER bh;
	memset (&bh, 0, sizeof(BITMAPINFOHEADER));
	bh.biSize = sizeof(BITMAPINFOHEADER);
	bh.biPlanes = 1;
	bh.biBitCount = 24;
	bh.biCompression = fccYV12;
	int output_height = height;
	int output_width = width;
	
	int origwidth = bh.biWidth = abs (width);
	int origheight = bh.biHeight = abs (height);

/* SCALING
	if (opt_outputheight || opt_outputwidth)
	{
		if (opt_outputheight)
		{
			bh.biHeight = opt_outputheight;
		}
		if (owidth)
		{
			bh.biWidth = opt_outputwidth;
		}
	}
*/

	if (opt_h > 0)
	{
		opt_h = min (opt_h, (bh.biHeight - opt_y));
		bh.biHeight = opt_h;
	}
	if (opt_w > 0)
	{
		opt_w = min (opt_w, (bh.biWidth - opt_x));
		bh.biWidth = opt_w;
	}

	bh.biSizeImage = bh.biWidth * bh.biHeight * 3;

	unsigned char *framebuffer = new unsigned char[bh.biWidth * bh.biHeight * 3];
	CImage imtarget ((BitmapInfo *) & bh, framebuffer, false);

	RiffHeader	riffheader;
	WaveHeader	waveheader;
	DataHeader	dataheader;

	if (audioexist>0)
	{

		fd_audio = fopen(audiofile, "r");
		if (fd_audio == 0)
		{
			mjpeg_error_exit1("Audio file could not be opened\n");
		}

		size_t	bytesread;
		bytesread = fread(&riffheader, 1, sizeof(RiffHeader), fd_audio);
		if (bytesread != sizeof(RiffHeader))
		{ 	//error
			mjpeg_error_exit1("Audio file too short\n");
		}
	
		if (  (strncmp(riffheader.riff, "RIFF", 4) != 0)
		   || (strncmp(riffheader.format, "WAVE", 4) != 0)
		   )
		{	// error
			mjpeg_error_exit1("Audio file invalid\n");
		}

		bytesread = fread(&waveheader, 1, sizeof(WaveHeader), fd_audio);	
		if (bytesread != sizeof (WaveHeader))
		{ 	// error
			mjpeg_error_exit1("No Wave Header in audio file\n");
		}
	
		bytesread = fread(&dataheader, 1, sizeof(DataHeader), fd_audio);
		if (bytesread != sizeof (DataHeader))
		{ 	// error
			mjpeg_error_exit1("No Data in audio file\n");
		}
	
		fprintf(stderr, "Audio file is %d bytes long.\n", dataheader.length) ;
	}

	WAVEFORMATEX format;
	memset(&format, 0, sizeof(WAVEFORMATEX));
	int audiosampsperframe = 0;

	if (audioexist > 0)
	{	
		format.wFormatTag = mmioFOURCC( waveheader.fmt[0]
					, waveheader.fmt[1]
					, waveheader.fmt[2]
					, waveheader.fmt[3]
					);
		format.nChannels = waveheader.channels;
		format.nSamplesPerSec = waveheader.rate;
		format.nBlockAlign = (waveheader.channels * waveheader.bits) / 8;
		format.nAvgBytesPerSec = waveheader.bps;
		format.wBitsPerSample = waveheader.bits;
		format.cbSize = sizeof(WAVEFORMATEX);


		audio_bytesperframe = (int) ( ( ((double) waveheader.rate) / framespersec) * ((double) waveheader.bits/8 * waveheader.channels) );
		fprintf(stderr, "ABF = %i\n", audio_bytesperframe);
		audio_buffer = (char*) malloc(audio_bytesperframe);
	}

	int fccHandler = opt_codec;
	IAviWriteFile *avifile;

	avifile = CreateIAviWriteFile (outputfile);

#if AVIFILE_MAJOR_VERSION == 0 && AVIFILE_MINOR_VERSION < 50
	const CodecInfo* codecInfo = CodecInfo::match(opt_codec);
	Creators::SetCodecAttr(*codecInfo, "BitRate", opt_divxbitrate);
	Creators::SetCodecAttr(*codecInfo, "Crispness", opt_crispness);
	Creators::SetCodecAttr(*codecInfo, "KeyFrames", opt_keyframes);
#else
	IVideoEncoder::SetExtendedAttr (opt_codec, "BitRate", opt_divxbitrate);
#endif

  	IAviVideoWriteStream *stream = avifile->AddVideoStream (
				fccHandler
				, &bh
				, (int)time_between_frames
				);

	stream->SetQuality (8000);

	stream->Start ();

	IAviAudioWriteStream *astream;

	if (audioexist > 0)
	{
		if (opt_mp3bitrate = -1)
		{
			switch (waveheader.rate)
			{
			case 48000:
				opt_mp3bitrate = 80 * waveheader.channels;
				break;
			case 44100:
				opt_mp3bitrate = ((waveheader.channels>1)?64:56) * waveheader.channels;
				break;
			case 22050:
				opt_mp3bitrate = 32 * waveheader.channels;
				break;
			}
		}

		astream = avifile->AddAudioStream (0x55, &format, (opt_mp3bitrate * 1000)/8);
		astream->Start();
	}	

	struct timeval tv;
	struct timezone tz = { 0, 0 };
	gettimeofday (&tv, &tz);
	long startsec = tv.tv_sec;
	long startusec = tv.tv_usec;
	double fps = 0;

	int lineguesstop = 0;
	int lineguessbottom = 0;
	int lineguesstopcount = 0;
	int lineguessbottomcount = 0;

//	init(); // initialize lav2yuv code.

	int yplane = bh.biWidth * bh.biHeight;
	int uplane = yplane / 4;
	int vplane = yplane / 4;

	int asis = 0;
	
	if ((opt_x > 0) || (opt_y > 0) || (opt_h > 0) || (opt_w > 0))
	{
		asis = 1;
		printf(" window (%i, %i) height: %i width: %i)\n", opt_x, opt_y, opt_h, opt_w);
	}

	long oldtime = 0;

	unsigned char* yuv[3];
	yuv[0] = (unsigned char*) malloc(width * height * sizeof(unsigned char));
	yuv[1] = (unsigned char*) malloc(width * height * sizeof(unsigned char) / 4);
	yuv[2] = (unsigned char*) malloc(width * height * sizeof(unsigned char) / 4);

	int currentframe = 0;

	while (y4m_read_frame(fd_in, streaminfo, frameinfo, yuv)==Y4M_OK && (!got_sigint)) 
	{
		if (opt_endframe > 0 && currentframe >= opt_endframe)
		{
			goto finished;
		}

		gettimeofday (&tv, &tz);
		if ((tv.tv_sec != oldtime) && (! opt_guess) )
		{
			fps = currentframe 
				/ ( (tv.tv_sec + tv.tv_usec / 1000000.0) 
				    - (startsec + startusec / 1000000.0)
				  );
			oldtime = tv.tv_sec;
			printf ("\rEncoding frame %i, %.1f frames per sec.    "
				, currentframe, fps);
			fflush (stdout);
		}

		// CLIPPING
		//
		// If there's no clipping, use a faster way of copying the frame data.
		//
		if (asis < 1)  //faster
		{
			memcpy (framebuffer, yuv[0], yplane);
			memcpy (framebuffer + yplane, yuv[1], uplane);
			memcpy (framebuffer + yplane + uplane, yuv[2], vplane);
		}
		else	// slower, but handles cropping.
		{
			int chromaw = opt_w / 2;
			int chromax = opt_x / 2;
			int chromawidth = output_width/2;
			for (int yy = 0; yy < opt_h; yy++)
			{
				int chromaoffset = (yy/2) * chromaw;
				int chromaoffsetsrc = ((yy+opt_y)/2) * chromawidth;
				memcpy ( framebuffer+yy*opt_w
					, yuv[0] + (yy+opt_y) * output_width + opt_x
					, opt_w
					);
				memcpy ( framebuffer + yplane + chromaoffset
					, yuv[1] + chromaoffsetsrc + chromax
					, chromaw
					);
				memcpy ( framebuffer + yplane + uplane + chromaoffset
					, yuv[2] + chromaoffsetsrc + chromax
					, chromaw
					);
			}
		}


/* SCALING - this is the original avi2divx scaling, which works with RGB.
 * expect it to be replaced sometime soon.
		else // if scaled.
		{
			ScaleX = (float) bh.biWidth / origwidth;
			ScaleY = (float) bh.biHeight / origheight;

			for (y = 0; abs ((int)y) < abs (bh.biHeight); y++)	//obscure gcc error without the abs jpc
			{
				NewY1 = (unsigned int) (y / ScaleY + cliplines - clipoffset) * origwidth * 3;
				for (x = 0; abs ((int)x) < abs (bh.biWidth); x++)	//obscure gcc error without the abs jpc
				{
					t1 = x / (double) bh.biWidth;
					t4 = t1 * bh.biWidth;
					t2 = t4 - (unsigned int) (t4);
					ft = t2 * M_PI;
					f = (1.0 - cos (ft)) * .5;
					NewX1 = ((unsigned int) (t4 / ScaleX)) * 3;
					NewX2 = ((unsigned int) (t4 / ScaleX) + 1) * 3;

					Size = y * bh.biWidth * 3 + x * 3;
					for (c = 0; c < 3; c++)
					{
						x1 = frame_buf[NewY1 + NewX1][c];
						x2 = frame_buf[NewY1 + NewX2][c];
						framebuffer[Size + c] = (unsigned char) ((1.0 - f) * x1 + f * x2);
					}
				}
			}
		}
*/
		if (opt_guess)
		{
			double totallum;
			double oldlum = 0;
	      		for (int y = 0; y < bh.biHeight; y++)
			{
				totallum = 0;
				for (int x = 0; x < bh.biWidth; x++)
				{
					totallum += *(framebuffer + y * bh.biWidth + x);
				}
				totallum /= 3 * bh.biWidth;
				//printf("line %d: totallum: %lf, oldlum: %lf\n",y,totallum,oldlum);
				if (totallum > oldlum + 30)
				{
					printf ("top line: %d\n", y);
					lineguesstop += y;
					lineguesstopcount++;
					break;
				}
				oldlum = totallum;
			}
			for (int y = bh.biHeight-1 ; y > 0; y--)
			{
				totallum = 0;
				for (int x = 0; x < bh.biWidth ; x++)
				{
					totallum += *(framebuffer + y * bh.biWidth + x);
				}
				totallum /= 3 * bh.biWidth;
				if (totallum > oldlum + 30)
				{
					printf ("bottom line: %d\n", y);
					lineguessbottom += y;
					lineguessbottomcount++;
					break;
				}
				oldlum = totallum;
			}
			continue;
		}

		stream->AddFrame (&imtarget);
		if (audioexist)
		{
			audio_bytes = fread (audio_buffer, 1, audio_bytesperframe, fd_audio);
			// get next audio buffer.
			astream->AddData(audio_buffer, audio_bytes);
		}

		currentframe++;
	}

finished:
	stream->Stop ();
	delete avifile;
	free (yuv[0]) ;
	free (yuv[1]) ;
	free (yuv[2]) ;

	y4m_free_frame_info(frameinfo);
	y4m_free_stream_info(streaminfo);

	if (audioexist > 0)
	{
		free (audio_buffer);
		fclose (fd_audio);
	}

	if (opt_guess)
	{
		int avgtop = lineguesstop / lineguesstopcount;
		int avgbottom = lineguessbottom / lineguessbottomcount;
		printf ("avg line top: %f\n", (double) lineguesstop / lineguesstopcount);
		printf ("avg line bottom: %f\n", (double) lineguessbottom / lineguessbottomcount);
		printf ("suggested options: -c %dx%d+0+%d\n", width, (avgbottom - avgtop), avgtop);
	}
}
