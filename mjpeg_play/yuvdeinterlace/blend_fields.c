#include "config.h"
#include "mjpeg_types.h"
#include "copy_frame.h"

extern int width;
extern int height;
extern int bttv_hack;

extern uint8_t * frame5[3];
extern uint8_t * frame6[3];
extern uint8_t * frame20[3];
extern uint8_t * frame21[3];
extern uint8_t * frame31[3];

void
blend_fields_non_accel (void)
{
	int w2 = width/2;
	int h2 = height/2;
	int x, y;
	int offs1 = width*height;
	int offs2 = offs1/4;

	for (y = 0; y < height; y++)
		for (x = 0; x < width; x++)
		{
			*(frame6[0]) = ( *(frame5[0]) + *(frame20[0]) )>>1;
			frame5[0]++;
			frame6[0]++;
			frame20[0]++;
		}
	frame5[0] -= offs1;
	frame6[0] -= offs1;
	frame20[0] -= offs1;

	for (y = 0; y < h2; y++)
		for (x = 0; x < w2; x++)
		{
			*(frame6[1]) = ( *(frame5[1]) + *(frame20[1]) )>>1;
			frame5[1]++;
			frame6[1]++;
			frame20[1]++;

			*(frame6[2]) = ( *(frame5[2]) + *(frame20[2]) )>>1;
			frame5[2]++;
			frame6[2]++;
			frame20[2]++;
		}
	frame5[1] -= offs2;
	frame6[1] -= offs2;
	frame20[1] -= offs2;
	frame5[2] -= offs2;
	frame6[2] -= offs2;
	frame20[2] -= offs2;
}


