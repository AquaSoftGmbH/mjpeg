#include <string.h>
#include "config.h"
#include "mjpeg_types.h"
#include "copy_frame.h"

extern int width;
extern int height;

void copy_frame	( uint8_t * fd[3], uint8_t * fs[3])
{
			memcpy (fd[0], fs[0], width * height);
			memcpy (fd[1], fs[1], width * height / 4);
			memcpy (fd[2], fs[2], width * height / 4);
}

