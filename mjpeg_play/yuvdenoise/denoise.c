#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <unistd.h>
#include <stdlib.h>
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
  SS_H=denoiser.frame.w/denoiser.frame.Cw;
  SS_V=denoiser.frame.h/denoiser.frame.Ch;

  for(dy=BUF_OFF;dy<(BY0+BUF_OFF);dy++)
    for(dx=0;dx<W;dx++)
    {
      *(denoiser.frame.avg2[0]+dx+dy*W)=16;
      *(denoiser.frame.avg2[1]+dx/SS_H+dy/SS_V*W2)=128;
      *(denoiser.frame.avg2[2]+dx/SS_H+dy/SS_V*W2)=128;
    }
    
  for(dy=(BY1+BUF_OFF);dy<(H+BUF_OFF);dy++)
    for(dx=0;dx<W;dx++)
    {
      *(denoiser.frame.avg2[0]+dx+dy*W)=16;
      *(denoiser.frame.avg2[1]+dx/SS_H+dy/SS_V*W2)=128;
      *(denoiser.frame.avg2[2]+dx/SS_H+dy/SS_V*W2)=128;
    }
    
  for(dy=BUF_OFF;dy<(H+BUF_OFF);dy++)
    for(dx=0;dx<BX0;dx++)
    {
      *(denoiser.frame.avg2[0]+dx+dy*W)=16;
      *(denoiser.frame.avg2[1]+dx/SS_H+dy/SS_V*W2)=128;
      *(denoiser.frame.avg2[2]+dx/SS_H+dy/SS_V*W2)=128;
    }

  for(dy=BUF_OFF;dy<(H+BUF_OFF);dy++)
    for(dx=BX1;dx<W;dx++)
    {
      *(denoiser.frame.avg2[0]+dx+dy*W)=16;
      *(denoiser.frame.avg2[1]+dx/SS_H+dy/SS_V*W2)=128;
      *(denoiser.frame.avg2[2]+dx/SS_H+dy/SS_V*W2)=128;
    }

}

/* lookup table implimentation of luma_contrast */
/* idea from yuvcorrect */

void init_luma_contrast_vector (void)
{
  int c;
  int value;

  for(c=0;c<256;c++)
    {
      value=c;
	  
      value-=128;
      value*=denoiser.luma_contrast;
      value=(value+50)/100;
      value+=128;
	  
      value=(value>Y_HI_LIMIT)? Y_HI_LIMIT:value;
      value=(value<Y_LO_LIMIT)? Y_LO_LIMIT:value;
	  
      luma_contrast_vector[c]=value;
  }
}  

