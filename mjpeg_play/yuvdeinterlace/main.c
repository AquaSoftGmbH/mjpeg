/***********************************************************
 * YUVDEINTERLACER for the mjpegtools                      *
 * ------------------------------------------------------- *
 * (C) 2001,2002 Stefan Fendt                              *
 *                                                         *
 * Licensed and protected by the GNU-General-Public-       *
 * License version 2 or if you prefer any later version of *
 * that license). See the file LICENSE for detailed infor- *
 * mation.                                                 *
 ***********************************************************/

/* dedicated to my grand-pa */

/* PLANS: I would like to be able to fill any kind of yuv4mpeg-format into 
 *        the deinterlacer. Mpeg2enc (all others, too??) doesn't seem to
 *        like interlaced material. Progressive frames compress much
 *        better... Despite that pogressive frames do look so much better
 *        even when viewed on a TV-Screen, that I really don't like to 
 *        deal with that weird 1920's interlacing sh*t...
 *
 *        I would like to support 4:2:2 PAL/NTSC recordings as well as 
 *        4:1:1 PAL/NTSC recordings to come out as correct 4:2:0 progressive-
 *        frames. I expect to get better colors and a better motion-search
 *        result, when using these two as the deinterlacer's input...
 *
 *        When I really get this working reliably (what I hope as I now own
 *        a digital PAL-Camcorder, but interlaced (progressive was to expen-
 *        sive *sic*)), I hope that I can get rid of that ugly interlacing
 *        crap in the denoiser's core. 
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include "mjpeg_types.h"
#include "yuv4mpeg.h"
#include "mjpeg_logging.h"
#include "motionsearch.h"

int fast_mode=0;
int field_order = 1;
int bttv_hack = 0;
int width = 0;
int height = 0;

uint8_t f10y[1024 * 768];
uint8_t f10cr[512 * 384];
uint8_t f10cb[512 * 384];
uint8_t *frame10[3] = { f10y, f10cr, f10cb };

uint8_t f11y[1024 * 768];
uint8_t f11cr[512 * 384];
uint8_t f11cb[512 * 384];
uint8_t *frame11[3] = { f11y, f11cr, f11cb };

uint8_t f20y[1024 * 768];
uint8_t f20cr[512 * 384];
uint8_t f20cb[512 * 384];
uint8_t *frame20[3] = { f20y, f20cr, f20cb };

uint8_t f21y[1024 * 768];
uint8_t f21cr[512 * 384];
uint8_t f21cb[512 * 384];
uint8_t *frame21[3] = { f21y, f21cr, f21cb };

uint8_t f30y[1024 * 768];
uint8_t f30cr[512 * 384];
uint8_t f30cb[512 * 384];
uint8_t *frame30[3] = { f30y, f30cr, f30cb };

uint8_t f31y[1024 * 768];
uint8_t f31cr[512 * 384];
uint8_t f31cb[512 * 384];
uint8_t *frame31[3] = { f31y, f31cr, f31cb };

uint8_t f4y[1024 * 768];
uint8_t f4cr[512 * 384];
uint8_t f4cb[512 * 384];
uint8_t *frame4[3] = { f4y, f4cr, f4cb };

uint8_t f5y[1024 * 768];
uint8_t f5cr[512 * 384];
uint8_t f5cb[512 * 384];
uint8_t *frame5[3] = { f5y, f4cr, f4cb };

uint8_t f6y[1024 * 768];
uint8_t f6cr[512 * 384];
uint8_t f6cb[512 * 384];
uint8_t *frame6[3] = { f6y, f4cr, f4cb };

struct vector
{
	int x;
	int y;
	uint32_t SAD;
};

struct vector mv_table[1024 / 32][1024 / 32][4];

/***********************************************************
 * helper-functions                                        *
 ***********************************************************/

void blend_fields (void);
void motion_compensate_field (void);
void cubic_interpolation (uint8_t * frame[3], int field);
void mc_interpolation (uint8_t * frame[3], int field);
void sinc_interpolation (uint8_t * frame[3], int field);
void aa_interpolation (uint8_t * frame[3], int field);
void aa_interpolation2 (uint8_t * frame[3], int field);

