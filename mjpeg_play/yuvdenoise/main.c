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
#include <string.h>
#include <unistd.h>
#include "mjpeg_types.h"
#include "yuv4mpeg.h"
#include "mjpeg_logging.h"
#include "cpu_accel.h"
#include "config.h"
#include "global.h"
#include "motion.h"
#include "denoise.h"
#include "deinterlace.h"

void allc_buffers(void);
void free_buffers(void);
void display_greeting(void);
void process_commandline(int argc, char *argv[]);
void print_settings(void);
void turn_on_accels(void);
void display_help(void);

static uint8_t *bufalloc(size_t size);

struct DNSR_GLOBAL denoiser;

extern uint32_t (*calc_SAD)         (uint8_t * , uint8_t * );
extern uint32_t (*calc_SAD_uv)      (uint8_t * , uint8_t * );
extern uint32_t (*calc_SAD_half)    (uint8_t * , uint8_t * ,uint8_t *);
extern void     (*deinterlace)      (void);

/***********************************************************
 *                                                         *
 ***********************************************************/

int main(int argc, char *argv[])
{
  int fd_in  = 0;
  int fd_out = 1;
  int errno  = 0;
  int frame_offset;
  int uninitialized = 1;
  
  y4m_frame_info_t frameinfo;
  y4m_stream_info_t streaminfo;

  /* display greeting */
  display_greeting();
  
  /* initialize stream-information */
  y4m_init_stream_info (&streaminfo);
  y4m_init_frame_info (&frameinfo);
  
  /* setup denoiser's global variables */
  denoiser.radius          = 8;
  denoiser.threshold       = 5; /* assume medium noise material */
  denoiser.pp_threshold    = 4; /* same for postprocessing */
  denoiser.delay           = 3; /* short delay for good regeneration of rapid sequences */
  denoiser.postprocess     = 1;
  denoiser.luma_contrast   = 100;
  denoiser.chroma_contrast = 100;
  denoiser.sharpen         = 125; /* very little sharpen by default */
  denoiser.deinterlace     = 0;
  denoiser.mode            = 0; /* initial mode is progressive */
  denoiser.border.x        = 0;
  denoiser.border.y        = 0;
  denoiser.border.w        = 0;
  denoiser.border.h        = 0;
  
  /* process commandline */
  process_commandline(argc, argv);

  /* open input stream */
  if ((errno = y4m_read_stream_header (fd_in, &streaminfo)) != Y4M_OK)
    {
      mjpeg_log (LOG_ERROR, "Couldn't read YUV4MPEG header: %s!\n", y4m_strerr (errno));
      exit (1);
    }
  denoiser.frame.w         = y4m_si_get_width(&streaminfo);
  denoiser.frame.h         = y4m_si_get_height(&streaminfo);
  frame_offset             = 32*denoiser.frame.w;
  if(denoiser.border.w == 0)
  {
      denoiser.border.x        = 0;
      denoiser.border.y        = 0;
      denoiser.border.w        = denoiser.frame.w;
      denoiser.border.h        = denoiser.frame.h;
  }

  /* stream is interlaced and shall not be
   * deinterlaced ?
   */ 
  if (y4m_si_get_interlace(&streaminfo) != Y4M_ILACE_NONE && 
      y4m_si_get_interlace(&streaminfo) != Y4M_UNKNOWN &&
      denoiser.deinterlace == 0 && 
      denoiser.mode !=2 )
  {
    /* setup interlace-mode */
    denoiser.mode=1;
  }
  
  /* stream is interlaced and should be
   * deinterlaced ?
   */ 
  if (y4m_si_get_interlace(&streaminfo) != Y4M_ILACE_NONE && 
      y4m_si_get_interlace(&streaminfo) != Y4M_UNKNOWN &&
      denoiser.deinterlace == 1)
  {
    y4m_si_set_interlace(&streaminfo,Y4M_ILACE_NONE);
  }
  
  /* write the outstream header */
  y4m_write_stream_header (fd_out, &streaminfo); 

  /* get enough memory for the buffers */
  allc_buffers();

  /* print denoisers settings */
  print_settings();

  /* turn on accelerations if any */
  turn_on_accels();
  
  /* read every single frame until the end of the input stream */
  while 
    (
          Y4M_OK == ( errno = y4m_read_frame (fd_in, 
                                              &streaminfo, 
                                              &frameinfo, 
                                              denoiser.frame.io) )
    )
    {
      
      /* Move frame down by 32 lines into reference buffer */
      memcpy(denoiser.frame.ref[Yy]+frame_offset  ,denoiser.frame.io[Yy],denoiser.frame.w*denoiser.frame.h  );
      memcpy(denoiser.frame.ref[Cr]+frame_offset/4,denoiser.frame.io[Cr],denoiser.frame.w*denoiser.frame.h/4);
      memcpy(denoiser.frame.ref[Cb]+frame_offset/4,denoiser.frame.io[Cb],denoiser.frame.w*denoiser.frame.h/4);
      
      if(uninitialized)
      {
        uninitialized=0;
        memcpy(denoiser.frame.avg[Yy]+frame_offset  ,denoiser.frame.io[Yy],denoiser.frame.w*denoiser.frame.h  );
        memcpy(denoiser.frame.avg[Cr]+frame_offset/4,denoiser.frame.io[Cr],denoiser.frame.w*denoiser.frame.h/4);
        memcpy(denoiser.frame.avg[Cb]+frame_offset/4,denoiser.frame.io[Cb],denoiser.frame.w*denoiser.frame.h/4);
        memcpy(denoiser.frame.avg2[Yy]+frame_offset  ,denoiser.frame.io[Yy],denoiser.frame.w*denoiser.frame.h  );
        memcpy(denoiser.frame.avg2[Cr]+frame_offset/4,denoiser.frame.io[Cr],denoiser.frame.w*denoiser.frame.h/4);
        memcpy(denoiser.frame.avg2[Cb]+frame_offset/4,denoiser.frame.io[Cb],denoiser.frame.w*denoiser.frame.h/4);
      }
      
      denoise_frame();
      
      /* Move frame up by 32 lines into I/O buffer */
      memcpy(denoiser.frame.io[Yy],denoiser.frame.avg2[Yy]+frame_offset  ,denoiser.frame.w*denoiser.frame.h  );
      memcpy(denoiser.frame.io[Cr],denoiser.frame.avg2[Cr]+frame_offset/4,denoiser.frame.w*denoiser.frame.h/4);
      memcpy(denoiser.frame.io[Cb],denoiser.frame.avg2[Cb]+frame_offset/4,denoiser.frame.w*denoiser.frame.h/4);

      y4m_write_frame ( fd_out, 
                        &streaminfo, 
                        &frameinfo, 
                        denoiser.frame.io);

    }
    
  /* did stream end unexpectedly ? */
  if(errno != Y4M_ERR_EOF )
          mjpeg_error_exit1( "%s\n", y4m_strerr( errno ) );
  
  /* just as name says... free them... */
  free_buffers();
  
  /* Exit gently */
  return(0);
}



