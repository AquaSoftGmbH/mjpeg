#include <png.h>
#include "quicktime.h"

int quicktime_init_codec_png(quicktime_video_map_t *vtrack)
{
	vtrack->codecs.png_codec.compression_level = 9;
}

int quicktime_delete_codec_png(quicktime_video_map_t *vtrack)
{
}

int quicktime_set_png(quicktime_t *file, int compression_level)
{
	int i;
	char *compressor;

	for(i = 0; i < file->total_vtracks; i++)
	{
		if(quicktime_match_32(quicktime_video_compressor(file, i), QUICKTIME_PNG))
		{
			quicktime_png_codec_t *codec = &(file->vtracks[i].codecs.png_codec);
			codec->compression_level = compression_level;
		}
	}
}

int quicktime_decode_png(quicktime_t *file, unsigned char **row_pointers, int track)
{
	int result = 0;
	long i, bytes;
	quicktime_trak_t *trak = file->vtracks[track].track;
	png_structp png_ptr;
	png_infop info_ptr;
	png_infop end_info = 0;	

//	if(!file->vtracks[track].frames_cached)
//	{
	quicktime_set_video_position(file, file->vtracks[track].current_position, track);
	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
	info_ptr = png_create_info_struct(png_ptr);
	png_init_io(png_ptr, quicktime_get_fd(file));	
	png_read_info(png_ptr, info_ptr);
//	}
// Forget cached frames
//	else
//	{
//	}

// read the image
	png_read_image(png_ptr, row_pointers);
	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
	return result;
}

int quicktime_encode_png(quicktime_t *file, unsigned char **row_pointers, int track)
{
	long offset = quicktime_position(file);
	int result = 0;
	int i;
	quicktime_trak_t *trak = file->vtracks[track].track;
	int height = trak->tkhd.track_height;
	int width = trak->tkhd.track_width;
	png_structp png_ptr;
	png_infop info_ptr;
	long bytes;
	quicktime_png_codec_t *codec = &(file->vtracks[track].codecs.png_codec);
	int depth = quicktime_video_depth(file, track);

	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
	info_ptr = png_create_info_struct(png_ptr);
	png_init_io(png_ptr, quicktime_get_fd(file));	
	png_set_compression_level(png_ptr, codec->compression_level);
	png_set_IHDR(png_ptr, info_ptr, width, height,
              8, depth == 24 ? PNG_COLOR_TYPE_RGB : PNG_COLOR_TYPE_RGB_ALPHA, 
			  PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	png_write_info(png_ptr, info_ptr);
	png_write_image(png_ptr, row_pointers);
	png_write_end(png_ptr, info_ptr);
	png_destroy_write_struct(&png_ptr, &info_ptr);

	bytes = quicktime_position(file) - offset;
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
