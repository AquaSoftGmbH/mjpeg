/* Include file for the movtar functions, 
 */

/* Depends on: stdio.h, tar.h, glib.h */

#include <stdio.h>
#include <glib.h>

#define MAXFILESIZE 300000
#define MAXINDEXFILESIZE 1000000

typedef struct 
{
  gint64 pos; /* Shows the file position of this frag */
  unsigned long size;
  int flags;
} movtar_audio_frag_t;

#define MOVTAR_VIDEO_FAKE 0x1

typedef struct 
{
  gint64 pos;
  unsigned long size;
  int flags;
} movtar_video_frag_t;

#define MOVTAR_NORM_NTSC  0x001
#define MOVTAR_NORM_PAL   0x002
#define MOVTAR_NORM_SECAM 0x004
#define MOVTAR_NORM_UNKNOWN 0x1000

#define MOVTAR_MODE_READ       0x1
#define MOVTAR_MODE_WRITE      0x2
#define MOVTAR_MODE_READWRITE  0x3

#define MOVTAR_FLAG_SHOW_FAKE  0x1
#define MOVTAR_FLAG_NO_INDEXBUILD  0x2 /* Set this flag only if you know EXACTLY what you are doing ! */

typedef struct
{
  char *filepath;  /* the path of the movtar (Use the glib functions to extract the parts) */
  char *name;      /* The filename of the movtar */
  FILE *file; /* The movtar file descriptor */

  /* This information is stored in/extraced from the INFO header file, 
     it is not needed for movtarlib:s operation */
  char *version; /* The movtar version string */

  char *gen_author; /* The autor of the file */
  char *gen_date;   /* The generation date */
  char *gen_software; /* The used software for creation */
  int gen_jpeg_quality;  /* it's the JPEG quality ! */
  char *gen_device;   /* The creating video device */
  char *gen_input;    /* The used video input */
  char *gen_classification;  /* A general classification of the movtar content */
  char *gen_description;     /* A longer description text */

  float mov_framerate; /* The movie framerate */
  int mov_norm; /* contains video norm */
  int mov_width; /* The frame width */
  int mov_height; /* The frame height */
  int mov_jpegnumfield; /* The number of partial frames in one file; 1 or 2 */

  int sound_avail; /* Sound available ? */
  int sound_stereo; /* Stereo in the raw audio files ? */
  int sound_size; /* 8 or 16 bit samples ? */ 
  int sound_rate; /* The sampling rate */

  int index_avail; /* For reading: Index file available ? */
  unsigned long long video_index_pos, audio_index_pos; /*The indexes position in the file, incl. tar header*/

  long audnr, vidnr; /*current video frame position, and audio frame position (index into audio/video_table)*/

  gint64 audio_pos; /* the absolute audio position */
  gint64 audio_rel_pos; /* the relative audio position (in the current frag) */
  gint64 audio_length; /* the total number of audio samples */

  GArray *audio_table; /* pointing to the audio fragments in the file, GLib Array */
  GArray *video_table; /* pointing to the video frames in the file, GLib Array  */

  long latest_real_video_frag; /* Stores the number of the latest frame that was _not_ fake */
  int mode; /* Signals the lib the opening state of the file: read, write, read/write */
  int show_fake_mode; /* Signals the lib that fake frames should be shown with frame size set to 1 byte
			 (and not hidden in special handling) */
  int INFO_written; /* Internal: Signals movtarlib that the INFO file has been written */
  int rtjpeg_mode; /* Signals if the movtar contains RTJPEG frames instead of "normal" JPEG frames */

#ifdef	NEVER
  fpos_t writepos; /*to be able to measure the bytes the application has written */
#else
  size_t   writepos;
#endif

  gint32 flags; /* This stores all the given flags that were passed in movtar_open */
} movtar_t;

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
#define MOVTAR_DATA_VIDEO_RTJPEG  0x0900
#define MOVTAR_DATA_FAKE    0x0001
#define MOVTAR_DATA_UNKNOWN 0x0000

/* The function prototypes */
unsigned long tarblock_size(struct tarinfotype *tarinfoptr);
unsigned int tarblock_checksum(struct tarinfotype *tarinfoptr);
void tarblock_create_checksum(struct tarinfotype *tarinfoptr);
unsigned int tarblock_original_checksum(struct tarinfotype *tarinfoptr);
void tarblock_makedefault(struct tarinfotype *tarinfoptr, 
			  const char *filename, long filesize);
void tarblock_makedefault_symlink(struct tarinfotype *tarinfoptr, 
				  const char *filename, const char *linkname);

void tarblock_print(struct tarinfotype *tarinfoptr);

unsigned long tar_read_tarblock(FILE *tarfile, struct tarinfotype *tarinfoptr);
int tar_read_data(FILE *tarfile, void *buffer, int filesize);
long tar_readto(FILE *tarfile, void *buffer, 
			   struct tarinfotype *tarinfoptr);
