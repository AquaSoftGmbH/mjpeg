/* 
movtar.c (part of libmovtar)
============================

   libmovtar: A library to process (read/write) the 
     movtar MJPEG video file format (short: movtar). 

   These routines are providing a high level interface
   to the movtar file format, and tries to resemble
   the quicktime4linux API by Adam Williams <quicktime@altavista.net>
   (see http://heroine.linuxave.net/quicktime.html) 
   as closely as reasonably possible. 

   Written and (c) by Gernot Ziegler in 2000.
   
   This sourcecode is released under the GNU LGPL license ... 

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU Library General Public License (LGPL) as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   Comments, bug fixes, etc. are welcomed.
   /gz <gz@lysator.liu.se>
*/

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <tar.h>
#include <glib.h>
#include <sys/types.h>

#include "movtar.h"

static int movtar_debug;

#define MDEBUG(x) if (movtar_debug) { printf("%s:", __FUNCTION__); x; }  

/* These are the definition tags for the INFO file in a movtar 
 * This frees me from writing an extensive parser for movtar. 
 *  
 * BTW: Completely incomplete :) */

#define INFO_VERSION "MOVTAR V"
#define INFO_COMMENT ';'
#define INFO_COMMENT2 '#'
#define INFO_EOF "0000"

/* General information */
#define INFO_GEN_AUTHOR "0100"
#define INFO_GEN_DATE "0110"
#define INFO_GEN_TIME "0111"
#define INFO_GEN_SOFTWARE "0120"
#define INFO_GEN_JPEGKOMPR "0130"
#define INFO_GEN_DEVICE "0140"
#define INFO_GEN_INPUT "0150"
#define INFO_GEN_CLASSIFICATION "0160"
#define INFO_GEN_DESCRIPTION "0170"

/* Movie information for playing back */

/* First the video section */
#define INFO_MOV_NORM "1000"
#define INFO_MOV_WIDTH "1010"
#define INFO_MOV_HEIGHT "1011"
#define INFO_MOV_JPEGNUMFIELD "1012"
#define INFO_MOV_FRAMERATE "1013"
#define INFO_MOV_RTJPEG "1014"

/* Then the sound section */
#define INFO_SOUND_AVAIL "1500"
#define INFO_SOUND_STEREO "1510"
#define INFO_SOUND_SIZE "1511"
#define INFO_SOUND_RATE "1512"

/* movtar_init
 * Initializes the movtar routines.
 * in: movtar_debug: Decides if the movtar routines should show debugging messages. 
 * returns: int: TRUE: initialization successful.
 */
int movtar_init(int debug, int tar_debug)
{ 
  gchar *debug_string; 

  movtar_debug = debug;

  debug_string = getenv("DEBUG_MOVTAR");
  if (debug_string) movtar_debug = 1;
  MDEBUG(printf("Initializing MJPEG library\n"));

  tar_init(tar_debug); 

  return TRUE;
}

/* movtar_forward_frames
 * Hops times frames forward in the file. 
 * in: tarfile: The movtar file.
 *     times: The number of files to hop over.
 * returns: TRUE if successful, FALSE if already end of file reached.
*/
int movtar_forward_frames(movtar_t *movtar, int times)
{
  int result;
  int i=0, datatype;
  struct tarinfotype tarinfo;
  long movfilepos; long movfilesize;

  movfilepos = ftell(movtar->file);  
  while (i < times)
  { 
    movfilepos = ftell(movtar->file);  
    movfilesize = tar_read_tarblock(movtar->file, &tarinfo);
    if (movfilesize == 0) 
      return FALSE;

    datatype = movtar_datatype(&tarinfo);
    if (datatype & MOVTAR_DATA_VIDEO) i++;

    /* Fill up to the next 512 byte border */
    if (movfilesize % 512 != 0) 
      movfilesize += (512 - (movfilesize % 512));

    MDEBUG(printf("Moving the file pointer %ld forward.\n", movfilesize));
    /* Move the file pointer relatively to ignore the file data */ 
    result = fseek(movtar->file, movfilesize, SEEK_CUR);
  }

  /* Make the last tarblock read "undone" */
  fseek(movtar->file, movfilepos, SEEK_SET);

  MDEBUG(printf("Leaving movtar_forward_frames.\n"));
 
  return TRUE;
}

/* 
 * Highlevel movtar routines
 */

/* movtar_datatype
 * Determines the type of data stored in a file.
 * in: tarinfoptr: Points to the tarblock containing the file info.
 * out: movtar: 
 * returns: TRUE if valid movtar, FALSE if not.
*/
int movtar_datatype(struct tarinfotype *tarinfoptr)
{ char *filename, *filenamesearch;

  filename = tarinfoptr->name;
  filenamesearch = filename + strlen(filename);
  while ((filenamesearch != filename) && (*filenamesearch != '.'))
    filenamesearch--;

  /* All symlinks are treated as fake frames */
  if (tarinfoptr->typeflag[0] == SYMTYPE)
    return (MOVTAR_DATA_VIDEO | MOVTAR_DATA_FAKE);

  if (strncasecmp(filenamesearch, ".wav", 4) == 0)
    return MOVTAR_DATA_AUDIO;
  if (strncasecmp(filenamesearch, ".raw", 4) == 0)
    return MOVTAR_DATA_AUDIO;
  if (strncasecmp(filenamesearch, ".jpeg", 5) == 0)
    return MOVTAR_DATA_VIDEO;
  if (strncasecmp(filenamesearch, ".rtjpeg", 5) == 0)
    return MOVTAR_DATA_VIDEO_RTJPEG;
  if (strncasecmp(filenamesearch, ".jpg", 4) == 0)
    return MOVTAR_DATA_VIDEO;
  return MOVTAR_DATA_UNKNOWN;
}

