/*
movtar_split (V1.0)
===================

  Splits a multiplexed movtar stream with video and sound into 
  several files: 
  a sound file in .wav format, and into either a movtar without sound 
  or several JPEG files.

  Usage: movtar_split -i filename -o outprefix <options>
  (see movtar_split -h for help (or have a look at the function "usage"))
  
  Copyright (C) 1999-2000 Gernot Ziegler (gz@lysator.liu.se)

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <glib.h>

#include <movtar.h>

/* (this struct and the definition was borrowed from aplay.c by Jaroslav Kysela */
/* Definitions for the Microsoft WAVE format */
#define WAV_RIFF		0x46464952
#define WAV_WAVE		0x45564157
#define WAV_FMT			0x20746D66
#define WAV_DATA		0x61746164
#define WAV_PCM_CODE		1

typedef struct wav_header {
	gint main_chunk;	/* 'RIFF' */
	gint length;		/* filelen */
	gint chunk_type;	/* 'WAVE' */

	gint sub_chunk;	/* 'fmt ' */
	gint sc_len;		/* length of sub_chunk, =16 */
	gshort format;		/* should be 1 for PCM-code */
	gshort modus;		/* 1 Mono, 2 Stereo */
	gint sample_fq;	/* frequence of sample */
	gint byte_p_sec;
	gshort byte_p_spl;	/* samplesize; 1 or 2 bytes */
	gshort bit_p_spl;	/* 8, 12 or 16 bit */

	gint data_chunk;	/* 'data' */
	gint data_length;	/* samplecount */
} WaveHeader;

static gchar *inmovtarname   = "dummy.movtar";
static gchar *outmovtarname = "out.movtar";
static gchar *outwavname   = "out.wav";
static gchar *jpegprefix = NULL;
movtar_t *inmovtar, *outmovtar;
FILE *outwav;

/* Default values for the playback */
static int begin = 0; /* the frame start */
static size_t numframes; /* the number of frames to copy */
static unsigned long long samplestart = 0; /* the sample start */

/* number of frames played - 
 * -1 means to the end of the movie */	 
static int absframes = -1; 

WaveHeader wh;

static int verbose = 0;
static int debug = 0;

/*
 * The User Interface parts 
 */

/* usage
 * Prints a short description of the program, including default values 
 * in: prog: The name of the program 
 */
void usage(char *prog)
{
    char *h;

    if (NULL != (h = (char *)strrchr(prog,'/')))
	prog = h+1;
    
  fprintf(stderr,
	  "%s splits a multiplexed movtar stream with video and sound \n"
          "(e.g. one recorded by buz_rec) into two files:\n"
	  "a movtar without sound and a sound file in .wav format."
	  "\n"
	  "\n"
	  "usage: %s [ options ]\n"
	  "\n"
	  "options:\n"
	  "  -v          verbose operation	       \n"
	  "  -d          debug information	       \n"
	  "  -b begin    start at frame #begin       [%d]\n"
	  "  -t frames   number of frames            [%d]\n"
	  "  -i file     input file name             [%s]\n"
	  "  -o file     output file name prefix     [%s]\n"
	  "  -w prefix   generate _only_ the WAV output  [%s]\n"
	  "  -j prefix   instead of an output movtar, generate jpeg files\n"
	  "              with the given common prefix\n"
	  "\n"
	  "examples:\n"
	  "  %s -i stream.movtar -o result | split a movtar file into \n"
          "    result.movtar (w/o sound) and result.wav (the sound part)\n"
	  "  %s -i in.movtar -b 10 -t 100 | splits 100 frames from in.movtar, \n"
          "    beginning with frame 10, into out.movtar and out.wav.\n"
	  "\n"
	  "--\n"
	  "(c) 1999 Gernot Ziegler <gz@lysator.liu.se>\n",
	  prog, prog, begin, absframes, 
	  inmovtarname, "out", "out", prog, prog);
}

/* parse_commandline
 * Parses the commandline for the supplied parameters.
 * in: argc, argv: the classic commandline parameters
 */
void parse_commandline(int argc, char ** argv)
{ int c;

/* parse options */
 for (;;) {
   if (-1 == (c = getopt(argc, argv, "dvi:t:b:o:w:j:")))
     break;
   switch (c) {
   case 'v':
     verbose = 1;
     break;
   case 'd':
     debug = 1;
     break;
   case 'i':
     inmovtarname = optarg;
     break;
   case 'o':
     /* generate the according filenames from the given prefix */
     outmovtarname = g_strdup_printf("%s.movtar", optarg);
     outwavname = g_strdup_printf("%s.wav", optarg);
     break;
   case 'w':
     /* make only WAV output */
     outmovtarname = NULL;
     outwavname = g_strdup_printf("%s.wav", optarg);
     break;
   case 'j':
     /* generate JPEG files instead of an output movtar */
     outmovtarname = NULL;
     jpegprefix = g_strdup(optarg);
     break;
   case 't':
     absframes = atoi(optarg);
     break;
   case 'b':
     begin = atoi(optarg);
     break;
   case 'h':
   default:
     usage(argv[0]);
     exit(1);
   }
 }
 if (strcmp(inmovtarname, "dummy.movtar") == 0)
   { printf("%s needs an input filename to proceed.\n"
	    "See also %s -h for help.\n", argv[0], argv[0]); exit(1);
   }
}

