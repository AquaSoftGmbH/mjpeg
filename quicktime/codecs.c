#include "quicktime.h"

int quicktime_init_vcodecs(quicktime_video_map_t *vtrack)
{
	char *compressor = vtrack->track->mdia.minf.stbl.stsd.table[0].format;
// no table -> SEGV
	if(quicktime_match_32(compressor, QUICKTIME_RAW))
		quicktime_init_codec_raw(vtrack);
	if(quicktime_match_32(compressor, QUICKTIME_DV))
		quicktime_init_codec_dv(vtrack);
	if(quicktime_match_32(compressor, QUICKTIME_JPEG))
		quicktime_init_codec_jpeg(vtrack, 0);
	if(quicktime_match_32(compressor, QUICKTIME_MJPA))
		quicktime_init_codec_jpeg(vtrack, 1);
// 	if(quicktime_match_32(compressor, QUICKTIME_MJPB))
// 		quicktime_init_codec_jpeg(vtrack, 2);
	if(quicktime_match_32(compressor, QUICKTIME_PNG))
		quicktime_init_codec_png(vtrack);
	if(quicktime_match_32(compressor, QUICKTIME_YUV2))
		quicktime_init_codec_yuv2(vtrack);
	if(quicktime_match_32(compressor, QUICKTIME_YUV4))
		quicktime_init_codec_yuv4(vtrack);
	if(quicktime_match_32(compressor, QUICKTIME_WMX1))
		quicktime_init_codec_wmx1(vtrack);

	return 0;
}

int quicktime_init_acodecs(quicktime_audio_map_t *atrack)
{
	char *compressor = atrack->track->mdia.minf.stbl.stsd.table[0].format;
// no table -> SEGV
	if(quicktime_match_32(compressor, QUICKTIME_TWOS))
		quicktime_init_codec_twos(atrack);
	if(quicktime_match_32(compressor, QUICKTIME_RAW))
		quicktime_init_codec_rawaudio(atrack);
	if(quicktime_match_32(compressor, QUICKTIME_IMA4))
		quicktime_init_codec_ima4(atrack);
	if(quicktime_match_32(compressor, QUICKTIME_ULAW))
		quicktime_init_codec_ulaw(atrack);
	if(quicktime_match_32(compressor, QUICKTIME_WMX2))
		quicktime_init_codec_wmx2(atrack);

	return 0;
}


int quicktime_delete_vcodecs(quicktime_video_map_t *vtrack)
{
	char *compressor = vtrack->track->mdia.minf.stbl.stsd.table[0].format;
	if(quicktime_match_32(compressor, QUICKTIME_RAW))
		quicktime_delete_codec_raw(vtrack);
	if(quicktime_match_32(compressor, QUICKTIME_DV))
		quicktime_delete_codec_dv(vtrack);
	if(quicktime_match_32(compressor, QUICKTIME_JPEG))
		quicktime_delete_codec_jpeg(vtrack);
	if(quicktime_match_32(compressor, QUICKTIME_MJPA))
		quicktime_delete_codec_jpeg(vtrack);
// 	if(quicktime_match_32(compressor, QUICKTIME_MJPB))
// 		quicktime_delete_codec_jpeg(vtrack);
	if(quicktime_match_32(compressor, QUICKTIME_PNG))
		quicktime_delete_codec_png(vtrack);
	if(quicktime_match_32(compressor, QUICKTIME_YUV2))
		quicktime_delete_codec_yuv2(vtrack);
	if(quicktime_match_32(compressor, QUICKTIME_YUV4))
		quicktime_delete_codec_yuv4(vtrack);
	if(quicktime_match_32(compressor, QUICKTIME_WMX1))
		quicktime_delete_codec_wmx1(vtrack);

	return 0;
}

int quicktime_delete_acodecs(quicktime_audio_map_t *atrack)
{
	char *compressor = atrack->track->mdia.minf.stbl.stsd.table[0].format;
	if(quicktime_match_32(compressor, QUICKTIME_TWOS))
		quicktime_delete_codec_twos(atrack);
	if(quicktime_match_32(compressor, QUICKTIME_RAW))
		quicktime_delete_codec_rawaudio(atrack);
	if(quicktime_match_32(compressor, QUICKTIME_IMA4))
		quicktime_delete_codec_ima4(atrack);
	if(quicktime_match_32(compressor, QUICKTIME_WMX2))
		quicktime_delete_codec_wmx2(atrack);
	if(quicktime_match_32(compressor, QUICKTIME_ULAW))
		quicktime_delete_codec_ulaw(atrack);

	return 0;
}