static uint8_t *bufalloc(size_t size)
{
  uint8_t *ret = (uint8_t *)malloc(size);
  if( ret == NULL )
    mjpeg_error_exit1( "Out of memory: could not allocate buffer\n" );
  return ret;
}



void allc_buffers(void)
{
  int luma_buffsize = denoiser.frame.w * denoiser.frame.h;
  int chroma_buffsize = (denoiser.frame.w * denoiser.frame.h) / 4;
  
  /* now, the MC-functions really(!) do go beyond the vertical
   * frame limits so we need to make the buffers larger to avoid
   * bound-checking (memory vs. speed...)
   */
  
  luma_buffsize += 64*denoiser.frame.w;
  chroma_buffsize += 64*denoiser.frame.w;
  
  denoiser.frame.io[0] = bufalloc (luma_buffsize);
  denoiser.frame.io[1] = bufalloc (chroma_buffsize);
  denoiser.frame.io[2] = bufalloc (chroma_buffsize);
  
  denoiser.frame.ref[0] = bufalloc (luma_buffsize);
  denoiser.frame.ref[1] = bufalloc (chroma_buffsize);
  denoiser.frame.ref[2] = bufalloc (chroma_buffsize);

  denoiser.frame.avg[0] = bufalloc (luma_buffsize);
  denoiser.frame.avg[1] = bufalloc (chroma_buffsize);
  denoiser.frame.avg[2] = bufalloc (chroma_buffsize);

  denoiser.frame.dif[0] = bufalloc (luma_buffsize);
  denoiser.frame.dif[1] = bufalloc (chroma_buffsize);
  denoiser.frame.dif[2] = bufalloc (chroma_buffsize);

  denoiser.frame.dif2[0] = bufalloc (luma_buffsize);
  denoiser.frame.dif2[1] = bufalloc (chroma_buffsize);
  denoiser.frame.dif2[2] = bufalloc (chroma_buffsize);

  denoiser.frame.avg2[0] = bufalloc (luma_buffsize);
  denoiser.frame.avg2[1] = bufalloc (chroma_buffsize);
  denoiser.frame.avg2[2] = bufalloc (chroma_buffsize);

  denoiser.frame.tmp[0] = bufalloc (luma_buffsize);
  denoiser.frame.tmp[1] = bufalloc (chroma_buffsize);
  denoiser.frame.tmp[2] = bufalloc (chroma_buffsize);

  denoiser.frame.sub2ref[0] = bufalloc (luma_buffsize);
  denoiser.frame.sub2ref[1] = bufalloc (chroma_buffsize);
  denoiser.frame.sub2ref[2] = bufalloc (chroma_buffsize);

  denoiser.frame.sub2avg[0] = bufalloc (luma_buffsize);
  denoiser.frame.sub2avg[1] = bufalloc (chroma_buffsize);
  denoiser.frame.sub2avg[2] = bufalloc (chroma_buffsize);

  denoiser.frame.sub4ref[0] = bufalloc (luma_buffsize);
  denoiser.frame.sub4ref[1] = bufalloc (chroma_buffsize);
  denoiser.frame.sub4ref[2] = bufalloc (chroma_buffsize);

  denoiser.frame.sub4avg[0] = bufalloc (luma_buffsize);
  denoiser.frame.sub4avg[1] = bufalloc (chroma_buffsize);
  denoiser.frame.sub4avg[2] = bufalloc (chroma_buffsize);

}



