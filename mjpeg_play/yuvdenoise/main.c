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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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


struct DNSR_GLOBAL denoiser;
int param_skip = 0;

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
  int frame_Coffset;
  int uninitialized = 1;
  int frame = 0;
  
  y4m_frame_info_t frameinfo;
  y4m_stream_info_t streaminfo;
  int chroma_mode;

  /* display greeting */
  display_greeting();
  
  y4m_accept_extensions(1);

  /* initialize stream-information */
  y4m_init_stream_info (&streaminfo);
  y4m_init_frame_info (&frameinfo);
  
  /* setup denoiser's global variables */
  denoiser.radius          = 8;
  denoiser.thresholdY      = 5; /* assume medium noise material */
  denoiser.thresholdCr     = 5;
  denoiser.thresholdCb     = 5;
  denoiser.chroma_flicker  = 1;
  denoiser.pp_threshold    = 4; /* same for postprocessing */
  denoiser.delayY          = 3; /* short delay for good regeneration of rapid sequences */
  denoiser.delayCr         = 3;
  denoiser.delayCb         = 3;
  denoiser.postprocess     = 1;
  denoiser.luma_contrast   = 100;
  denoiser.chroma_contrast = 100;
  denoiser.sharpen         = 0; /* no sharpening by default */
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
      mjpeg_error_exit1("Couldn't read YUV4MPEG header: %s!",y4m_strerr(errno));

  if (y4m_si_get_plane_count(&streaminfo) != 3)
     mjpeg_error_exit1("Only 3 plane formats supported");

  denoiser.frame.w         = y4m_si_get_width(&streaminfo);
  denoiser.frame.h         = y4m_si_get_height(&streaminfo);

  chroma_mode = y4m_si_get_chroma(&streaminfo);
  denoiser.frame.ss_h = y4m_chroma_ss_x_ratio(chroma_mode).d;
  denoiser.frame.ss_v = y4m_chroma_ss_y_ratio(chroma_mode).d;

  denoiser.frame.Cw=denoiser.frame.w/denoiser.frame.ss_h;
  denoiser.frame.Ch=denoiser.frame.h/denoiser.frame.ss_v;

  frame_offset             = BUF_OFF*denoiser.frame.w;
  frame_Coffset            = BUF_COFF*denoiser.frame.Cw;

  if(denoiser.border.w == 0)
  {
      denoiser.border.x        = 0;
      denoiser.border.y        = 0;
      denoiser.border.w        = denoiser.frame.w;
      denoiser.border.h        = denoiser.frame.h;
  }

  /* handle relative border height/width offsets. */
  if ( denoiser.border.w < 0 )
  {
      denoiser.border.w = denoiser.frame.w - denoiser.border.x + denoiser.border.w;
  }
  if ( denoiser.border.h < 0 )
  {
      denoiser.border.h = denoiser.frame.h - denoiser.border.y + denoiser.border.h;
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
  
  /* precompute luma contrast values for lookup table */
  init_luma_contrast_vector();
  
  /* read every single frame until the end of the input stream */
  while 
    (
          Y4M_OK == ( errno = y4m_read_frame (fd_in, 
                                              &streaminfo, 
                                              &frameinfo, 
                                              denoiser.frame.io) )
    )
    {
	  frame++;
	  if (frame <= param_skip)
	  {
         y4m_write_frame ( fd_out, 
                        &streaminfo, 
                        &frameinfo, 
                        denoiser.frame.io);
		 continue;
	  }
      
      /* Move frame down by BUF_OFF lines into reference buffer */
      memcpy(denoiser.frame.ref[Yy]+frame_offset ,denoiser.frame.io[Yy],denoiser.frame.w*denoiser.frame.h  );
      memcpy(denoiser.frame.ref[Cr]+frame_Coffset,denoiser.frame.io[Cr],denoiser.frame.Cw*denoiser.frame.Ch);
      memcpy(denoiser.frame.ref[Cb]+frame_Coffset,denoiser.frame.io[Cb],denoiser.frame.Cw*denoiser.frame.Ch);
      
      if(uninitialized)
      {
        uninitialized=0;
        memcpy(denoiser.frame.avg[Yy]+frame_offset ,denoiser.frame.io[Yy],denoiser.frame.w*denoiser.frame.h  );
        memcpy(denoiser.frame.avg[Cr]+frame_Coffset,denoiser.frame.io[Cr],denoiser.frame.Cw*denoiser.frame.Ch);
        memcpy(denoiser.frame.avg[Cb]+frame_Coffset,denoiser.frame.io[Cb],denoiser.frame.Cw*denoiser.frame.Ch);
        memcpy(denoiser.frame.avg2[Yy]+frame_offset ,denoiser.frame.io[Yy],denoiser.frame.w*denoiser.frame.h  );
        memcpy(denoiser.frame.avg2[Cr]+frame_Coffset,denoiser.frame.io[Cr],denoiser.frame.Cw*denoiser.frame.Ch);
        memcpy(denoiser.frame.avg2[Cb]+frame_Coffset,denoiser.frame.io[Cb],denoiser.frame.Cw*denoiser.frame.Ch);
      }
      
      denoise_frame();
      
      /* Move frame up by BUF_OFF lines into I/O buffer */
      memcpy(denoiser.frame.io[Yy],denoiser.frame.avg2[Yy]+frame_offset ,denoiser.frame.w*denoiser.frame.h );
      memcpy(denoiser.frame.io[Cr],denoiser.frame.avg2[Cr]+frame_Coffset,denoiser.frame.Cw*denoiser.frame.Ch);
      memcpy(denoiser.frame.io[Cb],denoiser.frame.avg2[Cb]+frame_Coffset,denoiser.frame.Cw*denoiser.frame.Ch);

      y4m_write_frame ( fd_out, 
                        &streaminfo, 
                        &frameinfo, 
                        denoiser.frame.io);

    }
    
  /* did stream end unexpectedly ? */
  if(errno != Y4M_ERR_EOF )
          mjpeg_error_exit1( "%s", y4m_strerr( errno ) );
  
  /* just as name says... free them... */
  free_buffers();
  
  /* Exit gently */
  return(0);
}






