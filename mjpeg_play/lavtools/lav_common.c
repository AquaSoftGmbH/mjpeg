/* lav_common - some general utility functionality used by multiple
	lavtool utilities. */

/* Copyright (C) 2000, Rainer Johanni, Andrew Stevens */
/* - added scene change detection code 2001, pHilipp Zabel */
/* - broke some code out to lav_common.h and lav_common.c 
 *   July 2001, Shawn Sulma <lavtools@athos.cx>.  In doing this,
 *   I replaced the large number of globals with a handful of structs
 *   that are passed into the appropriate methods.  Check lav_common.h
 *   for the structs.  I'm sure some of what I've done is inefficient,
 *   subtly incorrect or just plain Wrong.  Feedback is welcome.
 */
/* - removed a lot of subsumed functionality and unnecessary cruft
 *   March 2002, Matthew Marjanovic <maddog@mir.com>.
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
#include <string.h>

#include "lav_common.h"
#include "jpegutils.h"
#include "mpegconsts.h"

static uint8_t jpeg_data[MAX_JPEG_LEN];


#ifdef SUPPORT_READ_DV2

dv_decoder_t *decoder;
gint pitches[3];
uint8_t *dv_frame[3] = {NULL,NULL,NULL};

/*
 * As far as I (maddog) can tell, this is what is going on with libdv-0.9
 *  and the unpacking routine... 
 *    o Ft/Fb refer to top/bottom scanlines (interleaved) --- each sample
 *       is implicitly tagged by 't' or 'b' (samples are not mixed between
 *       fields)
 *    o Indices on Cb and Cr samples indicate the Y sample with which
 *       they are co-sited.
 *    o '^' indicates which samples are preserved by the unpacking
 *
 * libdv packs both NTSC 4:1:1 and PAL 4:2:0 into a common frame format of
 *  packed 4:2:2 pixels as follows:
 *
 *
 ***** NTSC 4:1:1 *****
 *
 *   libdv's 4:2:2-packed representation  (chroma repeated horizontally)
 *
 *Ft Y00.Cb00.Y01.Cr00.Y02.Cb00.Y03.Cr00 Y04.Cb04.Y05.Cr04.Y06.Cb04.Y07.Cr04
 *    ^    ^   ^    ^   ^    ^   ^    ^   ^    ^   ^    ^   ^    ^   ^    ^
 *Fb Y00.Cb00.Y01.Cr00.Y02.Cb00.Y03.Cr00 Y04.Cb04.Y05.Cr04.Y06.Cb04.Y07.Cr04
 *    ^    ^   ^    ^   ^    ^   ^    ^   ^    ^   ^    ^   ^    ^   ^    ^
 *Ft Y10.Cb10.Y11.Cr10.Y12.Cb10.Y13.Cr10 Y14.Cb14.Y15.Cr14.Y16.Cb14.Y17.Cr14
 *    ^        ^        ^        ^        ^        ^        ^        ^    
 *Fb Y10.Cb10.Y11.Cr10.Y12.Cb10.Y13.Cr10 Y14.Cb14.Y15.Cr14.Y16.Cb14.Y17.Cr14
 *    ^        ^        ^        ^        ^        ^        ^        ^    
 *
 *    lavtools unpacking into 4:2:0-planar  (note lossiness)
 *
 *Ft  Y00.Y01.Y02.Y03.Y04.Y05.Y06.Y07...
 *Fb  Y00.Y01.Y02.Y03.Y04.Y05.Y06.Y07...
 *Ft  Y10.Y11.Y12.Y13.Y14.Y15.Y16.Y17...
 *Fb  Y10.Y11.Y12.Y13.Y14.Y15.Y16.Y17...
 *
 *Ft  Cb00.Cb00.Cb04.Cb04...    Cb00,Cb04... are doubled
 *Fb  Cb00.Cb00.Cb04.Cb04...    Cb10,Cb14... are ignored
 *
 *Ft  Cr00.Cr00.Cr04.Cr04...
 *Fb  Cr00.Cr00.Cr04.Cr04...
 *
 * 
 ***** PAL 4:2:0 *****
 *
 *   libdv's 4:2:2-packed representation  (chroma repeated vertically)
 *
 *Ft Y00.Cb00.Y01.Cr10.Y02.Cb02.Y03.Cr12 Y04.Cb04.Y05.Cr14.Y06.Cb06.Y07.Cr16
 *    ^    ^   ^    ^   ^    ^   ^    ^   ^    ^   ^    ^   ^    ^   ^    ^
 *Fb Y00.Cb00.Y01.Cr10.Y02.Cb02.Y03.Cr12 Y04.Cb04.Y05.Cr14.Y06.Cb06.Y07.Cr16
 *    ^    ^   ^    ^   ^    ^   ^    ^   ^    ^   ^    ^   ^    ^   ^    ^
 *Ft Y10.Cb00.Y11.Cr10.Y12.Cb02.Y13.Cr12 Y14.Cb04.Y15.Cr14.Y16.Cb06.Y17.Cr16
 *    ^        ^        ^        ^        ^        ^        ^        ^    
 *Fb Y10.Cb00.Y11.Cr10.Y12.Cb02.Y13.Cr12 Y14.Cb04.Y15.Cr14.Y16.Cb06.Y17.Cr16
 *    ^        ^        ^        ^        ^        ^        ^        ^    
 *
 *    lavtools unpacking into 4:2:0-planar
 *
 *Ft  Y00.Y01.Y02.Y03.Y04.Y05.Y06.Y07...
 *Fb  Y00.Y01.Y02.Y03.Y04.Y05.Y06.Y07...
 *Ft  Y10.Y11.Y12.Y13.Y14.Y15.Y16.Y17...
 *Fb  Y10.Y11.Y12.Y13.Y14.Y15.Y16.Y17...
 *
 *Ft  Cb00.Cb02.Cb04.Cb06...
 *Fb  Cb00.Cb02.Cb04.Cb06...
 *
 *Ft  Cr10.Cr12.Cr14.Cr16...
 *Fb  Cr10.Cr12.Cr14.Cr16...
 *
 */