void free_buffers(void)
{
  int i;
  
  for (i = 0; i < 3; i++)
    {
      free (denoiser.frame.io[i]);
      free (denoiser.frame.ref[i]);
      free (denoiser.frame.avg[i]);
      free (denoiser.frame.dif[i]);
      free (denoiser.frame.dif2[i]);
      free (denoiser.frame.avg2[i]);
      free (denoiser.frame.tmp[i]);
      free (denoiser.frame.sub2ref[i]);
      free (denoiser.frame.sub2avg[i]);
      free (denoiser.frame.sub4ref[i]);
      free (denoiser.frame.sub4avg[i]);
    }
}


/*****************************************************************************
 * greeting - message                                                        *
 *****************************************************************************/

void
display_greeting (void)
{
  mjpeg_log (LOG_INFO, " ======================================================== \n");
  mjpeg_log (LOG_INFO, "         Y4M2 Motion-Compensating-YCrCb-Denoiser          \n");
  mjpeg_log (LOG_INFO, " ======================================================== \n");
  mjpeg_log (LOG_INFO, " Version: MjpegTools %s\n", VERSION);
}


void
process_commandline(int argc, char *argv[])
{
  char c;
  int i1,i2,i3,i4;

  while ((c = getopt (argc, argv, "h?t:b:r:l:S:L:C:p:Ff")) != -1)
  {
    switch (c)
    {
      case 'h':
      {
        display_help();
        exit (0);
        break;
      }
      case '?':
      {
        display_help();
        exit (0);
        break;
      }
      case 'b':
      {
        sscanf( optarg, "%i,%i,%i,%i", 
                &i1,
                &i2,
                &i3,
                &i4);
	denoiser.border.x = i1;
	denoiser.border.y = i2;
	denoiser.border.w = i3;
	denoiser.border.h = i4;
        break;
      }
      case 'r':
      {
        denoiser.radius = atoi(optarg);
        if(denoiser.radius<8)
        {
          denoiser.radius=8;
  	      mjpeg_log (LOG_WARN, "Minimum allowed search radius is 8 pixel.\n");
        }
        if(denoiser.radius>24)
        {
  	      mjpeg_log (LOG_WARN, "Maximum suggested search radius is 24 pixel.\n");
        }
        break;
      }
      case 't':
      {
        denoiser.threshold = atoi(optarg);
        break;
      }
      case 'p':
      {
        denoiser.pp_threshold = atoi(optarg);
        break;
      }
      case 'S':
      {
        denoiser.sharpen = atoi(optarg);
        break;
      }
      case 'L':
      {
        denoiser.luma_contrast = atoi(optarg);
        break;
      }
      case 'C':
      {
        denoiser.chroma_contrast = atoi(optarg);
        break;
      }
      case 'F':
      {
        denoiser.deinterlace = 1; /* turn on deinterlacer */
        break;
      }
      case 'f':
      {
        denoiser.mode = 2; /* fast mode */
        break;
      }
      case 'l':
      {
        denoiser.delay = atoi(optarg);
        if(denoiser.delay<1)
        {
          denoiser.delay=1;
  	      mjpeg_log (LOG_WARN, "Minimum allowed frame delay is 1.\n");
        }
        if(denoiser.delay>8)
        {
  	      mjpeg_log (LOG_WARN, "Maximum suggested frame delay is 8.\n");
        }
        break;
      }
    }
  }
    
}


