#include "config.h"
#include "mjpeg_types.h"
#include "copy_frame.h"
#include "blend_fields.h"
#include <stdio.h>

extern int width;
extern int height;

void
blend_fields_non_accel (uint8_t * dst[3], uint8_t * src[3] )
{
	int i;
	int offs = width*height;

	for (i = 0; i < offs; i++)
		{
			*(dst[0]) = ( *(src[0])+*(dst[0]) )/2;
			*(dst[1]) = ( *(src[1])+*(dst[1]) )/2;
			*(dst[2]) = ( *(src[2])+*(dst[2]) )/2;
			dst[0]++;
			dst[1]++;
			dst[2]++;
			src[0]++;
			src[1]++;
			src[2]++;
		}
	dst[0]  -= offs;
	src[0] -= offs;
	dst[1]  -= offs;
	src[1] -= offs;
	dst[2]  -= offs;
	src[2] -= offs;
}