/*
 * The file handling parts 
 */

int write_inteldw(unsigned long outlong, FILE *outwav)
{
  unsigned char out[4];
  out[0] = (outlong >> 0) & 0xff;
  out[1] = (outlong >> 8) & 0xff;
  out[2] = (outlong >> 16) & 0xff;
  out[3] = (outlong >> 24) & 0xff;
  fwrite(out, sizeof(unsigned char), 4, outwav);
}

int write_intelword(unsigned int outlong, FILE *outwav)
{
  unsigned char out[2];
  out[0] = (outlong >> 0) & 0xff;
  out[1] = (outlong >> 8) & 0xff;
  fwrite(out, sizeof(unsigned char), 2, outwav);
}

int write_wavheader(struct wav_header *wh, FILE *outwav)
{
  /* write a WAVE-header */
  write_inteldw(wh->main_chunk, outwav);
  write_inteldw(wh->length, outwav);
  write_inteldw(wh->chunk_type, outwav);
  write_inteldw(wh->sub_chunk, outwav);
  write_inteldw(wh->sc_len, outwav);
  write_intelword(wh->format, outwav);
  write_intelword(wh->modus, outwav);
  write_inteldw(wh->sample_fq, outwav);
  write_inteldw(wh->byte_p_sec, outwav);
  write_intelword(wh->byte_p_spl, outwav);
  write_intelword(wh->bit_p_spl, outwav);
  write_inteldw(wh->data_chunk, outwav);
  write_inteldw(wh->data_length, outwav);
  return 1;
} 

/* init_parse_files
 * Parses the input movtar INFO and generates the new files with
 * the according headers.
 * @param filename: movtar filename
 * @param buzdev:   UNIX file descriptor for the Buz video character device 
 * @returns      MJPEG buffer information. 
 */
int init_parse_files()
{ 
  /* open input movtar */
  inmovtar = movtar_open(inmovtarname, 1, 0, 0x0);
  if (inmovtar == NULL) { perror("Opening the input movtar file failed.\n"); exit(1); }
  
  if (debug) printf("%s, movtar_open passed.", __FUNCTION__);
  if (verbose) 
  { printf("This file (Movtar Version %s) was created by %s\n"
	   "(Content Classification: %s)\n"
	   "(Content Description: %s)\n"
	   "using the %s input, and having a quality of %d \n"
	   "TV-Norm: %s, Resolution: %dx%d, JPEG Fields per Buffer: %d\n", 
	   inmovtar->version, inmovtar->gen_author, 
	   inmovtar->gen_classification, 
	   inmovtar->gen_description, 
	   inmovtar->gen_input, inmovtar->gen_jpeg_quality,
	   movtar_norm_string(inmovtar),
	   inmovtar->mov_width, inmovtar->mov_height, 
	   inmovtar->mov_jpegnumfield);
  }

  if (!inmovtar->sound_avail) 
  { fprintf(stderr, "This movtar does not contain any sound - "
	            "splitting is meaningless.\n"); exit(1); 
  }

  if (verbose) 
    { 
      printf("Now generating output: \n");
      if (outmovtarname) printf("Video file: %s\n", outwavname); 
      else if (jpegprefix) printf("JPEG files, prefix: %s\n", jpegprefix);
      printf("Audio file: %s\n", outwavname); 
    }

  if (outmovtarname)
    {
      if (verbose) printf("Opening output file ""%s""\n", outmovtarname);
      /* create output movtar file */
      outmovtar = movtar_open(outmovtarname, 0, 1, 0x0);
      if (outmovtar == NULL) { perror("Creation of output movtar failed."); exit(1); }
    }

  /* create output wav file */
  outwav = fopen(outwavname, "w");
  if (outwav == NULL) { perror("Creation of output wav failed."); exit(1); }
  
  /* write a WAVE-header */
  wh.main_chunk = WAV_RIFF;
  wh.length = 65536 + sizeof(WaveHeader) - 8;
  wh.chunk_type = WAV_WAVE;
  wh.sub_chunk = WAV_FMT;
  wh.sc_len = 16;
  wh.format = WAV_PCM_CODE;
  wh.modus = inmovtar->sound_stereo ? 2 : 1;
  wh.sample_fq = inmovtar->sound_rate;
  wh.byte_p_spl = wh.modus * ((inmovtar->sound_size + 7) / 8);
  wh.byte_p_sec = wh.byte_p_spl * inmovtar->sound_rate;
  wh.bit_p_spl = inmovtar->sound_size;
  wh.data_chunk = WAV_DATA;
  wh.data_length = 65536; /* Preliminarily WRONG !!! see below */
#if 0  /* NOT PORTABLE ! */
  if (fwrite(&wh, sizeof(WaveHeader), 1, outwav) != 1) 
    { perror("Writing wav header failed.\n"); exit(1); }
  /* This writes the header a first time, but its size is invalid !! */
  /* The file size is not known at this time, therefore the header has to be rewritten */
  /* at the end of the conversion process -> bad header design */
#else
  //printf("Using new, portable WAV header code\n");
  write_wavheader(&wh, outwav);
#endif
 
  if (verbose) printf("File parsing complete\n");
  return TRUE;
}