void allc_buffers(void)
{
  int luma_buffsize; 
  int chroma_buffsize;

  /* now, the MC-functions really(!) do go beyond the vertical
   * frame limits so we need to make the buffers larger to avoid
   * bound-checking (memory vs. speed...)
   */
  
  luma_buffsize = denoiser.frame.w * (denoiser.frame.h+2*BUF_OFF);
  chroma_buffsize = denoiser.frame.Cw * (denoiser.frame.Ch+2*BUF_OFF);
  
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
  mjpeg_log (LOG_INFO, " ======================================================== ");
  mjpeg_log (LOG_INFO, "         Y4M2 Motion-Compensating-YCrCb-Denoiser          ");
  mjpeg_log (LOG_INFO, " ======================================================== ");
  mjpeg_log (LOG_INFO, " Version: MjpegTools %s", VERSION);
}


void
process_commandline(int argc, char *argv[])
{
  char c;
  int i1,i2,i3,i4;

  while ((c = getopt (argc, argv, "h?t:u:v:b:r:l:m:n:c:S:s:L:C:p:Ff")) != -1)
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
  	      mjpeg_log (LOG_WARN, "Minimum allowed search radius is 8 pixel.");
        }
        if(denoiser.radius>24)
        {
  	      mjpeg_log (LOG_WARN, "Maximum suggested search radius is 24 pixel.");
        }
        break;
      }
      case 't':
      {
        denoiser.thresholdY = atoi(optarg);
	denoiser.thresholdCr = denoiser.thresholdY;
	denoiser.thresholdCb = denoiser.thresholdY;
        break;
      }
      case 'u':
      {
        denoiser.thresholdCr = atoi(optarg);
        break;
      }
      case 'v':
      {
        denoiser.thresholdCb = atoi(optarg);
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
      case 's':
      {
        param_skip = atoi(optarg);
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
        denoiser.delayY = atoi(optarg);
        if(denoiser.delayY>8)
        {
  	      mjpeg_log (LOG_WARN, "Maximum suggested frame delay is 8.");
        }
	denoiser.delayCr =denoiser.delayY; 
	denoiser.delayCb =denoiser.delayY; 
        break;
      }
      case 'm':
      {
        denoiser.delayCr = atoi(optarg);
        if(denoiser.delayCr>8)
        {
  	      mjpeg_log (LOG_WARN, "Maximum suggested frame delay is 8.");
        }
        break;
      }
      case 'n':
      {
        denoiser.delayCb = atoi(optarg);
        if(denoiser.delayCb>8)
        {
  	      mjpeg_log (LOG_WARN, "Maximum suggested frame delay is 8.");
        }
        break;
      }
      case 'c':
      {
        denoiser.chroma_flicker = atoi(optarg);
        break;
      }
    }
  }
    
}