/* movtar_parse_info
 * Parses a movtar INFO into a movtar_t struct.
 * in: infobuffer: The buffer containing the INFO content.
 * out: movtar: 
 * returns: TRUE if valid movtar, FALSE if not.
*/
int movtar_parse_info(movtar_t *movtar, const char *infobuffer)
{ 
  gchar **strings;
  gchar **splitline;
  int found, i, valid = FALSE; 
  gchar *normstr;

  MDEBUG(printf("Parsing movtar INFO.\n"));
  strings = g_strsplit(infobuffer, "\n", 0);

  i = 0;
  while (strings[i] != NULL && strncmp(strings[i], INFO_EOF, 4) != 0)
  { if (*(strings[i]) != INFO_COMMENT && *(strings[i]) != INFO_COMMENT2)
    { found = FALSE;
      if (strncmp(strings[i], INFO_VERSION, 8) == 0)
      { splitline = g_strsplit(strings[i], "V ", 0); found = TRUE;
        movtar->version = g_strstrip(splitline[1]); valid = TRUE;
      };
      if (strncmp(strings[i], INFO_GEN_AUTHOR, 4) == 0)
      { splitline = g_strsplit(strings[i], ":", 0); found = TRUE;
        movtar->gen_author = g_strstrip(splitline[1]);
      };
      if (strncmp(strings[i], INFO_GEN_DATE, 4) == 0)
      { splitline = g_strsplit(strings[i], ":", 0); found = TRUE;
        movtar->gen_date = g_strstrip(splitline[1]);
      };
      if (strncmp(strings[i], INFO_GEN_SOFTWARE, 4) == 0)
      { splitline = g_strsplit(strings[i], ":", 0); found = TRUE;
        movtar->gen_software = g_strstrip(splitline[1]);
      };
      if (strncmp(strings[i], INFO_GEN_DEVICE, 4) == 0)
      { splitline = g_strsplit(strings[i], ":", 0); found = TRUE;
        movtar->gen_device = g_strstrip(splitline[1]);
      };
      if (strncmp(strings[i], INFO_GEN_CLASSIFICATION, 4) == 0)
      { splitline = g_strsplit(strings[i], ":", 0); found = TRUE;
        movtar->gen_classification = g_strstrip(splitline[1]);
      };
      if (strncmp(strings[i], INFO_GEN_DESCRIPTION, 4) == 0)
      { splitline = g_strsplit(strings[i], ":", 0); found = TRUE;
        movtar->gen_description = g_strstrip(splitline[1]);
      };
      if (strncmp(strings[i], INFO_GEN_JPEGKOMPR, 4) == 0)
      { splitline = g_strsplit(strings[i], ":", 0); found = TRUE;
        movtar->gen_jpeg_quality = atoi(splitline[1]);
      };
      if (strncmp(strings[i], INFO_GEN_INPUT, 4) == 0)
      { splitline = g_strsplit(strings[i], ":", 0); found = TRUE;
        movtar->gen_input = g_strstrip(splitline[1]);
      };
      if (strncmp(strings[i], INFO_MOV_NORM, 4) == 0)
      { 
	splitline = g_strsplit(strings[i], ":", 0); found = TRUE;
	normstr = g_strstrip(splitline[1]);
	if (0 == strcasecmp(normstr,"pal"))
	  movtar->mov_norm = MOVTAR_NORM_PAL;
	else if (0 == strcasecmp(normstr,"ntsc"))
	  movtar->mov_norm = MOVTAR_NORM_NTSC;
	else if (0 == strcasecmp(normstr,"secam"))
	  movtar->mov_norm = MOVTAR_NORM_SECAM;
	else {
	  printf("Warning: Unknown tv norm %s.\n", splitline[1]);
	  movtar->mov_norm = MOVTAR_NORM_UNKNOWN;      
	}
      };
      if (strncmp(strings[i], INFO_MOV_WIDTH, 4) == 0)
      { splitline = g_strsplit(strings[i], ":", 0); found = TRUE;
        movtar->mov_width = atoi(splitline[1]);
      };
      if (strncmp(strings[i], INFO_MOV_HEIGHT, 4) == 0)
      { splitline = g_strsplit(strings[i], ":", 0); found = TRUE;
        movtar->mov_height = atoi(splitline[1]);
      };
      if (strncmp(strings[i], INFO_SOUND_AVAIL, 4) == 0)
      { splitline = g_strsplit(strings[i], ":", 0); found = TRUE;
        movtar->sound_avail = atoi(splitline[1]);
      };
      if (strncmp(strings[i], INFO_MOV_RTJPEG, 4) == 0)
      { splitline = g_strsplit(strings[i], ":", 0); found = TRUE;
        movtar->rtjpeg_mode = atoi(splitline[1]);
      };
      if (strncmp(strings[i], INFO_MOV_JPEGNUMFIELD, 4) == 0)
      { splitline = g_strsplit(strings[i], ":", 0); found = TRUE;
        movtar->mov_jpegnumfield = atoi(splitline[1]);
      };
      if (strncmp(strings[i], INFO_MOV_FRAMERATE, 4) == 0)
      { splitline = g_strsplit(strings[i], ":", 0); found = TRUE;
        movtar->mov_framerate = atof(splitline[1]);
      };
      if (strncmp(strings[i], INFO_SOUND_STEREO, 4) == 0)
      { splitline = g_strsplit(strings[i], ":", 0); found = TRUE;
        movtar->sound_stereo = atoi(splitline[1]);
      };
      if (strncmp(strings[i], INFO_SOUND_SIZE, 4) == 0)
      { splitline = g_strsplit(strings[i], ":", 0); found = TRUE;
        movtar->sound_size = atoi(splitline[1]);
      };
      if (strncmp(strings[i], INFO_SOUND_RATE, 4) == 0)
      { splitline = g_strsplit(strings[i], ":", 0); found = TRUE;
        movtar->sound_rate = atoi(splitline[1]);
      };
      if (!found)
      MDEBUG(printf("Warning: Couldn't parse \"%s\"\n", strings[i]));
    }
    
    i++;
  }

  return TRUE;
}

/* movtar_create_info_content
 * Creates the INFO header file content, and returns a pointer to a newly created buffer.
 */
gchar *movtar_create_info_content(movtar_t *movtar)
{
  gchar *soundstring;

  if (movtar->sound_avail)
  { soundstring = g_strdup_printf(
	              INFO_SOUND_STEREO "Stereo available : %d\n"
		      INFO_SOUND_SIZE "Sample size : %d\n"
		      INFO_SOUND_RATE "Sample rate : %d\n",
		      movtar->sound_stereo, movtar->sound_size, 
		      movtar->sound_rate);
  }
  else
    soundstring = g_strdup_printf("%s", "");

  return g_strdup_printf( 
	  "MOVTAR V %s\n"
	  INFO_GEN_AUTHOR "Author : %s\n"
	  INFO_GEN_DATE "Date : %s\n"
	  INFO_GEN_SOFTWARE "Software : %s\n"
	  INFO_GEN_JPEGKOMPR "JPEG Quality : %d\n"
	  INFO_GEN_DEVICE "Device : %s\n"
	  INFO_GEN_INPUT "Input : %s\n" 
	  INFO_GEN_CLASSIFICATION "Content Classification : %s\n" 
	  INFO_GEN_DESCRIPTION "Content Description : %s\n" 
	  INFO_MOV_WIDTH "Width : %d\n"
	  INFO_MOV_HEIGHT "Height : %d\n"
	  INFO_MOV_NORM "Norm : %s\n"
	  INFO_MOV_JPEGNUMFIELD "Number of JPEG fields in a file : %d\n"
	  INFO_MOV_FRAMERATE "Framerate (floating point, frames/second) : %f\n"
	  INFO_MOV_RTJPEG "RTJPEG (needs a special codec) : %d\n"
	  INFO_SOUND_AVAIL "Sound : %d\n%s" 
	  INFO_EOF "\n",
	  movtar->version, movtar->gen_author, movtar->gen_date, 
	  movtar->gen_software, movtar->gen_jpeg_quality,
	  movtar->gen_device, movtar->gen_input, 
	  movtar->gen_classification, movtar->gen_description, 
	  movtar->mov_width, movtar->mov_height, 
	  movtar_norm_string(movtar), 
	  movtar->mov_jpegnumfield, 
	  movtar->mov_framerate, 
	  movtar->rtjpeg_mode, 	  
	  movtar->sound_avail, soundstring);

  free(soundstring);
}


/* movtar_write_info
 * Creates movtar header file, and writes it into the movtar. 
 * You can safely assume that this header file is larger than 2048 bytes.
 * in: movtar: The struct containing the movtar information.
*/
void movtar_write_info(movtar_t *movtar)
{ struct tarinfotype infotarinfo;
  char *infocontent, *infofilebuffer;
  char *padstring = (char *)g_strnfill(2048, ' ');
  int result;
 
  tarblock_makedefault(&infotarinfo, "INFO", 2048);

  infocontent = movtar_create_info_content(movtar);
  infofilebuffer = g_strdup_printf("%s%s", infocontent, padstring);
  g_free(infocontent);

  sprintf(infotarinfo.size, "%o", strlen(infofilebuffer));
  tarblock_create_checksum(&infotarinfo);
  result = tar_write(movtar->file, infofilebuffer, strlen(infofilebuffer), &infotarinfo);
  
  if (result != TRUE) 
    { perror("Failed in writing movtar info file."); exit(1); }
  else
    movtar->INFO_written = TRUE;

  free(infofilebuffer);
}