/* get_new_frame_play_audio
 * Traverses through the input movtar and write all audio snippets it finds into
 * the wav file, and all other data blocks into the output movtar.
 * returns: TRUE if successful ("normal" end reached), FALSE else
 */
int split_movtar()
{
   unsigned long long read_bytes = 1;
   unsigned long long samples_left;
   unsigned char *audiobuffer; 
   unsigned char videobuffer[300000]; /* ought to be enough */
   unsigned long bytes_read;
   int n = 0;
   FILE *jpeg;
   gchar jpegname[255];   

   if (verbose) printf("Now starting split process.\n");

   if (verbose) printf("Processing audio data.\n");
   samples_left = movtar_audio_length(inmovtar) - samplestart;

   movtar_set_audio_position(inmovtar, 0); /* Reset the audio stream */
   
   audiobuffer = g_malloc(movtar_audio_bytes(inmovtar, 150000));
   while (samples_left > 0 && read_bytes != 0)
     {
       if (debug) printf("samples_left = %lld\n", samples_left);
       /* Read audio - I won't set the audio position each time since it slows down the process */
       read_bytes = movtar_read_audio(inmovtar, audiobuffer, (samples_left > 150000) ? 
					150000 : samples_left);
       /* Write the information into the WAV */
       if (debug) printf("Writing %lld bytes.\n", read_bytes);
       fwrite(audiobuffer, read_bytes, 1, outwav); 
       samples_left -= movtar_audio_samples(inmovtar, read_bytes);
     }

   if (debug) printf("Writing new WAV header.\n");
   /* reposition to beginning of wavfile, rewrite the header 
      with the new size information */
   fseek(outwav, 0, SEEK_SET);
   wh.length = movtar_audio_length(inmovtar) - samplestart + sizeof(WaveHeader) - 8;
   wh.data_length = movtar_audio_bytes(inmovtar, movtar_audio_length(inmovtar) - samplestart);
#if 0 
   if (fwrite(&wh, sizeof(WaveHeader), 1, outwav) != 1) 
     { perror("Writing wav header failed.\n"); exit(1); }   
#else
  printf("Using new, portable WAV header code\n");
  write_wavheader(&wh, outwav);
#endif

  fclose(outwav);

  if (jpegprefix || outmovtarname)
    {
      if (verbose) printf("Processing video data\n");
      
      numframes = movtar_video_length(inmovtar) - begin;
      
      for (n = begin; n < numframes; n++)
	{
	  movtar_set_video_position(inmovtar, n); /* this is not really neccessary */
	  bytes_read = movtar_read_frame(inmovtar, videobuffer);
	  if (outmovtarname) 
	    movtar_write_frame(outmovtar, videobuffer, bytes_read);
	  
	  if (jpegprefix) 
	    {
	      g_snprintf(jpegname, 255, "%s_%06d.jpeg", 
			 jpegprefix, n);
	      if (verbose) printf("%s\n", jpegname);
	      jpeg = fopen(jpegname, "w");
	      fwrite(videobuffer, 1, bytes_read, jpeg);
	      fclose(jpeg);
	    }
	}
    }
  
  movtar_close(inmovtar);
  if (outmovtarname) 
    movtar_close(outmovtar); /* This builds the output index at the same time */
  
  return TRUE;
}


/* main
 * Rather simple. Opens the input file, and starts processing.. 
 * in: argc, argv:  Classic commandline parameters. 
 * returns: int: 0: success, !0: !success :-)
 */
int main(int argc, char **argv)
{ 
  movtar_init(FALSE, FALSE);

  parse_commandline(argc, argv);

  if (verbose) printf("Opening and parsing the input file (this can take a while)\n");
  if (init_parse_files() == FALSE)
  { 
    fprintf(stderr, "* Error processing the input movtar.\n");
    exit(1);
  }
  
  if (debug) printf("Setting internal file positions\n");
  
  /* start at the given position (default: start at frame 0 <=> begin = 0) */
  movtar_set_video_position(inmovtar, begin);
  samplestart = (begin * inmovtar->sound_rate / inmovtar->mov_framerate);
  movtar_set_audio_position(inmovtar, samplestart);
      
  if (outmovtarname)
    {
      if (debug) printf("Copying movtar headers\n");
      /* Copies all the movtar information, bypassing the quicktime4linux API */
      movtar_copy_header(inmovtar, outmovtar); 
    }

  if (verbose) printf("Now splitting input file\n");
  if (split_movtar() == FALSE)
  { fprintf(stderr, "* Error processing the input movtar.\n");
    exit(1);
  }

  if (verbose) printf("Split complete.\n");
  
  return 0;
}










