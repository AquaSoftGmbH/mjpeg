/* lav_common - some general utility functionality used by multiple
	lavtool utilities. */

/* Copyright (C) 2000, Rainer Johanni, Andrew Stevens */
/* - added scene change detection code 2001, pHilipp Zabel */ 
/* - broke some code out to lav_common.h and lav_common.c
 *   July 2001, Shawn Sulma <lavtools@athos.cx>.  Part of these changes were
 *   to replace the large number of globals with a handful of structs that
 *   get passed in to the relevant functions.  Some of this may be
 *   inefficient, subtly broken, or Wrong.  Helpful feedback is certainly
 *   welcome.
 */

/*
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
 
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.                
*/

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <lav_io.h>
#include <editlist.h>
#include <math.h>
#include "yuv4mpeg.h"



#include "mjpeg_logging.h"


#define MAX_EDIT_LIST_FILES 256
#define MAX_JPEG_LEN (3*576*768/2)

const static char roundadj[4] = { 0, 0, 1, 2 };

#define BUFFER_ALIGN 16

/**
 * (SS 2001-July-13)
 * The split of the globals into three structs is somewhat arbitrary, but
 * I've tried to do them based on role as used in lav2yuv and (my own)
 * lav2divx.  
 * - LavParam handles data that is generally per-run dependent
 *   (e.g. from the command line).
 * - LavBounds contains data about bounds used in processing.  It is generally
 *   not dependent on command line alteration.
 * - LavBuffer contains buffers used to perform various tasks.
 *
 **/

typedef struct {
   int offset;
   int frames;
   int noise_filt;
   int mono;
   char *scenefile;
   int YUV_swap_lines; // = 0;
   int DV_deinterlace; // = 0;
   int spatial_tolerance; // = 440;
   int temporal_tolerance; // = 220;
   int default_temporal_tolerance; // = -1;
   int delta_lum_threshold; // = 4;
   unsigned int scene_detection_decimation; // = 2;
   int output_width;
   int output_height;
   int interlace;
   y4m_ratio_t sar; /* sample aspect ratio (default 0:0 == unspecified) */
	int dar_code;				/* MPEG-2 Display aspect ratio code */
} LavParam;
//=
//{ 0, 0, 0, 0, 0, 0, NULL, 0, 0, 440, 220, -1, 4, 2, 0, 0 } ;


typedef struct {
   int luma_size;
   int luma_offset;
   int luma_top_size;
   int luma_bottom_size;
   int luma_left_size;
   int luma_right_size;
   int luma_right_offset;
   int chroma_size;
   int chroma_offset;
   int chroma_top_size;
   int chroma_bottom_size;
   int chroma_output_width;
   int chroma_output_height;
   int chroma_height;
   int chroma_width;
   int chroma_left_size;
   int chroma_right_size;
   int chroma_right_offset;
   int active_x; //= 0;
   int active_y; // = 0;
   int active_width; // = 0;
   int active_height; // = 0;
} LavBounds;

typedef struct {
   uint8_t *read_buf[3];
   uint8_t *double_buf[3];
   uint8_t luma_blank[768 * 480];
   uint8_t chroma_blank[768 * 480];
} LavBuffers;

int luminance_mean(uint8_t *frame[], int w, int h);
int decode_jpeg_raw(uint8_t *jpeg_data, int len,
                    int itype, int ctype, int width, int height,
                    uint8_t *raw0, uint8_t *raw1,
                    uint8_t *raw2);

int readframe(int numframe, uint8_t *frame[], LavBounds *bounds, LavParam *param, LavBuffers *buffer, EditList el);
void writeoutYUV4MPEGheader(int out_fd, LavParam *param, EditList el);
void writeoutframeinYUV4MPEG(int out_fd, uint8_t *frame[], LavBounds *bounds, LavParam *param, LavBuffers *buffer, y4m_frame_info_t *frame_info);
void init(LavBounds *bounds, LavParam *param, LavBuffers *buffer);

y4m_ratio_t guess_sar(int width, int height);

#ifdef SUPPORT_READ_DV2
#include <libdv/dv.h>
void frame_YUV422_to_YUV420P(uint8_t **output, uint8_t *input, int width, int height, LavParam *param);
void frame_YUV420P_deinterlace(uint8_t **frame, uint8_t *previous_Y, int width, int height, int SpatialTolerance, int TemporalTolerance, int mode);
void lav_init_dv_decoder(void);

#endif // SUPPORT_READ_DV2

