/*
movtar_unify (V1.0)
===================

  Combines a movtar file without sound (or several JPEG files) 
  and a wav audio file into a movtar stream with sound.

  Simple usage: movtar_split -w wavfile -i inmovtar -o outmovtar <options> 
  (see movtar_split -h for help (or have a look at the function "usage"))
  
  Copyright (C) 1999 Gernot Ziegler (gz@lysator.liu.se)

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
#include <jpeglib.h>

#include "movtar.h"

#define MAXPIXELS (1024*1024)  /* Maximum size of final image */

static unsigned char outBuffer[MAXPIXELS*3];

/* Definitions for Microsoft WAVE format */
#define WAV_RIFF		0x46464952
#define WAV_WAVE		0x45564157
#define WAV_FMT			0x20746D66
#define WAV_DATA		0x61746164
#define WAV_PCM_CODE		1

/* it's in chunks like .voc and AMIGA iff, but my source say there
   are in only in this combination, so I combined them in one header;
   it works on all WAVE-file I have
 */
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

static gchar *inmovtarname   = NULL;
static gchar *jpegformatstr   = NULL;
static gchar *inwavname   = "dummy.wav";
static gchar *outmovtarname = "out.movtar";
static movtar_t *inmovtar, *outmovtar;
static FILE *inwav;

static gint64 wavsize; /* Data size of the wav file */
static gint64 wavoffset = 0; /* Given byte offset in the wav file */
static float framerate;
static gint32 begin = 0; /* the video frame start */
static gint32 numframes = -1; /* -1 means: take all frames */
static gint64 audio_per_frame_x100;

WaveHeader wavheader;

static int verbose = 0;
static int debug = 0;

static int recompress = 0; /* recompress the information ? */
static int omit_sound = 0; /* do not add sound */

static struct jpeg_decompress_struct dinfo;
/* static struct jpeg_compress_struct newinfo; */
static struct jpeg_error_mgr jerr;

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
	  "%s combines a movtar file without sound (or JPEG files)\n"
	  "and a wav audio file into a movtar stream with sound.\n"
	  "Automatic conversion from other JPEG formats into YUV 4:2:2 is available.\n"
	  "\n"
	  "usage: %s [ options ]\n"
	  "\n"
	  "where options are ([] shows the defaults):\n"
	  "  -v          verbose operation	       \n"
	  "  -d          debug information	       \n"
	  "  -i file     input movtar file name                 [%s]\n"
	  "  -j filedesc  a general description for the JPEG files\n"
	  "  -w file     input wav file name                    [%s]\n"
	  "  -o file     output movtar file name                [%s]\n"
	  "  -b framenr. start at given frame nr.               [0]\n"
	  "  -n numframes processes the given nr. of frames     [all]\n"
	  "  -O wavoffset start at the given offset in the WAV  [0]\n"
	  "               (-1 omits sound (for conversion))        \n"
	  "\n"
	  "examples:\n"
	  "  %s -i stream.movtar -w music.wav -o result.movtar \n"
	  "  | combines music.wav and stream.movtar (w/o sound)\n"
          "    into result.movtar (with sound)\n"
	  "  %s -j in_%%06d.jpeg -b 100000 -w music.wav -o result2.movtar\n"
          "  | combines music.wav and all the available JPEGs that match \n"
          "    in_??????.jpeg, starting with 100000 (in_100000.jpeg, \n"
          "    in_100001.jpeg, etc...) into result2.movtar (with sound)\n"
	  "  %s -i in.movtar -n 150 -w music.wav -O 8192 -o result3.movtar\n"
          "  | combines music.wav from byteoffset 8192 and onwards with the \n"
	  "    150 first video frames from in.movtar, and writes them to result3.movtar\n"
	  "  %s -j t%%01d.jpeg -O-1 -v -d -o result.movtar\n"
          "  | combines t0.jpeg - t9.jpeg without sound into result.movtar\n"
	  "    (example for implicit JPEG conversion)\n"
	  "\n"
	  "--\n"
	  "(c) 2000 Gernot Ziegler <gz@lysator.liu.se>\n",
	  prog, prog, inmovtarname, inwavname, outmovtarname, prog, prog, prog);
}

/* parse_commandline
 * Parses the commandline for the supplied parameters.
 * in: argc, argv: the classic commandline parameters
 */
