#include <stdio.h>
#include "jpeg.h"
#include "quicktime.h"

/* JPEG MARKERS */
#define   M_SOF0    0xc0
#define   M_SOF1    0xc1
#define   M_SOF2    0xc2
#define   M_SOF3    0xc3
#define   M_SOF5    0xc5
#define   M_SOF6    0xc6
#define   M_SOF7    0xc7
#define   M_JPG     0xc8
#define   M_SOF9    0xc9
#define   M_SOF10   0xca
#define   M_SOF11   0xcb
#define   M_SOF13   0xcd
#define   M_SOF14   0xce
#define   M_SOF15   0xcf
#define   M_DHT     0xc4
#define   M_DAC     0xcc
#define   M_RST0    0xd0
#define   M_RST1    0xd1
#define   M_RST2    0xd2
#define   M_RST3    0xd3
#define   M_RST4    0xd4
#define   M_RST5    0xd5
#define   M_RST6    0xd6
#define   M_RST7    0xd7
#define   M_SOI     0xd8
#define   M_EOI     0xd9
#define   M_SOS     0xda
#define   M_DQT     0xdb
#define   M_DNL     0xdc
#define   M_DRI     0xdd
#define   M_DHP     0xde
#define   M_EXP     0xdf
#define   M_APP0    0xe0
#define   M_APP1    0xe1
#define   M_APP2    0xe2
#define   M_APP3    0xe3
#define   M_APP4    0xe4
#define   M_APP5    0xe5
#define   M_APP6    0xe6
#define   M_APP7    0xe7
#define   M_APP8    0xe8
#define   M_APP9    0xe9
#define   M_APP10   0xea
#define   M_APP11   0xeb
#define   M_APP12   0xec
#define   M_APP13   0xed
#define   M_APP14   0xee
#define   M_APP15   0xef
#define   M_JPG0    0xf0
#define   M_JPG13   0xfd
#define   M_COM     0xfe
#define   M_TEM     0x01
#define   M_ERROR   0x100

// Buffer and error handling from jpeg-6b examples

METHODDEF(void)
my_error_exit (j_common_ptr cinfo)
{
  /* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
  my_error_ptr myerr = (my_error_ptr) cinfo->err;

  /* Always display the message. */
  /* We could postpone this until after returning, if we chose. */
  (*cinfo->err->output_message) (cinfo);

  /* Return control to the setjmp point */
  longjmp(myerr->setjmp_buffer, 1);
}

// Original code

int quicktime_init_codec_jpeg(quicktime_video_map_t *vtrack, int jpeg_type)
{
	quicktime_jpeg_codec_t *codec = &(vtrack->codecs.jpeg_codec);
	int i;
	char *compressor = vtrack->track->mdia.minf.stbl.stsd.table[0].format;

	codec->quality = 100;
	codec->use_float = 0;
	codec->jpeg_type = jpeg_type;

	for(i = 0; i < TOTAL_MJPA_COMPRESSORS; i++)
	{
		codec->compressors[i] = 0;
		codec->decompressors[i] = 0;
	}
	codec->total_compressors = 0;
	codec->total_decompressors = 0;
	codec->input_buffer = 0;

	
// This information must be stored in the initialization routine because of
// direct copy rendering.  Quicktime for Windows must have this information.
	if(quicktime_match_32(compressor, QUICKTIME_MJPA) && !vtrack->track->mdia.minf.stbl.stsd.table[0].fields)
	{
		vtrack->track->mdia.minf.stbl.stsd.table[0].fields = 2;
		vtrack->track->mdia.minf.stbl.stsd.table[0].field_dominance = 1;
	}

}

