/* Helping routines for handling the movtar format. 
   Written and (c) by Gernot Ziegler in 1999.
   
   This source is released under the GNU LGPL license ... 

   Comment, bug fixes, etc. are welcomed.
   /gz <gz@lysator.liu.se>
*/

/* Still just internal helping routines, but in the long
 * run aiming at becoming a library.
 * (Did you _really_ believe that ? 
 * :-) Well, maybe I'm going to convert it. )
 */


#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <tar.h>
#include <glib.h>

#include <sys/types.h>
#include <linux/types.h>
#include <linux/videodev.h>

#include "movtar.h"

#define MAXFILESIZE 300000

char zeroes[512];
static int movtar_debug;

#define MDEBUG(x) if (movtar_debug)  x 

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

/* Then the sound section */
#define INFO_SOUND_AVAIL "1500"
#define INFO_SOUND_STEREO "1510"
#define INFO_SOUND_SIZE "1511"
#define INFO_SOUND_RATE "1512"


/********************************************************
 * tar info header (= tarblock) handling routines       *
 ********************************************************/


/* tarblock_size
 * Retrieves the size of the file stored in the tar.
 * in: tarinfo: The tarblock to be processed.
 * returns: unsigned int.: the size of the associated file.
 */

unsigned int tarblock_size(struct tarinfotype *tarinfoptr)
{ char sizestring[13];
  unsigned int size;

  strncpy(sizestring, tarinfoptr->size, 12);
  sizestring[12] = '\0';

  sscanf(sizestring, "%o", &size);

  return size;
}

/* tarblock_checksum
 * Calculates a new checksum for a given tarblock
 * in: tarinfoptr: The tarblock to be processed.
 * returns: unsigned int: the checksum of the tarblock.
 */

unsigned int tarblock_checksum(struct tarinfotype *tarinfoptr)
{ unsigned int i, checksum=0; 
  unsigned char *tarinfocharptr = (unsigned char *)tarinfoptr;

  for(i=0; i<=147; i++) checksum += *(tarinfocharptr + i);
  for(i=156; i<=511; i++) checksum += *(tarinfocharptr + i);

  checksum += 8*0x20;
 
  return checksum;
}

/* tarblock_create_checksum
 * Creates a new checksum for a tarblock and writes it 
 * into the same block.
 * in/out: tarinfoptr: The tarblock to be "checksummed".
 */

void tarblock_create_checksum(struct tarinfotype *tarinfoptr)
{ 
  sprintf(tarinfoptr->chksum, "%o", tarblock_checksum(tarinfoptr));
}

/* tarblock_original_checksum
 * Extracts the checksum stored in a tarblock.
 * in: tarinfoptr: The tarblock which contains the checksum.
 * returns: The extracted checksum.
 */

unsigned int tarblock_original_checksum(struct tarinfotype *tarinfoptr)
{ char chksumstring[9];
  unsigned int checksum;

  strncpy(chksumstring, tarinfoptr->chksum, 8);
  chksumstring[8] = '\0';

  sscanf(chksumstring, "%o", &checksum);

  return checksum;
}

/* tarblock_makedefault
 * Creates a default template for files stored in a movtar.
 * out: tarinfoptr: The pointer to the tarblock to be created.
 * in:  filename: The filename of the new file.
 *      filesize: The filesize of the new file in bytes. 
 */

void tarblock_makedefault(struct tarinfotype *tarinfoptr, const char *filename, long filesize)
{ int i;

  sprintf(tarinfoptr->name, "%s", filename);
  sprintf(tarinfoptr->mode, "%7o", TUREAD | TGREAD | TOREAD);
  sprintf(tarinfoptr->uid, "%7o", getuid());
  sprintf(tarinfoptr->gid, "%7o", 100);
  sprintf(tarinfoptr->size, "%11o", (guint)filesize);
  sprintf(tarinfoptr->mtime, "%11o", (guint)filesize);
  tarinfoptr->typeflag[0] = REGTYPE;
  *tarinfoptr->linkname = '\0';
  sprintf(tarinfoptr->magic, "%s", TMAGIC);
  sprintf(tarinfoptr->version, "%s", TVERSION);
  sprintf(tarinfoptr->uname, "%s", g_get_user_name ());
  sprintf(tarinfoptr->gname, "%s", "nobody");
  sprintf(tarinfoptr->devmajor, "%7o", 81);
  sprintf(tarinfoptr->devminor, "%7o", 0);
  sprintf(tarinfoptr->prefix, "./");
  for (i=0; i <= 11; i++) 
    tarinfoptr->pad[i] = ' ';

  tarblock_create_checksum(tarinfoptr);
}