/***********************************************************
 * Main Loop                                               *
 ***********************************************************/

int
main (int argc, char *argv[])
{
	char c;
	int fd_in = 0;
	int fd_out = 1;
	int errno = 0;

	y4m_frame_info_t frameinfo;
	y4m_stream_info_t streaminfo;

	mjpeg_log (LOG_INFO,
		   "-------------------------------------------------");
	mjpeg_log (LOG_INFO,
		   "       Motion-Compensating-Deinterlacer          ");
	mjpeg_log (LOG_INFO,
		   "-------------------------------------------------");

  	while ((c = getopt (argc, argv, "hbf")) != -1)
  	{
    	switch (c)
    	{
    		case 'h':
   			{
				mjpeg_log(LOG_INFO," -b activates the bttv-chroma-hack ");
				mjpeg_log(LOG_INFO," -f fast-mode (field-interpolation)");
        		exit (0);
        		break;
      		}
    		case 'b':
   			{
        		bttv_hack=1;
        		break;
      		}
    		case 'f':
   			{
        		fast_mode=1;
        		break;
      		}
		}
	}

	/* initialize motion_library */
	init_motion_search ();

	/* initialize stream-information */
	y4m_init_stream_info (&streaminfo);
	y4m_init_frame_info (&frameinfo);

	/* open input stream */
	if ((errno = y4m_read_stream_header (fd_in, &streaminfo)) != Y4M_OK)
	{
		mjpeg_log (LOG_ERROR, "Couldn't read YUV4MPEG header: %s!",
			   y4m_strerr (errno));
		exit (1);
	}
	width = y4m_si_get_width (&streaminfo);
	height = y4m_si_get_height (&streaminfo);

	if(Y4M_ILACE_TOP_FIRST == y4m_si_get_interlace (&streaminfo))
	{
		mjpeg_log(LOG_INFO,"top-field-first");
		field_order=1;
	}
	else
		if(Y4M_ILACE_BOTTOM_FIRST == y4m_si_get_interlace (&streaminfo))
		{
			mjpeg_log(LOG_INFO,"bottom-field-first");
			field_order=0;
		}
		else
		{
			mjpeg_log(LOG_WARN,"stream either is marked progressive");
			mjpeg_log(LOG_WARN,"or stream has unknown field order");
			mjpeg_log(LOG_WARN,"using top-field-first and activating");
			mjpeg_log(LOG_WARN,"BTTV-Chroma-Hack");
			bttv_hack=1;
			field_order=1;
		}

	/* the output is not interlaced 4:2:0 */
	y4m_si_set_interlace (&streaminfo, Y4M_ILACE_NONE);

	/* write the outstream header */
	y4m_write_stream_header (fd_out, &streaminfo);

	/* read every single frame until the end of the input stream */
	while (Y4M_OK == (errno = y4m_read_frame (fd_in,
						  &streaminfo,
						  &frameinfo, frame11)))
	{

		/* copy the frame */

		memcpy (frame10[0], frame11[0], width * height);
		memcpy (frame10[1], frame11[1], width * height / 4);
		memcpy (frame10[2], frame11[2], width * height / 4);

		/* interpolate the other field */
		if(fast_mode==0)
		{
			aa_interpolation2 (frame10, field_order);
			aa_interpolation2 (frame11, 1-field_order);
			
			/* create working copy */

			memcpy (frame4[0], frame20[0], width * height);
			memcpy (frame4[1], frame20[1], width * height / 4);
			memcpy (frame4[2], frame20[2], width * height / 4);

			/* deinterlace the frame in the middle of the ringbuffer */

			motion_compensate_field ();
			blend_fields ();

			/* write output-frame */

			y4m_write_frame (fd_out, &streaminfo, &frameinfo, frame6);
		}		
		else
		{
			aa_interpolation2 (frame10, field_order);
			y4m_write_frame (fd_out, &streaminfo, &frameinfo, frame10);
		}

		/* rotate buffers */

		memcpy (frame31[0], frame21[0], width * height);
		memcpy (frame31[1], frame21[1], width * height / 4);
		memcpy (frame31[2], frame21[2], width * height / 4);
		memcpy (frame30[0], frame20[0], width * height);
		memcpy (frame30[1], frame20[1], width * height / 4);
		memcpy (frame30[2], frame20[2], width * height / 4);

		memcpy (frame21[0], frame11[0], width * height);
		memcpy (frame21[1], frame11[1], width * height / 4);
		memcpy (frame21[2], frame11[2], width * height / 4);
		memcpy (frame20[0], frame10[0], width * height);
		memcpy (frame20[1], frame10[1], width * height / 4);
		memcpy (frame20[2], frame10[2], width * height / 4);

	}

	/* did stream end unexpectedly ? */
	if (errno != Y4M_ERR_EOF)
		mjpeg_error_exit1 ("%s", y4m_strerr (errno));

	/* Exit gently */
	return (0);
}