int movtar_find_index(movtar_t *movtar)
{
  gint64 oldpos;
  char indexident[29];
  gint64 blocksize;
  
  oldpos = ftell(movtar->file);

  fseek(movtar->file, -1024-28, SEEK_END); /* Seek to possible index identifier */
  fread(indexident, 28, 1, movtar->file);
  
  indexident[28] = '\0';
  MDEBUG(printf("Read index identifier \"%s\"\n", indexident)); 

  if (strncmp(indexident + 23, "INDEX", 5) != 0) 
    {
      MDEBUG(printf("No video index found !\n"));
      fseek(movtar->file, oldpos, SEEK_SET);
      return 0;
    }
  else
    {
      indexident[23] = '\0';

      sscanf(indexident, "%lld", &blocksize);
      MDEBUG(printf("indexstring: \"%s\", hence loaded value: %lld\n", indexident, blocksize));
      fseek(movtar->file, -1024-blocksize, SEEK_END);
      movtar->video_index_pos = ftell(movtar->file) - 512;
      MDEBUG(printf("video_index_pos at offset %lld\n", movtar->video_index_pos));
      
      if (movtar->sound_avail) /* !! If not sound_avail, then the audio index is not even searched for !*/
	{
	  fseek(movtar->file, -512-28, SEEK_CUR); /* Seek to possible index identifier */
	  fread(indexident, 28, 1, movtar->file);
	  if (strncmp(indexident + 23, "INDEX", 5) != 0) 
	    {
	      fprintf(stderr, "libmovtar: No audio index found, although sound_avail set !"
		      " Internal error\n");
	      return 0;
	    } 
	  
	  indexident[23] = '\0';	  
	  sscanf(indexident, "%lld", &blocksize);
	  MDEBUG(printf("audioindexstring: \"%s\", hence loaded value %lld\n", indexident, blocksize));

	  movtar->audio_index_pos = ftell(movtar->file) - 512 - blocksize;
	}
      
      fseek(movtar->file, oldpos, SEEK_SET);

      return 1;
    }
}


int movtar_load_index(movtar_t *movtar)
{
  gint64 oldpos;
  movtar_audio_frag_t audio_frag;
  movtar_video_frag_t video_frag;
  
  oldpos = ftell(movtar->file);

  if (movtar->sound_avail) /* !! If not sound_avail, then the audio index is not loaded !*/
    {
      fseek(movtar->file, movtar->audio_index_pos + 512, SEEK_SET); /* Seek to audio index start */
  	  
      MDEBUG(printf("Now loading audioindex...\n"));
      movtar->audio_length = 0;
      do
	{
	  fscanf(movtar->file, "%lld %ld %x\n", &audio_frag.pos, &audio_frag.size, &audio_frag.flags);
	  if (audio_frag.size != 0) 
	    { 
	      /* valid audio frag */
	      g_array_append_val(movtar->audio_table, audio_frag);
	      movtar->audio_length += audio_frag.size;
	      MDEBUG(printf("Adding audio frag: pos %lld, size %ld\n", 
			    audio_frag.pos, audio_frag.size));
	      movtar->audio_length += audio_frag.size;
	    }
	}
      while(audio_frag.size != 0 && audio_frag.pos != 0);
    }
  
  fseek(movtar->file, movtar->video_index_pos + 512, SEEK_SET); /* Set it past the header */
  
  MDEBUG(printf("Now loading video index...\n"));
  
  do
    {
      fscanf(movtar->file, "%lld %ld %x\n", &video_frag.pos, &video_frag.size, &video_frag.flags);
      if (video_frag.size != 0) 
	{ 
	  /* valid audio frag */
	  g_array_append_val(movtar->video_table, video_frag);
	  MDEBUG(printf("Adding video frag: pos %lld, size %ld\n", 
	  	    video_frag.pos, video_frag.size));
	}
    }
  while(video_frag.size != 0 && video_frag.pos != 0);
  
  fseek(movtar->file, oldpos, SEEK_SET);
  movtar->index_avail = TRUE;
  return 1;
}

int movtar_write_index(movtar_t *movtar)
{
  struct tarinfotype audiotarinfo, videotarinfo;
  movtar_audio_frag_t *audio_frag;
  movtar_video_frag_t *video_frag;
  unsigned long n, audc, vidc;
  gint64 audio_startpos, video_startpos;
  gint64 audio_endpos, video_endpos;
  gint64 audio_totalblocksize, video_totalblocksize;
  char returns[1024];
  int bytes_to_pad;

  for (n = 0; n < 1024; n++)
    returns[n] = '\n';

  if (movtar->video_index_pos) /* This is for readwrite mode */
    {
      fseek(movtar->file, movtar->video_index_pos, SEEK_SET);
      if (movtar->audio_index_pos)
	fseek(movtar->file, movtar->audio_index_pos, SEEK_SET);
    }
  else
    if (movtar->mode == MOVTAR_MODE_WRITE)
      fseek(movtar->file, 0, SEEK_END); /* Seek to file end */
    else /* This is for readwrite mode */
      fseek(movtar->file, -1024, SEEK_END); /* Position before the ending bytes */

  if (movtar->sound_avail)
    {
      audio_startpos = ftell(movtar->file);
      fwrite(returns, 512, 1, movtar->file);
      
      MDEBUG(printf("Writing audio index\n"));
      for (audc = 0; audc < movtar->audio_table->len; audc++)
	{
	  audio_frag = &g_array_index(movtar->audio_table, movtar_audio_frag_t, audc);
	  fprintf(movtar->file, "%lld %ld %x\n", audio_frag->pos, audio_frag->size, audio_frag->flags);
	}
      fprintf(movtar->file, "0 0 0\n");      

      audio_endpos = ftell(movtar->file);
      if (((audio_endpos - audio_startpos) % 512) > (512 - 30))
	{
	  /* Pad whole new 512 bytes junk block */
	  bytes_to_pad = 1024 - 28 - ((audio_endpos - audio_startpos) % 512);
	  audio_totalblocksize = ((audio_endpos - audio_startpos) / 512) * 512 + 512;
	}
      else
	{
	  bytes_to_pad = 512 - 28 - ((audio_endpos - audio_startpos) % 512);
	  audio_totalblocksize = ((audio_endpos - audio_startpos) / 512) * 512;
	}
      
      fwrite(returns, bytes_to_pad, 1, movtar->file);
      fprintf(movtar->file, "%23lldINDEX", audio_totalblocksize);
      
      tarblock_makedefault(&audiotarinfo, "AUDIO-INDEX", audio_totalblocksize);
      fseek(movtar->file, audio_startpos, SEEK_SET); /* Seek to file end */
      fwrite(&audiotarinfo, 512, 1, movtar->file);

      fseek(movtar->file, audio_startpos + 512 + audio_totalblocksize, SEEK_SET); /* Seek to file end */
      MDEBUG(printf("audio index with %d elements written.\n", movtar->audio_table->len));
      movtar->audio_index_pos = audio_startpos;
    }

  video_startpos = ftell(movtar->file);
  fwrite(returns, 512, 1, movtar->file);

  MDEBUG(printf("Writing video index\n"));
  for (vidc = 0; vidc < movtar->video_table->len; vidc++)
    {
      video_frag = &g_array_index(movtar->video_table, movtar_video_frag_t, vidc);
      fprintf(movtar->file, "%lld %ld %x\n", video_frag->pos, video_frag->size, video_frag->flags);
    }
  
  fprintf(movtar->file, "0 0 0\n");      
  video_endpos = ftell(movtar->file);
  if (((video_endpos - video_startpos) % 512) > (512 - 30))
    {
      /* Pad whole new 512 bytes junk block */
      bytes_to_pad = 1024 - 28 - ((video_endpos - video_startpos) % 512);
      video_totalblocksize = ((video_endpos - video_startpos) / 512) * 512 + 512;
    }
  else
    {
      bytes_to_pad = 512 - 28 - ((video_endpos - video_startpos) % 512);
      video_totalblocksize = ((video_endpos - video_startpos) / 512) * 512;
    }
      
  fwrite(returns, bytes_to_pad, 1, movtar->file);
  fprintf(movtar->file, "%23lldINDEX", video_totalblocksize);
  
  tarblock_makedefault(&videotarinfo, "VIDEO-INDEX", video_totalblocksize);
  fseek(movtar->file, video_startpos, SEEK_SET); /* Seek to tar block pos */
  fwrite(&videotarinfo, 512, 1, movtar->file);
  fseek(movtar->file, video_startpos + 512 + video_totalblocksize, SEEK_SET); /* Seek to file end */
  MDEBUG(printf("video index with %d elements written.\n", movtar->video_table->len));
  movtar->video_index_pos = video_startpos;

  movtar->index_avail = TRUE;
  return 0; 
}

