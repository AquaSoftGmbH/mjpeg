#include "config.h"
#include "mjpeg_types.h"
#include "copy_frame.h"
#include "blend_fields.h"

extern int width;
extern int height;
extern int bttv_hack;

void
mux_fields (uint8_t * dst[3], uint8_t * src1[3], uint8_t * src2[3] )
{
	int x, y;

	for (y = 0; y < height; y+=2)
	{
		for (x = 0; x < width; x++)
		{
			*(dst[0]+x+y*width)=*(src1[0]+x+y*width);
			*(dst[0]+x+(y+1)*width)=
				(
					*(src2[0]+x+(y+1)*width)+
					*(src2[0]+x+(y  )*width)
				)/2;
		}
	}
}

void
blend_fields_non_accel (uint8_t * dst[3], uint8_t * src1[3], uint8_t * src2[3] )
{
	int x, y;
	int offs = width*height;

	for (y = 0; y < height; y++)
		for (x = 0; x < width; x++)
		{
			*(dst[0]) = ( *(src1[0]) + *(src2[0]) )>>1;
			dst[0]++;
			src1[0]++;
			src2[0]++;

			*(dst[1]) = ( *(src1[1]) + *(src2[1]) )>>1;
			dst[1]++;
			src1[1]++;
			src2[1]++;

			*(dst[2]) = ( *(src1[2]) + *(src2[2]) )>>1;
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

#if 0
	for (y = 0; y < h2; y++)
		for (x = 0; x < w2; x++)
		{
			*(frame6[1]) = ( *(frame5[1]) + *(frame20[1]) )>>1;
			frame5[1]++;
			frame6[1]++;
			frame20[1]++;

			*(frame6[2]) = ( *(frame5[2]) + *(frame20[2]) )>>1;
			frame5[2]++;
			frame6[2]++;
			frame20[2]++;
		}
	frame5[1] -= offs2;
	frame6[1] -= offs2;
	frame20[1] -= offs2;
	frame5[2] -= offs2;
	frame6[2] -= offs2;
	frame20[2] -= offs2;
#endif
}
