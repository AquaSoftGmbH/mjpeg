#ifndef QUICKTIME_DV_H
#define QUICKTIME_DV_H

#include "libdv.h"

typedef struct
{
	dv_t *dv;
	unsigned char *data;
	unsigned char *temp_frame, *temp_rows[576];
} quicktime_dv_codec_t;

#endif
