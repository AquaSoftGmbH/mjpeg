/* Helping routines for handling the movtar format. 
   (general tar handling routines)
   Written and (c) by Gernot Ziegler in 1999.
   
   This source is released under the GNU LGPL license ... 

   Comment, bug fixes, etc. are welcomed.
   /gz <gz@lysator.liu.se>
*/

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <tar.h>
#include <glib.h>

#include <sys/types.h>
#include <linux/types.h>

#include "movtar.h"

static int tar_debug = 0;
static char zeroes[512];

#define TDEBUG(x) if (tar_debug)  x 

/********************************************************
 * tar info header (= tarblock) handling routines       *
 ********************************************************/

/* tarblock_size
 * Retrieves the size of the file stored in the tar.
 * in: tarinfo: The tarblock to be processed.
 * returns: unsigned int.: the size of the associated file.
 */

unsigned long tarblock_size(struct tarinfotype *tarinfoptr)
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
 * Creates a default template for files stored in a tar.
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
 * Creates a default template for symlinks stored in a tar.
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
 * Tar reading routines        *
 ********************************************************/
/* REM: In the moment these files are more or less just
   plain tar read/write routines, adapted for 
   reading/writing uniform files out of/into a huge stream */

/* tar_read_tarblock
 * Reads the tarblock of a file into the according structure.
 * in: tarfile: the tar file descriptor. 
 * in/out: tarinfoptr: pointer to the info structure to read to.
 * returns: the size of the file to come (or FALSE on error).
 */

unsigned long tar_read_tarblock(FILE *tarfile, struct tarinfotype *tarinfoptr)
{
  unsigned long result;

  TDEBUG(printf("Entering tar_read_tarblock.\n"));

  /* reading the first info block */
  result = fread(tarinfoptr, 512, 1, tarfile);
  if (result < 0) { perror("Can't read tar info"); exit(1); }                 

  if (feof(tarfile) || 
      ((tarinfoptr->name)[0] == '\0' && (tarinfoptr->name)[1] == '\0'))
  { TDEBUG(printf("End of tar file reached.\n")); 
    return FALSE;
  }

  TDEBUG({
    tarblock_print(tarinfoptr);
    printf("Comparing checksums:\n");
  });

  if (tarblock_checksum(tarinfoptr) != 
      tarblock_original_checksum(tarinfoptr))
  { TDEBUG(printf("Different checksums (%d != %d) !!\n", 
		  tarblock_checksum(tarinfoptr), 
		  tarblock_original_checksum(tarinfoptr))); 
    return FALSE;
  }

  TDEBUG(printf("Leaving tar_read_tarblock.\n"));
  return tarblock_size(tarinfoptr);
}  

/* tar_read_data
 * Reads the content of a tar'ed file into a buffer in memory.
 * in: tarfile: the tar file descriptor. 
 *     filesize: The size of the file to read.
 * in/out: buffer:  the buffer to read to.
 * returns: TRUE on success (or FALSE on error).
 */
int tar_read_data(FILE *tarfile, void *buffer, int filesize)
{
  char throw[512];
  int result;
  int readbufferptr;

  TDEBUG(printf("Entering tar_read_data.\n"));

  readbufferptr = 0;
  while (filesize >= 512)
  { fread(buffer+readbufferptr, 512, 1, tarfile);
    filesize -= 512;
    readbufferptr += 512;
  }

  if (filesize > 0) // There is something left to read. 
    { TDEBUG(printf("tar_read: Some bytes left. Filesize = %d\n", filesize)); 
      result = fread(buffer + readbufferptr, filesize, 1, tarfile);
      if (result != 1) return FALSE;
      result = fread(throw, 512 - filesize, 1, tarfile);
      if (result != 1) return FALSE;
    }

  TDEBUG(printf("Leaving tar_read_data.\n"));
  return TRUE;
}

/* tar_readto
 * Reads a file stored in a tar to a given buffer. 
 * MAXFILESIZE defines the maximum size of a file read this way.
 * in: tarfile: the tar file descriptor. 
 * out:buffer:  the buffer to read to.
 *     tarinfoptr: the tarblock read from the file
 *     bufferfilepos: The position of the plain file content in the file.
 * returns: the size of the read file (or FALSE on error).
 */
long tar_readto(FILE *tarfile, void *buffer, 
			   struct tarinfotype *tarinfoptr)
{ int result; 
  size_t filesize;

  TDEBUG(printf("Entering tar_readto.\n"));
 
  filesize = tar_read_tarblock(tarfile, tarinfoptr);
  if (filesize == 0) 
    return FALSE;

  TDEBUG(printf("\nFilesize is: %d bytes.\n", filesize));
  if (filesize > MAXFILESIZE)
  { perror("tar_readto: File too big to read. Aborting."); exit(1);
  }

  result = tar_read_data(tarfile, buffer, filesize);  
  if (result == FALSE) return FALSE;
 
  TDEBUG(printf("Leaving tar_readto.\n"));
  return filesize;
}

