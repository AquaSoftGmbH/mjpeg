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

#ifndef __DENOISER_GLOBAL_H__
#define __DENOISER_GLOBAL_H__

#define Yy (0)
#define Cr (1)
#define Cb (2)

#define Y_LO_LIMIT 16
#define Y_HI_LIMIT 235
#define C_LO_LIMIT 16
#define C_HI_LIMIT 240

#define W  (denoiser.frame.w)
#define H  (denoiser.frame.h)
#define W2 (denoiser.frame.Cw)
#define H2 (denoiser.frame.Ch)
#define SS_H (denoiser.frame.ss_h)
#define SS_V (denoiser.frame.ss_v)
#define BUF_OFF (32)
#define BUF_COFF (32/denoiser.frame.ss_v)


struct DNSR_GLOBAL
  {
    /* denoiser mode */
    /* 0 progressive */
    /* 1 interlaced */
    /* 2 PASS II only */
    uint8_t   mode;
    
    /* Search radius */    
    uint8_t   radius;

    /* Copy threshold */
    uint8_t   thresholdY;
    uint8_t   thresholdCb;
    uint8_t   thresholdCr;
    uint8_t   chroma_flicker;
    uint8_t   pp_threshold;
    
    /* Time-average-delay */
    uint8_t   delayY;
    uint8_t   delayCb;
    uint8_t   delayCr;
    
    /* Deinterlacer to be turned on? */    
    uint8_t   deinterlace;

    /* Postprocessing */
    uint16_t   postprocess;
    uint16_t   luma_contrast;
    uint16_t   chroma_contrast;
    uint16_t   sharpen;
    
    /* Frame information */
    struct 
    {
      int32_t  w;
      int32_t  h;
      int32_t  Cw;
      int32_t  Ch;
      uint8_t  ss_h;
      uint8_t  ss_v;

      uint8_t *io[3];
      uint8_t *ref[3];
      uint8_t *avg[3];
      uint8_t *dif[3];
      uint8_t *dif2[3];
      uint8_t *avg2[3];
      uint8_t *tmp[3];
      uint8_t *sub2ref[3];
      uint8_t *sub2avg[3];
      uint8_t *sub4ref[3];
      uint8_t *sub4avg[3];
    } frame;
    
    /* Border information */
    struct 
    {
      uint16_t  x;
      uint16_t  y;
      uint16_t  w;
      uint16_t  h;      
    } border;
    
  };
  
struct DNSR_VECTOR
{
  int8_t  x;
  int8_t  y;
  uint32_t SAD;
};

uint8_t luma_contrast_vector[256];

#endif
