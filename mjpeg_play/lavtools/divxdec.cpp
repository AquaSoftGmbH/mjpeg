// divxdec
//
// A utility for converting an AVI file (with an available avifile
// compatible codec) into separate YUV4MPEG (video) and WAVE (audio)
// files.  It was developed against the lav2yuv and yuv4mpeg utilities 
// from the MJPEG lavtools project.  (mjpeg.sourceforge.net).  It also leans
// heavily on the x2divx project of Ulrich Hecht.  As with both of these
// other projects, this utility is licensed under the GPL v2.
//
// (c) 2001/11/01 Shawn Sulma <lavtools@athos.cx>
//      based on very helpful work by the lavtools, mjpeg.sourceforge.net
//      and x2divx (Ulrich Hecht et al, www.emulinks.de/divx), which gave me
//      an idea of how avifile actually worked.
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
// This utility is a mix of C (from lavtools) and C++ (from x2divx) -- plus
// my own additions, continuing the mixture.  I'm not a guru at either, but 
// it works for me and is very handy.
//
// This is very early code.  Expect unusual things, naive assumptions, odd
// behaviour, and just plain Wrongness.  Helpful, constructive criticism is
// encouraged.  Flames... well... you know where you can stick them.
//
// 2001/11/04 Changes
//
// added playback through lavplay.  This required a *bit* of a reorganization of
// the code.
//
// 2001/11/06 Oversized handling, new options
//
// A new default method for handling inputs that cannot be played via hardware 
// due to their height.  The default is now to decimate the image (factor of 2)
// If you want to force output to be interlaced instead, use the -I option.
// Decimation is used by default as the quality is similar, but makes much more
// efficient use of CPU -- since the process of decode->encode->play is
// already very intensive.
//
// Added the "-r" option, to allow input to be 'rationalised'.  This constrains
// luma values to the 'legal' range of 16-240.  It will help situations where
// very black (or very white) regions of playback have odd colours.  It does come at
// a CPU penalty, though and is disabled by default.
//
// 2001/11/25
//
// Added the -b/--beginframe option to set the starting frame for processing.
// This currently doesn't work for audio-only extraction.
// Added the -m/--maxfilesize option to set the maximum file size (in MB) for LAV
// files.
//
// 2002/01/11
//
// Added some #ifdef blocks that allows use of the old, obsolete
// avifile-0.53. It is generally recommended that you use avifile 0.6
// instead whenever possible. Remaining support for 0.53 could disappear at
// any time.
//
// Addded code to redirect avifile log messages erroneously sent to stdout
// to stderr where they should be.  This doesn't always work with avifile
// 0.53.
//
// Discovered that convertPacked422 was badly damaged.  It's rewritten now
// and actually works.  Also removed performance hack that worked on my
// system, but not on some others with different versions of win32 codecs in
// avifile.
//
#define APPNAME "divxdec"
#define APPVERSION "0.0.25"
#define LastChanged "2002/01/11"
//#define DEBUG_DIVXDEC 1

#include <vector>

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
#include <signal.h>

// avifile includes.
#include <videoencoder.h>
#include <avifile.h>
#include <aviplay.h>
#include <avifile/version.h>
#include <avifile/except.h>
#if AVIFILE_MAJOR_VERSION == 0 && AVIFILE_MINOR_VERSION < 50
#include <image.h>
#include <fourcc.h>
#include <creators.h>
#else
#include <aviutil.h>
typedef unsigned int framepos_t;
#endif
#define __MODULE__ APPNAME	// Needed for avifile exceptions

extern "C"
{
#include "yuv4mpeg.h"
#include "mjpeg_logging.h"
#include "lav2wav.h"	// for wave structs, etc.
#include "jpegutils.h"
#include "liblavplay.h"
}

#include "riffinfo.h"

#ifndef min
#define min(a,b) (a<b)?(a):(b)
#endif

#ifndef max
#define max(a,b) (a>b)?(a):(b)
#endif

#define FD_STDOUT 1
#define NORM_NTSC 'n'
#define NORM_PAL 'p'

static int interrupted = 0;
static int errCode = 0;

// From lav2wav
/*
  Raw write does *not* guarantee to write the entire buffer load if it
  becomes possible to do so.  This does...
 */
static 
size_t doWrite( int fd, void *buf, size_t count )
{
	char *cbuf;
	cbuf = (char*) buf;
	size_t count_left = count;
	size_t written;
	while( count_left > 0 )
	{
		written = write( fd, cbuf, count_left );
		if( written < 0 )
		{
			return count-count_left;
		}
		count_left -= written;
		cbuf += written;
	}
	return count;
}

static int 
writeWaveHeader( wave_header wave
		, unsigned int bits
		, unsigned int rate
		, unsigned int channels
		, int audiobytes
		, int fd )
{
	// off_t dummy_size = 0x7fffff00+sizeof(wave);

	memset(&wave, 0, sizeof(wave));

	strncpy ( (char *) wave.riff.id, "RIFF", 4 );
	strncpy ( (char *) wave.riff.wave_id, "WAVE",4 );
	strncpy ( (char *) wave.format.id, "fmt ",4 );

	wave.format.len = sizeof(struct common_struct);

	/* Store information */
	wave.common.wFormatTag = WAVE_FORMAT_PCM;
	wave.common.wChannels = channels;
	wave.common.dwSamplesPerSec = rate;
	wave.common.dwAvgBytesPerSec = channels*rate*bits/8;
	wave.common.wBlockAlign = channels*bits/8;
	wave.common.wBitsPerSample = bits;

	wave.data.len = audiobytes ;
	wave.riff.len = audiobytes + wave.format.len + sizeof (struct chunk_struct) * 2;

	strncpy ( (char *) wave.data.id, "data", 4 );

	if (doWrite(fd, &wave, sizeof(wave)) != sizeof(wave)) 
	{
		return 1;
	}
	return 0;
}

static unsigned int
readyDestination ( 	IAviReadStream *instream
		, int flip, int32_t compression )
{
	IVideoDecoder::CAPS caps = instream->GetDecoder ()->GetCapabilities ();
	unsigned int fmt = 0;

	// choose the output format.  This way we can usually get the
	// codec to do most of the work for us, saving time and memory.
	// however, we need to know what the output format will be
	// in order to set it up properly.  Some are easier than others.
	if ( caps & IVideoDecoder::CAP_IYUV )
	{
		fmt = RIFFINFO_IYUV;
	}
// only in avifile-0.6
#if AVIFILE_MAJOR_VERSION == 0 && AVIFILE_MINOR_VERSION < 50
	else if ( caps & IVideoDecoder::CAP_I420 )
	{
		fmt = RIFFINFO_I420;
	}
#endif
	else if ( caps & IVideoDecoder::CAP_YV12 )
	{
		fmt = RIFFINFO_YV12;
	}
	else if ( caps & IVideoDecoder::CAP_YUY2 )
	{
		fmt = RIFFINFO_YUY2;
	}
	else if ( caps & IVideoDecoder::CAP_UYVY )
	{
		fmt = RIFFINFO_UYVY;
	}
	else if ( caps & IVideoDecoder::CAP_YVYU )
	{
		fmt = RIFFINFO_YVYU;
	}
	else 
	{
		mjpeg_error_exit1 ( "No YUV output capabilities." );
	}

	if ( instream->GetDecoder ()->SetDestFmt(16, fmt) < 0 )
	{
		mjpeg_warn ( "Decoder didn't like dest format\n");
	}
	instream->GetDecoder ()->SetDirection ( flip );
	return fmt;
}

/****
 * This function should convert any 4:2:2 packed YUV format
 * to the 4:2:0 YUV format liked by lavtools.
 *
 * Parameters:
 *	source - source buffer with 8 bit 4:2:2 YUV packed data in.
 *	dest[3] - the destination YUV planes
 *	height, width - height and width of the image represented by source
 *  lumaOffset - the position (0-3) of the first luma value in a 4 byte sequence.
 *  uOffset - the position (0-3) of the "u" component in a 4 byte sequence.
 *
 *	The last two parameters essentially tell it the format.
 *	YUY2	0,1	(YUYV)
 *	YVYU	0,3
 *	UYVY	1,0
 *	VYUY	1,2
 */