void print_settings(void)
{
  mjpeg_log (LOG_INFO, "\n");
  mjpeg_log (LOG_INFO, " Denoiser - Settings:\n");
  mjpeg_log (LOG_INFO, " --------------------\n");
  mjpeg_log (LOG_INFO, "\n");
  mjpeg_log (LOG_INFO, " Mode             : %s\n",
    (denoiser.mode==0)? "Progressive frames" : (denoiser.mode==1)? "Interlaced frames": "PASS II only");
  mjpeg_log (LOG_INFO, " Deinterlacer     : %s\n",(denoiser.deinterlace==0)? "Off":"On");
  mjpeg_log (LOG_INFO, " Postprocessing   : %s\n",(denoiser.postprocess==0)? "Off":"On");
  mjpeg_log (LOG_INFO, " Frame border     : x:%3i y:%3i w:%3i h:%3i\n",denoiser.border.x,denoiser.border.y,denoiser.border.w,denoiser.border.h);
  mjpeg_log (LOG_INFO, " Search radius    : %3i\n",denoiser.radius);
  mjpeg_log (LOG_INFO, " Filter delay     : %3i\n",denoiser.delay);
  mjpeg_log (LOG_INFO, " Filter threshold : %3i\n",denoiser.threshold);
  mjpeg_log (LOG_INFO, " Pass 2 threshold : %3i\n",denoiser.pp_threshold);
  mjpeg_log (LOG_INFO, " Y - contrast     : %3i %%\n",denoiser.luma_contrast);
  mjpeg_log (LOG_INFO, " Cr/Cb - contrast : %3i %%\n",denoiser.chroma_contrast);
  mjpeg_log (LOG_INFO, " Sharpen          : %3i %%\n",denoiser.sharpen);  
  mjpeg_log (LOG_INFO, "\n");

}