int quicktime_delete_codec_jpeg(quicktime_video_map_t *vtrack)
{
	quicktime_jpeg_codec_t *codec = &(vtrack->codecs.jpeg_codec);
	int i;
	for(i = 0; i < codec->total_compressors; i++)
	{
		if(codec->compressors[i])
		{
			quicktime_endcompressor_jpeg(codec->compressors[i]);
			jpeg_destroy((j_common_ptr)&(codec->compressors[i]->jpeg_compress));
			
			if(codec->compressors[i]->output_buffer)
				free(codec->compressors[i]->output_buffer);

			free(codec->compressors[i]);
			codec->compressors[i] = 0;
		}

		if(codec->decompressors[i])
		{
			quicktime_enddecompressor_jpeg(codec->decompressors[i]);
			jpeg_destroy_decompress(&(codec->decompressors[i]->jpeg_decompress));

			free(codec->decompressors[i]);
			codec->decompressors[i] = 0;
		}
	}
	if(codec->input_buffer)
		free(codec->input_buffer);
	codec->input_buffer = 0;
}

int quicktime_set_jpeg(quicktime_t *file, int quality, int use_float)
{
	int i;
	char *compressor;

	for(i = 0; i < file->total_vtracks; i++)
	{
		if(quicktime_match_32(quicktime_video_compressor(file, i), QUICKTIME_JPEG) ||
			quicktime_match_32(quicktime_video_compressor(file, i), QUICKTIME_MJPA) ||
			quicktime_match_32(quicktime_video_compressor(file, i), QUICKTIME_MJPB) ||
			quicktime_match_32(quicktime_video_compressor(file, i), QUICKTIME_RTJ0))
		{
			quicktime_jpeg_codec_t *codec = &(file->vtracks[i].codecs.jpeg_codec);
			codec->quality = quality;
			codec->use_float = use_float;
//			file->vtracks[i].codecs.rtjpeg_codec.quality = quality;
		}
	}
}

int quicktime_read_int32_jpeg(char **data, int *length)
{
	if(*length < 4) return 0;
	*length -= 4;
	*data += 4;
	return (int)((((unsigned char)(*data)[-4]) << 24) | 
		(((unsigned char)(*data)[-3]) << 16) | 
		(((unsigned char)(*data)[-2]) << 8) | 
		(((unsigned char)(*data)[-1])));
}

int quicktime_write_int32_jpeg(char **data, int *length, int value)
{
	if(*length < 4) return 0;
	*length -= 4;

	*(*data)++ = (unsigned int)(value & 0xff000000) >> 24;
	*(*data)++ = (unsigned int)(value & 0xff0000) >> 16;
	*(*data)++ = (unsigned int)(value & 0xff00) >> 8;
	*(*data)++ = (unsigned char)(value & 0xff);
	return 0;
}

int quicktime_read_int16_jpeg(char **data, int *length)
{
	if(*length < 2) return 0;
	*length -= 2;
	*data += 2;
	return (((QUICKTIME_INT16)((unsigned char)(*data)[-2]) << 8) | 
		((unsigned char)(*data)[-1]));
}

int quicktime_readbyte_jpeg(char **data, int *length)
{
	if(*length < 1) return 0;
	*length -= 1;
	*data += 1;
	return (unsigned char)(*data)[-1];
}

