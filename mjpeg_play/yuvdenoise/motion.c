#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "mjpeg_types.h"
#include "global.h"
#include "motion.h"

/* pointers on SAD functions */
uint32_t (*calc_SAD)         (uint8_t * , uint8_t * );
uint32_t (*calc_SAD_uv)      (uint8_t * , uint8_t * );
uint32_t (*calc_SAD_half)    (uint8_t * , uint8_t * ,uint8_t *);

/* global denoiser structure defined in main.c and global.h */
extern struct DNSR_GLOBAL denoiser;

struct DNSR_VECTOR vector;
struct DNSR_VECTOR varray44[8];
struct DNSR_VECTOR varray22[8];

/*****************************************************************************
 * generate a lowpassfiltered and subsampled copy                            *
 * of the source image (src) at the destination                              *
 * image location.                                                           *
 * Lowpass-filtering is important, as this subsampled                        *
 * image is used for motion estimation and the result                        *
 * is more reliable when filtered.                                           *
 * only subsample actual data, but keeping full buffer size for simplicity   *
 *****************************************************************************/

void
subsample_frame (uint8_t * dst[3], uint8_t * src[3], int w, int h)
{
  int x, y;
  int Cw=w/SS_H;
  int Ch=h/SS_V;
 
  uint8_t *s  = src[0];
  uint8_t *s2 = src[0]+W;
  uint8_t *d  = dst[0];

  /* subsample Y component */

  for (y = 0; y < (h>>1); y += 1)
  {
      for (x = 0; x < w; x += 2)
      {
	  *(d + (x>>1)) =
	      (
		  *(s  + x    ) +
		  *(s  + x + 1) + 
		  *(s2 + x    ) +
		  *(s2 + x + 1) + 2 )>>2;
      }
      s+=2*W;
      s2+=2*W;
      d+=W;
  }
#if 1
  /* subsample U and V components */

  s  = src[1];
  s2 = src[1]+W2;
  d  = dst[1];

  for (y = 0; y < (Ch>>1); y += 1)
  {
      for (x = 0; x < Cw; x += 2)
      {
	  *(d + (x>>1)) = 
	      (
		  *(s  + x    ) +
		  *(s  + x + 1) + 
		  *(s2 + x    ) +
		  *(s2 + x + 1) +2 )>>2;
      }
      s+=2*W2;
      s2+=2*W2;
      d+=W2;
  }

  s  = src[2];
  s2 = src[2]+W2;
  d  = dst[2];

  for (y = 0; y < (Ch>>1); y += 1)
  {
      for (x = 0; x < Cw; x += 2)
      {
	  *(d + (x>>1)) = 
	      (
		  *(s  + x    ) +
		  *(s  + x + 1) + 
		  *(s2 + x    ) +
		  *(s2 + x + 1) + 2 )>>2;
      }
      s+=2*W2;
      s2+=2*W2;
      d+=W2;
  }
#endif
}


/*********************************************************************
 *                                                                   *
 * SAD-function for Y without MMX/MMXE                               *
 *                                                                   *
 *********************************************************************/

uint32_t
calc_SAD_noaccel (uint8_t * frm, uint8_t * ref)
{
  uint8_t dx = 0;
  uint8_t dy = 0;
  int32_t Y = 0;
  uint32_t d = 0;
  
  for(dy=0;dy<8;dy++)
    for(dx=0;dx<8;dx++)
    {
      Y=*(frm+dx+dy*denoiser.frame.w)-*(ref+dx+dy*denoiser.frame.w);
      d+=(Y<0)? -Y:Y;
    }
  return d;
}

/*********************************************************************
 *                                                                   *
 * SAD-function for Y with MMX                                       *
 *                                                                   *
 *********************************************************************/

