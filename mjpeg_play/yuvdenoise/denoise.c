#include <unistd.h>
#include <string.h>
#include <math.h>
#include "mjpeg_types.h"
#include "motion.h"
#include "deinterlace.h"
#include "global.h"
#include "stdio.h"
#include "denoise.h"

extern struct DNSR_GLOBAL denoiser;
extern struct DNSR_VECTOR vector;
extern struct DNSR_VECTOR varray44[8];
extern struct DNSR_VECTOR varray22[8];

/* pointer on optimized deinterlacer 
 * defined in deinterlace.c
 */
extern void (*deinterlace) (void);

void black_border (void)
{
  int dx,dy;
  int BX0,BX1;
  int BY0,BY1;
  
  BX0=denoiser.border.x;
  BY0=denoiser.border.y;
  BX1=BX0+denoiser.border.w;
  BY1=BY0+denoiser.border.h;

  for(dy=32;dy<(BY0+32);dy++)
    for(dx=0;dx<W;dx++)
    {
      *(denoiser.frame.avg2[0]+dx+dy*W)=16;
      *(denoiser.frame.avg2[1]+dx/2+dy/2*W2)=128;
      *(denoiser.frame.avg2[2]+dx/2+dy/2*W2)=128;
    }
    
  for(dy=(BY1+32);dy<(H+32);dy++)
    for(dx=0;dx<W;dx++)
    {
      *(denoiser.frame.avg2[0]+dx+dy*W)=16;
      *(denoiser.frame.avg2[1]+dx/2+dy/2*W2)=128;
      *(denoiser.frame.avg2[2]+dx/2+dy/2*W2)=128;
    }
    
  for(dy=32;dy<(H+32);dy++)
    for(dx=0;dx<BX0;dx++)
    {
      *(denoiser.frame.avg2[0]+dx+dy*W)=16;
      *(denoiser.frame.avg2[1]+dx/2+dy/2*W2)=128;
      *(denoiser.frame.avg2[2]+dx/2+dy/2*W2)=128;
    }

  for(dy=32;dy<(H+32);dy++)
    for(dx=BX1;dx<W;dx++)
    {
      *(denoiser.frame.avg2[0]+dx+dy*W)=16;
      *(denoiser.frame.avg2[1]+dx/2+dy/2*W2)=128;
      *(denoiser.frame.avg2[2]+dx/2+dy/2*W2)=128;
    }

}

void contrast_frame (void)
{
  register int c;
  int value;
  uint8_t * p;

  p=denoiser.frame.ref[Yy]+32*W;

  for(c=0;c<(W*H);c++)
  {
  	value=*(p);

	  value-=128;
	  value*=denoiser.luma_contrast;
    value/=100;
	  value+=128;

	  value=(value>Y_HI_LIMIT)? Y_HI_LIMIT:value;
	  value=(value<Y_LO_LIMIT)? Y_LO_LIMIT:value;

	  *(p++)=value;
  }

  p=denoiser.frame.ref[Cr]+16*W2;
 
  for(c=0;c<(W2*H2);c++)
  {
  	value=*(p);

	  value-=128;
	  value*=denoiser.chroma_contrast;
    value/=100;
	  value+=128;

	  value=(value>C_HI_LIMIT)? C_HI_LIMIT:value;
	  value=(value<C_LO_LIMIT)? C_LO_LIMIT:value;

	  *(p++)=value;
  }
  
  p=denoiser.frame.ref[Cb]+16*W2;
 
  for(c=0;c<(W2*H2);c++)
  {
  	value=*(p);

	  value-=128;
	  value*=denoiser.chroma_contrast;
    value/=100;
	  value+=128;

	  value=(value>C_HI_LIMIT)? C_HI_LIMIT:value;
	  value=(value<C_LO_LIMIT)? C_LO_LIMIT:value;

	  *(p++)=value;
  }
  
}