static void
convertPacked422 ( unsigned char *source
		, unsigned char *dest[3]
		, int height, int width
		, int lumaOffset, int uOffset )
{
	lumaOffset = lumaOffset % 2; // YxYxYx {0,1}
	uOffset = uOffset % 4; // xUxxxU {0,1,2,3}
	int vOffset = ( 4 - uOffset ) % 4; // xxxVxxxV {0,1,2,3}
	
	int inWidth = 2 * width;
	int inPoint = 0;
	int inPointNext = inWidth;
	int outPoint = 0;
	int outPointNext = width;
	int outPointChroma = 0;
	unsigned int u,v;
	
	for ( int y = 0; y < height; y += 2 )
	{
		for ( int x = 0; x < inWidth; x += 4 )
		{
			// luma
			dest[0][ outPoint ] = source[ inPoint + lumaOffset ];
			dest[0][ outPointNext ] = source[ inPointNext + lumaOffset];
			dest[0][ outPoint + 1 ] = source[ inPoint + lumaOffset + 2 ];
			dest[0][ outPointNext + 1 ] = source[ inPointNext + lumaOffset + 2 ];
			
			// chroma
			u = source[ inPoint + uOffset ];
			u += source[ inPointNext + uOffset ];
			dest[1][ outPointChroma ] = u >> 1;
			v = source[ inPoint + vOffset ];
			v += source[ inPointNext + vOffset ];
			dest[2][ outPointChroma ] = v >> 1;

			inPoint += 4;
			inPointNext += 4;
			outPoint += 2;
			outPointNext += 2;
			outPointChroma++;
	
		}
		inPoint = inPointNext;
		inPointNext += inWidth;
		outPoint = outPointNext;
		outPointNext += width;
	}
}

static void
fourCCToString ( unsigned int fourcc, char str[5] )
{
	str[4]=0;
	str[3]=fourcc>>24;
	str[2]=fourcc>>16 & 255;
	str[1]=fourcc>>8 & 255;
	str[0]=fourcc & 255;
}

// struct for current frame
struct FrameData 
{
	uint8_t* jpegFrame;// = NULL;
	int jpegSize;// = 0;

	unsigned char* audio;
	unsigned char *lavplayAudio;
	int audioSize; // = 0;
	
	int audioBytes;// = 0;
	int lavplayAudioBytes;

	uint8_t *inputBuffer; // = NULL;
	int lumaPlaneSize;// = 0;
	int chromaPlaneSize;// = 0;

	int audioOffset;
	long prevFrameDecodedTime; // = 0;

	y4m_frame_info_t y4m_info;

	uint8_t* frameYUV[3];
	uint8_t* lavYUV[3];
	uint8_t* decimateYUV[3];

};
static FrameData currentFrame = { NULL,0, NULL,NULL,0, 0,0, NULL,0,0, 0,0 };

// struct for a file
struct FileData
{
	char *filename; // = NULL;
	int32_t inputCodec; // = 0;
	int32_t outputCodec; // = 0;
	int frames; // = 0;
	int flip ;
} ;

// struct for all files
struct InputData
{
	int numFiles; // = 0;
	int currentFile; // = -1;
	int currentFrame; // = -1;

	int height; // = 0;
	int width; // = 0;
	double frameTime; // = 0;
	double frameRate; // = 0;
	int totalFrames; // = 0;

	int audioTotalBytes; // = 0;
	int audioBytesPerFrame;
	int audioReadBytes ;
	int audioRate;
	int audioBitsPerSample;
	int audioChannels;
	double dAudioBytesPerFrame; // = 0;
	unsigned int audioBlockAlign; // = 0;
#ifdef DEBUG_DIVXDEC
	int readAudioSamples; // = 0;
#endif

	unsigned int processVideo; // = 0;
	unsigned int processAudio; // = 0;

	long startTime_Sec; // = 0;
	long startTime_uSec; // = 0;

	IAviReadFile *file; // = NULL;
	IAviReadStream *invstream; // = NULL;
	IAviReadStream *inastream; // = NULL;

	struct timezone tz; // = { 0, 0 };
	struct timeval tv;
	vector<FileData> files; // = NULL;
};
static InputData input = 
	{ 0,-1,-1, 0,0,0,0,0, 0,0,0,0,0,0,0,0,
#ifdef DEBUG_DIVXDEC
	 	0, 
#endif		
		0,0, 0,0, NULL,NULL,NULL, {0, 0}} ;

struct OutputData
{
	int toYUV; // = 0;
	int toWAVE; // = 0;
	int toLAV; // = 0;
	int toPLAY; // = 0;

	int fdWAVE; // = 0;
	int fdYUV; // = 0;
	lav_file_t *fdLAV; // = 0;

	unsigned int lavQuality; // = 50;
	int currentLAVFile; 

	int beginFrame; // = 0;
	int endFrame; // = 0;
	int currentLavplayAudioFrame;
	int currentLavplayVideoFrame;
	int processedFrames; // = 0;
	int lavOutputBytes; // = 0;
	int lavFileSizeLimit; // MiB.

	int audioBytesWritten;
	int borderHeight; // = 0;
	int borderWidth; // = 0;
	int bordered; // = 0;
	int height;
	int width;
	lavplay_t *lavplayInfo;

	// special flags to indicate handling for lavplay
	int lavForcedInterlace ;
	int lavDecimate;
	int rationalise; // = 0;
	int yCrop;	// not yet implemented
	int xCrop;	// not yet implemented
	int yScale;	// not yet implemented
	int xScale;	// not yet implemented

	char *filenameLAV;
	char lavFormat;
	// for YUV output
	y4m_stream_info_t y4m_info;

};
static OutputData output = {0,0,0,0, 0,0,0, 50,1, 0,0,0,0,0,-1,1536,
		 0,0,0,0,0,0,0, LAV_NOT_INTERLACED,1,0,0,0,0,0};

static void
error ( char *text )
{
	mjpeg_error ( text );
}

static void
print_usage ( void )
{
	mjpeg_info ( "\n" );
	mjpeg_info ( "Usage: %s [OPTION]... [input AVI]\n", APPNAME );
	exit ( 0 );
}

static void
print_help ( void )
{
	printf ( "\nUsage: %s [OPTION] [input AVI...]\n", APPNAME );
	printf ( "\nOptions:\n" );
	printf ( "  -W --wavefile\t\tspecify the file to write WAVE data.\n\t\t\tif option present but no filename specified,\n\t\t\tstdout is used.\n" );
	printf ( "  -Y --yuvfile\t\tspecify the file to write YUV4MPEG data.\n\t\t\tif option present but no filename specified,\n\t\t\tstdout is used.\n" );
	printf ( "  -L --lavfile\t\tspecify the file to write LAV data.\n\t\t\tFormat must be given by (-f) below.\n" );
	printf ( "  -p --play\t\toutput through lav play.  Requires an argument:\n\t\t\tS - software\n\t\t\tH - Hardware to Screen\n\t\t\tC - hardware to output device.\n" );
	printf ( "  -f --format\t\tspecify the output LAV format {aAqm}\n" );
	printf ( "  -q --quality\t\tthe quality of the resulting LAV file/playback\n" );
	printf ( "  -n --norm\t\tthe norm to use with -p.  Must be one of [pn]\n" );
	printf ( "  -I --interlace\tfor -p [CH], if the input is too large to be played\n\t\t\tnon-interlaced, force interlacing. If not specified\n\t\t\tinput is automatically decimated, which is much better\n\t\t\tfor performance and still reasonable for quality.\n" );
	printf ( "  -r --rationalise\tEnsure that luma values are valid\n" );
	printf ( "  -e --endframe\t\tlast frame number to process\n" );
	printf ( "  -b --beginframe\tfirst frame number to process\n" );
	printf ( "  -m --maxfilesize\tFor lav output, MiB per file\n" );
	printf ( "  -V --version\t\tVersion and license.\n" );
	printf ( "  -v --verbose\t\tVerbosity level [0-2]\n" );
	printf ( "  -h --help\t\tPrint this help list.\n\n" );
	exit ( 0 );
}

static void
display_license ( void )
{
	mjpeg_info ( "This is %s version %s \n", APPNAME, APPVERSION );
	mjpeg_info ( "%s", "Copyright (C) Shawn Sulma <lavtools@athos.cx>\n" );
	mjpeg_info ( "Part of the MJPEG Square utilities (mjpeg.sourceforge.net). This\n" );
	mjpeg_info ( "program is distributed in the hope that it will be useful, but\n" );
	mjpeg_info ( "WITHOUT ANY WARRANTY; without even the implied warranty of\n" );
	mjpeg_info ( "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n" );
	mjpeg_info ( "See the GNU General Public License for more information.\n" );
	exit ( 0 );
}

static void
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