void contrast_frame (void)
{
  register int c;
  int value;
  uint8_t * p;

  p=denoiser.frame.ref[Yy]+BUF_OFF*W;

  if(denoiser.luma_contrast!=100)
    {
      for(c=0;c<(W*H);c++)
	{
	  *(p+c)=luma_contrast_vector[*(p+c)];
	}
    }

  p=denoiser.frame.ref[Cr]+BUF_OFF/SS_V*W2;
 
  if(denoiser.chroma_contrast!=100)
    {
      for(c=0;c<(W2*H2);c++)
	{
	  value=*(p);
	  
	  value-=128;
	  value*=denoiser.chroma_contrast;
	  value=(value+50)/100;
	  value+=128;
	  
	  value=(value>C_HI_LIMIT)? C_HI_LIMIT:value;
	  value=(value<C_LO_LIMIT)? C_LO_LIMIT:value;
	  
	  *(p++)=value;
	}
      
      p=denoiser.frame.ref[Cb]+BUF_OFF/SS_V*W2;
      
      for(c=0;c<(W2*H2);c++)
	{
	  value=*(p);
	  
	  value-=128;
	  value*=denoiser.chroma_contrast;
	  value=(value+50)/100;
	  value+=128;
	  
	  value=(value>C_HI_LIMIT)? C_HI_LIMIT:value;
	  value=(value<C_LO_LIMIT)? C_LO_LIMIT:value;

	  *(p++)=value;
	}
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
      d = abs(*(dst) - *(src));
      max = (d>(2*denoiser.thresholdY/3))? max+1:max;
      src++;
      dst++;
    }
  src+=W-8;
  dst+=W-8;
  }
  
  x/=SS_H;
  y/=SS_V;
  
  src = denoiser.frame.ref[Cr] + x + y * W2;
  dst = denoiser.frame.avg[Cr] + x + y * W2;
  
  for (yy=0;yy<8/SS_V;yy++)
  {
    for (xx=0;xx<8/SS_H;xx++)
    {
      d = abs(*(dst) - *(src));
      max = (d>(2*denoiser.thresholdCr/3))? max+1:max;
      src++;
      dst++;
    }
  src+=W2-8/SS_H;
  dst+=W2-8/SS_H;
  }
  
  src = denoiser.frame.ref[Cb]+x+y*W2;
  dst = denoiser.frame.avg[Cb]+x+y*W2;
  
  for (yy=0;yy<8/SS_V;yy++)
  {
    for (xx=0;xx<8/SS_H;xx++)
    {
      d = abs(*(dst) - *(src));
      max = (d>(denoiser.thresholdCb/2))? max+1:max;
      src++;
      dst++;
    }
  src+=W2-8/SS_H;
  dst+=W2-8/SS_H;
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
  
  w /= SS_H;
  dst = denoiser.frame.tmp[1]+x/SS_H+y/SS_V*w;
  src1= denoiser.frame.avg[1]+(x+qx   )/SS_H+(y+qy   )/SS_V*w;
  src2= denoiser.frame.avg[1]+(x+qx+sx)/SS_H+(y+qy+sy)/SS_V*w;

  for (dy=0;dy<8/SS_V;dy++)
  {
    for (dx=0;dx<8/SS_H;dx++)
    {
      *(dst+dx)=(*(src1+dx)+*(src2+dx))>>1;
    }
      src1+=w;
      src2+=w;
      dst+=w;
  }
  
  dst = denoiser.frame.tmp[2]+x/SS_H+y/SS_V*w;
  src1= denoiser.frame.avg[2]+(x+qx   )/SS_H+(y+qy   )/SS_V*w;
  src2= denoiser.frame.avg[2]+(x+qx+sx)/SS_H+(y+qy+sy)/SS_V*w;

  for (dy=0;dy<8/SS_V;dy++)
  {
    for (dx=0;dx<8/SS_H;dx++)
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

static void average_frame_helper(uint8_t *dst, uint8_t *src, int wh,int delay)
{
    int c, tY=delay, tY1=delay+1;
    const unsigned fmax=65536;
    int m1=tY*fmax/tY1, m2=fmax/tY1;

    for (c = 0; c < wh; c++)
    {
        *dst = ((unsigned)( *dst*m1 + *src*m2 + fmax/2 ))/fmax;
        dst++;
        src++;
    }
}

void
average_frame (void)
{
  register uint8_t * src_Yy;
  register uint8_t * src_Cr;
  register uint8_t * src_Cb;
  register uint8_t * dst_Yy;
  register uint8_t * dst_Cr;
  register uint8_t * dst_Cb;
  int tY =denoiser.delayY;
  int tCr =denoiser.delayCr;
  int tCb =denoiser.delayCb;
  
  src_Yy=denoiser.frame.ref[Yy]+BUF_OFF*W;
  src_Cr=denoiser.frame.ref[Cr]+BUF_OFF/SS_V*W2;
  src_Cb=denoiser.frame.ref[Cb]+BUF_OFF/SS_V*W2;

  dst_Yy=denoiser.frame.tmp[Yy]+BUF_OFF*W;
  dst_Cr=denoiser.frame.tmp[Cr]+BUF_OFF/SS_V*W2;
  dst_Cb=denoiser.frame.tmp[Cb]+BUF_OFF/SS_V*W2;


  average_frame_helper(dst_Yy, src_Yy, W*H, tY);
  average_frame_helper(dst_Cr, src_Cr, W2*H2, tCr);
  average_frame_helper(dst_Cb, src_Cb, W2*H2, tCb);
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
  register int threshold = denoiser.thresholdY;

  /* Only Y Component */
  
  src[Yy]=denoiser.frame.ref[Yy]+BUF_OFF*W;
  dst[Yy]=denoiser.frame.tmp[Yy]+BUF_OFF*W;
  df1[Yy]=denoiser.frame.dif[Yy]+BUF_OFF*W;
  df2[Yy]=denoiser.frame.dif2[Yy]+BUF_OFF*W;

  /* Calc difference image */
  
  for (c = 0; c < (W*H); c++)
  {
    d = abs(*(dst[Yy]) - *(src[Yy]));
    d = (d<threshold)? 0:d;
      
    *(df1[Yy])=d;
    
    dst[Yy]++;
    src[Yy]++;
    df1[Yy]++;
  }

  /* LP-filter difference image */
  
  df1[Yy]=denoiser.frame.dif[Yy]+BUF_OFF*W;
  df2[Yy]=denoiser.frame.dif2[Yy]+BUF_OFF*W;
  
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
            *(df1[Yy] + W * +1 +1 ) + 4)/9;
      
      /* error is linear but visability is exponential */
      
      d = 4*(d*d);
      d=(d>255)? 255:d;
      
      *(df2[Yy]) = d ;
      
      df2[Yy]++;
      df1[Yy]++;
    }
}

