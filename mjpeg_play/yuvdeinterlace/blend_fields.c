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
blend_fields (void)
{
	int mode = 0;
	int x, y;
	int delta;
#if 0
	float korr1, korr2, korr3;
	static float mean_korr = 0.95;

	korr1 = 0;
	for (y = 0; y < height; y ++)
		for (x = 0; x < width; x++)
		{
			delta = *(frame20[0] + x + y * width) -
				*(frame5[0] + x + y * width) ;
			delta *= delta;
			korr1 += delta;
		}
	korr1 /= width * (height / 2);
	korr1 = 1 - sqrt (korr1) / 256;

	korr2 = 0;
	for (y = 0; y < height; y++)
		for (x = 0; x < width; x++)
		{
			delta = *(frame20[0] + x + y * width) -
				*(frame21[0] + x + y * width) ;
			delta *= delta;
			korr2 += delta;
		}
	korr2 /= width * (height / 2);
	korr2 = 1 - sqrt (korr2) / 256;

	korr3 = 0;
	for (y = 0; y < height; y ++)
		for (x = 0; x < width; x++)
		{
			delta = *(frame20[0] + x + y * width) -
				*(frame31[0] + x + y * width) ;
			delta *= delta;
			korr3 += delta;
		}
	korr3 /= width * (height / 2);
	korr3 = 1 - sqrt (korr3) / 256;

	if(korr1>=korr2 && korr1>=korr3 )
		mean_korr = mean_korr * 0.9 + 0.1 * korr1;
		else
			if(korr2>=korr1 && korr2>=korr3)
				mean_korr = mean_korr * 0.9 + 0.1 * korr2;
			else
				if(korr3>=korr1 && korr3>=korr2)
					mean_korr = mean_korr * 0.9 + 0.1 * korr3;
				else
					mean_korr = mean_korr * 0.9 + 0.1 * (korr1+korr2+korr3)/3;
					
		if (korr2 >= (korr1 * 0.995))
	{
		if(verbose==1)
		mjpeg_log (LOG_INFO,
			   " di:%3.0f(%3.0f) pr:%3.0f de:%3.0f -- mode: PROGRESSIVE",
			   korr1 * 100, mean_korr * 100, korr2 * 100,
			   korr3 * 100);
		mode = 1;
	}
	else if (korr3 >= (korr1 * 0.995))
	{
		if(verbose==1)
		mjpeg_log (LOG_INFO,
			   " di:%3.0f(%3.0f) pr:%3.0f de:%3.0f -- mode: TELECINED",
			   korr1 * 100, mean_korr * 100, korr2 * 100,
			   korr3 * 100);
		mode = 2;
	}
	else
	{
		mode = 0;

		if (korr1 < (mean_korr * 0.99))
		{
			mode = 3;
			if(verbose==1)
			mjpeg_log (LOG_INFO,
				   " di:%3.0f(%3.0f) pr:%3.0f de:%3.0f -- mode: ILACE (2-FAST)",
				   korr1 * 100, mean_korr * 100, korr2 * 100,
				   korr3 * 100);
		}
		else
		{
			if(verbose==1)
			mjpeg_log (LOG_INFO,
				   " di:%3.0f(%3.0f) pr:%3.0f de:%3.0f -- mode: ILACE (MC)",
				   korr1 * 100, mean_korr * 100, korr2 * 100,
				   korr3 * 100);
		}
	}
#endif
	/* process motion compensated fields (Interlaced Video M1) */
mode=0;
	if (mode == 0)
	{
		for (y = 0; y < height; y++)
			for (x = 0; x < width; x++)
			{
				*(frame6[0]) = ( *(frame5[0])>>1 ) + ( *(frame20[0])>>1 );
				frame5[0]++;
				frame6[0]++;
				frame20[0]++;
			}
		frame5[0] -= width*height;
		frame6[0] -= width*height;
		frame20[0] -= width*height;

		for (y = 0; y < height / 2; y++)
			for (x = 0; x < width / 2; x++)
			{
				*(frame6[1]) = ( *(frame5[1])>>1 ) + ( *(frame20[1])>>1 );
				frame5[1]++;
				frame6[1]++;
				frame20[1]++;

				*(frame6[2]) = ( *(frame5[2])>>1 ) + ( *(frame20[2])>>1 );
				frame5[2]++;
				frame6[2]++;
				frame20[2]++;
			}
		frame5[1] -= width*height/4;
		frame6[1] -= width*height/4;
		frame20[1] -= width*height/4;
		frame5[2] -= width*height/4;
		frame6[2] -= width*height/4;
		frame20[2] -= width*height/4;
	}
	else
		/* progressive scan or low motion processing */
	if (mode == 1)
	{
		for (y = 0; y < height; y++)
			for (x = 0; x < width; x++)
			{
				*(frame6[0] + x + y * width) =
					*(frame21[0] + x + y * width) * 0.5 +
					*(frame20[0] + x + y * width) * 0.5;
			}
		for (y = 0; y < height / 2; y++)
			for (x = 0; x < width / 2; x++)
			{
				*(frame6[1] + x + y * width / 2) =
					*(frame21[1] + x +
					  y * width / 2) * 0.5 +
					*(frame20[1] + x +
					  y * width / 2) * 0.5;

				*(frame6[2] + x + y * width / 2) =
					*(frame21[2] + x +
					  y * width / 2) * 0.5 +
					*(frame20[2] + x +
					  y * width / 2) * 0.5;
			}
	}
	else
		/* telecined material needs fieldswapping sometimes */
	if (mode == 2)
	{
		for (y = 0; y < height; y++)
			for (x = 0; x < width; x++)
			{
				*(frame6[0] + x + y * width) =
					*(frame31[0] + x + y * width) * 0.5 +
					*(frame20[0] + x + y * width) * 0.5;
			}
		for (y = 0; y < height / 2; y++)
			for (x = 0; x < width / 2; x++)
			{
				*(frame6[1] + x + y * width / 2) =
					*(frame31[1] + x +
					  y * width / 2) * 0.5 +
					*(frame20[1] + x +
					  y * width / 2) * 0.5;

				*(frame6[2] + x + y * width / 2) =
					*(frame31[2] + x +
					  y * width / 2) * 0.5 +
					*(frame20[2] + x +
					  y * width / 2) * 0.5;
			}
	}
	/* too fast movement to be corrected or scenechange */
	else if (mode == 3)
	{
		copy_frame ( frame6, frame20 );
	}
}

