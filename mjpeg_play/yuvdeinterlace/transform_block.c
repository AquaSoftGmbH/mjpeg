#include <string.h>
#include "config.h"
#include "mjpeg_types.h"
#include "transform_block.h"

void transform_block ( uint8_t * a1, uint8_t * a2, int rowstride)
{
	int y;

	for(y=0;y<16;y++)
	{	
		memcpy ( a1, a2, 16);
		a1 += rowstride;
		a2 += rowstride;
	}
}