void
blend_fields (void)
{
	int mode=0;
	int x, y;
	int delta,delta1,delta2,delta3,delta4,delta5;

	float korr1,korr2,korr3;
	float blend;

	korr1 = 0;
	for (y = 0; y < height; y++)
		for (x = 0; x < width; x++)
		{
			delta =	*(frame20[0] + x + y * width) -
						*(frame5[0] + x + y * width) ;
			delta *= delta;
			korr1 += delta;
		}
	korr1 /= width*height;
	korr1 = 1-sqrt(korr1)/256;

	korr2 = 0;
	for (y = 0; y < height; y++)
		for (x = 0; x < width; x++)
		{
			delta =	*(frame20[0] + x + y * width) -
						*(frame21[0] + x + y * width) ;
			delta *= delta;
			korr2 += delta;
		}
	korr2 /= width*height;
	korr2 = 1-sqrt(korr2)/256;

	korr3 = 0;
	for (y = 0; y < height; y++)
		for (x = 0; x < width; x++)
		{
			delta =	*(frame20[0] + x + y * width) -
						*(frame11[0] + x + y * width) ;
			delta *= delta;
			korr3 += delta;
		}
	korr3 /= width*height;
	korr3 = 1-sqrt(korr3)/256;

	if(korr2>=(korr1*0.99))
	{
		mjpeg_log(LOG_INFO," %3.1f %3.1f %3.1f -- mode: PROGRESSIVE SCAN",korr1*100,korr2*100,korr3*100);
		mode=1;
	}
	else		
		if(korr3>=(korr1*0.99))
		{
			mjpeg_log(LOG_INFO," %3.1f %3.1f %3.1f -- mode: TELECINED VIDEO",korr1*100,korr2*100,korr3*100);
			mode=1;
		}
		else		
		{
			mjpeg_log(LOG_INFO," %3.1f %3.1f %3.1f -- mode: INTERLACED VIDEO",korr1*100,korr2*100,korr3*100);
			mode=0;
		}

	if(mode==0)
	for (y = 0; y < height; y++)
		for (x = 0; x < width; x++)
		{

			delta1 = 
				  *(frame4[0] + x + (y+0) * width) -
				  *(frame5[0] + x + (y+0) * width);
			delta1 *= delta1;
			delta1 /= 256;

			blend = (256.f-delta1)/(256.f*2);

			*(frame6[0] + x + y * width)=
			*(frame4[0] + x + y * width)*(  blend)+
			*(frame5[0] + x + y * width)*(1-blend);
		}
	else
		if(mode==1)
			for (y = 0; y < height; y++)
				for (x = 0; x < width; x++)
				{
					*(frame6[0] + x + y * width)=
						*(frame21[0] + x + y * width)*0.5+
						*(frame20[0] + x + y * width)*0.5;
				}
	else
		if(mode==2)
			for (y = 0; y < height; y++)
				for (x = 0; x < width; x++)
				{
					*(frame6[0] + x + y * width)=
						*(frame11[0] + x + y * width)*0.5+
						*(frame20[0] + x + y * width)*0.5;
				}
}


