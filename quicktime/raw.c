#include "quicktime.h"
#include "raw.h"
#include "funcprotos.h"

static int quicktime_delete_codec_raw(quicktime_video_map_t *vtrack)
{
	quicktime_raw_codec_t *codec = ((quicktime_codec_t*)vtrack->codec)->priv;
	if(codec->temp_frame)
	{
		free(codec->temp_rows);
		free(codec->temp_frame);
	}
	free(codec);
	return 0;
}

static int quicktime_decode_raw(quicktime_t *file, unsigned char **row_pointers, int track)
{
	int result = 0;
	quicktime_trak_t *trak = file->vtracks[track].track;
	int frame_depth = quicktime_video_depth(file, track);
	int track_height = trak->tkhd.track_height;
	int track_width = trak->tkhd.track_width;
	long bytes;
	quicktime_raw_codec_t *codec = ((quicktime_codec_t*)file->vtracks[track].codec)->priv;
	int pixel_size = frame_depth / 8;
	long bytes_per_row = track_width * pixel_size;

	if(codec->scaletable)
	{
		if(codec->scaletable && 
			quicktime_compare_scaletable(codec->scaletable, file->in_w, file->in_h, file->out_w, file->out_h))
		{
			quicktime_delete_scaletable(codec->scaletable);
			codec->scaletable = 0;
		}
	}
	
	if(!codec->scaletable)
	{
		codec->scaletable = quicktime_new_scaletable(file->in_w, file->in_h, file->out_w, file->out_h);
	}

/* Read data */
	quicktime_set_video_position(file, file->vtracks[track].current_position, track);
	bytes = quicktime_frame_size(file, file->vtracks[track].current_position, track);

/* Read data directly into output */
	if(quicktime_identity_scaletable(codec->scaletable) && 
		((file->color_model == QUICKTIME_RGB888 && frame_depth == 24) || 
		(file->color_model == QUICKTIME_BGR8880 && frame_depth == 32) ||
		(file->color_model == QUICKTIME_RGBA8880 && frame_depth == 32)))
	{
		result = !quicktime_read_data(file, row_pointers[0], bytes);

/* Swap bytes */
/* ARGB to RGBA */
		if(file->color_model == QUICKTIME_RGBA8880)
		{
			register unsigned char temp;
			register int i, j;
			for(i = 0; i < track_height; i++)
			{
				for(j = 0; j < bytes_per_row; j += 4)
				{
					temp = row_pointers[i][j];
					row_pointers[i][j] = row_pointers[i][j + 1];
					row_pointers[i][j + 1] = row_pointers[i][j + 2];
					row_pointers[i][j + 2] = row_pointers[i][j + 3];
					row_pointers[i][j + 3] = temp;
				}
			}
		}
		else
/* ARGB to BGR */
		if(file->color_model == QUICKTIME_BGR8880)
		{
			register unsigned char temp;
			register int i, j;
			for(i = 0; i < track_height; i++)
			{
				for(j = 0; j < bytes_per_row; j += 4)
				{
					temp = row_pointers[i][j];
					row_pointers[i][j] = row_pointers[i][j + 2];
					row_pointers[i][j + 2] = temp;
				}
			}
		}
	}
	else
/* Read data into temp frame */
/* Eventually this should be put into graphics.c */
	{
		int i, j;
		unsigned char *input_row, *output_row;

		if(!codec->temp_frame)
		{
			codec->temp_frame = calloc(1, pixel_size * track_width * track_height);
			codec->temp_rows = calloc(1, sizeof(unsigned char*) * track_height);
			for(i = 0; i < track_height; i++) codec->temp_rows[i] = &codec->temp_frame[i * pixel_size * track_width];
		}

		result = !quicktime_read_data(file, codec->temp_frame, bytes);

/* Scale rows */
		for(i = 0; i < file->out_h; i++)
		{
			input_row = codec->temp_rows[codec->scaletable->input_y[i] + file->in_y] + pixel_size * file->in_x;
			output_row = row_pointers[i];

/* Scale columns */
			if(file->out_w != file->in_w)
			{
				switch(file->color_model)
				{
					case QUICKTIME_RGB888:
/* ARGB to RGB */
						if(frame_depth == 32)
						{
							for(j = 0; j < file->out_w; j++)
							{
								*output_row++ = input_row[codec->scaletable->input_x[j] * pixel_size] + 1;
								*output_row++ = input_row[codec->scaletable->input_x[j] * pixel_size] + 2;
								*output_row++ = input_row[codec->scaletable->input_x[j] * pixel_size] + 3;
							}
						}
						else
/* RGB to RGB */
						if(frame_depth == 24)
						{
							for(j = 0; j < file->out_w; j++)
							{
								*output_row++ = input_row[codec->scaletable->input_x[j] * pixel_size];
								*output_row++ = input_row[codec->scaletable->input_x[j] * pixel_size] + 1;
								*output_row++ = input_row[codec->scaletable->input_x[j] * pixel_size] + 2;
							}
						}
						break;

					case QUICKTIME_BGR8880:
/* ARGB to BGR0 */
						if(frame_depth == 32)
						{
							for(j = 0; j < file->out_w; j++)
							{
								*output_row++ = input_row[codec->scaletable->input_x[j] * pixel_size] + 3;
								*output_row++ = input_row[codec->scaletable->input_x[j] * pixel_size] + 2;
								*output_row++ = input_row[codec->scaletable->input_x[j] * pixel_size] + 1;
								*output_row++;
							}
						}
						else
/* RGB to BGR0 */
						if(frame_depth == 24)
						{
							for(j = 0; j < file->out_w; j++)
							{
								*output_row++ = input_row[codec->scaletable->input_x[j] * pixel_size] + 2;
								*output_row++ = input_row[codec->scaletable->input_x[j] * pixel_size] + 1;
								*output_row++ = input_row[codec->scaletable->input_x[j] * pixel_size];
							}
						}
						break;

					case QUICKTIME_RGBA8880:
/* ARGB to RGBA */
						if(frame_depth == 32)
						{
							for(j = 0; j < file->out_w; j++)
							{
								*output_row++ = input_row[codec->scaletable->input_x[j] * pixel_size] + 1;
								*output_row++ = input_row[codec->scaletable->input_x[j] * pixel_size] + 2;
								*output_row++ = input_row[codec->scaletable->input_x[j] * pixel_size] + 3;
								*output_row++ = input_row[codec->scaletable->input_x[j] * pixel_size];
							}
						}
						else
/* RGB to RGBA */
						if(frame_depth == 24)
						{
							for(j = 0; j < file->out_w; j++)
							{
								*output_row++ = input_row[codec->scaletable->input_x[j] * pixel_size];
								*output_row++ = input_row[codec->scaletable->input_x[j] * pixel_size] + 1;
								*output_row++ = input_row[codec->scaletable->input_x[j] * pixel_size] + 2;
								*output_row++ = 255;
							}
						}
						break;
				}
			}
			else
/* Copy columns directly */
			{
				switch(file->color_model)
				{
					case QUICKTIME_RGB888:
/* ARGB to RGB */
						if(frame_depth == 32)
						{
							input_row++;
							for(j = 0; j < file->out_w; j++)
							{
								*output_row++ = *input_row++;
								*output_row++ = *input_row++;
								*output_row++ = *input_row++;
								input_row++;
							}
						}
						else
/* RGB to RGB */
						if(frame_depth == 24)
						{
							memcpy(output_row, input_row, pixel_size * file->out_w);
						}
						break;

					case QUICKTIME_BGR8880:
/* ARGB to BGR0 */
						if(frame_depth == 32)
						{
							for(j = 0; j < file->out_w; j++)
							{
								*output_row++ = input_row[3];
								*output_row++ = input_row[2];
								*output_row++ = input_row[1];
								*output_row++;
								input_row += 4;
							}
						}
						else
/* RGB to BGR0 */
						if(frame_depth == 24)
						{
							for(j = 0; j < file->out_w; j++)
							{
								*output_row++ = input_row[2];
								*output_row++ = input_row[1];
								*output_row++ = input_row[0];
								input_row += 3;
							}
						}
						break;

					case QUICKTIME_RGBA8880:
/* ARGB to RGBA */
						if(frame_depth == 32)
						{
							for(j = 0; j < file->out_w; j++)
							{
								*output_row++ = input_row[1];
								*output_row++ = input_row[2];
								*output_row++ = input_row[3];
								*output_row++ = input_row[0];
								input_row += 4;
							}
						}
						else
/* RGB to RGBA */
						if(frame_depth == 24)
						{
							for(j = 0; j < file->out_w; j++)
							{
								*output_row++ = input_row[0];
								*output_row++ = input_row[1];
								*output_row++ = input_row[2];
								*output_row++ = 255;
								input_row += 3;
							}
						}
						break;
				}
			}
		}
	}

	return result;
}

