#include "config.h"
#include "mjpeg_types.h"
#include "interleave.h"
#include "copy_frame.h"

extern int width;
extern int height;
extern int bttv_hack;

void interleave_fields (int w, int h, uint8_t * frame[3], uint8_t * fbuff[3])
{
	int x,y;
	int l1,l2;
	int w2=w/2;
	int h2=h/2;

	for(y=0;y<h2;y++)
	for(x=0;x<w;x++)
	{
		l1 = *(frame[0]+x+y*w);
		l2 = *(frame[0]+x+y*w+h2*w);

		*(fbuff[0]+x+(y*2  )*w)=l1;
		*(fbuff[0]+x+(y*2+1)*w)=l2;
	}	

	w  /= 2;
	h  /= 2;
	w2 /= 2;
	h2 /= 2;

	for(y=0;y<h2;y++)
	for(x=0;x<w;x++)
	{
		l1 = *(frame[1]+x+y*w);
		l2 = *(frame[1]+x+y*w+h2*w);

		*(fbuff[1]+x+(y*2  )*w)=l1;
		*(fbuff[1]+x+(y*2+1)*w)=l2;

		l1 = *(frame[2]+x+y*w);
		l2 = *(frame[2]+x+y*w+h2*w);

		*(fbuff[2]+x+(y*2  )*w)=l1;
		*(fbuff[2]+x+(y*2+1)*w)=l2;
	}	

	w *= 2;
	h *= 2;

	copy_frame (frame, fbuff);
}
