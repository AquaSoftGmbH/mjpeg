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
 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "lav_io.h"
#include "editlist.h"
#include "mjpeg_logging.h"
#include "lav2wav.h"

struct wave_header wave;
int verbose = 1;
int big_endian; /* The endian detection */
static uint8_t audio_buff[2*256*1024]; /* Enough for 1fps, 48kHz ... */
int silence_sr, silence_bs, silence_ch ; 
EditList el;

int wav_header( unsigned int bits, unsigned int rate, unsigned int channels, int fd );
void Usage(char *str);

/* Raw write does *not* guarantee to write the entire buffer load if it
   becomes possible to do so.  This does...  */
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

uint32_t reorder_32(uint32_t todo)
{
unsigned char b0, b1, b2, b3;
uint32_t reversed;

b0 = (todo & 0x000000FF);
b1 = (todo & 0x0000FF00) >> 8;
b2 = (todo & 0x00FF0000) >> 16;
b3 = (todo & 0xFF000000) >> 24;

reversed = (b0 << 24) + (b1 << 16) + (b2 << 8) +b3;
return reversed;
}

int wav_header( unsigned int bits, unsigned int rate, unsigned int channels, int fd )
{
	uint16_t temp_16; /* temp variables for swapping the bytes */
	uint32_t temp_32; /* temp variables for swapping the bytes */
        unsigned int dummy_size = 0x7fffff00+sizeof(wave);

	/* Write out a ZEROD wave header first */
	memset(&wave, 0, sizeof(wave));

	strncpy((char*)wave.riff.id, "RIFF", 4);
	strncpy((char*)wave.riff.wave_id, "WAVE",4);
	dummy_size -=8;
        if (big_endian == 0)
	  wave.riff.len =  dummy_size;
	else 
	  wave.riff.len = reorder_32((uint32_t)dummy_size);

	strncpy((char*)wave.format.id, "fmt ",4);
        if (big_endian == 0)
	  wave.format.len = sizeof(struct common_struct);
        else
          wave.format.len = reorder_32((uint32_t)sizeof(struct common_struct));

	/* Store information */
        if (big_endian == 0) 
          {
             wave.common.wFormatTag = WAVE_FORMAT_PCM;
             wave.common.wChannels = channels;
             wave.common.dwSamplesPerSec = rate;
             wave.common.dwAvgBytesPerSec = channels*rate*bits/8;
             wave.common.wBlockAlign = channels*bits/8;
             wave.common.wBitsPerSample = bits;
          }
        else  /* We have a big endian machine and have to swapp the bytes */
          {
             temp_16 = WAVE_FORMAT_PCM;
             swab(&temp_16, &wave.common.wFormatTag,2);
             temp_16 = channels;
             swab(&temp_16, &wave.common.wChannels, 2);
             temp_32 = rate;
             wave.common.dwSamplesPerSec = reorder_32 (temp_32);
             temp_32 = channels*rate*bits/8;
             wave.common.dwAvgBytesPerSec = reorder_32 (temp_32);
             temp_16 = channels*bits/8;
             swab(&temp_16, &wave.common.wBlockAlign, 2);
             temp_16 = bits;
             swab(&temp_16, &wave.common.wBitsPerSample, 2);
          }

	strncpy((char*)wave.data.id, "data",4);
	dummy_size -= 20+sizeof(struct common_struct);

        if (big_endian == 0) 
	    wave.data.len = dummy_size;
        else
            wave.data.len = reorder_32 ((uint32_t)dummy_size);
	if (do_write(fd, &wave, sizeof(wave)) != sizeof(wave)) 
	{
		return 1;
	}
	return 0;
}