void print_settings(void)
{
  mjpeg_log (LOG_INFO, " ");
  mjpeg_log (LOG_INFO, " Denoiser - Settings:");
  mjpeg_log (LOG_INFO, " --------------------");
  mjpeg_log (LOG_INFO, " ");
  mjpeg_log (LOG_INFO, " Mode             : %s",
    (denoiser.mode==0)? "Progressive frames" : (denoiser.mode==1)? "Interlaced frames": "PASS II only");
  mjpeg_log (LOG_INFO, " Deinterlacer     : %s",(denoiser.deinterlace==0)? "Off":"On");
  mjpeg_log (LOG_INFO, " Postprocessing   : %s",(denoiser.postprocess==0)? "Off":"On");
  mjpeg_log (LOG_INFO, " Y frame size     : w:%3i h:%3i",denoiser.frame.w,denoiser.frame.h);
  mjpeg_log (LOG_INFO, " CbCr frame size  : w:%3i h:%3i",denoiser.frame.Cw,denoiser.frame.Ch);
  mjpeg_log (LOG_INFO, " Frame border     : x:%3i y:%3i w:%3i h:%3i",denoiser.border.x,denoiser.border.y,denoiser.border.w,denoiser.border.h);
  mjpeg_log (LOG_INFO, " Search radius    : %3i",denoiser.radius);
  mjpeg_log (LOG_INFO, " Y  Filter delay     : %3i",denoiser.delayY);
  mjpeg_log (LOG_INFO, " Cr Filter delay     : %3i",denoiser.delayCr);
  mjpeg_log (LOG_INFO, " Cb Filter delay     : %3i",denoiser.delayCb);
  mjpeg_log (LOG_INFO, " Y  Filter threshold : %3i",denoiser.thresholdY);
  mjpeg_log (LOG_INFO, " Cr Filter threshold : %3i",denoiser.thresholdCr);
  mjpeg_log (LOG_INFO, " Cb Filter threshold : %3i",denoiser.thresholdCb);
  mjpeg_log (LOG_INFO, " Pass 2 threshold : %3i",denoiser.pp_threshold);
  mjpeg_log (LOG_INFO, " Y - contrast     : %3i %%",denoiser.luma_contrast);
  mjpeg_log (LOG_INFO, " Cr/Cb - contrast : %3i %%",denoiser.chroma_contrast);
  mjpeg_log (LOG_INFO, " Sharpen          : %3i %%",denoiser.sharpen);  
  mjpeg_log (LOG_INFO, " ");

}

