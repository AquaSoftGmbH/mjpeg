/* toplevel library to support both yuv4mpeg1 (mjpegtools-1.4)
   as well as yuv4mpeg2 (mjpegtools-1.5) */


#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include "y4m12.h"

/* allocate everything */

y4m12_t *
y4m12_malloc (void)
{
  int n;
  y4m12_t *y4m12 = (y4m12_t *) malloc (sizeof(y4m12_t));

  for (n=0;n<3;n++)
    y4m12->buffer[n] = NULL;
  y4m12->version = 0;
  y4m12->width = 0;
  y4m12->height = 0;

#ifdef HAVE_MJPEGTOOLS_15
  y4m12->v_150_streaminfo = (y4m_stream_info_t *) malloc(sizeof(y4m_stream_info_t));
  y4m_init_stream_info(y4m12->v_150_streaminfo);
  y4m12->v_150_frameinfo = (y4m_frame_info_t *) malloc(sizeof(y4m_frame_info_t));
  y4m_init_frame_info(y4m12->v_150_frameinfo);
#endif

  return y4m12;
}


/* read header */

int
y4m12_read_header (y4m12_t *y4m12, int fd)
{
  int fake_fd[2], n;
  char fake_buff[256];

  /* fake_fd[0] is for reading, fake_fd[1] for writing */
  if (pipe(fake_fd))
  {
    fprintf(stderr, "Pipe() failed\n");
    return -1;
  }
  fcntl(fake_fd[1], F_SETFL, O_NONBLOCK);

  /* we've now made a fake pipe. read one line, see whether
     it's Y4M ('YUV4MPEG ') or Y4M2 ('YUV4MPEG2 ') and continue */
  for (n=0;n<sizeof(fake_buff);n++)
  {
    if (read(fd, fake_buff + n, 1) != 1)
    {
      fprintf(stderr, "Failed to read() header\n");
      return -1;
    }
    if (fake_buff[n] == '\n')
    {
      fake_buff[n+1] = '\0';

      /* Y4M or Y4M2? */
      if (!strncmp(fake_buff, "YUV4MPEG ", 9))
      {
        /* Y4M1 */
        if (write(fake_fd[1], fake_buff, strlen(fake_buff)) != strlen(fake_buff))
        {
          fprintf(stderr, "Failed to write 1.4 pipe info to fake pipe\n");
          return -1;
        }
        if (yuv_read_header(fake_fd[0],
            &y4m12->v_140_width, &y4m12->v_140_height, &y4m12->v_140_frameratecode) < 0)
        {
          fprintf(stderr, "Failed to read 1.4 pipe info from fake pipe\n");
          return -1;
        }
        y4m12->version = 140;
        y4m12->width = y4m12->v_140_width;
        y4m12->height = y4m12->v_140_height;
        return 0;
      }
#ifdef HAVE_MJPEGTOOLS_15
      else if (!strncmp(fake_buff, "YUV4MPEG2 ", 10))
      {
        /* Y4M2 */
        if (write(fake_fd[1], fake_buff, strlen(fake_buff)) != strlen(fake_buff))
        {
          fprintf(stderr, "Failed to write 1.5 pipe info to fake pipe\n");
          return -1;
        }
        if (y4m_read_stream_header(fake_fd[0], y4m12->v_150_streaminfo) != Y4M_OK)
        {
          fprintf(stderr, "Failed to read 1.f pipe info from fake pipe\n");
          return -1;
        }
        y4m12->version = 150;
        y4m12->width = y4m_si_get_width(y4m12->v_150_streaminfo);
        y4m12->height = y4m_si_get_height(y4m12->v_150_streaminfo);
        return 0;
      }
#endif
      else
      {
        fprintf(stderr, "Unknown header\n");
        return -1;
      }
    }
  }

  /* problem - byebye */
  fprintf(stderr, "No header found\n");
  return -1;
}


/* read a frame */