int movtar_build_index(movtar_t *movtar)
{
  movtar_audio_frag_t audio_frag;
  movtar_video_frag_t video_frag;
  struct tarinfotype frameinfo;
  unsigned int video_index = 0;
  unsigned long datasize, datatype;

  MDEBUG(printf("Building movtar index for %s\n", movtar->name));
  movtar->audio_length = 0;

  do
    {
      // MDEBUG(printf("Reading entry\n"));
      /* read the next datafile */
      datasize=tar_read_tarblock(movtar->file, &frameinfo);
      datatype=movtar_datatype(&frameinfo);
      if (datasize <= 0 && !(datatype & MOVTAR_DATA_FAKE)) break;       
      /* TODO: integrate some callback for indexing status ? */

      /* check if we read video or audio data */
      if(datatype & MOVTAR_DATA_AUDIO)
	{
	  //MDEBUG(printf("Adding audio frag: name %s, pos %ld, size %ld\n", 
	  //	frameinfo.name, ftell(movtar->file), datasize));
	  audio_frag.pos = ftell(movtar->file);
	  audio_frag.size = datasize;
	  audio_frag.flags = 0;
	  g_array_append_val(movtar->audio_table, audio_frag);
	  movtar->audio_length += datasize;
	}
      else if((datatype & MOVTAR_DATA_VIDEO))
	{
	  /* A fake video frame has size 0 !!! */
	  if ((datasize == FALSE) && !(datatype & MOVTAR_DATA_FAKE)) 
	    MDEBUG(printf("Warning, empty video frame detected: name %s, pos %lld\n",
			  frameinfo.name, (gint64)ftell(movtar->file)));
	   
	  if (datatype & MOVTAR_DATA_FAKE)
	    {
	      MDEBUG(printf("FAKE video frag ! name %s -> %s, pos %lld\n",
		     frameinfo.name, frameinfo.linkname, (gint64)ftell(movtar->file)));
	      memcpy(&video_frag, 
		     &g_array_index(movtar->video_table, movtar_video_frag_t, 
				    movtar->latest_real_video_frag), 
		     sizeof(movtar_video_frag_t));
	      video_frag.flags = MOVTAR_VIDEO_FAKE;
	    }
	  else
	    {
	      video_frag.pos = ftell(movtar->file);
	      video_frag.size = datasize;
	      video_frag.flags = 0;
	      //MDEBUG(printf("Adding video frag nr. %d to video_table: name %s, pos %lld, size %d\n",
	      //		    video_index, frameinfo.name, video_frag.pos, video_frag.size));

	      movtar->latest_real_video_frag = video_index; /* Save this video index */
	    }

	  g_array_append_val(movtar->video_table, video_frag);
	  video_index++;
	}

      tar_ignore_data(movtar->file, &frameinfo);
    }
  /* the second part in the while is for quick testing */
  while (!tar_eof(movtar->file) && (!movtar_debug || (video_index < 100))); 

  movtar->index_avail = TRUE;
  return 1;
}

/* movtar_open
 * Opens an existing movtar file. 
 * in: tarfilename: The filename of the movtar file.
 * returns: FILE *: The file descriptor for the movtar.
 */
