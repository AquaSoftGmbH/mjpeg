#include "config.h"
#include "mjpeg_types.h"
#include "copy_frame.h"
#include "blend_fields.h"

extern int width;
extern int height;
extern int bttv_hack;

extern uint8_t * frame1[3];
extern uint8_t * frame2[3];
extern uint8_t * frame3[3];
extern uint8_t * frame4[3];
extern uint8_t * inframe[3];
extern uint8_t * outframe[3];

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
			*(outframe[0]) = ( *(outframe[0]) + *(frame1[0]) )>>1;
			frame1[0]++;
			outframe[0]++;

			*(outframe[1]) = ( *(outframe[1]) + *(frame1[1]) )>>1;
			frame1[1]++;
			outframe[1]++;

			*(outframe[2]) = ( *(outframe[2]) + *(frame1[2]) )>>1;
			frame1[2]++;
			outframe[2]++;
		}
	frame1[0] -= offs1;
	outframe[0] -= offs1;
	frame1[1] -= offs1;
	outframe[1] -= offs1;
	frame1[2] -= offs1;
	outframe[2] -= offs1;

#if 0
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
#endif
}
