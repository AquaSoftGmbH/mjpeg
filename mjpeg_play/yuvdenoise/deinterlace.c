/***********************************************************
 * YUVDENOISER for the mjpegtools                          *
 * ------------------------------------------------------- *
 * (C) 2001,2002 Stefan Fendt                              *
 *                                                         *
 * Licensed and protected by the GNU-General-Public-       *
 * License version 2 or if you prefer any later version of *
 * that license). See the file LICENSE for detailed infor- *
 * mation.                                                 *
 ***********************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "mjpeg_types.h"
#include "global.h"
#include "deinterlace.h"

/* global denoiser structure defined in main.c and global.h */
extern struct DNSR_GLOBAL denoiser;

/* pointer on deinterlace functions */
void (*deinterlace) (void);

void 
deinterlace_noaccel(void)
{
  unsigned int d=0;
  unsigned int min;
  register int x;
  register int y;
  register int xx;
  register int i;
  int xpos;
  int l1;
  int l2;
  int lumadiff = 0;
  uint8_t line_buf[8192]; 
  uint8_t *line = line_buf;

  /* Go through the frame by every two lines */
  for (y = 32; y < (H+32) ; y += 2)
    {
      /* Go through each line by a "block" length of 32 pixels */
      for (x = 0; x < W ; x += 8)
        {
          /* search best matching block of pixels in other field line */
          min = 65535;
          xpos = 0;
          /* search in the range of +/- 8 pixels offset in the line */

          for (xx = -8; xx < 8; xx++)
            {
              d = 0;
              /* Calculate SAD */
              for (i = -8; i < 16; i++)
                {
                  /* to avoid blocking in ramps we analyse the best match on */
                  /* two lines ... */
                  d += (int) abs (*(denoiser.frame.ref[0] + (x + i) + y * W) -
                                  *(denoiser.frame.ref[0] + (x + xx + i) + (y + 1) * W));
                  d += (int) abs (*(denoiser.frame.ref[0] + (x + i) + (y + 2) * W) -
                                  *(denoiser.frame.ref[0] + (x + xx + i) + (y + 1) * W));
                }


              /* if SAD reaches a minimum store the position */
              if (min > d)
                {
                  min = d;
                  xpos = xx;
                  
                  l1 = l2 = 0;
                  for (i = 0; i < 8; i++)
                    {
                      l1 += *(denoiser.frame.ref[0] + (x + i) + y * W);
                      l2 += *(denoiser.frame.ref[0] + (x + i + xpos) + (y + 1) * W);
                    }
                  l1 /= 8;
                  l2 /= 8;
                  lumadiff = abs (l1 - l2);
                  lumadiff = (lumadiff < 8) ? 0 : 1;
                }
            }

          /* copy pixel-block into the line-buffer */

          /* if lumadiff is small take the fields block, if not */
          /* take the other fields block */

          if (lumadiff || min > (12 * 24))
            for (i = 0; i < 8; i++) /* average pixels :( */
              {
                *(line + x + i) =
                  (*(denoiser.frame.ref[0] + (x + i) + ((y) * W)) >>1) +
                  (*(denoiser.frame.ref[0] + (x + i) + ((y + 2) * W))>>1) + 1;
              }
          else
            for (i = 0; i < 8; i++) /* move block :) */
              {
                *(line + x + i) =
                  (*(denoiser.frame.ref[0] + (x + i + xpos) + ((y + 1) * W)) >>1) +
                  (*(denoiser.frame.ref[0] + (x + i) + ((y + 0) * W)) >> 1) + 1;
              }
        }

      /* copy the line-buffer into the source-line */
      for (i = 0; i < W; i++)
        *(denoiser.frame.ref[0] + i + (y + 1) * W) = *(line + i);
    }
}