static void wav_close(int fd)
{
        unsigned int size;

	/* Find how long our file is in total, including header */
	size = lseek(fd, 0, SEEK_CUR);

	if (size < 0 ) 
	{
		if( fd > 2 )
		{
			mjpeg_error("lseek failed - wav-header is corrupt");
		}
		goto EXIT;
	}

	/* Rewind file */
	if (lseek(fd, 0, SEEK_SET) < 0) 
	{
		mjpeg_error("rewind failed - wav-header is corrupt");
		goto EXIT;
	}
	mjpeg_debug("Writing WAV header");

	/* Fill out our wav-header with some information.  */
	size -= 8;
        if (big_endian == 0)
	  wave.riff.len = size;
        else
          wave.riff.len = reorder_32 ((uint32_t)size);

	size -= 20+sizeof(struct common_struct);
        if (big_endian == 0)
	  wave.data.len = size;
	else
	  wave.data.len = reorder_32 ((uint32_t)size);

	if (do_write(fd, &wave, sizeof(wave)) < sizeof(wave)) 
	{
		mjpeg_error("wav-header write failed -- file is corrupt");
		goto EXIT;
	}
	mjpeg_info("WAV done");

EXIT:
	close(fd);
}

/* The typical help output */
void Usage(char *str)
{
   fprintf(stderr, "Usage: %s [options] inputfiles\n",str);
   fprintf(stderr, "where options are:\n");
   fprintf(stderr, "-o num        Start extracting at video frame (num)\n");
   fprintf(stderr, "-f num        Extract (num) frames of audio\n");
   fprintf(stderr, "-s/-c         Backwards compatibility options for -o/-f\n");
   fprintf(stderr, "-v num        verbose level [0..2]\n");
   fprintf(stderr, "-I            Ignore unsupported bitrates/bits per sample\n");
   fprintf(stderr, "-r sr,bs,ch   If the file does not contain any sound generate silence\n");
   fprintf(stderr, "              sr samplerate 32000,44100,48000 bs bytesize 8,16, ch channes 1/2\n");
   fprintf(stderr, "-R            The same as -r but now a silence with 44100, 16, 2 is created\n");
 
   exit(0);
}

/* Setting the type of silence, samplerate, bitesize, chanels */
void set_silence (char *optarg)
{
unsigned int u1, u2, u3; 
int i;

i = (sscanf (optarg, "%i,%i,%i", &u1, &u2, &u3));

if (i != 3) 
  {
    mjpeg_error("Wrong number of arguments given to the -r option");
    exit(1);
  }
else /*Setting user chosen values */
  {
    if ( (u1 == 32000) || (u1 == 44100) || (u1 == 48000) )
      silence_sr = u1;
    else {
      mjpeg_error("Wrong sampelrate use: 32000,44100,48000, exiting");
      exit(1); }
   
    if ( (u2 == 8) || (u2 == 16) )
      silence_bs = u2;
    else {
      mjpeg_error("Wrong audio bitsize use 8 or 16, exiting");
      exit(1); }

    if ( (u3 == 1) || (u3 == 2) )
      silence_ch = u3;
    else {
      mjpeg_error("Wrong number of chanels use 1 or 2, exiting");
      exit(1); }
  }

mjpeg_info("Setting silence to %i sampelrate, %i bitsize, %i chanels",
     silence_sr, silence_bs, silence_ch);

}