void turn_on_accels(void)
{
  int CPU_CAP=cpu_accel ();
  
#ifdef HAVE_ASM_MMX
  if( (CPU_CAP & ACCEL_X86_MMXEXT)!=0 ||
      (CPU_CAP & ACCEL_X86_SSE   )!=0 
    ) /* MMX+SSE */
  {
    calc_SAD    = &calc_SAD_mmxe;
    calc_SAD_uv = &calc_SAD_uv_mmxe;
    calc_SAD_half = &calc_SAD_half_mmxe;
    deinterlace = &deinterlace_mmx;
    mjpeg_log (LOG_INFO, "Using extended MMX SIMD optimisations.\n");
  }
  else
    if( (CPU_CAP & ACCEL_X86_MMX)!=0 ) /* MMX */
    {
      calc_SAD    = &calc_SAD_mmx;
      calc_SAD_uv = &calc_SAD_uv_mmx;
      calc_SAD_half = &calc_SAD_half_mmx;
      deinterlace = &deinterlace_mmx;
      mjpeg_log (LOG_INFO, "Using MMX SIMD optimisations.\n");
    }
    else
#endif
    {
      calc_SAD    = &calc_SAD_noaccel;
      calc_SAD_uv = &calc_SAD_uv_noaccel;
      calc_SAD_half = &calc_SAD_half_noaccel;
      deinterlace = &deinterlace_noaccel;
      mjpeg_log (LOG_INFO, "Sorry, no SIMD optimisations available.\n");
    }
}

void
display_help(void)
{
  fprintf(
  stderr,
  "\n\n"
  "Y4M2 - Denoiser Usage:\n"
  "===========================================================================\n"
  "\n"
  "-t <0...255>       Denoiser threshold\n"
  "                   accept any image-error up to +/- threshold for a single\n"
  "                   pixel to be accepted as valid for the image. If the\n"
  "                   absolute error is greater than this, exchange the pixel\n"
  "                   with the according pixel of the reference image.\n"
  "\n"
  "-l <1...255>       Average 'n' frames for a time-lowpassed pixel. Values\n"
  "                   below 2 will lead to a good response to the reference\n"
  "                   frame, while larger values will cut out more noise (and\n"
  "                   as a drawback will lead to noticable artefacts on high\n"
  "                   motion scenes.) Values above 8 are allowed but rather\n"
  "                   useless.\n"
  "\n"
  "-r <8...24>        Limit the search radius to that value. Usually it will\n"
  "                   not make sense to go higher than 16. Esp. for VCD sizes.\n"
  "\n"
  "-b <x>,<y>,<w>,<h> Set active image area. Every pixel outside will be set\n"
  "                   to <16,128,128> (\"pure black\"). This can save a lot of bits\n"
  "                   without even touching the image itself (eg. on 16:9 movies\n"
  "                   on 4:3 (VCD and SVCD)\n"
  "\n"
  "-L <0...255>       Set luminance contrast in percent.\n"
  "\n"
  "-C <0...255>       Set chrominance contrast in percent. AKA \"Saturation\"\n"
  "\n"
  "-S <0...255>       Set sharpness in percent. WARNING: do not set too high\n"
  "                   as this will gain bit-noise.\n"
  "\n"
  "-F                 Force deinterlacing. By default denoise interlaced.\n"
  "\n"
  "-f                 Fast mode. Use only Pass II (bitnoise-reduction) for\n"
  "                   low to very low noise material.\n"
  "\n"
  "-p <0...255>       Pass II threshold (same as -t).\n"
  "                   WARNING: If set to values greater than 8 you *will* see\n"
  "                   artefacts...\n\n"
  );
}