static void make_fval(int fmax,int *fval,int dred,int ddiv)
{
    int i, f;

    for( i=0; i<256; i++ ) {
        f = (fmax*(i-dred))/ddiv;
        f = (f>fmax)? fmax:f;
        f = (f<0   )?    0:f;

        fval[i]=f;
    }
}

static void correct_frame2_helper(uint8_t *dst, uint8_t *src, int wh, int dval)
{
    int c, fval[256];
    const unsigned fmax=65536;
    
    make_fval(fmax,fval,dval,dval);

    for (c = 0; c <wh; c++)
    {
        int q, f1, f2;

        q = abs(*src-*dst);

        if (q>dval)
        {
            f1 = fval[q];
            f2 = fmax-f1;
        
            *(dst)=((unsigned)(*(src)*f1 + *(dst)*f2 + (fmax+1)/2))/fmax;
        }
        
        dst++;
        src++;
    }
}

static void correct_frame2_chroma_helper(uint8_t *dst, uint8_t *src, int wh, int w2, int dval)
{
    int c, fval[256];
    const unsigned fmax=65536, rfmax=fmax*3;
    
    make_fval(fmax,fval,dval,dval);

    for (c = 0; c <wh; c++)
    {
        int q, f1, f2;

        q = abs(*src-*dst);

        if (q>dval)
        {
            unsigned int v;

            f1 = fval[q];
            f2 = fmax-f1;
            
            v=(*(src) + *(src+w2) + *(src-w2))*f1 +  
                (*(dst) + *(dst+w2) + *(dst-w2))*f2 + rfmax/2;
            *(dst)=v/rfmax;
        }
        
        dst++;
        src++;
    }
}

void
correct_frame2 (void)
{
  uint8_t * src;
  uint8_t * dst;
  
  /* Only correct Y portion... Cr and Cb remain uncorrected and have to
   * fade to the right value in about 2..3 frames
   * This solution quite effectivly kills chroma-flicker and is nearly not 
   * perceptable despite of any other method I tried to get rid of it.
   */
  
  src=denoiser.frame.ref[Yy]+W*BUF_OFF;
  dst=denoiser.frame.tmp[Yy]+W*BUF_OFF;

  if (denoiser.thresholdY>0)
    {
        correct_frame2_helper(dst, src, W*H, denoiser.thresholdY);
    }
  else
    {
        memcpy(dst, src, W*H);
    }

  src=denoiser.frame.ref[Cr]+W2*BUF_OFF/SS_V;
  dst=denoiser.frame.tmp[Cr]+W2*BUF_OFF/SS_V;

  if (denoiser.thresholdCr>0)
    {
        if( denoiser.chroma_flicker==1 ) {
            correct_frame2_helper(dst, src, W2, denoiser.thresholdCr);
            correct_frame2_chroma_helper(dst+W2, src+W2, W2*(H2-2), W2, denoiser.thresholdCr);
            correct_frame2_helper(dst+W2*(H2-1), src+W2*(H2-1), W2, denoiser.thresholdCr);
        } else {
            correct_frame2_helper(dst, src, W2*H2, denoiser.thresholdCr);
        }
    }
  else
    {
        memcpy(dst, src, W2*H2);
    }

  src=denoiser.frame.ref[Cb]+W2*BUF_OFF/SS_V;
  dst=denoiser.frame.tmp[Cb]+W2*BUF_OFF/SS_V;

  if (denoiser.thresholdCb>0)
    {
        if( denoiser.chroma_flicker==1 ) {
            correct_frame2_helper(dst, src, W2, denoiser.thresholdCb);
            correct_frame2_chroma_helper(dst+W2, src+W2, W2*(H2-2), W2, denoiser.thresholdCb);
            correct_frame2_helper(dst+W2*(H2-1), src+W2*(H2-1), W2, denoiser.thresholdCb);
        } else {
            correct_frame2_helper(dst, src, W2*H2, denoiser.thresholdCb);
        }
    }
  else
    {
        memcpy(dst, src, W2*H2);
    }
}

