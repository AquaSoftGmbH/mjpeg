/* 
 *  libdv.c
 *
 *     Copyright (C) Adam Williams - May 2000
 *     Copyright (C) Charles 'Buck' Krasic - April 2000
 *     Copyright (C) Erik Walthinsen - April 2000
 *
 *  This file is part of libdv, a free DV (IEC 61834/SMPTE 314M)
 *  decoder.
 *
 *  libdv is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your
 *  option) any later version.
 *   
 *  libdv is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *   
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 
 *
 *  The libdv homepage is http://libdv.sourceforge.net/.  
 */

#include "dv.h"
#include "libdv.h"

#include <stdio.h>
#include <string.h>

static int dv_initted = 0;
#ifdef HAVE_FIREWIRE
static dv_grabber_t *dv_grabber = 0;
#endif


void convert_coeffs(dv_block_t *bl)
{
	int i;
	for(i = 0; i < 64; i++) 
	{
    	bl->coeffs248[i] = bl->coeffs[i];
	}
} // convert_coeffs

void convert_coeffs_prime(dv_block_t *bl)
{
	int i;
	for(i = 0; i < 64; i++) 
	{
		bl->coeffs[i] = bl->coeffs248[i];
	}
} // convert_coeffs_prime

#ifdef HAVE_FIREWIRE
int dv_advance_frame(dv_grabber_t *grabber, int *frame_number)
{
	(*frame_number)++;
	if((*frame_number) >= grabber->frames) (*frame_number) = 0;
	return 0;
}

int dv_iso_handler(raw1394handle_t handle, 
		int channel, 
		size_t length,
		quadlet_t *data)
{
	dv_grabber_t *grabber = dv_grabber;

	if(!grabber) return 0;
	if(grabber->done) return 0;

	if(length > 16)
	{
		unsigned char *ptr = (unsigned char*)&data[3];
		int section_type = ptr[0] >> 5;
		int dif_sequence = ptr[1] >> 4;
		int dif_block = ptr[2];

		switch(section_type)
		{
			case 0: // Header
// Start over for incomplete frame or new frame
				if((dif_sequence == 0) && 
					(dif_block == 0) && 
					(grabber->bytes_read < grabber->frame_size))
				{
					grabber->bytes_read = 0;
				}

				if(ptr[3] & 0x80)
// PAL
					grabber->frame_size = 140000;
				else
// NTSC
					grabber->frame_size = 120000;

                memcpy(grabber->frame_buffer[grabber->input_frame] + 
					dif_sequence * 150 * 80, ptr, length - 16);
				break;

			case 1: // Subcode
				memcpy(grabber->frame_buffer[grabber->input_frame] + 
					dif_sequence * 150 * 80 + (1 + dif_block) * 80, 
					ptr, 
					length - 16);
				break;

			case 2: // VAUX
				memcpy(grabber->frame_buffer[grabber->input_frame] + 
					dif_sequence * 150 * 80 + (3 + dif_block) * 80, 
					ptr, 
					length - 16);
				break;

			case 3: // Audio block
				memcpy(grabber->frame_buffer[grabber->input_frame] + 
					dif_sequence * 150 * 80 + (6 + dif_block * 16) * 80, 
					ptr, 
					length - 16);
				break;

			case 4: // Video block
				memcpy(grabber->frame_buffer[grabber->input_frame] + 
					dif_sequence * 150 * 80 + (7 + (dif_block / 15) + dif_block) * 80, 
					ptr, 
					length - 16);
				break;
		}

		grabber->bytes_read += length - 16;

// Frame completed
		if(grabber->bytes_read >= grabber->frame_size)
		{
			pthread_mutex_unlock(&grabber->output_lock[grabber->input_frame]);
			dv_advance_frame(grabber, &(grabber->input_frame));
			grabber->bytes_read = 0;
			pthread_mutex_lock(&grabber->input_lock[grabber->input_frame]);
		}
	}
	return 0;
}


int dv_reset_handler(raw1394handle_t handle)
{
	dv_grabber_t *grabber = dv_grabber;
	if(!grabber) return 0;
	if(grabber->done) return 0;
	grabber->crash = 1;

	printf("dv_reset_handler\n");

    return 0;
}

void dv_grabber_thread(dv_grabber_t *grabber)
{
	while(!grabber->done)
	{
		raw1394_loop_iterate(grabber->handle);
	}
}

