#include "config.h"
#include "mjpeg_types.h"
#include "copy_frame.h"
#include "blend_fields.h"

extern int width;
extern int height;
extern int bttv_hack;

void
blend_fields_non_accel (uint8_t * dst[3], uint8_t * src1[3], uint8_t * src2[3] )
{
	int x, y;
	int offs = width*height;

	for (y = 0; y < height; y++)
		for (x = 0; x < width; x++)
		{
			*(dst[0]) = ( *(src1[0]) + *(src2[0]) +1 )>>1;
			dst[0]++;
			src1[0]++;
			src2[0]++;

			*(dst[1]) = ( *(src1[1]) + *(src2[1]) +1 )>>1;
			dst[1]++;
			src1[1]++;
			src2[1]++;

			*(dst[2]) = ( *(src1[2]) + *(src2[2]) +1 )>>1;
			dst[2]++;
			src1[2]++;
			src2[2]++;
		}
	dst[0]  -= offs;
	src1[0] -= offs;
	src2[0] -= offs;
	dst[1]  -= offs;
	src1[1] -= offs;
	src2[1] -= offs;
	dst[2]  -= offs;
	src1[2] -= offs;
	src2[2] -= offs;
}
