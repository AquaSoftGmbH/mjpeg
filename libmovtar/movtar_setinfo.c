/*
movtar_setinfo (V1.0)
=====================

  Sets (and dumps) the movtar header definition in the movtar file. 

  Usage: movtar_setinfo -i filename <options>
  (see movtar_info -h for help (or have a look at the function "usage"))

  Copyright (C) 2000 Gernot Ziegler (gz@lysator.liu.se)

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

static gchar *movtarname   = NULL;
static movtar_t *movtar;

static int verbose = 0;
static int debug = 0;
static int write_info = 0;

/* These are the variables I want to change in the INFO - feel free to add some (from movtar.h) */
/* Be aware that you can do all the changes manually, too - just unpack the movtar, 
   edit the INFO text file, and pack it back as the first file of a movtar */
static char *gen_author = NULL; /* The author of the file */
static char *gen_date = NULL;   /* The generation date */
static char *gen_software = NULL; /* The used software for creation */
static int gen_jpeg_quality = 0;  /* it's the JPEG quality ! */
static char *gen_device = NULL;   /* The creating video device */
static char *gen_input = NULL;    /* The used video input */
static char *gen_classification = NULL;  /* A general classification of the movtar content */
static char *gen_description = NULL;     /* A longer description text */

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
	  "%s changes (or dumps) the editable parts in the header information\n"
          "of a movtar file (the textfile called INFO):\n"
	  "if you don't pass an editing parameter, the movtar header will simply be dumped.\n"
	  "\n"
	  "\n"
	  "usage: %s -i filename [ options ]\n"
	  "\n"
	  "options:\n"
          "  -i          the movtar file name\n"
	  "  -v          verbose operation	       \n"
	  "  -d          debug information	       \n"
	  "  -A string   the author of the file\n"
	  "  -D string   the date of creation\n"
	  "  -S string   the software used for creation\n"
	  "  -J num      the JPEG quality\n"
	  "  -G string   the creating video device\n"
	  "  -I string   the used video input\n"
	  "  -C string   A general classification of the movtar content\n"
	  "  -T string   A longer descriptional text\n"
	  "\n"
	  "examples:\n"
	  "  %s -i stream.movtar -v\n"
	  "  | dump the header of stream.movtar in verbose operation (explanation & INFO content)\n"
	  "  %s -D \"13:40:33, Jan 1995\" -i stream.movtar\n"
	  "  | set the date of recording of stream.movtar to the given string\n"
	  "  %s -i stream.movtar\n"
	  "  | dumps the video index (and audio index, if it exists) of the movtar\n"
	  "\n"
	  "--\n"
	  "(c) 2000 Gernot Ziegler <gz@lysator.liu.se>\n",
	  prog, prog, prog, prog, prog);
}

/* parse_commandline
 * Parses the commandline for the supplied parameters.
 * in: argc, argv: the classic commandline parameters
 */
void parse_commandline(int argc, char ** argv)
{ int c;

/* parse options */
 for (;;) {
   if (-1 == (c = getopt(argc, argv, "dvi:A:D:S:J:G:I:C:T:")))
     break;
   switch (c) {
   case 'v':
     verbose = 1;
     break;
   case 'd':
     debug = 1;
     break;
   case 'i':
     movtarname = optarg;
     break;
   case 'A':
     gen_author = optarg; write_info = 1;
     break;
   case 'D':
     gen_date = optarg; write_info = 1;
     break;
   case 'S':
     gen_software = optarg; write_info = 1;
     break;
   case 'J':
     gen_jpeg_quality = atoi(optarg); write_info = 1;
     break;
   case 'G':
     gen_device = optarg; write_info = 1;
     break;
   case 'I':
     gen_input = optarg; write_info = 1;
     break;
   case 'C':
     gen_classification = optarg; write_info = 1;
     break;
   case 'T':
     gen_description = optarg; write_info = 1;
     break;
   case 'h':
   default:
     usage(argv[0]);
     exit(1);
   }
 }
 if (movtarname == NULL)
   { printf("%s needs an input filename to proceed.\n"
	    "See also %s -h for help.\n", argv[0], argv[0]); exit(1);
   }
}

/* main
 * Rather simple. Opens the input file, and starts processing.. 
 * in: argc, argv:  Classic commandline parameters. 
 * returns: int: 0: success, !0: !success :-)
 */
