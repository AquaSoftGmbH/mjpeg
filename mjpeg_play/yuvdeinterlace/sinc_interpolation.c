#include "config.h"
#include "mjpeg_types.h"
#include "sinc_interpolation.h"

extern int width;
extern int height;
extern int bttv_hack;

#if 0
void
sinc_interpolation_420JPEG_to_444 (uint8_t * frame[3], int field)
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
#endif

void
sinc_interpolation_420JPEG_to_444 (uint8_t * frame[3], uint8_t * inframe[3], int field)
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

	/* realign chroma pixels */
	for(y=0;y<(height/2);y++)
		for(x=0;x<(width/2);x++)
		{
			*(frame[1]+x*2+y*2*width) = *(inframe[1]+x+y*width/2);
			*(frame[2]+x*2+y*2*width) = *(inframe[2]+x+y*width/2);
		}

	/* interpolate missing horizontal chroma pixels */
	for (y = field*2; y < height; y += 4)
	{
		for (x = 1; x < width; x+= 2)
		{
			v  =	 -9 * *(frame[1] + (x-7) + y * width );
			v +=	 21 * *(frame[1] + (x-5) + y * width );
			v +=	-47 * *(frame[1] + (x-3) + y * width );
			v +=	163 * *(frame[1] + (x-1) + y * width );
			v +=	163 * *(frame[1] + (x+1) + y * width );
			v +=	-47 * *(frame[1] + (x+3) + y * width );
			v +=	 21 * *(frame[1] + (x+5) + y * width );
			v +=	 -9 * *(frame[1] + (x+7) + y * width );
			v >>= 8;

			/* Limit output to allowed range */
			v = v > 235 ? 235 : v;
			v = v < 16 ? 16 : v;
			*(frame[1]+x+y*width) = v;

			v  =	 -9 * *(frame[2] + (x-7) + y * width );
			v +=	 21 * *(frame[2] + (x-5) + y * width );
			v +=	-47 * *(frame[2] + (x-3) + y * width );
			v +=	163 * *(frame[2] + (x-1) + y * width );
			v +=	163 * *(frame[2] + (x+1) + y * width );
			v +=	-47 * *(frame[2] + (x+3) + y * width );
			v +=	 21 * *(frame[2] + (x+5) + y * width );
			v +=	 -9 * *(frame[2] + (x+7) + y * width );
			v >>= 8;

			/* Limit output to allowed range */
			v = v > 235 ? 235 : v;
			v = v < 16 ? 16 : v;
			*(frame[2]+x+y*width) = v;
		}
	}
	/* interpolate missing vertical chroma pixels (1st Stage) */
	for (y =(field*2+2); y < height; y += 4)
	{
		for (x = 0; x < width; x++ )
		{
			v  =	 -9 * *(frame[1] + x + (y-14) * width );
			v +=	 21 * *(frame[1] + x + (y-10) * width );
			v +=	-47 * *(frame[1] + x + (y -6) * width );
			v +=	163 * *(frame[1] + x + (y -2) * width );
			v +=	163 * *(frame[1] + x + (y +2) * width );
			v +=	-47 * *(frame[1] + x + (y +6) * width );
			v +=	 21 * *(frame[1] + x + (y+10) * width );
			v +=	 -9 * *(frame[1] + x + (y+14) * width );
			v >>= 8;

			/* Limit output to allowed range */
			v = v > 235 ? 235 : v;
			v = v < 16 ? 16 : v;
			*(frame[1]+x+y*width) = v;

			v  =	 -9 * *(frame[2] + x + (y-14) * width );
			v +=	 21 * *(frame[2] + x + (y-10) * width );
			v +=	-47 * *(frame[2] + x + (y -6) * width );
			v +=	163 * *(frame[2] + x + (y -2) * width );
			v +=	163 * *(frame[2] + x + (y +2) * width );
			v +=	-47 * *(frame[2] + x + (y +6) * width );
			v +=	 21 * *(frame[2] + x + (y+10) * width );
			v +=	 -9 * *(frame[2] + x + (y+14) * width );
			v >>= 8;

			/* Limit output to allowed range */
			v = v > 235 ? 235 : v;
			v = v < 16 ? 16 : v;
			*(frame[2]+x+y*width) = v;
		}
	}
	/* interpolate missing vertical chroma pixels (2nd Stage) */
	for (y = 1; y < height; y += 2)
	{
		for (x = 0; x < width; x++ )
		{
			v  =	 -9 * *(frame[1] + x + (y-7) * width );
			v +=	 21 * *(frame[1] + x + (y-5) * width );
			v +=	-47 * *(frame[1] + x + (y-3) * width );
			v +=	163 * *(frame[1] + x + (y-1) * width );
			v +=	163 * *(frame[1] + x + (y+1) * width );
			v +=	-47 * *(frame[1] + x + (y+3) * width );
			v +=	 21 * *(frame[1] + x + (y+5) * width );
			v +=	 -9 * *(frame[1] + x + (y+7) * width );
			v >>= 8;

			/* Limit output to allowed range */
			v = v > 235 ? 235 : v;
			v = v < 16 ? 16 : v;
			*(frame[1]+x+y*width) = v;

			v  =	 -9 * *(frame[2] + x + (y-7) * width );
			v +=	 21 * *(frame[2] + x + (y-5) * width );
			v +=	-47 * *(frame[2] + x + (y-3) * width );
			v +=	163 * *(frame[2] + x + (y-1) * width );
			v +=	163 * *(frame[2] + x + (y+1) * width );
			v +=	-47 * *(frame[2] + x + (y+3) * width );
			v +=	 21 * *(frame[2] + x + (y+5) * width );
			v +=	 -9 * *(frame[2] + x + (y+7) * width );
			v >>= 8;

			/* Limit output to allowed range */
			v = v > 235 ? 235 : v;
			v = v < 16 ? 16 : v;
			*(frame[2]+x+y*width) = v;
		}
	}

	/* interpolate missing vertical luma pixels */
	for (y = field+1; y < height; y += 2)
	{
		for (x = 0; x < width; x++ )
		{
			v  =	 -9 * *(inframe[0] + x + (y-7) * width );
			v +=	 21 * *(inframe[0] + x + (y-5) * width );
			v +=	-47 * *(inframe[0] + x + (y-3) * width );
			v +=	163 * *(inframe[0] + x + (y-1) * width );
			v +=	163 * *(inframe[0] + x + (y+1) * width );
			v +=	-47 * *(inframe[0] + x + (y+3) * width );
			v +=	 21 * *(inframe[0] + x + (y+5) * width );
			v +=	 -9 * *(inframe[0] + x + (y+7) * width );
			v >>= 8;

			/* Limit output to allowed range */
			v = v > 235 ? 235 : v;
			v = v < 16 ? 16 : v;
			*(frame[0]+x+y*width) = v;
			*(frame[0]+x+(y+1)*width) = *(inframe[0]+x+(y+1)*width);
		}
	}

}
