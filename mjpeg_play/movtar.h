/* Include file for the movtar functions, 
 *  I didn't want to do it, but now my strictness
 *  forced me to introduce it :-) ... 
 */

/* Depends on: stdio.h, tar.h */

/* A struct which contains the parsed information
   from the INFO file, and is used for writing 
   it back, too ...  */
struct movtarinfotype
{ char *version;

  char *gen_author;
  char *gen_date;
  char *gen_software;
  int gen_jpegkompr;
  char *gen_device;
  char *gen_input;  
  char *gen_classification;  
  char *gen_description;  

  int mov_norm; /* corresponds to VIDEO_NORM_* in videodev.h */
  int mov_width; /* The frame width */
  int mov_height; /* The frame height */
  int mov_jpegnumfield; /* The number of partial frames in one file; 1 or 2 */

  int sound_avail; /* Sound available ? */
  int sound_stereo; /* Stereo in the raw audio files ? */
  int sound_size; /* 8 or 16 bit samples ? */ 
  int sound_rate; /* The sampling rate */
};

/* This struct contains the information 
   a tar file has for each of the stored files
   (see also /usr/include/tar.h)  */
struct tarinfotype
{ char name[100];
  char mode[8];
  char uid[8];
  char gid[8];
  char size[12];
  char mtime[12];
  char chksum[8];
  char typeflag[1];
  char linkname[100];
  char magic[6];
  char version[2];
  char uname[32];

  char gname[32];
  char devmajor[8];
  char devminor[8];
  char prefix[155];
  char pad[12];
};

/* These tags define the possible datatags 
   returned by movtar_getdatatype */
#define MOVTAR_DATA_AUDIO   0x8000
#define MOVTAR_DATA_VIDEO   0x0800
#define MOVTAR_DATA_FAKE    0x0001
#define MOVTAR_DATA_UNKNOWN 0x0000

/* The function prototypes */
unsigned int tarblock_size(struct tarinfotype *tarinfoptr);
unsigned int tarblock_checksum(struct tarinfotype *tarinfoptr);
void tarblock_create_checksum(struct tarinfotype *tarinfoptr);
unsigned int tarblock_original_checksum(struct tarinfotype *tarinfoptr);
void tarblock_makedefault(struct tarinfotype *tarinfoptr, 
			  const char *filename, long filesize);
void tarblock_makedefault_symlink(struct tarinfotype *tarinfoptr, 
				  const char *filename, const char *linkname);

void tarblock_print(struct tarinfotype *tarinfoptr);

int movtar_read_tarblock(FILE *tarfile, struct tarinfotype *tarinfoptr);
int movtar_read_data(FILE *tarfile, void *buffer, int filesize);
long movtar_readto(FILE *tarfile, void *buffer, 
			   struct tarinfotype *tarinfoptr);

/* movtar_read
 * Reads a file stored in a movtar, and creates a content buffer. 
 * in: tarfile: the movtar file descriptor. 
 *     tarinfoptr: the tarblock read from the file
 * returns: void *: A pointer to the file content.
 */
void *movtar_read(FILE *tarfile, struct tarinfotype *tarinfoptr);

/* movtar_write
 * Stores a buffer in a movtar. 
 * in: tarfile: the movtar file descriptor. 
 *     buffer: A pointer to the content to write.
 * returns: int: TRUE if successfully written.
 */
int movtar_write(FILE *tarfile, void *buffer, size_t filesize, 
	      struct tarinfotype *tarinfoptr);

/* movtar_init
 * Initializes the movtar routines.
 * in: movtar_debug: Decides if the movtar routines should show debugging messages. 
 * returns: int: TRUE: initialization successful.
 */
int movtar_init(int debug);

/* movtar_create
 * Creates a movtar file. 
 * in: tarfilename: The filename for the new movtar file.
 * returns: FILE *: The file descriptor for the movtar.
 */
FILE *movtar_create(const char *tarfilename);

/* movtar_open
 * Opens an existing movtar file. 
 * in: tarfilename: The filename of the movtar file.
 * returns: FILE *: The file descriptor for the movtar.
 */
FILE *movtar_open(const char *tarfilename);

/* movtar_eof
 * tests if a movtar file has reached its data end (_not_ equal to eof() !)
 * in: FILE *: The file descriptor for the movtar.
 * returns: int: TRUE: end reached. FALSE: otherwise.
 */
int movtar_eof(FILE *tarfile);

/* movtar_finish_close
 * writes the finishing tarblocks (zeroed) and closes the file
 * in: FILE *: The file descriptor for the movtar.
 * returns: int: TRUE: file closed. FALSE: error during close.
 */
int movtar_finish_close(FILE *tarfile);
int movtar_ignore_data(FILE *tarfile, struct tarinfotype *tarinfoptr);
/* movtar_ignorefile
 * Ignores a file in the movtar, and hops on to the next one.
 * (but returns the tarblock of the ignored file).
 * in: tarfile: The movtar file.
 * out:tarinfoptr: The tarblock of the returned file.
 * returns: TRUE if successful, FALSE if already end of file reached.
*/
int movtar_ignore_file(FILE *tarfile, struct tarinfotype *tarinfoptr);

/* 
 * The _real_ movtar routines (not tar based)
 */


/* movtar_forward_frames
 * Hops times frames forward in the file. 
 * in: tarfile: The movtar file.
 *     times: The number of files to hop over.
 * returns: TRUE if successful, FALSE if already end of file reached.
*/
int movtar_forward_frames(FILE *tarfile, int times);

/* 
 * Highlevel movtar routines
 */

/* movtar_parse_info
 * Determines the type of data stored in a file.
 * in: tarinfoptr: Points to the tarblock containing the file info.
 * out: movtarinfo: 
 * returns: TRUE if valid movtar, FALSE if not.
*/
int movtar_datatype(struct tarinfotype *tarinfoptr);

/* movtar_parse_info
 * Parses a movtar INFO into a movtarinfotype struct.
 * in: infobuffer: The buffer containing the INFO content.
 * out: movtarinfo: 
 * returns: TRUE if valid movtar, FALSE if not.
*/
int movtar_parse_info(const char *infobuffer, struct movtarinfotype *movtarinfo);

/* movtar_write_info
 * Creates a 1024 byte header file, and writes it in a movtar. 
 * in: movtarinfo: The struct containing the movtar information.
 *     movtar: movtar file descriptor.

 * returns: TRUE if valid movtar, FALSE if not.
*/
void movtar_write_info(FILE *movtar, struct movtarinfotype *movtarinfo);





