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

int      width  = 0;
int      height = 0;

uint8_t  f10y   [1024*768];
uint8_t  f10cr  [512*384]; 
uint8_t  f10cb  [512*384]; 
uint8_t* frame10[3]={f10y,f10cr,f10cb};

uint8_t  f11y   [1024*768];
uint8_t  f11cr  [512*384]; 
uint8_t  f11cb  [512*384]; 
uint8_t* frame11[3]={f11y,f11cr,f11cb};

uint8_t  f20y   [1024*768];
uint8_t  f20cr  [512*384]; 
uint8_t  f20cb  [512*384]; 
uint8_t* frame20[3]={f20y,f20cr,f20cb};

uint8_t  f21y   [1024*768];
uint8_t  f21cr  [512*384]; 
uint8_t  f21cb  [512*384]; 
uint8_t* frame21[3]={f21y,f21cr,f21cb};

uint8_t  f30y   [1024*768];
uint8_t  f30cr  [512*384]; 
uint8_t  f30cb  [512*384]; 
uint8_t* frame30[3]={f30y,f30cr,f30cb};

uint8_t  f31y   [1024*768];
uint8_t  f31cr  [512*384]; 
uint8_t  f31cb  [512*384]; 
uint8_t* frame31[3]={f31y,f31cr,f31cb};

uint8_t  f4y   [1024*768];
uint8_t  f4cr  [512*384]; 
uint8_t  f4cb  [512*384]; 
uint8_t* frame4[3]={f4y,f4cr,f4cb};

uint8_t  f5y   [1024*768];
uint8_t  f5cr  [512*384]; 
uint8_t  f5cb  [512*384]; 
uint8_t* frame5[3]={f5y,f4cr,f4cb};

struct vector
{
	int x;
    int y;
};

struct vector mv_table[1024/32][1024/32][4];

/***********************************************************
 * helper-functions                                        *
 ***********************************************************/

void blend_fields(void);
void motion_compensate_field(void);
void cubic_interpolation(uint8_t * frame[3], int field);
void mc_interpolation(uint8_t * frame[3],int field);
void sinc_interpolation(uint8_t * frame[3], int field);

/***********************************************************
 * Main Loop                                               *
 ***********************************************************/

int main(int argc, char *argv[])
{
    int               fd_in  = 0;
    int               fd_out = 1;
    int               errno  = 0;
    
    y4m_frame_info_t  frameinfo;
    y4m_stream_info_t streaminfo;
    
    init_motion_search();
    
    /* initialize stream-information */
    y4m_init_stream_info (&streaminfo);
    y4m_init_frame_info (&frameinfo);
    
    /* open input stream */
    if ((errno = y4m_read_stream_header (fd_in, &streaminfo)) != Y4M_OK)
    {
	mjpeg_log (LOG_ERROR, "Couldn't read YUV4MPEG header: %s!", y4m_strerr (errno));
	exit (1);
    }
    width    = y4m_si_get_width(&streaminfo);
    height   = y4m_si_get_height(&streaminfo);
    
    /* we don't care if the stream is marked as not interlaced 
       because, if the user uses this tool, it is... */
    y4m_si_set_interlace(&streaminfo,Y4M_ILACE_NONE);
    
    /* write the outstream header */
    y4m_write_stream_header (fd_out, &streaminfo); 
    
    mjpeg_log (LOG_INFO, "---------------------------------------------------------");
    mjpeg_log (LOG_INFO, "           Motion-Compensating-Deinterlacer              ");
    mjpeg_log (LOG_INFO, "---------------------------------------------------------");

    /* read every single frame until the end of the input stream */
    while 
	(
	    Y4M_OK == ( errno = y4m_read_frame (fd_in, 
						&streaminfo, 
						&frameinfo, 
						frame11) )
	    )
    {

	/* copy the frame */

	memcpy ( frame10[0], frame11[0], width*height );
	memcpy ( frame10[1], frame11[1], width*height/4 );
	memcpy ( frame10[2], frame11[2], width*height/4 );

	/* interpolate the other field */

	sinc_interpolation( frame10,0 );
	sinc_interpolation( frame11,1 );

	/* create working copy */

	memcpy ( frame4[0], frame20[0], width*height );
	memcpy ( frame4[1], frame20[1], width*height/4 );
	memcpy ( frame4[2], frame20[2], width*height/4 );

	/* deinterlace the frame in the middle of the ringbuffer */

	motion_compensate_field();
	blend_fields();

	/* write output-frame */

	y4m_write_frame ( fd_out, 
			  &streaminfo, 
			  &frameinfo, 
			  frame4);

	/* rotate buffers */

	memcpy ( frame31[0], frame21[0], width*height );
	memcpy ( frame31[1], frame21[1], width*height/4 );
	memcpy ( frame31[2], frame21[2], width*height/4 );
	memcpy ( frame30[0], frame20[0], width*height );
	memcpy ( frame30[1], frame20[1], width*height/4 );
	memcpy ( frame30[2], frame20[2], width*height/4 );

	memcpy ( frame21[0], frame11[0], width*height );
	memcpy ( frame21[1], frame11[1], width*height/4 );
	memcpy ( frame21[2], frame11[2], width*height/4 );
	memcpy ( frame20[0], frame10[0], width*height );
	memcpy ( frame20[1], frame10[1], width*height/4 );
	memcpy ( frame20[2], frame10[2], width*height/4 );
	
    }
    
  /* did stream end unexpectedly ? */
  if(errno != Y4M_ERR_EOF )
      mjpeg_error_exit1( "%s", y4m_strerr( errno ) );
  
  /* Exit gently */
  return(0);
}