movtar_t *movtar_open(const char *movtarfilename, int read, int write, gint32 flags)
{ 
  int datasize;
  movtar_t *movtar;
  struct tarinfotype frameinfo;
  char readbuffer[MAXFILESIZE];
  gchar **prefix;
  FILE *thefile = NULL;
  
  movtar = g_malloc(sizeof(movtar_t)); 
  movtar->mode = 0; /* unknown mode */
  if (read && !write) movtar->mode = MOVTAR_MODE_READ;
  if (!read && write) movtar->mode = MOVTAR_MODE_WRITE;
  if (read && write) movtar->mode = MOVTAR_MODE_READWRITE;

  movtar->filepath = g_strdup(movtarfilename);
  /* prefix[0] stores the first part of the filename */
  prefix = g_strsplit(g_basename(movtar->filepath), ".", 0); 
  movtar->name = prefix[0];
  MDEBUG(printf("movtar->name is \"%s\"\n", movtar->name));
  movtar->INFO_written = 0;
    
  /* Initialize the fragment table */
  movtar->audio_table = (GArray *)g_array_new(FALSE, FALSE, sizeof(movtar_audio_frag_t));
  movtar->video_table = (GArray *)g_array_new(FALSE, FALSE, sizeof(movtar_video_frag_t));
  
  movtar->audnr = 0; movtar->audio_pos = 0; 
  movtar->vidnr = 0; movtar->audio_rel_pos = 0; /* reset the internal positions */
  movtar->show_fake_mode = (flags & MOVTAR_FLAG_SHOW_FAKE);
  movtar->flags = flags;
  movtar->video_index_pos = 0;
  movtar->audio_index_pos = 0;

  // MDEBUG(printf("movtar struct allocated & initialized\n"));
  switch (movtar->mode)
    {
    case MOVTAR_MODE_READ:
      {
	MDEBUG(printf("Opening movtar \"%s\" in read mode.\n", movtarfilename));
	thefile = fopen(movtarfilename, "r");
	if(thefile == NULL)
	  {
	    fprintf(stderr,"The movtarfile %s couldn't be opened for reading.\n", movtarfilename);
	    g_free(movtar); return NULL;
	  }
	
	movtar->file = thefile;
	/* read header data from the INFO file */
	datasize = tar_read_tarblock(movtar->file, &frameinfo);
	if (!(strncmp(frameinfo.name, "INFO", 4) == 0)) /* not a movtar !! */
	  {
	    fprintf(stderr,"The file \"%s\" is not a movtar !! (always use movtar_check_sig first)\n", 
		    movtarfilename);
	    g_free(movtar); return NULL;
	  }
	
	tar_read_data(movtar->file,readbuffer,datasize);

	movtar->rtjpeg_mode = 0; /* set to a default */
	movtar_parse_info(movtar, readbuffer);
	
	if (movtar_find_index(movtar))
	  { 
	    if (!movtar_load_index(movtar)) /* invalid index ? */
	      {
		fprintf(stderr,"%s: Invalid index found, building own index\n", movtarfilename);	   
		movtar_build_index(movtar); /* build your own index */
	      }
	  }
	else
	  if (movtar->flags && MOVTAR_FLAG_NO_INDEXBUILD)
	    break;
	  else
	    {
	      MDEBUG(fprintf(stderr,"%s: No index found, building own index\n", movtarfilename));	   
	      movtar_build_index(movtar); /* build your own index */
	    }
      }; break;

    case MOVTAR_MODE_WRITE:
	{
	  char *user = getenv("USER");
	  time_t now;
	  
	  thefile = fopen(movtarfilename, "w");
	  if(thefile == NULL)
	    {
	      fprintf(stderr,"The movtarfile %s couldn't be opened for writing.\n", movtarfilename);
	      return NULL;
	    }
	  
	  movtar->file = thefile;
	  movtar->index_avail = FALSE;
	  
	  movtar->version = g_strdup_printf("%s", VERSION);
	  //FIXME: is ctime() threadsafe?
	  time(&now);
	  movtar_set_info(user ? user : "", /* gen_author */
	    ctime(&now), /* gen_date */
	    "", /* gen_software */
	    0, /* gen_jpeg_quality */
	    "", /* gen_device */
	    "", /* gen_input */
	    "", /* gen_classification */
	    "", /* gen_description */
	    movtar);
	}; break;

    case MOVTAR_MODE_READWRITE:
      {
	thefile = fopen(movtarfilename, "r+");
	if(thefile == NULL)
	  {
	    fprintf(stderr,"The movtarfile %s couldn't be opened for read/write.\n", movtarfilename);
	    g_free(movtar); return NULL;
	  }
	
	movtar->file = thefile;
	/* read header data from the INFO file */
	datasize = tar_read_tarblock(movtar->file, &frameinfo);
	if (!(strncmp(frameinfo.name, "INFO", 4) == 0)) /* not a movtar !! */
	  {
	    fprintf(stderr,"The file \"%s\" is not a movtar !! (always use movtar_check_sig first)\n", 
		    movtarfilename);
	    g_free(movtar); return NULL;
	  }

	tar_read_data(movtar->file,readbuffer,datasize);
	movtar_parse_info(movtar, readbuffer);
	
	if (movtar_find_index(movtar))
	  { 
	    if (!movtar_load_index(movtar)) /* invalid index ? */
	      {
		MDEBUG(fprintf(stderr,"%s: Invalid index found, building own index\n", movtarfilename)); 
		movtar_build_index(movtar); /* build your own index */
	      }
	  }
	else
	  if (movtar->flags && MOVTAR_FLAG_NO_INDEXBUILD)
	    break;
	  else
	    {
	      fprintf(stderr,"%s: No index found, building own index\n", movtarfilename);	   
	      movtar_build_index(movtar); /* build your own index */
	    }
      }; break;

    default:
      {
	MDEBUG(printf("Eeeerm, what do you want from %s ? read or write, or both ?\n", __FUNCTION__));
	g_free(movtar);
	return NULL;
      }
    }
  
  return movtar;
}

int movtar_video_tracks(movtar_t *movtar)
{
  return 1;
}

int movtar_audio_tracks(movtar_t *movtar)
{
  return (movtar->sound_avail) ? 1: 0;
}

int movtar_check_sig(char *path)
{
  FILE *movtar;
  struct tarinfotype frameinfo;

  movtar = fopen(path, "r");
  if(movtar == NULL)
    {
      fprintf(stderr,"The movtarfile %s couldn't be opened.\n", path);
      return 0;
    }
  
  /* read data from INFO file */
  tar_read_tarblock(movtar, &frameinfo);

  if (strncmp(frameinfo.name, "INFO", 4) == 0)
    return 1; 
  else 
    return 0;
};


int movtar_audio_bits(movtar_t *movtar)
{
  return movtar->sound_size;
}; 

int movtar_track_channels(movtar_t *movtar)
{
  return (movtar->sound_stereo) ? 2 : 1;
}; 

int movtar_sample_rate(movtar_t *movtar)
{
  return movtar->sound_rate;
};

gint64 movtar_audio_bytes(movtar_t *movtar, gint64 samples)
{
  return (movtar->sound_size == 16) ? (samples *2) : samples;
}

gint64 movtar_audio_samples(movtar_t *movtar, gint64 bytes)
{
  MDEBUG (if (bytes & 1) printf(">> 1 SPILL detected ! Check the source !\n"));
  return (movtar->sound_size == 16) ? (bytes /2) : bytes;
}

gint64 movtar_audio_length(movtar_t *movtar)
{
  if (movtar->index_avail)
    /* return the accumulated audio length */
    return movtar_audio_samples(movtar, movtar->audio_length);
  else
    return 0; /* this is an invalid call if no index exists */
};

long movtar_video_length(movtar_t *movtar)
{
  return movtar->video_table->len;
};

int movtar_video_width(movtar_t *movtar)
{
  return movtar->mov_width;
};

int movtar_video_height(movtar_t *movtar)
{
  return movtar->mov_height;
};

float movtar_frame_rate(movtar_t *movtar)
{
  if (movtar->mov_framerate != 0.0)
    return movtar->mov_framerate;
  else /* Guess the framerate based on the NORM */
    switch (movtar->mov_norm)
      {
      case MOVTAR_NORM_NTSC: return 29.97;
      case MOVTAR_NORM_PAL: return 25.0;
      case MOVTAR_NORM_SECAM: return 25.0;
	
      default:
	return 0.0;
      }
};

/* the movtar lib does always support decoded audio, since movtar only contains 16bit samples 
   (no compressed audio streams possible) */ 
int movtar_supported_audio(movtar_t *movtar)
{
  return 1;
}; 

int movtar_decode_audio(movtar_t *movtar, short int *output_i, float *output_f, size_t samples)
{
  char *audio_buffer = g_malloc(movtar_audio_bytes(movtar, samples));
  size_t x;

  if (output_f != NULL)
    {
      MDEBUG(printf("Floating point audio output is not supported !"));
      return -1;
    };
  movtar_read_audio(movtar, audio_buffer, samples); /* read the samples */

  for (x = 0; x <= samples; x++)
    memcpy(&output_i[x], &audio_buffer[x], ((movtar->sound_size == 16) ? 2 : 1)); 

  return samples;
}

/* gives up the number of bytes in the specified frame in the specified track even if you haven't read the frame yet. Frame numbers start on 0. */
long movtar_frame_size(movtar_t *movtar, long frame)
{
  movtar_video_frag_t *curframe;
  
  if (frame >= movtar->video_table->len)
    { 
      MDEBUG(printf("Error: Requested frame %ld is beyond range !", frame));
      return 0; /* range check error */
    }
   
  curframe = &g_array_index(movtar->video_table, movtar_video_frag_t, frame); 

  if (curframe->flags & MOVTAR_VIDEO_FAKE)
    {
      if (movtar->show_fake_mode)
	return 1; /* This signals a fake frame (since a real video frame never has size 1) */
      else
	{
	  movtar_video_frag_t *realframe = 
	    &g_array_index(movtar->video_table, movtar_video_frag_t, 
			   movtar->latest_real_video_frag); 
	  return realframe->size;
	}
    }
  else
    return curframe->size;
};

/* movtar_read_frame reads one frame worth of raw data from your current position on the 
   specified video track and returns the number of bytes in the frame. 
   You have to make sure the buffer is big enough for the frame. A return value
   of 0 means error. */