static void
nextLavFile ( )
{
	if ( output.fdLAV > 0 )
	{
		mjpeg_debug ( "Closing LAV output file\n" );
		lav_close ( output.fdLAV );
	}

	size_t thisfile_size = strlen ( output.filenameLAV ) + 10;
	char thisfile [ thisfile_size ];

	snprintf ( thisfile, thisfile_size, output.filenameLAV, output.currentLAVFile++ );

	mjpeg_debug ( "Opening LAV output file %s\n", thisfile );
	output.fdLAV = lav_open_output_file ( thisfile
			, output.lavFormat
			, output.width + output.borderWidth 
			, output.height + output.borderHeight
			, output.lavForcedInterlace
			, input.frameRate
			, ( input.processAudio ? input.audioBitsPerSample : 0 )
			, ( input.processAudio ? input.audioChannels : 0 )
			, ( input.processAudio ? input.audioRate : 0 ) ) ;
	// reset counter (per file)
	output.lavOutputBytes = 0;
}


static int
drawProgress ()
{
	// Progress...
	gettimeofday ( &input.tv, &input.tz );
	if ( input.tv.tv_sec != currentFrame.prevFrameDecodedTime )
	{
		double fps = output.processedFrames 
				/ ( 	( input.tv.tv_sec + input.tv.tv_usec / 1000000.0 ) 
					- ( input.startTime_Sec + input.startTime_uSec / 1000000.0 ) );
		currentFrame.prevFrameDecodedTime = input.tv.tv_sec;
		mjpeg_info ("Decoding frame %u, %.1f frames per second, %.1f seconds left.\r"
				, output.processedFrames
				, fps
				, ( ( double ) ( output.endFrame - output.beginFrame - output.processedFrames ) / fps ) );
		fflush ( stderr );
	}
	return 1;
}

static int
readInputFrame ()
{
	if ( input.processVideo )
	{
		// read the frame from the video stream, and get an object that represents
		input.invstream->ReadFrame ( );
		// the image of the frame.
		CImage *imsrc = input.invstream->GetFrame ();
		if ( imsrc == NULL )
		{
			// end of data, yes?
			return 0;
		}
#if AVIFILE_MAJOR_VERSION == 0 && AVIFILE_MINOR_VERSION < 50
		currentFrame.inputBuffer = imsrc->Data ();
#else
		currentFrame.inputBuffer = imsrc->data ();
#endif /* AVIFILE_MAJOR_VERSION */
		switch ( input.files[input.currentFile].outputCodec )
		{
		case RIFFINFO_YUY2:
			convertPacked422( currentFrame.inputBuffer, currentFrame.frameYUV, input.height, input.width, 0, 1 );
			break;
		case RIFFINFO_UYVY:
			convertPacked422( currentFrame.inputBuffer, currentFrame.frameYUV, input.height, input.width, 1, 0 );
			break;
		case RIFFINFO_YVYU:
			convertPacked422( currentFrame.inputBuffer, currentFrame.frameYUV, input.height, input.width, 0, 3 );
			break;
		case RIFFINFO_YV12:
			memcpy (currentFrame.frameYUV[0]
					, currentFrame.inputBuffer
					, currentFrame.lumaPlaneSize );
			memcpy (currentFrame.frameYUV[1]
					, currentFrame.inputBuffer + currentFrame.lumaPlaneSize
					, currentFrame.chromaPlaneSize );		
			memcpy (currentFrame.frameYUV[2]
					, currentFrame.inputBuffer + currentFrame.lumaPlaneSize + currentFrame.chromaPlaneSize
					, currentFrame.chromaPlaneSize );
			break;
		case RIFFINFO_IYUV:
		case RIFFINFO_I420:
			memcpy (currentFrame.frameYUV[0], currentFrame.inputBuffer, currentFrame.lumaPlaneSize );
			memcpy (currentFrame.frameYUV[2]
					, currentFrame.inputBuffer + currentFrame.lumaPlaneSize
					, currentFrame.chromaPlaneSize );		
			memcpy (currentFrame.frameYUV[1]
					, currentFrame.inputBuffer + currentFrame.lumaPlaneSize + currentFrame.chromaPlaneSize
					, currentFrame.chromaPlaneSize );
			break;
		}
		// done with image.  ( was delete imsrc; )
#if AVIFILE_MAJOR_VERSION == 0 && AVIFILE_MINOR_VERSION < 50
		imsrc->Release() ;
#else
		imsrc->release() ;
#endif
	}
	if ( input.processAudio )
	{
		// int targetBytes = ( output.processedFrames + 1 ) * input.audioBytesPerFrame ;
		double targetBytes = ( output.processedFrames + 1.0 ) * input.dAudioBytesPerFrame;
		int readSoFar = 0;
		int leftToGo = targetBytes - input.audioReadBytes; // - currentFrame.audioOffset;
		unsigned int bytesRead = 1;
		unsigned int samplesRead = 0;
		unsigned int audioSize = 0;
		unsigned char *currentAudio;
#ifdef DEBUG_DIVXDEC
		int samplesSoFar = 0 ;
		mjpeg_debug ("read %i, as-ao %i, left %i, eof %i\n", readSoFar, currentFrame.audioSize - currentFrame.audioOffset, leftToGo, input.inastream->Eof() );
#endif
		while ( ( readSoFar < ( currentFrame.audioSize - currentFrame.audioOffset ) ) // room in the buffer
				&& ( leftToGo > 0 ) // still want more bytes
				&& ( ! input.inastream->Eof () ) // not at end of file
				&& ( bytesRead > 0 ) // not getting empty buffers
				)
		{
			currentAudio = currentFrame.audio + currentFrame.audioOffset + readSoFar;
			audioSize = (unsigned int) (currentFrame.audioSize - currentFrame.audioOffset - readSoFar);
			input.inastream->ReadFrames ( currentAudio
					, audioSize
					, audioSize
					, samplesRead
					, bytesRead );
			readSoFar = (int) ( (unsigned int) readSoFar + bytesRead );
#ifdef DEBUG_DIVXDEC
			samplesSoFar = (int) ( (unsigned int) samplesSoFar + samplesRead );
#endif
			leftToGo = (int) ( (unsigned int) leftToGo - bytesRead );
		}
		input.audioReadBytes += readSoFar;
		currentFrame.audioBytes = readSoFar + currentFrame.audioOffset;
#ifdef DEBUG_DIVXDEC
		input.readAudioSamples += samplesSoFar ;
		mjpeg_debug ( "Available: %i, readSoFar: %i, samples: %i (total %i), offset: %i, total: %i\n"
				, currentFrame.audioBytes
				, readSoFar
				, samplesSoFar
				, input.readAudioSamples
				, currentFrame.audioOffset
				, input.audioReadBytes 
				);
		if ( ( currentFrame.audioBytes % input.audioBlockAlign ) != 0 )
		{
			mjpeg_debug ( "audio read is not block aligned\n" );
		}
#endif
	}
	return 1;
}

static void
rationalise ( uint8_t* luma, int height, int width )
{
	for (int i = 0; i < (height * width) ; i ++ )
	{
		luma[i] = ( luma [i] > 240 )  ? ( 240 ) : ( ( luma [i] < 16 ) ? 16 : luma [i] );
	}
}

static void
decimate ( uint8_t* source[3], uint8_t* dest[3], int height, int width )
{
	int destLumaWidth = width / 2;
	int destChromaWidth = width / 4;
	int sourceChromaWidth = width / 2;
	for (int y = 0; y < height; y += 2 )
	{
		int yDest = y / 2;
		int yOffset = y*width;
		for (int x = 0; x < width; x += 2 )
		{
			dest[0][yDest * destLumaWidth + x/2]
					= ( source[0][yOffset+ x]
					  + source[0][yOffset + width + x]
					  + source[0][yOffset + x + 1]
					  + source[0][yOffset + width + x + 1]
					  ) / 4;
		}
	}
	for (int y = 0; y < height/2; y += 2 )
	{
		int yDest = y / 2;
		int yOffset = y * sourceChromaWidth;
		for (int x = 0; x < width/2; x += 2 )
		{
			dest[1][yDest * destChromaWidth + x/2]
					= ( source[1][yOffset + x]
					  + source[1][yOffset + sourceChromaWidth + x]
					  + source[1][yOffset + x + 1]
					  + source[1][yOffset + sourceChromaWidth + x + 1]
					  ) / 4;
			dest[2][yDest * destChromaWidth + x/2]
					= ( source[2][yOffset + x]
					  + source[2][yOffset + sourceChromaWidth + x]
					  + source[2][yOffset + x + 1]
					  + source[2][yOffset + sourceChromaWidth + x + 1]
					  ) / 4;
		}
	}
}

