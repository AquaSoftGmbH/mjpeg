/*
 *  yuv4mpeg.c:  Functions for reading and writing "new" YUV4MPEG streams
 *
 *  Copyright (C) 2001 Matthew J. Marjanovic <maddog@mir.com>
 *
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#include <config.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "yuv4mpeg.h"
#include "yuv4mpeg_intern.h"
#include "mjpeg_logging.h"


/*************************************************************************
 *
 * Convenience functions for fd read/write
 *
 *   - guaranteed to transfer entire payload (or fail)
 *   - return 0 on success, or # of remaining bytes on failure
 *     
 *************************************************************************/


size_t y4m_read(int fd, char *buf, size_t len)
{
   size_t n;

   while (len > 0) {
     n = read(fd, buf, len);
     if (n <= 0) return len;  /* return amount left to read */
     buf += n;
     len -= n;
   }
   return 0;
}


size_t y4m_write(int fd, char *buf, size_t len)
{
   size_t n;

   while (len > 0) {
     n = write(fd, buf, len);
     if (n <= 0) return len;  /* return amount left to write */
     buf += n;
     len -= n;
   }
   return 0;
}



/*************************************************************************
 *
 * Creators/destructors for y4m_*_info_t structures
 *
 *************************************************************************/


y4m_stream_info_t *y4m_init_stream_info(y4m_stream_info_t *i)
{
  /* create structure, if necessary */
  if (i == NULL) {
    if ((i = malloc(sizeof(*i))) == NULL) return NULL;
  }
  /* initialize info */
  i->width = Y4M_UNKNOWN;
  i->height = Y4M_UNKNOWN;
  i->interlace = Y4M_UNKNOWN;
  i->framerate = 0.0;
  i->aspectratio = 0.0;
  return i;
}


void y4m_free_stream_info(y4m_stream_info_t *i)
{
  free(i);
}



y4m_frame_info_t *y4m_init_frame_info(y4m_frame_info_t *i)
{
  /* create structure, if necessary */
  if (i == NULL) {
    if ((i = malloc(sizeof(*i))) == NULL) return NULL;
  }
  /* initialize info */
  /*i->length = Y4M_UNKNOWN;*/
  return i;
}


void y4m_free_frame_info(y4m_frame_info_t *i)
{
  free(i);
}

    

/*************************************************************************
 *
 * Tag parsing 
 *
 *************************************************************************/


int y4m_parse_stream_tags(char *s, y4m_stream_info_t *i)
{
  char *token, *value;
  char tag;

  /* parse fields */
  for (token = strtok(s, Y4M_DELIM); 
       token != NULL; 
       token = strtok(NULL, Y4M_DELIM)) {
    if (token[0] == '\0') continue;   /* skip empty strings */
    tag = token[0];
    value = token + 1;
    switch (tag) {
    case 'W':  /* width */
      i->width = atoi(value);
      if (i->width <= 0) return Y4M_ERR_RANGE;
      break;
    case 'H':  /* height */
      i->height = atoi(value); 
      if (i->height <= 0) return Y4M_ERR_RANGE;
      break;
    case 'F':  /* frame rate (fps) */
      i->framerate = ((double) atoi(value)) / Y4M_FPS_MULT;
      /* 0.0 == unknown */
      if (i->framerate < 0.0) return Y4M_ERR_RANGE;
      break;
    case 'I':  /* interlacing */
      switch (value[0]) {
      case 'p':  i->interlace = Y4M_ILACE_NONE; break;
      case 't':  i->interlace = Y4M_ILACE_TOP_FIRST; break;
      case 'b':  i->interlace = Y4M_ILACE_BOTTOM_FIRST; break;
      case '?':
      default:
	i->interlace = Y4M_UNKNOWN; break;
      }
      break;
    case 'A':  /* display aspect ratio */
      i->aspectratio = ((double) atoi(value)) / Y4M_ASPECT_MULT;
      /* 0.0 == unknown */
      if (i->aspectratio < 0.0) return Y4M_ERR_RANGE;
      break;
    default:
      /* error on unknown options */
      return Y4M_ERR_BADTAG;
      break;
    }
  }

  /* Error checking... width and height must be known since we can't
   * parse without them
   */
  if( i->width == Y4M_UNKNOWN || i->height == Y4M_UNKNOWN )
	  return Y4M_ERR_BADTAG;
  /* ta da!  done. */
  return Y4M_OK;
}