static int quicktime_encode_raw(quicktime_t *file, unsigned char **row_pointers, int track)
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
/* Swap byte order to match Quicktime's ARGB. */
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
/* Rows are consecutive so write as a block */
			result = quicktime_write_data(file, row_pointers[0], bytes);
			if(result) result = 0; else result = 1;
		}
		else
		for(i = 0; i < height && !result; i++)
		{
/* Rows aren't consecutive but this should never happen. */
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
/* see if row_pointers are consecutive */
	for(i = 1, result = 1; i < h; i++)
	{
		if(row_pointers[i] - row_pointers[i - 1] != w * depth) result = 0;
	}
	return result;
}

static int quicktime_test_colormodel_raw(quicktime_t *file, 
		int colormodel, 
		int track)
{
	if(colormodel == QUICKTIME_RGB888 ||
		colormodel == QUICKTIME_BGR8880)
		return 1;

	return 0;
}

int quicktime_init_codec_raw(quicktime_video_map_t *vtrack)
{
	quicktime_raw_codec_t *priv;

	((quicktime_codec_t*)vtrack->codec)->priv = calloc(1, sizeof(quicktime_raw_codec_t));
	((quicktime_codec_t*)vtrack->codec)->delete_vcodec = quicktime_delete_codec_raw;
	((quicktime_codec_t*)vtrack->codec)->decode_video = quicktime_decode_raw;
	((quicktime_codec_t*)vtrack->codec)->encode_video = quicktime_encode_raw;
	((quicktime_codec_t*)vtrack->codec)->decode_audio = 0;
	((quicktime_codec_t*)vtrack->codec)->encode_audio = 0;
	((quicktime_codec_t*)vtrack->codec)->test_colormodel = quicktime_test_colormodel_raw;

	priv = ((quicktime_codec_t*)vtrack->codec)->priv;
	return 0;
}