int quicktime_read_markers_jpeg(quicktime_mjpeg_hdr *mjpeg_hdr, struct jpeg_decompress_struct *jpeg_decompress)
{
	int done = 0;
	int length;
	char *data;
	jpeg_saved_marker_ptr marker_ptr;

	if(jpeg_decompress)
		marker_ptr = jpeg_decompress->marker_list;

	while((marker_ptr || !jpeg_decompress) && !done)
	{
		if((marker_ptr && marker_ptr->marker == JPEG_APP0 + 1) || !jpeg_decompress)
		{
			if(marker_ptr)
			{
				length = marker_ptr->data_length;
				data = marker_ptr->data;
			}
			else
			{
				length = QUICKTIME_JPEG_MARKSIZE;
				data = mjpeg_hdr->mjpeg_marker;
			}

			quicktime_read_int32_jpeg(&data, &length);
			quicktime_read_int32_jpeg(&data, &length);
			mjpeg_hdr->field_size = quicktime_read_int32_jpeg(&data, &length);
			mjpeg_hdr->padded_field_size = quicktime_read_int32_jpeg(&data, &length);
			mjpeg_hdr->next_offset = quicktime_read_int32_jpeg(&data, &length);
			mjpeg_hdr->quant_offset = quicktime_read_int32_jpeg(&data, &length);
			mjpeg_hdr->huffman_offset = quicktime_read_int32_jpeg(&data, &length);
			mjpeg_hdr->image_offset = quicktime_read_int32_jpeg(&data, &length);
			mjpeg_hdr->scan_offset = quicktime_read_int32_jpeg(&data, &length);
			mjpeg_hdr->data_offset = quicktime_read_int32_jpeg(&data, &length);
// printf("%x %x %x %x %x %x %x %x\n", mjpeg_hdr->field_size, 
// 	mjpeg_hdr->padded_field_size, 
// 	mjpeg_hdr->next_offset, 
// 	mjpeg_hdr->quant_offset, 
// 	mjpeg_hdr->huffman_offset, 
// 	mjpeg_hdr->image_offset, 
// 	mjpeg_hdr->scan_offset, 
// 	mjpeg_hdr->data_offset);
			done = 1;
		}
		if(marker_ptr) marker_ptr = marker_ptr->next;
	}
	return 0;
}

int quicktime_skipmarker_jpeg(char **buffer_ptr, int *buffer_size, int *len)
{
	*buffer_ptr += *len;
	*len = 0;
	*buffer_size -= *len;
}

int quicktime_getmarker_jpeg(char **buffer_ptr, int *buffer_size, int *len)
{
	int c, done = 0;  // 1 - completion    2 - error

	while(!done)
	{
		c = quicktime_readbyte_jpeg(buffer_ptr, buffer_size);
/* look for FF */
		while(c != 0xFF)
		{
			if(!*buffer_size) done = 2;
			c = quicktime_readbyte_jpeg(buffer_ptr, buffer_size);
		}

/* now we've got 1 0xFF, keep reading until not 0xFF */
		do
		{
			if(!*buffer_size) done = 2;
			c = quicktime_readbyte_jpeg(buffer_ptr, buffer_size);
		}while (c == 0xFF);
		
/* not a 00 or FF */
		if (c != 0) done = 1; 
	}

	*len = 0;
	if(done == 1) 
		return c;
	else
		return 0;
}