/* tarblock_makedefault_symlink
 * Creates a default template for symlinks stored in a movtar.
 * out: tarinfoptr: The pointer to the tarblock to be created.
 * in:  filename: The filename of the new symlink.
 *      linkname: The filename of the file the symlink should point to.
 */

void tarblock_makedefault_symlink(struct tarinfotype *tarinfoptr, 
				  const char *filename, const char *linkname)
{ int i;

  sprintf(tarinfoptr->name, "%s", filename);
  sprintf(tarinfoptr->mode, "%7o", TUREAD | TGREAD | TOREAD);
  sprintf(tarinfoptr->uid, "%7o", getuid());
  sprintf(tarinfoptr->gid, "%7o", 100);
  sprintf(tarinfoptr->size, "%11o", 0);
  sprintf(tarinfoptr->mtime, "%11o", 0);
  tarinfoptr->typeflag[0] = SYMTYPE;
  sprintf(tarinfoptr->linkname, "%s", linkname);
  sprintf(tarinfoptr->magic, "%s", TMAGIC);
  sprintf(tarinfoptr->version, "%s", TVERSION);
  sprintf(tarinfoptr->uname, "nobody");
  sprintf(tarinfoptr->gname, "nobody");
  sprintf(tarinfoptr->devmajor, "%7o", 81);
  sprintf(tarinfoptr->devminor, "%7o", 0);
  sprintf(tarinfoptr->prefix, "./");
  for (i=0; i <= 11; i++) 
    tarinfoptr->pad[i] = ' ';

  tarblock_create_checksum(tarinfoptr);
}

/* tarblock_print
 * Prints parts of a given tarblock to stdout.
 * Mainly there for debugging purposes. 
 * in: tarinfoptr: the tarblock to be printed.
 */

void tarblock_print(struct tarinfotype *tarinfoptr)
{ int i;

  printf("The file name is \"%s.\"\n", tarinfoptr->name);

  printf("The User ID is  :");
  for (i = 0; i <= 7; i++)
    printf("%c", tarinfoptr->uid[i]);

  printf("\nThe Group ID is :");
  for (i = 0; i <= 7; i++)
    printf("%c", tarinfoptr->gid[i]);

  printf("\nThe size is :");
  for (i = 0; i <= 11; i++)
    printf("%c", tarinfoptr->size[i]);

  printf("\nThe checksum is :");
  for (i = 0; i <= 7; i++)
    printf("%c", tarinfoptr->chksum[i]);

  printf("\n");
}

/********************************************************
 * Movtar reading routines        *
 ********************************************************/
/* REM: In the moment these files are more or less just
   plain tar read/write routines, adapted for 
   reading/writing uniform files out of/into a huge stream */

/* movtar_read_tarblock
 * Reads the tarblock of a file into the according structure.
 * in: tarfile: the movtar file descriptor. 
 * in/out: tarinfoptr: pointer to the info structure to read to.
 * returns: the size of the file to come (or FALSE on error).
 */

int movtar_read_tarblock(FILE *tarfile, struct tarinfotype *tarinfoptr)
{
  int result;

  MDEBUG(printf("Entering movtar_read_tarblock.\n"));

  /* reading the first info block */
  result = fread(tarinfoptr, 512, 1, tarfile);
  if (result < 0) { perror("Can't read tar info"); exit(1); }                 

  if (feof(tarfile) || 
      ((tarinfoptr->name)[0] == '\0' && (tarinfoptr->name)[1] == '\0'))
  { MDEBUG(printf("End of tar file reached.\n")); 
    return FALSE;
  }

  MDEBUG({
    tarblock_print(tarinfoptr);
    printf("Comparing checksums:\n");
  });

  if (tarblock_checksum(tarinfoptr) != 
      tarblock_original_checksum(tarinfoptr))
  { MDEBUG(printf("Different checksums (%d != %d) !!\n", 
		  tarblock_checksum(tarinfoptr), 
		  tarblock_original_checksum(tarinfoptr))); 
    return FALSE;
  }

  MDEBUG(printf("Leaving movtar_read_tarblock.\n"));
  return tarblock_size(tarinfoptr);
}  

