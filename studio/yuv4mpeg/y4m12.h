/* toplevel library to support both yuv4mpeg1 (mjpegtools-1.4)
   as well as yuv4mpeg2 (mjpegtools-1.5) */

#include "yuv4mpeg-1.4.h"
#ifdef HAVE_MJPEGTOOLS_15
#include <stdint.h>
#include <yuv4mpeg.h>
#endif

typedef struct _y4m12_t {
  unsigned char *buffer[3];

  /* 140 for 1.4, 150 for 1.5 */
  short version;

  /* width/height of yuv */
  int width;
  int height;

  /* in case of 1.4 */
  int v_140_width;
  int v_140_height;
  int v_140_frameratecode;
#ifdef HAVE_MJPEGTOOLS_15
  /* in case of 1.5 */
  y4m_stream_info_t *v_150_streaminfo;
  y4m_frame_info_t *v_150_frameinfo;
#endif
} y4m12_t;

y4m12_t *y4m12_malloc (void);

int y4m12_read_header (y4m12_t *y4m12, int fd);
int y4m12_read_frame  (y4m12_t *y4m12, int fd);

int y4m12_write_header(y4m12_t *y4m12, int fd);
int y4m12_write_frame (y4m12_t *y4m12, int fd);

void y4m12_free       (y4m12_t *y4m12);