int quicktime_fixmarker_jpeg(quicktime_mjpeg_hdr *mjpeg_hdr, char *buffer, long image_size, int write_next_offset)
{
	char *buffer_ptr = buffer;
	int buffer_size = image_size;
	int done = 0, offset = 0, app1_offset = 0;
	int marker = 0;
	int len;

	mjpeg_hdr->field_size = 0;
	mjpeg_hdr->padded_field_size = 0;
	mjpeg_hdr->next_offset = 0;
	mjpeg_hdr->quant_offset = 0;
	mjpeg_hdr->huffman_offset = 0;
	mjpeg_hdr->image_offset = 0;
	mjpeg_hdr->scan_offset = 0;
	mjpeg_hdr->data_offset = 0;

	while(!done)
	{
		marker = quicktime_getmarker_jpeg(&buffer_ptr, &buffer_size, &len);
		offset = buffer_ptr - buffer - 1;
		len = 0;

		switch(marker)
		{
			case M_SOI:
				len = 0;
				break;
			
			case M_DHT:
				mjpeg_hdr->huffman_offset = offset - 1;
				len = quicktime_read_int16_jpeg(&buffer_ptr, &buffer_size);
				if(!mjpeg_hdr->mjpg_kludge) 
					len -= 2;
				break;
			
			case M_DQT:
				mjpeg_hdr->quant_offset = offset - 1;
				len = quicktime_read_int16_jpeg(&buffer_ptr, &buffer_size);
				if(!mjpeg_hdr->mjpg_kludge) 
					len -= 2;
				break;

			case M_SOF0:
				mjpeg_hdr->image_offset = offset - 1;
				len = quicktime_read_int16_jpeg(&buffer_ptr, &buffer_size);
				if(!mjpeg_hdr->mjpg_kludge) 
					len -= 2;
				break;

			case M_SOS:
				mjpeg_hdr->scan_offset = offset - 1;
				len = quicktime_read_int16_jpeg(&buffer_ptr, &buffer_size);
				if(!mjpeg_hdr->mjpg_kludge) 
					len -= 2;
				mjpeg_hdr->data_offset = offset + len + 3;
				done = 1;
				break;

			case (JPEG_APP0 + 1):
				app1_offset = offset + 3;
				len = quicktime_read_int16_jpeg(&buffer_ptr, &buffer_size) - 2;
				break;

			case 0:
				done = 1;
				break;
		}

		if(!done) quicktime_skipmarker_jpeg(&buffer_ptr, &buffer_size, &len);
	}

	mjpeg_hdr->field_size = mjpeg_hdr->padded_field_size = mjpeg_hdr->next_offset = image_size;
	buffer_ptr = buffer + app1_offset;
	buffer_size = image_size - app1_offset;
	if(!write_next_offset) mjpeg_hdr->next_offset = 0;

// printf("%d %x %x %x %x %x %x %x %x \n", row_offset, mjpeg_hdr->field_size, 
// 	mjpeg_hdr->padded_field_size, 
// 	mjpeg_hdr->next_offset, 
// 	mjpeg_hdr->quant_offset, 
// 	mjpeg_hdr->huffman_offset, 
// 	mjpeg_hdr->image_offset, 
// 	mjpeg_hdr->scan_offset, 
// 	mjpeg_hdr->data_offset);

	quicktime_write_int32_jpeg(&buffer_ptr, &buffer_size, 0);
	quicktime_write_int32_jpeg(&buffer_ptr, &buffer_size, QUICKTIME_JPEG_TAG);
	quicktime_write_int32_jpeg(&buffer_ptr, &buffer_size, mjpeg_hdr->field_size);
	quicktime_write_int32_jpeg(&buffer_ptr, &buffer_size, mjpeg_hdr->padded_field_size);
	quicktime_write_int32_jpeg(&buffer_ptr, &buffer_size, mjpeg_hdr->next_offset);
	quicktime_write_int32_jpeg(&buffer_ptr, &buffer_size, mjpeg_hdr->quant_offset);
	quicktime_write_int32_jpeg(&buffer_ptr, &buffer_size, mjpeg_hdr->huffman_offset);
	quicktime_write_int32_jpeg(&buffer_ptr, &buffer_size, mjpeg_hdr->image_offset);
	quicktime_write_int32_jpeg(&buffer_ptr, &buffer_size, mjpeg_hdr->scan_offset);
	quicktime_write_int32_jpeg(&buffer_ptr, &buffer_size, mjpeg_hdr->data_offset);
}