static int
writeOutputFrame()
{
	if ( input.processVideo )
	{
		if ( output.rationalise )
		{
			rationalise ( currentFrame.frameYUV[0], input.height, input.width );
		}

		if ( output.toYUV )
		{					
			// frame is now converted.  Write out to the output stream.
			y4m_write_frame ( output.fdYUV, &output.y4m_info, &currentFrame.y4m_info, currentFrame.frameYUV );
		}
		if ( output.toLAV || output.toPLAY )
		{
			uint8_t* localYUV[3];
			int localHeight = input.height;
			int localWidth = input.width;
			if ( output.lavDecimate > 1 )
			{
				localYUV[0] = currentFrame.decimateYUV[0];
				localYUV[1] = currentFrame.decimateYUV[1];
				localYUV[2] = currentFrame.decimateYUV[2];
				//
				localHeight /= output.lavDecimate;
				localWidth /= output.lavDecimate;
				decimate (currentFrame.frameYUV, localYUV, input.height, input.width);
			}
			else
			{
				localYUV[0] = currentFrame.frameYUV[0];
				localYUV[1] = currentFrame.frameYUV[1];
				localYUV[2] = currentFrame.frameYUV[2];
			}

			if ( output.bordered )
			{
				// need to copy to another buffer, leaving the borders
				// to make it all nice and multiples of 16.
				// we only need to copy the active area, since the borders
				// have been set and are constant.
				int fulllumawidth = (localWidth + output.borderWidth);
				int fullchromawidth = (localWidth + output.borderWidth)/2;
				int origchromawidth = localWidth / 2;
				int startY = output.borderHeight/2;
				int startX = output.borderWidth/2;
				for ( int y = 0; y < localHeight; y++ )
				{
					memcpy ( currentFrame.lavYUV[0] + (startY + y) * fulllumawidth + startX
							, localYUV[0] + y * localWidth
							, localWidth );
				}
				for ( int y = 0; y < localHeight/2; y++ )
				{
					memcpy ( currentFrame.lavYUV[1] + (startY/2 + y) * fullchromawidth + startX/2
							, localYUV[1] + y * origchromawidth
							, origchromawidth );
					memcpy ( currentFrame.lavYUV[2] + (startY/2 + y) * fullchromawidth + startX/2
							, localYUV[2] + y * origchromawidth
							, origchromawidth );
				}
			}
			// generate a JPEG from the YUV frame.
			currentFrame.jpegSize = encode_jpeg_raw ( currentFrame.jpegFrame
					, (localHeight + output.borderHeight) * (localWidth + output.borderWidth)
					, output.lavQuality
					, output.lavForcedInterlace
					, 420	/* ctype */
					, output.borderWidth + localWidth
					, output.borderHeight + localHeight
					, output.bordered ? currentFrame.lavYUV[0] : localYUV[0]
					, output.bordered ? currentFrame.lavYUV[1] : localYUV[1]
					, output.bordered ? currentFrame.lavYUV[2] : localYUV[2] 
					);
			if ( currentFrame.jpegSize < 0 )
			{
				mjpeg_error ( "\n" );
				mjpeg_error ( "Error converting YUV to JPEG.\n" );
				errCode = 1;
				return 0;
			}
			// write frame to LAV file.
			if ( output.toLAV )
			{
				int lavwrite = lav_write_frame ( output.fdLAV, (char *) currentFrame.jpegFrame, currentFrame.jpegSize, 1) ;
				if ( lavwrite != 0 )
				{
					mjpeg_error ( "\n" );
					mjpeg_error ( "Error writing LAV frame: %s\n", lav_strerror () );
					errCode = 1;
					return 0;
				}
				output.lavOutputBytes += currentFrame.jpegSize;
			}
		}
	}
	if ( input.processAudio )
	{
		// similar to the read, we need to calculate the closest number of audio blocks
		// to catch up but not get ahead.  However, now we're more accurate.
		double targetBytes = ( output.processedFrames + 1.0 ) * input.dAudioBytesPerFrame - (double) output.audioBytesWritten;
		int writeBytes =  ( ( (int) targetBytes )/ input.audioBlockAlign ) * input.audioBlockAlign ;
		// Never write more bytes than we actually have.  Some care is taken to make
		// sure this doesn't happen, but still...
		writeBytes = min ( writeBytes, currentFrame.audioBytes );
		output.audioBytesWritten += writeBytes;

		if ( output.toWAVE )
		{
			doWrite ( output.fdWAVE
					, currentFrame.audio
					, writeBytes 
					);
		}
		if ( output.toLAV )
		{
			int res = lav_write_audio ( output.fdLAV
						, (char*) currentFrame.audio
						, writeBytes / input.audioBlockAlign 
						);
			if ( res != 0 )
			{
				mjpeg_error ( "\n" ); // break out of progress line.
				mjpeg_error ( "Error writing audio to LAV.\n" );
				errCode = 1;
				return 0;
			}
			output.lavOutputBytes += writeBytes;
		}
		if ( output.toPLAY )
		{
			// need to copy to outgoing lavplay audio buffer, which might be read
			// at some point in the future.
			memcpy ( currentFrame.lavplayAudio, currentFrame.audio, writeBytes );
		}
		if ( currentFrame.audioBytes > writeBytes )
		{
			currentFrame.audioOffset = currentFrame.audioBytes - writeBytes ;
			memmove ( currentFrame.audio
						, currentFrame.audio + writeBytes // input.audioBytesPerFrame
						, currentFrame.audioOffset
						);
		}
		else
		{
			currentFrame.audioOffset = 0;
		}
		currentFrame.lavplayAudioBytes = writeBytes; 
		
	}
	return 1;
}

static int
nextFile ()
{
	if ( input.currentFile )
	{
		delete input.file;
		input.file = NULL;
		delete input.invstream;
		input.invstream = NULL;
		delete input.inastream;
		input.inastream = NULL;
	}
	if ( (++input.currentFile) >= input.numFiles )
	{
		return 0; // all done.
	}
	// check to see if we're starting yet.
	if ( input.currentFrame < output.beginFrame )
	{
		if ( ( input.currentFrame + input.files[input.currentFile].frames ) < output.beginFrame )
		{
			input.currentFrame += input.files[input.currentFile].frames;
			// keep recursing forward through files until we're out of files
			// or find the frame we're looking for.
			return nextFile ();
		}
	}
	
	input.file = CreateIAviReadFile ( input.files[input.currentFile].filename );

	if ( input.processVideo )
	{
		input.invstream = input.file->GetStream (0, AviStream::Video);
		input.invstream->StartStreaming ();

		input.files[input.currentFile].outputCodec
			= readyDestination ( input.invstream
					, input.files[input.currentFile].flip
					, input.files[input.currentFile].inputCodec );
		char sFourCC[5];
		fourCCToString ( input.files[input.currentFile].inputCodec, sFourCC ) ;
		mjpeg_debug ( "VIDEO: Using decoder %s\n", sFourCC );
		fourCCToString ( input.files[input.currentFile].outputCodec, sFourCC );
		mjpeg_debug ( "VIDEO: Using interim YUV format %s\n", sFourCC );
#if AVIFILE_MAJOR_VERSION == 0 && AVIFILE_MINOR_VERSION < 50
		input.files[input.currentFile].frames = input.invstream->GetLength ();
#else
		input.files[input.currentFile].frames = input.invstream->GetEndPos ();
#endif
	}

	if ( input.processAudio )
	{
		input.inastream = input.file->GetStream (0, AviStream::Audio);
		input.inastream->StartStreaming ();
		if ( input.files[input.currentFile].frames == 0)
		{
#if AVIFILE_MAJOR_VERSION == 0 && AVIFILE_MINOR_VERSION < 50
			input.files[input.currentFile].frames = input.inastream->GetLength ();
#else
			input.files[input.currentFile].frames = input.inastream->GetEndPos ();
#endif
		}						
	}
	if ( input.currentFrame < output.beginFrame )
	{
		// need to seek inside this file to the right frame.
		framepos_t firstFrame = output.beginFrame - input.currentFrame;
		// firstFrame is the frame in this file that we start with.
		// well, actually, we get the closest we can -- the nearest
		// key frame.
		if ( input.processVideo )
		{
#if AVIFILE_MAJOR_VERSION == 0 && AVIFILE_MINOR_VERSION < 50
			framepos_t fp = input.invstream->SeekToKeyFrame ( firstFrame );
#else
			framepos_t fp = input.invstream->SeekToKeyframe ( firstFrame );
#endif
			if ( input.processAudio )
			{
#if AVIFILE_MAJOR_VERSION == 0 && AVIFILE_MINOR_VERSION < 50
				double pos = input.invstream->GetTime ( max( 0, fp - 1 ) );
				input.inastream->SeekTime ( pos );
#else
				double pos = input.invstream->GetTime (  );
				input.inastream->SeekToTime ( pos );
#endif
			}
		}
		else if ( input.processAudio )
		{
			mjpeg_error_exit1 ( "specifying start frame in audio-only output not yet supported\n" );
		}
		input.currentFrame = output.beginFrame;
	}
	return 1;
}