int
low_contrast_block (int x, int y)
{
  /* Only do a motion search in blocks where at least eight pixels do
   * differ more than 2/3 of our noise threshold in either color or luma
   */
  
  int xx;
  int yy;
  int max=0;
  int d;
  
  uint8_t * src = denoiser.frame.ref[Yy]+x+y*W;
  uint8_t * dst = denoiser.frame.avg[Yy]+x+y*W;
  
  for (yy=0;yy<8;yy++)
  {
    for (xx=0;xx<8;xx++)
    {
      d = *(dst) - *(src);
      d = (d<0)? -d:d;
      
      max = (d>(2*denoiser.threshold/3))? max+1:max;
      
      src++;
      dst++;
    }
  src+=W-8;
  dst+=W-8;
  }
  
  x/=2;
  y/=2;
  
  src = denoiser.frame.ref[Cr] + x + y * W2;
  dst = denoiser.frame.avg[Cr] + x + y * W2;
  
  for (yy=0;yy<4;yy++)
  {
    for (xx=0;xx<4;xx++)
    {
      d = *(dst) - *(src);
      d = (d<0)? -d:d;
      
      max = (d>(2*denoiser.threshold/3))? max+1:max;
      
      src++;
      dst++;
    }
  src+=W2-4;
  dst+=W2-4;
  }
  
  src = denoiser.frame.ref[Cb]+x+y*W2;
  dst = denoiser.frame.avg[Cb]+x+y*W2;
  
  for (yy=0;yy<4;yy++)
  {
    for (xx=0;xx<4;xx++)
    {
      d = *(dst) - *(src);
      d = (d<0)? -d:d;
      
      max = (d>(denoiser.threshold/2))? max+1:max;
      
      src++;
      dst++;
    }
  src+=W2-4;
  dst+=W2-4;
  }
  
  return ((max>8)? 0:1);
}

void
move_block (int x, int y)
{
  int qx = vector.x/2;
  int qy = vector.y/2;
  int sx = vector.x-(qx<<1);
  int sy = vector.y-(qy<<1);
  int dx,dy;
  uint16_t w = denoiser.frame.w;

  uint8_t * dst;
  uint8_t * src1;
  uint8_t * src2;
  
  dst = denoiser.frame.tmp[0]+x+y*denoiser.frame.w;
  src1= denoiser.frame.avg[0]+(x+qx   )+(y+qy   )*denoiser.frame.w;
  src2= denoiser.frame.avg[0]+(x+qx+sx)+(y+qy+sy)*denoiser.frame.w;
  
  for (dy=0;dy<8;dy++)
  {
    for (dx=0;dx<8;dx++)
    {
      *(dst+dx)=(*(src1+dx)+*(src2+dx))>>1;
    }
      src1+=w;
      src2+=w;
      dst+=w;
  }
  
  w /= 2;
  dst = denoiser.frame.tmp[1]+x/2+y/2*w;
  src1= denoiser.frame.avg[1]+(x+qx   )/2+(y+qy   )/2*w;
  src2= denoiser.frame.avg[1]+(x+qx+sx)/2+(y+qy+sy)/2*w;

  for (dy=0;dy<4;dy++)
  {
    for (dx=0;dx<4;dx++)
    {
      *(dst+dx)=(*(src1+dx)+*(src2+dx))>>1;
    }
      src1+=w;
      src2+=w;
      dst+=w;
  }
  
  dst = denoiser.frame.tmp[2]+x/2+y/2*w;
  src1= denoiser.frame.avg[2]+(x+qx   )/2+(y+qy   )/2*w;
  src2= denoiser.frame.avg[2]+(x+qx+sx)/2+(y+qy+sy)/2*w;

  for (dy=0;dy<4;dy++)
  {
    for (dx=0;dx<4;dx++)
    {
      *(dst+dx)=(*(src1+dx)+*(src2+dx))>>1;
    }
      src1+=w;
      src2+=w;
      dst+=w;
  }
  
}  