int quicktime_supported_video(quicktime_t *file, int track)
{
	char *compressor = quicktime_video_compressor(file, track);

	if(quicktime_match_32(compressor, QUICKTIME_DV)) return 1;
	if(quicktime_match_32(compressor, QUICKTIME_RAW)) return 1;
	if(quicktime_match_32(compressor, QUICKTIME_JPEG)) return 1;
	if(quicktime_match_32(compressor, QUICKTIME_PNG)) return 1;
	if(quicktime_match_32(compressor, QUICKTIME_MJPA)) return 1;
//	if(quicktime_match_32(compressor, QUICKTIME_MJPB)) return 1;
	if(quicktime_match_32(compressor, QUICKTIME_YUV4)) return 1;
	if(quicktime_match_32(compressor, QUICKTIME_YUV2)) return 1;
	if(quicktime_match_32(compressor, QUICKTIME_WMX1)) return 1;

	return 0;
}

int quicktime_supported_audio(quicktime_t *file, int track)
{
	char *compressor = quicktime_audio_compressor(file, track);
	if(quicktime_match_32(compressor, QUICKTIME_TWOS)) return 1;
	if(quicktime_match_32(compressor, QUICKTIME_RAW)) return 1;
	if(quicktime_match_32(compressor, QUICKTIME_IMA4)) return 1;
	if(quicktime_match_32(compressor, QUICKTIME_WMX2)) return 1;
	if(quicktime_match_32(compressor, QUICKTIME_ULAW)) return 1;
	
	return 0;
}

int quicktime_decode_video(quicktime_t *file, unsigned char **row_pointers, int track)
{
	char *compressor = quicktime_video_compressor(file, track);
	int result = 0;

// test ram cache boundary
	if(file->vtracks[track].frames_cached &&
		file->vtracks[track].current_position >= file->vtracks[track].frames_cached) result = 1;

	if(!result)
	{
		if(quicktime_match_32(compressor, QUICKTIME_DV))
			result = quicktime_decode_dv(file, row_pointers, track);
		else
		if(quicktime_match_32(compressor, QUICKTIME_RAW))
			result = quicktime_decode_raw(file, row_pointers, track);
		else
		if(quicktime_match_32(compressor, QUICKTIME_JPEG))
			result = quicktime_decode_jpeg(file, row_pointers, track);
		else
		if(quicktime_match_32(compressor, QUICKTIME_MJPA))
			result = quicktime_decode_jpeg(file, row_pointers, track);
		else
// 		if(quicktime_match_32(compressor, QUICKTIME_MJPB))
// 			result = quicktime_decode_jpeg(file, row_pointers, track);
// 		else
		if(quicktime_match_32(compressor, QUICKTIME_PNG))
			result = quicktime_decode_png(file, row_pointers, track);
		else
		if(quicktime_match_32(compressor, QUICKTIME_YUV4))
			result = quicktime_decode_yuv4(file, row_pointers, track);
		else
		if(quicktime_match_32(compressor, QUICKTIME_YUV2))
			result = quicktime_decode_yuv2(file, row_pointers, track);
		else
		if(quicktime_match_32(compressor, QUICKTIME_WMX1))
			result = quicktime_decode_wmx1(file, row_pointers, track);
		else
			result = 1;
	}

	file->vtracks[track].current_position++;

	return result;
}