/* movtar_read_data
 * Reads the content of a tar'ed file into a buffer in memory.
 * in: tarfile: the movtar file descriptor. 
 *     filesize: The size of the file to read.
 * in/out: buffer:  the buffer to read to.
 * returns: TRUE on success (or FALSE on error).
 */
int movtar_read_data(FILE *tarfile, void *buffer, int filesize)
{
  char throw[512];
  int result;
  int readbufferptr;

  MDEBUG(printf("Entering movtar_read_data.\n"));

  readbufferptr = 0;
  while (filesize >= 512)
  { fread(buffer+readbufferptr, 512, 1, tarfile);
    filesize -= 512;
    readbufferptr += 512;
  }

  if (filesize > 0) // There is something left to read. 
    { MDEBUG(printf("movtar_read: Some bytes left. Filesize = %d\n", filesize)); 
      result = fread(buffer + readbufferptr, filesize, 1, tarfile);
      if (result != 1) return FALSE;
      result = fread(throw, 512 - filesize, 1, tarfile);
      if (result != 1) return FALSE;
    }

  MDEBUG(printf("Leaving movtar_read_data.\n"));
  return TRUE;
}

/* movtar_readto
 * Reads a file stored in a movtar to a given buffer. 
 * MAXFILESIZE defines the maximum size of a file read this way.
 * in: tarfile: the movtar file descriptor. 
 * out:buffer:  the buffer to read to.
 *     tarinfoptr: the tarblock read from the file
 *     bufferfilepos: The position of the plain file content in the file.
 * returns: the size of the read file (or FALSE on error).
 */
long movtar_readto(FILE *tarfile, void *buffer, 
			   struct tarinfotype *tarinfoptr)
{ int result; 
  size_t filesize;

  MDEBUG(printf("Entering movtar_readto.\n"));
 
  filesize = movtar_read_tarblock(tarfile, tarinfoptr);
  if (filesize == 0) 
    return FALSE;

  MDEBUG(printf("\nFilesize is: %d bytes.\n", filesize));
  if (filesize > MAXFILESIZE)
  { perror("movtar_readto: File too big to read. Aborting."); exit(1);
  }

  result = movtar_read_data(tarfile, buffer, filesize);  
  if (result == FALSE) return FALSE;
 
  MDEBUG(printf("Leaving movtar_readto.\n"));
  return filesize;
}

/* movtar_read
 * Reads a file stored in a movtar, and creates a content buffer. 
 * in: tarfile: the movtar file descriptor. 
 *     tarinfoptr: the tarblock read from the file
 * returns: void *: A pointer to the file content.
 */
void *movtar_read(FILE *tarfile, struct tarinfotype *tarinfoptr)
{
  int result; 
  size_t filesize;
  void *buffer;

  MDEBUG(printf("Entering movtar_read.\n"));

  filesize = movtar_read_tarblock(tarfile, tarinfoptr);
  if (filesize == 0) 
    return FALSE;

  MDEBUG 
    ({ printf("\nFilesize: %d Bytes.\n", filesize);
      printf("Allocating memory for the stored file\n");
    });
  buffer = (void *)malloc(filesize);  

  result = movtar_read_data(tarfile, buffer, filesize);
  if (result == FALSE) return FALSE;

  MDEBUG(printf("Leaving movtar_read.\n"));
  return buffer;
}

/* movtar_write
 * Stores a buffer in a movtar. 
 * in: tarfile: the movtar file descriptor. 
 *     buffer: A pointer to the content to write.
 * returns: int: TRUE if successfully written.
 */
int movtar_write(FILE *tarfile, void *buffer, size_t filesize, 
	      struct tarinfotype *tarinfoptr)
{ 
  int result; 
  size_t filepointer;

  fwrite(tarinfoptr, 512, 1, tarfile);

  MDEBUG(printf("movtar_write: Writing %d bytes\n", filesize));
  filepointer = 0;
  while (filesize >= 512)
  { result = fwrite(buffer + filepointer, 512, 1, tarfile);
    if (result != 1) return FALSE;
    filesize -= 512;
    filepointer += 512; 
  }
    