uint32_t
calc_SAD_mmx (uint8_t * frm, uint8_t * ref)
{
  static uint16_t a[4];
  
#ifdef HAVE_ASM_MMX
  __asm__ __volatile__
    (
    " pxor        %%mm0 , %%mm0;           /* clear mm0                                          */\n"
    " pxor        %%mm7 , %%mm7;           /* clear mm7                                          */\n"
    "                                      /*                                                    */\n"
    " movl         %1    , %%eax;          /* load frameadress into eax                          */\n"
    " movl         %2    , %%ebx;          /* load frameadress into ebx                          */\n"
    " movl         %3    , %%ecx;          /* load width       into ecx                          */\n"
    "                                      /*                                                    */\n"
    ".rept 8                    ;          /* Loop for 8 lines                                   */\n"
    " movq        (%%eax), %%mm1;          /* 8 Pixels from filtered frame to mm1                */\n"
    " movq        (%%ebx), %%mm2;          /* 8 Pixels from reference frame to mm2               */\n"
    " movq         %%mm2 , %%mm3;          /* hold a copy of mm2 in mm3                          */\n"
    " psubusb      %%mm1 , %%mm3;          /* positive differences between mm2 and mm1           */\n"
    " psubusb      %%mm2 , %%mm1;          /* positive differences between mm1 and mm3           */\n"
    " paddusb      %%mm3 , %%mm1;          /* mm1 now contains abs(mm1-mm2)                      */\n"
    " movq         %%mm1 , %%mm2;          /* copy mm1 to mm2                                    */\n"
    " punpcklbw    %%mm7 , %%mm1;          /* unpack mm1 into mm1 and mm2                        */\n"
    " punpckhbw    %%mm7 , %%mm2;          /*                                                    */\n"
    " paddusw      %%mm1 , %%mm0;          /* add mm1 (stored in mm1 and mm2...)                 */\n"
    " paddusw      %%mm2 , %%mm0;          /* to mm0                                             */\n"
    " addl         %%ecx , %%eax;          /* add framewidth to frameaddress                     */\n"
    " addl         %%ecx , %%ebx;          /* add framewidth to frameaddress                     */\n"
    " .endr                                /* end loop                                           */\n"
    "                                      /*                                                    */\n"
    " movq         %%mm0 , %0   ;          /* make mm0 available to gcc ...                      */\n"
    :"=m" (a)     
    :"m" (frm), "m" (ref), "m" (denoiser.frame.w)
    :"%eax", "%ebx", "%ecx"
    );
#endif

  return (uint32_t)(a[0]+a[1]+a[2]+a[3]);
}

/*********************************************************************
 *                                                                   *
 * SAD-function for Y with MMXE                                      *
 *                                                                   *
 *********************************************************************/

uint32_t
calc_SAD_mmxe (uint8_t * frm, uint8_t * ref)
{
  static uint32_t a;
  
#ifdef HAVE_ASM_MMX
  __asm__ __volatile__
    (
    " pxor         %%mm0 , %%mm0;          /* clear mm0                                          */\n"
    " movl         %1    , %%eax;          /* load frameadress into eax                          */\n"
    " movl         %2    , %%ebx;          /* load frameadress into ebx                          */\n"
    " movl         %3    , %%ecx;          /* load width       into ecx                          */\n"
    "                           ;          /*                                                    */\n"
    " .rept 8                   ;          /*                                                    */\n"
    " movq        (%%eax), %%mm1;          /* 8 Pixels from filtered frame to mm1                */\n"
    " psadbw      (%%ebx), %%mm1;          /* 8 Pixels difference to mm1                         */\n"
    " paddusw      %%mm1 , %%mm0;          /* add result to mm0                                  */\n"
    " addl         %%ecx , %%eax;          /* add framewidth to frameaddress                     */\n"
    " addl         %%ecx , %%ebx;          /* add framewidth to frameaddress                     */\n"
    " .endr                     ;          /*                                                    */\n"
    "                                      /*                                                    */\n"
    " movq         %%mm0 , %0   ;          /* make mm0 available to gcc ...                      */\n"
    :"=m" (a)     
    :"m" (frm), "m" (ref), "m" (denoiser.frame.w)
    :"%eax", "%ebx", "%ecx"
    );
#endif
  return a;
}


/*********************************************************************
 *                                                                   *
 * SAD-function for UV without MMX/MMXE                              *
 *                                                                   *
 *********************************************************************/

uint32_t
calc_SAD_uv_noaccel (uint8_t * frm, uint8_t * ref)
{
  register uint8_t dx = 0;
  register uint8_t dy = 0;
  int32_t Y = 0;
  uint32_t d = 0;
  
  for(dy=0;dy<8/SS_V;dy++)
    for(dx=0;dx<8/SS_H;dx++)
    {
      Y=*(frm+dx+dy*W2)-*(ref+dx+dy*W2);
      d+=(Y<0)? -Y:Y;
    }
  return d;
}