void quicktime_decompressor_jpeg(mjpa_decompress_engine *engine)
{
// Run continuously
	unsigned char **interlaced_row;
	unsigned char **last_row;

	while(!engine->done)
	{
		pthread_mutex_lock(&(engine->input_lock));

// Image decompression core
		if(!engine->done)
		{
			if(setjmp(engine->jpeg_error.setjmp_buffer))
			{
// If we get here, the JPEG code has signaled an error.
				goto finish;
			}

			jpeg_buffer_src(&(engine->jpeg_decompress), engine->input_ptr, engine->input_size);
			if(engine->markers_only)
				jpeg_save_markers(&(engine->jpeg_decompress), JPEG_APP0 + 1, QUICKTIME_JPEG_MARKSIZE);

			jpeg_read_header(&(engine->jpeg_decompress), TRUE);

			if(engine->markers_only)
			{
				quicktime_read_markers_jpeg(&(engine->mjpeg_hdr), &(engine->jpeg_decompress));
				engine->field_offset = engine->mjpeg_hdr.next_offset;
				pthread_mutex_unlock(&(engine->output_lock));
				pthread_mutex_lock(&(engine->input_lock));
			}

			jpeg_start_decompress(&(engine->jpeg_decompress));
			if(!engine->interlaced)
			{
				while(engine->jpeg_decompress.output_scanline < engine->jpeg_decompress.output_height)
				{
					jpeg_read_scanlines(&engine->jpeg_decompress, 
						(JSAMPROW*)&engine->row_pointers[engine->jpeg_decompress.output_scanline], 
						engine->jpeg_decompress.output_height - engine->jpeg_decompress.output_scanline);
				}
			}
			else
			{
				interlaced_row = engine->row_pointers;
				last_row = &(engine->row_pointers[engine->height]);
				while(engine->jpeg_decompress.output_scanline < engine->jpeg_decompress.output_height &&
						interlaced_row < last_row)
				{
					jpeg_read_scanlines(&(engine->jpeg_decompress), 
						(JSAMPROW*)interlaced_row, 
						1);

					interlaced_row += 2;
				}
			}
			jpeg_finish_decompress(&(engine->jpeg_decompress));
		}

finish:
		pthread_mutex_unlock(&(engine->output_lock));
	}
}

int quicktime_startdecompressor_jpeg(mjpa_decompress_engine *engine)
{
	pthread_attr_t  attr;
	struct sched_param param;
	pthread_mutexattr_t mutex_attr;

	pthread_mutexattr_init(&mutex_attr);
	pthread_mutex_init(&(engine->input_lock), &mutex_attr);
	pthread_mutex_lock(&(engine->input_lock));
	pthread_mutex_init(&(engine->output_lock), &mutex_attr);
	pthread_mutex_lock(&(engine->output_lock));

	pthread_attr_init(&attr);
	pthread_create(&(engine->tid), &attr, (void*)quicktime_decompressor_jpeg, engine);
}

int quicktime_enddecompressor_jpeg(mjpa_decompress_engine *engine)
{
	engine->done = 1;
	pthread_mutex_unlock(&(engine->input_lock));
	pthread_join(engine->tid, 0);
}

int quicktime_decompressfield_jpeg(mjpa_decompress_engine *engine, 
				char *input_ptr, 
				long input_size, 
				unsigned char **row_pointers, 
				int markers_only, 
				int resume)
{
	engine->markers_only = markers_only;
	engine->row_pointers = row_pointers;
	engine->input_ptr = input_ptr;
	engine->input_size = input_size;
	pthread_mutex_unlock(&(engine->input_lock));
}

int quicktime_decompresswait_jpeg(mjpa_decompress_engine *engine)
{
	pthread_mutex_lock(&(engine->output_lock));
}