static int y4m_parse_frame_tags(char *s, y4m_frame_info_t *i)
{
  char *token, *value;
  char tag;

  /* parse fields */
  for (token = strtok(s, Y4M_DELIM); 
       token != NULL; 
       token = strtok(NULL, Y4M_DELIM)) {
    if (token[0] == '\0') continue;   /* skip empty strings */
    tag = token[0];
    value = token + 1;
    switch (tag) {
      /* err, there aren't any legal tags yet... */
    default:
      /* error on unknown options */
      return Y4M_ERR_BADTAG;
      break;
    }
  }
  /* ta da!  done. */
  return Y4M_OK;
}





/*************************************************************************
 *
 * Read/Write stream header
 *
 *************************************************************************/


int y4m_read_stream_header(int fd, y4m_stream_info_t *i)
{
   char line[Y4M_LINE_MAX];
   char *p;
   int n;
   int err;

   /* read the header line */
   for (n = 0, p = line; n < Y4M_LINE_MAX; n++, p++) {
     if (read(fd, p, 1) < 1) 
       return Y4M_ERR_SYSTEM;
     if (*p == '\n') {
       *p = '\0';           /* Replace linefeed by end of string */
       break;
     }
   }
   if (n >= Y4M_LINE_MAX)
      return Y4M_ERR_HEADER;
   /* look for keyword in header */
   if (strncmp(line, Y4M_MAGIC, strlen(Y4M_MAGIC)))
    return Y4M_ERR_MAGIC;
   if ((err = y4m_parse_stream_tags(line + strlen(Y4M_MAGIC), i)) != Y4M_OK)
     return err;

   i->framelength = (i->height * i->width) * 3 / 2;
   return Y4M_OK;
}



int y4m_write_stream_header(int fd, y4m_stream_info_t *i)
{
  char s[Y4M_LINE_MAX+1];
  int n;

  n = snprintf(s, sizeof(s), "%s W%d H%d F%d I%s A%d\n",
	       Y4M_MAGIC,
	       i->width,
	       i->height,
	       (int)(i->framerate * Y4M_FPS_MULT),
	       (i->interlace == Y4M_ILACE_NONE) ? "p" :
	       (i->interlace == Y4M_ILACE_TOP_FIRST) ? "t" :
	       (i->interlace == Y4M_ILACE_BOTTOM_FIRST) ? "b" : "?",
	       (int)(i->aspectratio * Y4M_ASPECT_MULT));
  if ((n < 0) || (n > Y4M_LINE_MAX)) return Y4M_ERR_HEADER;
  /* non-zero on error */
  return (y4m_write(fd, s, strlen(s)) ? Y4M_ERR_SYSTEM : Y4M_OK);
}





/*************************************************************************
 *
 * Read/Write frame header
 *
 *************************************************************************/

int y4m_read_frame_header(int fd, y4m_frame_info_t *i)
{
   char line[Y4M_LINE_MAX];
   char *p;
   int n;

   /* This is more clever than read_stream_header...
       Try to read "FRAME\n" all at once, and don't try to parse
       if nothing else is there...
   */
  if (y4m_read(fd, line, sizeof(Y4M_FRAME_MAGIC)-1+1))
    return Y4M_ERR_SYSTEM;
  if (strncmp(line, Y4M_FRAME_MAGIC, sizeof(Y4M_FRAME_MAGIC)-1))
    return Y4M_ERR_MAGIC;
  if (line[sizeof(Y4M_FRAME_MAGIC)-1] == '\n')
    return Y4M_OK; /* done -- no tags:  that was the end-of-line. */

  if (line[sizeof(Y4M_FRAME_MAGIC)-1] != Y4M_DELIM[0]) {
    return Y4M_ERR_MAGIC; /* wasn't a space -- what was it? */
  }

  /* proceed to get the tags... (overwrite the magic) */
  for (n = 0, p = line; n < Y4M_LINE_MAX; n++, p++) {
    if (read(fd, p, 1) < 1)
      return Y4M_ERR_SYSTEM;
    if (*p == '\n') {
      *p = '\0';           /* Replace linefeed by end of string */
      break;
    }
  }
  if (n >= Y4M_LINE_MAX) return Y4M_ERR_HEADER;
  /* non-zero on error */
  return y4m_parse_frame_tags(line, i);
}