void turn_on_accels(void)
{
#ifdef HAVE_ASM_MMX
  int CPU_CAP=cpu_accel ();
  
  if( (CPU_CAP & ACCEL_X86_MMXEXT)!=0 ||
      (CPU_CAP & ACCEL_X86_SSE   )!=0 
    ) /* MMX+SSE */
  {
    calc_SAD    = &calc_SAD_mmxe;
    if (denoiser.frame.Cw==denoiser.frame.w/4 && denoiser.frame.Ch==denoiser.frame.h)
      {
	mjpeg_log (LOG_INFO, "Using 4:1:1 extended MMX SIMD optimisations.");
	calc_SAD_uv = &calc_SAD_uv411_mmxe;
      }
    else
      {
	mjpeg_log (LOG_INFO, "Using 4:2:0 extended MMX SIMD optimisations.");
	calc_SAD_uv = &calc_SAD_uv420_mmxe;
      }
    calc_SAD_half = &calc_SAD_half_mmxe;
    deinterlace = &deinterlace_mmx;
  }
  else
    if ( (CPU_CAP & ACCEL_X86_MMX)!=0 ) /* MMX */
    {
      calc_SAD    = &calc_SAD_mmx;
      if (denoiser.frame.Cw==denoiser.frame.w/4 && denoiser.frame.Ch==denoiser.frame.h)
	{
	  mjpeg_log (LOG_INFO, "Using 4:1:1 MMX SIMD optimisations.");
	  calc_SAD_uv = &calc_SAD_uv411_mmx;
	}
      else
	{
	  mjpeg_log (LOG_INFO, "Using 4:2:0 MMX SIMD optimisations.");
	  calc_SAD_uv = &calc_SAD_uv420_mmx;
	}
      calc_SAD_half = &calc_SAD_half_mmx;
      deinterlace = &deinterlace_mmx;
    }
    else
#endif
    {
      calc_SAD    = &calc_SAD_noaccel;
      calc_SAD_uv = &calc_SAD_uv_noaccel;
      calc_SAD_half = &calc_SAD_half_noaccel;
      deinterlace = &deinterlace_noaccel;
      mjpeg_log (LOG_INFO, "Sorry, no SIMD optimisations available.");
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
  "-t <0...255>       Y Denoiser threshold\n"
  "                   accept any image-error up to +/- threshold for a single\n"
  "                   pixel to be accepted as valid for the image. If the\n"
  "                   absolute error is greater than this, exchange the pixel\n"
  "                   with the according pixel of the reference image.\n"
  "                   (default=%i)"
  "\n"
  "-u <0...255>       Cr Denoiser threshold\n"
  "                   (default=%i)"
  "\n"
  "-v <0...255>       Cb Denoiser threshold\n"
  "                   (default=%i)"
  "\n"
  "-c <0|1>           Chroma-flicker reduction\n"
  "                   (default=%i)"
  "\n"
  "-l <0...255>       Average 'n' Y frames for a time-lowpassed pixel. Values\n"
  "                   below 2 will lead to a good response to the reference\n"
  "                   frame, while larger values will cut out more noise (and\n"
  "                   as a drawback will lead to noticable artefacts on high\n"
  "                   motion scenes.) Values above 8 are allowed but rather\n"
  "                   useless. (default=%i)\n"
  "\n"
  "-m <0...255>       Average 'n' Cr frames for a time-lowpassed pixel.\n"
  "                   (default=%i)\n"
  "\n"
  "-n <0...255>       Average 'n' Cb frames for a time-lowpassed pixel.\n"
  "                   (default=%i)\n"
  "\n"
  "-r <8...24>        Limit the search radius to that value. Usually it will\n"
  "                   not make sense to go higher than 16. Esp. for VCD sizes.\n"
  "                   (default=%i)"
  "\n"
  "-b <x>,<y>,<w>,<h> Set active image area. Every pixel outside will be set\n"
  "                   to <16,128,128> (\"pure black\"). This can save a lot of bits\n"
  "                   without even touching the image itself (eg. on 16:9 movies\n"
  "                   on 4:3 (VCD and SVCD) (default=%i,%i,%i,%i)\n"
  "\n"
  "-L <0...255>       Set luminance contrast in percent. (default=%i)\n"
  "                   100 = no effect\n"
  "\n"
  "-C <0...255>       Set chrominance contrast in percent. AKA \"Saturation\"\n"
  "                   (default=%i) 100 = no effect\n"
  "\n"
  "-S <0...255>       Set sharpness in percent. WARNING: do not set too high\n"
  "                   as this will gain bit-noise. (default=%i)\n"
  "                   0 = no effect\n"
  "\n"
  "-F                 Force deinterlacing. By default denoise interlaced.\n"
  "\n"
  "-f                 Fast mode. Use only Pass II (bitnoise-reduction) for\n"
  "                   low to very low noise material. (default off)\n"
  "\n"
  "-p <0...255>       Pass II threshold (same as -t).\n"
  "                   WARNING: If set to values greater than 8 you *will* see\n"
  "                   artefacts...(default=%i)\n\n",
  denoiser.thresholdY,
  denoiser.thresholdCr,
  denoiser.thresholdCb,
  denoiser.chroma_flicker,
  denoiser.delayY,
  denoiser.delayCr,
  denoiser.delayCb,
  denoiser.radius,
  denoiser.border.x,
  denoiser.border.y,
  denoiser.border.w,
  denoiser.border.h,
  denoiser.luma_contrast,
  denoiser.chroma_contrast,
  denoiser.sharpen,
  denoiser.pp_threshold
  );
}
