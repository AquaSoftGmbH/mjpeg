#include "quicktime.h"

static int quicktime_delete_vcodec_stub(quicktime_video_map_t *vtrack)
{
	return 0;
}

static int quicktime_delete_acodec_stub(quicktime_audio_map_t *atrack)
{
	return 0;
}

static int quicktime_decode_video_stub(quicktime_t *file, 
				unsigned char **row_pointers, 
				int track)
{
	return 1;
}

static int quicktime_encode_video_stub(quicktime_t *file, 
				unsigned char **row_pointers, 
				int track)
{
	return 1;
}

static int quicktime_decode_audio_stub(quicktime_t *file, 
					QUICKTIME_INT16 *output_i, 
					float *output_f, 
					long samples, 
					int track, 
					int channel)
{
	return 1;
}

static int quicktime_encode_audio_stub(quicktime_t *file, 
				QUICKTIME_INT16 **input_i, 
				float **input_f, 
				int track, 
				long samples)
{
	return 1;
}


static int quicktime_test_colormodel_stub(quicktime_t *file, 
		int colormodel, 
		int track)
{
	return 0;
}

int quicktime_codec_defaults(quicktime_codec_t *codec)
{
	codec->delete_vcodec = quicktime_delete_vcodec_stub;
	codec->delete_acodec = quicktime_delete_acodec_stub;
	codec->decode_video = quicktime_decode_video_stub;
	codec->encode_video = quicktime_encode_video_stub;
	codec->decode_audio = quicktime_decode_audio_stub;
	codec->encode_audio = quicktime_encode_audio_stub;
	codec->test_colormodel = quicktime_test_colormodel_stub;
	return 0;
}

int quicktime_init_vcodec(quicktime_video_map_t *vtrack)
{
	char *compressor = vtrack->track->mdia.minf.stbl.stsd.table[0].format;
	vtrack->codec = calloc(1, sizeof(quicktime_codec_t));
	quicktime_codec_defaults((quicktime_codec_t*)vtrack->codec);

/* no table -> SEGV */
	if(quicktime_match_32(compressor, QUICKTIME_RAW))
		quicktime_init_codec_raw(vtrack);
	if(quicktime_match_32(compressor, QUICKTIME_DV))
		quicktime_init_codec_dv(vtrack);
	if(quicktime_match_32(compressor, QUICKTIME_JPEG))
		quicktime_init_codec_jpeg(vtrack, 0);
	if(quicktime_match_32(compressor, QUICKTIME_MJPA))
		quicktime_init_codec_jpeg(vtrack, 1);
	if(quicktime_match_32(compressor, QUICKTIME_PNG))
		quicktime_init_codec_png(vtrack);
	if(quicktime_match_32(compressor, QUICKTIME_YUV2))
		quicktime_init_codec_yuv2(vtrack);
	if(quicktime_match_32(compressor, QUICKTIME_YUV4))
		quicktime_init_codec_yuv4(vtrack);

	return 0;
}

int quicktime_init_acodec(quicktime_audio_map_t *atrack)
{
	char *compressor = atrack->track->mdia.minf.stbl.stsd.table[0].format;
	atrack->codec = calloc(1, sizeof(quicktime_codec_t));
	quicktime_codec_defaults((quicktime_codec_t*)atrack->codec);

/* no table -> SEGV */
	if(quicktime_match_32(compressor, QUICKTIME_TWOS))
		quicktime_init_codec_twos(atrack);
	if(quicktime_match_32(compressor, QUICKTIME_RAW))
		quicktime_init_codec_rawaudio(atrack);
	if(quicktime_match_32(compressor, QUICKTIME_IMA4))
		quicktime_init_codec_ima4(atrack);
	if(quicktime_match_32(compressor, QUICKTIME_ULAW))
		quicktime_init_codec_ulaw(atrack);

	return 0;
}


int quicktime_delete_vcodec(quicktime_video_map_t *vtrack)
{
	((quicktime_codec_t*)vtrack->codec)->delete_vcodec(vtrack);
	free(vtrack->codec);
	vtrack->codec = 0;
	return 0;
}

int quicktime_delete_acodec(quicktime_audio_map_t *atrack)
{
	((quicktime_codec_t*)atrack->codec)->delete_acodec(atrack);
	free(atrack->codec);
	atrack->codec = 0;
	return 0;
}

int quicktime_supported_video(quicktime_t *file, int track)
{
	char *compressor = quicktime_video_compressor(file, track);

/* Read only */
	if(file->rd && quicktime_match_32(compressor, QUICKTIME_DV)) return 1;

/* Read/Write */
	if(quicktime_match_32(compressor, QUICKTIME_RAW)) return 1;
	if(quicktime_match_32(compressor, QUICKTIME_JPEG)) return 1;
	if(quicktime_match_32(compressor, QUICKTIME_PNG)) return 1;
	if(quicktime_match_32(compressor, QUICKTIME_MJPA)) return 1;
	if(quicktime_match_32(compressor, QUICKTIME_YUV4)) return 1;
	if(quicktime_match_32(compressor, QUICKTIME_YUV2)) return 1;

	return 0;
}