/*****************************************************************************
 * takes a frame and blends it into the average                              *
 * buffer.                                                                   *
 * The blend may not be to slow, as noise wouldn't                           *
 * be filtered good than, but it may not be to fast                          *
 * either...                                                                 *
 *****************************************************************************/

void
average_frame (void)
{
  register uint8_t * src_Yy;
  register uint8_t * src_Cr;
  register uint8_t * src_Cb;
  register uint8_t * dst_Yy;
  register uint8_t * dst_Cr;
  register uint8_t * dst_Cb;
  int t =denoiser.delay;
  int t1=denoiser.delay+1;
  int c;
  
  src_Yy=denoiser.frame.ref[Yy]+32*W;
  src_Cr=denoiser.frame.ref[Cr]+16*W2;
  src_Cb=denoiser.frame.ref[Cb]+16*W2;

  dst_Yy=denoiser.frame.tmp[Yy]+32*W;
  dst_Cr=denoiser.frame.tmp[Cr]+16*W2;
  dst_Cb=denoiser.frame.tmp[Cb]+16*W2;

  for (c = 0; c < (W*H); c++)
  {
    *(dst_Yy) = ( *(dst_Yy) * t + *(src_Yy))/(t1);
    dst_Yy++;
    src_Yy++;
  }      
  
  for (c = 0; c < (W2*H2); c++)
  {
    *(dst_Cr) = ( *(dst_Cr) * t + *(src_Cr) )/(t1);
    dst_Cr++;
    src_Cr++;
  }
  
  for (c = 0; c < (W2*H2); c++)
  {
    *(dst_Cr) = ( *(dst_Cr) * t + *(src_Cr) )/(t1);
    *(dst_Cb) = ( *(dst_Cb) * t + *(src_Cb) )/(t1);
    dst_Cb++;
    src_Cb++;
  }
}


void
difference_frame (void)
{
  uint8_t * src[3];
  uint8_t * dst[3];
  uint8_t * df1[3];
  uint8_t * df2[3];
  register int c;
  register int d;
  register int threshold = denoiser.threshold;

  /* Only Y Component */
  
  src[Yy]=denoiser.frame.ref[Yy]+32*W;
  dst[Yy]=denoiser.frame.tmp[Yy]+32*W;
  df1[Yy]=denoiser.frame.dif[Yy]+32*W;
  df2[Yy]=denoiser.frame.dif2[Yy]+32*W;

  /* Calc difference image */
  
  for (c = 0; c < (W*H); c++)
  {
    d = *(dst[Yy]) - *(src[Yy]);
    d = (d<0)? -d:d;
    d = (d<threshold)? 0:d;
      
    *(df1[Yy])=d;
    
    dst[Yy]++;
    src[Yy]++;
    df1[Yy]++;
  }

  /* LP-filter difference image */
  
  df1[Yy]=denoiser.frame.dif[Yy]+32*W;
  df2[Yy]=denoiser.frame.dif2[Yy]+32*W;
  
  for (c = 0; c < (W*H); c++)
    {
      d = ( *(df1[Yy] + W * -1 -1 ) +
            *(df1[Yy] + W * -1 +0 ) +
            *(df1[Yy] + W * -1 +1 ) +
            *(df1[Yy] + W * +0 -1 ) +
            *(df1[Yy] + W * +0 +0 ) +
            *(df1[Yy] + W * +0 +1 ) +
            *(df1[Yy] + W * +1 -1 ) +
            *(df1[Yy] + W * +1 +0 ) +
            *(df1[Yy] + W * +1 +1 ) )/9;
      
      /* error is linear but visability is exponential */
      
      d = 4*(d*d);
      d=(d>255)? 255:d;
      
      *(df2[Yy]) = d ;
      
      df2[Yy]++;
      df1[Yy]++;
    }
}