void dv_keepalive_thread(dv_grabber_t *grabber)
{
	while(!grabber->interrupted)
	{
		grabber->still_alive = 0;
		
		grabber->delay.tv_sec = 0;
		grabber->delay.tv_usec = 500000;
		select(0,  NULL,  NULL, NULL, &grabber->delay);
		
		if(grabber->still_alive == 0 && grabber->capturing)
		{
//			printf("dv_keepalive_thread: device crashed\n");
			grabber->crash = 1;
		}
		else
			grabber->crash = 0;
	}
}

void dv_reset_keepalive(dv_grabber_t *grabber)
{
	grabber->capturing = 0;
	grabber->still_alive = 1;
}

// ===================================================================
//                             Entry points
// ===================================================================

// The grabbing algorithm is derived from dvgrab
// http://www.schirmacher.de/arne/dvgrab/.

dv_grabber_t *dv_grabber_new()
{
	dv_grabber_t *grabber;
	grabber = calloc(1, sizeof(dv_grabber_t));
	return grabber;
}

int dv_grabber_delete(dv_grabber_t *grabber)
{
	free(grabber);
	return 0;
}

int dv_start_grabbing(dv_grabber_t *grabber, int port, int channel, int buffers)
{
	int i;
	pthread_attr_t  attr;
	pthread_mutexattr_t mutex_attr;
    int numcards;
    struct raw1394_portinfo pinf[16];
    iso_handler_t oldhandler;

	grabber->port = port;
	grabber->channel = channel;
	grabber->frames = buffers;
	grabber->frame_size = DV_PAL_SIZE;
	grabber->input_frame = grabber->output_frame = 0;

    if(!(grabber->handle = raw1394_get_handle()))
	{
        printf("dv_start_grabbing: couldn't get handle\n");
        return 1;
    }

    if((numcards = raw1394_get_port_info(grabber->handle, pinf, 16)) < 0)
	{
        perror("dv_start_grabbing: couldn't get card info");
        return 1;
    }

	if(!pinf[port].nodes)
	{
		printf("dv_start_grabbing: no devices detected\n");
		raw1394_destroy_handle(grabber->handle);
		return 1;
	}

    if(raw1394_set_port(grabber->handle, grabber->port) < 0)
	{
        perror("dv_start_grabbing: couldn't set port");
		raw1394_destroy_handle(grabber->handle);
        return 1;
    }

    oldhandler = raw1394_set_iso_handler(grabber->handle, grabber->channel, dv_iso_handler);
    raw1394_set_bus_reset_handler(grabber->handle, dv_reset_handler);

    if(raw1394_start_iso_rcv(grabber->handle, grabber->channel) < 0)
	{
        printf("dv_start_grabbing: couldn't start iso receive");
		raw1394_destroy_handle(grabber->handle);
        return 1;
    }

	grabber->frame_buffer = calloc(1, grabber->frames * sizeof(unsigned char*));
	grabber->input_lock = calloc(1, grabber->frames * sizeof(pthread_mutex_t));
	grabber->output_lock = calloc(1, grabber->frames * sizeof(pthread_mutex_t));
	pthread_mutexattr_init(&mutex_attr);
	for(i = 0; i < grabber->frames; i++)
	{
		grabber->frame_buffer[i] = calloc(1, DV_PAL_SIZE);

		pthread_mutex_init(&(grabber->input_lock[i]), &mutex_attr);
		pthread_mutex_init(&(grabber->output_lock[i]), &mutex_attr);
		pthread_mutex_lock(&(grabber->output_lock[i]));
	}

	dv_grabber = grabber;
	grabber->done = 0;

	pthread_attr_init(&attr);
// Start keepalive
	grabber->still_alive = 1;
	grabber->interrupted = 0;
	grabber->crash = 0;
	grabber->capturing = 0;
	pthread_create(&(grabber->keepalive_tid), &attr, (void*)dv_keepalive_thread, grabber);

// Start grabber
	pthread_create(&(grabber->tid), &attr, (void*)dv_grabber_thread, grabber);

	return 0;
}

int dv_stop_grabbing(dv_grabber_t* grabber)
{
	int i;

	grabber->done = 1;
	for(i = 0; i < grabber->frames; i++)
	{
		pthread_mutex_unlock(&grabber->input_lock[i]);
	}

	pthread_join(grabber->tid, 0);
	raw1394_stop_iso_rcv(grabber->handle, grabber->channel);
	raw1394_destroy_handle(grabber->handle);

	for(i = 0; i < grabber->frames; i++)
	{
		pthread_mutex_destroy(&(grabber->input_lock[i]));
		pthread_mutex_destroy(&(grabber->output_lock[i]));
		free(grabber->frame_buffer[i]);
	}

	free(grabber->frame_buffer);
	free(grabber->input_lock);
	free(grabber->output_lock);

// Stop keepalive
	grabber->interrupted = 1;
	pthread_cancel(grabber->keepalive_tid);
	pthread_join(grabber->keepalive_tid, 0);

	memset(grabber, 0, sizeof(dv_grabber_t));
	return 0;
}