/*********************************************************************
 *                                                                   *
 * SAD-function for 4:2:0 UV with MMX                                *
 *                                                                   *
 *********************************************************************/

uint32_t
calc_SAD_uv420_mmx (uint8_t * frm, uint8_t * ref)
{
  static uint16_t a[4];
  
#ifdef HAVE_ASM_MMX
  __asm__ __volatile__
    (
    " pxor        %%mm0 , %%mm0;           /* clear mm0                                          */\n"
    " pxor        %%mm7 , %%mm7;           /* clear mm7                                          */\n"
    "                                      /*                                                    */\n"
    " movl         %1    , %%eax;          /* load frameadress into eax                          */\n"
    " movl         %2    , %%ebx;          /* load frameadress into ebx                          */\n"
    " movl         %3    , %%ecx;          /* load width       into ecx                          */\n"
    "                                      /*                                                    */\n"
    ".rept 4                    ;          /* Loop for 4 lines                                   */\n"
    " movd        (%%eax), %%mm1;          /* 4 Pixels from filtered frame to mm1                */\n"
    " movd        (%%ebx), %%mm2;          /* 4 Pixels from reference frame to mm2               */\n"
    " movq         %%mm2 , %%mm3;          /* hold a copy of mm2 in mm3                          */\n"
    " psubusb      %%mm1 , %%mm3;          /* positive differences between mm2 and mm1           */\n"
    " psubusb      %%mm2 , %%mm1;          /* positive differences between mm1 and mm3           */\n"
    " paddusb      %%mm3 , %%mm1;          /* mm2 now contains abs(mm1-mm2)                      */\n"
    " movq         %%mm1 , %%mm2;          /* copy mm1 to mm2                                    */\n"
    " punpcklbw    %%mm7 , %%mm1;          /* unpack mm1 into mm1 and mm2                        */\n"
    " punpckhbw    %%mm7 , %%mm2;          /*                                                    */\n"
    " paddusw      %%mm1 , %%mm2;          /* add mm1 (stored in mm1 and mm2...)                 */\n"
    " paddusw      %%mm2 , %%mm0;          /* to mm0                                             */\n"
    " addl         %%ecx , %%eax;          /* add framewidth to frameaddress                     */\n"
    " addl         %%ecx , %%ebx;          /* add framewidth to frameaddress                     */\n"
    " .endr                                /* end loop                                           */\n"
    "                                      /*                                                    */\n"
    " movq         %%mm0 , %0   ;          /* make mm0 available to gcc ...                      */\n"
    :"=m" (a)     
    :"m" (frm), "m" (ref), "m" (denoiser.frame.w/2)
    :"%eax", "%ebx", "%ecx"
    );
#endif
  return (uint32_t)(a[0]+a[1]+a[2]+a[3]);
}

/*********************************************************************
 *                                                                   *
 * SAD-function for 4:1:1 UV with MMX                                *
 *                                                                   *
 *********************************************************************/

