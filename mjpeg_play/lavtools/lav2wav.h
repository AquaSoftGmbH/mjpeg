#ifndef LAV2WAV_H
#define LAV2WAV_H

#define WAVE_FORMAT_PCM  0x0001
#define FORMAT_MULAW     0x0101
#define IBM_FORMAT_ALAW  0x0102
#define IBM_FORMAT_ADPCM 0x0103


struct riff_struct 
{
  uint8_t id[4];   /* RIFF */
  uint32_t len;
  uint8_t wave_id[4]; /* WAVE */
};


struct chunk_struct 
{
	uint8_t id[4];
	uint32_t len;
};

struct common_struct 
{
	uint16_t wFormatTag;
	uint16_t wChannels;
	uint32_t dwSamplesPerSec;
	uint32_t dwAvgBytesPerSec;
	uint16_t wBlockAlign;
	uint16_t wBitsPerSample;  /* Only for PCM */
};

struct wave_header 
{
	struct riff_struct   riff;
	struct chunk_struct  format;
	struct common_struct common;
	struct chunk_struct  data;
};
#endif /* LAV2WAV_H */