void
correct_frame2 (void)
{
  uint8_t * src;
  uint8_t * dst;
  uint8_t * dif;
  int q;
  int c;
  //register int m;
  int f1=0;
  int f2=0;
  
  /* Only correct Y portion... Cr and Cb remain uncorrected and have to
   * fade to the right value in about 2..3 frames
   * This solution quite effectivly kills chroma-flicker and is nearly not 
   * perceptable despite of any other method I tried to get rid of it.
   */
  
  src=denoiser.frame.ref[Yy]+W*32;
  dst=denoiser.frame.tmp[Yy]+W*32;
  dif=denoiser.frame.dif2[Yy]+W*32;

  for (c = 0; c < (W*H); c++)
  {
    q = *(src)-*(dst);
    q = (q<0)? -q:q;

    f1 = (255*(q-denoiser.threshold))/denoiser.threshold;
    f1 = (f1>255)? 255:f1;
    f1 = (f1<0)?     0:f1;
    f2 = 255-f1;

    if (q>denoiser.threshold)
    {
	*(dst)=(*(dst)*f2 + *(src)*f1)/255;;
    }

    dst++;
    src++;
  }

  src=denoiser.frame.ref[Cr]+W2*16;
  dst=denoiser.frame.tmp[Cr]+W2*16;
  
  for (c = 0; c < (W2*H2); c++)
  {
    q = *(src) - *(dst) ;
    q = (q<0)? -q:q;
    
    f1 = (255*(q-denoiser.threshold))/denoiser.threshold;
    f1 = (f1>255)? 255:f1;
    f1 = (f1<0)?     0:f1;
    f2 = 255-f1;

    if (q>denoiser.threshold)
    {
	if(c>W2 && c<(W2*H2-W2))
	{
	    *(dst)=( (*(src) + *(src+W2) + *(src-W2))*f1/3 +  
		     (*(dst) + *(dst+W2) + *(dst-W2))*f2/3 )/255;
	}
	else
	{
	    *(dst)=(*(dst)*f2 + *(src)*f1)/255;;
	}
    }

    dst++;
    src++;
  }

  src=denoiser.frame.ref[Cb]+W2*16;
  dst=denoiser.frame.tmp[Cb]+W2*16;
  
  for (c = 0; c < (W2*H2); c++)
  {
    q = *(src) - *(dst) ;
    q = (q<0)? -q:q;
    
    f1 = (255*(q-denoiser.threshold))/denoiser.threshold;
    f1 = (f1>255)? 255:f1;
    f1 = (f1<0)?     0:f1;
    f2 = 255-f1;

    if (q>denoiser.threshold)
    {
	if(c>W2 && c<(W2*H2-W2))
	{
	    *(dst)=( (*(src) + *(src+W2) + *(src-W2))*f1/3 +  
		     (*(dst) + *(dst+W2) + *(dst-W2))*f2/3 )/255;
	}
	else
	{
	    *(dst)=(*(dst)*f2 + *(src)*f1)/255;;
	}
    }

    dst++;
    src++;
  }
}

