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

uint8_t  f1y   [1024*768];
uint8_t  f1cr  [512*384]; 
uint8_t  f1cb  [512*384]; 
uint8_t* frame1[3]={f1y,f1cr,f1cb};

uint8_t  f2y   [1024*768];
uint8_t  f2cr  [512*384]; 
uint8_t  f2cb  [512*384]; 
uint8_t* frame2[3]={f2y,f1cr,f1cb};

uint8_t  f3y   [1024*768];
uint8_t  f3cr  [512*384]; 
uint8_t  f3cb  [512*384]; 
uint8_t* frame3[3]={f3y,f1cr,f1cb};

/***********************************************************
 * helper-functions                                        *
 ***********************************************************/

void blend_fields(void);
void motion_compensate_field(void);
void cubic_interpolation(uint8_t * frame[3], int field);
void mc_interpolation(uint8_t * frame[3],int field);

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
						frame1) )
	    )
    {
	
	/* make a copy of the current frame so we can work on it */
	memcpy ( frame2[0], frame1[0], width*height );
	
	/* frame1[] will contain the first field and frame2[] the second.
	 * so here we go and interpolate the missing lines in every single
	 * field ... 
	 */
	
	mc_interpolation(frame1,0);
	mc_interpolation(frame2,1);
	
	/* motion compensate the interpolated frame of field 2 to the 
         * interpolated frame of field 1
         */

	//motion_compensate_field();
	
	/* blend fields */

	//blend_fields();

	/* write output-frame */

	y4m_write_frame ( fd_out, 
			  &streaminfo, 
			  &frameinfo, 
			  frame1); 
	y4m_write_frame ( fd_out, 
			  &streaminfo, 
			  &frameinfo, 
			  frame2); 

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
	    delta = 
		*(frame1[0]+x+y*width) -
		*(frame3[0]+x+y*width) ;
	    
	    delta = delta<0 ? -delta:delta;
	    
	    if(delta<12)
	    {
		*(frame2[0]+x+y*width)=
		    (*(frame1[0]+x+y*width)>>1)+
		    (*(frame3[0]+x+y*width)>>1);
	    }
	    else
	    {
		*(frame2[0]+x+y*width)=
		    *(frame1[0]+x+y*width);
		qcnt++;
	    }
	}
    
    mjpeg_log (LOG_INFO, 
	       "motion-compensated pixels: %3.1f %%", 
	       100-100*(float)qcnt / ( (float)width*(float)height ) );
}

void motion_compensate_field(void)
{ 
    int vx,vy,tx,ty;
    int x,y,xx,yy;
    uint32_t SAD;
    uint32_t cSAD;
    uint32_t min;
    int addr1=0;
    int addr2=0;
    
    for(yy=0;yy<height;yy+=32)
	for(xx=0;xx<width;xx+=32)
	{
	    tx=ty=0;
	    
	    addr1=(xx   )+(yy   )*width;
	    min=psad_00(frame1[0]+addr1,frame2[0]+addr1,width,32,0);
	    min+=psad_00(frame1[0]+addr1+16,frame2[0]+addr1+16,width,32,0);
	    
	    for(vy=-24;vy<24;vy++)
		for(vx=-24;vx<24;vx++)
		{
		    addr1=(xx   )+(yy   )*width;
		    addr2=(xx+vx)+(yy+vy)*width;
		    SAD  = psad_00(frame1[0]+addr1,frame2[0]+addr2,width,32,0);
		    SAD += psad_00(frame1[0]+addr1+16,frame2[0]+addr2+16,width,32,0);
		    
		    if(SAD<min)
		    {
			min=SAD;
			tx=vx;
			ty=vy;
		    }  
		}
	    
	    /* transform the sourceblock by the found vector */
	    for(y=0;y<32;y++)
		for(x=0;x<32;x++)
		{  
			*(frame3[0]+(xx+x   )+(yy+y   )*width) =
			*(frame2[0]+(xx+x+tx)+(yy+y+ty)*width);
		}
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
	int addr3=0,addr4=0;
	int z;
	int32_t p,lum1,lum2;

	for(y=field;y<height;y+=2)
	  	for(x=0;x<width;x+=8)
		{

			/* search match */
			
			vx=0;
			minSAD=0x00ffffff;
			for(dx=-8;dx<=8;dx++)
			{
				addr1=y*width+x-width-dx;
				addr2=y*width+x+width+dx;
				
				SAD=0;
				for(z=0;z<8;z++)
				{
					p = *(frame[0]+addr1+z) - 
					    *(frame[0]+addr2+z) ;
					p *= p;
					SAD += p;
				}
		
				if( SAD<=minSAD )
				{
					vx=dx;
					minSAD=SAD;
				}
			}

			for (dx=0;dx<8;dx++)
			{
				*(frame[0]+y*width+dx+x)=
				  (*(frame[0]+y*width-width-vx+dx+x)>>1)+
				  (*(frame[0]+y*width+width+vx+dx+x)>>1);
			}
		}
}
