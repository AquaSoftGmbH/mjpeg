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

#include "lav_common.h"
#include "mpegconsts.h"

static uint8_t jpeg_data[MAX_JPEG_LEN];

uint8_t *filter_buf;
int chroma_format = CHROMA420;

#ifdef SUPPORT_READ_DV2

dv_decoder_t *decoder;
gint pitches[3];
uint8_t *dv_frame[3] = {NULL,NULL,NULL};
uint8_t *previous_Y;

void frame_YUV422_to_YUV420P(
	uint8_t **output
        , uint8_t *input
        , int width
        , int height
        , LavParam *param
        )
{
    int i,j,w2,w4;
    char *y, *u, *v, *inp, *ym;

    w2 = width/2;
    y = output[0];
    u = output[1];
    v = output[2];

    w4 = 4*width;
    if (param->YUV_swap_lines) inp = input+2*width;
    else inp = input;

    for (i=0; i<height; i+=2) {
        ym = y;
        for (j=0; j<w2; j++) {
            /* packed YUV 422 is: Y[i] U[i] Y[i+1] V[i] */
            *(y++) = *(inp++);
            *(u++) = *(inp++);
            *(y++) = *(inp++);
            *(v++) = *(inp++);
        }
        if (param->YUV_swap_lines) inp -= w4;
        switch (param->DV_deinterlace) {
        case 0:
        case 1:
            for (j=0; j<w2; j++) {
                /* skip every second line for U and V */
                *(y++) = *(inp++);
                inp++;
                *(y++) = *(inp++);
                inp++;
            }
            break;
        case 2:
            /* average over the adjacent Y lines */
            inp += 2*width;
            y += width;
            break;
        default:
            /* just duplicate the last Y line */
            memcpy(y,ym,width);
            inp += 2*width;
            y += width;
            break;
        }
        if (param->YUV_swap_lines) inp += w4;
    }
}
                    
void frame_YUV420P_deinterlace(uint8_t **frame, 
        uint8_t *previous_Y, int width, int height,
        int SpatialTolerance, int TemporalTolerance, int mode)
{
    int i,j,weave;
    int cnt = 0;
    uint8_t *M0, *T0, *B0, *M1, *T1, *B1;

    switch (mode) {
    case 1:
        for (i=1; i<height-1; i+=2) {
            M1 = frame[0] + i*width;
            T1 = M1 - width;
            B1 = M1 + width;
            M0 = previous_Y + i*width;
            T0 = M0 - width;
            B0 = M0 + width;
            for (j=0; j<width; j++, M0++, T0++, B0++, M1++, T1++, B1++) {
                register int f;
                f = *T1 - *M1;
                f *= f;
                weave = (f < SpatialTolerance);
                if (!weave) {
                    f = *B1 - *M1;
                    f *= f;
                    weave = (f < SpatialTolerance);
                    if (!weave) {
                        f = *M0 - *M1;
                        f *= f;
                        weave = (f < TemporalTolerance);
                        f = *T0 - *T1;
                        f *= f;
                        weave = (f < TemporalTolerance) && weave;
                        f = *B0 - *B1;
                        f *= f;
                        weave = (f < TemporalTolerance) && weave;
                        if (!weave) {
                            f = *T1 + *B1;
                            *M1 = f>>1;
                            cnt++;
                        }
                    }
                }
            }
        }
		mjpeg_info("De-Interlace mode 1, bob count = %d\n",cnt);
        break;
    case 2:
        for (i=1; i<height-1; i+=2) {
            M1 = frame[0] + i*width;
            T1 = M1 - width;
            B1 = M1 + width;
            for (j=0; j<width; j++, M1++, T1++, B1++) {
                register int f = *T1 + *B1;
                *M1 = f>>1;
            }
        }
        break;
    }
    /* just copy the last line */
    M1 = frame[0] + (height - 1)*width;
    T1 = M1 - width;
    memcpy(M1,T1,width);
}
#endif

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
   adjust = BUFFER_ALIGN - ((int) buf) % BUFFER_ALIGN;
   if (adjust == BUFFER_ALIGN)
      adjust = 0;
   return (uint8_t *) (buf + adjust);
}

