#include <string.h>
#include "config.h"
#include "mjpeg_types.h"
#include "transform_block.h"

void transform_block ( uint8_t * a1, uint8_t * a2, int rowstride)
{
	int y;

	for(y=0;y<8;y++)
	{	
		memcpy ( a1, a2, 8);
		a1 += rowstride;
		a2 += rowstride;
	}
}