uint32_t
calc_SAD_uv411_mmx (uint8_t * frm, uint8_t * ref)
{
  static uint16_t a[8];
  
#ifdef HAVE_ASM_MMX
  __asm__ __volatile__
    (
    " pxor        %%mm0 , %%mm0;           /* clear mm0                                          */\n"
    " pxor        %%mm7 , %%mm7;           /* clear mm7                                          */\n"
    "                                      /*                                                    */\n"
    " movl         %1    , %%eax;          /* load frameadress into eax                          */\n"
    " movl         %2    , %%ebx;          /* load frameadress into ebx                          */\n"
    " movl         %3    , %%ecx;          /* load width       into ecx                          */\n"
    "                                      /*                                                    */\n"
    ".rept 8                    ;          /* Loop for 8 lines                                   */\n"
    " movw        (%%eax),  %%dx;          /*                                                    */\n"
    " movd         %%edx , %%mm1;          /* 2 Pixels from filtered frame to mm1                */\n"
    " movw        (%%ebx),  %%dx;          /*                                                    */\n"
    " movd         %%edx , %%mm2;          /* 2 Pixels from reference frame to mm2               */\n"
    " movq         %%mm2 , %%mm3;          /* hold a copy of mm2 in mm3                          */\n"
    " psubusb      %%mm1 , %%mm3;          /* positive differences between mm2 and mm1           */\n"
    " psubusb      %%mm2 , %%mm1;          /* positive differences between mm1 and mm3           */\n"
    " paddusb      %%mm3 , %%mm1;          /* mm2 now contains abs(mm1-mm2)                      */\n"
    " movq         %%mm1 , %%mm2;          /* copy mm1 to mm2                                    */\n"
    " punpcklbw    %%mm7 , %%mm1;          /* unpack mm1 into mm1 and mm2                        */\n"
    " punpckhbw    %%mm7 , %%mm2;          /*                                                    */\n"
    " paddusw      %%mm1 , %%mm2;          /* add mm1 (stored in mm1 and mm2...)                 */\n"
    " paddusw      %%mm2 , %%mm0;          /* to mm0                                             */\n"
    " addl         %%ecx , %%eax;          /* add framewidth to frameaddress                     */\n"
    " addl         %%ecx , %%ebx;          /* add framewidth to frameaddress                     */\n"
    " .endr                                /* end loop                                           */\n"
    "                                      /*                                                    */\n"
    " movq         %%mm0 , %0   ;          /* make mm0 available to gcc ...                      */\n"
    :"=m" (a)     
    :"m" (frm), "m" (ref), "m" (W2)
    :"%eax", "%ebx", "%ecx"
    );
#endif
  return (uint32_t)(a[0]+a[1]+a[2]+a[3]+a[4]+a[5]+a[6]+a[7]);
}


/*********************************************************************
 *                                                                   *
 * SAD-function for 4:2:0 UV with MMXE                               *
 *                                                                   *
 *********************************************************************/

uint32_t
calc_SAD_uv420_mmxe (uint8_t * frm, uint8_t * ref)
{
  static uint32_t a;

#ifdef HAVE_ASM_MMX
  __asm__ __volatile__
    (
    " pxor         %%mm0 , %%mm0;          /* clear mm0                                          */\n"
    " movl         %1    , %%eax;          /* load frameadress into eax                          */\n"
    " movl         %2    , %%ebx;          /* load frameadress into ebx                          */\n"
    " movl         %3    , %%ecx;          /* load width       into ecx                          */\n"
    "                           ;          /*                                                    */\n"
    " .rept 4                   ;          /*                                                    */\n"
    " movd        (%%eax), %%mm1;          /* 4 Pixels from filtered frame to mm1                */\n"
    " movd        (%%ebx), %%mm2;          /* 4 Pixels from filtered frame to mm2                */\n"
    " psadbw       %%mm2 , %%mm1;          /* 4 Pixels difference to mm1                         */\n"
    " paddusw      %%mm1 , %%mm0;          /* add result to mm0                                  */\n"
    " addl         %%ecx , %%eax;          /* add framewidth to frameaddress                     */\n"
    " addl         %%ecx , %%ebx;          /* add framewidth to frameaddress                     */\n"
    " .endr                     ;          /*                                                    */\n"
    "                                      /*                                                    */\n"
    " movq         %%mm0 , %0   ;          /* make mm0 available to gcc ...                      */\n"
    :"=m" (a)     
    :"m" (frm), "m" (ref), "m" (denoiser.frame.w/2)
    :"%eax", "%ebx", "%ecx"
    );
#endif
  return a;
}

/*********************************************************************
 *                                                                   *
 * SAD-function for 4:1:1 UV with MMXE                               *
 *                                                                   *
 *********************************************************************/