void init(LavBounds *bounds, LavParam *param, LavBuffers *buffer)
{
   int size, i;

   int chrom_top;
   int chrom_bottom;

   bounds->luma_offset = bounds->active_y * param->output_width;
   bounds->luma_size = param->output_width * bounds->active_height;
   bounds->luma_top_size = bounds->active_y * param->output_width;
   bounds->luma_bottom_size =
       (param->output_height - (bounds->active_y + bounds->active_height)) * param->output_width;

   bounds->luma_left_size = bounds->active_x;
   bounds->luma_right_offset = bounds->active_x + bounds->active_width;
   bounds->luma_right_size = param->output_width - bounds->luma_right_offset;

   bounds->chroma_width =
       (chroma_format != CHROMA420) ? bounds->active_width : bounds->active_width / 2;
   bounds->chroma_height =
       (chroma_format != CHROMA420) ? bounds->active_height : bounds->active_height / 2;

   chrom_top = (chroma_format != CHROMA420) ? bounds->active_y : bounds->active_y / 2;
   chrom_bottom =
       (chroma_format !=
        CHROMA420) ? param->output_height - (bounds->chroma_height +
                                      chrom_top) : param->output_height / 2 -
       (bounds->chroma_height + chrom_top);

   bounds->chroma_size = (chroma_format != CHROMA420) ? bounds->luma_size : bounds->luma_size / 4;
   bounds->chroma_offset =
       (chroma_format != CHROMA420) ? bounds->luma_offset : bounds->luma_offset / 4;
   bounds->chroma_top_size =
       (chroma_format != CHROMA420) ? bounds->luma_top_size : bounds->luma_top_size / 4;
   bounds->chroma_bottom_size =
       (chroma_format !=
        CHROMA420) ? bounds->luma_bottom_size : bounds->luma_bottom_size / 4;

   bounds->chroma_output_width =
       (chroma_format != CHROMA420) ? param->output_width : param->output_width / 2;
   bounds->chroma_output_height =
       (chroma_format != CHROMA420) ? param->output_height : param->output_height / 2;
   bounds->chroma_left_size =
       (chroma_format != CHROMA420) ? bounds->active_x : bounds->active_x / 2;
   bounds->chroma_right_offset = bounds->chroma_left_size + bounds->chroma_width;
   bounds->chroma_right_size = bounds->chroma_output_width - bounds->chroma_right_offset;

   memset(buffer->luma_blank, 0, sizeof(buffer->luma_blank));
   memset(buffer->chroma_blank, 0x80, sizeof(buffer->chroma_blank));

   for (i = 0; i < 3; i++) {
      if (i == 0)
         size = (param->output_width * param->output_height);
      else
         size = bounds->chroma_output_width * bounds->chroma_output_height;

      buffer->read_buf[i] = bufalloc(size);
      buffer->double_buf[i] = NULL;
   }

#ifdef SUPPORT_READ_DV2
   previous_Y  = bufalloc(param->output_width * param->output_height);
   dv_frame[0] = bufalloc(3 * param->output_width * param->output_height);
   dv_frame[1] = NULL;
   dv_frame[2] = NULL;
#endif

   filter_buf = bufalloc(param->output_width * param->output_height);
}