/*
 * Unpack libdv's 4:2:2-packed into our 4:2:0-planar
 *
 */
void frame_YUV422_to_YUV420P(uint8_t **output, uint8_t *input,
			     int width, int height)
{
    int i, j, w2;
    uint8_t *y, *cb, *cr;

    w2 = width/2;
    y = output[0];
    cb = output[1];
    cr = output[2];

    for (i=0; i<height; i+=2) {
	/* process two scanlines (one from each field, interleaved) */
        for (j=0; j<w2; j++) {
            /* packed YUV 422 is: Y[i] U[i] Y[i+1] V[i] */
            *(y++) =  *(input++);
            *(cb++) = *(input++);
            *(y++) =  *(input++);
            *(cr++) = *(input++);
        }
	/* process next two scanlines (one from each field, interleaved) */
	for (j=0; j<w2; j++) {
	  /* skip every second line for U and V */
	  *(y++) = *(input++);
	  input++;
	  *(y++) = *(input++);
	  input++;
	}
    }
}

#endif /* SUPPORT_READ_DV2 */



/***********************
 *
 * Take a random(ish) sampled mean of a frame luma and chroma
 * Its intended as a rough and ready hash of frame content.
 * The idea is that changes above a certain threshold are treated as
 * scene changes.
 *
 **********************/

int luminance_mean(uint8_t *frame[], int w, int h )
{
	uint8_t *p;
	uint8_t *lim;
	int sum = 0;
	int count = 0;
	p = frame[0];
	lim = frame[0] + w*(h-1);
	while( p < lim )
	{
		sum += (p[0] + p[1]) + (p[w-3] + p[w-2]);
		p += 31;
		count += 4;
	}

    w = w / 2;
	h = h / 2;

	p = frame[1];
	lim = frame[1] + w*(h-1);
	while( p < lim )
	{
		sum += (p[0] + p[1]) + (p[w-3] + p[w-2]);
		p += 31;
		count += 4;
	}
	p = frame[2];
	lim = frame[2] + w*(h-1);
	while( p < lim )
	{
		sum += (p[0] + p[1]) + (p[w-3] + p[w-2]);
		p += 31;
		count += 4;
	}
	return sum / count;
}


/*
	Wrapper for malloc that allocates pbuffers aligned to the 
	specified byte boundary and checks for failure.
	N.b.  don't try to free the resulting pointers, eh...
	BUG: 	Of course this won't work if a char * won't fit in an int....
*/

static uint8_t *bufalloc(size_t size)
{
   char *buf = malloc(size + BUFFER_ALIGN);
   int adjust;
   if (buf == NULL) {
      perror("malloc failed\n");
   }
   adjust = BUFFER_ALIGN - ((unsigned long) buf) % BUFFER_ALIGN;
   if (adjust == BUFFER_ALIGN)
      adjust = 0;
   return (uint8_t *) (buf + adjust);
}



void init(LavParam *param, uint8_t *buffer[])
{
   param->luma_size = param->output_width * param->output_height;
   param->chroma_width = param->output_width / 2;
   param->chroma_height = param->output_height / 2;
   param->chroma_size = param->chroma_height * param->chroma_width;

   buffer[0] = bufalloc(param->luma_size);
   buffer[1] = bufalloc(param->chroma_size);
   buffer[2] = bufalloc(param->chroma_size);
   
#ifdef SUPPORT_READ_DV2
   dv_frame[0] = bufalloc(3 * param->output_width * param->output_height);
   dv_frame[1] = NULL;
   dv_frame[2] = NULL;
#endif
}