  if (filesize > 0) // There is something left. 
    { 
      MDEBUG(printf("movtar_write: Special handling. Filesize = %d\n", filesize)); 
      result = fwrite(buffer + filepointer, filesize, 1, tarfile);
      if (result != 1) return FALSE;
      result = fwrite(zeroes, 512 - filesize, 1, tarfile);
      if (result != 1) return FALSE;
    }
  return TRUE;
}

/* movtar_init
 * Initializes the movtar routines.
 * in: movtar_debug: Decides if the movtar routines should show debugging messages. 
 * returns: int: TRUE: initialization successful.
 */
int movtar_init(int debug)
{ int i;
  
  for (i=0; i < 512; i++) 
    zeroes[i] = 0;

  movtar_debug = debug;

  return TRUE;
}

/* movtar_create
 * Creates a movtar file. 
 * in: tarfilename: The filename for the new movtar file.
 * returns: FILE *: The file descriptor for the movtar.
 */
FILE *movtar_create(const char *tarfilename)
{ 
  return fopen(tarfilename, "w");
}

/* movtar_open
 * Opens an existing movtar file. 
 * in: tarfilename: The filename of the movtar file.
 * returns: FILE *: The file descriptor for the movtar.
 */
FILE *movtar_open(const char *tarfilename)
{ 
  return fopen(tarfilename, "r");
}

/* movtar_eof
 * tests if a movtar file has reached its data end (_not_ equal to eof() !)
 * in: FILE *: The file descriptor for the movtar.
 * returns: int: TRUE: end reached. FALSE: otherwise.
 */
int movtar_eof(FILE *tarfile)
{ char test[3];
  long filepos;

  filepos = ftell(tarfile);  
  fread(test, sizeof(char), 3, tarfile);
  fseek(tarfile, filepos, SEEK_SET);

  /* reached the two empty tarblocks (see tar.h) ? */
  if (test[0] == 0 && test[1] == 0 && test[2] == 0)
  { return TRUE;
  }
  else
    return FALSE;
}

/* movtar_finish_close
 * writes the finishing tarblocks (zeroed) and closes the file
 * in: FILE *: The file descriptor for the movtar.
 * returns: int: TRUE: file closed. FALSE: error during close.
 */
int movtar_finish_close(FILE *tarfile)
{ 
  /* Write the zeroes at the end of the tarfile */
  fwrite(zeroes, 1024, 1, tarfile);
  return fclose(tarfile);
}

int movtar_ignore_data(FILE *tarfile, struct tarinfotype *tarinfoptr)
{
  unsigned int filesize, hop_over;

  filesize = tarblock_size(tarinfoptr);
  hop_over = (filesize / 512) * 512;
  if ((filesize % 512) != 0) hop_over += 512;
  MDEBUG(printf("movtar_ignore: filesize: %d, hop_over: %d\n", filesize, hop_over));
  fseek(tarfile, hop_over, SEEK_CUR);
 
  return TRUE;
}

/* movtar_ignorefile
 * Ignores a file in the movtar, and hops on to the next one.
 * (but returns the tarblock of the ignored file).
 * in: tarfile: The movtar file.
 * out:tarinfoptr: The tarblock of the returned file.
 * returns: TRUE if successful, FALSE if already end of file reached.
*/
int movtar_ignore_file(FILE *tarfile, struct tarinfotype *tarinfoptr)
{ 
  int result; 

  /* reading the first info block */
  MDEBUG(printf("Reading tar info.\n"));
  result = fread(tarinfoptr, 512, 1, tarfile);
  if (result < 0) { perror("Can't read tar info"); exit(1); }                 

  if ((tarinfoptr->name)[0] == '\0' && (tarinfoptr->name)[1] == '\0')
  { MDEBUG(printf("End of file reached.")); 
    return FALSE;
  }

  if (movtar_ignore_data(tarfile, tarinfoptr) != TRUE) return FALSE;
  return TRUE;
}


/* 
 * The _real_ movtar routines (not tar based)
 */