void
motion_compensate_field (void)
{
	static struct vector vlist[4];

	int vx, vy, tx, ty;
	int x, y, xx, yy;
	int x1, x2, y1, y2, x3, y3, x4, y4;
	uint32_t SAD;
	uint32_t cSAD;
	uint32_t min;
	int addr1 = 0;
	int addr2 = 0;
	int w = width, h = height;
	int ci = 0, di = 0;
	int cnt;


	//search-radius
	int r = 16;

	for (yy = 0; yy < h; yy += 16)
		for (xx = 0; xx < w; xx += 16)
		{
			/* search motionvector one field forwards ... */

			x1 = y1 = 0;

			/* the center match is two fields forwards, other than
               the motion search. This stabelizes zero-vectors! */
			min = 0;
			addr1 = (xx) + (yy) * w;
			min  += psad_00 (frame20[0] + addr1,
					        frame10[0] + addr1, w,
					        16, 0);
			if(bttv_hack==0)
			{
				/* match chroma */
				addr1 = (xx) / 2 + (yy) * w / 4;
				min += psad_00 (frame20[1] + addr1,
						frame10[1] + addr1,
						w / 2, 8, 0);
			}
			/* begin the search */
			for (vy = -r; vy < r; vy+=2)
				for (vx = -r; vx <r; vx++)
				{
					/* match luma */
					addr1 = (xx) + (yy) * w;
					addr2 = (xx + vx) + (yy + vy) * w;
					SAD = psad_00 (frame20[0] + addr1,
						       frame21[0] + addr2, w,
						       16, 0);

					if(bttv_hack==0)
					{
					/* match chroma */
					addr1 = (xx) / 2 + (yy) * w / 4;
					addr2 = (xx + vx) / 2 + (yy +
								 vy) * w / 4;
					SAD += psad_00 (frame20[1] + addr1,
							frame21[1] + addr2,
							w / 2, 8, 0);
					}

					if (SAD < min)
					{
						min = SAD;

						x1 = vx;
						y1 = vy;
					}
				}

			/* transform the sourceblock by the found vector */
			for (y = 0; y < 16; y++)
				for (x = 0; x < 16; x++)
				{
					*(frame5[0] + (xx + x) + (yy + y) * w) =
						*(frame21[0] + (xx + x + x1) + (yy + y + y1) * w);
				}
		}
}


void
sinc_interpolation (uint8_t * frame[3], int field)
{
	int x, y;

	// Luma 

	for (y = field; y < height; y += 2)
		for (x = 0; x < width; x++)
		{
			float v;
			v = (0.114)  * (float) *(frame[0] + (y - 5) * width + x) +
			    (-0.188) * (float) *(frame[0] + (y - 3) * width + x) +
				(0.574)  * (float) *(frame[0] + (y - 1) * width + x) +
				(0.574)  * (float) *(frame[0] + (y + 1) * width + x) +
				(-0.188) * (float) *(frame[0] + (y + 3) * width + x) +
				(0.114)  * (float) *(frame[0] + (y + 5) * width + x);

			v = v > 235 ? 235 : v;
			v = v < 16 ? 16 : v;
			*(frame[0] + y * width + x) = v;
		}

	if(bttv_hack==0)
	{
	// Chroma 

	height /= 2;
	width /= 2;

	for (y = (field + 2); y < (height - 2); y += 2)
		for (x = 0; x < width; x++)
		{
			float v;
			v = 0.5 * (float) *(frame[1] + (y - 1) * width + x) +
				0.5 * (float) *(frame[1] + (y + 1) * width +
						x);

			v = v > 240 ? 240 : v;
			v = v < 16 ? 16 : v;
			*(frame[1] + y * width + x) = v;

			v = 0.5 * (float) *(frame[2] + (y - 1) * width + x) +
				0.5 * (float) *(frame[2] + (y + 1) * width +
						x);

			v = v > 240 ? 240 : v;
			v = v < 16 ? 16 : v;
			*(frame[2] + y * width + x) = v;
		}

	height *= 2;
	width *= 2;
    }
}