long movtar_read_frame(movtar_t *movtar, unsigned char *video_buffer)
{
  long datasize;
  movtar_video_frag_t *curframe = 
    &g_array_index(movtar->video_table, movtar_video_frag_t, movtar->vidnr); 
  movtar_video_frag_t *realframe;

  /* MDEBUG(printf("Reading video frame at pos %lld, size %ld, flags %d\n", 
     curframe->pos, curframe->size, curframe->flags));
  */
  if (movtar->mode == MOVTAR_MODE_WRITE) return 0;

  if (curframe->flags & MOVTAR_VIDEO_FAKE)
    {
      if (movtar->show_fake_mode)
	return 1; /* This signals a fake frame (since a real video frame never has size 1) */
      else /* "betray" the application with another frame (the latest real one) */
	{
	  realframe = &g_array_index(movtar->video_table, movtar_video_frag_t, 
				     movtar->latest_real_video_frag); 

	  fseek(movtar->file, realframe->pos, SEEK_SET);
	  fread(video_buffer, realframe->size, 1, movtar->file);

	  datasize = realframe->size;
	}
    }
  else
    { 
      fseek(movtar->file, curframe->pos, SEEK_SET);      
      fread(video_buffer, 1, curframe->size, movtar->file);
      datasize = curframe->size;      
    }

  movtar->vidnr++;
  
  return datasize;
} 

/* Now some of you are going to want to read frames directly from a file descriptor using another 
   library like libjpeg or something. To read a frame directly start by calling */
int movtar_read_frame_init(movtar_t *movtar)
{
  movtar_video_frag_t *curframe = 
    &g_array_index(movtar->video_table, movtar_video_frag_t, movtar->vidnr); 
  movtar_video_frag_t *realframe;

  if (movtar->mode == MOVTAR_MODE_WRITE) return 0;

  if (curframe->flags & MOVTAR_VIDEO_FAKE)
    {
      if (movtar->show_fake_mode)
	return 1; /* This signals a fake frame (since a real video frame never has size 1) */
      else /* "betray" the application with another frame (the latest real one) */
	{
	    realframe = &g_array_index(movtar->video_table, movtar_video_frag_t, 
			   movtar->latest_real_video_frag); 

	  fseek(movtar->file, realframe->pos, SEEK_SET);
	}
    }
  else
    { 
      fseek(movtar->file, curframe->pos, SEEK_SET);
    }

  return curframe->size;
} 

/* Then read your raw, compressed data from a file descriptor given by */
FILE* movtar_get_fd(movtar_t *movtar)
{
  return movtar->file;
}

/* End the frame read operation by calling
   You can get the file descriptor any time the file is opened, not just when reading or writing, 
   but you must call the init
   and end routines to read a frame. 
*/
int movtar_read_frame_end(movtar_t *movtar)
{
  movtar->vidnr++;
  return 1;
}
 
/*
  These commands are good for reading raw sample data. They should only be used for codecs 
  not supported in the library and only work for interleaved, linear PCM data.
  movtar_read_audio requires a number of samples of raw audio data to read. Then it reads that 
  corresponding number of bytes on the specified track and returns the equivalent number of bytes read 
  or 0 if error. The data read is PCM audio data of interleaved channels depending on the format
  of the track. 
*/
gint64 movtar_read_audio(movtar_t *movtar, char *audio_buffer, gint64 samples)
{
  gint64 bytesize;
  movtar_audio_frag_t *curaudio;
  unsigned long left_to_read, buffpos = 0, avail_bytes;

  bytesize = movtar_audio_bytes(movtar, samples); /* gaah, I hate this samples calculation ! */
  if (movtar->mode == MOVTAR_MODE_WRITE) return 0;
  if (!movtar->sound_avail) return 0;

  left_to_read = bytesize;
  curaudio = &g_array_index(movtar->audio_table, movtar_audio_frag_t, movtar->audnr); 
  fseek(movtar->file, curaudio->pos + movtar->audio_rel_pos, SEEK_SET);

  //MDEBUG(printf("%lld bytes must now be read. audnr: %ld. audpos %lld aud_relpos %lld\n",
  //	bytesize, movtar->audnr, movtar->audio_pos, movtar->audio_rel_pos));
  while (left_to_read > 0 && movtar->audnr < movtar->audio_table->len)
    {
      //MDEBUG(printf("curaudio: pos: %lld, size %ld | audnr: %ld\n", 
      //	    curaudio->pos, curaudio->size, movtar->audnr));  

      /* Does the current audio frag suffice ? */
      if (left_to_read >= (curaudio->size - movtar->audio_rel_pos))
	{
	  avail_bytes =  (curaudio->size - movtar->audio_rel_pos);
	  fread(audio_buffer + buffpos, avail_bytes, 1, movtar->file);
	  movtar->audnr++; /* Get the next audio frag */
	  curaudio = &g_array_index(movtar->audio_table, movtar_audio_frag_t, movtar->audnr); 
	  movtar->audio_rel_pos = 0;
	  fseek(movtar->file, curaudio->pos, SEEK_SET);
	}
      else
	{
	  avail_bytes = left_to_read;
	  fread(audio_buffer + buffpos, avail_bytes, 1, movtar->file);
	  movtar->audio_rel_pos += avail_bytes;	  
	}

      movtar->audio_pos += avail_bytes;
      buffpos += avail_bytes;
      left_to_read -= avail_bytes;
    }

  if (left_to_read) MDEBUG(printf("left_to_read = %ld\n", left_to_read));
  return (bytesize - left_to_read); 
} 

int movtar_seek_end(movtar_t *movtar)
{
  movtar_audio_frag_t *curaudio; 

  MDEBUG(printf("%s\n", __FUNCTION__));
  movtar->vidnr = movtar->video_table->len - 1;
  movtar->audio_pos -= movtar->audio_rel_pos;
  movtar->audio_rel_pos = 0;

  while (movtar->audnr != movtar->audio_table->len - 1)
    {
      curaudio = &g_array_index(movtar->audio_table, movtar_audio_frag_t, movtar->audnr); 
      movtar->audio_pos += curaudio->size;
      movtar->audnr++;
    }

  curaudio = &g_array_index(movtar->audio_table, movtar_audio_frag_t, movtar->audnr); 
  movtar->audio_pos += curaudio->size;
  movtar->audio_rel_pos = curaudio->size;

  return 1;
};

int movtar_seek_start(movtar_t *movtar)
{
  MDEBUG(printf("%s\n", __FUNCTION__));
  movtar->audnr = 0;
  movtar->vidnr = 0;
  movtar->audio_rel_pos = 0;
  movtar->audio_pos = 0;

  return 1;
};