void blend_fields(void)
{
    int qcnt=0;
    int x,y;
    int delta;
    
    for(y=0;y<height;y++)
	for(x=0;x<width;x++)
	{
		*(frame4[0]+x+y*width)=
			(
				*(frame4[0]+x+y*width)+
				*(frame5[0]+x+y*width)
			)/2;
	}
}

void motion_compensate_field(void)
{ 
#if 1
    int vx,vy,tx,ty;
    int x,y,xx,yy;
    int x1,x2,y1,y2,x3,y3,x4,y4;
    uint32_t SAD;
    uint32_t cSAD;
    uint32_t min;
    int addr1=0;
    int addr2=0;
    int w=width,h=height;
    int ci=0,di=0;

    //search-radius
    int r=32;

    for(yy=0;yy<h;yy+=32)
	for(xx=0;xx<w;xx+=32)
	{
	    /* search forwards-frame-vector */

	    addr1=(xx)+(yy)*w;
	    addr2=(xx)+(yy)*w;
	    min  = psad_00(frame10[0]+addr1,   frame20[0]+addr2   ,w,32,0);
	    min += psad_00(frame10[0]+addr1+16,frame20[0]+addr2+16,w,32,0);

	    x2=y2=0;
	    for(vy=(-r);vy<(r);vy++)
		for(vx=(-r);vx<(r);vx++)
		{
		    addr1=(xx   -0)+(yy   -0)*w;
		    addr2=(xx+vx-0)+(yy+vy-0)*w;
		    SAD = psad_00(frame10[0]+addr1,   frame20[0]+addr2   ,w,32,0);
		    SAD+= psad_00(frame10[0]+addr1+16,frame20[0]+addr2+16,w,32,0);

		    if((SAD*1.1)<min)
		    {
			min=SAD;
			x2=vx;
			y2=vy;
		    }  
		}

	    /* search forwards-field-vector */

	    addr1=(xx)+(yy)*w;
	    addr2=(xx)+(yy)*w;
	    min  = psad_00(frame21[0]+addr1    ,frame20[0]+addr2   ,w,32,0);
	    min += psad_00(frame21[0]+addr1+16 ,frame20[0]+addr2+16,w,32,0);

	    x1=y1=0;
	    for(vy=(-r/2);vy<(r/2);vy++)
		for(vx=(-r/2);vx<(r/2);vx++)
		{
		    addr1=(xx   -0)+(yy   -0)*w;
		    addr2=(xx+vx-0)+(yy+vy-0)*w;
		    SAD  = psad_00(frame21[0]+addr1,   frame20[0]+addr2   ,w,32,0);
		    SAD += psad_00(frame21[0]+addr1+16,frame20[0]+addr2+16,w,32,0);

		    if((SAD*1.1)<min)
		    {
			min=SAD;
			x1=vx;
			y1=vy;
		    }  
		}

	    /* search backwards-frame-vector */

	    addr1=(xx)+(yy)*w;
	    addr2=(xx)+(yy)*w;
	    min  = psad_00(frame30[0]+addr1,   frame20[0]+addr2   ,w,32,0);
	    min += psad_00(frame30[0]+addr1+16,frame20[0]+addr2+16,w,32,0);

	    x3=y3=0;
	    for(vy=(-r);vy<(r);vy++)
		for(vx=(-r);vx<(r);vx++)
		{
		    addr1=(xx   -0)+(yy   -0)*w;
		    addr2=(xx+vx-0)+(yy+vy-0)*w;
		    SAD = psad_00(frame30[0]+addr1,   frame20[0]+addr2   ,w,32,0);
		    SAD+= psad_00(frame30[0]+addr1+16,frame20[0]+addr2+16,w,32,0);

		    if((SAD*1.1)<min)
		    {
			min=SAD;
			x3=vx;
			y3=vy;
		    }  
		}

	    /* search backwards-field-vector */

	    addr1=(xx)+(yy)*w;
	    addr2=(xx)+(yy)*w;
	    min  = psad_00(frame30[0]+addr1,   frame20[0]+addr2   ,w,32,0);
	    min += psad_00(frame30[0]+addr1+16,frame20[0]+addr2+16,w,32,0);

	    x4=y4=0;
	    for(vy=(-r/2);vy<(r/2);vy++)
		for(vx=(-r/2);vx<(r/2);vx++)
		{
		    addr1=(xx   -0)+(yy   -0)*w;
		    addr2=(xx+vx-0)+(yy+vy-0)*w;
		    SAD = psad_00(frame31[0]+addr1,   frame20[0]+addr2   ,w,32,0);
		    SAD+= psad_00(frame31[0]+addr1+16,frame20[0]+addr2+16,w,32,0);

		    if((SAD*1.1)<min)
		    {
			min=SAD;
			x4=vx;
			y4=vy;
		    }  
		}

	    /* transform the sourceblock by the found vector */

        x1 = (-x3-x4+x1+x2)/6;
        y1 = (-y3-y4+y1+y2)/6;
	    
	    for(y=0;y<32;y++)
		for(x=0;x<32;x++)
		{  
			*( frame5[0] +(xx+x   )+(yy+y   )*w) =
			*( frame21[0]+(xx+x-x1)+(yy+y-y1)*w);
		}
	}
#endif
}


