#include "config.h"
#include "mjpeg_types.h"

extern int width;
extern int height;

void transform_block_Y ( uint8_t * a1, uint8_t * a2, int rowstride)
{
	int x,y;

	for(y=0;y<32;y++)
	{	
		for(x=0;x<32;x++)
		{
			*(a1)=*(a2);
			a1++;
			a2++;
		}
	a1 -= 32;
	a2 -= 32;
	a1 += rowstride;
	a2 += rowstride;
	}
}

void transform_block_UV ( uint8_t * a1, uint8_t * a2, int rowstride)
{
	int x,y;

	for(y=0;y<16;y++)
	{	
		for(x=0;x<16;x++)
		{
			*(a1)=*(a2);
			a1++;
			a2++;
		}
	a1 -= 16;
	a2 -= 16;
	a1 += rowstride;
	a2 += rowstride;
	}
}
