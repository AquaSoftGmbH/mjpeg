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

	for(y=1;y<height;y+=2)
	    for(x=0;x<width;x++)
		*(frame2[0]+x+y*width)=
		    ( *(frame2[0]+x+(y-1)*width)+
		      *(frame2[0]+x+(y+1)*width)  )/2;

	for(y=0;y<height;y+=2)
	    for(x=0;x<width;x++)
		*(frame1[0]+x+y*width)=
		    ( *(frame1[0]+x+(y-1)*width)+
		      *(frame1[0]+x+(y+1)*width)  )/2;

	for(y=1;y<height;y++)
	    for(x=0;x<width;x++)
		*(frame4[0]+x+y*width)=
		    ( *(frame2[0]+x+(y-2)*width)+
		      *(frame2[0]+x+(y-1)*width)+ 
		      *(frame2[0]+x+(y+0)*width)+ 
		      *(frame2[0]+x+(y+1)*width)+ 
		      *(frame2[0]+x+(y+2)*width)  )/5;

	for(y=1;y<height;y++)
	    for(x=0;x<width;x++)
		*(frame5[0]+x+y*width)=
		    ( *(frame1[0]+x+(y-2)*width)+
		      *(frame1[0]+x+(y-1)*width)+ 
		      *(frame1[0]+x+(y+0)*width)+ 
		      *(frame1[0]+x+(y+1)*width)+ 
		      *(frame1[0]+x+(y+2)*width)  )/5;
	}

#if 1
	// motion compensate the true 2nd field to the interpolated one
	{ 
	    int vx,vy,tx,ty;
	    int x,y,xx,yy;
	    uint32_t SAD;
	    uint32_t sum_SAD;
	    int addr1=0;
	    int addr2=0;

	    sum_SAD=0;
	    for(yy=0;yy<height;yy+=16)
		for(xx=0;xx<width;xx+=16)
		{
		    vlist[0].x=0;
		    vlist[0].y=0;
		    vlist[0].SAD=0xffffff;
		    vlist[1].x=0;
		    vlist[1].y=0;
		    vlist[1].SAD=0xffffff;

		    for(vy=-8;vy<8;vy++)
			for(vx=-8;vx<8;vx++)
			{
			    addr1=(xx   )+(yy   )*width;
			    addr2=(xx+vx)+(yy+vy)*width;
			    SAD=calc_SAD_mmxe(frame5[0]+addr1,frame4[0]+addr2);

			    if(SAD<=vlist[0].SAD)
			    {
				vlist[1]=vlist[0];
				vlist[0].SAD=SAD;
				vlist[0].x=vx;
				vlist[0].y=vy;
			    }  
			}

		    if(vlist[0].SAD==vlist[1].SAD)
		    {
			tx =( vlist[0].x + vlist[1].x )/2;
			ty =( vlist[0].y + vlist[1].y )/2;
			SAD=( vlist[0].SAD + vlist[1].SAD )>>1;
		    }
		    else
		    {
			tx=vlist[0].x;
			ty=vlist[0].y;
			SAD=vlist[0].SAD;
		    }

		    sum_SAD+=SAD;

		    // transform the sourceblock by that vector if match is good
		    // else use interpolation
		    if(SAD<=mean_SAD/2)
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
				    (*(frame1[0]+(xx+x)+(yy+y+1)*width)>>1)+
				    (*(frame1[0]+(xx+x)+(yy+y-1)*width)>>1);
			    }			
		}
	    sum_SAD  = sum_SAD*256/width/height;
	    mean_SAD = sum_SAD>mean_SAD? sum_SAD:mean_SAD;
	    mean_SAD *= 80;
	    mean_SAD /= 100;
	    mean_SAD = mean_SAD <= (3*16*16)? (3*16*16):mean_SAD;

	    fprintf(stderr,"%d\n",mean_SAD);
	}
	
#endif

#if 1	
	{
	    int x,y;

	    for(y=0;y<height;y+=2)
		for(x=0;x<width;x++)
		{
		    *(frame1[0]+x+y*width)=
			*(frame3[0]+x+y*width);
		}
	}
#endif

	/* write output-frame */
	y4m_write_frame ( fd_out, 
			  &streaminfo, 
			  &frameinfo, 
			  frame1);

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




