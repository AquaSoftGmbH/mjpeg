#include <string.h>
#include "config.h"
#include "mjpeg_types.h"
#include "transform_block.h"

void transform_block ( uint8_t * a1, uint8_t * a2, uint8_t * a3, int rowstride)
{
	int x,y;

	for(y=0;y<16;y++)
	{	
		for(x=0;x<16;x++)
		{
			*(a1) = (*(a2)+*(a3))/2 ;
			a1++;
			a2++;
			a3++;
		}
		/* process every second line */
		a1 += rowstride-16;
		a2 += rowstride-16;
		a3 += rowstride-16;
	}
}