int tar_ignore_data(FILE *tarfile, struct tarinfotype *tarinfoptr);
int tar_eof(FILE *tarfile);
int tar_finish_close(FILE *tarfile);
int tar_finish_truncate_close(FILE *tarfile);

/* movtar_read
 * Reads a file stored in a movtar, and creates a content buffer. 
 * in: tarfile: the movtar file descriptor. 
 *     tarinfoptr: the tarblock read from the file
 * returns: void *: A pointer to the file content.
 */
void *tar_read(FILE *tarfile, struct tarinfotype *tarinfoptr);

/* movtar_write
 * Stores a buffer in a movtar. 
 * in: tarfile: the movtar file descriptor. 
 *     buffer: A pointer to the content to write.
 * returns: int: TRUE if successfully written.
 */
int tar_write(FILE *tarfile, void *buffer, size_t filesize, 
	      struct tarinfotype *tarinfoptr);
int tar_init(int debug);

/* Adds the required padding bytes (given: the written filesize) at the current file position */
int tar_pad_with_zeroes(FILE *tarfile, long filesize);

/* movtar_init
 * Initializes the movtar routines.
 * in: movtar_debug: Decides if the movtar routines should show debugging messages. 
 * returns: int: TRUE: initialization successful.
 */
int movtar_init(int debug, int tar_debug);

/* movtar_open
 * Opens an existing movtar file. 
 * in: tarfilename: The filename of the movtar file.
 * returns: FILE *: The file descriptor for the movtar.
 */
movtar_t *movtar_open(const char *tarfilename, int read, int write, gint32 flags);

/* movtar_eof
 * tests if a movtar file has reached its data end (_not_ equal to eof() !)
 * in: FILE *: The file descriptor for the movtar.
 * returns: int: 1: end reached. 
                 0x10: There is more data after this file, linked to the next. Call movtar_add_linked_file.
		 FALSE: otherwise.
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
//int movtar_ignore_file(movtar_t *tarfile, struct tarinfotype *tarinfoptr);


void movtar_show_fake_frames(movtar_t *movtar, int yes);

/* movtar_forward_frames
 * Hops times frames forward in the file. 
 * in: tarfile: The movtar file.
 *     times: The number of files to hop over.
 * returns: TRUE if successful, FALSE if already end of file reached.
*/
int movtar_forward_frames(movtar_t *movtar, int times);

/* movtar_datatype
 * Determines the type of data stored in a file.
 * in: tarinfoptr: Points to the tarblock containing the file info.
 * out: movtarinfo: 
 * returns: TRUE if valid movtar, FALSE if not.
*/
int movtar_datatype(struct tarinfotype *tarinfoptr);

gchar *movtar_create_info_content (movtar_t *movtar);

/* movtar_parse_info
 * Parses a movtar INFO into a movtarinfotype struct.
 * in: infobuffer: The buffer containing the INFO content.
 * out: movtarinfo: 
 * returns: TRUE if valid movtar, FALSE if not.
*/
int movtar_parse_info(movtar_t *movtar, const char *infobuffer);

/* movtar_write_info
 * Creates a 1024 byte header file, and writes it in a movtar. 
 * in: movtarinfo: The struct containing the movtar information.
 *     movtar: movtar file descriptor.

 * returns: TRUE if valid movtar, FALSE if not.
*/
void movtar_write_info(movtar_t *movtar);

/* This returns 1 if it looks like a movtar file or 0 if it doesn't. Then you can open the file. */
int movtar_check_sig(char *path);

/* You'll only encounter a single audio track. Inside the audio track 
   is a variable number of channels. To get the channel count (stereo: 2, mono: 1, for example): */
int movtar_track_channels(movtar_t *movtar); 

/* Other routines you might find useful for getting audio information are: */
/* quicktime_audio_length gives you the total number of samples. The sample rate is samples per second. */
gint64 movtar_audio_length(movtar_t *movtar);
int movtar_audio_tracks(movtar_t *movtar);
int movtar_sample_rate(movtar_t *movtar);

/* The bits parameter returns the number of bits in an audio sample. */
int movtar_audio_bits(movtar_t *movtar);

/*
  The video length is in frames. The width and height are in
  pixels. The frame rate is in frames per second. 
  Depth returns the total number of bits per pixel. These are not the
  number of bits per pixel on disk but the number of bits per pixel in 
  the decompressed data returned by one of the
  video decoding routines. */
int movtar_video_tracks(movtar_t *movtar);
long movtar_video_length(movtar_t *movtar);
int movtar_video_width(movtar_t *movtar);
int movtar_video_height(movtar_t *movtar);
int movtar_video_tracks(movtar_t *movtar);
int movtar_audio_tracks(movtar_t *movtar);
float movtar_frame_rate(movtar_t *movtar);
// int movtar_video_depth(movtar_t *movtar); the movtar lib does NOT decompress video !

/* Tells if the stored audio format can be decompressed by this lib */
int movtar_supported_audio(movtar_t *movtar);

