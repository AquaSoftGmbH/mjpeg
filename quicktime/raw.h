#ifndef QUICKTIME_RAW_H
#define QUICKTIME_RAW_H

#include "graphics.h"

typedef struct
{
	unsigned char *temp_frame;  /* For changing color models and scaling */
	unsigned char **temp_rows;  /* For changing color models and scaling */

	quicktime_scaletable_t *scaletable;
} quicktime_raw_codec_t;

#endif