uint32_t
calc_SAD_uv411_mmxe (uint8_t * frm, uint8_t * ref)
{
  static uint32_t a;

#ifdef HAVE_ASM_MMX
  __asm__ __volatile__
    (
    " pxor         %%mm0 , %%mm0;          /* clear mm0                                          */\n"
    " movl         %1    , %%eax;          /* load frameadress into eax                          */\n"
    " movl         %2    , %%ebx;          /* load frameadress into ebx                          */\n"
    " movl         %3    , %%ecx;          /* load width       into ecx                          */\n"
    "                           ;          /*                                                    */\n"
    " .rept 8                   ;          /*                                                    */\n"
    " movw        (%%eax),  %%dx;          /*                                                    */\n"
    " movd         %%edx , %%mm1;          /* 2 Pixels from filtered frame to mm1                */\n"
    " movw        (%%ebx),  %%dx;          /*                                                    */\n"
    " movd         %%edx , %%mm2;          /* 2 Pixels from filtered frame to mm2                */\n"
    " psadbw       %%mm2 , %%mm1;          /* 2 Pixels difference to mm1                         */\n"
    " paddusw      %%mm1 , %%mm0;          /* add result to mm0                                  */\n"
    " addl         %%ecx , %%eax;          /* add framewidth to frameaddress                     */\n"
    " addl         %%ecx , %%ebx;          /* add framewidth to frameaddress                     */\n"
    " .endr                     ;          /*                                                    */\n"
    "                                      /*                                                    */\n"
    " movq         %%mm0 , %0   ;          /* make mm0 available to gcc ...                      */\n"
    :"=m" (a)     
    :"m" (frm), "m" (ref), "m" (W2)
    :"%eax", "%ebx", "%ecx"
    );
#endif
  return a;
}

/*********************************************************************
 *                                                                   *
 * halfpel SAD-function for Y without MMX/MMXE                       *
 *                                                                   *
 *********************************************************************/

uint32_t
calc_SAD_half_noaccel (uint8_t * ref, uint8_t * frm1, uint8_t * frm2)
{
  uint8_t dx = 0;
  uint8_t dy = 0;
  int32_t Y = 0;
  uint32_t d = 0;
  
  for(dy=0;dy<8;dy++)
    for(dx=0;dx<8;dx++)
    {
      Y=((*(frm1+dx+dy*denoiser.frame.w)+*(frm2+dx+dy*denoiser.frame.w))>>1)-*(ref+dx+dy*denoiser.frame.w);
      d+=(Y<0)? -Y:Y;
    }
  return d;
}

/*********************************************************************
 *                                                                   *
 * halfpel SAD-function for Y with MMX                               *
 *                                                                   *
 *********************************************************************/

uint32_t
calc_SAD_half_mmx (uint8_t * ref, uint8_t * frm1, uint8_t * frm2) 
{
  static uint32_t a;
#ifdef HAVE_ASM_MMX
  static uint32_t bit_mask[2] = {0x7f7f7f7f,0x7f7f7f7f};

  __asm__ __volatile__
      (
	  " pxor         %%mm0 , %%mm0;          /* clear mm0                                          */"
	  " movl         %1    , %%eax;          /* load frameadress into eax                          */"
	  " movl         %2    , %%ebx;          /* load frameadress into ebx                          */"
	  " movl         %3    , %%ecx;          /* load frameadress into ecx                          */"
	  " movl         %4    , %%edx;          /* load width       into edx                          */"
	  "                           ;          /*                                                    */"
	  " .rept 8                   ;          /*                                                    */"
	  " movq        (%%eax), %%mm1;          /* 8 Pixels from filtered frame to mm1                */"
	  " movq        (%%ebx), %%mm2;          /* 8 Pixels from filtered frame to mm2 (displaced)    */"
	  " movq        (%%ecx), %%mm3;          /* reference to mm3                                   */"
	  " psrlq        $1    , %%mm1;          /* average source pixels                              */"
	  " psrlq        $1    , %%mm2;          /* shift right by one (divide by two)                 */"
	  " pand         %5    , %%mm1;          /* kill downshifted bits                              */"
	  " pand         %5    , %%mm2;          /* kill downshifted bits                              */"
	  " paddusw      %%mm2 , %%mm1;          /* add up ...                                         */"

	  " movq         %%mm3 , %%mm4;          /* copy reference to mm4                              */"
	  " psubusb      %%mm1 , %%mm3;          /* positive differences between mm2 and mm1 */"
	  " psubusb      %%mm4 , %%mm1;          /* positive differences between mm1 and mm3 */"
	  " paddusb      %%mm3 , %%mm1;          /* mm1 now contains abs(mm1-mm2) */"
	  " paddusw      %%mm1 , %%mm0;          /* add result to mm0                                  */"
	  " addl         %%edx , %%eax;          /* add framewidth to frameaddress                     */"
	  " addl         %%edx , %%ebx;          /* add framewidth to frameaddress                     */"
	  " addl         %%edx , %%ecx;          /* add framewidth to frameaddress                     */"
	  " .endr                     ;          /*                                                    */"
	  "                                      /*                                                    */"
	  " movq         %%mm0 , %0   ;          /* make mm0 available to gcc ...                      */"
	  :"=m" (a)     
	  :"m" (frm1),"m" (frm2), "m" (ref), "m" (denoiser.frame.w), "m" (bit_mask)
	  :"%eax", "%ebx", "%ecx", "%edx"
	  );
#endif
  return a;
}

