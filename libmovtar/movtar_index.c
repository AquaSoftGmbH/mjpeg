/*
movtar_index (V1.0)
===================

  Creates (or removes) the audio and video fragment index stored 
  in a movtar file. 

  Usage: movtar_index <options> -i filename 
  (see movtar_index -h for help (or have a look at the function "usage"))
  
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

static gchar *movtarname   = "dummy.movtar";
static movtar_t *movtar;

static int verbose = 0;
static int debug = 0;
static int dump_audio = 0;
static int dump_video = 0;
static int create_index = 0;
static int remove_index = 0;

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
	  "%s creates/removes/dumps the video and audio fragment index \n"
          "in a movtar file (e.g. one recorded by lavrec):\n"
	  "\n"
	  "\n"
	  "usage: %s -i filename [ options ]\n"
	  "\n"
	  "options:\n"
          "  -i          the movtar file name\n"
	  "  -v          verbose operation	       \n"
	  "  -d          debug information	       \n"
	  "  -R          removes the movtar index\n"
	  "  -A          dumps the audio fragment index\n"
	  "  -V          dumps the video fragment index\n"	  
          "  -C          creates an audio and video index\n"
	  "\n"
	  "examples:\n"
	  "  %s -C -i stream.movtar\n"
	  "  | creates a new audio and video index in the movtar (e g for old files)\n"
	  "  %s -R -i stream.movtar\n"
	  "  | removes the existing video index (and audio index, if it exists)\n"
	  "  %s -A -V -i stream.movtar\n"
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
   if (-1 == (c = getopt(argc, argv, "dvAVRCi:")))
     break;
   switch (c) {
   case 'v':
     verbose = 1;
     break;
   case 'd':
     debug = 1;
     break;
   case 'A':
     dump_audio = 1;
     break;
   case 'V':
     dump_video = 1;
     break;
   case 'R':
     remove_index = 1;
     break;
   case 'C':
     create_index = 1;
     break;
   case 'i':
     movtarname = optarg;
     break;
   case 'h':
   default:
     usage(argv[0]);
     exit(1);
   }
 }
 if (strcmp(movtarname, "dummy.movtar") == 0)
   { printf("%s needs an input filename to proceed.\n"
	    "See also %s -h for help.\n", argv[0], argv[0]); exit(1);
   }
 if (create_index && remove_index)
   { printf("You can't both create_index and remove_index and index at the same time !\n"
	    "See also %s -h for help.\n", argv[0]); exit(1);
   }   
}

/*
 * The file handling parts 
 */

int remove_movtar_index()
{
  /* open input movtar, do not build an index if it doesn't exist  */
  movtar = movtar_open(movtarname, 1, 1, MOVTAR_FLAG_NO_INDEXBUILD); 
  if (movtar == NULL) { perror("Opening the input movtar file failed.\n"); exit(1); }

  if ((movtar->audio_index_pos == 0) && (movtar->video_index_pos == 0))
    {
      printf("There is no audio and no video index stored in the file !\n");
      return 0;
    }

  /* Now do real nasty stuff (the lib does not allow to close a file without writing an index */
  if (movtar->audio_index_pos)
    fseek(movtar->file, movtar->audio_index_pos, SEEK_SET);
  else
    fseek(movtar->file, movtar->video_index_pos, SEEK_SET);
  
  /* Muahaha }:-) */
  tar_finish_truncate_close(movtar->file);
  movtar_destroy_struct(movtar);

  return TRUE;
}

/* main
 * Rather simple. Opens the input file, and starts processing.. 
 * in: argc, argv:  Classic commandline parameters. 
 * returns: int: 0: success, !0: !success :-)
 */
int main(int argc, char **argv)
{ 
  long audc, vidc;
  movtar_audio_frag_t *audio_frag;
  movtar_video_frag_t *video_frag;

  movtar_init(FALSE, FALSE);

  parse_commandline(argc, argv);

  if (!movtar_check_sig(movtarname))
    { 
      fprintf(stderr, "\"%s\" doesn't seem to be a movtar file\n", movtarname); exit(1);
    }

  if (remove_index)
  {
    if (remove_movtar_index() == FALSE)
      { 
	fprintf(stderr, "* Removing the movtar index failed.\n");
	exit(1);
      }
  }
  else
    {
      if (create_index)
	{
	  /* open the movtar in read/write mode ! */
	  /* when closing it, the index will automatically be create_indexd ! */
	  /* this implies also that a file that already has an index will have its index rewritten */
	  if (verbose) printf("Opening and parsing the input file (this can take a while)\n");
	  
	  movtar = movtar_open(movtarname, 1, 1, 0);  
	  if (movtar == FALSE)
	    { 
	      fprintf(stderr, "* Creating the movtar index failed.\n");
	      exit(1);
	    }  
	}
      else
	movtar = movtar_open(movtarname, 1, 0, 0); /* only open for reading */
 
      if (dump_audio)
	{
	  if (movtar->sound_avail) /* this shows if there is an audio index */
	    {
	      printf("Audio index: %d elements\n", movtar->audio_table->len);
	      for (audc = 0; audc < movtar->audio_table->len; audc++)
		{
		  audio_frag = &g_array_index(movtar->audio_table, movtar_audio_frag_t, audc);
		  printf("pos:%lld size:%ld flags:0x%x\n", 
			 audio_frag->pos, audio_frag->size, audio_frag->flags);
		}
	      if (verbose) printf("Audio dump finished.\n");
	    }
	  else
	    if (verbose) 
	      fprintf(stderr, "Warning: This movtar does not contain any audio ! No audio dump done.\n");
	    else
	      fprintf(stderr, "Audio index: None available.\n");		      
	}
      
      if (dump_video)
	{ /* Video index must always be available */
	  printf("Video index: %d elements\n", movtar->video_table->len);
	  for (vidc = 0; vidc < movtar->video_table->len; vidc++)
	    {
	      video_frag = &g_array_index(movtar->video_table, movtar_video_frag_t, vidc);
	      printf("pos:%lld size:%ld flags:0x%x\n", 
		     video_frag->pos, video_frag->size, video_frag->flags);
	    }
	  if (verbose) printf("Video dump finished.\n");
	}
      
      movtar_close(movtar);
      if (create_index && verbose) printf("Movtar index written.\n");
    }
  
  return 0;
}