int main(int argc, char **argv)
{ 
  struct tarinfotype info_tarheader;
  gchar *old_info_string;
  gchar *new_info_string;
  long datasize;

  movtar_init(FALSE, FALSE);

  parse_commandline(argc, argv);

  if (!movtar_check_sig(movtarname))
    { 
      fprintf(stderr, "\"%s\" doesn't seem to be a movtar file\n", movtarname); exit(1);
    }

  if (write_info)
    {
      movtar = g_malloc(sizeof(movtar_t));
      movtar->file = fopen(movtarname, "r+");
      
      /* read header data from the INFO file */
      datasize = tar_read_tarblock(movtar->file, &info_tarheader);
      old_info_string = malloc(datasize);
      tar_read_data(movtar->file, old_info_string, datasize);
      movtar_parse_info(movtar,old_info_string);
      
      if (gen_author != NULL) movtar->gen_author  = gen_author;
      if (gen_date != NULL) movtar->gen_date  = gen_date;
      if (gen_software != NULL) movtar->gen_software  = gen_software;
      if (gen_jpeg_quality != 0) movtar->gen_jpeg_quality  = gen_jpeg_quality;
      if (gen_device != NULL) movtar->gen_device  = gen_device;
      if (gen_input != NULL) movtar->gen_input = gen_input;
      if (gen_classification != NULL) movtar->gen_classification  = gen_classification;
      if (gen_description != NULL) movtar->gen_description = gen_description;

      new_info_string = movtar_create_info_content(movtar);
      if (strlen(new_info_string) > datasize)
	{
	  fprintf(stderr, "The new INFO file header (size %d bytes) is larger "
                  "than the old one (size %ld).\nSorry, can't write a new header.\n"
		  "Abbreviate the new text or unpack and repack the movtar instead.\n", 
		  strlen(new_info_string), datasize);
	}
      else
	{
	  fseek(movtar->file, 512, SEEK_SET);
	  /* Simply _overwrite_ the old INFO content, it must fit in */
	  fwrite(new_info_string, strlen(new_info_string), 1, movtar->file);
	  if (verbose) printf("new INFO file/movtar header written.\n");
	}
  
      fclose(movtar->file);
    }
  else
    {
      movtar = movtar_open(movtarname, 1, 0, MOVTAR_FLAG_NO_INDEXBUILD);

      printf("This file was created by: %s\n"
	     "Time of creation: %s\n"
	     "Content Classification: %s\n"
	     "Content Description: %s\n"
	     "Used input: %s\n" 
	     "TV-Norm: %s\n"
	     "Resolution: %dx%d\n" 
	     "JPEG quality: %d %% \n"
	     "JPEG Fields per Buffer: %d\n"
	     "RTJPEG mode: %d\n"
	     "(movtar format version %s)\n", 
	     movtar->gen_author, movtar->gen_date, 
	     movtar->gen_classification, movtar->gen_description, 
	     movtar->gen_input, movtar_norm_string(movtar),
	     movtar->mov_width, movtar->mov_height, 
	     movtar->gen_jpeg_quality,
	     movtar->mov_jpegnumfield,
	     movtar->rtjpeg_mode,
	     movtar->version);

      if (movtar->sound_avail)
	{
	  printf("Audio content: Sample rate: %d Hz, Sample size: %d bits, Stereo: %s\n",
		 movtar->sound_rate, movtar->sound_size, 
		 (movtar->sound_stereo == TRUE) ? "yes" : "no");
	}
      else
	  printf("Audio content: None available.\n");

      if (movtar->index_avail)
	{
	  if (movtar->audio_index_pos)
	    printf("Audio index: %d elements available at file pos %lld\n",
		 movtar->audio_table->len, movtar->audio_index_pos);
          else
            printf("Audio index: None available.\n");

	  if (movtar->video_index_pos)
	    printf("Video index: %d elements available at file pos %lld\n",
		 movtar->video_table->len, movtar->video_index_pos);
          else
	    /* Remark: index_available = TRUE && video_index none available must never happen ! */
            printf("Video index: None available. (SHOULD NOT BE HAPPENING ! CONTACT AUTHOR)\n");
	}
      else
	printf("Audio index: None available.\nVideo index: None available.\n");	
      
      movtar_close(movtar);
    }
  return 0;
}


