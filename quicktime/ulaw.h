#ifndef QUICKTIME_ULAW_H
#define QUICKTIME_ULAW_H

#include "quicktime.h"

typedef struct
{
	float *ulawtofloat_table;
	float *ulawtofloat_ptr;
	QUICKTIME_INT16 *ulawtoint16_table;
	QUICKTIME_INT16 *ulawtoint16_ptr;
	unsigned char *int16toulaw_table;
	unsigned char *int16toulaw_ptr;
	unsigned char *read_buffer;
	long read_size;
} quicktime_ulaw_codec_t;


#endif