int readframe(	int numframe, 
				uint8_t *frame[],
				LavBounds *bounds,
				LavParam *param,
				LavBuffers *buffer,
				EditList el
	)
{
	int len, i, res, data_format;
	uint8_t *frame_tmp;

	frame[0] = buffer->read_buf[0];
	frame[1] = buffer->read_buf[1];
	frame[2] = buffer->read_buf[2];

	if (MAX_JPEG_LEN < el.max_frame_size) {
		mjpeg_error_exit1( "Max size of JPEG frame = %ld: too big\n",
						   el.max_frame_size);
	}

	len = el_get_video_frame(jpeg_data, numframe, &el);

	data_format = el_video_frame_data_format(numframe, &el);

	switch(data_format) {
	case DATAFORMAT_DV2 :
#ifndef SUPPORT_READ_DV2
		mjpeg_error("DV input was not configured at compile time\n");
		res = 1;
#else
		mjpeg_info("DV frame %d   len %d\n",numframe,len);
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
				pitches[1] != bounds->chroma_output_width) {
				mjpeg_error("for DV 4:2:0 only full width output is supported\n");
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
#endif
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
			if (pitches[0] != 2*param->output_width) {
				mjpeg_error("for DV only full width output is supported\n");
				res = 1;
			} else {
				dv_decode_full_frame(decoder, jpeg_data, e_dv_color_yuv,
									 (guchar **) dv_frame, pitches);
				frame_YUV422_to_YUV420P(frame, dv_frame[0], decoder->width,
                                        decoder->height, param);

			}
			break;
		default:
			res = 1;
			break;
		}
		if (res == 0 && (param->DV_deinterlace == 1 || param->DV_deinterlace == 2)) 
		{
			frame_YUV420P_deinterlace(frame, previous_Y, decoder->width,
									  decoder->height, param->spatial_tolerance,
									  param->default_temporal_tolerance, param->DV_deinterlace);
			if (param->default_temporal_tolerance < 0)
				param->default_temporal_tolerance = param->temporal_tolerance;
			memcpy(previous_Y, frame[0], decoder->width * decoder->height);
		}
#endif
		break;
	case DATAFORMAT_YUV420 :
		mjpeg_info("YUV420 frame %d   len %d\n",numframe,len);
		frame_tmp = jpeg_data;
		memcpy(frame[0], frame_tmp, param->output_width * param->output_height);
		frame_tmp += param->output_width * param->output_height;
		memcpy(frame[1], frame_tmp, param->output_width * param->output_height/4);
		frame_tmp += param->output_width * param->output_height/4;
		memcpy(frame[2], frame_tmp, param->output_width * param->output_height/4);
		res = 0;
		break;
	default:
		mjpeg_debug("MJPEG frame %d   len %d\n",numframe,len);

		res = decode_jpeg_raw(jpeg_data, len, el.video_inter,
							  chroma_format, param->output_width, param->output_height,
							  frame[0], frame[1], frame[2]);

	}


	if (res) {
		mjpeg_warn( "Decoding of Frame %d failed\n", numframe);
		/* TODO: Selective exit here... */
		return 1;
	}



	if (param->noise_filt) {
		uint8_t *bp;
		uint8_t *p = frame[0] + param->output_width + 1;
		uint8_t *end = frame[0] + param->output_width * (param->output_height - 1);

		bp = filter_buf + param->output_width + 1;
		if (param->noise_filt == 1) {
			for (p = frame[0] + param->output_width + 1; p < end; ++p) {
				register int f =
					(p[-param->output_width - 1] + p[-param->output_width] +
					 p[-param->output_width + 1] + p[-1] + p[1] + p[param->output_width -
																	1] +
					 p[param->output_width] + p[param->output_width + 1]);
				f = f + ((*p) << 3) + ((*p) << 4);	/* 8 + (8 + 16) = 32 */
				*bp = (f + 16) >> (4 + 1);
				++bp;
			}
		} else if (param->noise_filt == 2) {
			for (p = frame[0] + param->output_width + 1; p < end; ++p) {
				register int f =
					(p[-param->output_width - 1] + p[-param->output_width] +
					 p[-param->output_width + 1] + p[-1] + p[1] + p[param->output_width -
																	1] +
					 p[param->output_width] + p[param->output_width + 1]);

				f = f + ((*p) << 3);
				*bp = (f + 8) >> (3 + 1);
				++bp;
			}
		} else {
			for (p = frame[0] + param->output_width + 1; p < end; ++p) {
				register int f =
					(p[-param->output_width - 1] + p[-param->output_width] +
					 p[-param->output_width + 1] + p[-1] + p[1] + p[param->output_width -
																	1] +
					 p[param->output_width] + p[param->output_width + 1]);
				/* f = f + (f<<1) + (*p << 3);
				 *bp = (f + (1 << 4)) >> (3+2); */
				f = f + (*p << 3);
				*bp = (f + (1 << 2)) >> (3 + 1);
				++bp;
			}
		}

		bp = filter_buf + param->output_width + 1;
		for (p = frame[0] + param->output_width + 1; p < end; ++p) {
			*p = *bp;
			++bp;
		}


	}

	if (param->mono) {
		for (i = 0; i < bounds->chroma_output_width * bounds->chroma_output_height; ++i) {
			frame[1][i] = frame[2][i] = 0x80;
		}
	}
	return 0;

}