void
denoise_frame_pass2 (void)
{
  uint8_t * src[3];
  uint8_t * dst[3];
  register int d;
  register int c;
  int f1=0;
  int f2=0;

  src[Yy]=denoiser.frame.tmp[Yy]+32*W;
  src[Cr]=denoiser.frame.tmp[Cr]+16*W2;
  src[Cb]=denoiser.frame.tmp[Cb]+16*W2;

  dst[Yy]=denoiser.frame.avg2[Yy]+32*W;
  dst[Cr]=denoiser.frame.avg2[Cr]+16*W2;
  dst[Cb]=denoiser.frame.avg2[Cb]+16*W2;

  /* blend frame with error threshold */

  /* Y */
  for (c = 0; c < (W*H); c++)
  {
    *(dst[Yy]) = ( *(dst[Yy]) * 2 + *(src[Yy]) )/3;
      
    d = *(dst[Yy])-*(src[Yy]);
    d = (d<0)? -d:d;

    f1 = (255*d)/denoiser.pp_threshold;
    f1 = (f1>255)? 255:f1;
    f1 = (f1<0)?     0:f1;
    f2 = 255-f1;
    
    *(dst[Yy]) =( *(src[Yy])*f1 +*(dst[Yy])*f2 )/255;

    dst[Yy]++;
    src[Yy]++;
  }
  
  /* Cr and Cb */
  for (c = 0; c < (W2*H2); c++)
  {
    *(dst[Cr]) = ( *(dst[Cr]) * 2 + *(src[Cr]) )/3;      
    d = *(dst[Cr])-*(src[Cr]);
    d = (d<0)? -d:d;
    
    f1 = (255*(d-denoiser.pp_threshold))/denoiser.pp_threshold;
    f1 = (f1>255)? 255:f1;
    f1 = (f1<0)?     0:f1;
    f2 = 255-f1;
    *(dst[Cr]) =( *(src[Cr])*f1 +*(dst[Cr])*f2 )/255;


    *(dst[Cb]) = ( *(dst[Cb]) * 2 + *(src[Cb]) )/3;      
    d = *(dst[Cb])-*(src[Cb]);
    d = (d<0)? -d:d;
    
    f1 = (255*(d-denoiser.pp_threshold))/denoiser.pp_threshold;
    f1 = (f1>255)? 255:f1;
    f1 = (f1<0)?     0:f1;
    f2 = 255-f1;
    *(dst[Cb]) =( *(src[Cb])*f1 +*(dst[Cb])*f2 )/255;

    dst[Cr]++;
    src[Cr]++;
    dst[Cb]++;
    src[Cb]++;
  }
    
}




void
sharpen_frame(void)
{
  uint8_t * dst[3];
  register int d;
  register int m;
  register int c;

  dst[Yy]=denoiser.frame.avg2[Yy]+32*W;

  /* Y */
  for (c = 0; c < (W*H); c++)
  {
    m = ( *(dst[Yy]) + *(dst[Yy]+1) + *(dst[Yy]+W) + *(dst[Yy]+1+W) ) /4;
      
    d = *(dst[Yy]) - m;
    
    d *= denoiser.sharpen;
    d /= 100;
    
    m = m+d;
    m = (m>Y_HI_LIMIT)? Y_HI_LIMIT:m;
    m = (m<Y_LO_LIMIT)? Y_LO_LIMIT:m;
    
    *(dst[Yy]) = m;
    dst[Yy]++;
  }    
}

