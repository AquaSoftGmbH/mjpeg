/*
 *
 *  lav2wav.c
 *    
 *	Copyright (C) 2000 MSSG, Rainer Johanni, Aaron Holtzman, Andrew Stevens
 *
 *  This file is part of lav2wav,a component of the "MJPEG tools"
 *  suite of open source tools for MJPEG and MPEG processing.
 *	
 *  lav2wav is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *   
 *  lav2wav is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *   
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 
 *
 *
 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "lav_io.h"
#include "editlist.h"
#include "mjpeg_logging.h"

#define WAVE_FORMAT_PCM  0x0001
#define FORMAT_MULAW     0x0101
#define IBM_FORMAT_ALAW  0x0102
#define IBM_FORMAT_ADPCM 0x0103


struct riff_struct 
{
  unsigned char id[4];   /* RIFF */
  unsigned long len;
  unsigned char wave_id[4]; /* WAVE */
};


struct chunk_struct 
{
	unsigned char id[4];
	unsigned long len;
};

struct common_struct 
{
	unsigned short wFormatTag;
	unsigned short wChannels;
	unsigned long dwSamplesPerSec;
	unsigned long dwAvgBytesPerSec;
	unsigned short wBlockAlign;
	unsigned short wBitsPerSample;  /* Only for PCM */
};

struct wave_header 
{
	struct riff_struct   riff;
	struct chunk_struct  format;
	struct common_struct common;
	struct chunk_struct  data;
};

struct wave_header wave;
int verbose = 1;

int wav_header( unsigned int bits, unsigned int rate, unsigned int channels, int fd );
void Usage(char *str);


/*
  Raw write does *not* guarantee to write the entire buffer load if it
  becomes possible to do so.  This does...
 */

static size_t do_write( int fd, void *buf, size_t count )
{
	char *cbuf = buf;
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

int wav_header( unsigned int bits, unsigned int rate, unsigned int channels, int fd )
{
	off_t dummy_size = 0x7fffff00+sizeof(wave);

	/* Write out a ZEROD wave header first */
	memset(&wave, 0, sizeof(wave));

	strncpy(wave.riff.id, "RIFF", 4);
	strncpy(wave.riff.wave_id, "WAVE",4);
	dummy_size -=8;
	wave.riff.len =  dummy_size;
	strncpy(wave.format.id, "fmt ",4);
	wave.format.len = sizeof(struct common_struct);

	/* Store information */
	wave.common.wFormatTag = WAVE_FORMAT_PCM;
	wave.common.wChannels = channels;
	wave.common.dwSamplesPerSec = rate;
	wave.common.dwAvgBytesPerSec = channels*rate*bits/8;
	wave.common.wBlockAlign = channels*bits/8;
	wave.common.wBitsPerSample = bits;

	strncpy(wave.data.id, "data",4);
	dummy_size -= 20+sizeof(struct common_struct);
	wave.data.len = dummy_size;
	if (do_write(fd, &wave, sizeof(wave)) != sizeof(wave)) 
	{
		return 1;
	}
	return 0;
}



static void wav_close(int fd)
{
	off_t size;

	/* Find how long our file is in total, including header */
	size = lseek(fd, 0, SEEK_CUR);

	if (size < 0 ) 
	{
		if( fd > 2 )
		{
			mjpeg_error("lseek failed - wav-header is corrupt\n");
		}
		goto EXIT;

	}

	/* Rewind file */
	if (lseek(fd, 0, SEEK_SET) < 0) 
	{
		mjpeg_error("rewind failed - wav-header is corrupt\n");
		goto EXIT;
	}
	mjpeg_debug("Writing WAV header\n");

	// Fill out our wav-header with some information. 
	size -= 8;
	wave.riff.len = size;
	size -= 20+sizeof(struct common_struct);
	wave.data.len = size;

	if (do_write(fd, &wave, sizeof(wave)) < sizeof(wave)) 
	{
		mjpeg_error("wav-header write failed -- file is corrupt\n");
		goto EXIT;
	}
	mjpeg_info("WAV done\n");

EXIT:
	close(fd);
}


EditList el;

void Usage(char *str)
{
   fprintf(stderr, "Usage: %s [options] inputfiles\n",str);
   fprintf(stderr, "where options are:\n");
   fprintf(stderr, "-s num        Start extracting at video frame (num)\n");
   fprintf(stderr, "-c num        Extract (num) frames of audio\n");
   fprintf(stderr, "-v num        verbose level [0..2]\n");
   fprintf(stderr, "-I            Ignore unsupported bitrates/bits per sample\n");
 
   exit(0);
}

static short audio_buff[256*1024]; /* Enough for 1fps, 48kHz ... */

int
main(argc, argv)
int     argc;
char    **argv;
{
	int n,f;
	int res;
	int warned = 0;
	int start_frame = 0;
	int num_frames = -1;
        int ignore_bitrate = 0;

    while( (n=getopt(argc,argv,"v:s:c:I")) != EOF)
    {
        switch(n) {

	   case 'v':
		verbose = atoi(optarg);
		if( verbose < 0 || verbose > 2 )
			Usage(argv[0]);
		break;
	   case 's':
		start_frame = atoi(optarg);
		break;
	   case 'c':
		num_frames = atoi(optarg);
		break;
	   case 'I':
		ignore_bitrate = 1;
		break;
	   case '?':
		Usage(argv[0]);
        }
    }

    /* Open editlist */


	if( argc-optind < 1)
		Usage(argv[0]);

	(void)mjpeg_default_handler_verbosity(verbose);

    read_video_files(argv + optind, argc - optind, &el);

    if(!el.has_audio)
    {
        mjpeg_error("Input file(s) have no audio\n");
        exit(1);
    }

    if(!ignore_bitrate) 
      {
        if(el.audio_bits!=16)
          {
            mjpeg_error("Input file(s) must have 16 bit audio!\n");
            exit(1);
          }
        
        switch (el.audio_rate) {
        case 48000 : 
        case 44100 :
        case 32000 :
          break;
        default:
          mjpeg_error("Audio rate is %ld Hz\n",el.audio_rate);
          mjpeg_error("Audio rate must be 32000, 44100 or 48000 !\n");
          exit(1);
        }
      }

    switch (el.audio_chans) {
    case 1:
    case 2:
      break;
      
    default:
      mjpeg_error("Audio channels %d not allowed\n",el.audio_chans);
      exit(1);
    }
 
	/* Create wav header (skeleton) on stdout ... */
	wav_header( el.audio_bits, el.audio_rate, el.audio_chans, 1);
	/* skip to the start frame if specified */
	f = 0;
	if (start_frame != 0)
		f = start_frame;
	/* num_frames = -1, read em all, else read specified # */
	if ( num_frames == -1)
		num_frames = el.video_frames;

	/* Stream out audio wav-style... in per-frame chunks */
	for( ; f < num_frames; ++f )
	{
		n = el_get_audio_data((char *)audio_buff, f, &el, 0);
		if( n < 0 )
		{
			mjpeg_error_exit1( "%s: Couldn't get audio for frame %d!\n", argv[0], f );
		}
		if( n != 0 )
		{
			res = do_write( 1, audio_buff, n );
			if( res != n )
			{
				mjpeg_error_exit1( "%s: Couldn't write audio for frame %d!\n", argv[0], f );
				exit(1);
			}
		}

		else if( f < num_frames && ! warned )
		{
			mjpeg_warn( "%s: audio ends early at frame %d\n", argv[0], f );
			warned = 1;
		}
	}

	wav_close(1);
	exit(0);
}