int
y4m12_read_frame (y4m12_t *y4m12, int fd)
{
  if (!y4m12->buffer[0] ||
      !y4m12->buffer[1] ||
      !y4m12->buffer[2])
  {
    fprintf(stderr, "No buffer found\n");
    return 1;
  }

  if (y4m12->version == 140)
  {
    y4m12->v_140_width = y4m12->width;
    y4m12->v_140_height = y4m12->height;
    if (yuv_read_frame(fd, y4m12->buffer, y4m12->v_140_width, y4m12->v_140_height) < 1)
    {
      fprintf(stderr, "Failed to read 1.4 frame\n");
      return -1;
    }
    return 0;
  }
#ifdef HAVE_MJPEGTOOLS_15
  else if (y4m12->version == 150)
  {
    y4m_si_set_height(y4m12->v_150_streaminfo, y4m12->height);
    y4m_si_set_width(y4m12->v_150_streaminfo, y4m12->width);
    if (y4m_read_frame(fd, y4m12->v_150_streaminfo, y4m12->v_150_frameinfo, y4m12->buffer) != Y4M_OK)
    {
      fprintf(stderr, "Failed to read 1.5 frame\n");
      return -1;
    }
    return 0;
  }
#endif
  else
  {
    fprintf(stderr, "Unknown version\n");
    return -1;
  }
}


/* write a header */

int
y4m12_write_header(y4m12_t *y4m12, int fd)
{
  if (y4m12->version == 140)
  {
    y4m12->v_140_width = y4m12->width;
    y4m12->v_140_height = y4m12->height;
    yuv_write_header(fd, y4m12->v_140_width, y4m12->v_140_height, y4m12->v_140_frameratecode);
    return 0;
  }
#ifdef HAVE_MJPEGTOOLS_15
  else if (y4m12->version == 150)
  {
    y4m_si_set_height(y4m12->v_150_streaminfo, y4m12->height);
    y4m_si_set_width(y4m12->v_150_streaminfo, y4m12->width);
    if (y4m_write_stream_header(fd, y4m12->v_150_streaminfo) != Y4M_OK)
    {
      fprintf(stderr, "Failed to write 1.5 header\n");
      return -1;
    }
    return 0;
  }
#endif
  else
  {
    fprintf(stderr, "Unknown version\n");
    return -1;
  }
}


/* write a frame */

int
y4m12_write_frame (y4m12_t *y4m12, int fd)
{
  if (!y4m12->buffer[0] ||
      !y4m12->buffer[1] ||
      !y4m12->buffer[2])
  {
    fprintf(stderr, "No buffer found\n");
    return 1;
  }

  if (y4m12->version == 140)
  {
    y4m12->v_140_width = y4m12->width;
    y4m12->v_140_height = y4m12->height;
    yuv_write_frame(fd, y4m12->buffer, y4m12->v_140_width, y4m12->v_140_height);
    return 0;
  }
#ifdef HAVE_MJPEGTOOLS_15
  else if (y4m12->version == 150)
  {
    y4m_si_set_height(y4m12->v_150_streaminfo, y4m12->height);
    y4m_si_set_width(y4m12->v_150_streaminfo, y4m12->width);
    if (y4m_write_frame(fd, y4m12->v_150_streaminfo, y4m12->v_150_frameinfo, y4m12->buffer) != Y4M_OK)
    {
      fprintf(stderr, "Failed to write 1.5 frame\n");
      return -1;
    }
    return 0;
  }
#endif
  else
  {
    fprintf(stderr, "Unknown version\n");
    return -1;
  }
}


/* free everything */

void
y4m12_free (y4m12_t *y4m12)
{
#ifdef HAVE_MJPEGTOOLS_15
  y4m_fini_stream_info(y4m12->v_150_streaminfo);
  free(y4m12->v_150_streaminfo);
  y4m_fini_frame_info(y4m12->v_150_frameinfo);
  free(y4m12->v_150_frameinfo);
#endif
  free(y4m12);
}