static int
nextFrame ()
{
	if ( interrupted )
	{
		return 0;
	}
	input.currentFrame++;
	if ( ( input.currentFrame >= output.endFrame ) || ( input.currentFile < 0 ) )
	{
		// either no file opened yet, or end of previous file.
		// open the next file.
		if ( ! nextFile () )
		{
			return 0;
		}
	}
	// everything's ready.
	int i = 1;
	i = i && drawProgress ();
	i = i && readInputFrame ();
	i = i && writeOutputFrame();
	output.processedFrames++;
	if ( ( output.toLAV ) && ( ( output.lavOutputBytes>>20 ) > ( output.lavFileSizeLimit ) ) )
	{
		nextLavFile ();
		if ( output.fdLAV == NULL )
		{
			mjpeg_error ( "Could not open LAV output file.  Aborting.\n" );
			return 0;
		}
	}
	return i;
}


//
//  Callbacks for liblavplay.
//

void lavplayMessage(int type, char *message)
{
	switch (type)
	{
	case LAVPLAY_MSG_ERROR:
		mjpeg_error ( "%s\n", message );
		break;
	case LAVPLAY_MSG_WARNING:
		mjpeg_warn ( "%s\n", message );
		break;
	case LAVPLAY_MSG_INFO:
		mjpeg_info ( "%s\n", message );
		break;
	case LAVPLAY_MSG_DEBUG:
#ifdef DEBUG_DIVX
		mjpeg_debug ( "%s\n", message );
#endif
		break;
	}
}

//
// Never called at the moment.
//
static void 
lavplayOutputStatistics(video_playback_stats *stats)
{
	// for now we do nothing.  We're not too interested in
	// stats.  Perhaps we'll eventually create a string
	// that we can have appear on the progress line.
	int h,m,s,f, fps;
	fps = stats->norm==1?30:25;
	h = stats->frame / (3600 * fps);
	m = (stats->frame / (60 * fps)) % 60;
	s = (stats->frame / fps) % 60;
	f = stats->frame % fps;
	printf("%d:%2.2d:%2.2d.%2.2d (%6.6d/%6.6ld) - Speed: %c%d, Norm: %s, Diff: %f\r\n"
			, h, m, s, f
			, stats->frame
			, output.lavplayInfo->editlist->video_frames
			, stats->play_speed>0?'+':(stats->play_speed<0?'-':' '), abs(stats->play_speed)
			, stats->norm==1?"NTSC":"PAL"
			, stats->tdiff
			);
	fflush(stdout);
}

void lavplayStateChanged(int newState)
{
	switch ( newState )
	{
	case LAVPLAY_STATE_PLAYING:
		mjpeg_debug ( "Changed to PLAYING\n" );
		break;
	case LAVPLAY_STATE_STOP:
		mjpeg_debug ( "Changed to STOP\n" );
		interrupted = 1;
		break;
	case LAVPLAY_STATE_PAUSED:
		mjpeg_debug ( "Changed to PAUSED\n" );
		break;
	}
}

static void 
lavplayGetVideoFrame(char *buffer, int *len, long num)
{
  /* the trick is to put the video buffer into chr *buffer
   * and set the length of the JPEG buffer into int *len
   */

	output.currentLavplayVideoFrame = max ( output.currentLavplayAudioFrame, output.currentLavplayVideoFrame + 1 );
	if ( output.currentLavplayVideoFrame > output.processedFrames )
	{
		if ( ! nextFrame () )
		{
			interrupted = true;
		}
	}
	
	if ( interrupted )
	{
		*(len) = 0;
	}
	else
	{
		*(len) = currentFrame.jpegSize ;
#ifdef DEBUG_DIVXDEC
		mjpeg_debug ( "VIDEO: frame %d, actually %i of %i (done %i).  %i bytes sent\n"
				, num
				, output.currentLavplayVideoFrame
				, output.endFrame
				, output.processedFrames
				, *(len) 
				);
#endif
		memcpy ( buffer, currentFrame.jpegFrame, currentFrame.jpegSize );
	}
}

static void 
lavplayGetAudioSamples(char *buffer, int *samps, long num)
{
  /* put a number of audio samples (int *samps) in the
   * char *buffer
   */
	output.currentLavplayAudioFrame = max ( output.currentLavplayAudioFrame + 1, output.currentLavplayVideoFrame );
	if ( output.currentLavplayAudioFrame > output.processedFrames )
	{
		if ( ! nextFrame () )
		{
			interrupted = true;
		}
	}
	if ( interrupted )
	{
		*(samps) = 0;
	}
	else
	{
		*(samps) = currentFrame.lavplayAudioBytes;
#ifdef DEBUG_DIVXDEC
		mjpeg_debug ( "AUDIO: frame %d, actually %i of %i (done %i).  %i bytes sent\n"
				, num
				, output.currentLavplayAudioFrame
				, output.endFrame
				, output.processedFrames 
				, *(samps)
				);
#endif
		memcpy ( buffer, currentFrame.lavplayAudio, *(samps) );
	}
}

static int freed = false;

static void 
freeAll ()
{
	if (freed)
	{
		return;
	}
	freed = true;
	if ( output.toYUV )
	{
		y4m_fini_stream_info(&output.y4m_info);
		y4m_fini_frame_info(&currentFrame.y4m_info);
		if ( output.fdYUV != FD_STDOUT )
		{
			close ( output.fdYUV );
		}
	}
		
	if ( output.toWAVE )
	{
		close ( output.fdWAVE );
	}
	
	if ( output.toLAV || output.toPLAY )
	{
		if ( output.fdLAV )
		{
			// close off LAV file.
			lav_close ( output.fdLAV );
		}
		delete currentFrame.jpegFrame ;
		if ( output.bordered )
		{
			delete [] currentFrame.lavYUV[0] ;
			delete [] currentFrame.lavYUV[1] ;
			delete [] currentFrame.lavYUV[2] ;
		}
		if ( output.toPLAY )
		{
			lavplay_stop ( output.lavplayInfo );
			lavplay_free ( output.lavplayInfo );
			if ( input.processAudio )
			{
				delete currentFrame.lavplayAudio ;
			}
		}
		if ( output.lavDecimate > 1 )
		{
			delete [] currentFrame.decimateYUV[0];
			delete [] currentFrame.decimateYUV[1];
			delete [] currentFrame.decimateYUV[2];
		}
	}

	
	if ( input.processVideo )
	{
		delete [] currentFrame.frameYUV[0] ;
		delete [] currentFrame.frameYUV[1] ;
		delete [] currentFrame.frameYUV[2] ;
	}	
	
	if ( input.processAudio ) 
	{
		delete currentFrame.audio ;
	}
	
	if ( input.invstream != NULL ) 
	{
		if ( input.invstream->IsStreaming () )
		{
			input.invstream->StopStreaming () ;
		}
		delete input.invstream ;
	}
	if ( input.inastream != NULL )
	{
		if ( input.inastream->IsStreaming () )
		{
			input.inastream->StopStreaming () ;
		}
		delete input.inastream ;
	}
	// this appears to segfault!?
	if ( input.file != NULL ) 
	{
		delete input.file;
	}
}

// From yuvplay

static void
sigint_handler ( int signal )
{
	if ( ! interrupted )
	{
		mjpeg_warn ( "Caught SIGINT, exiting...\n" );
		interrupted = 1;
		if ( ( output.toPLAY ) && ( output.lavplayInfo != 0 ) )
		{
			lavplay_stop( output.lavplayInfo );
		}
	}
}

static void
sigkill_handler ( int signal )
{
	mjpeg_warn ( "Caught SIGKILL, exiting...\n" );
	if ( ( ! interrupted ) && ( output.toPLAY ) && ( output.lavplayInfo != 0 ) )
	{
		lavplay_stop( output.lavplayInfo );
	}
	freeAll ();
	interrupted = 1;
	exit (1);
}

FILE real_stdout;

