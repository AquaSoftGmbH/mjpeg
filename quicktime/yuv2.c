#include "quicktime.h"

/* U V values are signed but Y R G B values are unsigned! */
/*
 *      R = Y               + 1.40200 * V
 *      G = Y - 0.34414 * U - 0.71414 * V
 *      B = Y + 1.77200 * U
 */

/*
 *		Y =  0.2990 * R + 0.5870 * G + 0.1140 * B
 *		U = -0.1687 * R - 0.3310 * G + 0.5000 * B
 *		V =  0.5000 * R - 0.4187 * G - 0.0813 * B  
 */


int quicktime_init_codec_yuv2(quicktime_video_map_t *vtrack)
{
	int i;
	quicktime_yuv2_codec_t *codec = &(vtrack->codecs.yuv2_codec);

	for(i = 0; i < 256; i++)
	{
// compression
		codec->rtoy_tab[i] = (long)( 0.2990 * 65536 * i);
		codec->rtou_tab[i] = (long)(-0.1687 * 65536 * i);
		codec->rtov_tab[i] = (long)( 0.5000 * 65536 * i);

		codec->gtoy_tab[i] = (long)( 0.5870 * 65536 * i);
		codec->gtou_tab[i] = (long)(-0.3320 * 65536 * i);
		codec->gtov_tab[i] = (long)(-0.4187 * 65536 * i);

		codec->btoy_tab[i] = (long)( 0.1140 * 65536 * i);
		codec->btou_tab[i] = (long)( 0.5000 * 65536 * i);
		codec->btov_tab[i] = (long)(-0.0813 * 65536 * i);
	}

	codec->vtor = &(codec->vtor_tab[128]);
	codec->vtog = &(codec->vtog_tab[128]);
	codec->utog = &(codec->utog_tab[128]);
	codec->utob = &(codec->utob_tab[128]);
	for(i = -128; i < 128; i++)
	{
// decompression
		codec->vtor[i] = (long)( 1.4020 * 65536 * i);
		codec->vtog[i] = (long)(-0.7141 * 65536 * i);

		codec->utog[i] = (long)(-0.3441 * 65536 * i);
		codec->utob[i] = (long)( 1.7720 * 65536 * i);
	}
	codec->bytes_per_line = vtrack->track->tkhd.track_width * 2;
	if((float)codec->bytes_per_line / 4 > (int)(codec->bytes_per_line / 4))
		codec->bytes_per_line += 2;
		
	codec->work_buffer = malloc(codec->bytes_per_line *
							vtrack->track->tkhd.track_height);
}

int quicktime_delete_codec_yuv2(quicktime_video_map_t *vtrack)
{
	free(vtrack->codecs.yuv2_codec.work_buffer);
}

int quicktime_decode_yuv2(quicktime_t *file, unsigned char **row_pointers, int track)
{
	long bytes, x, y;
	quicktime_video_map_t *vtrack = &(file->vtracks[track]);
	quicktime_yuv2_codec_t *codec = &(vtrack->codecs.yuv2_codec);
	int width = vtrack->track->tkhd.track_width;
	int height = vtrack->track->tkhd.track_height;
	unsigned char *buffer;
	char *input_row;
	int result = 0;
	int y1, u, v, y2, r, g, b;
	int endpoint = width * 3;

	vtrack->track->tkhd.track_width;
	quicktime_set_video_position(file, vtrack->current_position, track);
	bytes = quicktime_frame_size(file, vtrack->current_position, track);

	if(file->vtracks[track].frames_cached)
	{
		buffer = file->vtracks[track].frame_cache[vtrack->current_position];
	}
	else
	{
		buffer = codec->work_buffer;
		result = quicktime_read_data(file, buffer, bytes);
		if(result) result = 0; else result = 1;
	}

	for(y = 0; y < height; y++)
	{
		input_row = &buffer[y * codec->bytes_per_line];
		for(x = 0; x < endpoint; )
		{
			y1 = (unsigned char)*input_row++;
			u = *input_row++;
			y2 = (unsigned char)*input_row++;
			v = *input_row++;
			y1 <<= 16;
			y2 <<= 16;

			r = ((y1 + codec->vtor[v]) >> 16);
			g = ((y1 + codec->utog[u] + codec->vtog[v]) >> 16);
			b = ((y1 + codec->utob[u]) >> 16);
			if(r < 0) r = 0;
			if(g < 0) g = 0;
			if(b < 0) b = 0;
			if(r > 255) r = 255;
			if(g > 255) g = 255;
			if(b > 255) b = 255;

			row_pointers[y][x++] = r;
			row_pointers[y][x++] = g;
			row_pointers[y][x++] = b;

// Odd numbers of columns quit here
			if(x < endpoint)
			{
				r = ((y2 + codec->vtor[v]) >> 16);
				g = ((y2 + codec->utog[u] + codec->vtog[v]) >> 16);
				b = ((y2 + codec->utob[u]) >> 16);
				if(r < 0) r = 0;
				if(g < 0) g = 0;
				if(b < 0) b = 0;
				if(r > 255) r = 255;
				if(g > 255) g = 255;
				if(b > 255) b = 255;

				row_pointers[y][x++] = r;
				row_pointers[y][x++] = g;
				row_pointers[y][x++] = b;
			}
		}
	}
	return result;
}