int dv_grabber_crashed(dv_grabber_t* grabber)
{
	if(grabber)
		return grabber->crash;
	else
		return 0;
}

int dv_interrupt_grabber(dv_grabber_t* grabber)
{
	int i;
	
	if(grabber)
	{
		pthread_cancel(grabber->tid);
		for(i = 0; i < grabber->frames; i++)
			pthread_mutex_unlock(&(grabber->output_lock[i]));
	}
	return 0;
}

int dv_grab_frame(dv_grabber_t* grabber, unsigned char **frame, long *size)
{
// Device failed to open
	if(!grabber->frame_buffer || grabber->crash)
	{
		return 1;
	}

// Wait for frame to become available
	grabber->capturing = 1;
	pthread_mutex_lock(&(grabber->output_lock[grabber->output_frame]));
	dv_reset_keepalive(grabber);

	*frame = grabber->frame_buffer[grabber->output_frame];
	*size = grabber->frame_size;
	return 0;
}

int dv_unlock_frame(dv_grabber_t* grabber)
{
	pthread_mutex_unlock(&(grabber->input_lock[grabber->output_frame]));
	dv_advance_frame(grabber, &grabber->output_frame);
	return 0;
}

#endif // HAVE_FIREWIRE



dv_t* dv_new()
{
	dv_t *dv = calloc(1, sizeof(dv_t));

// Recursive math occuring later is not thread-safe.
	if(!dv_initted)
	{
		dv_initted = 1;
		weight_init(dv);  
		dct_init(dv);
		dv_dct_248_init(dv);
		dv_construct_vlc_table(dv);
		dv_parse_init(dv);
		dv_place_init(dv);
		dv_ycrcb_init(dv);
	}

	dv->videoseg.bs = bitstream_init();
	dv->audioseg.bs = bitstream_init();
	dv->audioseg.map_norm = -1;
	return dv;
}


int dv_delete(dv_t *dv)
{
	free(dv->audioseg.bs);
	free(dv->videoseg.bs);
	free(dv);
	return 0;
}

int dv_read_video(dv_t *dv, 
		unsigned char **rgb_rows, 
		unsigned char *data, 
		long bytes,
		int color_model)
{
	int dif = 0;
	int lost_coeffs = 0;
	long offset = 0;
	int isPAL = 0;
	int is61834 = 0;
	int numDIFseq;
	int ds;
	int v, b, m;
	dv_block_t *bl;
	long mb_offset;
	dv_sample_t sampling;
	dv_macroblock_t *mb;
	int pixel_size;

	switch(bytes)
	{
		case DV_PAL_SIZE:
			break;
		case DV_NTSC_SIZE:
			break;
		default:
			return 1;
			break;
	}

	if(data[0] != 0x1f) return 1;

	switch(color_model)
	{
		case DV_RGB888:
			pixel_size = 3;
			break;
		
		case DV_BGR8880:
			pixel_size = 4;
			break;
		
		case DV_RGB8880:
			pixel_size = 4;
			break;
	}


	while(offset < bytes)
	{
		offset = dif * 80;
		
		isPAL = data[offset + 3] & 0x80;
		is61834 = data[offset + 3] & 0x01;

		if(isPAL && is61834)
			sampling = e_dv_sample_420;
		else
			sampling = e_dv_sample_411;

		numDIFseq = isPAL ? 12 : 10;

// each DV frame consists of a sequence of DIF segments
		for(ds = 0; ds < numDIFseq; ds++)
		{
// Each DIF segment conists of 150 dif blocks, 135 of which are video blocks
// skip the first 6 dif blocks in a dif sequence 
			dif += 6; 
			
/* A video segment consists of 5 video blocks, where each video
   block contains one compressed macroblock.  DV bit allocation
   for the VLC stage can spill bits between blocks in the same
   video segment.  So the parser needs a complete segment to decode
   the VLC data */
  
// Loop through video segments
   			for(v = 0; v < 27; v++) 
			{
// skip audio block interleaved before every 3rd video segment
				if(!(v % 3)) dif++; 

// stage 1: parse and VLC decode the 5 macroblocks in a video segment	
				offset = dif * 80;
				bitstream_new_buffer(dv->videoseg.bs, &data[offset], 80 * 5); 
				offset += 80 * 5;

				dv->videoseg.i = ds;
				dv->videoseg.k = v;
				dv->videoseg.isPAL = isPAL;

				lost_coeffs += dv_parse_video_segment(&dv->videoseg);


// stage 2: dequant/unweight/iDCT blocks, and place the macroblocks
        		for(m = 0, mb = dv->videoseg.mb; m < 5; m++, mb++)
				{
					for(b = 0, bl = mb->b; b < 6; b++, bl++)
					{
						if(bl->dct_mode == DV_DCT_248) 
						{
	    					quant_248_inverse(bl->coeffs, mb->qno, bl->class_no);
	    					weight_248_inverse(bl->coeffs);
	    					convert_coeffs(bl);
	    					dv_idct_248(bl->coeffs248);
	    					convert_coeffs_prime(bl);
						}
						else
						{
	    					quant_88_inverse(bl->coeffs, mb->qno, bl->class_no);
	    					weight_88_inverse(bl->coeffs);
	    					idct_88(bl->coeffs);
						}
					}

					if(sampling == e_dv_sample_411)
					{
	    				mb_offset = dv_place_411_macroblock(mb, pixel_size);
	    				if((mb->j == 4) && (mb->k > 23)) 
	    					dv_ycrcb_420_block(rgb_rows[0] + mb_offset, mb->b, pixel_size);
	    				else
	    					dv_ycrcb_411_block(rgb_rows[0] + mb_offset, mb->b, pixel_size);
					}
					else 
					{
	    				mb_offset = dv_place_420_macroblock(mb, pixel_size);
	    				dv_ycrcb_420_block(rgb_rows[0] + mb_offset, mb->b, pixel_size);
					}
				}

				dif += 5;
			}
		}
	}
	return 0;
}