/* movtar_forward_frames
 * Hops times frames forward in the file. 
 * in: tarfile: The movtar file.
 *     times: The number of files to hop over.
 * returns: TRUE if successful, FALSE if already end of file reached.
*/
int movtar_forward_frames(FILE *tarfile, int times)
{ int result;
  int i=0, datatype;
  struct tarinfotype tarinfo;
  long movfilepos; long movfilesize;

  MDEBUG(printf("Entering movtar_forward_frames.\n"));

  movfilepos = ftell(tarfile);  
  while (i < times)
  { 
    movfilepos = ftell(tarfile);  
    movfilesize = movtar_read_tarblock(tarfile, &tarinfo);
    if (movfilesize == 0) 
      return FALSE;

    datatype = movtar_datatype(&tarinfo);
    if (datatype & MOVTAR_DATA_VIDEO) i++;

    /* Fill up to the next 512 byte border */
    if (movfilesize % 512 != 0) 
      movfilesize += (512 - (movfilesize % 512));

    MDEBUG(printf("Moving the file pointer %ld forward.\n", movfilesize));
    /* Move the file pointer relatively to ignore the file data */ 
    result = fseek(tarfile, movfilesize, SEEK_CUR);
  }

  /* Make the last tarblock read "undone" */
  fseek(tarfile, movfilepos, SEEK_SET);

  MDEBUG(printf("Leaving movtar_forward_frames.\n"));
 
  return TRUE;
}

/* 
 * Highlevel movtar routines
 */

/* movtar_parse_info
 * Determines the type of data stored in a file.
 * in: tarinfoptr: Points to the tarblock containing the file info.
 * out: movtarinfo: 
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
  return MOVTAR_DATA_UNKNOWN;
}

/* movtar_parse_info
 * Parses a movtar INFO into a movtarinfotype struct.
 * in: infobuffer: The buffer containing the INFO content.
 * out: movtarinfo: 
 * returns: TRUE if valid movtar, FALSE if not.
*/
int movtar_parse_info(const char *infobuffer, struct movtarinfotype *movtarinfo)
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
        movtarinfo->version = g_strstrip(splitline[1]); valid = TRUE;
      };
      if (strncmp(strings[i], INFO_GEN_AUTHOR, 4) == 0)
      { splitline = g_strsplit(strings[i], ":", 0); found = TRUE;
        movtarinfo->gen_author = g_strstrip(splitline[1]);
      };
      if (strncmp(strings[i], INFO_GEN_DATE, 4) == 0)
      { splitline = g_strsplit(strings[i], ":", 0); found = TRUE;
        movtarinfo->gen_date = g_strstrip(splitline[1]);
      };
      if (strncmp(strings[i], INFO_GEN_SOFTWARE, 4) == 0)
      { splitline = g_strsplit(strings[i], ":", 0); found = TRUE;
        movtarinfo->gen_software = g_strstrip(splitline[1]);
      };
      if (strncmp(strings[i], INFO_GEN_DEVICE, 4) == 0)
      { splitline = g_strsplit(strings[i], ":", 0); found = TRUE;
        movtarinfo->gen_device = g_strstrip(splitline[1]);
      };
      if (strncmp(strings[i], INFO_GEN_CLASSIFICATION, 4) == 0)
      { splitline = g_strsplit(strings[i], ":", 0); found = TRUE;
        movtarinfo->gen_classification = g_strstrip(splitline[1]);
      };
      if (strncmp(strings[i], INFO_GEN_DESCRIPTION, 4) == 0)
      { splitline = g_strsplit(strings[i], ":", 0); found = TRUE;
        movtarinfo->gen_description = g_strstrip(splitline[1]);
      };
      if (strncmp(strings[i], INFO_GEN_JPEGKOMPR, 4) == 0)
      { splitline = g_strsplit(strings[i], ":", 0); found = TRUE;
        movtarinfo->gen_jpegkompr = atoi(splitline[1]);
      };
      if (strncmp(strings[i], INFO_GEN_INPUT, 4) == 0)
      { splitline = g_strsplit(strings[i], ":", 0); found = TRUE;
        movtarinfo->gen_input = g_strstrip(splitline[1]);
      };
      if (strncmp(strings[i], INFO_MOV_NORM, 4) == 0)
      { 
	splitline = g_strsplit(strings[i], ":", 0); found = TRUE;
	normstr = g_strstrip(splitline[1]);
	if (0 == strcasecmp(normstr,"pal"))
	  movtarinfo->mov_norm = VIDEO_MODE_PAL;
	else if (0 == strcasecmp(normstr,"ntsc"))
	  movtarinfo->mov_norm = VIDEO_MODE_NTSC;
	else if (0 == strcasecmp(normstr,"secam"))
	  movtarinfo->mov_norm = VIDEO_MODE_SECAM;
	else {
	  printf("Warning: Unknown tv norm %s. Defaulting to PAL\n", splitline[1]);
	  movtarinfo->mov_norm = VIDEO_MODE_PAL;      
	}
      };
      if (strncmp(strings[i], INFO_MOV_WIDTH, 4) == 0)
      { splitline = g_strsplit(strings[i], ":", 0); found = TRUE;
        movtarinfo->mov_width = atoi(splitline[1]);
      };
      if (strncmp(strings[i], INFO_MOV_HEIGHT, 4) == 0)
      { splitline = g_strsplit(strings[i], ":", 0); found = TRUE;
        movtarinfo->mov_height = atoi(splitline[1]);
      };
      if (strncmp(strings[i], INFO_SOUND_AVAIL, 4) == 0)
      { splitline = g_strsplit(strings[i], ":", 0); found = TRUE;
        movtarinfo->sound_avail = atoi(splitline[1]);
      };
      if (strncmp(strings[i], INFO_MOV_JPEGNUMFIELD, 4) == 0)
      { splitline = g_strsplit(strings[i], ":", 0); found = TRUE;
        movtarinfo->mov_jpegnumfield = atoi(splitline[1]);
      };
      if (strncmp(strings[i], INFO_SOUND_STEREO, 4) == 0)
      { splitline = g_strsplit(strings[i], ":", 0); found = TRUE;
        movtarinfo->sound_stereo = atoi(splitline[1]);
      };
      if (strncmp(strings[i], INFO_SOUND_SIZE, 4) == 0)
      { splitline = g_strsplit(strings[i], ":", 0); found = TRUE;
        movtarinfo->sound_size = atoi(splitline[1]);
      };
      if (strncmp(strings[i], INFO_SOUND_RATE, 4) == 0)
      { splitline = g_strsplit(strings[i], ":", 0); found = TRUE;
        movtarinfo->sound_rate = atoi(splitline[1]);
      };
      if (!found)
      MDEBUG(printf("Warning: Couldn't parse \"%s\"\n", strings[i]));
    }
    
    i++;
  }

  MDEBUG(printf("End of parsing the information.\n"));
  return TRUE;
}