/*********************************************************************
 *                                                                   *
 * halfpel SAD-function for Y with MMXE                              *
 *                                                                   *
 *********************************************************************/

uint32_t
calc_SAD_half_mmxe (uint8_t * ref, uint8_t * frm1, uint8_t * frm2) 
{
  static uint32_t a;

#ifdef HAVE_ASM_MMX
  __asm__ __volatile__
      (
	  " pxor         %%mm0 , %%mm0;          /* clear mm0                                          */\n"
	  " movl         %1    , %%eax;          /* load frameadress into eax                          */\n"
	  " movl         %2    , %%ebx;          /* load frameadress into ebx                          */\n"
	  " movl         %3    , %%ecx;          /* load frameadress into ecx                          */\n"
	  " movl         %4    , %%edx;          /* load width       into edx                          */\n"
	  "                           ;          /*                                                    */\n"
	  " .rept 8                   ;          /*                                                    */\n"
	  " movq        (%%eax), %%mm1;          /* 8 Pixels from filtered frame to mm1                */\n"
	  " movq        (%%ebx), %%mm2;          /* 8 Pixels from filtered frame to mm2 (displaced)    */\n"
	  " movq        (%%ecx), %%mm3;          /* 8 Pixels from reference frame to mm3               */\n"
	  " pavgb        %%mm2 , %%mm1;          /* average source pixels                              */\n"
	  " psadbw       %%mm3 , %%mm1;          /* 8 Pixels difference to mm1                         */\n"
	  " paddusw      %%mm1 , %%mm0;          /* add result to mm0                                  */\n"
	  " addl         %%edx , %%eax;          /* add framewidth to frameaddress                     */\n"
	  " addl         %%edx , %%ebx;          /* add framewidth to frameaddress                     */\n"
	  " addl         %%edx , %%ecx;          /* add framewidth to frameaddress                     */\n"
	  " .endr                     ;          /*                                                    */\n"
	  "                                      /*                                                    */\n"
	  " movq         %%mm0 , %0   ;          /* make mm0 available to gcc ...                      */\n"
	  :"=m" (a)     
	  :"m" (frm1),"m" (frm2), "m" (ref), "m" (denoiser.frame.w)
	  :"%eax", "%ebx", "%ecx", "%edx"
	  );
#endif
  return a;
}

/*********************************************************************
 *                                                                   *
 * Estimate Motion Vectors in 4 times subsampled frames              *
 *                                                                   *
 *********************************************************************/