int quicktime_supported_audio(quicktime_t *file, int track)
{
	char *compressor = quicktime_audio_compressor(file, track);
	if(quicktime_match_32(compressor, QUICKTIME_TWOS)) return 1;
	if(quicktime_match_32(compressor, QUICKTIME_RAW)) return 1;
	if(quicktime_match_32(compressor, QUICKTIME_IMA4)) return 1;
	if(quicktime_match_32(compressor, QUICKTIME_ULAW)) return 1;

	return 0;
}

int quicktime_decode_video(quicktime_t *file, unsigned char **row_pointers, int track)
{
	int result;
	quicktime_trak_t *trak = file->vtracks[track].track;
	int track_height = trak->tkhd.track_height;
	int track_width = trak->tkhd.track_width;

// Fake scaling parameters
	file->do_scaling = 0;
	file->color_model = QUICKTIME_RGB888;
	file->in_x = 0;
	file->in_y = 0;
	file->in_w = track_width;
	file->in_h = track_height;
	file->out_w = track_width;
	file->out_h = track_height;

	result = ((quicktime_codec_t*)file->vtracks[track].codec)->decode_video(file, row_pointers, track);
	file->vtracks[track].current_position++;
	return result;
}

long quicktime_decode_scaled(quicktime_t *file, 
	int in_x,                    /* Location of input frame to take picture */
	int in_y,
	int in_w,
	int in_h,
	int out_w,                   /* Dimensions of output frame */
	int out_h,
	int color_model,             /* One of the color models defined above */
	unsigned char **row_pointers, 
	int track)
{
	int result;

	file->do_scaling = 1;
	file->color_model = color_model;
	file->in_x = in_x;
	file->in_y = in_y;
	file->in_w = in_w;
	file->in_h = in_h;
	file->out_w = out_w;
	file->out_h = out_h;

	result = ((quicktime_codec_t*)file->vtracks[track].codec)->decode_video(file, row_pointers, track);
	file->vtracks[track].current_position++;
	return result;
}


int quicktime_encode_video(quicktime_t *file, unsigned char **row_pointers, int track)
{
	int result = ((quicktime_codec_t*)file->vtracks[track].codec)->encode_video(file, row_pointers, track);
	file->vtracks[track].current_position++;
	return result;
}

int quicktime_decode_audio(quicktime_t *file, 
				QUICKTIME_INT16 *output_i, 
				float *output_f, 
				long samples, 
				int channel)
{
	int quicktime_track, quicktime_channel;
	int result = 1;

	quicktime_channel_location(file, &quicktime_track, &quicktime_channel, channel);
	result = ((quicktime_codec_t*)file->atracks[quicktime_track].codec)->decode_audio(file, 
				output_i, 
				output_f, 
				samples, 
				quicktime_track, 
				quicktime_channel);
	file->atracks[quicktime_track].current_position += samples;

	return result;
}

/* Since all channels are written at the same time: */
/* Encode using the compressor for the first audio track. */
/* Which means all the audio channels must be on the same track. */

int quicktime_encode_audio(quicktime_t *file, QUICKTIME_INT16 **input_i, float **input_f, long samples)
{
	int result = 1;
	char *compressor = quicktime_audio_compressor(file, 0);
	
	if(quicktime_test_position(file)) return 1;

	result = ((quicktime_codec_t*)file->atracks[0].codec)->encode_audio(file, 
		input_i, 
		input_f,
		0, 
		samples);
	file->atracks[0].current_position += samples;

	return result;
}

int quicktime_test_colormodel(quicktime_t *file, 
		int colormodel, 
		int track)
{
	int result = ((quicktime_codec_t*)file->vtracks[track].codec)->test_colormodel(file, colormodel, track);
	return result;
}

long quicktime_samples_to_bytes(quicktime_trak_t *track, long samples)
{
	char *compressor = track->mdia.minf.stbl.stsd.table[0].format;
	int channels = track->mdia.minf.stbl.stsd.table[0].channels;

	if(quicktime_match_32(compressor, QUICKTIME_IMA4)) 
		return samples * channels;

	if(quicktime_match_32(compressor, QUICKTIME_ULAW)) 
		return samples * channels;

/* Default use the sample size specification for TWOS and RAW */
	return samples * channels * track->mdia.minf.stbl.stsd.table[0].sample_size / 8;
}

/* Compressors that can only encode a window at a time */
/* need to flush extra data here. */

int quicktime_flush_acodec(quicktime_t *file, int track)
{
	int result = 0;
	if(quicktime_match_32(quicktime_audio_compressor(file, track), QUICKTIME_IMA4))
	{
		result = quicktime_flush_ima4(file, track);
	}
	return result;
};

int quicktime_flush_vcodec(quicktime_t *file, int track)
{
}

int quicktime_codecs_flush(quicktime_t *file)
{
	int result = 0;
	int i;
	if(!file->wr) return result;

	if(file->total_atracks)
	{
		for(i = 0; i < file->total_atracks && !result; i++)
		{
			result += quicktime_flush_acodec(file, i);
		}
	}
	return result;
}