/* movtar_write_info
 * Creates a 1024 byte header file, and writes it in a movtar. 
 * in: movtarinfo: The struct containing the movtar information.
 *     movtar: movtar file descriptor.

 * returns: TRUE if valid movtar, FALSE if not.
*/
void movtar_write_info(FILE *movtar, struct movtarinfotype *movtarinfo)
{ struct tarinfotype infotarinfo;
  gchar *infofilebuffer, *soundbuffer;
  int result;
 
  tarblock_makedefault(&infotarinfo, "INFO", 2048);

  if (movtarinfo->sound_avail)
  { soundbuffer = g_strdup_printf(
	              INFO_SOUND_STEREO "Stereo available : %d\n"
		      INFO_SOUND_SIZE "Sample size : %d\n"
		      INFO_SOUND_RATE "Sample rate : %d\n",
		      movtarinfo->sound_stereo, movtarinfo->sound_size, 
		      movtarinfo->sound_rate);
  }
  else
    soundbuffer = g_strdup_printf("%s", "");

  infofilebuffer = g_strdup_printf( 
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
	  INFO_SOUND_AVAIL "Sound : %d\n%s" 
	  INFO_EOF "\n",
	  movtarinfo->version, movtarinfo->gen_author, movtarinfo->gen_date, 
	  movtarinfo->gen_software, movtarinfo->gen_jpegkompr,
	  movtarinfo->gen_device, movtarinfo->gen_input, 
	  movtarinfo->gen_classification, movtarinfo->gen_description, 
	  movtarinfo->mov_width, movtarinfo->mov_height, 
	  (movtarinfo->mov_norm == 0) ? "PAL" : "NTSC", 
	  movtarinfo->mov_jpegnumfield, movtarinfo->sound_avail, soundbuffer);

  sprintf(infotarinfo.size, "%o", strlen(infofilebuffer));
  tarblock_create_checksum(&infotarinfo);
  result = movtar_write(movtar, infofilebuffer, strlen(infofilebuffer), &infotarinfo);
  if (result != TRUE) 
    { perror("Failed in writing movtar info file."); exit(1); 
    }

  free(infofilebuffer);
  free(soundbuffer);
}
















