/*
 *  yuv4mpeg.h:  Functions for reading and writing "new" YUV4MPEG streams
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
 */

#ifndef __YUV4MPEG_H__
#define __YUV4MPEG_H__

#include <stdlib.h>

/* error codes returned by y4m_* functions */
#define Y4M_OK          0
#define Y4M_ERR_RANGE   1
#define Y4M_ERR_SYSTEM  2
#define Y4M_ERR_HEADER  3
#define Y4M_ERR_BADTAG  4
#define Y4M_ERR_MAGIC   5
#define Y4M_EOF     6

/* generic 'unknown' value for integer parameters */
#define Y4M_UNKNOWN -1

/* useful list of standard framerates */
#define Y4M_FPS_TV_FILM    23.976024
#define Y4M_FPS_FILM       24.000000
#define Y4M_FPS_PAL        25.000000
#define Y4M_FPS_NTSC       29.970030
#define Y4M_FPS_PAL_FIELD  50.000000
#define Y4M_FPS_NTSC_FIELD 59.940060
#define Y4M_FPS_NDF_FIELD  60.000000

#if 0
#define Y4M_ASPECT_
#define Y4M_ASPECT_
#define Y4M_ASPECT_
#define Y4M_ASPECT_
#endif

/* possible options for the interlacing parameter */
#define Y4M_ILACE_NONE          0
#define Y4M_ILACE_TOP_FIRST     1
#define Y4M_ILACE_BOTTOM_FIRST  2



typedef struct _y4m_stream_info {
  /* values from header */
  int width;
  int height;
  int interlace;   
  double framerate;   /* 0.0 == unknown */
  double aspectratio; /* 0.0 == unknown */

  /* computed/derivative values */
  int framelength;    /* bytes of data per frame (not including header) */

  /* y4m_tag_list_t *extra_tags; */
} y4m_stream_info_t;


typedef struct _y4m_frame_info {
  /* y4m_tag_list_t *extra_tags; */
} y4m_frame_info_t;


#ifdef __cplusplus
extern "C" {
#else
#endif

/* info structure initialization/creation and destruction */
y4m_stream_info_t *y4m_init_stream_info(y4m_stream_info_t *i);
void y4m_free_stream_info(y4m_stream_info_t *i);
y4m_frame_info_t *y4m_init_frame_info(y4m_frame_info_t *i);
void y4m_free_frame_info(y4m_frame_info_t *i);


/* convenient blocking read/write */
size_t y4m_read(int fd, char *buf, size_t len, int *eof);
size_t y4m_write(int fd, char *buf, size_t len);


/* stream header tag parsing */
int y4m_parse_stream_tags(char *s, y4m_stream_info_t *i);


/* stream header reading/writing */
int y4m_read_stream_header(int fd, y4m_stream_info_t *i);
int y4m_write_stream_header(int fd,  y4m_stream_info_t *i);

/* frame header reading/writing */
int y4m_read_frame_header(int fd, y4m_frame_info_t *i);
int y4m_write_frame_header(int fd, y4m_frame_info_t *i);

/* complete frame (header+data) reading/writing */
int y4m_read_frame(int fd, y4m_stream_info_t *si, 
		   y4m_frame_info_t *fi, unsigned char *yuv[3]);
int y4m_write_frame(int fd, y4m_stream_info_t *si, 
		    y4m_frame_info_t *fi, unsigned char *yuv[3]);

#ifdef DEBUG
/* convenient dump of stream header info */
void y4m_print_stream_info(FILE *fp, y4m_stream_info_t *i);
#endif

/* convert error code into mildly explanatory string */
const char *y4m_errstr(int err);

#ifdef __cplusplus
}
#endif

/*

  Description of the (new, forever) YUV4MPEG stream format:

  STREAM consists of
    o '\n' terminated STREAM-HEADER
    o unlimited number of FRAMEs

  FRAME consists of
    o '\n' terminated FRAME-HEADER
    o "length" octets of planar YCrCb 4:2:0 image data
        (if frame is interlaced, then the two fields are interleaved)


  STREAM-HEADER consists of
     o string "YUV4MPEG "
     o unlimited number of ' ' separated TAGGED-FIELDs
     o '\n' line terminator

  FRAME-HEADER consists of
     o string "FRAME "
     o unlimited number of ' ' separated TAGGED-FIELDs
     o '\n' line terminator

  TAGGED-FIELD consists of
    o single ascii character tag
    o value (which does not contain whitespace)


  The currently supported tags for the STREAM-HEADER:
     W - integer frame width, pixels, should be > 0
     H - integer frame height, pixels, should be > 0
     F - fixed-point frame-rate:  fps * YUV_FPS_MULT (== 1000000.0)
     I - interlacing:  p - progressive (none)
                       t - top-field-first
                       b - bottom-field-first
		       ? - unknown
     A - display aspect ratio, fixed-point
    [X - character string 'metadata'] -- ToBeImplemented

  The currently supported tags for the FRAME-HEADER:
     L - integer frame data length, octets
    [X - character string 'metadata'] -- ToBeImplemented

 */
#endif


#if 0

/* extra tag stuff */

struct _yuv4_tag_list_elt {
  char *field;
  struct _yuv4_tag_list_elt *next;
};

typedef struct _yuv4_tag_list {
  struct _yuv4_tag_list_elt *head;
  struct _yuv4_tag_list_elt *tail;
  struct _yuv4_tag_list_elt *current;
} yuv4_tag_list_t;

#endif