void 
mb_search_44 (uint16_t x, uint16_t y)
{
  uint32_t best_SAD=0x00ffffff;
  uint32_t SAD=0x00ffffff; 
  uint32_t SAD_uv=0x00ffffff; 
  uint8_t  radius = denoiser.radius>>2;       /* search radius /4 in pixels */
  int32_t  MB_ref_offset = denoiser.frame.w * (y>>2) + (x>>2);
  int32_t  MB_avg_offset;
  int32_t  MB_ref_offset_uv = (denoiser.frame.Cw) * (y>>2)/SS_V + (x>>2)/SS_H;
  int32_t  MB_avg_offset_uv;
  int32_t  last_uv_offset=0;
  int16_t  xx;
  int16_t  yy;

  SAD = calc_SAD ( denoiser.frame.sub4ref[0]+MB_ref_offset, 
                   denoiser.frame.sub4avg[0]+MB_ref_offset );
      
  SAD += calc_SAD_uv ( denoiser.frame.sub4ref[1]+MB_ref_offset_uv, 
                       denoiser.frame.sub4avg[1]+MB_ref_offset_uv );
  SAD += calc_SAD_uv ( denoiser.frame.sub4ref[2]+MB_ref_offset_uv, 
                       denoiser.frame.sub4avg[2]+MB_ref_offset_uv );
  
  for(yy=-radius;yy<=radius;yy++)
    for(xx=-radius;xx<=radius;xx++)
    {
      MB_avg_offset    = MB_ref_offset+(xx)+(yy*denoiser.frame.w);
      MB_avg_offset_uv = MB_ref_offset_uv+(xx/SS_H)+((yy/SS_V)*denoiser.frame.Cw);
      
      SAD = calc_SAD ( denoiser.frame.sub4ref[0]+MB_ref_offset, 
                       denoiser.frame.sub4avg[0]+MB_avg_offset );

      if(MB_ref_offset_uv != last_uv_offset)
        {
          last_uv_offset=MB_ref_offset_uv;
          SAD_uv = calc_SAD_uv ( denoiser.frame.sub4ref[1]+MB_ref_offset_uv, 
                                 denoiser.frame.sub4avg[1]+MB_avg_offset_uv );
          SAD_uv += calc_SAD_uv ( denoiser.frame.sub4ref[2]+MB_ref_offset_uv, 
                                  denoiser.frame.sub4avg[2]+MB_avg_offset_uv );
        }
        SAD += SAD_uv;
      
      SAD += xx*xx + yy*yy; /* favour center matches... */
        
      if(SAD<=best_SAD)
      {
        best_SAD = SAD;

        vector.x = xx;
        vector.y = yy;
      }
    }
}

/*********************************************************************
 *                                                                   *
 * Estimate Motion Vectors in 2 times subsampled frames              *
 *                                                                   *
 *********************************************************************/

void 
mb_search_22 (uint16_t x, uint16_t y)
{
  uint32_t best_SAD=0x00ffffff;
  uint32_t SAD=0x00ffffff; 
  uint32_t SAD_uv=0x00ffffff; 
  int32_t  MB_ref_offset = denoiser.frame.w * (y>>1) + (x>>1);
  int32_t  MB_avg_offset;
  int32_t  MB_ref_offset_uv = (denoiser.frame.Cw) * (y>>1)/SS_V + (x>>1)/SS_H;
  int32_t  MB_avg_offset_uv;
  int32_t  last_uv_offset=0;
  int16_t  xx;
  int16_t  yy;
  int16_t  vx=vector.x<<1;
  int16_t  vy=vector.y<<1;
    
  /* motion-vectors from 44 can/will be wrong by +/- 3 pixels */
    
  for(yy=-2;yy<=2;yy++)
    for(xx=-2;xx<=2;xx++)
    {
      MB_avg_offset=MB_ref_offset+(xx+vx)+((yy+vy)*denoiser.frame.w);
      /* MB_avg_offset_uv = MB_ref_offset_uv+((xx+vx)>>1)/SS_H+(((yy+vy)>>1)/SS_V)*(denoiser.frame.Cw); */
      MB_avg_offset_uv = MB_ref_offset_uv+((xx+vx)/SS_H)+((yy+vy)/SS_V)*(denoiser.frame.Cw);
      
      SAD = calc_SAD ( denoiser.frame.sub2ref[0]+MB_ref_offset, 
                       denoiser.frame.sub2avg[0]+MB_avg_offset );
      
      if(MB_ref_offset_uv != last_uv_offset)
      {
        last_uv_offset=MB_ref_offset_uv;
        SAD_uv = calc_SAD_uv ( denoiser.frame.sub2ref[1]+MB_ref_offset_uv, 
                               denoiser.frame.sub2avg[1]+MB_avg_offset_uv );
        SAD_uv += calc_SAD_uv ( denoiser.frame.sub2ref[2]+MB_ref_offset_uv, 
                                denoiser.frame.sub2avg[2]+MB_avg_offset_uv );
      }
      SAD += SAD_uv;
      
      if(SAD<=best_SAD)
      {
        best_SAD = SAD;

        varray22[2]=varray22[1];
        varray22[1]=varray22[0];

        varray22[0].x = xx+vx;
        varray22[0].y = yy+vy;
        vector.x = xx+vx;
        vector.y = yy+vy;
      }
    }
}



