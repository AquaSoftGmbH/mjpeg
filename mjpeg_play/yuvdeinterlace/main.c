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

#define BLK 16
#define TR 128

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

uint8_t  f5y   [1024*768];
uint8_t  f5cr  [512*384]; 
uint8_t  f5cb  [512*384]; 
uint8_t* frame5[3]={f5y,f1cr,f1cb};

uint8_t  f4y   [1024*768];
uint8_t  f4cr  [512*384]; 
uint8_t  f4cb  [512*384]; 
uint8_t* frame4[3]={f4y,f1cr,f1cb};

uint8_t  f3y   [1024*768];
uint8_t  f3cr  [512*384]; 
uint8_t  f3cb  [512*384]; 
uint8_t* frame3[3]={f3y,f1cr,f1cb};

uint32_t calc_SAD_mmxe (uint8_t * frm, uint8_t * ref);

struct VECTOR
{
    int x;
    int y;
    uint32_t SAD;
};

struct VECTOR vlist[2];

/***********************************************************
 *                                                         *
 ***********************************************************/

int main(int argc, char *argv[])
{
  int               fd_in  = 0;
  int               fd_out = 1;
  int               errno  = 0;
  uint32_t          mean_SAD=0;

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

  /* read every single frame until the end of the input stream */
  while 
    (
          Y4M_OK == ( errno = y4m_read_frame (fd_in, 
                                              &streaminfo, 
                                              &frameinfo, 
                                              frame1) )
    )
    {

	memcpy ( frame2[0], frame1[0], width*height );

	{
	    int x,y;

	    for(y=0;y<height;y+=2)
		for(x=0;x<width;x++)
		{
		    float a,b,c;
		    float o[4];
		    float v;
		    
		    o[0]=*(frame1[0]+x+(y-2)*width);
		    o[1]=*(frame1[0]+x+(y+0)*width);
		    o[2]=*(frame1[0]+x+(y+2)*width);
		    o[3]=*(frame1[0]+x+(y+4)*width);
		    
		    a = ( 3.f * ( o[1] - o[2] ) - o[0] + o[3] )/2.f;
		    b = 2.f * o[2] + o[0] - ( 5.f * o[1] + o[3] ) / 2.f;
		    c = ( o[2] - o[0] ) / 2.f;
		    
		    v = a * 0.125 + b * 0.25 + c * 0.5 + o[1];
		    v = v>240? 240:v;
		    v = v<16 ? 16:v;
		    
		    *(frame1[0]+x+(y+1)*width)=v;
		}

	    for(y=1;y<height;y+=2)
		for(x=0;x<width;x++)
		{
		    float a,b,c;
		    float o[4];
		    float v;
		    
		    o[0]=*(frame2[0]+x+(y-2)*width);
		    o[1]=*(frame2[0]+x+(y+0)*width);
		    o[2]=*(frame2[0]+x+(y+2)*width);
		    o[3]=*(frame2[0]+x+(y+4)*width);
		    
		    a = ( 3.f * ( o[1] - o[2] ) - o[0] + o[3] )/2.f;
		    b = 2.f * o[2] + o[0] - ( 5.f * o[1] + o[3] ) / 2.f;
		    c = ( o[2] - o[0] ) / 2.f;
		    
		    v = a * 0.125 + b * 0.25 + c * 0.5 + o[1];
		    v = v>240? 240:v;
		    v = v<16 ? 16:v;

		    *(frame2[0]+x+(y+1)*width)=v;
		}
	    
	}
	
#if 1
	// motion compensate the true 2nd field to the interpolated one
	{ 
	    int vx,vy,tx,ty;
	    int x,y,xx,yy;
	    uint32_t SAD;
	    uint32_t cSAD;
	    uint32_t min;
	    uint32_t sum_SAD;
	    int addr1=0;
	    int addr2=0;

	    sum_SAD=0;
	    for(yy=0;yy<height;yy+=16)
		for(xx=0;xx<width;xx+=16)
		{
		    tx=ty=0;

		    addr1=(xx   )+(yy   )*width;
		    cSAD=psad_00(frame1[0]+addr1,frame2[0]+addr1,width,16,0);
		    min=cSAD;

		    for(vy=-16;vy<16;vy++)
			for(vx=-16;vx<16;vx++)
			{
			    addr1=(xx   )+(yy   )*width;
			    addr2=(xx+vx)+(yy+vy)*width;
				//SAD=calc_SAD_mmxe(frame5[0]+addr1,frame4[0]+addr2);
			    SAD=psad_00(frame1[0]+addr1,frame2[0]+addr2,width,16,0);
			    
			    if(SAD<min)
			    {
				min=SAD;
				tx=vx;
				ty=vy;
			    }  
			}
		    sum_SAD+=SAD;

		    if( ((cSAD)<=(2*min)) && tx!=0 && ty!=0 )
		    {
			fprintf(stderr,"!");
			tx=ty=0;
		    }

		    // transform the sourceblock by that vector if match is good
		    // else use interpolation
		    if(SAD<=(mean_SAD*80) )
			for(y=0;y<16;y++)
			    for(x=0;x<16;x++)
			    {
				*(frame3[0]+(xx+x   )+(yy+y   )*width) =
				    *(frame2[0]+(xx+x+tx)+(yy+y+ty)*width);
			    }
		    else
			for(y=0;y<16;y++)
			    for(x=0;x<16;x++)
			    {
				*(frame3[0]+(xx+x   )+(yy+y   )*width) = 
				    *(frame1[0]+(xx+x)+(yy+y)*width);
			    }			

		}
	    sum_SAD  = sum_SAD*256/width/height;
	    mean_SAD = sum_SAD>mean_SAD? sum_SAD:mean_SAD;
	    mean_SAD *= 80;
	    mean_SAD /= 100;
//	    mean_SAD = mean_SAD <= (3*16*16)? (3*16*16):mean_SAD;

	    fprintf(stderr,"%d\n",mean_SAD);
	}
	
#endif

#if 1	
	{
	    int x,y;
	    int delta;

	    for(y=0;y<height;y++)
		for(x=0;x<width;x++)
		{
		    delta = 
			*(frame1[0]+x+y*width) -
			*(frame3[0]+x+y*width) ;

		    delta = delta<0 ? -delta:delta;

		    if(delta<16)
			*(frame2[0]+x+y*width)=
			    (*(frame1[0]+x+y*width)>>1)+
			    (*(frame3[0]+x+y*width)>>1);
		    else
			*(frame2[0]+x+y*width)=
			    *(frame1[0]+x+y*width);
		}
	}
#endif

	/* write output-frame */

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

uint32_t
calc_SAD_mmxe (uint8_t * frm, uint8_t * ref)
{
  static uint32_t a;
  
#ifdef HAVE_ASM_MMX
  __asm__ __volatile__
    (
    " pxor         %%mm0 , %%mm0;          /* clear mm0                           */\n"
    " movl         %1    , %%eax;          /* load frameadress into eax           */\n"
    " movl         %2    , %%ebx;          /* load frameadress into ebx           */\n"
    " movl         %3    , %%ecx;          /* load width       into ecx           */\n"
    "                           ;          /*                                     */\n"
    " .rept 16                  ;          /*                                     */\n"
    " movq        (%%eax), %%mm1;          /* 8 Pixels from filtered frame to mm1 */\n"
    " psadbw      (%%ebx), %%mm1;          /* 8 Pixels difference to mm1          */\n"
    " paddusw      %%mm1 , %%mm0;          /* add result to mm0                   */\n"
    " addl         %%ecx , %%eax;          /* add framewidth to frameaddress      */\n"
    " addl         %%ecx , %%ebx;          /* add framewidth to frameaddress      */\n"
    " .endr                     ;          /*                                     */\n"
    "                           ;          /*                                     */\n"
    " movl         %1    , %%eax;          /* load frameadress into eax           */\n"
    " movl         %2    , %%ebx;          /* load frameadress into ebx           */\n"
    " addl         $8    , %%eax;          /* shift frame by 8 pixels             */\n"
    " addl         $8    , %%ebx;          /*                                     */\n"
    "                           ;          /*                                     */\n"
    " .rept 16                  ;          /*                                     */\n"
    " movq        (%%eax), %%mm1;          /* 8 Pixels from filtered frame to mm1 */\n"
    " psadbw      (%%ebx), %%mm1;          /* 8 Pixels difference to mm1          */\n"
    " paddusw      %%mm1 , %%mm0;          /* add result to mm0                   */\n"
    " addl         %%ecx , %%eax;          /* add framewidth to frameaddress      */\n"
    " addl         %%ecx , %%ebx;          /* add framewidth to frameaddress      */\n"
    " .endr                     ;          /*                                     */\n"
    "                           ;          /*                                     */\n"
    " movq         %%mm0 , %0   ;          /* make mm0 available to gcc ...       */\n"
    :"=m" (a)     
    :"m" (frm), "m" (ref), "m" (width*2)
    :"%eax", "%ebx", "%ecx"
    );
#endif
  return a;
}