void
denoise_frame(void)
{
  uint16_t x,y;

  /* adjust contrast for luma and chroma */
  contrast_frame();

  switch(denoiser.mode)
  {
    
    case 0: /* progressive mode */
    {
    /* deinterlacing wanted ? */
    if(denoiser.deinterlace) deinterlace();
      
    /* Generate subsampled images */
    subsample_frame (denoiser.frame.sub2ref,denoiser.frame.ref);
    subsample_frame (denoiser.frame.sub4ref,denoiser.frame.sub2ref);
    subsample_frame (denoiser.frame.sub2avg,denoiser.frame.avg);
    subsample_frame (denoiser.frame.sub4avg,denoiser.frame.sub2avg);
  
    for(y=32;y<(denoiser.frame.h+32);y+=8)
      for(x=0;x<denoiser.frame.w;x+=8)
      {
        vector.x=0;
        vector.y=0;

        if( !low_contrast_block(x,y) && 
          x>(denoiser.border.x) && y>(denoiser.border.y+32) &&
          x<(denoiser.border.x+denoiser.border.w) && y<(denoiser.border.y+32+denoiser.border.h) 
          )        
        {
        mb_search_44(x,y);
        mb_search_22(x,y);
        mb_search_11(x,y);
        mb_search_00(x,y);
        }
      
        if  ( (vector.x+x)>0 && 
	            (vector.x+x)<W &&
	            (vector.y+y)>32 && 
	            (vector.y+y)<(32+H) )
        {
    	    move_block(x,y);
        }
        else
        {
          vector.x=0;
          vector.y=0;
    	    move_block(x,y);
        }
      }
    average_frame();
    correct_frame2();
    denoise_frame_pass2();
    //sharpen_frame();
    black_border();
    
    memcpy(denoiser.frame.avg[0],denoiser.frame.tmp[0],denoiser.frame.w*(denoiser.frame.h+64));
    memcpy(denoiser.frame.avg[1],denoiser.frame.tmp[1],denoiser.frame.w*(denoiser.frame.h+64)/4);
    memcpy(denoiser.frame.avg[2],denoiser.frame.tmp[2],denoiser.frame.w*(denoiser.frame.h+64)/4);
    break;
    }
    
    case 1: /* interlaced mode */
    {
      /* Generate subsampled images */
      subsample_frame (denoiser.frame.sub2ref,denoiser.frame.ref);
      subsample_frame (denoiser.frame.sub4ref,denoiser.frame.sub2ref);
      subsample_frame (denoiser.frame.sub2avg,denoiser.frame.avg);
      subsample_frame (denoiser.frame.sub4avg,denoiser.frame.sub2avg);
  
      /* process the fields as two seperate images */
      denoiser.frame.h /= 2;
      denoiser.frame.w *= 2;
      
      /* if lines are twice as wide as normal the offset is only 16 lines
       * despite 32 in progressive mode...
       */
      for(y=16;y<(denoiser.frame.h+16);y+=8)
        for(x=0;x<denoiser.frame.w;x+=8)
        {
          vector.x=0;
          vector.y=0;

          if(!low_contrast_block(x,y) && 
            x>(denoiser.border.x) && y>(denoiser.border.y+32) &&
            x<(denoiser.border.x+denoiser.border.w) && y<(denoiser.border.y+32+denoiser.border.h) 
            )        
          {
          mb_search_44(x,y);
          mb_search_22(x,y);
          mb_search_11(x,y);
          mb_search_00(x,y);
          }
      
          if  ( (vector.x+x)>0 && 
	            (vector.x+x)<W &&
	            (vector.y+y)>32 && 
	            (vector.y+y)<(32+H) )
          {
    	      move_block(x,y);
          }
          else
          {
            vector.x=0;
            vector.y=0;
    	      move_block(x,y);
          }
        }
        
      /* process the fields in one image again */
      denoiser.frame.h *= 2;
      denoiser.frame.w /= 2;
      
      average_frame();
      correct_frame2();
      denoise_frame_pass2();
      sharpen_frame();
      black_border();
    
      memcpy(denoiser.frame.avg[0],denoiser.frame.tmp[0],denoiser.frame.w*(denoiser.frame.h+64));
      memcpy(denoiser.frame.avg[1],denoiser.frame.tmp[1],denoiser.frame.w*(denoiser.frame.h+64)/4);
      memcpy(denoiser.frame.avg[2],denoiser.frame.tmp[2],denoiser.frame.w*(denoiser.frame.h+64)/4);
      break;
    }
    
    case 2: /* PASS II only mode */
    {      
      /* deinterlacing wanted ? */
      if(denoiser.deinterlace) deinterlace();
      
      /* as the normal denoising functions are not used we need to copy ... */
      memcpy(denoiser.frame.tmp[0],denoiser.frame.ref[0],denoiser.frame.w*(denoiser.frame.h+64));
      memcpy(denoiser.frame.tmp[1],denoiser.frame.ref[1],denoiser.frame.w*(denoiser.frame.h+64)/4);
      memcpy(denoiser.frame.tmp[2],denoiser.frame.ref[2],denoiser.frame.w*(denoiser.frame.h+64)/4);
      
      denoise_frame_pass2();
      sharpen_frame();
      black_border();
      
      break;
    }
  }
}
