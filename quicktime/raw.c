#include "quicktime.h"

int quicktime_init_codec_raw(quicktime_video_map_t *vtrack)
{
// Need a temporary frame to swap the byte order
// Not used.
//	if(vtrack->track->mdia.minf.stbl.stsd.table[0].depth == 32)
//		vtrack->codecs.raw_codec.temp_frame = 
//			malloc(vtrack->track->tkhd.track_width * vtrack->track->tkhd.track_height);
}

int quicktime_delete_codec_raw(quicktime_video_map_t *vtrack)
{
//	if(vtrack->track->mdia.minf.stbl.stsd.table[0].depth == 32)
//		free(vtrack->codecs.raw_codec.temp_frame);
}

int quicktime_decode_raw(quicktime_t *file, unsigned char **row_pointers, int track)
{
	register int i, j;
	int result = 0;
	quicktime_trak_t *trak = file->vtracks[track].track;
	int depth = quicktime_video_depth(file, track);
	int height = trak->tkhd.track_height;
	int width = trak->tkhd.track_width;
	int consecutive_rows;
	unsigned char *direct_output;
	long bytes;
	unsigned char temp;
	long bytes_per_row = width * depth / 8;

	if(depth == 32)
	{
		quicktime_set_video_position(file, file->vtracks[track].current_position, track);
		bytes = quicktime_frame_size(file, file->vtracks[track].current_position, track);
// Assume rows are consecutive
		result = !quicktime_read_data(file, row_pointers[0], bytes);
		for(i = 0; i < height; i++)
		{
			for(j = 0; j < bytes_per_row; j += 4)
			{
// Swap alpha from ARGB to RGBA
				temp = row_pointers[i][j];
				row_pointers[i][j] = row_pointers[i][j + 1];
				row_pointers[i][j + 1] = row_pointers[i][j + 2];
				row_pointers[i][j + 2] = row_pointers[i][j + 3];
				row_pointers[i][j + 3] = temp;
			}
		}
	}
	else
	{
		if(!file->vtracks[track].frames_cached)
		{
// read a frame from disk
			quicktime_set_video_position(file, file->vtracks[track].current_position, track);

			if(quicktime_raw_rows_consecutive(row_pointers, width, height, depth / 8))
			{
				bytes = quicktime_frame_size(file, file->vtracks[track].current_position, track);
				result = quicktime_read_data(file, row_pointers[0], bytes);
				if(result) result = 0; else result = 1;
			}
			else
			for(i = 0; i < height && !result; i++)
			{
// Probably never happen so not optimized
				result = quicktime_read_data(file, row_pointers[i], width * depth / 8);
				if(result) result = 0; else result = 1;
			}
		}
		else
		{
// copy a frame from cache.  No longer supported.
			unsigned char *buffer = file->vtracks[track].frame_cache[file->vtracks[track].current_position];

			for(i = 0; i < height && !result; i++)
			{
				for(j = 0; j < width * depth / 8; j++)
				{
					row_pointers[i][j] = *buffer++;
				}
			}
		}
	}   // depth == 32

	return result;
}

int quicktime_encode_raw(quicktime_t *file, unsigned char **row_pointers, int track)
{
	long offset = quicktime_position(file);
	int result = 0;
	register int i, j;
	quicktime_trak_t *trak = file->vtracks[track].track;
	int height = trak->tkhd.track_height;
	int width = trak->tkhd.track_width;
	int depth = quicktime_video_depth(file, track);
	long bytes = height * width * depth / 8;
	long bytes_per_row = width * depth / 8;
	unsigned char temp;

	if(depth == 32)
	{
// Swap byte order to match Quicktime's ARGB.
		for(i = 0; i < height; i++)
		{
			for(j = 0; j < bytes_per_row; j += 4)
			{
				temp = row_pointers[i][j + 3];
				row_pointers[i][j + 3] = row_pointers[i][j + 2];
				row_pointers[i][j + 2] = row_pointers[i][j + 1];
				row_pointers[i][j + 1] = row_pointers[i][j];
				row_pointers[i][j] = temp;
			}
		}
		result = !quicktime_write_data(file, row_pointers[0], bytes);
	}
	else
	{
		if(quicktime_raw_rows_consecutive(row_pointers, width, height, depth / 8))
		{
// Rows are consecutive so write as a block
			result = quicktime_write_data(file, row_pointers[0], bytes);
			if(result) result = 0; else result = 1;
		}
		else
		for(i = 0; i < height && !result; i++)
		{
// Rows aren't consecutive but this should never happen.
			result = quicktime_write_data(file, row_pointers[i], width * depth / 8);
			if(result) result = 0; else result = 1;
		}
	}

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

int quicktime_raw_rows_consecutive(unsigned char **row_pointers, int w, int h, int depth)
{
	int i, result;
// see if row_pointers are consecutive
	for(i = 1, result = 1; i < h; i++)
	{
		if(row_pointers[i] - row_pointers[i - 1] != w * depth) result = 0;
	}
	return result;
}
