#include "config.h"
#include "mjpeg_types.h"
#include "transform_block.h"

extern int width;
extern int height;

void transform_block ( uint8_t * a1, uint8_t * a2, int rowstride)
{
	int x,y;

	for(y=0;y<8;y++)
	{	
		for(x=0;x<8;x++)
		{
			*(a1)=*(a2);
			a1++;
			a2++;
		}
	a1 -= 8;
	a2 -= 8;
	a1 += rowstride;
	a2 += rowstride;
	}
}

void transform_block01 ( uint8_t * a1, uint8_t * a2, int rowstride)
{
	int x,y;
	uint8_t * a3 = a2+width;

	for(y=0;y<8;y++)
	{	
		for(x=0;x<8;x++)
		{
			*(a1)=(*(a2)+*(a3))>>1;
			a1++;
			a2++;
			a3++;
		}
	a1 -= 8;
	a2 -= 8;
	a3 -= 8;
	a1 += rowstride;
	a2 += rowstride;
	a3 += rowstride;
	}
}

void transform_block10 ( uint8_t * a1, uint8_t * a2, int rowstride)
{
	int x,y;
	uint8_t * a3 = a2+1;

	for(y=0;y<8;y++)
	{	
		for(x=0;x<8;x++)
		{
			*(a1)=(*(a2)+*(a3))>>1;
			a1++;
			a2++;
			a3++;
		}
	a1 -= 8;
	a2 -= 8;
	a3 -= 8;
	a1 += rowstride;
	a2 += rowstride;
	a3 += rowstride;
	}
}

void transform_block11 ( uint8_t * a1, uint8_t * a2, int rowstride)
{
	int x,y;
	uint8_t * a3 = a2+1+width;

	for(y=0;y<8;y++)
	{	
		for(x=0;x<8;x++)
		{
			*(a1)=(*(a2)+*(a3))>>1;
			a1++;
			a2++;
			a3++;
		}
	a1 -= 8;
	a2 -= 8;
	a3 -= 8;
	a1 += rowstride;
	a2 += rowstride;
	a3 += rowstride;
	}
}

void mark_block ( uint8_t * a1, int rowstride)
{
	int x,y;

	for(y=0;y<16;y++)
	{	
		for(x=0;x<16;x++)
		{
			*(a1)=64;
			a1++;
		}
	a1 -= 16;
	a1 += rowstride;
	}
}