void parse_commandline(int argc, char ** argv)
{ int c;

/* parse options */
 for (;;) {
   if (-1 == (c = getopt(argc, argv, "dhvi:w:o:O:b:j:n:")))
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
   case 'j':
     jpegformatstr = optarg;
     inmovtarname = NULL;
     break;
   case 'b':
     begin = atol(optarg);
     break;
   case 'n':
     numframes = atol(optarg);
     break;
   case 'O':
     wavoffset = atol(optarg);
     if (wavoffset == -1) omit_sound = 1;
     break;
   case 'o':
     outmovtarname = optarg;
     break;
   case 'w':
     inwavname = optarg;
     break;
   case 'h':
   default:
     usage(argv[0]);
     exit(1);
   }
 }
 if (inmovtarname == NULL && jpegformatstr == NULL)
   { printf("%s needs an input filename to proceed.\n\n", argv[0]); 
     usage(argv[0]); 
     exit(1);
   }
}

/*
 * The file handling parts 
 */

/*
 * test, if it's a .WAV file, 0 if ok (and set the speed, stereo etc.)
 *                            < 0 if not
 */
static int test_wavefile(void *buffer)
{
  WaveHeader *wp = buffer;
  
  if (wp->main_chunk == WAV_RIFF && wp->chunk_type == WAV_WAVE &&
      wp->sub_chunk == WAV_FMT && wp->data_chunk == WAV_DATA) 
    {
      if (wp->format != WAV_PCM_CODE) 
	{
	  fprintf(stderr, "Can't process other than PCM-coded WAVE-files\n"); exit(1);
	}
      if (wp->modus < 1 || wp->modus > 2) 
	{
	  fprintf(stderr, "Can't process WAVE-files with %d tracks\n", wp->modus); exit(1);
	}
      outmovtar->sound_avail = TRUE;
      outmovtar->sound_stereo = (wp->modus == 2) ? TRUE : FALSE;
      outmovtar->sound_size = wp->bit_p_spl;
      outmovtar->sound_rate = wp->sample_fq;
      wavsize = wp->data_length;
      return TRUE;
    }
  return FALSE;
}

/* init_parse_files
 * Parses the input movtar INFO and generates the new files with
 * the according headers.
 * in: filename: movtar filename
 *     buzdev:   UNIX file descriptor for the Buz video character device 
 * out:vb:       MJPEG buffer information. 
 */