/* tar_read
 * Reads a file stored in a tar, and creates a content buffer. 
 * in: tarfile: the tar file descriptor. 
 *     tarinfoptr: the tarblock read from the file
 * returns: void *: A pointer to the file content.
 */
void *tar_read(FILE *tarfile, struct tarinfotype *tarinfoptr)
{
  int result; 
  size_t filesize;
  void *buffer;

  TDEBUG(printf("Entering tar_read.\n"));

  filesize = tar_read_tarblock(tarfile, tarinfoptr);
  if (filesize == 0) 
    return FALSE;

  TDEBUG 
    ({ printf("\nFilesize: %d Bytes.\n", filesize);
      printf("Allocating memory for the stored file\n");
    });
  buffer = (void *)malloc(filesize);  

  result = tar_read_data(tarfile, buffer, filesize);
  if (result == FALSE) return FALSE;

  TDEBUG(printf("Leaving tar_read.\n"));
  return buffer;
}

/* tar_write
 * Stores a buffer in a tar. 
 * in: tarfile: the tar file descriptor. 
 *     buffer: A pointer to the content to write.
 * returns: int: TRUE if successfully written.
 */
int tar_write(FILE *tarfile, void *buffer, size_t filesize, 
	      struct tarinfotype *tarinfoptr)
{ 
  fwrite(tarinfoptr, 512, 1, tarfile);
  fwrite(buffer, filesize, 1, tarfile);
  tar_pad_with_zeroes(tarfile, filesize);
  
  return TRUE;
}

/* tar_eof
 * tests if a tar file has reached its data end (_not_ equal to eof() !)
 * in: FILE *: The file descriptor for the tar.
 * returns: int: TRUE: end reached. FALSE: otherwise.
 */
int tar_eof(FILE *tarfile)
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

/* tar_finish_close
 * writes the finishing tarblocks (zeroed), and closes the file
 * in: FILE *: The file descriptor for the tar.
 * returns: int: TRUE: file closed. FALSE: error during close.
 */
int tar_finish_close(FILE *tarfile)
{ 
  /* Write the zeroes at the end of the tarfile */
  fwrite(zeroes, 1024, 1, tarfile);
  return fclose(tarfile);
}

/* tar_finish_truncate_close
 * the more radical version:
 * writes the finishing tarblocks (zeroed), truncates and closes the file
 * in: FILE *: The file descriptor for the tar.
 * returns: int: TRUE: file closed. FALSE: error during close.
 */
int tar_finish_truncate_close(FILE *tarfile)
{ 
  /* Write the zeroes at the end of/current pos in the tarfile */
  fwrite(zeroes, 1024, 1, tarfile);
  if (ftruncate(fileno(tarfile), ftell(tarfile)) == -1)
    perror("An error occured whilst truncating the tarfile\n");
  return fclose(tarfile);
}

int tar_ignore_data(FILE *tarfile, struct tarinfotype *tarinfoptr)
{
  unsigned int filesize, hop_over;

  filesize = tarblock_size(tarinfoptr);
  hop_over = (filesize / 512) * 512;
  if ((filesize % 512) != 0) hop_over += 512;
  TDEBUG(printf("tar_ignore: filesize: %d, hop_over: %d\n", filesize, hop_over));
  fseek(tarfile, hop_over, SEEK_CUR);
 
  return TRUE;
}

/* tar_ignorefile
 * Ignores a file in the tar, and hops on to the next one.
 * (but returns the tarblock of the ignored file).
 * in: tarfile: The tar file.
 * out:tarinfoptr: The tarblock of the returned file.
 * returns: TRUE if successful, FALSE if already end of file reached.
*/
int tar_ignore_file(FILE *tarfile, struct tarinfotype *tarinfoptr)
{ 
  int result; 

  /* reading the first info block */
  TDEBUG(printf("Reading tar info.\n"));
  result = fread(tarinfoptr, 512, 1, tarfile);
  if (result < 0) { perror("Can't read tar info"); exit(1); }                 

  if ((tarinfoptr->name)[0] == '\0' && (tarinfoptr->name)[1] == '\0')
  { TDEBUG(printf("End of file reached.")); 
    return FALSE;
  }

  if (tar_ignore_data(tarfile, tarinfoptr) != TRUE) return FALSE;
  return TRUE;
}

int tar_pad_with_zeroes(FILE *tarfile, long filesize)
{
  /* no padding if filesize goes round*/
  int bytes_to_pad = (filesize % 512 == 0) ? 0 : (512 - (filesize % 512));
  
  fwrite(zeroes, bytes_to_pad, 1, tarfile);

  return 1;
}

int tar_init(int debug)
{
  gchar *debug_string; 
  int i;

  tar_debug = debug;

  debug_string = getenv("DEBUG_TAR");
  if (debug_string) tar_debug = 1;

  for (i=0; i < 512; i++) 
    zeroes[i] = 0;

  return 1;
}
