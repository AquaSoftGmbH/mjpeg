#include "twos.h"

/* =================================== private for twos */

int twos_byte_order()
{                /* 1 if little endian */
	QUICKTIME_INT16 byteordertest;
	int byteorder;

	byteordertest = 0x0001;
	byteorder = *((unsigned char *)&byteordertest);
	return byteorder;
}

int twos_swap_bytes(char *buffer, long samples, int channels, int bits)
{
	long i;
	char byte1, byte2, byte3;
	char *buffer1, *buffer2, *buffer3;

	if(!twos_byte_order()) return 0;

	switch(bits)
	{
		case 8:
			break;

		case 16:
			buffer1 = buffer;
			buffer2 = buffer + 1;
			while(i < samples * 2)
			{
				byte1 = buffer2[i];
				buffer2[i] = buffer1[i];
				buffer1[i] = byte1;
				i += 2;
			}
			break;

		case 24:
			buffer1 = buffer;
			buffer2 = buffer + 2;
			while(i < samples * 3)
			{
				byte1 = buffer2[i];
				buffer2[i] = buffer1[i];
				buffer1[i] = byte1;
				i += 3;
			}
			break;

		default:
			break;
	}
	return 0;
}

int twos_get_work_buffer(quicktime_t *file, int track, long bytes)
{
	quicktime_twos_codec_t *codec = ((quicktime_codec_t*)file->atracks[track].codec)->priv;

	if(codec->work_buffer && codec->buffer_size != bytes)
	{
		free(codec->work_buffer);
		codec->work_buffer = 0;
	}
	
	if(!codec->work_buffer) 
	{
		codec->buffer_size = bytes;
		if(!(codec->work_buffer = malloc(bytes))) return 1;
	}
	return 0;
}

/* =================================== public for twos */

static int quicktime_delete_codec_twos(quicktime_audio_map_t *atrack)
{
	quicktime_twos_codec_t *codec = ((quicktime_codec_t*)atrack->codec)->priv;

	if(codec->work_buffer) free(codec->work_buffer);
	codec->work_buffer = 0;
	codec->buffer_size = 0;
	free(codec);
	return 0;
}

static int quicktime_decode_twos(quicktime_t *file, 
					QUICKTIME_INT16 *output_i, 
					float *output_f, 
					long samples, 
					int track, 
					int channel)
{
	int result = 0;
	long i, j;
	quicktime_twos_codec_t *codec = ((quicktime_codec_t*)file->atracks[track].codec)->priv;
	int step = file->atracks[track].channels * quicktime_audio_bits(file, track) / 8;

	twos_get_work_buffer(file, track, samples * step);
	result = quicktime_read_audio(file, codec->work_buffer, samples, track);
	result = !result;     /* Defeat fwrite's return */

	switch(quicktime_audio_bits(file, track))
	{
		case 8:
			if(output_i && !result)
			{
				for(i = 0, j = channel; i < samples; i++)
				{
					output_i[i] = (QUICKTIME_INT16)(codec->work_buffer[j]) << 8;
					j += step;
				}
			}
			else
			if(output_f && !result)
			{
				for(i = 0, j = channel; i < samples; i++)
				{
					output_f[i] = (float)(codec->work_buffer[j]) / 0x7f;
					j += step;
				}
			}
			break;
		
		case 16:
			if(output_i && !result)
			{
				for(i = 0, j = channel * 2; i < samples; i++)
				{
					output_i[i] = (QUICKTIME_INT16)(codec->work_buffer[j]) << 8 |
									(unsigned char)(codec->work_buffer[j + 1]);
					j += step;
				}
			}
			else
			if(output_f && !result)
			{
				for(i = 0, j = channel * 2; i < samples; i++)
				{
					output_f[i] = (float)((QUICKTIME_INT16)(codec->work_buffer[j]) << 8 |
									(unsigned char)(codec->work_buffer[j + 1])) / 0x7fff;
					j += step;
				}
			}
			break;
		
		case 24:
			if(output_i && !result)
			{
				for(i = 0, j = channel * 3; i < samples; i++)
				{
					output_i[i] = ((QUICKTIME_INT16)(codec->work_buffer[j]) << 8) | 
									(unsigned char)(codec->work_buffer[j + 1]);
					j += step;
				}
			}
			else
			if(output_f && !result)
			{
				for(i = 0, j = channel * 3; i < samples; i++)
				{
					output_f[i] = (float)(((int)(codec->work_buffer[j]) << 16) | 
									((unsigned int)(codec->work_buffer[j + 1]) << 8) |
									(unsigned char)(codec->work_buffer[j + 2])) / 0x7fffff;
					j += step;
				}
			}
			break;
		
		default:
			break;
	}

	return result;
}

static int quicktime_encode_twos(quicktime_t *file, 
							QUICKTIME_INT16 **input_i, 
							float **input_f, 
							int track, 
							long samples)
{
	int result = 0;
printf("quicktime_encode_twos: not implemented\n");
	return result;
}


int quicktime_init_codec_twos(quicktime_audio_map_t *atrack)
{
	quicktime_twos_codec_t *codec;

/* Init public items */
	((quicktime_codec_t*)atrack->codec)->priv = calloc(1, sizeof(quicktime_twos_codec_t));
	((quicktime_codec_t*)atrack->codec)->delete_acodec = quicktime_delete_codec_twos;
	((quicktime_codec_t*)atrack->codec)->decode_video = 0;
	((quicktime_codec_t*)atrack->codec)->encode_video = 0;
	((quicktime_codec_t*)atrack->codec)->decode_audio = quicktime_decode_twos;
	((quicktime_codec_t*)atrack->codec)->encode_audio = quicktime_encode_twos;

/* Init private items */
	codec = ((quicktime_codec_t*)atrack->codec)->priv;
	codec->work_buffer = 0;
	codec->buffer_size = 0;
	return 0;
}