int quicktime_decode_jpeg(quicktime_t *file, unsigned char **row_pointers, int track)
{
	int result = 0;
	register int i;
	long color_channels, bytes;
	quicktime_trak_t *trak = file->vtracks[track].track;
	quicktime_video_map_t *vtrack = &(file->vtracks[track]);
	quicktime_jpeg_codec_t *codec = &(vtrack->codecs.jpeg_codec);
	int is_mjpa = vtrack->codecs.jpeg_codec.jpeg_type == 1;
	int is_mjpb = vtrack->codecs.jpeg_codec.jpeg_type == 2;
	int interlaced = is_mjpa || is_mjpb;
	int row_offset, field_offset;
	unsigned char **interlaced_row, **last_row;
	int field_dominance = trak->mdia.minf.stbl.stsd.table[0].field_dominance;
	long size;
	long size_remaining;
	int height = (int)trak->tkhd.track_height;
	int width = (int)trak->tkhd.track_width;

// Create decompression engines as needed
	for(i = 0; i < (interlaced ? TOTAL_MJPA_COMPRESSORS : 1); i++)
	{
		if(!codec->decompressors[i])
		{
			codec->decompressors[i] = malloc(sizeof(mjpa_decompress_engine));
			codec->decompressors[i]->done = 0;
			codec->decompressors[i]->jpeg_decompress.err = jpeg_std_error(&(codec->decompressors[i]->jpeg_error.pub));
			codec->decompressors[i]->jpeg_error.pub.error_exit = my_error_exit;
// Ideally the error handler would be set here but it must be called in a thread
			jpeg_create_decompress(&(codec->decompressors[i]->jpeg_decompress));
			codec->decompressors[i]->is_mjpa = is_mjpa;
			codec->decompressors[i]->mjpeg_hdr.mjpg_kludge = 0;
			codec->decompressors[i]->interlaced = interlaced;
			codec->decompressors[i]->width = width;
			codec->decompressors[i]->height = height;
			codec->decompressors[i]->codec = codec;
			quicktime_startdecompressor_jpeg(codec->decompressors[i]);
			codec->total_decompressors++;
		}
	}

// Read the entire chunk from disk.

	quicktime_set_video_position(file, vtrack->current_position, track);
	size = quicktime_frame_size(file, vtrack->current_position, track);
	if(size > codec->buffer_size && codec->input_buffer)
	{
		free(codec->input_buffer);
		codec->input_buffer = 0;
	}
	if(!codec->input_buffer)
	{
		codec->input_buffer = malloc(size);
		codec->buffer_size = size;
	}
	result = quicktime_read_data(file, codec->input_buffer, size);
	result = !result;

// Start the decompressors
	if(field_dominance == 2 && interlaced) 
		row_offset = 1;
	else
		row_offset = 0;

	field_offset = 0;
	for(i = 0; i < (interlaced ? TOTAL_MJPA_COMPRESSORS : 1) && !result; i++)
	{
		interlaced_row = row_pointers + row_offset;

		if(i > 0) size_remaining = size - field_offset;
		else
		size_remaining = size;

		quicktime_decompressfield_jpeg(codec->decompressors[i], 
						codec->input_buffer + field_offset, 
						size_remaining, 
						interlaced_row, 
						(interlaced && i == 0), 
						0);

		if(interlaced && i == 0)
		{
// Get field offset from first field
			quicktime_decompresswait_jpeg(codec->decompressors[i]);
			field_offset = codec->decompressors[i]->field_offset;
			quicktime_decompressfield_jpeg(codec->decompressors[i], 
						codec->input_buffer, 
						field_offset, 
						interlaced_row, 
						0, 
						1);
		}

// Wait for decompressor completion on uniprocessor
		if(file->cpus < 2)
		{
			quicktime_decompresswait_jpeg(codec->decompressors[i]);
		}

		row_offset ^= 1;
	}

// Wait for decompressor completion
	for(i = 0; i < (interlaced ? TOTAL_MJPA_COMPRESSORS : 1) && !result && file->cpus > 1; i++)
	{
		quicktime_decompresswait_jpeg(codec->decompressors[i]);
	}

	return result;
}