int quicktime_encode_yuv2(quicktime_t *file, unsigned char **row_pointers, int track)
{
	long offset = quicktime_position(file);
	quicktime_video_map_t *vtrack = &(file->vtracks[track]);
	quicktime_yuv2_codec_t *codec = &(vtrack->codecs.yuv2_codec);
	int result = 0;
	int width = vtrack->track->tkhd.track_width;
	int height = vtrack->track->tkhd.track_height;
	long bytes = height * codec->bytes_per_line;
	unsigned char *buffer = vtrack->codecs.yuv2_codec.work_buffer;
	unsigned char *row_pointer;
	int x, y;
	int y1, u, y2, v;
	int r, g, b;
	int endpoint = width * 3;

	for(y = 0; y < height; y++)
	{
		row_pointer = buffer + y * codec->bytes_per_line;
		for(x = 0; x < endpoint; )
		{
			r = row_pointers[y][x++];
			g = row_pointers[y][x++];
			b = row_pointers[y][x++];

			y1 = (codec->rtoy_tab[r] + codec->gtoy_tab[g] + codec->btoy_tab[b]);
			u  = (codec->rtou_tab[r] + codec->gtou_tab[g] + codec->btou_tab[b]);
			v  = (codec->rtov_tab[r] + codec->gtov_tab[g] + codec->btov_tab[b]);

			if(x < endpoint)
			{
				r = row_pointers[y][x++];
				g = row_pointers[y][x++];
				b = row_pointers[y][x++];
			}

			y2 = (codec->rtoy_tab[r] + codec->gtoy_tab[g] + codec->btoy_tab[b]);
			u += (codec->rtou_tab[r] + codec->gtou_tab[g] + codec->btou_tab[b]);
			v += (codec->rtov_tab[r] + codec->gtov_tab[g] + codec->btov_tab[b]);

			y1 /= 0x10000;
			y2 /= 0x10000;
			u /= 0x20000;
			v /= 0x20000;
			if(y1 > 255) y1 = 255;
			if(y2 > 255) y2 = 255;
			if(u > 127) u = 127;
			if(v > 127) v = 127;
			if(y1 < 0) y1 = 0;
			if(y2 < 0) y2 = 0;
			if(u < -128) u = -128;
			if(v < -128) v = -128;

			*row_pointer++ = y1;
			*row_pointer++ = u;
			*row_pointer++ = y2;
			*row_pointer++ = v;
		}
	}

	result = quicktime_write_data(file, buffer, bytes);
	if(result) result = 0; else result = 1;

	quicktime_update_tables(file,
						file->vtracks[track].track,
						offset,
						file->vtracks[track].current_chunk,
						file->vtracks[track].current_position,
						1,
						bytes);

	file->vtracks[track].current_chunk++;
	return result;
}