int init_parse_files()
{ 
  gchar jpegname[255];
  FILE *jpegfile;

  if (inmovtarname)
    {
      /* open input movtarfile */
      inmovtar = movtar_open(inmovtarname, 1, 0, 0x0);
      if (inmovtar == NULL) { perror("Opening the input movtar file failed.\n"); exit(1); }
      
      if (verbose) 
	{ 
	  printf("This file (Movtar Version %s) was created by %s\n"
		 "(Content Classification: %s)\n"
		 "(Content Description: %s)\n"
		 "using the %s input, and having a quality of %d %%\n"
		 "TV-Norm: %s, Resolution: %dx%d, JPEG Fields per Buffer: %d\n", 
		 inmovtar->version, inmovtar->gen_author, 
		 inmovtar->gen_classification, 
		 inmovtar->gen_description, 
		 inmovtar->gen_input, inmovtar->gen_jpeg_quality,
		 movtar_norm_string(inmovtar),
		 inmovtar->mov_width, inmovtar->mov_height, 
		 inmovtar->mov_jpegnumfield);
	}

      if (inmovtar->sound_avail) 
	{ fprintf(stderr, "The input movtar does already have sound - can't unify.\n"); exit(1); }
    }
  else
    {
      
    }

  /* create output movtar file */
  if (verbose) printf("Creating output movtar.\n");
  outmovtar = movtar_open(outmovtarname, 0, 1, 0x0);
  if (outmovtar == NULL) { perror("Creation of output movtar failed."); exit(1); }

  if (inmovtarname)
    {
      /* Copy all information from the input movtar, but: with sound !*/
      /* I'm changing the internal inmovtar struct, I shouldn't do that, but hey:
	 I'm lazy, and I am the creator of the movtar lib - just trust me ;) */
      movtar_copy_header(inmovtar, outmovtar);

      /* Prepare for the calculations in unify_movtar, see below */
      framerate = movtar_frame_rate(inmovtar);
      outmovtar->mov_framerate = framerate;
    }
  else
    {
      g_snprintf(jpegname, 254, jpegformatstr, begin);
      if (debug) printf("Analyzing %s to get the right pic params\n", jpegname);
      jpegfile = fopen(jpegname, "r");
      if (jpegfile == NULL)
	{
	  printf("While opening: \"%s\"\n", jpegname);
	  perror("Fatal: Couldn't open first given JPEG frame"); exit(1);
	}
      else
	{
	  /* Now open this JPEG file, and examine its header to retrieve the 
	     movtar info that shall be written */
	  dinfo.err = jpeg_std_error(&jerr);
	  jpeg_create_decompress(&dinfo);
	  jpeg_stdio_src(&dinfo, jpegfile);
	  
	  jpeg_read_header(&dinfo, TRUE);
	  dinfo.out_color_space = JCS_YCbCr;      
	  jpeg_start_decompress(&dinfo);

	  if(dinfo.output_components != 3)
	    {
	      printf("Output components of JPEG image = %d, must be 3\n",dinfo.output_components);
	      exit(1);
	    }

	  /* Check if the input JPEG YUV factors fit the MJPEG codec of the Buz */
	  /* They determine the sample ratio of the YUV input (here: 2:1:1, or also 4:2:2) */
	  if (dinfo.comp_info[0].h_samp_factor != 2 || 
	      dinfo.comp_info[0].v_samp_factor != 1 || 
	      dinfo.comp_info[1].h_samp_factor != 1 || 
	      dinfo.comp_info[1].v_samp_factor != 1 || 
	      dinfo.comp_info[2].h_samp_factor != 1 ||
	      dinfo.comp_info[2].v_samp_factor != 1)
	    {
	      printf("The YUV input ratio is not correct (not YUV 4:2:2)\n"
		     " -> automatic conversion activated\n");
	      recompress = 1;
	    }

	  framerate = 12.5; /* assuming some strange frame rate */

	  if (dinfo.output_width == 720 || dinfo.output_width == 352 || dinfo.output_width == 176)
	    {
	      outmovtar->mov_norm = MOVTAR_NORM_PAL;
	      framerate = 25.0; /* We are guessing this to be PAL */
	    }

	  if (dinfo.output_width == 640 || dinfo.output_width == 320 || dinfo.output_width == 160)
	    {
	      outmovtar->mov_norm = MOVTAR_NORM_NTSC; 
	      framerate = 29.97; /* We are guessing this to be NTSC */
	    }

	  /* SECAM ? No clue, contact me if you have info about possible resolutions ! */

	  /* Setting the vital movtar parameters, the last parameters guesses interlaced */
	  movtar_set_video(outmovtar, 1, dinfo.output_width, dinfo.output_height, 
			   framerate, "MJPG", (dinfo.output_width > 360));
	  fprintf(stderr, "Image dimensions are %dx%d, framerate = %f frames/s, chosen norm: %s\n" 
                          "YUV-convert: %s\n", 
		  dinfo.output_width, dinfo.output_height, framerate, movtar_norm_string(outmovtar),
		  recompress ? "yes" : "no");
	  jpeg_destroy_decompress(&dinfo);
	  fclose(jpegfile);
	}
    }

  if (!omit_sound)
    {
      /* At this point all the outmovtar variables must be set, except for the sound information */
      /* open input wavfile */
      inwav = fopen(inwavname, "r");
      if (inwav == NULL) { perror("Opening the input wav file failed.\n"); exit(1); }
      
      if (fread(&wavheader, 1, sizeof(WaveHeader), inwav) != sizeof(WaveHeader))
	{ perror("Error parsing the WAV file header.\n"); exit(1); }
      if (test_wavefile(&wavheader) == FALSE)
	{ perror("This is no wav file."); exit(1); };
      
      /* position to the needed wav offset in the file */
      fseek(inwav, wavoffset, SEEK_CUR);
      
      if (verbose) 
	{ printf("The wav audio file %s contains the following parameters\n"
		 "Audio rate: %d Hz, Sample size: %d bits, Stereo: %s\n",
		 inwavname, outmovtar->sound_rate, outmovtar->sound_size, 
		 (outmovtar->sound_stereo == TRUE) ? "yes" : "no");
	}
    }
  else
    {
      if (verbose) printf("(Sound omitted)\n");
      wavsize = 0;
    }

  if (verbose) printf("movie frame rate is: %f frames/s\n", framerate);
  audio_per_frame_x100 = (100 * movtar_sample_rate(outmovtar) * movtar_audio_bits(outmovtar) / 8 * 
		    movtar_track_channels(outmovtar)) / framerate;

  return TRUE;
}