int quicktime_encode_video(quicktime_t *file, unsigned char **row_pointers, int track)
{
	char *compressor = quicktime_video_compressor(file, track);
	int result = 0;

// Defeat 32 bit file size limit.
	if(quicktime_test_position(file)) return 1;

	if(quicktime_match_32(compressor, QUICKTIME_DV))
		result = quicktime_encode_dv(file, row_pointers, track);
	else
	if(quicktime_match_32(compressor, QUICKTIME_RAW))
		result = quicktime_encode_raw(file, row_pointers, track);
	else
	if(quicktime_match_32(compressor, QUICKTIME_JPEG))
		result = quicktime_encode_jpeg(file, row_pointers, track);
	else
	if(quicktime_match_32(compressor, QUICKTIME_MJPA))
		result = quicktime_encode_jpeg(file, row_pointers, track);
	else
// 	if(quicktime_match_32(compressor, QUICKTIME_MJPB))
// 		result = quicktime_encode_jpeg(file, row_pointers, track);
// 	else
	if(quicktime_match_32(compressor, QUICKTIME_PNG))
		result = quicktime_encode_png(file, row_pointers, track);
	else
	if(quicktime_match_32(compressor, QUICKTIME_YUV4))
		result = quicktime_encode_yuv4(file, row_pointers, track);
	else
	if(quicktime_match_32(compressor, QUICKTIME_YUV2))
		result = quicktime_encode_yuv2(file, row_pointers, track);
	else
	if(quicktime_match_32(compressor, QUICKTIME_WMX1))
		result = quicktime_encode_wmx1(file, row_pointers, track);
	else
		result = 1;

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
	char *compressor;
	int result = 1;

	quicktime_channel_location(file, &quicktime_track, &quicktime_channel, channel);
	compressor = quicktime_audio_compressor(file, quicktime_track);

	if(quicktime_match_32(compressor, QUICKTIME_TWOS))
		result = quicktime_decode_twos(file, output_i, output_f, samples, quicktime_track, quicktime_channel);
	else
	if(quicktime_match_32(compressor, QUICKTIME_RAW))
		result = quicktime_decode_rawaudio(file, output_i, output_f, samples, quicktime_track, quicktime_channel);
	else
	if(quicktime_match_32(compressor, QUICKTIME_IMA4))
		result = quicktime_decode_ima4(file, output_i, output_f, samples, quicktime_track, quicktime_channel);
	else
	if(quicktime_match_32(compressor, QUICKTIME_WMX2))
		result = quicktime_decode_wmx2(file, output_i, output_f, samples, quicktime_track, quicktime_channel);
	else
	if(quicktime_match_32(compressor, QUICKTIME_ULAW))
		result = quicktime_decode_ulaw(file, output_i, output_f, samples, quicktime_track, quicktime_channel);
	else
		result = 1;

	file->atracks[quicktime_track].current_position += samples;

	return result;
}

// Since all channels are written at the same time:
// Encode using the compressor for the first audio track.
// Which means all the audio channels must be on the same track.

int quicktime_encode_audio(quicktime_t *file, QUICKTIME_INT16 **input_i, float **input_f, long samples)
{
	int result = 1;
	char *compressor = quicktime_audio_compressor(file, 0);
	
	if(quicktime_test_position(file)) return 1;

	if(quicktime_match_32(compressor, QUICKTIME_TWOS))
		result = quicktime_encode_twos(file, input_i, input_f, 0, samples, 0);
	else
	if(quicktime_match_32(compressor, QUICKTIME_RAW))
		result = quicktime_encode_rawaudio(file, input_i, input_f, 0, samples, 0);
	else
	if(quicktime_match_32(compressor, QUICKTIME_IMA4))
		result = quicktime_encode_ima4(file, input_i, input_f, 0, samples, 0);
	else
	if(quicktime_match_32(compressor, QUICKTIME_WMX2))
		result = quicktime_encode_wmx2(file, input_i, input_f, 0, samples, 0);
	else
	if(quicktime_match_32(compressor, QUICKTIME_ULAW))
		result = quicktime_encode_ulaw(file, input_i, input_f, 0, samples, 0);
	else
		result = 1;

	file->atracks[0].current_position += samples;

	return result;
}

long quicktime_samples_to_bytes(quicktime_trak_t *track, long samples)
{
	char *compressor = track->mdia.minf.stbl.stsd.table[0].format;
	int channels = track->mdia.minf.stbl.stsd.table[0].channels;

	if(quicktime_match_32(compressor, QUICKTIME_IMA4)) 
		return samples * channels;

	if(quicktime_match_32(compressor, QUICKTIME_WMX2)) 
		return samples * channels;

	if(quicktime_match_32(compressor, QUICKTIME_ULAW)) 
		return samples * channels;

// Default use the sample size specification for TWOS and RAW
	return samples * channels * track->mdia.minf.stbl.stsd.table[0].sample_size / 8;
}

// Compressors that can only encode a window at a time
// need to flush extra data here.

int quicktime_flush_acodec(quicktime_t *file, int track)
{
	int result = 0;
	if(quicktime_match_32(quicktime_audio_compressor(file, track), QUICKTIME_IMA4))
	{
		result = quicktime_flush_ima4(file, track);
	}
	return result;
}

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