int
main(argc, argv)
int     argc;
char    **argv;
{
	int n,f,i;
	int res;
	int warned = 0;
	int start_frame = 0;
	int num_frames = -1;
        int ignore_bitrate = 0;
	int fragments;
        unsigned int fred;
        char *pfred;
   
silence_sr=0; /* silence_sr is use for detecting if the -r option is used */
silence_bs=0; silence_ch=0; 

    while( (n=getopt(argc,argv,"v:s:r:Rc:o:f:I")) != EOF)
    {
        switch(n) {

	   case 'v':
		verbose = atoi(optarg);
		if( verbose < 0 || verbose > 2 )
			Usage(argv[0]);
		break;
           case 'o':
	   case 's':
		start_frame = atoi(optarg);
		break;
	   case 'r':
                set_silence(optarg);
                break;
	   case 'R':
		silence_sr=44100;
		silence_bs=16;
		silence_ch=2;
                break;
           case 'f':
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

      /* The endiat detection copied from mp2enc */
      fred = 2 | (1 << (sizeof(int)*8-8));
      pfred = (char *)&fred;

      if(*pfred == 1)
      {
         big_endian = 1;
         mjpeg_info("System is big endian");
      }
      else if(*pfred == 2)
      {
         big_endian = 0;
                 mjpeg_info("System is little endian");
      }
      else
      {
         mjpeg_error("Can not determine if system is big/lttle endian");
         mjpeg_error_exit1("Are you running on a Cray - or what?");
      }

    /* Open editlist */
	if( argc-optind < 1)
		Usage(argv[0]);

	(void)mjpeg_default_handler_verbosity(verbose);

    read_video_files(argv + optind, argc - optind, &el,0);

    if(!el.has_audio)
    {
      if (silence_sr == 0) 
        {   
          mjpeg_error("Input file(s) have no audio, use the -r option to generate silence"); 
          exit(1);
        }
     else
        {

  mjpeg_info("mjpeg_framerate %f ", el.video_fps);  

	/* Create wav header (skeleton) on stdout ... */
        /* audio_bits, audio_rate, audio_chans */
	wav_header( silence_bs, silence_sr, silence_ch, 1);
	/* skip to the start frame if specified */
        f = 0;
	if (start_frame != 0)
		f = start_frame;
	/* num_frames = -1, read em all, else read specified # */
	if ( num_frames == -1)
		num_frames = el.video_frames;

	fragments = silence_sr / el.video_fps;
       
	/* Here is a dirty hack for the number of fragments we put out 
	   I dont know a other way how to put out a certain number of \x00
           So we put always 4bytes of 00h out and reduce the number of 
	   fragments. Works perfect for PAL framerate. But in NTSC there is 
	   a round problem. At 44k1Hz, 16bs 2ch 0,47 samples/frame
           That means that we have on second offset per hour of film  */
        if (silence_bs == 8)
          fragments = fragments / 2;
        if (silence_ch == 1)
          fragments = fragments / 2;

	/* Stream out audio wav-style... in per-frame chunks */
	for( ; f < num_frames; ++f )
	  {
	    for ( i = 0; i < fragments ; i++) 
	 	do_write (1, "\x0000000", 4);
	  }

        wav_close(1);
        exit(0);
        }
    }

    if (!ignore_bitrate) 
      {
        if(el.audio_bits!=16)
          {
		  mjpeg_error("Input file(s) must have 16 bit audio!");
		  exit(1);
	  }

	switch (el.audio_rate) {
		case 48000 : 
		case 44100 :
		case 32000 :
			break;
		default:
			mjpeg_error("Audio rate is %ld Hz",el.audio_rate);
			mjpeg_error("Audio rate must be 32000, 44100 or 48000 !");
			exit(1);
	}
      }

    switch (el.audio_chans) {
	    case 1:
	    case 2:
		    break;

	    default:
		    mjpeg_error("Audio channels %d not allowed",el.audio_chans);
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
	    n = el_get_audio_data(audio_buff, f, &el, 0);
	    if( n < 0 )
	    {
		    mjpeg_error_exit1( "%s: Couldn't get audio for frame %d!", argv[0], f );
	    }
	    if( n != 0 )
	    {
		    res = do_write( 1, audio_buff, n );
		    if( res != n )
		    {
			    mjpeg_error_exit1( "%s: Couldn't write audio for frame %d!", argv[0], f );
			    exit(1);
		    }
	    }

	    else if( f < num_frames && ! warned )
	    {
		    mjpeg_warn( "%s: audio ends early at frame %d", argv[0], f );
		    warned = 1;
	    }
    }

    wav_close(1);
    exit(0);
}