int movtar_set_audio_position(movtar_t *movtar, long sample)
{
  gint64 bytepos = sample * ((movtar->sound_size == 16) ? 2 : 1);
  movtar_audio_frag_t *curaudio; 
  gint64 fragstart;

  if (movtar->mode == MOVTAR_MODE_WRITE) return 0;
  MDEBUG(printf("Setting audio position to %ld\n", sample));
  if (bytepos == movtar->audio_pos) return 0;
  curaudio = &g_array_index(movtar->audio_table, movtar_audio_frag_t, movtar->audnr); 
  fragstart = movtar->audio_pos - movtar->audio_rel_pos;

  if ((bytepos - fragstart) < curaudio->size) /* in this frag ? */
    {
      movtar->audio_rel_pos = bytepos - fragstart;
      movtar->audio_pos = bytepos;
      return 1;
    }
  else
    {
      /* This algorithm could be MUCH more efficient ! */
      /* Maybe I should use a binary sorting tree in the future ? */
      movtar->audio_pos = fragstart; /* Reset to this fragment start for easier calculations */
      movtar->audio_rel_pos = 0;

      if (bytepos > movtar->audio_pos)
	{
	  do
	    {
	      if (movtar->audnr == movtar->audio_table->len - 1) /* max it out */ 
		{
		  movtar->audio_pos += curaudio->size;
		  movtar->audio_rel_pos = curaudio->size;
		  return 0;
		}; 
	  
	      movtar->audio_pos += curaudio->size;
	      movtar->audio_rel_pos = 0;
	      movtar->audnr++; /* go to next frag */
	      curaudio = &g_array_index(movtar->audio_table, movtar_audio_frag_t, movtar->audnr); 
	    }
	  while ((bytepos + curaudio->size) >= movtar->audio_pos);
	  /* Now we have the right frag */
	  movtar->audio_rel_pos = bytepos - movtar->audio_pos;
	  movtar->audio_pos = bytepos;
	  return 1; /* Success */
	}
      else
	{
	  do
	    {
	      if (movtar->audnr == 0) /* max it out */ 
		{
		  movtar->audio_pos = 0;
		  movtar->audio_rel_pos = 0;
		  return 0;
		}; 
	  
	      movtar->audio_pos -= curaudio->size;
	      movtar->audio_rel_pos = 0;
	      movtar->audnr--; /* go to frag before */
	      curaudio = &g_array_index(movtar->audio_table, movtar_audio_frag_t, movtar->audnr); 
	    }
	  while (bytepos < movtar->audio_pos);
	  /* Now we have the right frag */
	  movtar->audio_rel_pos = bytepos - movtar->audio_pos;
	  movtar->audio_pos = bytepos;
	  return 1; /* Success */
	}
    }
  return 0;
}

int movtar_set_video_position(movtar_t *movtar, long frame)
{
  if (movtar->mode == MOVTAR_MODE_WRITE) return 0;
  // MDEBUG(printf("moving to frame %ld \n", frame));
  movtar->vidnr = frame;
  return 0;
}

int movtar_set_audio(movtar_t *movtar, int channels, int sample_rate, int bits, char *compressor)
{
  movtar->sound_avail = 1;

  if (channels > 2) return -1;
  movtar->sound_stereo = (channels == 2) ? 1 : 0;
  movtar->sound_rate = sample_rate;

  if (bits != 16 && bits != 8) return -1;
  movtar->sound_size = bits;

  return 0;
}

int movtar_set_video(movtar_t *movtar, int tracks, int frame_w, int frame_h, 
		     float frame_rate, char *compressor, int interlaced)
{
  if (tracks > 1) return -1;
  movtar->mov_framerate = frame_rate;
  movtar->mov_width = frame_w; 
  movtar->mov_height = frame_h; 
  movtar->mov_jpegnumfield = (interlaced) ? 2 : 1;
  return 0;
}

int movtar_set_info(const char *gen_author, const char *gen_date, 
		    const char *gen_software, guint gen_jpeg_quality,
		    const char *gen_device, const char *gen_input,
		    const char *gen_classification, const char *gen_description,
		    movtar_t *dest)
{
  dest->gen_author = g_strdup(gen_author); /* The autor of the file */
  dest->gen_date = g_strdup(gen_date);   /* The generation date */
  dest->gen_software = g_strdup(gen_software); /* The used software for creation */
  dest->gen_jpeg_quality = gen_jpeg_quality;  /* JPEG quality ! */
  dest->gen_device = g_strdup(gen_device);   /* The creating video device */
  dest->gen_input = g_strdup(gen_input);    /* The used video input */
  dest->gen_classification = g_strdup(gen_classification);  /* A general classification of the content*/
  dest->gen_description = g_strdup(gen_description);     /* A longer description text */
  return 0;
}

void movtar_show_fake_frames(movtar_t *movtar, int yes)
{
  movtar->show_fake_mode = yes;
}

int movtar_write_frame_init(movtar_t *movtar)
{
  char junk[512];

  /* Nothing to do here ? */
  /* Write 512 empty bytes, which are becoming a tar header */
  /* Save the file position */
  if (movtar->mode == MOVTAR_MODE_READ) return -1;
  if (movtar->mode == MOVTAR_MODE_READWRITE) return -1;

  if (movtar->mode == MOVTAR_MODE_WRITE)
    {
      if (!movtar->INFO_written) 
	movtar_write_info(movtar);

      if (movtar->vidnr != movtar->video_table->len) return -1;
      
      if (movtar->INFO_written)
	{
	  fseek(movtar->file, 0, SEEK_END);
	  movtar->writepos = ftell(movtar->file);
	  fwrite(junk, 512, 1, movtar->file);
	  return 0;
	}
      else
	return -1;
    }
  else
    return -1;
}

int movtar_write_frame_end(movtar_t *movtar)
{
  struct tarinfotype frameinfo, frcopyinfo;
#ifdef	NEVER
  fpos_t newpos, filesize;
#else
  size_t newpos, filesize;
#endif
  gchar jpegname[255], jpegcopyname[255];
  movtar_video_frag_t video_frag;
  
/* Now check how many bytes have been written */
  /* if zero bytes have been written, it was a fake frame */
  /* fill in the tar header accordingly, write out padding zeroes */
  /* and write the tar header */ 
  newpos = ftell(movtar->file);
  fseek(movtar->file, movtar->writepos, SEEK_SET); /* go back to the tar header position */
  if (newpos == movtar->writepos) /* fake frames have size 0 */
    {
      /* We have a fake frame here ! Trace back & create a symlink */
      if (!movtar->rtjpeg_mode)
	{
	  sprintf(jpegcopyname, "%s_%06ld.jpeg", movtar->name, movtar->vidnr + 100000);
	  sprintf(jpegname, "%s_%06ld.jpeg", movtar->name, movtar->latest_real_video_frag + 100000);
	}
      else
	{
	  sprintf(jpegcopyname, "%s_%06ld.rtjpeg", movtar->name, movtar->vidnr + 100000);
	  sprintf(jpegname, "%s_%06ld.rtjpeg", movtar->name, movtar->latest_real_video_frag + 100000);
	}

      tarblock_makedefault_symlink(&frcopyinfo, jpegcopyname, jpegname);
      fwrite(&frcopyinfo, 512, 1, movtar->file);
      fseek(movtar->file, newpos, SEEK_SET); /* return to the end of the file */

      /* Copy the old frag's index entry */
      memcpy(&video_frag, 
	     &g_array_index(movtar->video_table, movtar_video_frag_t, 
			    movtar->latest_real_video_frag), 
	     sizeof(movtar_video_frag_t));
      video_frag.flags = MOVTAR_VIDEO_FAKE;
    }
  else
    {
      /* Calculate the filesize to put into the header */
      filesize = newpos - movtar->writepos - 512;
      printf("%s: Filesize to store: %lld\n", __FUNCTION__, (gint64)filesize);

      /* create the new frame name */
      if (!movtar->rtjpeg_mode)
	g_snprintf(jpegname, 99, "%s_%06ld.jpeg", movtar->name, movtar->vidnr + 100000);
      else
	g_snprintf(jpegname, 99, "%s_%06ld.rtjpeg", movtar->name, movtar->vidnr + 100000);

      tarblock_makedefault(&frameinfo, jpegname, filesize);
      fwrite(&frameinfo, 512, 1, movtar->file);
      
      movtar->latest_real_video_frag = movtar->vidnr;
      movtar->vidnr++;
      fseek(movtar->file, newpos, SEEK_SET); /* return to the end of the file */
      /* pad the remaining bytes to the next 512-byte border */
      tar_pad_with_zeroes(movtar->file, filesize);

      /* Create a new video frag index entry */ 
      video_frag.pos = movtar->writepos + 512;
      video_frag.size = ftell(movtar->file) - movtar->writepos;
      video_frag.flags = 0;
    }
  
  g_array_append_val(movtar->video_table, video_frag);
  return 0;
}