/*
How to get the exact number of audio snippets per frame (sync !!):

Calculations:
25 frames per second (for PAL, NTSC has ~30). x bytes per second for some audio.
x/25 is the exact number of audio bytes per frame.

So:
Add every time a video frame is written these x/25 bytes to some fake
"audio buffer size". If the bytes behind the decimal point accumulate
to a whole byte, add it to the audiobuffer (and reduce the accumulated sum).
*/

int jpeg_recompress(FILE *injpeg, FILE *outfile)
{
  int img_height, img_width, field = 0, y, ystart;
  unsigned char *addr;
  struct jpeg_decompress_struct dinfo;
  struct jpeg_compress_struct newinfo;
  struct jpeg_error_mgr jerr;
  
  dinfo.err = jpeg_std_error(&jerr);
  jpeg_create_decompress(&dinfo);
  jpeg_stdio_src(&dinfo, injpeg);

  jpeg_read_header(&dinfo, TRUE);
  dinfo.out_color_space = JCS_YCbCr;      
  jpeg_start_decompress(&dinfo);
  
  if(dinfo.output_components != 3)
    {
      fprintf(stderr,"Output components of JPEG image = %d, must be 3\n",dinfo.output_components);
      exit(1);
    }
  
  img_width  = dinfo.output_width;
  img_height = dinfo.output_height;
  
  for (y=0; y<img_height; y++)
    {
      addr = outBuffer+y*img_width*3;
      jpeg_read_scanlines(&dinfo, (JSAMPARRAY) &addr, 1);
    }
   
  (void) jpeg_finish_decompress(&dinfo);
  fprintf(stderr,"JPEG file successfully read.\n",field);
   
  /* Decompressing finished */
  jpeg_destroy_decompress(&dinfo);
  
  /* Starting the compression */
  newinfo.err = jpeg_std_error(&jerr);
  jpeg_create_compress(&newinfo);
  jpeg_stdio_dest(&newinfo, outfile);

  newinfo.in_color_space = JCS_YCbCr;      
  jpeg_set_defaults(&newinfo);
  
  newinfo.image_width = img_width;
  newinfo.input_components = 3;
  newinfo.input_gamma = 1.0;
  
  /* Set the output JPEG factors fitting the MJPEG codec of the Buz */
  /* These sets the sample ratio of the YUV output (here: 2:1:1) */
  newinfo.comp_info[0].h_samp_factor = 2; 
  newinfo.comp_info[0].v_samp_factor = 1; 
  newinfo.comp_info[1].h_samp_factor = 1; 
  newinfo.comp_info[1].v_samp_factor = 1; 
  newinfo.comp_info[2].h_samp_factor = 1; 
  newinfo.comp_info[2].v_samp_factor = 1; 
  
  if (img_width < 630)
    { 
      newinfo.image_height = img_height;
      
      jpeg_start_compress(&newinfo, FALSE);
      
      for (y=0; y<img_height; y++)
	{
	  addr = outBuffer+y*img_width*3;
	  jpeg_write_scanlines(&newinfo, (JSAMPARRAY) &addr, 1);
	}
      
      jpeg_finish_compress(&newinfo);
      fprintf(stderr, "1 field written.\n");
    }
  else
    {
      newinfo.image_height = img_height/2;
      
      jpeg_start_compress(&newinfo, FALSE);
      
      ystart = field;
      for (y=0; y<img_height; y+=2)
	{
	  addr = outBuffer+y*img_width*3;
	  jpeg_write_scanlines(&newinfo, (JSAMPARRAY) &addr, 1);
	}
      
      jpeg_finish_compress(&newinfo);
      
      jpeg_start_compress(&newinfo, FALSE);
      
      for (y=1; y<img_height; y+=2)
	{
	  addr = outBuffer+y*img_width*3;
	  jpeg_write_scanlines(&newinfo, (JSAMPARRAY) &addr, 1);
	}
      
      jpeg_finish_compress(&newinfo);
      fprintf(stderr, "2 fields written.\n");    
    }
  
   jpeg_destroy_compress(&newinfo);
}