static void denoise_frame_pass2_helper(uint8_t *dst,uint8_t *src,int wh,int dred,int ddiv)
{
    int i, fval[256];
    const unsigned fmax=65536;

    make_fval(fmax,fval,dred,ddiv);

    for( i=0; i<wh; i++ ) {
        int d, f1, f2;

        *dst = ( *dst * 2 + *src + 1)/3;
        d = abs(*dst-*src);
        
        f1 = fval[d];
        f2 = fmax-f1;
        *dst =((unsigned)( *src*f1 +*dst*f2 + fmax/2 ))/fmax;
        
        dst++;
        src++;
    }
}

void
denoise_frame_pass2 (void)
{
  uint8_t * src[3];
  uint8_t * dst[3];

  src[Yy]=denoiser.frame.tmp[Yy]+BUF_OFF*W;
  src[Cr]=denoiser.frame.tmp[Cr]+BUF_OFF/SS_V*W2;
  src[Cb]=denoiser.frame.tmp[Cb]+BUF_OFF/SS_V*W2;

  dst[Yy]=denoiser.frame.avg2[Yy]+BUF_OFF*W;
  dst[Cr]=denoiser.frame.avg2[Cr]+BUF_OFF/SS_V*W2;
  dst[Cb]=denoiser.frame.avg2[Cb]+BUF_OFF/SS_V*W2;

  /* blend frame with error threshold */

  if (denoiser.pp_threshold>0)
    {
      /* Y */

        denoise_frame_pass2_helper( dst[Yy], src[Yy], W*H, 0, denoiser.pp_threshold );
      
      /* Cr and Cb */
        denoise_frame_pass2_helper( dst[Cr], src[Cr], W2*H2, denoiser.pp_threshold, denoiser.pp_threshold );
        denoise_frame_pass2_helper( dst[Cb], src[Cb], W2*H2, denoiser.pp_threshold, denoiser.pp_threshold );
    }
  else
    {
        memcpy( dst[Yy], src[Yy], W*H );
        memcpy( dst[Cr], src[Cr], W2*H2 );
        memcpy( dst[Cb], src[Cb], W2*H2 );
    }
}