void aa_interpolation( uint8_t * frame[3], int field)
{
	int x,y,dx,v1,v2,z,d;
	uint32_t SSE;
	uint32_t min;
	int w=24;

	for (y = (field + 2); y < (height - 2); y += 2)
		for (x = 0; x < width; x+=2)
		{
			
			v1=v2=0;
			min=0;
			for(z=-w; z <= w ;z++)
			{
				d = 	*(frame[0]+z+x     +(y-1)*width) -
						*(frame[0]+z+x     +(y+1)*width);
				d *= d;
				min += d;
			}

			for(dx=-(w/2) ; dx<=(w/2) ; dx++)
			{
				SSE=0;
				for(z=-w; z <= w ;z++)
				{
					d = 	*(frame[0]+z+x     +(y-1)*width) -
							*(frame[0]+z+x+dx  +(y+1)*width);
					d *= d;
					SSE += d;
					//SSE += z*z/4;
				}
				if (SSE<(min*0.5))
				{
					min=SSE;
					v1=dx/2;
				}
			}

			d=
				(	*(frame[0]-v1+(x)+(y-1)*width) +
					*(frame[0]+v1+(x)+(y+1)*width)	)/2;

			*(frame[0]+(x)+(y)*width) = d;
		}	

	if(bttv_hack==0)
	{
	// Chroma 
	height /= 2;
	width /= 2;

	for (y = (field + 2); y < (height - 2); y += 2)
		for (x = 0; x < width; x++)
		{
			float v;
			v = 0.5 * (float) *(frame[1] + (y - 1) * width + x) +
				0.5 * (float) *(frame[1] + (y + 1) * width +
						x);

			v = v > 240 ? 240 : v;
			v = v < 16 ? 16 : v;
			*(frame[1] + y * width + x) = v;

			v = 0.5 * (float) *(frame[2] + (y - 1) * width + x) +
				0.5 * (float) *(frame[2] + (y + 1) * width +
						x);

			v = v > 240 ? 240 : v;
			v = v < 16 ? 16 : v;
			*(frame[2] + y * width + x) = v;
		}

	height *= 2;
	width *= 2;
	}
}

void aa_interpolation2( uint8_t * frame[3], int field)
{
	int x,y,dx,v1,v2,z,d;
	uint32_t SSE;
	uint32_t min;
	int w1=3;
	int w2=3;

	for (y = (field + 2); y < (height - 2); y += 2)
		for (x = 0; x < width; x++)
		{
			
			v1=v2=0;
			min=0;
			for(z=-w1; z <= w1 ;z++)
			{
				d = 	*(frame[0]+z+x     +(y-1)*width) -
						*(frame[0]+z+x     +(y+1)*width);
				d *= d;
				min += d;
			}
			min=0x00ffffff;
			for(dx=-w2 ; dx<=w2 ; dx++)
			{
				SSE=0;
				for(z=-w1; z <= w1 ;z++)
				{
					d = 	*(frame[0]+z+x     +(y-1)*width) -
							*(frame[0]+z+x+dx  +(y+1)*width);
					d *= d;
					SSE += d;
					d = 	*(frame[0]+z+x     +(y+1)*width) -
							*(frame[0]+z+x+dx  +(y+3)*width);
					d *= d;
					SSE += d;
					d = 	*(frame[0]+z+x     +(y-3)*width) -
							*(frame[0]+z+x+dx  +(y-1)*width);
					d *= d;
					SSE += d;
				}
				if (SSE<=min)
				{
					min=SSE;
					v1=dx;
				}
			}


			//if(v1==0) d=0;
			//else
			{
				v1/=2;
				d=
					(	*(frame[0]-v1+(x)+(y-1)*width) +
						*(frame[0]+v1+(x)+(y+1)*width)	)/2;
			}
			*(frame[0]+(x)+(y)*width) = d;
		}	

	if(bttv_hack==0)
	{
	// Chroma 
	height /= 2;
	width /= 2;

	for (y = (field + 2); y < (height - 2); y += 2)
		for (x = 0; x < width; x++)
		{
			float v;
			v = 0.5 * (float) *(frame[1] + (y - 1) * width + x) +
				0.5 * (float) *(frame[1] + (y + 1) * width +
						x);

			v = v > 240 ? 240 : v;
			v = v < 16 ? 16 : v;
			*(frame[1] + y * width + x) = v;

			v = 0.5 * (float) *(frame[2] + (y - 1) * width + x) +
				0.5 * (float) *(frame[2] + (y + 1) * width +
						x);

			v = v > 240 ? 240 : v;
			v = v < 16 ? 16 : v;
			*(frame[2] + y * width + x) = v;
		}

	height *= 2;
	width *= 2;
	}
}