void quicktime_compressor_jpeg(mjpa_compress_engine *engine)
{
// Run continuously
	unsigned char **interlaced_row;

	while(!engine->done)
	{
		pthread_mutex_lock(&(engine->input_lock));

		if(!engine->done)
		{
// Image compression core
			engine->image_size = 0;
			jpeg_buffer_dest(&(engine->jpeg_compress), engine);

// Initialize interlaced output
			jpeg_start_compress(&(engine->jpeg_compress), TRUE);

// Write a fake MJPA marker
			if(engine->is_mjpa)
			{
// 			char string[] = { 'A', 'p', 'p', 'l', 'e', 'M', 'a', 'r', 'k', 10 };
// 			
// 			jpeg_write_marker(&(engine->jpeg_compress), 
// 						JPEG_COM, 
// 						string, 
// 						strlen(string));
				jpeg_write_marker(&(engine->jpeg_compress), 
							JPEG_APP0 + 1, 
							engine->mjpeg_hdr.mjpeg_marker, 
							QUICKTIME_JPEG_MARKSIZE);
			}

			if(!engine->interlaced)
			{
				while(engine->jpeg_compress.next_scanline < engine->jpeg_compress.image_height)
				{
					jpeg_write_scanlines(&engine->jpeg_compress, 
								&(engine->row_pointers[engine->jpeg_compress.next_scanline]), 
								engine->jpeg_compress.image_height - engine->jpeg_compress.next_scanline);
				}
			}
			else
			{
				interlaced_row = engine->row_pointers;
				while(engine->jpeg_compress.next_scanline < engine->jpeg_compress.image_height)
				{
					jpeg_write_scanlines(&engine->jpeg_compress, 
						(JSAMPROW*)interlaced_row, 
						1);

					interlaced_row += 2;
				}
			}
			jpeg_finish_compress(&engine->jpeg_compress);

			if(engine->is_mjpa)
			{
// Fix markers and write whole thing
				quicktime_fixmarker_jpeg(&(engine->mjpeg_hdr), 
							engine->output_buffer, 
							engine->image_size, 
							engine->write_next_offset);
			}
			pthread_mutex_unlock(&(engine->output_lock));
		}
	}
}

int quicktime_startcompressor_jpeg(mjpa_compress_engine *engine)
{
	pthread_attr_t  attr;
	struct sched_param param;
	pthread_mutexattr_t mutex_attr;

	pthread_mutexattr_init(&mutex_attr);
	pthread_mutex_init(&(engine->input_lock), &mutex_attr);
	pthread_mutex_lock(&(engine->input_lock));
	pthread_mutex_init(&(engine->output_lock), &mutex_attr);
	pthread_mutex_lock(&(engine->output_lock));

	pthread_attr_init(&attr);
	pthread_create(&(engine->tid), &attr, (void*)quicktime_compressor_jpeg, engine);
}

int quicktime_endcompressor_jpeg(mjpa_compress_engine *engine)
{
	engine->done = 1;
	pthread_mutex_unlock(&(engine->input_lock));
	pthread_join(engine->tid, 0);
}

int quicktime_compressfield_jpeg(mjpa_compress_engine *engine, 
				unsigned char **row_pointers, int write_next_offset)
{
	engine->row_pointers = row_pointers;
	engine->write_next_offset = write_next_offset;
	pthread_mutex_unlock(&(engine->input_lock));
}

int quicktime_compresswait_jpeg(mjpa_compress_engine *engine)
{
	pthread_mutex_lock(&(engine->output_lock));
}