void sinc_interpolation(uint8_t * frame[3], int field)
{
    int x,y;
    
    for(y=field;y<height;y+=2)
	for(x=0;x<width;x++)
	{
		float v;
		v =
			( 0.114) * (float)*(frame[0]+(y-5)*width+x)+
			(-0.188) * (float)*(frame[0]+(y-3)*width+x)+
			( 0.574) * (float)*(frame[0]+(y-1)*width+x)+
			( 0.574) * (float)*(frame[0]+(y+1)*width+x)+
			(-0.188) * (float)*(frame[0]+(y+3)*width+x)+
			( 0.114) * (float)*(frame[0]+(y+5)*width+x);
		v = v>240? 240:v;
		v = v< 16?  16:v;
		*(frame[0]+y*width+x)=v;
	}
}

void cubic_interpolation(uint8_t * frame[3], int field)
{
    int x,y;
    
    for(y=field;y<height;y+=2)
	for(x=0;x<width;x++)
	{
	    float a,b,c;
	    float o[4];
	    float v;
	    
	    o[0]=*(frame[0]+x+(y-2)*width);
	    o[1]=*(frame[0]+x+(y+0)*width);
	    o[2]=*(frame[0]+x+(y+2)*width);
	    o[3]=*(frame[0]+x+(y+4)*width);
	    
	    a = ( 3.f * ( o[1] - o[2] ) - o[0] + o[3] )/2.f;
	    b = 2.f * o[2] + o[0] - ( 5.f * o[1] + o[3] ) / 2.f;
	    c = ( o[2] - o[0] ) / 2.f;
	    
	    v = a * 0.125 + b * 0.25 + c * 0.5 + o[1];
	    v = v>240? 240:v;
	    v = v<16 ? 16:v;
	    
	    *(frame[0]+x+(y+1)*width)=v;
	}
}

void mc_interpolation ( uint8_t * frame[3], int field)
{
	int x,y,dx,vx=0;
	uint32_t SAD=0x00ffffff;
	uint32_t minSAD=0x00ffffff;
	int addr1=0,addr2=0;
	int z;
	int p;

	for(y=field;y<height;y+=2)
	  	for(x=0;x<width;x+=8)
		{

			/* search match */
			
			vx=0;
			minSAD=0x00ffffff;

			for(dx=-4;dx<=4;dx++)
			{
				addr1=y*width+x-width   ;
				addr2=y*width+x+width+dx;
				
				SAD=0;
				for(z=0;z<8;z++)
				{
					p = *(frame[0]+addr1+z) - 
					    *(frame[0]+addr2+z) ;
					p = p*p;
					SAD += p + dx*dx;
				}

				if( SAD<minSAD )
				{
					vx=dx;
					minSAD=SAD;
				}
			}

			for (dx=0;dx<8;dx++)
			{
				*(frame[0]+y*width+dx+x)=
				  (*(frame[0]+y*width-width-vx/2+dx+x)>>1)+
				  (*(frame[0]+y*width+width+vx/2+dx+x)>>1);
			}
		}
}
