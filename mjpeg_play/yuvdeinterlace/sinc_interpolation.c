#include "config.h"
#include "mjpeg_types.h"
#include "sinc_interpolation.h"
#include "motionsearch.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"

extern int width;
extern int height;

void
sinc_interpolation_luma (uint8_t * frame, uint8_t * inframe, int field)
{
	int x,y,v;

	memcpy ( frame, inframe, width*height );

	for (y = field; y < height; y += 2)
	{
		if(y>=7 && y<=(height-8))
		for (x = 0; x < width; x++ )
		{
			v  =	 -9 * *(inframe + x + (y-7) * width );
			v +=	 21 * *(inframe + x + (y-5) * width );
			v +=	-47 * *(inframe + x + (y-3) * width );
			v +=	163 * *(inframe + x + (y-1) * width );
			v +=	163 * *(inframe + x + (y+1) * width );
			v +=	-47 * *(inframe + x + (y+3) * width );
			v +=	 21 * *(inframe + x + (y+5) * width );
			v +=	 -9 * *(inframe + x + (y+7) * width );
			v >>= 8;

			/* Limit output to allowed range */
			v = v > 235 ? 235 : v;
			v = v < 16 ? 16 : v;
			*(frame+x+y*width) = v;
		}
		else
			if(y>=5 && y<=(height-6))
			for (x = 0; x < width; x++ )
			{
				v  =	 19 * *(inframe + x + (y-5) * width );
				v +=	-44 * *(inframe + x + (y-3) * width );
				v +=	153 * *(inframe + x + (y-1) * width );
				v +=	153 * *(inframe + x + (y+1) * width );
				v +=	-44 * *(inframe + x + (y+3) * width );
				v +=	 19 * *(inframe + x + (y+5) * width );
				v >>= 8;

				/* Limit output to allowed range */
				v = v > 235 ? 235 : v;
				v = v < 16 ? 16 : v;
				*(frame+x+y*width) = v;
			}
			else
				if(y>=3 && y<=(height-4))
				for (x = 0; x < width; x++ )
				{
					v  =	-51 * *(inframe + x + (y-3) * width );
					v +=	179 * *(inframe + x + (y-1) * width );
					v +=	179 * *(inframe + x + (y+1) * width );
					v +=	-51 * *(inframe + x + (y+3) * width );
					v >>= 8;

					/* Limit output to allowed range */
					v = v > 235 ? 235 : v;
					v = v < 16 ? 16 : v;
					*(frame+x+y*width) = v;
				}
				else
					for (x = 0; x < width; x++ )
					{
						v  = *(inframe + x + (y-1) * width );
						v += *(inframe + x + (y+1) * width );
						v >>= 1;
	
						/* Limit output to allowed range */
						v = v > 235 ? 235 : v;
						v = v < 16 ? 16 : v;
						*(frame+x+y*width) = v;
				}
	}
}

void
interpolation_420JPEG_to_444_chroma (uint8_t * frame, uint8_t * inframe, int field)
{
	uint8_t * s0 = inframe+field*(width/2);
	uint8_t * d0 = frame;
	uint8_t * d1 = frame+width;
	uint8_t * d2 = frame+width*2;
	uint8_t * d3 = frame+width*3;
	int offset1 = width*4;
	int offset2 = width;

	int x,y;
	for(y=0;y<height/4;y++)
	{
		for(x=0;x<width/2;x++)
		{
			*( d0+(x<<1)     ) = *(s0+x);
			*( d0+(x<<1) + 1 ) = *(s0+x);
			*( d1+(x<<1)     ) = *(s0+x);
			*( d1+(x<<1) + 1 ) = *(s0+x);
			*( d2+(x<<1)     ) = *(s0+x);
			*( d2+(x<<1) + 1 ) = *(s0+x);
			*( d3+(x<<1)     ) = *(s0+x);
			*( d3+(x<<1) + 1 ) = *(s0+x);
		}
		d0 += offset1;
		d1 += offset1;
		d2 += offset1;
		d3 += offset1;
		s0 += offset2;
	}
}

void
interpolation_420MPEG2_to_444_chroma (uint8_t * frame, uint8_t * inframe, int field)
{
	uint8_t * s0 = inframe+field*(width/2);
	uint8_t * d0 = frame;
	uint8_t * d1 = frame+width;
	uint8_t * d2 = frame+width*2;
	uint8_t * d3 = frame+width*3;
	int offset1 = width*4;
	int offset2 = width;

	int x,y;
	for(y=0;y<height/4;y++)
	{
		for(x=0;x<width/2;x++)
		{
			*( d0+(x<<1)     ) = *(s0+x);
			*( d0+(x<<1) + 1 ) = *(s0+x);
			*( d1+(x<<1)     ) = *(s0+x);
			*( d1+(x<<1) + 1 ) = *(s0+x);
			*( d2+(x<<1)     ) = *(s0+x);
			*( d2+(x<<1) + 1 ) = *(s0+x);
			*( d3+(x<<1)     ) = *(s0+x);
			*( d3+(x<<1) + 1 ) = *(s0+x);
		}
		d0 += offset1;
		d1 += offset1;
		d2 += offset1;
		d3 += offset1;
		s0 += offset2;
	}
}