int readframe(int numframe, 
	      uint8_t *frame[],
	      LavParam *param,
	      EditList el)
{
  int len, i, res, data_format;
  uint8_t *frame_tmp;

  if (MAX_JPEG_LEN < el.max_frame_size) {
    mjpeg_error_exit1( "Max size of JPEG frame = %ld: too big",
		       el.max_frame_size);
  }
  
  len = el_get_video_frame(jpeg_data, numframe, &el);
  data_format = el_video_frame_data_format(numframe, &el);
  
  switch(data_format) {

  case DATAFORMAT_DV2 :
#ifndef SUPPORT_READ_DV2
    mjpeg_error("DV input was not configured at compile time");
    res = 1;
#else
    mjpeg_info("DV frame %d   len %d",numframe,len);
    res = 0;
    dv_parse_header(decoder, jpeg_data);
    switch(decoder->sampling) {
    case e_dv_sample_420:
#ifdef LIBDV_PAL_YV12
      /* libdv decodes PAL DV directly as planar YUV 420
       * (YV12 or 4CC 0x32315659) if configured with the flag
       * --with-pal-yuv=YV12 which is not (!) the default
       */
      pitches[0] = decoder->width;
      pitches[1] = decoder->width / 2;
      pitches[2] = decoder->width / 2;
      if (pitches[0] != param->output_width ||
	  pitches[1] != param->chroma_width) {
	mjpeg_error("for DV 4:2:0 only full width output is supported");
	res = 1;
      } else {
	dv_decode_full_frame(decoder, jpeg_data, e_dv_color_yuv,
			     (guchar **) frame, pitches);
	/* swap the U and V components */
	frame_tmp = frame[2];
	frame[2] = frame[1];
	frame[1] = frame_tmp;
      }
      break;
#endif /* LIBDV_PAL_YV12 */
    case e_dv_sample_411:
    case e_dv_sample_422:
      /* libdv decodes NTSC DV (native 411) and by default also PAL
       * DV (native 420) as packed YUV 422 (YUY2 or 4CC 0x32595559)
       * where the U and V information is repeated.  This can be
       * transformed to planar 420 (YV12 or 4CC 0x32315659).
       * For NTSC DV this transformation is lossy.
       */
      pitches[0] = decoder->width * 2;
      pitches[1] = 0;
      pitches[2] = 0;
      if (decoder->width != param->output_width) {
	mjpeg_error("for DV only full width output is supported");
	res = 1;
      } else {
	dv_decode_full_frame(decoder, jpeg_data, e_dv_color_yuv,
			     (guchar **) dv_frame, pitches);
	frame_YUV422_to_YUV420P(frame, dv_frame[0],
				decoder->width,	decoder->height);
	
      }
      break;
    default:
      res = 1;
      break;
    }
#endif /* SUPPORT_READ_DV2 */
    break;

  case DATAFORMAT_YUV420 :
    mjpeg_info("YUV420 frame %d   len %d",numframe,len);
    frame_tmp = jpeg_data;
    memcpy(frame[0], frame_tmp, param->output_width * param->output_height);
    frame_tmp += param->output_width * param->output_height;
    memcpy(frame[1], frame_tmp, param->output_width * param->output_height/4);
    frame_tmp += param->output_width * param->output_height/4;
    memcpy(frame[2], frame_tmp, param->output_width * param->output_height/4);
    res = 0;
    break;

  default:
    mjpeg_debug("MJPEG frame %d   len %d",numframe,len);
    res = decode_jpeg_raw(jpeg_data, len, el.video_inter,
			  CHROMA420,
			  param->output_width, param->output_height,
			  frame[0], frame[1], frame[2]);
    
  }
  
  
  if (res) {
    mjpeg_warn( "Decoding of Frame %d failed", numframe);
    /* TODO: Selective exit here... */
    return 1;
  }
  
  
  if (param->mono) {
    for (i = 0;
	 i < param->chroma_size;
	 ++i) {
      frame[1][i] = 0x80;
      frame[2][i] = 0x80;
    }
  }
  return 0;
  
}


void writeoutYUV4MPEGheader(int out_fd,
			    LavParam *param,
			    EditList el,
			    y4m_stream_info_t *streaminfo)
{
   int n;

   y4m_si_set_width(streaminfo, param->output_width);
   y4m_si_set_height(streaminfo, param->output_height);
   y4m_si_set_interlace(streaminfo, param->interlace);
   y4m_si_set_framerate(streaminfo, mpeg_conform_framerate(el.video_fps));
   if (!Y4M_RATIO_EQL(param->sar, y4m_sar_UNKNOWN)) {
     y4m_si_set_sampleaspect(streaminfo, param->sar);
   } else if ((el.video_sar_width != 0) || (el.video_sar_height != 0)) {
     y4m_ratio_t sar;
     sar.n = el.video_sar_width;
     sar.d = el.video_sar_height;
     y4m_si_set_sampleaspect(streaminfo, sar);
   } else {
     /* no idea! ...eh, just guess. */
     mjpeg_warn("unspecified sample-aspect-ratio --- taking a guess...");
     y4m_si_set_sampleaspect(streaminfo,
			     y4m_guess_sar(param->output_width, 
					   param->output_height,
					   param->dar));
   }

   n = y4m_write_stream_header(out_fd, streaminfo);
   if (n != Y4M_OK)
      mjpeg_error("Failed to write stream header: %s", y4m_strerr(n));
}




#ifdef SUPPORT_READ_DV2
void lav_init_dv_decoder()
{
   decoder = dv_decoder_new();
   dv_init();
   decoder->quality = DV_QUALITY_BEST;
}
#endif