void
sharpen_frame(void)
{
  uint8_t * dst[3];
  register int d;
  register int m;
  register int c;

  if (denoiser.sharpen == 0)
    return;

  dst[Yy]=denoiser.frame.avg2[Yy]+BUF_OFF*W;

  /* Y */
  for (c = 0; c < (W*H); c++)
  {
    m = ( *(dst[Yy]) + *(dst[Yy]+1) + *(dst[Yy]+W) + *(dst[Yy]+1+W) + 2 )/4;
      
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
    subsample_frame (denoiser.frame.sub2ref,denoiser.frame.ref,W,H+2*BUF_OFF);
    subsample_frame (denoiser.frame.sub4ref,denoiser.frame.sub2ref,W/2,H/2+BUF_OFF);
    subsample_frame (denoiser.frame.sub2avg,denoiser.frame.avg,W,H+2*BUF_OFF);
    subsample_frame (denoiser.frame.sub4avg,denoiser.frame.sub2avg,W/2,H/2+BUF_OFF);
  
    for(y=BUF_OFF;y<(denoiser.frame.h+BUF_OFF);y+=8)
      for(x=0;x<denoiser.frame.w;x+=8)
      {
        vector.x=0;
        vector.y=0;

//        if( !low_contrast_block(x,y) && 
//          x>(denoiser.border.x) && y>(denoiser.border.y+BUF_OFF) &&
//          x<(denoiser.border.x+denoiser.border.w) && y<(denoiser.border.y+BUF_OFF+denoiser.border.h) 
//          )        
        {
        mb_search_44(x,y);
        mb_search_22(x,y);
        mb_search_11(x,y);
        mb_search_00(x,y);
        }
      
        if  ( (vector.x+x)>0 && 
	            (vector.x+x)<W &&
	            (vector.y+y)>BUF_OFF && 
	            (vector.y+y)<(BUF_OFF+H) )
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
    sharpen_frame();
    black_border();
    
    memcpy(denoiser.frame.avg[0],denoiser.frame.tmp[0],denoiser.frame.w*(denoiser.frame.h+2*BUF_OFF));
    memcpy(denoiser.frame.avg[1],denoiser.frame.tmp[1],denoiser.frame.Cw*(denoiser.frame.Ch+2*BUF_COFF));
    memcpy(denoiser.frame.avg[2],denoiser.frame.tmp[2],denoiser.frame.Cw*(denoiser.frame.Ch+2*BUF_COFF));
    break;
    }
    
    case 1: /* interlaced mode */
    {
      /* Generate subsampled images */
      subsample_frame (denoiser.frame.sub2ref,denoiser.frame.ref,W,H+2*BUF_OFF);
      subsample_frame (denoiser.frame.sub4ref,denoiser.frame.sub2ref,W/2,H/2+BUF_OFF);
      subsample_frame (denoiser.frame.sub2avg,denoiser.frame.avg,W,H+2*BUF_OFF);
      subsample_frame (denoiser.frame.sub4avg,denoiser.frame.sub2avg,W/2,H/2+BUF_OFF);
  
      /* process the fields as two separate images */
      denoiser.frame.h /= 2;
      denoiser.frame.w *= 2;
      denoiser.frame.Ch /= 2;
      denoiser.frame.Cw *= 2;
      
      /* if lines are twice as wide as normal the offset is only BUF_OFF/2 lines
       * despite BUF_OFF in progressive mode...
       */
      for(y=BUF_OFF/2;y<(denoiser.frame.h+BUF_OFF/2);y+=8)
        for(x=0;x<denoiser.frame.w;x+=8)
        {
          vector.x=0;
          vector.y=0;

          if(!low_contrast_block(x,y) && 
            x>(denoiser.border.x) && y>(denoiser.border.y+BUF_OFF/2) &&
            x<(denoiser.border.x+denoiser.border.w) && y<(denoiser.border.y+BUF_OFF/2+denoiser.border.h) 
            )        
          {
          mb_search_44(x,y);
          mb_search_22(x,y);
          mb_search_11(x,y);
          mb_search_00(x,y);
          }
      
          if  ( (vector.x+x)>0 && 
	            (vector.x+x)<W &&
	            (vector.y+y)>BUF_OFF/2 && 
	            (vector.y+y)<(BUF_OFF/2+H) )
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
      denoiser.frame.Ch *= 2;
      denoiser.frame.Cw /= 2;
      
      average_frame();
      correct_frame2();
      denoise_frame_pass2();
      sharpen_frame();
      black_border();
    
      memcpy(denoiser.frame.avg[0],denoiser.frame.tmp[0],denoiser.frame.w*(denoiser.frame.h+2*BUF_OFF));
      memcpy(denoiser.frame.avg[1],denoiser.frame.tmp[1],denoiser.frame.Cw*(denoiser.frame.Ch+2*BUF_OFF));
      memcpy(denoiser.frame.avg[2],denoiser.frame.tmp[2],denoiser.frame.Cw*(denoiser.frame.Ch+2*BUF_OFF));
      break;
    }
    
    case 2: /* PASS II only mode */
    {      
      /* deinterlacing wanted ? */
      if (denoiser.deinterlace) deinterlace();
      
      /* as the normal denoising functions are not used we need to copy ... */
      memcpy(denoiser.frame.tmp[0],denoiser.frame.ref[0],denoiser.frame.w*(denoiser.frame.h+2*BUF_OFF));
      memcpy(denoiser.frame.tmp[1],denoiser.frame.ref[1],denoiser.frame.Cw*(denoiser.frame.Ch+2*BUF_OFF));
      memcpy(denoiser.frame.tmp[2],denoiser.frame.ref[2],denoiser.frame.Cw*(denoiser.frame.Ch+2*BUF_OFF));
      
      denoise_frame_pass2();
      sharpen_frame();
      black_border();
      
      break;
    }
  }
}