int
main (int argc, char **argv)
{
	// redirect stdout, so that avifile log messages don't appear in normal std out, but go
	// to stderr where they SHOULD be.
	cout = cerr;		// this could be naughty.
	real_stdout = * ( stdout );
	stdout = stderr;
	
	displayGreeting();

	if ( GetAvifileVersion (  ) != AVIFILE_VERSION )
	{
		mjpeg_error_exit1 ( "This binary was compiled for Avifile version %.2f but the library is %.2f\n"
				, AVIFILE_VERSION
				, GetAvifileVersion () );
	}

	( void ) mjpeg_default_handler_verbosity ( 3 );
	signal ( SIGINT, sigint_handler );
	signal ( SIGKILL, sigkill_handler );

	int copt;		// getopt switch variable

	char *filenameWAVE = NULL;
	char *filenameYUV = NULL;
	char *filenameLAV = NULL;
//	char optLavFormat = 'a';

	input.currentFile = -1;
	input.currentFrame = -1;
		
	int optVerbosity = 1;
	int optLavplayDestination = 0;
	int optNorm = 0;
	int optRationalise = false;
	int optAllowInterlace = false;
	output.lavFileSizeLimit = 1536;  // default
	
	mjpeg_debug ("Started\n");
	
	while (1)
	{
		int option_index = 0;

		static struct option long_options[] = 
		{
			{"help", 0, 0, 'h'},
			{"wavefile", optional_argument, NULL, 'W'},
			{"yuvfile", optional_argument, NULL, 'Y'},
			{"lavfile", required_argument, NULL, 'L'},
			{"play", required_argument, NULL, 'p'},
			{"format", required_argument, NULL, 'f' },
			{"version", no_argument, NULL, 'V'},
			{"license", no_argument, NULL, 'V'},
			{"beginframe", required_argument, NULL, 'b'},
			{"endframe", required_argument, NULL, 'e'},
			{"quality", required_argument, NULL, 'q'},
			{"interlace", no_argument, NULL, 'I'},
			{"rationalise", no_argument, NULL, 'r'},
			{"norm", required_argument, NULL, 'n'},
			{"verbose", required_argument, NULL, 'v'},
			{"maxfilesize", required_argument, NULL, 'm'},
			{0, 0, 0, 0}
		};

		copt = getopt_long ( argc, argv, "b:Irv:hW:Y:L:f:Ve:n:q:p:m:", long_options, &option_index );
		if (copt == -1)
		{
			break;
		}

		switch (copt)
		{
		case 0:
			// no more
			break;
		case 'h':
			print_help ();
			break;
		case 'W':
			// wave file specified.
			if ( ( strlen (optarg) < 1 ) 
					|| ( 0 == strncmp( optarg, "-", 1) ) ) 
			{
				filenameWAVE = "-";
			}
			else
			{
				filenameWAVE = optarg;
			}
			break;
		case 'Y':
			// yuv file specified.
			if ( ( strlen (optarg) < 1 ) 
					|| ( 0 == strncmp( optarg, "-", 1) ) ) 
			{
				filenameYUV = "-";
			}
			else
			{
				filenameYUV = optarg;
			}
			break;
		case 'L':
			filenameLAV = optarg;
			break;
		case 'f':
			// lav output format
			if ( strlen (optarg) < 1 )
			{
				mjpeg_error_exit1 ( "Lav format option requires an argument\n" );
			}
			output.lavFormat = optarg[0];
			switch ( output.lavFormat )
			{
				case 'A':
				case 'a':
				case 'm':
				case 'q':
					break;
				case 'M':
					output.lavFormat = 'm';
					break;
				case 'Q':
					output.lavFormat = 'q';
					break;
				default:
					mjpeg_error_exit1 ( "Lav format must be one of 'm', 'q', 'A', 'a'\n" );
			}
			break;
		case 'e':
			output.endFrame = atoi ( optarg );
			if ( output.endFrame  < 1 )
			{
				mjpeg_error_exit1 ( "Invalid endframe specified.\n" );
			}
			break;
		case 'b':
			output.beginFrame = atoi ( optarg );
			if ( output.beginFrame  < 1 )
			{
				mjpeg_error_exit1 ( "Invalid beginframe specified.\n" );
			}
			break;
		case 'V':
			display_license ();
			break;
		case 'v':
			optVerbosity = atoi( optarg );
			if (optVerbosity < 0 || optVerbosity > 2)
			{
				mjpeg_error_exit1 ( "-v option requires arg 0, 1, or 2\n" );
			}
			break;
		case 'q':
			output.lavQuality = atoi ( optarg );
			if ( ( output.lavQuality < 24 ) || ( output.lavQuality > 100 ) )
			{
				mjpeg_error_exit1 ( "quality parameter (%d) out of range [24..100]\n", output.lavQuality);
			}
			break;
		case 'n':
			if ( strlen ( optarg ) > 0 ) 
			{
				optNorm = optarg[0];
				if ( ( optNorm == NORM_PAL ) || ( optNorm = NORM_PAL ) )
				{
					break;
				}
			}
			mjpeg_error_exit1 ( "Specify 'n' for NTSC or 'p' for PAL with -n\n" );
		case 'p':
			if ( strlen ( optarg ) > 0 )
			{
				optLavplayDestination = optarg[0];
				switch ( optLavplayDestination )
				{
#ifdef HAVE_V4L
					case 'C':
					case 'H':
#endif
#ifdef HAVE_SDL
					case 'S':
#endif
						break;
					default:
						mjpeg_error_exit1 ( "Unknown playback mode: %c\n", optLavplayDestination );
				}
			}
			else
			{
				mjpeg_error_exit1 ( "You must specify an argument for -p.\n" );
			}
			break;
		case 'I':
			optAllowInterlace = true;
			break;
		case 'r':
			output.rationalise = true;
			break;
		case 'm':
			output.lavFileSizeLimit = atoi ( optarg );
			if ( output.lavFileSizeLimit < 1 )
			{
				mjpeg_error_exit1 ( "maxfilesize cannot be negative\n" );
			}
			break;
		case '?':
			print_usage ();
			break;
		case ':':
			printf ("You missed giving something and argument.\n");	// since we have non-options
			break;							// never gets called
		}
	}

	(void) mjpeg_default_handler_verbosity ( optVerbosity );

	input.numFiles = argc - optind;

	if ( input.numFiles < 1 )
	{
		mjpeg_error_exit1 ( "You need specify at least one input file.\n" );
	}

	if ( ( filenameWAVE != NULL ) 
			&& ( filenameYUV != NULL ) 
			&& ( 0 == strcmp ( filenameWAVE, filenameYUV ) ) )
	{
		mjpeg_error_exit1 ( "You cannot send both video and audio to the same file (or stdout).\n" );
	}

	// mjpeg_debug ( "allocating variables\n" );
	BITMAPINFOHEADER bmpHeader;
	WAVEFORMATEX waveHeader;
	int i = 0;
	int tempfile;

	output.toWAVE = ( filenameWAVE != NULL );
	output.toYUV = ( filenameYUV != NULL );
	output.toLAV = ( filenameLAV != NULL );
	output.toPLAY = ( optLavplayDestination != 0 );
	input.processVideo = ( output.toYUV || output.toLAV || output.toPLAY );
	input.processAudio = ( output.toWAVE || output.toLAV || output.toPLAY );
	
	while (optind < argc)
	{
		IAviReadFile *file = NULL;
		IAviReadStream *invstream = NULL;
		IAviReadStream *inastream = NULL;
		FileData *fileData = new FileData;
		input.files.push_back( *fileData );
		input.files[i].filename = argv[optind];
		mjpeg_debug ( "Checking file %s\n", input.files[i].filename );
		tempfile = access ( input.files[i].filename, R_OK );
		if ( tempfile < 0 )
		{
			mjpeg_error ( "Could not open %s.\n", input.files[i].filename );
			errCode = EXIT_FAILURE;
			break;
		}
		try
		{
			mjpeg_debug ( "about to read from %s\n", input.files[i].filename );
			file = CreateIAviReadFile ( input.files[i].filename );
			// always need to look at the video stream at this point.
			invstream = file->GetStream ( 0, AviStream::Video );
			invstream->StartStreaming ();
			mjpeg_debug ( "VIDEO stream queried\n" );

			if ( input.processAudio )
			{
				inastream = file->GetStream ( 0, AviStream::Audio );
				mjpeg_debug ( "AUDIO stream queried\n" );
			}
#if AVIFILE_MAJOR_VERSION == 0 && AVIFILE_MINOR_VERSION < 50
			input.files[i].frames = invstream->GetLength();
#else
			input.files[i].frames = invstream->GetEndPos ();
#endif
			input.files[i].inputCodec = invstream->GetDecoder()->GetCodecInfo().fourcc;
			input.totalFrames += input.files[i].frames;
			if ( i == 0 )
			{
				invstream->GetOutputFormat ( &bmpHeader, sizeof bmpHeader );
				input.frameTime = invstream->GetFrameTime ();
				input.frameRate = 1.0 / input.frameTime;
				if ( input.processVideo )
				{
					mjpeg_debug ( "VIDEO: framesize: %i\n", invstream->GetFrameSize ());
					// not needed as it's just used as a pointer into CImage
					// currentFrame.inputBuffer = new unsigned char [invstream->GetFrameSize ()];
					input.files[i].flip = ( bmpHeader.biHeight < 0 ) ? 0 : 1;
				}							
				if ( input.processAudio )
				{
					if ( inastream != NULL ) 
					{
						inastream->StartStreaming ();
						inastream->GetOutputFormat ( &waveHeader, sizeof waveHeader );
						input.audioBlockAlign = waveHeader.wBitsPerSample / 8 * waveHeader.nChannels;
						input.audioBitsPerSample = waveHeader.wBitsPerSample;
						input.audioRate = waveHeader.nSamplesPerSec;
						input.audioChannels = waveHeader.nChannels;

						input.dAudioBytesPerFrame = (input.frameTime * (double) waveHeader.nSamplesPerSec * (double) input.audioBlockAlign);
						input.audioBytesPerFrame = (int) input.dAudioBytesPerFrame;
						input.audioBytesPerFrame += input.audioBlockAlign - ( input.audioBytesPerFrame % input.audioBlockAlign );
//						input.dAudioBytesPerFrame = input.audioBytesPerFrame;
						currentFrame.audioOffset = 0;

						input.audioTotalBytes = input.totalFrames 
									* input.dAudioBytesPerFrame;
						mjpeg_debug ( "AUDIO length of file: %i\n", input.audioTotalBytes );
						
					}
					else
					{
						mjpeg_warn ( "Audio not present in first file. Ignoring all audio.\n" );
						input.processAudio = 0 ;
					}
				}
			}
			else
			{
				BITMAPINFOHEADER this_bh;
				WAVEFORMATEX this_fmt;
				if ( input.processVideo )
				{
					invstream->GetOutputFormat ( &this_bh, sizeof this_bh );
					input.files[i].flip = ( this_bh.biHeight > 0 ) ? 0 : 1;
					if ( ( bmpHeader.biWidth != this_bh.biWidth )
						 || ( bmpHeader.biHeight != this_bh.biHeight ) )
					{
						mjpeg_error_exit1 ( "Width and Height of all files must be the same.\n" );
					}
					if ( invstream->GetFrameTime() != input.frameTime )
					{
						mjpeg_error_exit1 ( "Frame rate of all files must be the same.\n" );
					}
				}

				if ( input.processAudio )
				{
					if ( inastream == NULL )
					{
						mjpeg_error_exit1 ( "Audio does not exist for all files.\n" );
					}
					inastream->StartStreaming ();
					inastream->GetOutputFormat ( &this_fmt, sizeof this_fmt );
					input.audioTotalBytes += waveHeader.cbSize;
					if (   ( waveHeader.wFormatTag != this_fmt.wFormatTag )
					   ||  ( waveHeader.nChannels != this_fmt.nChannels )
					   ||  ( waveHeader.nSamplesPerSec != this_fmt.nSamplesPerSec )
					   ||  ( waveHeader.nAvgBytesPerSec != this_fmt.nAvgBytesPerSec )
					   ||  ( waveHeader.wBitsPerSample != this_fmt.wBitsPerSample )
					   ||  ( waveHeader.nBlockAlign != this_fmt.nBlockAlign )
					   )
					{
						mjpeg_error_exit1 ( "Audio format of file '%s' incompatible.\n", input.files[i].filename );
					}
				}
			}
			if ( invstream != NULL )
			{
				delete invstream ;
			}
			if ( inastream != NULL )
			{
/*				StreamInfo *si = inastream->GetStreamInfo();
		
				mjpeg_debug ( "AUDIO: stream->bps: %f\n", si->GetBps () );
				mjpeg_debug ( "AUDIO: stream->fps: %f\n", si->GetFps () );
				mjpeg_debug ( "AUDIO: stream->lt: %f\n", si->GetLengthTime () );
				mjpeg_debug ( "AUDIO: stream->ss: %i\n", si->GetStreamSize () );
				mjpeg_debug ( "AUDIO: stream->sf: %u\n", si->GetStreamFrames () );
				mjpeg_debug ( "AUDIO==samples per frame->%f\n", (double) si->GetStreamSize() / (double) si->GetStreamFrames() );
				mjpeg_debug ( "AUDIO: stream->vbr: %i\n", si->IsAudioVbr() );
				cerr << "*********" << si->GetString() << "\n" ;
				delete si;*/
				delete inastream ;
			}
			//delete file ;
			mjpeg_debug ( "Frames found so far: %u\n", input.totalFrames );
		}
		catch ( FatalError &error )
		{
			mjpeg_error ( "An error occurred checking %s\n", input.files[i].filename );
			error.PrintAll ();
			exit ( 1 );
		}

		optind++;
		i++;
	}

	mjpeg_debug ( "All input files checked.\n" );

	if ( input.processVideo )
	{
		input.height = abs (bmpHeader.biHeight);
		input.width = abs (bmpHeader.biWidth);
		currentFrame.lumaPlaneSize = input.width * input.height ;
		currentFrame.chromaPlaneSize = currentFrame.lumaPlaneSize / 4;

		mjpeg_debug ( "VIDEO: framerate: %.5f\n", input.frameRate );
		mjpeg_debug ( "VIDEO: Total frames: %i\n", input.totalFrames );

		currentFrame.frameYUV[0] = new unsigned char [currentFrame.lumaPlaneSize];
		currentFrame.frameYUV[1] = new unsigned char [currentFrame.chromaPlaneSize];
		currentFrame.frameYUV[2] = new unsigned char [currentFrame.chromaPlaneSize];
		memset (currentFrame.frameYUV[0], 0, currentFrame.lumaPlaneSize);
		memset (currentFrame.frameYUV[1], 128, currentFrame.chromaPlaneSize);
		memset (currentFrame.frameYUV[2], 128, currentFrame.chromaPlaneSize);
	}
		
	if ( output.toYUV )
	{
		if ( 0 == strcmp ( "-", filenameYUV ) )
		{
			//output.fdYUV = FD_STDOUT;
			output.fdYUV = fileno ( &real_stdout );
		}
		else
		{
			mjpeg_debug ("VIDEO: YUV output file: %s\n", filenameYUV );
			output.fdYUV = open ( filenameYUV, O_WRONLY | O_CREAT | O_TRUNC);
		}
		if ( output.fdYUV < 1 )
		{
			mjpeg_error_exit1 ( "Could not open output YUV file.\n" );
		}

		y4m_init_stream_info(&output.y4m_info);
		y4m_init_frame_info(&currentFrame.y4m_info);

		y4m_si_set_width ( &output.y4m_info, input.width );
		y4m_si_set_height ( &output.y4m_info, input.height );
	
		// Interlacing appears to be unique to the MJPEG avi
		// implementation, and avifile doesn't provide a way to
		// determine if a stream is interlaced or not.  From a
		// preliminary investigation there is no way from the AVI
		// stream header information available in the file to
		// determine this. :(
		mjpeg_warn ( "Cannot detect interlacing in avi files.\n" );
		y4m_si_set_interlace ( &output.y4m_info, Y4M_ILACE_NONE );

		y4m_ratio_t y4m_framerate;
		y4m_framerate.n = (int) (input.frameRate * 1000000.0);
		y4m_framerate.d = 1000000;

		y4m_si_set_framerate ( &output.y4m_info, y4m_framerate );

		//y4m_ratio_t y4m_aspect;
		//y4m_aspect.n = input.width;
		//y4m_aspect.d = input.height;

		y4m_si_set_sampleaspect ( &output.y4m_info, y4m_sar_UNKNOWN );
		y4m_write_stream_header ( output.fdYUV, &output.y4m_info);

	}
	
	if ( input.processAudio )
	{
		mjpeg_debug ( "AUDIO: Number of channels: %i\n", waveHeader.nChannels );
		mjpeg_debug ( "AUDIO: Detected samples per second: %i\n", waveHeader.nSamplesPerSec );
		currentFrame.audioSize = input.audioBytesPerFrame * input.audioBlockAlign;
		currentFrame.audio = new uint8_t [ currentFrame.audioSize ];
		memset ( currentFrame.audio, 0, currentFrame.audioSize );

		if ( ( output.endFrame > 0 ) && ( output.endFrame < input.totalFrames ) )
		{
			input.audioTotalBytes = (int) (output.endFrame * input.dAudioBytesPerFrame);
			input.audioTotalBytes += input.audioBlockAlign - ( input.audioTotalBytes % input.audioBlockAlign );
		}
		mjpeg_debug ( "AUDIO: Total audio data: %i bytes.\n", input.audioTotalBytes );
		mjpeg_debug ( "AUDIO: Audio bytes per frame: %i\n", input.audioBytesPerFrame);
		mjpeg_debug ( "AUDIO: (but more precisely: %.5f)\n", input.dAudioBytesPerFrame);


		if ( output.toWAVE )
		{
			if ( 0 == strcmp ( "-", filenameWAVE ) )
			{
				output.fdWAVE = FD_STDOUT;
			}
			else
			{
				mjpeg_debug ("AUDIO: WAVE output file: %s\n", filenameWAVE );
				output.fdWAVE = open ( filenameWAVE, O_WRONLY | O_CREAT | O_TRUNC );
			}
			if ( output.fdWAVE < 1 )
			{
				mjpeg_error_exit1 ( "Could not open output WAVE file.\n" );
			}
			wave_header wave;
		
			writeWaveHeader( wave
				, waveHeader.wBitsPerSample
				, waveHeader.nSamplesPerSec
				, waveHeader.nChannels
				, input.audioTotalBytes
				, output.fdWAVE );

		}

		if ( output.toPLAY )
		{
			currentFrame.lavplayAudio = new uint8_t [ currentFrame.audioSize ];
			memset ( currentFrame.lavplayAudio, 0, currentFrame.audioSize );
		}

	}

	output.height = input.height;
	output.width = input.width;
	
	if ( output.toPLAY )
	{
		// set up, but don't start liblavplay.
		output.lavplayInfo = lavplay_malloc ();

		if ( ! output.lavplayInfo )
		{
			mjpeg_error_exit1 ( "Could not initialise lavplay.\n" );
		}
		output.lavplayInfo->playback_mode = optLavplayDestination;
		output.lavplayInfo->msg_callback = lavplayMessage;
		output.lavplayInfo->state_changed = lavplayStateChanged;
		output.lavplayInfo->get_video_frame = lavplayGetVideoFrame;
		output.lavplayInfo->zoom_to_fit = 1; // need a way to detect this.
		output.lavplayInfo->continuous = 1;
		if ( input.processAudio )
		{
			output.lavplayInfo->get_audio_sample = lavplayGetAudioSamples;
		}
		else
		{
			output.lavplayInfo->audio = 0; /* disable audio */
		}

		// Seems like I need to set up an edit list as well.

		EditList *editList = new EditList ;
		memset (editList, 0, sizeof ( EditList ) );

		// Guessing on norm based on editlist.c
		if ( optNorm != 0 )
		{
			editList->video_norm = optNorm;
		}
		else if ( ( input.frameRate > 24 ) && ( input.frameRate < 26 ) )
		{
			editList->video_norm = NORM_PAL;
		}
		else if ( ( input.frameRate > 29 ) && ( input.frameRate < 31 ) )
		{
			editList->video_norm = NORM_NTSC;
		}
		else 
		{
			mjpeg_error_exit1 ( "Norm could not be guessed, specify with -n [pn]\n" );
		}
		editList->video_fps = ( editList->video_norm == NORM_NTSC )
				? 30000.0 / 1001.0
				: 25.0;

#ifdef HAVE_V4L
		if ( optLavplayDestination != 'S' ) // hardware playback of some sort requires additional checks
		{
			// need to detect hardware width, eventually.
			int hwWidth = 0;
			// height we can guess based on the norm.
			int hwHeight = ( editList->video_norm == NORM_NTSC ) ? 480 : 576 ;
			// simple initial version
			if ( ( input.height + output.borderHeight ) > hwHeight )
			{
				mjpeg_error_exit1 ( "Cannot handle an image this tall... yet.\n" );
			}
			if ( ( input.height + output.borderHeight ) > ( hwHeight / 2 ) )
			{
				if ( optAllowInterlace )
				{
					// in this case, we can play it, but need to fake an interlace.
					//
					// as a side effect, if both lavplay and lavfile output
					// are chosen, the outputs will both share the same interlacing.
					//
					output.lavForcedInterlace = LAV_INTER_TOP_FIRST;
					output.lavDecimate = 1;
				}
				else
				{
					// try decimation instead;
					output.lavDecimate = 2;
					// initialise the decimation buffers
					output.height /= output.lavDecimate;
					output.width /= output.lavDecimate;
					currentFrame.decimateYUV[0] = new unsigned char [ output.height * output.width ];
					currentFrame.decimateYUV[1] = new unsigned char [ output.height * output.width / 4 ];
					currentFrame.decimateYUV[2] = new unsigned char [ output.height * output.width / 4 ];
					memset (currentFrame.decimateYUV[0], 0, output.height * output.width );
					memset (currentFrame.decimateYUV[1], 128, output.height * output.width / 4 );
					memset (currentFrame.decimateYUV[2], 128, output.height * output.width / 4 );
					mjpeg_debug ( " Lav decimation factor: %i\n", output.lavDecimate );
				}
			}
		}
#endif
		output.lavplayInfo->editlist = editList;
	}    
		
	if ( output.toLAV || output.toPLAY )
	{
		// if not a multiple of 16 in each direction, get ready to
		// add a black border.
		if ( output.height % 16 != 0 )
		{
			output.borderHeight = 16 - ( output.height % 16 );
		}
		if ( output.width % 16 != 0 )
		{
			output.borderWidth = 16 - ( output.width % 16 );
		}

		currentFrame.jpegFrame =  new uint8_t [ input.width * input.height ];
		output.bordered = ( ( output.borderHeight + output.borderWidth) > 0 );
		mjpeg_info ( "Original input is %i x %i\n", input.width, input.height );
		if ( output.bordered )
		{
			int fullluma = ( output.borderHeight + output.height )
					* ( output.borderWidth + output.width );
			int fullchroma = fullluma / 4;
			// need to set up a second buffer.
			currentFrame.lavYUV[0] = new unsigned char [ fullluma ];
			currentFrame.lavYUV[1] = new unsigned char [ fullchroma ];
			currentFrame.lavYUV[2] = new unsigned char [ fullchroma ];
			memset (currentFrame.lavYUV[0], 0, fullluma);
			memset (currentFrame.lavYUV[1], 128, fullchroma);
			memset (currentFrame.lavYUV[2], 128, fullchroma);

			mjpeg_info ( "Outbound input is %i x %i\n", output.width, output.height );
			mjpeg_info ( "Adding %i wide and %i high border for lav file/play\n", output.borderWidth, output.borderHeight );
		}
	}

	if ( output.toLAV )
	{
		output.filenameLAV = filenameLAV;
		nextLavFile ();
		if ( output.fdLAV == NULL )
		{
			mjpeg_error_exit1 ( "Could not open output LAV file.\n" );
		}
	}


	if ( output.endFrame < 1 )
	{
		output.endFrame = input.totalFrames;
	}

	struct timeval tv;
	struct timezone tz = { 0, 0 };
	gettimeofday (&tv, &tz);
	input.startTime_Sec = tv.tv_sec;
	input.startTime_uSec = tv.tv_usec;

	input.currentFile = -1;
	if ( output.toPLAY )
	{
		output.lavplayInfo->editlist->video_width = output.width + output.borderWidth;
		output.lavplayInfo->editlist->video_height = output.height + output.borderHeight;
		output.lavplayInfo->editlist->video_fps = input.frameRate;
		output.lavplayInfo->editlist->video_frames = 0;
		output.lavplayInfo->editlist->video_inter = output.lavForcedInterlace;
		output.lavplayInfo->editlist->has_audio = input.processAudio;
		output.lavplayInfo->editlist->audio_rate = input.audioRate;
		output.lavplayInfo->editlist->audio_chans = input.audioChannels;
		output.lavplayInfo->editlist->audio_bits = input.audioBitsPerSample;
		output.lavplayInfo->editlist->audio_bps = ( input.audioBitsPerSample * input.audioChannels + 7 ) / 8 ;
		output.lavplayInfo->editlist->MJPG_chroma = CHROMA420;
		output.lavplayInfo->editlist->max_frame_size = input.height * input.width;
		// all ready?

		// now start the music... er... liblavplay.
		// everything will be driven by the player, including
		// output to any other files.

		output.currentLavplayVideoFrame = 0;
		output.currentLavplayAudioFrame = 0;
		lavplay_main ( output.lavplayInfo );
		// lavplay_busy ( output.lavplayInfo );
		while ( ( ! interrupted ) && ( input.currentFrame < output.endFrame ) )
		{
			// sleep for two frames
			usleep ( (unsigned int) (input.frameTime * 2000000.0) );
		}
	}
	else
	{
		// we drive it internally, as fast as plausible.
		while ( nextFrame () )
		{
			// progress bar?  Nah.  That should be in nextFrame, no?
		}
	}

	freeAll ();

	mjpeg_info ( "\n" );
	mjpeg_info ( "Done. %i frames\n", output.processedFrames );

	return errCode;
}