/*
   Take a wild guess at the SAR (sample aspect ratio) by checking the
    frame width and height for common values.  
   There is, of course, no way to tell a 16:9 anamorphic frame from a
    good old 4:3 frame.  This function assumes a 'roughly' 4:3 intended
    display aspect ratio.
*/

y4m_ratio_t guess_sar(int width, int height)
{
  if ((height > 470) && (height < 490)) {
    if ((width > 700) && (width < 724))       /* 704x480 or 720x480 */
      return y4m_sar_NTSC_CCIR601;
    else if ((width > 630) && (width < 650))  /* 640x480 */
      return y4m_sar_SQUARE;
    else if ((width > 470) && (width < 490))  /* 480x480 */
      return y4m_sar_NTSC_SVCD_4_3;
  } else if ((height > 230) && (height < 250)) {
    if ((width > 350) && (width < 362))       /* 352x240 or 360x240 */
      return y4m_sar_NTSC_CCIR601;
    else if ((width > 310) && (width < 330))
      return y4m_sar_SQUARE;                  /* 320x240 */
  } else if ((height > 565) && (height < 585)) {
    if ((width > 700) && (width < 724))       /* 704x576 or 720x576 */
      return y4m_sar_PAL_CCIR601;
    else if ((width > 760) && (width < 780))  /* 768x576 */
      return y4m_sar_SQUARE;
    else if ((width > 470) && (width < 490))  /* 480x576 */
      return y4m_sar_PAL_SVCD_4_3;
  } else if ((height > 280) && (height < 300)) {
    if ((width > 350) && (width < 362))       /* 352x288 */
      return y4m_sar_PAL_CCIR601;
    else if ((width > 380) && (width < 390))  /* 386x288 */
      return y4m_sar_SQUARE;
  }
  return y4m_sar_UNKNOWN;
}

void writeoutYUV4MPEGheader(
	int out_fd
	, LavParam *param
	, EditList el
	)
{
   //int frame_rate_code = yuv_fps2mpegcode (el.video_fps);
   int n;
   y4m_stream_info_t stream_info;


   /* this is old YUV4MPEG stream stuff - obsolete
    * yuv_write_header (out_fd, param->output_width, param->output_height, frame_rate_code);
    */

   /* new YUV4MPEG streaming */
   y4m_init_stream_info(&stream_info);
   y4m_si_set_width(&stream_info, param->output_width);
   y4m_si_set_height(&stream_info, param->output_height);
   y4m_si_set_interlace(&stream_info, param->interlace );
   y4m_si_set_framerate(&stream_info, mpeg_conform_framerate(el.video_fps));
   if (!Y4M_RATIO_EQL(param->sar, y4m_sar_UNKNOWN)) {
     y4m_si_set_sampleaspect(&stream_info, param->sar);
   } else if ((el.video_sar_width != 0) || (el.video_sar_height != 0)) {
     y4m_ratio_t sar;
     sar.n = el.video_sar_width;
     sar.d = el.video_sar_height;
     y4m_si_set_sampleaspect(&stream_info, sar);
   } else {
     /* no idea! ...eh, just guess. */
     mjpeg_warn("unspecified sample-aspect-ratio --- taking a guess...\n");
     y4m_si_set_sampleaspect(&stream_info,
			     guess_sar(param->output_width,
				       param->output_height));
   }

   if( param->dar_code != 0 )
   {
	   char buf[20];
	   snprintf( buf, 19, "XM2AR%03d", param->dar_code );
	   y4m_xtag_add( y4m_si_xtags(&stream_info), buf );
   }

   n = y4m_write_stream_header(out_fd, &stream_info);
   if (n != Y4M_OK)
      mjpeg_error("Failed to write stream header: %s\n", y4m_strerr(n));
   y4m_fini_stream_info(&stream_info);
}