int quicktime_encode_jpeg(quicktime_t *file, unsigned char **row_pointers, int track)
{
	long offset = quicktime_position(file);
	int result = 0;
	register int i;
	quicktime_trak_t *trak = file->vtracks[track].track;
	quicktime_video_map_t *vtrack = &(file->vtracks[track]);
	quicktime_jpeg_codec_t *codec = &(vtrack->codecs.jpeg_codec);
	int is_mjpa = codec->jpeg_type == 1;
	int is_mjpb = codec->jpeg_type == 2;
	int interlaced = is_mjpa || is_mjpb;
	int row_offset;
	int image_start;
	unsigned char **interlaced_row, **last_row;
	long bytes;
	int height = (int)trak->tkhd.track_height;
	int width = (int)trak->tkhd.track_width;

// Create compression engines as needed
	for(i = 0; i < (interlaced ? TOTAL_MJPA_COMPRESSORS : 1); i++)
	{
		if(!codec->compressors[i])
		{
			codec->compressors[i] = malloc(sizeof(mjpa_compress_engine));
			codec->compressors[i]->output_buffer = malloc(width * height * 3 / 2);
			codec->compressors[i]->output_size = width * height * 3 / 2;
			codec->compressors[i]->done = 0;
// Initialize the jpeglib structures here
			codec->compressors[i]->jpeg_compress.err = jpeg_std_error(&(codec->compressors[i]->jpeg_error));
			jpeg_create_compress(&(codec->compressors[i]->jpeg_compress));
			codec->compressors[i]->jpeg_compress.input_components = 3;
			codec->compressors[i]->jpeg_compress.in_color_space = JCS_RGB;
			jpeg_set_defaults(&(codec->compressors[i]->jpeg_compress));
			jpeg_set_quality(&(codec->compressors[i]->jpeg_compress), vtrack->codecs.jpeg_codec.quality, 0);
			if(vtrack->codecs.jpeg_codec.use_float)
				codec->compressors[i]->jpeg_compress.dct_method = JDCT_FLOAT;

// Progression made it twice as slow.	
//			jpeg_simple_progression(&(codec->compressors[i]->jpeg_compress));

// Changing the sampling to all values but default increased compression time.
			if(interlaced)
			{
// Fix sampling for interlaced
// 				codec->compressors[i]->jpeg_compress.comp_info[0].h_samp_factor = 2;
// 				codec->compressors[i]->jpeg_compress.comp_info[0].v_samp_factor = 2;
// 				codec->compressors[i]->jpeg_compress.comp_info[1].h_samp_factor = 1;
// 				codec->compressors[i]->jpeg_compress.comp_info[1].v_samp_factor = 1;
// 				codec->compressors[i]->jpeg_compress.comp_info[2].h_samp_factor = 1;
// 				codec->compressors[i]->jpeg_compress.comp_info[2].v_samp_factor = 1;
			}
// Huffman optimization saved %5
//			codec->compressors[i]->jpeg_compress.optimize_coding = TRUE;
// Omitting tables saved the same %5
//			jpeg_suppress_tables(&(codec->compressors[i]->jpeg_compress), TRUE);
			codec->compressors[i]->jpeg_compress.image_width = width;
			codec->compressors[i]->jpeg_compress.image_height = !interlaced ? height : ((height - i) >> 1);
			codec->compressors[i]->is_mjpa = is_mjpa;
			codec->compressors[i]->mjpeg_hdr.mjpg_kludge = 0;
			codec->compressors[i]->width = width;
			codec->compressors[i]->height = height;
			codec->compressors[i]->interlaced = interlaced;
			codec->compressors[i]->codec = codec;

// Start threads waiting
			quicktime_startcompressor_jpeg(codec->compressors[i]);

			codec->total_compressors++;
		}
	}


// Start the compressors on the image fields
	for(row_offset = 0; row_offset < (interlaced ? TOTAL_MJPA_COMPRESSORS : 1) && !result; row_offset++)
	{
		quicktime_compressfield_jpeg(codec->compressors[row_offset], 
				row_pointers + row_offset, !row_offset);

		if(file->cpus < 2 && row_offset < TOTAL_MJPA_COMPRESSORS - 1)
		{
			quicktime_compresswait_jpeg(codec->compressors[row_offset]);
		}
	}

// Wait for the compressors and write to disk
	for(row_offset = 0; row_offset < (interlaced ? TOTAL_MJPA_COMPRESSORS : 1) && !result; row_offset++)
	{
		if(file->cpus > 1 || row_offset == TOTAL_MJPA_COMPRESSORS - 1)
		{
			quicktime_compresswait_jpeg(codec->compressors[row_offset]);
		}

		result = quicktime_write_data(file, 
					codec->compressors[row_offset]->output_buffer, 
					codec->compressors[row_offset]->image_size);
		result = !result;
	}

	bytes = quicktime_position(file) - offset;
	quicktime_update_tables(file,
						vtrack->track,
						offset,
						vtrack->current_chunk,
						vtrack->current_position,
						1,
						bytes);

	vtrack->current_chunk++;
	return result;
}