int dv_read_audio(dv_t *dv, 
		unsigned char *samples,
		unsigned char *data,
		long size)
{
	long current_position;
	dv_audiosegment_t *seg = &(dv->audioseg);
	int norm;
	int i;
    int audio_bytes;
	int *map1 = seg->map1;
	int *map2 = seg->map2;
	short *samples_int16 = (short*)samples;

	switch(size)
	{
		case DV_PAL_SIZE:
			norm = DV_PAL;
			break;
		case DV_NTSC_SIZE:
			norm = DV_NTSC;
			break;
		default:
			return 0;
			break;
	}

	if(data[0] != 0x1f) return 0;

	if(seg->map_norm != norm)
	{
		switch(norm)
		{
			case DV_PAL:
				for(i = 0; i < 1920; i++)
				{
					int sequence1 = ((i / 3) + 2 * (i % 3)) % 6;
					int sequence2 = ((i / 3) + 2 * (i % 3)) % 6 + 6;
					int block = 3 * (i % 3) + ((i % 54) / 18);
					int byte = 8 + 2 * (i / 54);

					block = 6 + block * 16;
					map1[i] = sequence1 * 150 * 80 + block * 80 + byte;
					map2[i] = sequence2 * 150 * 80 + block * 80 + byte;
				}
				break;
			case DV_NTSC:
				for(i = 0; i < 1620; i++)
				{
					int sequence1 = ((i / 3) + 2 * (i % 3)) % 5;
					int sequence2 = ((i / 3) + 2 * (i % 3)) % 5 + 5;
					int block = 3 * (i % 3) + ((i % 45) / 15);
					int byte = 8 + 2 * (i / 45);

					block = 6 + block * 16;
					map1[i] = sequence1 * 150 * 80 + block * 80 + byte;
					map2[i] = sequence2 * 150 * 80 + block * 80 + byte;
				}
				break;
		}

		seg->map_norm = norm;
	}

	switch(data[6 * 80 + 3 * 16 * 80 + 4] & 0x3f)
	{
		case 20: seg->samples_read = 1600; break;
		case 21: seg->samples_read = 1600; break;
		case 22: seg->samples_read = 1602; break;
		case 23: seg->samples_read = 1602; break;
		case 24: seg->samples_read = 1920; break;
		default: seg->samples_read = 0;    break;
	}

	for(i = 0; i < seg->samples_read; i++)
	{
		*samples_int16++ = ((unsigned int)data[map1[i]] << 8) | data[map1[i] + 1];
		*samples_int16++ = ((unsigned int)data[map2[i]] << 8) | data[map2[i] + 1];
	}

	return seg->samples_read;
}