int unify_movtar()
{
  int newpicsavail = 1;
  gint64 read_bytes, read_video_bytes = 0;
  gint64 bytes_to_read;
  int audio_filled_100 = 0;
  gint64 audio_margin = audio_per_frame_x100/100;
  gchar *audiobuffer;
  guchar videobuffer[300000]; /* ought to be enough */
  gint32 vid_index = begin;
  gchar jpegname[255];
  FILE *jpegfile;

  if (verbose) printf("Now starting unify process.\n");
  if (debug) printf("Allocating %lld bytes\n", audio_per_frame_x100/50);
  audiobuffer = malloc(audio_per_frame_x100/50);

  if (debug) printf("input wavsize is %lld\n", wavsize);

  while (newpicsavail)
    {
      if (!omit_sound)
	if (!feof(inwav)) break;

      if (wavsize == 0)
	printf("Warning: No WAV file data left for frame %d\n", vid_index);
      else
	{
	  bytes_to_read = audio_margin + audio_filled_100/100; 
	  bytes_to_read = ((bytes_to_read + 1)/2 * 2); /* round it to an even value */
	  if (wavsize >= bytes_to_read)
	    { 
	      // if (debug) printf("Inwav pos: %ld", ftell(inwav));
	      read_bytes = fread(audiobuffer, 1, bytes_to_read, inwav); 
	      if (read_bytes != bytes_to_read)
		perror("Warning: Read from wav file failed. (non-critical)\n"); 
	      wavsize -= audio_margin;
	    }  
	  else
	    { 
	      read_bytes = fread(audiobuffer, 1, wavsize, inwav);
	      if (read_bytes != wavsize)
		perror("Warning: Read from wav file failed. (non-critical)\n"); 
	      wavsize = 0;
	    }
	  
	  audio_filled_100 = audio_filled_100 % 200;
	  audio_filled_100 += audio_margin;
	  if (debug) printf("Read %lld audio bytes.\n", read_bytes);
	  movtar_write_audio(outmovtar, audiobuffer, movtar_audio_samples(outmovtar, read_bytes));
	}

      if (inmovtarname)
	{
	  movtar_set_video_position(inmovtar, vid_index); 
	  read_video_bytes = movtar_read_frame(inmovtar, videobuffer);
	}
      else /* assuming JPEG input */
	{
	  g_snprintf(jpegname, 254, jpegformatstr, vid_index);
	  jpegfile = fopen(jpegname, "r");
	  if (jpegfile == NULL)
	    { 
	      if (verbose) printf("While opening %s:\n", jpegname);

	      if (!recompress)
		{
		  if (verbose) perror("Warning: Read from jpeg file failed (non-critical)");
		  if (verbose) printf("(Rewriting latest frame, size %lld, instead)\n", read_video_bytes);
		}
	      else
		{
		  if (verbose) printf("Last frame encountered (%s invalid). Stop.\n", jpegname);
		  newpicsavail = 0;
		}
	    } 
	  else
	    {
	      if (!recompress)
		{
		  read_video_bytes = fread(videobuffer, 1, 300000, jpegfile);
		  if (verbose) printf("Got %s, size %lld\n", jpegname, read_video_bytes); 
		  fclose(jpegfile);
		}
	      else
		{
		  if (debug) printf("Preparing frame\n");
		  movtar_write_frame_init(outmovtar);
		  if (debug) printf("Recompressing frame\n");
		  jpeg_recompress(jpegfile, movtar_get_fd(outmovtar));
		  if (debug) printf("Finishing frame\n");
		  movtar_write_frame_end(outmovtar);
		}
	    }	 
	 }

      if (!recompress)
	{
	  /* The video data should now be in videobuffer, read_bytes contains the read bytes */      
	  if (read_video_bytes != 0)
	    movtar_write_frame(outmovtar, videobuffer, read_video_bytes); /* write it out */
	}

      vid_index++; /* reached the end of the file, or the desired number of frames ? */
      if ((inmovtarname && vid_index == movtar_video_length(inmovtar)) || 
	  (numframes != -1 && vid_index == numframes + begin))
	newpicsavail = 0;
    }

  g_free(audiobuffer);

  return TRUE;
}

/* main
 * Rather simple. Opens the video device, and then calls all the subfunctions in a row. 
 * in: argc, argv:  Classic commandline parameters. 
 * returns: int: 0: success, !0: !success :-)
 */
int main(int argc, char ** argv)
{ 
  movtar_init(FALSE, FALSE);

  parse_commandline(argc, argv);

  if (debug) printf("Parsing & checking input files.\n");
  if (init_parse_files() == FALSE)
  { fprintf(stderr, "* Error processing the input movtar/audio file.\n"); exit(1); }

  if (debug) printf("Now unifying.\n");
  if (unify_movtar() == FALSE)
  { fprintf(stderr, "* Error processing the input files.\n");
    exit(1);
  }

  if (!omit_sound) fclose(inwav);
  if (inmovtarname) movtar_close(inmovtar); 
  movtar_close(outmovtar);

  return 0;
}