/*********************************************************************
 *                                                                   *
 * Estimate Motion Vectors in not subsampled frames                  *
 *                                                                   *
 *********************************************************************/

void 
mb_search_11 (uint16_t x, uint16_t y)
{
  uint32_t best_SAD = 0x00ffffff;
  uint32_t SAD=0x00ffffff; 
  uint32_t SAD_uv=0x00ffffff; 
  int32_t  MB_ref_offset = denoiser.frame.w * (y) + (x);
  int32_t  MB_avg_offset;
  int32_t  MB_ref_offset_uv = (denoiser.frame.Cw) * y/SS_V + x/SS_H;
  int32_t  MB_avg_offset_uv;
  int32_t  last_uv_offset=0;
  int16_t  xx;
  int16_t  yy;
  int16_t  vx=vector.x<<1;
  int16_t  vy=vector.y<<1;
  
  /* motion-vectors from 22 can/will be wrong by +/- 2 pixels */

  for(yy=-2;yy<=2;yy++)
    for(xx=-2;xx<=2;xx++)
    {
      MB_avg_offset=MB_ref_offset+(xx+vx)+((yy+vy)*denoiser.frame.w);
      MB_avg_offset_uv = MB_ref_offset_uv+((xx+vx)/SS_H)+((yy+vy)/SS_V)*(denoiser.frame.Cw);
      
        SAD = calc_SAD ( denoiser.frame.ref[0]+MB_ref_offset, 
                         denoiser.frame.avg[0]+MB_avg_offset );
 
      if(MB_ref_offset_uv != last_uv_offset)
      {
        last_uv_offset=MB_ref_offset_uv;
        SAD_uv = calc_SAD_uv ( denoiser.frame.sub2ref[1]+MB_ref_offset_uv, 
                               denoiser.frame.sub2avg[1]+MB_avg_offset_uv );
        SAD_uv += calc_SAD_uv ( denoiser.frame.sub2ref[2]+MB_ref_offset_uv, 
                                denoiser.frame.sub2avg[2]+MB_avg_offset_uv );
      }
      SAD += SAD_uv;
       
      if(SAD<best_SAD)
      {
        best_SAD = SAD;
        vector.SAD = SAD;
        vector.x = xx+vx;
        vector.y = yy+vy;
      }
    }
    
  /* finally do a zero check against the found vector */
    
  SAD = calc_SAD ( denoiser.frame.ref[0]+MB_ref_offset, 
                   denoiser.frame.avg[0]+MB_ref_offset );
  
  if(SAD<=best_SAD)
  {
    vector.x = 0;
    vector.y = 0;
    vector.SAD = SAD;
  }
}


/*********************************************************************
 *                                                                   *
 * Estimate Motion Vectors in not subsampled frames                  *
 *                                                                   *
 *********************************************************************/

void 
mb_search_00 (uint16_t x, uint16_t y)
{
  uint32_t best_SAD = 0x00ffffff;
  uint32_t SAD;
  int32_t  MB_ref_offset = denoiser.frame.w * (y) + (x);
  int32_t  MB_avg_offset1;
  int32_t  MB_avg_offset2;
  int16_t  xx;
  int16_t  yy;
  int16_t  vx=vector.x;
  int16_t  vy=vector.y;
  
  MB_avg_offset1=MB_ref_offset+(vx)+((vy)*denoiser.frame.w);

  for(yy=-1;yy<=1;yy++)
    for(xx=-1;xx<=1;xx++)
    {
      MB_avg_offset2=MB_ref_offset+(vx+xx)+((vy+yy)*denoiser.frame.w);
      
      SAD = calc_SAD_half  (denoiser.frame.ref[0]+MB_ref_offset, 
                            denoiser.frame.avg[0]+MB_avg_offset1,
                            denoiser.frame.avg[0]+MB_avg_offset2);

      if(SAD<best_SAD)
      {
        best_SAD = SAD;
        vector.x = xx+vx*2;
        vector.y = yy+vy*2;
      }
    }
}
