#ifndef LAV_IO_H

#define LAV_IO_H

#ifdef COMPILE_LAV_IO_C
#include <avilib.h>

#ifdef BUILD_QUICKTIME
#include <quicktime.h>
#endif

#ifdef BUILD_MOVTAR
#include <movtar.h>
#endif
#else
typedef void avi_t;
typedef void quicktime_t;
typedef void movtar_t;
#endif

#define LAV_NOT_INTERLACED      0
#define LAV_INTER_ODD_FIRST     1
#define LAV_INTER_EVEN_FIRST    2
#define LAV_INTER_UNKNOWN      99

/* chroma_format */
#define CHROMAUNKNOWN 0
#define CHROMA420 1
#define CHROMA422 2
#define CHROMA444 3

typedef struct
{
   avi_t       *avi_fd;
#ifdef BUILD_QUICKTIME
   quicktime_t *qt_fd;
#endif
#ifdef BUILD_MOVTAR
   movtar_t    *movtar_fd;
#endif
   int         format;
   int         interlacing;
   int         has_audio;
   int         bps;
   int         is_MJPG;
   int         MJPG_chroma;
} lav_file_t;

int  lav_query_APP_marker(char format);
int  lav_query_APP_length(char format);
int  lav_query_polarity(char format);
lav_file_t *lav_open_output_file(char *filename, char format,
                    int width, int height, int interlaced, double fps,
                    int asize, int achans, long arate);
int  lav_close(lav_file_t *lav_file);
int  lav_write_frame(lav_file_t *lav_file, char *buff, long size, long count);
int  lav_write_audio(lav_file_t *lav_file, char *buff, long samps);
long lav_video_frames(lav_file_t *lav_file);
int  lav_video_width(lav_file_t *lav_file);
int  lav_video_height(lav_file_t *lav_file);
double lav_frame_rate(lav_file_t *lav_file);
int  lav_video_interlacing(lav_file_t *lav_file);
int  lav_video_is_MJPG(lav_file_t *lav_file);
int  lav_video_MJPG_chroma(lav_file_t *lav_file);
char *lav_video_compressor(lav_file_t *lav_file);
int  lav_audio_channels(lav_file_t *lav_file);
int  lav_audio_bits(lav_file_t *lav_file);
long lav_audio_rate(lav_file_t *lav_file);
long lav_audio_samples(lav_file_t *lav_file);
long lav_frame_size(lav_file_t *lav_file, long frame);
int  lav_seek_start(lav_file_t *lav_file);
int  lav_set_video_position(lav_file_t *lav_file, long frame);
int  lav_read_frame(lav_file_t *lav_file, char *vidbuf);
int  lav_set_audio_position(lav_file_t *lav_file, long sample);
long lav_read_audio(lav_file_t *lav_file, char *audbuf, long samps);
int  lav_filetype(lav_file_t *lav_file);
lav_file_t *lav_open_input_file(char *filename);
int  lav_get_field_size(unsigned char * jpegdata, long jpeglen);
char *lav_strerror();

#endif