void
interpolation_420PALDV_to_444_chroma (uint8_t * frame, uint8_t * inframe, int field)
{
	uint8_t * s0 = inframe+field*(width/2);
	uint8_t * d0 = frame;
	uint8_t * d1 = frame+width;
	uint8_t * d2 = frame+width*2;
	uint8_t * d3 = frame+width*3;
	int offset1 = width*4;
	int offset2 = width;

	int x,y;
	for(y=0;y<height/4;y++)
	{
		for(x=0;x<width/2;x++)
		{
			*( d0+(x<<1)     ) = *(s0+x);
			*( d0+(x<<1) + 1 ) = *(s0+x);
			*( d1+(x<<1)     ) = *(s0+x);
			*( d1+(x<<1) + 1 ) = *(s0+x);
			*( d2+(x<<1)     ) = *(s0+x);
			*( d2+(x<<1) + 1 ) = *(s0+x);
			*( d3+(x<<1)     ) = *(s0+x);
			*( d3+(x<<1) + 1 ) = *(s0+x);
		}
		d0 += offset1;
		d1 += offset1;
		d2 += offset1;
		d3 += offset1;
		s0 += offset2;
	}
}

void
interpolation_411_to_444_chroma (uint8_t * frame, uint8_t * inframe, int field)
{
	uint8_t * s0 = inframe+field*(width/4);
	uint8_t * d0 = frame;
	uint8_t * d1 = frame+width;
	int offset1 = width*2;
	int offset2 = width/2;

	int x,y;
	for(y=0;y<height/2;y++)
	{
		for(x=0;x<width/4;x++)
		{
			*( d0+(x<<2)     ) = *(s0+x);
			*( d0+(x<<2) + 1 ) = *(s0+x);
			*( d0+(x<<2) + 2 ) = *(s0+x);
			*( d0+(x<<2) + 3 ) = *(s0+x);
			*( d1+(x<<2)     ) = *(s0+x);
			*( d1+(x<<2) + 1 ) = *(s0+x);
			*( d1+(x<<2) + 2 ) = *(s0+x);
			*( d1+(x<<2) + 3 ) = *(s0+x);
		}
		d0 += offset1;
		d1 += offset1;
		s0 += offset2;
	}
}

void
interpolation_422_to_444_chroma (uint8_t * frame, uint8_t * inframe, int field)
{
	uint8_t * s0 = inframe+field*width/2;
	uint8_t * d0 = frame;
	uint8_t * d1 = frame+width;
	int offset1 = width*2;
	int offset2 = width;

	int x,y;
	for(y=0;y<height/2;y++)
	{
		for(x=0;x<width/2;x++)
		{
			*( d0+(x<<1) )   = *(s0+x);
			*( d1+(x<<1) )   = *(s0+x);
			*( d0+(x<<1) +1) = *(s0+x);
			*( d1+(x<<1) +1) = *(s0+x);
		}
		d0 += offset1;
		d1 += offset1;
		s0 += offset2;
	}
}

void C444_to_C420 (uint8_t * dst, uint8_t * src)
{
uint8_t * s = src;
uint8_t * s1= src+width;
uint8_t * d = dst;
int x;
int y;

	for(y=0;y<height;y+=2)
	{
		for(x=0;x<width;x+=2)
			{

			*(d) = (
							 *( x + s      ) +
							 *( x + s  + 1 ) +
							 *( x + s1     ) +
							 *( x + s1 + 1 ) + 2 
						   )>>2;
			d++;
			}
	s += width*2;
	s1+= width*2;
	}
}

void subsample ( uint8_t * dst , uint8_t * src)
{
uint8_t * s = src;
uint8_t * s1= src+width;
uint8_t * d = dst;
int x;
int y;

	for(y=0;y<height;y+=2)
	{
		for(x=0;x<width;x+=2)
			{

			*(d+(x>>1)) = (
							 *( x + s      ) +
							 *( x + s  + 1 ) +
							 *( x + s1     ) +
							 *( x + s1 + 1 ) + 2 
						   )>>2;

			}
	s += width*2;
	s1+= width*2;
	d += width;
	}
}