int movtar_write_frame(movtar_t *movtar, unsigned char *video_buffer, size_t bytes)
{
  struct tarinfotype frameinfo, frcopyinfo;
  gchar jpegname[255], jpegcopyname[255];
  movtar_video_frag_t video_frag;
  
  if (movtar->mode == MOVTAR_MODE_WRITE)
    {
      if (!movtar->INFO_written) 
	movtar_write_info(movtar);
      
      /* sanity checks */
      if (!movtar->INFO_written) return -1;
      if (movtar->vidnr != movtar->video_table->len) return -1;
      
      /* Write at the end of the file */
      fseek(movtar->file, 0, SEEK_END);

      if (bytes == 0) /* fake frames have size 0 */
	{
	  /* We have a fake frame here ! Trace back & create a symlink */
	  if (!movtar->rtjpeg_mode)
	    {
	      g_snprintf(jpegcopyname, 99, "%s_%06ld.jpeg", movtar->name, movtar->vidnr + 100000);
	      g_snprintf(jpegname, 99, "%s_%06ld.jpeg", movtar->name, movtar->latest_real_video_frag + 100000);
	    }
	  else
	    {
	      g_snprintf(jpegcopyname, 99, "%s_%06ld.rtjpeg", movtar->name, movtar->vidnr + 100000);
	      g_snprintf(jpegname, 99, "%s_%06ld.rtjpeg", movtar->name, movtar->latest_real_video_frag + 100000);
	    } 
	  tarblock_makedefault_symlink(&frcopyinfo, jpegcopyname, jpegname);
	  fwrite(&frcopyinfo, 512, 1, movtar->file);

	  /* Copy the old frag's index entry */
	  memcpy(&video_frag, 
		 &g_array_index(movtar->video_table, movtar_video_frag_t, 
				movtar->latest_real_video_frag), 
		 sizeof(movtar_video_frag_t));
	  video_frag.flags = MOVTAR_VIDEO_FAKE;
	}
      else
	{
	  MDEBUG(printf("%s: Filesize to store: %d\n", __FUNCTION__, bytes));

	  /* create the new frame name */
	  if (!movtar->rtjpeg_mode)
	      g_snprintf(jpegname, 99, "%s_%06ld.jpeg", movtar->name, movtar->vidnr + 100000);
	  else
	      g_snprintf(jpegname, 99, "%s_%06ld.rtjpeg", movtar->name, movtar->vidnr + 100000);

	  tarblock_makedefault(&frameinfo, jpegname, bytes);
	  
	  movtar->latest_real_video_frag = movtar->vidnr;
	  movtar->vidnr++;

	  /* Create a new video frag index entry */ 
	  video_frag.pos = ftell(movtar->file) + 512;
	  video_frag.size = bytes;
	  video_frag.flags = 0;

	  tar_write(movtar->file, video_buffer, bytes, &frameinfo);
	}

      g_array_append_val(movtar->video_table, video_frag);
      return 0;
    }
  else return -1; /* not possible */
} 

int movtar_write_audio(movtar_t *movtar, char *audio_buffer, size_t samples)
{
  movtar_audio_frag_t audio_frag;
  struct tarinfotype audioinfo;
  gchar audioname[100];
  gint32 bytesize = movtar_audio_bytes(movtar, samples);

  switch (movtar->mode)
    {
    case MOVTAR_MODE_WRITE:
      {
	if (!movtar->INFO_written) 
	  movtar_write_info(movtar);

	/* Write at the end of the file */
	fseek(movtar->file, 0, SEEK_END);
	
	/* Create the internal file name */
	g_snprintf(audioname, 99, "%s_%06ld.raw", 
		   movtar->name, movtar->audnr + 100000);
	tarblock_makedefault(&audioinfo, audioname, bytesize);

	/* Add this frag to the audio frag table */
	audio_frag.pos = ftell(movtar->file) + 512; /* 512+, to skip the header */
	audio_frag.size = bytesize;
	audio_frag.flags = 0;
	g_array_append_val(movtar->audio_table, audio_frag);
	movtar->audio_length += bytesize; /* don't forget the overall audio length */
	
	MDEBUG(printf("Audio frag with pos %lld, size %ld, written", audio_frag.pos, audio_frag.size));
	/* Write the file incl. its new header */

	tar_write(movtar->file, audio_buffer, bytesize, &audioinfo);

	movtar->audnr++;
	return 0; /* ok */
      }
    default: 
      return -1; /* sorry, read/write not possible yet */
      
    }
}

/* returns the name of the movtar (which is part of the path) */
gchar *movtar_name(movtar_t *movtar)
{
  return movtar->name;
}

gchar *movtar_norm_string(movtar_t *movtar)
{
  switch (movtar->mov_norm)
    {
    case MOVTAR_NORM_PAL: 
      return "PAL";
    case MOVTAR_NORM_NTSC: 
      return "NTSC";
    case MOVTAR_NORM_SECAM: 
      return "SECAM";
    default:      
      return "unknown";
    }
}

void movtar_copy_header(movtar_t *src, movtar_t *dest)
{
  dest->version = g_strdup(src->version); /* The movtar version string */
  dest->gen_author = g_strdup(src->gen_author); /* The autor of the file */
  dest->gen_date = g_strdup(src->gen_date);   /* The generation date */
  dest->gen_software = g_strdup(src->gen_software); /* The used software for creation */
  dest->gen_jpeg_quality = src->gen_jpeg_quality;  /* JPEG quality ! */
  dest->gen_device = g_strdup(src->gen_device);   /* The creating video device */
  dest->gen_input = g_strdup(src->gen_input);    /* The used video input */
  dest->gen_classification = g_strdup(src->gen_classification);  /* A general classification of the content*/
  dest->gen_description = g_strdup(src->gen_description);     /* A longer description text */
  dest->mov_framerate = src->mov_framerate; /* The movie framerate */
  dest->mov_norm = src->mov_norm; /* contains video norm */
  dest->mov_width = src->mov_width; /* The frame width */
  dest->mov_height = src->mov_height; /* The frame height */
  dest->mov_jpegnumfield = src->mov_jpegnumfield; /* The number of partial frames in one file; 1 or 2 */
}

void movtar_destroy_struct(movtar_t *movtar)
{
  g_array_free(movtar->audio_table, TRUE);
  g_array_free(movtar->video_table, TRUE);
  g_free(movtar);
}

int movtar_close(movtar_t *movtar)
{
  switch (movtar->mode)
  {
  case MOVTAR_MODE_WRITE:
    {
      movtar_write_index(movtar);
      tar_finish_close(movtar->file);
    }; break;
    
  case MOVTAR_MODE_READWRITE:
    {
      movtar_write_index(movtar);
      tar_finish_truncate_close(movtar->file); /* to cut off unwanted rest index data */
    }; break;
    
  case MOVTAR_MODE_READ:
    fclose(movtar->file); break;
  }
  
  movtar_destroy_struct(movtar);
  return 1;
}