/* Decodes the audio information in the movtar into PCM samples */
int movtar_decode_audio(movtar_t *movtar, short int *output_i, float *output_f, size_t samples);

/* gives up the number of bytes in the specified frame in the specified track even if you haven't read the frame yet. Frame numbers start on 0. */
long movtar_frame_size(movtar_t *movtar, long frame);

/* movtar_read_frame reads one frame worth of raw data from your current position on the 
   specified video track and returns the number of bytes in the frame. 
   You have to make sure the buffer is big enough for the frame. A return value
   of 0 means error. */
long movtar_read_frame(movtar_t *movtar, unsigned char *video_buffer); 

/* Now some of you are going to want to read frames directly from a file descriptor using another 
   library like libjpeg or something. To read a frame directly start by calling */
int movtar_read_frame_init(movtar_t *movtar); 

/* Then read your raw, compressed data from a file descriptor given by */
FILE* movtar_get_fd(movtar_t *movtar); 

/* End the frame read operation by calling
   You can get the file descriptor any time the file is opened, not just when reading or writing, 
   but you must call the init
   and end routines to read a frame. 
*/
int movtar_read_frame_end(movtar_t *movtar); 

/*
  These commands are good for reading raw sample data. They should only be used for codecs 
  not supported in the library and only work for interleaved, linear PCM data.
  movtar_read_audio requires a number of samples of raw audio data to read. Then it reads that 
  corresponding number of bytes on the specified track and returns the equivalent number of bytes read 
  or 0 if error. The data read is PCM audio data of interleaved channels depending on the format
  of the track. Be aware that Movtar for Linux tries
  to guess the number of bytes by the codec type, so attempts to read most nonlinear codecs will crash. 
*/
gint64 movtar_read_audio(movtar_t *movtar, char *audio_buffer, gint64 samples); 

/*
  The seek_end and seek_start seek all tracks to their ends or starts. 
  The set_position commands seek one track to the
  desired position. The track parameter for audio is always going to be 0.
*/
int movtar_seek_end(movtar_t *movtar);
int movtar_seek_start(movtar_t *movtar);
int movtar_set_audio_position(movtar_t *movtar, long sample);
int movtar_set_video_position(movtar_t *movtar, long frame);
gint64 movtar_audio_bytes(movtar_t *movtar, gint64 samples);
gint64 movtar_audio_samples(movtar_t *movtar, gint64 bytes);

/* Immediately after opening the file, set up some tracks to write with these commands: */
/* Don't call the audio command if you don't intend to store any audio data. 
   Some programs rely on an audio track for video timing. */
int movtar_set_audio(movtar_t *movtar, int channels, int sample_rate, int bits, char *compressor);

/* Likewise, don't call the video command if you're just going to save audio. */
int movtar_set_video(movtar_t *movtar, int tracks, int frame_w, int frame_h, 
		      float frame_rate, char *compressor, int interlaced); 
void movtar_show_fake_frames(movtar_t *movtar, int yes);

/* Now some of you are going to want to write frames directly to a file descriptor using another library like libjpeg or something. For every frame start by calling movtar_write_frame_init to initialize the output. */
int movtar_write_frame_init(movtar_t *movtar); 

/* Then write your raw, compressed data to the file descriptor given by 
   the function movtar_get_fd. */
/* End the frame by calling movtar_write_frame_end. 
   (If you wrote zero bytes, then this frame is assumed to be fake 
   (a copy of the last real one) */
int movtar_write_frame_end(movtar_t *movtar); 

/* Repeat starting at movtar_write_frame_init for every frame. */

/* For writing raw data, you need to supply a buffer of data exactly as you intend the 
   read operations to see it, with the encoding done, then call one of these functions 
   to write it. For video, specify the number of bytes in the frame buffer
   and the track this frame belongs to. Video can only be written one frame at a time. */
int movtar_write_frame(movtar_t *movtar, unsigned char *video_buffer, size_t bytes); 

/* Writing raw audio data */

/* Writing audio involves writing the raw audio data exactly the way the read operations 
   are going to see it, with channels interleaved and whatever else. */
/* The library automatically converts the sample count to the number of bytes
   of data in the buffer, based on channels and bits values you passed to movtar_set_audio. */
int movtar_write_audio(movtar_t *movtar, char *audio_buffer, size_t samples); 

/* Copies the information in a movtar header to another movtar header
   (useful if you want to copy the contents of an input file to an output file, for example) */
void movtar_copy_header(movtar_t *src, movtar_t *dest);
void movtar_copy_mov_header(movtar_t *src, movtar_t *dest);
void movtar_copy_sound_header(movtar_t *src, movtar_t *dest);

/* Returns a string description of the used TV norm in the movtar */
gchar *movtar_norm_string(movtar_t *movtar);

/* Destroys the movtar struct and all its internal allocations */
void movtar_destroy_struct(movtar_t *movtar);

/* Closes the movtar, and writes the index (in write mode and read/write mode) */
int movtar_close(movtar_t *movtar);





