#include "config.h"
#include "mjpeg_types.h"

extern int width;
extern int height;
extern int bttv_hack;

void
sinc_interpolation (uint8_t * frame[3], int field)
{
	/*                                                        */
	/* 8-tap-cos^2-windowed-sinc-interpolation (integer)      */
    /*                                                        */
	/* This one assumes that the input signal is bandlimited. */
    /* This is not the case for high vertical contrasts, so   */
    /* the result of this filter alone (fast-mode) will be    */
    /* quite bad as it emphasizes the aliasing in such cases. */

	int x;
	int y;
	int v;
	uint8_t * fb=frame[0];
	uint8_t * fu=frame[1];
	uint8_t * fv=frame[2];

	// Luma 
	if(field==1)
	{
		frame[0] += width;
		frame[1] += width/2;
		frame[2] += width/2;
	}

	for (y = 0; y < height; y += 2)
	{
//		if( (y-7)>=0 && (y+7)<=height )
		for (x = 0; x < width; x++)
		{
			v  =	 -9 * *(frame[0] -7 * width );
			v +=	 21 * *(frame[0] -5 * width );
			v +=	-47 * *(frame[0] -3 * width );
			v +=	163 * *(frame[0] -1 * width );
			v +=	163 * *(frame[0] +1 * width );
			v +=	-47 * *(frame[0] +3 * width );
			v +=	 21 * *(frame[0] +5 * width );
			v +=	 -9 * *(frame[0] +7 * width );
			v >>= 8;

			/* Limit output to allowed range */
			v = v > 235 ? 235 : v;
			v = v < 16 ? 16 : v;
			*(frame[0]) = v;
			frame[0]++;
		}
		frame[0]+=width;
	}

	/* if the croma is interlaced, too (which is the case for  */
    /* every capture done not with BTTV420), deinterlace it by */
    /* linear interpolation, as resolution is bad anyways.     */

	if (bttv_hack == 0)
	{
		// Chroma 

		height /= 2;
		width /= 2;

		for (y = field; y < height; y += 2)
		{
			for (x = 0; x < width; x++)
			{
				v  =	 -9 * *(frame[1] - 7 * width );
				v +=	 21 * *(frame[1] - 5 * width );
				v +=	-47 * *(frame[1] - 3 * width );
				v +=	163 * *(frame[1] - 1 * width );
				v +=	163 * *(frame[1] + 1 * width );
				v +=	-47 * *(frame[1] + 3 * width );
				v +=	 21 * *(frame[1] + 5 * width );
				v +=	 -9 * *(frame[1] + 7 * width );
				v >>= 8;
	
				/* Limit output to allowed range */
				v = v > 240 ? 240 : v;
				v = v < 16 ? 16 : v;
				*(frame[1])++ = v;

				v  =	 -9 * *(frame[2] - 7 * width );
				v +=	 21 * *(frame[2] - 5 * width );
				v +=	-47 * *(frame[2] - 3 * width );
				v +=	163 * *(frame[2] - 1 * width );
				v +=	163 * *(frame[2] + 1 * width );
				v +=	-47 * *(frame[2] + 3 * width );
				v +=	 21 * *(frame[2] + 5 * width );
				v +=	 -9 * *(frame[2] + 7 * width );
				v >>= 8;
	
				/* Limit output to allowed range */
				v = v > 240 ? 240 : v;
				v = v < 16 ? 16 : v;
				*(frame[2])++ = v;
			}
			frame[1]+=width;
			frame[2]+=width;
		}
		height *= 2;
		width *= 2;
	}
	/* restore pointers */
	frame[0] = fb;
	frame[1] = fu;
	frame[2] = fv;
}