int y4m_write_frame_header(int fd, y4m_frame_info_t *i)
{
  char s[Y4M_LINE_MAX+1];
  int n;
  
  n = snprintf(s, sizeof(s), "%s\n", Y4M_FRAME_MAGIC);
  if ((n < 0) || (n > Y4M_LINE_MAX)) return Y4M_ERR_HEADER;
  /* non-zero on error */
  return (y4m_write(fd, s, strlen(s)) ? Y4M_ERR_SYSTEM : Y4M_OK);
}



/*************************************************************************
 *
 * Read/Write entire frame
 *
 *************************************************************************/

int y4m_read_frame(int fd, y4m_stream_info_t *si, 
		   y4m_frame_info_t *fi, unsigned char *yuv[3])
{
  int v, h, i;
  unsigned char *p;
  int err;
  
  /* read frame header */
  if ((err = y4m_read_frame_header(fd, fi)) != Y4M_OK) return err;

  h = si->width;
  v = si->height;
  /* Read luminance scanlines */
  for (i = 0, p = yuv[0]; i < v; i++, p += h)
    if (y4m_read(fd, p, h)) return Y4M_ERR_SYSTEM;
  
  v /= 2;
  h /= 2;
  /* Read chrominance scanlines */
  for (i = 0, p = yuv[1]; i < v; i++, p += h)
    if (y4m_read(fd, p, h)) return Y4M_ERR_SYSTEM;
  for (i = 0, p = yuv[2]; i < v; i++, p += h)
    if (y4m_read(fd, p, h)) return Y4M_ERR_SYSTEM;
  return Y4M_OK;
}




int y4m_write_frame(int fd, y4m_stream_info_t *si, 
		    y4m_frame_info_t *fi, unsigned char *yuv[3])
{
  int err;
  int width = si->width;
  int height = si->height;

  /* return non-zero on error... */
  if ((err = y4m_write_frame_header(fd, fi)) != Y4M_OK)
    return err;
  if (y4m_write(fd, yuv[0], width*height) ||
      y4m_write(fd, yuv[1], width*height/4) ||
      y4m_write(fd, yuv[2], width*height/4))
    return Y4M_ERR_SYSTEM;
  return Y4M_OK;
}



#ifdef DEBUG

/*************************************************************************
 *
 * Handy print-out of stream info
 *
 *************************************************************************/

void y4m_print_stream_info(FILE *fp, y4m_stream_info_t *i)
{
  fprintf(fp, "  frame size:  ");
  if (i->width != Y4M_UNKNOWN)
    fprintf(fp, "%d", i->width);
  else
    fprintf(fp, "(?)");
  fprintf(fp, "x");
  if (i->height != Y4M_UNKNOWN)
    fprintf(fp, "%d", i->height);
  else
    fprintf(fp, "(?)");
  fprintf(fp, " pixels (");
  if (i->framelength != Y4M_UNKNOWN)
    fprintf(fp, "%d bytes)\n", i->framelength);
  else
    fprintf(fp, "? bytes)\n");
  if (i->framerate > 0.0)
    fprintf(fp, "  frame rate:  %6.3f fps\n", i->framerate);
  else
    fprintf(fp, "  frame rate:  ??? fps\n");
  fprintf(fp, "   interlace:  %s\n",
	  (i->interlace == Y4M_ILACE_NONE) ? "none/progressive" :
	  (i->interlace == Y4M_ILACE_TOP_FIRST) ? "top-field-first" :
	  (i->interlace == Y4M_ILACE_BOTTOM_FIRST) ? "bottom-field-first" :
	  "anyone's guess");
  if (i->aspectratio > 0.0)
    fprintf(fp, "aspect ratio:  1:%f\n", i->aspectratio);
  else
    fprintf(fp, "aspect ratio:  ???\n");
}

#endif 

/*************************************************************************
 *
 * Convert error code to string
 *
 *************************************************************************/

const char *y4m_errstr(int err)
{
  switch (err) {
  case Y4M_OK:          return "no error";
  case Y4M_ERR_RANGE:   return "parameter out of range";
  case Y4M_ERR_SYSTEM:  return "system error (failed read/write)";
  case Y4M_ERR_HEADER:  return "bad stream or frame header";
  case Y4M_ERR_BADTAG:  return "unknown header tag";
  case Y4M_ERR_MAGIC:   return "bad header magic";
  default: 
    return "unknown error code";
  }
}