void writeoutframeinYUV4MPEG(int out_fd,
			     uint8_t *frame[],
			     LavBounds *bounds,
			     LavParam *param,
			     LavBuffers *buffer,
			     y4m_frame_info_t *frame_info)
{
   int i;
   char *ptr;

   /* new YUV4MPEG stream info */
   /* NB: TODO - there's no frame info yet, since we don't need it yet */
   i = y4m_write_frame_header(out_fd, frame_info); /* &frame_info */ 
   if (i != Y4M_OK)
      mjpeg_error("Failed to write frame header: %s\n", y4m_strerr(i));

   for (i = 0; i < bounds->active_height; i++) {
      ptr = &frame[0][bounds->luma_offset + (i * param->output_width)];
      if (bounds->luma_left_size) {
         memset(ptr, 0x00, bounds->luma_left_size);
      }
      if (bounds->luma_right_size) {
         memset(&ptr[bounds->luma_right_offset], 0x00, bounds->luma_right_size);
      }
   }

   for (i = 0; i < bounds->chroma_height; i++) {
      ptr = &frame[1][bounds->chroma_offset + (i * bounds->chroma_output_width)];
      if (bounds->chroma_left_size) {
         memset(ptr, 0x80, bounds->chroma_left_size);
      }

      if (bounds->chroma_right_size) {
         memset(&ptr[bounds->chroma_right_offset], 0x80, bounds->chroma_right_size);
      }

      ptr = &frame[2][bounds->chroma_offset + (i * bounds->chroma_output_width)];
      if (bounds->chroma_left_size) {
         memset(ptr, 0x80, bounds->chroma_left_size);
      }

      if (bounds->chroma_right_size) {
         memset(&ptr[bounds->chroma_right_offset], 0x80, bounds->chroma_right_size);
      }

   }

   if (bounds->luma_top_size) {
      y4m_write(out_fd, buffer->luma_blank, bounds->luma_top_size);
   }
   y4m_write(out_fd, &frame[0][bounds->luma_offset], bounds->luma_size);
   if (bounds->luma_bottom_size) {
      y4m_write(out_fd, buffer->luma_blank, bounds->luma_bottom_size);
   }


   if (bounds->chroma_top_size) {
      y4m_write(out_fd, buffer->chroma_blank, bounds->chroma_top_size);
   }
   y4m_write(out_fd, &frame[1][bounds->chroma_offset], bounds->chroma_size);
   if (bounds->chroma_bottom_size) {
      y4m_write(out_fd, buffer->chroma_blank, bounds->chroma_bottom_size);
   }

   if (bounds->chroma_top_size) {
      y4m_write(out_fd, buffer->chroma_blank, bounds->chroma_top_size);
   }
   y4m_write(out_fd, &frame[2][bounds->chroma_offset], bounds->chroma_size);
   if (bounds->chroma_bottom_size) {
      y4m_write(out_fd, buffer->chroma_blank, bounds->chroma_bottom_size);
   }
}

#ifdef SUPPORT_READ_DV2
void lav_init_dv_decoder()
{
   decoder = dv_decoder_new();
   dv_init();
   decoder->quality = DV_QUALITY_BEST;
}
#endif