void 
deinterlace_mmx(void)
{
  uint32_t d=0;
  uint16_t a[4]={0,0,0,0};
  unsigned int min;
  register int x;
  register int y;
  int xx;
  int i;
  int xpos;
  int l1;
  int l2;
  int lumadiff = 0;
  uint8_t line_buf[8192]; 
  uint8_t *line = line_buf;
  uint8_t *ref1;
  uint8_t *ref2;
  uint8_t *ref3;
  
  /* Go through the frame by every two lines */
  for (y = 32; y < (H+32) ; y += 2)
    {
      /* Go through each line by a "block" length of 32 pixels */
      for (x = 0; x < W ; x += 8)
        {
          /* search best matching block of pixels in other field line */
          min = 65535;
          xpos = 0;
          /* search in the range of +/- 8 pixels offset in the line */

          for (xx = 0; xx < 8; xx++)
            {
              
              ref1=denoiser.frame.ref[0]+x+y*W;      /* not displaced */
              ref2=denoiser.frame.ref[0]+x+y*W+W*2;  /* not displaced two lines below */
              ref3=denoiser.frame.ref[0]+x+y*W+W+xx; /* displaced one line below */
              
              #ifdef HAVE_ASM_MMX
              __asm__ __volatile__
                (
                  " pxor        %%mm0 , %%mm0;           /* clear mm0                                          */\n"
                  " pxor        %%mm7 , %%mm7;           /* clear mm7                                          */\n"
                  "                                      /*                                                    */\n"
                  " movl         %1    , %%eax;          /* load frameadress into eax                          */\n"
                  " movl         %2    , %%ebx;          /* load frameadress into ebx                          */\n"
                  " movl         %3    , %%ecx;          /* load frameadress into ecx                          */\n"
                  "                                      /*                                                    */\n"
                  " .rept 3                              /* repeat 3 times                                     */\n"
                  " movq        (%%eax), %%mm1;          /* 8 Pixels from line                                 */\n"
                  " movq        (%%ecx), %%mm2;          /* 8 Pixels from displaced                            */\n"
                  " movq         %%mm2 , %%mm3;          /* hold a copy of mm2 in mm3                          */\n"
                  " psubusb      %%mm1 , %%mm3;          /* positive differences between mm2 and mm1           */\n"
                  " psubusb      %%mm2 , %%mm1;          /* positive differences between mm1 and mm3           */\n"
                  " paddusb      %%mm3 , %%mm1;          /* mm1 now contains abs(mm1-mm2)                      */\n"
                  " movq         %%mm1 , %%mm2;          /* copy mm1 to mm2                                    */\n"
                  " punpcklbw    %%mm7 , %%mm1;          /* unpack mm1 into mm1 and mm2                        */\n"
                  " punpckhbw    %%mm7 , %%mm2;          /*                                                    */\n"
                  " paddusw      %%mm1 , %%mm0;          /* add mm1 (stored in mm1 and mm2...)                 */\n"
                  " paddusw      %%mm2 , %%mm0;          /* to mm0                                             */\n"
                  "                                      /*                                                    */\n"
                  " movq        (%%ebx), %%mm1;          /* 8 Pixels from line                                 */\n"
                  " movq        (%%ecx), %%mm2;          /* 8 Pixels from displaced line                       */\n"
                  " movq         %%mm2 , %%mm3;          /* hold a copy of mm2 in mm3                          */\n"
                  " psubusb      %%mm1 , %%mm3;          /* positive differences between mm2 and mm1           */\n"
                  " psubusb      %%mm2 , %%mm1;          /* positive differences between mm1 and mm3           */\n"
                  " paddusb      %%mm3 , %%mm1;          /* mm1 now contains abs(mm1-mm2)                      */\n"
                  " movq         %%mm1 , %%mm2;          /* copy mm1 to mm2                                    */\n"
                  " punpcklbw    %%mm7 , %%mm1;          /* unpack mm1 into mm1 and mm2                        */\n"
                  " punpckhbw    %%mm7 , %%mm2;          /*                                                    */\n"
                  " paddusw      %%mm1 , %%mm0;          /* add mm1 (stored in mm1 and mm2...)                 */\n"
                  " paddusw      %%mm2 , %%mm0;          /* to mm0                                             */\n"
                  " addl         $8    , %%eax;          /* add 8 to frameaddress                              */\n"
                  " addl         $8    , %%ebx;          /* add 8 to frameaddress                              */\n"
                  " addl         $8    , %%ecx;          /* add 8 to frameaddress                              */\n"
                  " .endr                                /* end loop                                           */\n"
                  "                                      /*                                                    */\n"
                  " movq         %%mm0 , %0   ;          /* make mm0 available to gcc ...                      */\n"
                  :"=m" (a)
                  :"m" (ref1), "m" (ref2), "m" (ref3)
                  :"%eax", "%ebx", "%ecx"
                );
              
              d=a[0]+a[1]+a[2]+a[3];
              #endif
              
              /* if SAD reaches a minimum store the position */
              if (min > d)
                {
                  min = d;
                  xpos = xx;
                  
                  l1 = l2 = 0;
                  for (i = 0; i < 8; i++)
                    {
                      l1 += *(denoiser.frame.ref[0] + (x + i) + y * W);
                      l2 += *(denoiser.frame.ref[0] + (x + i + xpos) + (y + 1) * W);
                    }
                  l1 /= 8;
                  l2 /= 8;
                  lumadiff = abs (l1 - l2);
                  lumadiff = (lumadiff < 8) ? 0 : 1;
                }
            }

          /* copy pixel-block into the line-buffer */

          /* if lumadiff is small take the fields block, if not */
          /* take the other fields block */

          if (lumadiff || min > (12 * 24))
            for (i = 0; i < 8; i++) /* average pixels :( */
              {
                *(line + x + i) =
                  (*(denoiser.frame.ref[0] + (x + i) + ((y) * W)) >>1) +
                  (*(denoiser.frame.ref[0] + (x + i) + ((y + 2) * W))>>1) + 1;
              }
          else
            for (i = 0; i < 8; i++) /* move block :) */
              {
                *(line + x + i) =
                  (*(denoiser.frame.ref[0] + (x + i + xpos) + ((y + 1) * W)) >>1) +
                  (*(denoiser.frame.ref[0] + (x + i) + ((y + 0) * W)) >> 1) + 1;
              }
        }

      /* copy the line-buffer into the source-line */
      for (i = 0; i < W; i++)
        *(denoiser.frame.ref[0] + i + (y + 1) * W) = *(line + i);
    }
}

void 
deinterlace_mmxe(void)
{
}
