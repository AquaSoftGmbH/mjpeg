
/****************************************************************************
 *                                                                          *
 * Yuv-Denoiser to be used with Andrew Stevens mpeg2enc                     *
 *                                                                          *
 * (C)2001 S.Fendt 				                                            *
 *                                                                          *
 * Licensed and protected by the GNU-General-Public-License version 2       *
 * (or if you prefer any later version of that license). See the file       *
 * LICENSE for detailed information.                                        *
 *                                                                          *
 ****************************************************************************/

/* Anjuta is most cool ! */

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include "mjpeg_types.h"
#include "yuv4mpeg.h"
#include "mjpeg_logging.h"
#include "cpu_accel.h"
#include "mmx.h"
#include "config.h"

/*****************************************************************************
 * if this is commented out, a lot of messages                               *
 * should apear on the screen...                                             *
 *****************************************************************************/

/* #define DEBUG */



/*****************************************************************************
 * here the search-blocksize is defined. The blocks which will be exchanged  *
 * will have the half size of the search block                               *
 * search radius is defined here, too. If you insist to change the blocksize *
 * you have to turn off MMX/SSE2 accelerations !!!                           *
 *****************************************************************************/

#define BLOCKSIZE     8
#define BLOCKOFFSET   (BLOCKSIZE-(BLOCKSIZE/2))/2
#define RADIUS        24



/*****************************************************************************
 * Pointer definitions to some used buffers. These are allocated later and   *
 * (hopefully) freed all at the end of the program.                          *
 *****************************************************************************/

uint8_t *yuv[3];
uint8_t *yuv1[3];
uint8_t *yuv2[3];
uint8_t *avrg[3];
uint8_t *sub_r2[3];
uint8_t *sub_t2[3];
uint8_t *sub_r4[3];
uint8_t *sub_t4[3];

uint8_t *line_buf;

/*****************************************************************************
 * a frame counter is needed in calculate_motion_vectors, as we don't like   *
 * continious scene changes ... even if there isn't one. :)                  *
 *****************************************************************************/

uint32_t framenr = 0;



/*****************************************************************************
 * four global flags which control the behaveiour of the denoiser.           *
 *****************************************************************************/

int deinterlace_only = 0;       /* set to 1 if only deinterlacing needed */
int deinterlace = 0;            /* set to 1 if deinterlacing needed */
int scene_change = 0;           /* set to 1 if a scene change was detected */
int fixed_search_radius=0;      /* set to 1 if fixed search radius should be used */
int despeckle_filter = 1;       /* set to 0 if despeckle filter should not be used */
int verbose = 0;                /* set to 1 and it's very noisy on stderr... */
int CPU_CAP = 0;                /* 0== no Accel - see cpu_accel.h  */

/*****************************************************************************
 * some global variables needed for handling the YUV planar frames.          *
 *****************************************************************************/

int width;                      /* width of the images */
int height;                     /* height of the images */
int u_offset;                   /* offset of the U-component from beginning of the image */
int v_offset;                   /* offset of the V-component from beginning of the image */
int uv_width;                   /* width of the UV-components */
int uv_height;                  /* height of the UV-components */



/*****************************************************************************
 * global variables needed for the motion estimation                         *
 *****************************************************************************/

uint32_t mean_SAD=100;          /* mean sum of absolute differences */
int mod_radius=RADIUS;          /* initial modulated radius */
int radius=RADIUS;              /* initial fixed radius */
float block_quality;
int BX0=0;
int BY0=0;
int BX1=0;
int BY1=0;


/*****************************************************************************
 * matrices to store various block-results                                   *
 *****************************************************************************/

int matrix[1024][768][2];
uint32_t SAD_matrix[1024][768];



/*****************************************************************************
 * declaration of used functions                                             *
 *****************************************************************************/

void display_greeting ();
void display_help ();
void denoise_frame (uint8_t * ref[3]);
void antiflicker_reference (uint8_t * frame[3]);
void despeckle_frame_hard (uint8_t * frame[3]);
void despeckle_frame_soft (uint8_t * frame[3]);
void generate_black_border(int BX0, int BY0, int BX1, int BY1, uint8_t *frame[3]);
void preblocking_frame (uint8_t * avg[3]);
int test_CPU ();

/* SAD functions */
uint32_t
calc_SAD_noaccel (uint8_t * frm, 
                     uint8_t * ref, 
                     uint32_t frm_offs, 
                     uint32_t ref_offs, 
                     int div);
uint32_t
calc_SAD_half_noaccel (uint8_t * frm, 
		     uint8_t * ref, 
		     uint32_t frm_offs, 
		     uint32_t ref_offs, 
		     int xx, 
		     int yy);
#ifdef HAVE_X86CPU                     
uint32_t
calc_SAD_mmx     (uint8_t * frm, 
                     uint8_t * ref, 
                     uint32_t frm_offs, 
                     uint32_t ref_offs, 
                     int div);
                     
uint32_t
calc_SAD_mmxe     (uint8_t * frm, 
                     uint8_t * ref, 
                     uint32_t frm_offs, 
 		     uint32_t ref_offs,
                     int div);
uint32_t
calc_SAD_half_mmx (uint8_t * frm, 
		     uint8_t * ref, 
		     uint32_t frm_offs, 
		     uint32_t ref_offs, 
		     int xx, 
		     int yy);
uint32_t
calc_SAD_half_mmxe (uint8_t * frm, 
		     uint8_t * ref, 
		     uint32_t frm_offs, 
		     uint32_t ref_offs, 
		     int xx, 
		     int yy);
#endif

/* pointer on them */
uint32_t (*calc_SAD) (uint8_t * frm, 
                     uint8_t * ref, 
                     uint32_t frm_offs, 
                     uint32_t ref_offs, 
                     int div);
uint32_t (*calc_SAD_half) (uint8_t * frm, 
		     uint8_t * ref, 
		     uint32_t frm_offs, 
		     uint32_t ref_offs, 
		     int xx, 
		     int yy);

/* SAD_uv functions */
uint32_t
calc_SAD_uv_noaccel (uint8_t * frm, 
                     uint8_t * ref, 
                     uint32_t frm_offs, 
                     uint32_t ref_offs, 
                     int div);
#ifdef HAVE_X86CPU                     
                     
uint32_t
calc_SAD_uv_mmx     (uint8_t * frm, 
                     uint8_t * ref, 
                     uint32_t frm_offs, 
                     uint32_t ref_offs, 
                     int div);
                     
uint32_t
calc_SAD_uv_mmxe     (uint8_t * frm, 
                     uint8_t * ref, 
                     uint32_t frm_offs, 
                     uint32_t ref_offs, 
                     int div);
#endif

/* pointer on them */
uint32_t (*calc_SAD_uv) (uint8_t * frm, 
                     uint8_t * ref, 
                     uint32_t frm_offs, 
                     uint32_t ref_offs, 
                     int div);

/* Deinterlacers and pointer to them */
void deinterlace_frame_noaccel (uint8_t * yuv[3]);

#ifdef HAVE_X86CPU                     
void deinterlace_frame_mmxe (uint8_t * yuv[3]);
void deinterlace_frame_mmx (uint8_t * yuv[3]);
#endif

void (*deinterlace_frame) (uint8_t * yuv[3]);


static uint8_t *bufalloc(size_t size)
{
	uint8_t *ret = (uint8_t *)malloc(size);
	if( ret == NULL )
		mjpeg_error_exit1( "Out of memory: could not allocate buffer\n" );
	return ret;
}

/*****************************************************************************
 * MAIN                                                                      *
 *****************************************************************************/

int
main (int argc, char *argv[])
{
  int fd_in = 0;
  int fd_out = 1;
  int luma_bufsize;
  int chroma_bufsize;
  int errnr;
  char c;
  int i;
  y4m_frame_info_t frameinfo;
  y4m_stream_info_t streaminfo;

  y4m_init_stream_info(&streaminfo);
  y4m_init_frame_info(&frameinfo);

  display_greeting ();
  CPU_CAP=cpu_accel ();
  
  mjpeg_log ( LOG_INFO, "\n");
#ifdef HAVE_X86CPU
  if( (CPU_CAP & ACCEL_X86_MMXEXT)!=0 ||
      (CPU_CAP & ACCEL_X86_SSE   )!=0 
    ) /* MMX+SSE */
  {
    calc_SAD    = &calc_SAD_mmxe;
    calc_SAD_uv = &calc_SAD_uv_mmxe;
    calc_SAD_half = &calc_SAD_half_mmxe;
    deinterlace_frame = &deinterlace_frame_mmxe;
    mjpeg_log (LOG_INFO, "Extended MMX SIMD optimisations.\n");
  }
  else
    if( (CPU_CAP & ACCEL_X86_MMX)!=0 ) /* MMX */
    {
      calc_SAD    = &calc_SAD_mmx;
      calc_SAD_uv = &calc_SAD_uv_mmx;
      calc_SAD_half = &calc_SAD_half_mmx;
      deinterlace_frame = &deinterlace_frame_mmx;
      mjpeg_log (LOG_INFO, "Using MMX SIMD optimisations.\n");
    }
    else
#endif
    {
      calc_SAD    = &calc_SAD_noaccel;
      calc_SAD_uv = &calc_SAD_uv_noaccel;
      calc_SAD_half = &calc_SAD_half_noaccel;
      deinterlace_frame = &deinterlace_frame_noaccel;
      mjpeg_log (LOG_INFO, "No SIMD optimisations available.\n");
    }

  /* initialize stream-information */

  y4m_init_stream_info (&streaminfo);
  y4m_init_frame_info (&frameinfo);

  if ((errnr = y4m_read_stream_header (fd_in, &streaminfo)) != Y4M_OK)
    {
      mjpeg_log (LOG_ERROR, "Couldn't read YUV4MPEG header: %s!\n", y4m_strerr (errnr));
      exit (1);
    }

  /* calculate frame constants used for YUV-processing */

  width = y4m_si_get_width(&streaminfo);
  height = y4m_si_get_height(&streaminfo);
  u_offset = width * height;
  v_offset = width * height * 1.25;
  uv_width = width >> 1;
  uv_height = height >> 1;

  /* process commandline */

  while ((c = getopt (argc, argv, "?hvb:r:fDI")) != -1)
    {
      switch (c)
        {
        case '?':
          display_help ();
          break;
        case 'h':
          display_help ();
          break;
        case 'f':
          fixed_search_radius=1;
          mjpeg_log (LOG_INFO, "Using fixed radius. Radius optimization turned off.\n");
          break;
        case 'D':
          despeckle_filter=0;
          mjpeg_log (LOG_INFO, "Turning off despeckle filter.\n");
          break;
        case 'I':
          deinterlace_only=1;
          mjpeg_log (LOG_INFO, "Turning off denoising filter.\n");
          mjpeg_log (LOG_INFO, "Only deinterlacing material.\n");
          break;
        case 'v':
          verbose = 1;
          break;
        case 'b':
          sscanf( optarg, "%i,%i,%i,%i", &BX0,&BY0,&BX1,&BY1);
	  if(BX1<0) BX1=width+BX1;
	  if(BY1<0) BY1=height+BY1;

          mjpeg_log (LOG_INFO, "Search area set to: (%i,%i)-(%i,%i)\n",BX0,BY0,BX1,BY1);
          break;
        case 'r':
          radius = atoi(optarg)*2;
          if(radius<8)
          {
            mjpeg_log (LOG_WARN, "Minimum allowed search radius is 4 pixel.\n");
            radius=8;
          }
          if(radius>256)
          {
            mjpeg_log (LOG_WARN, "Maximum sensfull search radius is 128 pixel.\n");
            radius=256;
          }
          mjpeg_log (LOG_INFO, "Maximum search radius set to %i pixel.\n",radius/2);
          break;
        }
    }

  /* was a border set ? */
  if(BX0==0 && BY0==0 && BX1==0 && BY1==0)
  {
    BX1=width;
    BY1=height;
  }
  else
  {
    BX0 = (BX0<0)? 0:BX0;
    BX0 = (BX0>width)? width:BX0; /* who would do this ??? */
    BY0 = (BY0<0)? 0:BY0;
    BY0 = (BY0>height)? height:BY0;
    BX1 = (BX1<0)? 0:BX1;
    BX1 = (BX1>width)? width:BX1; 
    BY1 = (BY1<0)? 0:BY1;
    BY1 = (BY1>height)? height:BY1;
  }
  
  /* Allocate memory for buffers.  We add a "safety margin" of
	 16 bytes because some SIMD block moves may "overshoot" a little.
	 This doesn't matter for the image as we fill frame buffers 
	 from top to bottom, but we want to avoid messing up the
	 heap manager... 
   */

  luma_bufsize = width * height * sizeof (uint8_t)+16;
  chroma_bufsize = width * height * sizeof (uint8_t)/4+16;
  yuv[0] = bufalloc (luma_bufsize);
  yuv[1] = bufalloc (chroma_bufsize);
  yuv[2] = bufalloc (chroma_bufsize);

  yuv1[0] = bufalloc (luma_bufsize);
  yuv1[1] = bufalloc (chroma_bufsize);
  yuv1[2] = bufalloc (chroma_bufsize);

  yuv2[0] = bufalloc (luma_bufsize);
  yuv2[1] = bufalloc (chroma_bufsize);
  yuv2[2] = bufalloc (chroma_bufsize);

  avrg[0] = bufalloc (luma_bufsize);
  avrg[1] = bufalloc (chroma_bufsize);
  avrg[2] = bufalloc (chroma_bufsize);

  sub_t2[0] = bufalloc (luma_bufsize);
  sub_t2[1] = bufalloc (chroma_bufsize);
  sub_t2[2] = bufalloc (chroma_bufsize);

  sub_r2[0] = bufalloc (luma_bufsize);
  sub_r2[1] = bufalloc (chroma_bufsize);
  sub_r2[2] = bufalloc (chroma_bufsize);

  sub_t4[0] = bufalloc (luma_bufsize);
  sub_t4[1] = bufalloc (chroma_bufsize);
  sub_t4[2] = bufalloc (chroma_bufsize);

  sub_r4[0] = bufalloc (luma_bufsize);
  sub_r4[1] = bufalloc (chroma_bufsize);
  sub_r4[2] = bufalloc (chroma_bufsize);

  line_buf = bufalloc( width+16 );

/* if needed, deinterlace frame */

  if (y4m_si_get_interlace(&streaminfo) != Y4M_ILACE_NONE)
    {
      mjpeg_log (LOG_INFO, "stream read is interlaced: using deinterlacer.\n");
      mjpeg_log (LOG_INFO, "stream written is frame-based.\n");

      /* setting Y4M header to non-interlaced video */
      y4m_si_set_interlace(&streaminfo, Y4M_ILACE_NONE);

      /* turning on deinterlacer */
      deinterlace = 1;
    }

/* write the outstream header */

  y4m_write_stream_header (fd_out, &streaminfo);

/* read every single frame until the end of the input stream */

  while ((errnr = y4m_read_frame (fd_in, &streaminfo, &frameinfo, yuv)) == Y4M_OK)
    {
      /* process the frame */

      /* if needed, deinterlace frame */
      if (deinterlace)
		  (*deinterlace_frame) (yuv);

      /* should we only deinterlace frames ? */
      if (!deinterlace_only)
      {
	  /* deflicker */
	  //antiflicker_reference (yuv);

	  /* main denoise processing */
	  denoise_frame (yuv);

	  /* despeckling */
	  if (scene_change && despeckle_filter)
	      despeckle_frame_hard (yuv);
	  else
	      if(despeckle_filter)
		  despeckle_frame_soft (yuv);
      }

      //preblocking_frame (yuv);

      generate_black_border(BX0,BY0,BX1,BY1,yuv);

// debugging only
#if 0
      {
	  int ix,iy;
	      for(iy=0;iy<height/2;iy++)
		  for(ix=0;ix<width/2;ix++)
		  {
		      *(yuv[0]+ix+iy*width)=*(sub_r2[0]+ix+(iy*width/2));
		      *(yuv[1]+ix/2+iy/2*uv_width)=*(sub_r2[1]+ix/2+(iy/2*uv_width/2));
		      *(yuv[2]+ix/2+iy/2*uv_width)=*(sub_r2[2]+ix/2+(iy/2*uv_width/2));
		  }
      }
#endif

      /* write the frame */
      y4m_write_frame (fd_out, &streaminfo, &frameinfo, yuv);
    }

  if(  errnr !=  Y4M_ERR_EOF )
	  mjpeg_error_exit1( "%s\n", y4m_strerr( errnr ) );
  /* free all used buffers and structures */

  y4m_fini_stream_info(&streaminfo);
  y4m_fini_frame_info(&frameinfo);

  for (i = 0; i < 3; i++)
    {
      free (yuv[i]);
      free (yuv1[i]);
      free (yuv2[i]);
      free (avrg[i]);
      free (sub_t2[i]);
      free (sub_r2[i]);
      free (sub_t4[i]);
      free (sub_r4[i]);
    }

  /* Exit with zero status */
  return (0);
}



/*****************************************************************************
 * greeting - message                                                        *
 *****************************************************************************/

void
display_greeting ()
{
  mjpeg_log (LOG_INFO, "------------------------------------ \n");
  mjpeg_log (LOG_INFO, "Y4M Motion-Compensating-YUV-Denoiser \n");
  mjpeg_log (LOG_INFO, "------------------------------------ \n");
  mjpeg_log (LOG_INFO, "Version: MjpegTools %s\n", VERSION);
  mjpeg_log (LOG_INFO, "\n");
  mjpeg_log (LOG_INFO, "This is development software. It may \n");
  mjpeg_log (LOG_INFO, "corrupt your movies. \n");
  mjpeg_log (LOG_INFO, "------------------------------------ \n");
}

/*****************************************************************************
 * help - message                                                            *
 *****************************************************************************/

void
display_help ()
{
  fprintf (stderr,"\n ----------------------------------------------------------- ");
  fprintf (stderr,"\n yuvdenoise - A brief summery of options                     ");
  fprintf (stderr,"\n ----------------------------------------------------------- ");
  fprintf (stderr,"\n                                                             ");
  fprintf (stderr,"\n Usage :  yuvdenoise [options]                               ");
  fprintf (stderr,"\n                                                             ");
  fprintf (stderr,"\n Currently supported options are:                            ");
  fprintf (stderr,"\n                                                             ");
  fprintf (stderr,"\n                                                             ");
  fprintf (stderr,"\n -v  Verbose output.                                         ");
  fprintf (stderr,"\n     You will get some statistical information on the input  ");
  fprintf (stderr,"\n     video-stream as mean-SAD so far and the average SAD of  ");
  fprintf (stderr,"\n     the last image. Additionaly you will have a translation ");
  fprintf (stderr,"\n     of these values on a single Pixel (Noiselevel).         ");
  fprintf (stderr,"\n                                                             ");
  fprintf (stderr,"\n                                                             ");
  fprintf (stderr,"\n -b  Border Setting.                                         ");
  fprintf (stderr,"\n     If you allready know parts of the image are and remain  ");
  fprintf (stderr,"\n     black all over the complete sequence of images, you may ");
  fprintf (stderr,"\n     use something like this:                                ");
  fprintf (stderr,"\n                                                             ");
  fprintf (stderr,"\n     yuvdenoise -b 16,16,704,560                             ");
  fprintf (stderr,"\n                                                             ");
  fprintf (stderr,"\n     This will set all the pixels in the image outside that  ");
  fprintf (stderr,"\n     search window to absolute pure black. The area outside  ");
  fprintf (stderr,"\n     the search window will not go into the motion compen-   ");
  fprintf (stderr,"\n     sation algorithm and therefor you might have a speed    ");
  fprintf (stderr,"\n     gain in processing the movie.                           ");
  fprintf (stderr,"\n                                                             ");
  fprintf (stderr,"\n                                                             ");
  fprintf (stderr,"\n -r  Maximum Search Radius.                                  ");
  fprintf (stderr,"\n     By adding this option you may limit the search radius.  ");
  fprintf (stderr,"\n     Search radius may internally be reduced by the algorithm");
  fprintf (stderr,"\n     depending on the quality of the center match.           ");
  fprintf (stderr,"\n                                                             ");
  fprintf (stderr,"\n     yuvdenoise -r 16                                        ");
  fprintf (stderr,"\n                                                             ");
  fprintf (stderr,"\n     Sets the maximum search Radius to 16 pixels.            ");
  fprintf (stderr,"\n                                                             ");
  fprintf (stderr,"\n                                                             ");
  fprintf (stderr,"\n -f  Fixed Search Radius.                                    ");
  fprintf (stderr,"\n     By adding this option you may advise the filter to use  ");
  fprintf (stderr,"\n     a fixed radius for searching block matches. This dis-   ");
  fprintf (stderr,"\n     ables the internal radius optimization/reduction.       ");
  fprintf (stderr,"\n                                                             ");
  fprintf (stderr,"\n                                                             ");
  fprintf (stderr,"\n -D  Disable Additional Despeckle Filter.                    ");
  fprintf (stderr,"\n     This option allows you to turn off the second pass of   ");
  fprintf (stderr,"\n     the noisefilter.                                        ");
  fprintf (stderr,"\n");
  fprintf (stderr,"\n");
  exit (0);
}


/*****************************************************************************
 * Line-Deinterlacer                                                         *
 * 2001 S.Fendt                                                              *
 * it does a better job than just dropping lines but sometimes it            *
 * fails and reduces y-resolution... hmm... but everything else is           *
 * supposed to fail, too, sometimes, and is #much# more time consuming.      *
 * 2001 A.Stevens.  Not a bad algorithm!  The only real improvements that    *
 * are possible are to add full 2D motion compensation, and to use a low-pass *
 * match-filter to control the mixing gain in the construction of the        *
 * deinterlaced line.                                                        *
 *****************************************************************************/

void
deinterlace_frame_noaccel (uint8_t * yuv[3])
{

  unsigned int d;
  unsigned int min;
  int x;
  int y;
  int xx;
  int i;
  int xpos;
  int l1;
  int l2;
  int lumadiff = 0;
  uint8_t *line = line_buf;

  /* Go through the frame by every two lines */
  for (y = 0; y < height; y += 2)
    {
      /* Go through each line by a "block" length of 32 pixels */
      for (x = 0; x < width; x += 8)
        {
          /* search best matching block of pixels in other field line */
          min = 65535;
          xpos = 0;
          /* search in the range of +/- 16 pixels offset in the line */

          for (xx = -16; xx < 16; xx++)
            {
              d = 0;
              /* Calculate SAD */
              for (i = -8; i < 16; i++)
                {
                  /* to avoid blocking in ramps we analyse the best match on */
                  /* two lines ... */
                  d += (int) abs (*(yuv[0] + (x + i) + y * width) -
                                  *(yuv[0] + (x + xx + i) + (y + 1) * width));
                  d += (int) abs (*(yuv[0] + (x + i) + (y + 2) * width) -
                                  *(yuv[0] + (x + xx + i) + (y + 1) * width));
                }


              /* if SAD reaches a minimum store the position */
              if (min > d)
                {
                  min = d;
                  xpos = xx;
                  l1 = l2 = 0;
                  for (i = 0; i < 8; i++)
                    {
                      l1 += *(yuv[0] + (x + i) + y * width);
                      l2 += *(yuv[0] + (x + i + xpos) + (y + 1) * width);
                    }
                  l1 /= 8;
                  l2 /= 8;
                  lumadiff = abs (l1 - l2);
                  lumadiff = (lumadiff < 6) ? 0 : 1;
                }
            }

          /* copy pixel-block into the line-buffer */

          /* if lumadiff is small take the fields block, if not */
          /* take the other fields block */

          if (lumadiff && min > (8 * 24))
            for (i = 0; i < 8; i++)
              {
                *(line + x + i) =
                  (*(yuv[0] + (x + i) + ((y) * width)) >>1) +
                  (*(yuv[0] + (x + i) + ((y + 2) * width))>>1) + 1;
              }
          else
            for (i = 0; i < 8; i++)
              {
                *(line + x + i) =
                  (*(yuv[0] + (x + i + xpos) + ((y + 1) * width)) >>1) +
                  (*(yuv[0] + (x + i) + ((y + 0) * width)) >> 1) + 1;
              }

        }

      /* copy the line-buffer into the source-line */
      for (i = 0; i < width; i++)
        *(yuv[0] + i + (y + 1) * width) = *(line + i);
    }
}

#ifdef HAVE_X86CPU                     

/*********************************
 *
 * MMX and extended MMX versions of Stefan's deinterlacer...
 * (C) 2001 A.Stevens, same license as rest
 *
 ********************************/

void
deinterlace_frame_mmxe (uint8_t * yuv[3])
{

  /* Buffer one line */
  unsigned int dd;
  unsigned int min;
  int x;
  int y;
  int xx;
  int i;
  int xpos;
  int l1d,l2d;
#ifdef ORIGINAL_CODE
  int l2,l2,d;
#endif
  int lumadiff = 0;
  uint8_t *base;
  uint8_t *row;
  uint8_t *tmp_addr;
  uint8_t *line = line_buf;

  /* Go through the frame by every two lines */
  for (y = 0; y < height; y += 2)
    {
      /* Go through each line by a "block" length of 32 pixels */
      for (x = 0; x < width; x += 8)
        {
          /* search best matching block of pixels in other field line */
          min = 65535;
          xpos = 0;
          /* search in the range of +/- 16 pixels offset in the line */

		  base = (yuv[0] + x + y * width);
		  row = base + width;

          for (xx = -16; xx < 16; xx++)
            {
#ifdef ORIGINAL_CODE
              d = 0;
              /* Calculate SAD */
              for (i = -8; i < 16; i++)
                {
                  /* to avoid blocking in ramps we analyse the best match on */
                  /* two lines ... */

                  d += (int) abs (*(yuv[0] + (x + i) + y * width) -
                                  *(yuv[0] + (x + xx + i) + (y + 1) * width));
                  d += (int) abs (*(yuv[0] + (x + i) + (y + 2) * width) -
                                  *(yuv[0] + (x + xx + i) + (y + 1) * width));
                }


			  if( row + xx != (yuv[0] + (x + xx ) + (y + 1) * width) ||
				  base != yuv[0] + (x ) + y * width)
			  {
				  fprintf( stderr, "row is wrong.. %08x %08x\n",
						   base, (yuv[0] + (x + xx ) + (y + 1) * width));
				  exit(1);
			  }
#endif

			  tmp_addr = &base[2*width];

			  				/* Calculate SAD through mm7 */
			  movq_m2r( row[xx-8], mm0 );
			  movq_r2r( mm0, mm7 );
			  psadbw_m2r( base[-8], mm7 );
			  movq_m2r( row[xx], mm6 );
			  movq_r2r( mm6, mm1 );
			  psadbw_m2r( base[0], mm6 );
			  paddw_r2r( mm6, mm7 );
			  movq_m2r( row[xx+8], mm6 );
			  movq_r2r( mm6, mm2 );
			  psadbw_m2r( base[8], mm6 );
			  paddw_r2r( mm6, mm7 );
			  
			  psadbw_m2r( tmp_addr[-8], mm0 );
			  paddw_r2r( mm0, mm7 );
			  psadbw_m2r( tmp_addr[0], mm1 );
			  paddw_r2r( mm1, mm7 );
			  psadbw_m2r( tmp_addr[8], mm2 );
			  paddw_r2r( mm2, mm7 );
			  movd_r2m(mm7, dd);

#ifdef ORIGINAL_CODE
			  if( d != dd )
			  {
				  fprintf( stderr, "MMX2 res is wrong.. %08x %08x\n",
						   dd, d);
				  exit(1);
			  }
#endif
              /* if SAD reaches a minimum store the position */
              if (min > dd)
			  {
				  tmp_addr = &base[width+xx];
                  min = dd;
                  xpos = xx;
				  movq_m2r( base[0], mm0 );
				  movq_m2r( *tmp_addr, mm4 );
				
				  movq_r2r( mm0, mm1 );
				  movq_r2r( mm4, mm5 );

				  pxor_r2r( mm7, mm7 );
				  punpcklbw_r2r( mm7, mm0 );
				  punpckhbw_r2r( mm7, mm1 );
				  punpcklbw_r2r( mm7, mm4 );
				  punpckhbw_r2r( mm7, mm5 );
				  
				  paddw_r2r( mm1, mm0 );
				  paddw_r2r( mm5, mm4 );

				  movq_r2r( mm0, mm1 );
				  psrlq_i2r( 32, mm0 );

				  movq_r2r( mm4, mm5 );
				  psrlq_i2r( 32, mm4 );
				  
				  paddw_r2r( mm1, mm0 );
				  paddw_r2r( mm5, mm4 );

				  movq_r2r( mm0, mm1 );
				  psrlq_i2r( 16, mm0 );

				  movq_r2r( mm4, mm5 );
				  psrlq_i2r( 16, mm4 );

				  paddw_r2r( mm1, mm0 );
				  paddw_r2r( mm5, mm4 );
				  
				  movd_r2m( mm0, l1d );
				  movd_r2m( mm4, l2d );
				  l1d = (l1d & 0xffff)/8;
				  l2d = (l2d & 0xffff)/8;
#ifdef ORIGINAL_CODE
                  l1 = l2 = 0;
                  for (i = 0; i < 8; i++)
                    {
                      l1 += *(yuv[0] + (x + i) + y * width);
                      l2 += *(yuv[0] + (x + i + xpos) + (y + 1) * width);
                    }
                  l1 /= 8;
                  l2 /= 8;
				  if( l1 != l1d || l2 != l2d )
				  {
					  fprintf( stderr, "LUM DIFF %d %d %d %d\n", l1, l1d, l2, l2d);
				  }
#endif
                  lumadiff = abs(l1d - l2d);
                  //lumadiff = (lumadiff < 6) ? 0 : 1;
                }

            }

          /* copy pixel-block into the line-buffer */

          /* if lumadiff is small take the fields block, if not */
          /* take the other fields block */

          if (lumadiff >= 6 && min > (8 * 24))
#ifdef ORIGINAL_CODE
			  for (i = 0; i < 8; i++)
              {
				  *(line + x + i) =
					  (*(yuv[0] + (x + i) + ((y) * width)) >>1) +
					  (*(yuv[0] + (x + i) + ((y + 2) * width))>>1) + 1;
              }
#else
		  {
			  movq_m2r( base[0], mm0 );
			  pavgb_m2r( base[(width<<1)], mm0 );
			  movq_r2m( mm0, line[x] );
		  }
#endif
          else
#ifdef ORIGINAL_CODE
			  for (i = 0; i < 8; i++)
              {
				  *(line + x + i) =
					  (*(yuv[0] + (x + i + xpos) + ((y + 1) * width)) >>1) +
					  (*(yuv[0] + (x + i) + ((y + 0) * width)) >> 1) + 1;
              }

#else
		  {
			  tmp_addr = &base[width+xpos];
			  movq_m2r( base[0], mm0 );
			  pavgb_m2r( *tmp_addr, mm0 );
			  movq_r2m( mm0, line[x] );
		  }
		  
#endif		  

        }

      /* copy the line-buffer into the source-line */
			  base = yuv[0] + (y + 1) * width;
			  for (i = 0; i < width; i+=8)
			  {
#ifdef ORIGINAL_CODE
				  *(yuv[0] + i + (y + 1) * width) = *(line + i);
#else
				  movq_m2r( line[i], mm0 );
				  movq_r2m( mm0, base[i] );
#endif
			  }
		  emms();

    }
}

void
deinterlace_frame_mmx (uint8_t * yuv[3])
{
	static uint64_t four1w = 0x0001000100010001LL;
  /* Buffer one line */
	uint8_t *line = line_buf;
  unsigned int dd;
  unsigned int min;
  int x;
  int y;
  int xx;
  int i;
  int xpos;
  int l1d,l2d;
#ifdef ORIGINAL_CODE
  int l1,l2,d;
#endif
  int lumadiff = 0;
	uint8_t *base;
	uint8_t *base_down2;
	uint8_t *row;
	uint8_t *tmp_addr;

  /* Go through the frame by every two lines */
  for (y = 0; y < height; y += 2)
    {
      /* Go through each line by a "block" length of 32 pixels */
      for (x = 0; x < width; x += 8)
        {
          /* search best matching block of pixels in other field line */
          min = 65535;
          xpos = 0;
          /* search in the range of +/- 16 pixels offset in the line */

		  base = (yuv[0] + x + y * width);
		  row = base + width;
		  base_down2 = row + width;

          for (xx = -16; xx < 16; xx++)
            {
#ifdef ORIGINAL_CODE
              d = 0;
              /* Calculate SAD */
              for (i = -8; i < 16; i++)
                {
                  /* to avoid blocking in ramps we analyse the best match on */
                  /* two lines ... */
                  d += (int) abs (*(yuv[0] + (x + i) + y * width) -
                                  *(yuv[0] + (x + xx + i) + (y + 1) * width));
                  d += (int) abs (*(yuv[0] + (x + i) + (y + 2) * width) -
                                  *(yuv[0] + (x + xx + i) + (y + 1) * width));
                }


#endif
			  pxor_r2r(mm7,mm7); /* Zero */
			  pxor_r2r(mm6,mm6); /* Accumulators for SADs */
			  for( i = -8; i < 16; i += 8 )
			  {
				  /* mm0=mm1=base[i], mm2=mm3=base_down2[i], 
					 mm4=mm5=row[xx+i]
				  */
				  movq_m2r( base[i], mm0 );
				  movq_r2r( mm0, mm1 );
				  movq_m2r( base_down2[i], mm2 );
				  movq_r2r( mm2, mm3 );
				  tmp_addr = &row[xx+i];
				  movq_m2r( *tmp_addr, mm4 );
				  movq_r2r( mm4, mm5 );
				  
				  /* mm0=mm1=SAD(base[i],row[xx+i] */
				  psubusb_r2r( mm4, mm0 );
				  psubusb_r2r( mm1, mm4 );
				  paddb_r2r( mm4, mm0 );
				  movq_r2r( mm0, mm1 );	
				  
				  /* mm6 = mm6 + [mm0|0..3W] + [mm1|4..7W] */

				  punpcklbw_r2r( mm7, mm0 );
				  punpckhbw_r2r( mm7, mm1 );
				  paddw_r2r( mm0, mm6 );
				  paddw_r2r( mm1, mm6 );
				  
				  /* mm2=mm3=SAD(base_down2[i],row[xx+i]) */
				  psubusb_r2r( mm5, mm2 );
				  psubusb_r2r( mm3, mm5 );
				  paddb_r2r( mm5, mm2 );
				  movq_r2r( mm2, mm3 );

				  /* mm6 = mm6 + [mm2|0..3W] + [mm3|4..7W] */

				  punpcklbw_r2r( mm7, mm2 );
				  punpckhbw_r2r( mm7, mm3 );
				  paddw_r2r( mm2, mm6 );
				  paddw_r2r( mm3, mm6 );
			  }

			  /* d = sum of words in mm6 */
			  movq_r2r( mm6, mm7 );
			  psrlq_i2r( 32, mm6 );
			  paddw_r2r( mm7, mm6 );

			  movq_r2r( mm6, mm7 );
			  psrlq_i2r( 16, mm6 );
			  paddw_r2r( mm7, mm6 );
			  				  
			  movd_r2m( mm6, dd );
			  dd &= 0xffff;

#ifdef ORIGINAL_CODE
			  if( d != dd )
			  {
				  fprintf( stderr, "MMX res is wrong.. %08x %08x\n",
						   dd, d);
				  exit(1);
			  }
#endif
              /* if SAD reaches a minimum store the position */
              if (min > dd)
			  {
				  tmp_addr = &base[width+xx];
                  min = dd;
                  xpos = xx;
				  movq_m2r( base[0], mm0 );
				  movq_m2r( *tmp_addr, mm4 );
				
				  movq_r2r( mm0, mm1 );
				  movq_r2r( mm4, mm5 );

				  pxor_r2r( mm7, mm7 );
				  punpcklbw_r2r( mm7, mm0 );
				  punpckhbw_r2r( mm7, mm1 );
				  punpcklbw_r2r( mm7, mm4 );
				  punpckhbw_r2r( mm7, mm5 );
				  
				  paddw_r2r( mm1, mm0 );
				  paddw_r2r( mm5, mm4 );

				  movq_r2r( mm0, mm1 );
				  psrlq_i2r( 32, mm0 );

				  movq_r2r( mm4, mm5 );
				  psrlq_i2r( 32, mm4 );
				  
				  paddw_r2r( mm1, mm0 );
				  paddw_r2r( mm5, mm4 );

				  movq_r2r( mm0, mm1 );
				  psrlq_i2r( 16, mm0 );

				  movq_r2r( mm4, mm5 );
				  psrlq_i2r( 16, mm4 );

				  paddw_r2r( mm1, mm0 );
				  paddw_r2r( mm5, mm4 );
				  
				  movd_r2m( mm0, l1d );
				  movd_r2m( mm4, l2d );
				  l1d = (l1d & 0xffff)/8;
				  l2d = (l2d & 0xffff)/8;
#ifdef ORIGINAL_CODE
                  l1 = l2 = 0;
                  for (i = 0; i < 8; i++)
                    {
                      l1 += *(yuv[0] + (x + i) + y * width);
                      l2 += *(yuv[0] + (x + i + xpos) + (y + 1) * width);
                    }
                  l1 /= 8;
                  l2 /= 8;
				  if( l1 != l1d || l2 != l2d )
				  {
					  fprintf( stderr, "MMX LUM DIFF %d %d %d %d\n", l1, l1d, l2, l2d);
				  }
#endif
                  lumadiff = abs(l1d - l2d);
                  //lumadiff = (lumadiff < 6) ? 0 : 1;
                }

            }

          /* copy pixel-block into the line-buffer */

          /* if lumadiff is small take the fields block, if not */
          /* take the other fields block */

          if (lumadiff >= 6 && min > (8 * 24))
#ifdef ORIGINAL_CODE
			  for (i = 0; i < 8; i++)
              {
				  *(line + x + i) =
					  (*(yuv[0] + (x + i) + ((y) * width)) >>1) +
					  (*(yuv[0] + (x + i) + ((y + 2) * width))>>1) + 1;
              }
#else
		  {
			  movq_m2r( base[0], mm0 );
			  pxor_r2r( mm7,mm7 );
			  movq_r2r( mm0, mm1 );
			  punpcklbw_r2r( mm7, mm0 );
			  punpckhbw_r2r( mm7, mm1 );
			  movq_m2r( base[(width<<1)], mm2 );
			  movq_r2r( mm2, mm3 );
			  movq_m2r( four1w, mm4 );
			  pxor_r2r(mm4,mm4);
			  punpcklbw_r2r( mm7, mm2 );
			  punpckhbw_r2r( mm7, mm3 );

			  paddw_r2r( mm2, mm0 );
			  paddw_r2r( mm3, mm1 );
			  paddw_r2r( mm4, mm0 );
			  paddw_r2r( mm4, mm1 ); 
			  psrlw_i2r( 1, mm0 );
			  psrlw_i2r( 1, mm1 );
			  
			  packuswb_r2r( mm1, mm0 ); 

			  movq_r2m( mm0, line[x] );
		  }
#endif
          else
#ifdef ORIGINAL_CODE
			  for (i = 0; i < 8; i++)
              {
				  *(line + x + i) =
					  (*(yuv[0] + (x + i + xpos) + ((y + 1) * width)) >>1) +
					  (*(yuv[0] + (x + i) + ((y + 0) * width)) >> 1) + 1;
              }

#else
		  {
			  tmp_addr = &base[width+xpos];

			  movq_m2r( base[0], mm0 );
			  pxor_r2r( mm7,mm7 );
			  movq_r2r( mm0, mm1 );
			  punpcklbw_r2r( mm7, mm0 );
			  punpckhbw_r2r( mm7, mm1 );
			  movq_m2r( *tmp_addr, mm2 );
			  movq_r2r( mm2, mm3 );
			  movq_m2r( four1w, mm4 );
			  punpcklbw_r2r( mm7, mm2 );
			  punpckhbw_r2r( mm7, mm3 );

			  paddw_r2r( mm2, mm0 );
			  paddw_r2r( mm3, mm1 );
			  paddw_r2r( mm4, mm0 );
			  paddw_r2r( mm4, mm1 );
			  psrlw_i2r( 1, mm0 );      
			  psrlw_i2r( 1, mm1 );      
			  
			  packuswb_r2r( mm1, mm0 ); 
			  movq_r2m( mm0, line[x] );

		  }
		  
#endif		  

        }

      /* copy the line-buffer into the source-line */
			  base = yuv[0] + (y + 1) * width;
			  for (i = 0; i < width; i+=8)
			  {
#ifdef ORIGINAL_CODE
				  *(yuv[0] + i + (y + 1) * width) = *(line + i);
#else
				  movq_m2r( line[i], mm0 );
				  movq_r2m( mm0, base[i] );
#endif
			  }
		  emms();

    }
}

#endif

/*****************************************************************************
 * takes a frame and blends it into the average                              *
 * buffer.                                                                   *
 * The blend may not be to slow, as noise wouldn't                           *
 * be filtered good than, but it may not be to fast                          *
 * either... I decided to take approx 5 frames into                          *
 * account. Probably need's some finetuning ...                              *
 *****************************************************************************/

void
average_frames (uint8_t * ref[3], uint8_t * avg[3])
{
  int __attribute__ ((aligned (32))) x, y;
  uint8_t __attribute__ ((aligned (32))) * av = avg[0];
  uint8_t __attribute__ ((aligned (32))) * rf = ref[0];
  uint8_t __attribute__ ((aligned (32))) * av2 = avg[2];
  uint8_t __attribute__ ((aligned (32))) * rf2 = ref[2];

  /* blend Y component */

  for (y = 0; y < height; y++)
  {
    for (x = 0; x < width; x++)
      {
        *(av + x ) = (*(av + x ) * 7 + *(rf + x ) * 1) >>3;
      }
    av+=width;
    rf+=width;
  }

  /* blend U and V components */

  av = avg[1];
  rf = ref[1];
  for (y = 0; y < uv_height; y++)
  {
    for (x = 0; x < uv_width; x++)
      {
        *(av + x ) = (*(av + x) * 6 + *(rf + x) * 2) >>3;
        *(av2 + x ) = (*(av2 + x) * 6 + *(rf2 + x) * 2) >>3;
      }
    av+=uv_width;
    rf+=uv_width;
    av2+=uv_width;
    rf2+=uv_width;
  }
}



/*****************************************************************************
 * generate a lowpassfiltered and subsampled copy                            *
 * of the source image (src) at the destination                              *
 * image location.                                                           *
 * Lowpass-filtering is important, as this subsampled                        *
 * image is used for motion estimation and the result                        *
 * is more reliable when filtered.                                           *
 *****************************************************************************/

void
subsample_frame (uint8_t * dst[3], uint8_t * src[3])
{
  int __attribute__ ((aligned (32))) x, y;
  uint8_t __attribute__ ((aligned (32))) *s  = src[0];
  uint8_t __attribute__ ((aligned (32))) *s2 = src[0]+width;
  uint8_t __attribute__ ((aligned (32))) *d  = dst[0];

  /* subsample Y component */

  for (y = 0; y < (height>>1); y += 1)
  {
      for (x = 0; x < width; x += 2)
      {
	  *(d + (x>>1)) =
	      (
		  *(s  + x    ) +
		  *(s  + x + 1) + 
		  *(s2 + x    ) +
		  *(s2 + x + 1) )>>2;
      }
      s+=width<<1;
      s2+=width<<1;
      d+=width;
  }

  /* subsample U and V components */

  s  = src[1];
  s2 = src[1]+uv_width;
  d  = dst[1];

  for (y = 0; y < (uv_height>>1); y += 1)
  {
      for (x = 0; x < uv_width; x += 2)
      {
	  *(d + (x>>1)) = 
	      (
		  *(s  + x    ) +
		  *(s  + x + 1) + 
		  *(s2 + x    ) +
		  *(s2 + x + 1) )>>2;
      }
      s+=width;
      s2+=width;
      d+=uv_width;
  }

  s  = src[2];
  s2 = src[2]+uv_width;
  d  = dst[2];

  for (y = 0; y < (uv_height>>1); y += 1)
  {
      for (x = 0; x < uv_width; x += 2)
      {
	  *(d + (x>>1)) = 
	      (
		  *(s  + x    ) +
		  *(s  + x + 1) + 
		  *(s2 + x    ) +
		  *(s2 + x + 1) )>>2;
      }
      s+=width;
      s2+=width;
      d+=uv_width;
  }

}



/*****************************************************************************
 * halfpel-SAD-function for Y without MMX/SSE                                *
 *****************************************************************************/

uint32_t
calc_SAD_half_noaccel (uint8_t * frm, uint8_t * ref, uint32_t frm_offs, uint32_t ref_offs, int xx, int yy)
{
  int dx = 0;
  int dy = 0;
  int Y = 0;
  uint32_t d = 0;
  uint8_t *fs = frm + frm_offs;
  uint8_t *fs2 = frm + frm_offs+xx+yy*width;
  uint8_t *rs = ref + ref_offs;
  static uint16_t a[4] __attribute__ ((aligned (32))) = { 0, 0, 0, 0 };

  for (dy = 0; dy < BLOCKSIZE; dy++)
    {
      for (dx = 0; dx < BLOCKSIZE; dx++)
        {
          Y = 
	      ( 
		  (int) *(fs  + dx) + 
		  (int) *(fs2 + dx) 
	      )/2 - (int) *(rs + dx);

          d += (Y > 0) ? Y : -Y;
        }
      fs += width;
      fs2 += width;
      rs += width;
    }

  return a[0];
}


/*****************************************************************************
 * halfpel-SAD-function for Y with MMX                                       *
 *****************************************************************************/
#ifdef HAVE_X86CPU

uint32_t
calc_SAD_half_mmxe (uint8_t * frm, uint8_t * ref, uint32_t frm_offs, uint32_t ref_offs, int xx, int yy)
{
  int dx = 0;
  int dy = 0;
  int Y = 0;
  uint32_t d = 0;
  uint8_t *fs = frm + frm_offs;
  uint8_t *fs2 = frm + frm_offs+xx+yy*width;
  uint8_t *rs = ref + ref_offs;
  static uint16_t a[4] __attribute__ ((aligned (32))) = { 0, 0, 0, 0 };

  asm volatile
      (
	  " pxor         %%mm0 , %%mm0;          /* clear mm0                                          */"
	  " movl         %1    , %%eax;          /* load frameadress into eax                          */"
	  " movl         %2    , %%ebx;          /* load frameadress into ebx                          */"
	  " movl         %3    , %%ecx;          /* load frameadress into ecx                          */"
	  " movl         %4    , %%edx;          /* load width       into edx                          */"
	  "                           ;          /*                                                    */"
	  " .rept 8                   ;          /*                                                    */"
	  " movq        (%%eax), %%mm1;          /* 4 Pixels from filtered frame to mm1                */"
	  " movq        (%%ebx), %%mm2;          /* 4 Pixels from filtered frame to mm1 (displaced)    */"
	  " pavgb        %%mm2 , %%mm1;          /* average source pixels                              */"
	  " psadbw      (%%ecx), %%mm1;          /* 4 Pixels difference to mm1                         */"
	  " paddusw      %%mm1 , %%mm0;          /* add result to mm0                                  */"
	  " addl         %%edx , %%eax;          /* add framewidth to frameaddress                     */"
	  " addl         %%edx , %%ebx;          /* add framewidth to frameaddress                     */"
	  " addl         %%edx , %%ecx;          /* add framewidth to frameaddress                     */"
	  " .endr                     ;          /*                                                    */"
	  "                                      /*                                                    */"
	  " movq         %%mm0 , %0   ;          /* make mm0 available to gcc ...                      */"
	  :"=m" (a)     
	  :"m" (fs),"m" (fs2), "m" (rs), "m" (width)
	  :"%eax", "%ebx", "%ecx", "%edx"
	  );

  return a[0];
}

/*****************************************************************************
 * halfpel-SAD-function for Y with MMXext                                    *
 *****************************************************************************/

uint32_t
calc_SAD_half_mmx (uint8_t * frm, uint8_t * ref, uint32_t frm_offs, uint32_t ref_offs, int xx, int yy)
{
  int dx = 0;
  int dy = 0;
  int Y = 0;
  uint32_t d = 0;
  uint8_t *fs = frm + frm_offs;
  uint8_t *fs2 = frm + frm_offs+xx+yy*width;
  uint8_t *rs = ref + ref_offs;
  static uint8_t bit_mask[4] __attribute__ ((aligned (32))) = { 127,127,127,127 };
  static uint16_t a[4] __attribute__ ((aligned (32))) = { 0, 0, 0, 0 };

  asm volatile
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
	  :"m" (fs),"m" (fs2), "m" (rs), "m" (width), "m" (bit_mask)
	  :"%eax", "%ebx", "%ecx", "%edx"
	  );

  return a[0];
}
#endif

/*****************************************************************************
 * SAD-function for Y without MMX/SSE                                        *
 *****************************************************************************/

uint32_t
calc_SAD_noaccel (uint8_t * frm, uint8_t * ref, uint32_t frm_offs, uint32_t ref_offs, int div)
{
  int dx = 0;
  int dy = 0;
  int Y = 0;
  uint32_t d = 0;
  uint8_t *fs = frm + frm_offs;
  uint8_t *rs = ref + ref_offs;

  for (dy = 0; dy < BLOCKSIZE / div; dy++)
    {
      for (dx = 0; dx < BLOCKSIZE / div; dx++)
        {
          Y = (int) *(fs + dx) - (int) *(rs + dx);

          d += (Y > 0) ? Y : -Y;
        }
      fs += width;
      rs += width;
    }
  return d;
}


#ifdef HAVE_X86CPU
/*****************************************************************************
 * SAD-function for Y with MMX                                               *
 *****************************************************************************/

uint32_t
calc_SAD_mmx (uint8_t * frm, uint8_t * ref, uint32_t frm_offs, uint32_t ref_offs, int div)
{
  uint32_t d = 0;
  uint8_t *fs = frm + frm_offs;
  uint8_t *rs = ref + ref_offs;
  static uint16_t a[4] = { 0, 0, 0, 0 };
  int i;

      switch (div)
        {
        case 1:                /* 8x8 */

          asm volatile
            (" pxor        %%mm0 , %%mm0;            /* clear mm0 */"
             " pxor        %%mm7 , %%mm7;            /* clear mm7 */"::);

              for (i = 8; i != 0; i--)
                {
                  /* pure MMX */
                  asm volatile
                    (" movq         %0    , %%mm1;         /* 8 Pixels from filtered frame to mm1 */"
                     " movq         %1    , %%mm2;         /* 8 Pixels from reference frame to mm2 */"
                     " movq         %%mm2 , %%mm3;         /* hold a copy of mm2 in mm3 */"
                     " psubusb      %%mm1 , %%mm3;         /* positive differences between mm2 and mm1 */"
                     " psubusb      %%mm2 , %%mm1;         /* positive differences between mm1 and mm3 */"
                     " paddusb      %%mm3 , %%mm1;         /* mm2 now contains abs(mm1-mm2) */"
                     " movq         %%mm1 , %%mm2;         /* copy mm1 to mm2 */"
                     " punpcklbw    %%mm7 , %%mm1;         /* unpack mm1 into mm1 and mm2 */"
                     " punpckhbw    %%mm7 , %%mm2;         /*   */"
                     " paddusw      %%mm1 , %%mm2;         /* add mm1 (stored in mm1 and mm2...) */"
                     " paddusw      %%mm2 , %%mm0;         /* to mm0 */"::"X" (*(fs)),
                     "X" (*(rs)));

                  fs += width;  /* next row */
                  rs += width;
                }
              asm volatile
                (" movq        %%mm0, %0;                /* make mm0 available to gcc ... */ ":"=X"
                 (*a):);
              d = a[0] + a[1] + a[2] + a[3];
          break;

        case 2:                /* 4x4 */

          asm volatile
            (" pxor        %%mm0 , %%mm0;            /* clear mm0 */"
             " pxor        %%mm7 , %%mm7;            /* clear mm7 */"::);

          for (i = 4; i != 0; i--)
            {
              /* pure MMX */
              asm volatile
                (" movd         %0    , %%mm1;         /* 4 Pixels from filtered frame to mm1 */"
                 " movd         %1    , %%mm2;         /* 4 Pixels from reference frame to mm2 */"
                 "                                     /* Bits 32-63 zero'd */"
                 " movq         %%mm2 , %%mm3;         /* hold a copy of mm2 in mm3 */"
                 " psubusb      %%mm1 , %%mm3;         /* positive differences between mm2 and mm1 */"
                 " psubusb      %%mm2 , %%mm1;         /* positive differences between mm1 and mm3 */"
                 " paddusb      %%mm3 , %%mm1;         /* mm2 now contains abs(mm1-mm2) */"
                 " movq         %%mm1 , %%mm2;         /* copy mm1 to mm2 */"
                 " punpcklbw    %%mm7 , %%mm1;         /* unpack mm1 into mm1 and mm2 */"
                 " punpckhbw    %%mm7 , %%mm2;         /*   */"
                 " paddusw      %%mm1 , %%mm2;         /* add mm1 (stored in mm1 and mm2...) */"
                 " paddusw      %%mm2 , %%mm0;         /* to mm0 */"::"X" (*(fs)),
                 "X" (*(rs)));

              fs += width;      /* next row */
              rs += width;
            }

          asm volatile
            (" movq        %%mm0, %0;                /* make mm0 available to gcc ... */ ":"=X"
             (*a):);

          d = a[0] + a[1] + a[2] + a[3];
          break;
        }
    
  return d;
}

/*****************************************************************************
 * SAD-function for Y with SSE                                               *
 *****************************************************************************/

uint32_t
calc_SAD_mmxe (uint8_t * frm, uint8_t * ref, uint32_t frm_offs, uint32_t ref_offs, int div)
{
  uint8_t *fs __attribute__ ((aligned (32))) = frm + frm_offs;
  uint8_t *rs __attribute__ ((aligned (32))) = ref + ref_offs;
  static uint16_t a[4] __attribute__ ((aligned (32))) = { 0, 0, 0, 0 };
    
      switch (div)
        { 
        case 1:                /* 8x8 */
          
          asm volatile
            (
             " pxor         %%mm0 , %%mm0;          /* clear mm0                                          */"
             " movl         %1    , %%eax;          /* load frameadress into eax                          */"
             " movl         %2    , %%ebx;          /* load frameadress into ebx                          */"
             " movl         %3    , %%ecx;          /* load width       into ecx                          */"
             "                           ;          /*                                                    */"
             " .rept 8                   ;          /*                                                    */"
             " movq        (%%eax), %%mm1;          /* 8 Pixels from filtered frame to mm1                */"
             " psadbw      (%%ebx), %%mm1;          /* 8 Pixels difference to mm1                         */"
             " paddusw      %%mm1 , %%mm0;          /* add result to mm0                                  */"
             " addl         %%ecx , %%eax;          /* add framewidth to frameaddress                     */"
             " addl         %%ecx , %%ebx;          /* add framewidth to frameaddress                     */"
             " .endr                     ;          /*                                                    */"
             "                                      /*                                                    */"
             " movq         %%mm0 , %0   ;          /* make mm0 available to gcc ...                      */"
             :"=m" (a)     
             :"m" (fs), "m" (rs), "m" (width)
             :"%eax", "%ebx", "%ecx"
             );

              return a[0];
              
          break;

        case 2:                /* 4x4 */

          asm volatile
            (
             " pxor         %%mm0 , %%mm0;          /* clear mm0                                          */"
             " movl         %1    , %%eax;          /* load frameadress into eax                          */"
             " movl         %2    , %%ebx;          /* load frameadress into ebx                          */"
             " movl         %3    , %%ecx;          /* load framewidth  into ecx                          */"
             "                           ;          /*                                                    */"
             " .rept 4                   ;          /*                                                    */"
             " movd        (%%eax), %%mm1;          /* 4 Pixels from filtered frame to mm1                */"
             " movd        (%%ebx), %%mm2;          /* 4 Pixels from filtered frame to mm1                */"
             " psadbw       %%mm2 , %%mm1;          /* 4 Pixels difference to mm1                         */"
             " paddusw      %%mm1 , %%mm0;          /* add result to mm0                                  */"
             " addl         %%ecx , %%eax;          /* add framewidth to frameaddress                     */"
             " addl         %%ecx , %%ebx;          /* add framewidth to frameaddress                     */"
             " .endr                     ;          /*                                                    */"
             "                                      /*                                                    */"
             " movq         %%mm0 , %0   ;          /* make mm0 available to gcc ...                      */"
             :"=m" (a)     
             :"m" (fs), "m" (rs), "m" (width)
             :"%eax", "%ebx", "%ecx"
             );

              return a[0];
              
          break;
        }
}

#endif

/*****************************************************************************
 * SAD-function for U+V without MMX/SSE                                      *
 *****************************************************************************/

uint32_t
calc_SAD_uv_noaccel (uint8_t * frm, 
                     uint8_t * ref, 
                     uint32_t frm_offs, 
                     uint32_t ref_offs, 
                     int div)
{
  int dx, dy, Y;
  uint32_t d = 0;
  uint8_t *fs = frm + frm_offs;
  uint8_t *rs = ref + ref_offs;

  for (dy = 0; dy < BLOCKSIZE / 2 / div; dy++)
    {
      for (dx = 0; dx < BLOCKSIZE / 2 / div; dx++)
        {
          Y = (int) *(fs + dx) - (int) *(rs + dx);

          d += (Y > 0) ? Y : -Y;
        }
      fs += uv_width;
      rs += uv_width;
    }
  return d;
}

#ifdef HAVE_X86CPU

/*****************************************************************************
 * SAD-function for U+V with MMX                                             *
 *****************************************************************************/

uint32_t
calc_SAD_uv_mmx (uint8_t * frm,
                 uint8_t * ref, uint32_t frm_offs, uint32_t ref_offs, int div)
{
  uint32_t d = 0;
  uint8_t *fs = frm + frm_offs;
  uint8_t *rs = ref + ref_offs;
  static uint16_t a[4] = { 0, 0, 0, 0 };
  int i;

  switch (div)
    {
    case 1:                    /* 4x4 --> subsampled chroma planes ! */

      asm volatile
        (" pxor        %%mm0 , %%mm0;            /* clear mm0 */"
         " pxor        %%mm7 , %%mm7;            /* clear mm7 */"::);

      for (i = 4; i != 0; i--)
        {
          /* MMX */
          asm volatile
            (" movd         %0    , %%mm1;         /* 4 Chroma Pixels from filtered frame to mm1 */"
             " movd         %1    , %%mm2;         /* 4 Chroma Pixels from reference frame to mm2 */"
             "                                     /* Bits 32-63 zero'd */"
             " movq         %%mm2 , %%mm3;         /* hold a copy of mm2 in mm3 */"
             " psubusb      %%mm1 , %%mm3;         /* positive differences between mm2 and mm1 */"
             " psubusb      %%mm2 , %%mm1;         /* positive differences between mm1 and mm3 */"
             " paddusb      %%mm3 , %%mm1;         /* mm2 now contains abs(mm1-mm2) */"
             " movq         %%mm1 , %%mm2;         /* copy mm1 to mm2 */"
             " punpcklbw    %%mm7 , %%mm1;         /* unpack mm1 into mm1 and mm2 */"
             " punpckhbw    %%mm7 , %%mm2;         /*   */"
             " paddusw      %%mm1 , %%mm2;         /* add mm1 (stored in mm1 and mm2...) */"
             " paddusw      %%mm2 , %%mm0;         /* to mm0 */"::"X" (*(fs)),
             "X" (*(rs)));

          fs += uv_width;       /* next row */
          rs += uv_width;
        }

      asm volatile
        (" movq        %%mm0, %0;                /* make mm0 available to gcc ... */ ":"=X"
         (*a):);

      d = a[0] + a[1] + a[2] + a[3];

      break;

    case 2:                    /* 2x2 */

      asm volatile
        (" pxor        %%mm0 , %%mm0;            /* clear mm0 */"
         " pxor        %%mm7 , %%mm7;            /* clear mm7 */"::);

      /* MMX */
      /* loop is not needed... (is MMX really faster here?!?) */

      asm volatile
        (" movd         %0,    %%mm1;         /* 4 Pixels from filtered frame to mm1 */"
         " movd         %1,    %%mm2;         /* 4 Pixels from reference frame to mm2 */"
         " psrlq        $16,   %%mm1;         /* kick 2 Pixels  ... */"
         " psrlq        $16,   %%mm2;         /* kick 2 Pixels  ... */"
         " movq         %%mm2, %%mm3;         /* hold a copy of mm2 in mm3 */"
         " psubusb      %%mm1, %%mm3;         /* positive differences between mm2 and mm1 */"
         " psubusb      %%mm2, %%mm1;         /* positive differences between mm1 and mm3 */"
         " paddusb      %%mm3, %%mm1;         /* mm2 now contains abs(mm1-mm2) */"
         " movq         %%mm1, %%mm2;         /* copy mm1 to mm2 */"
         " punpcklbw    %%mm7, %%mm1;         /* unpack mm1 into mm1 and mm2 */"
         " punpckhbw    %%mm7, %%mm2;         /*   */"
         " paddusw      %%mm1, %%mm2;         /* add mm1 (stored in mm1 and mm2...) */"
         " paddusw      %%mm2, %%mm0;         /* to mm0 */"::"X" (*(fs)), "X" (*(rs)));

      fs += uv_width;           /* next row */
      rs += uv_width;

      asm volatile
        (" movd         %0,    %%mm1;         /* 4 Pixels from filtered frame to mm1 */"
         " movd         %1,    %%mm2;         /* 4 Pixels from reference frame to mm2 */"
         " psrlq        $16,   %%mm1;         /* kick 2 Pixels  ... */"
         " psrlq        $16,   %%mm2;         /* kick 2 Pixels  ... */"
         " movq         %%mm2, %%mm3;         /* hold a copy of mm2 in mm3 */"
         " psubusb      %%mm1, %%mm3;         /* positive differences between mm2 and mm1 */"
         " psubusb      %%mm2, %%mm1;         /* positive differences between mm1 and mm3 */"
         " paddusb      %%mm3, %%mm1;         /* mm2 now contains abs(mm1-mm2) */"
         " movq         %%mm1, %%mm2;         /* copy mm1 to mm2 */"
         " punpcklbw    %%mm7, %%mm1;         /* unpack mm1 into mm1 and mm2 */"
         " punpckhbw    %%mm7, %%mm2;         /*   */"
         " paddusw      %%mm1, %%mm2;         /* add mm1 (stored in mm1 and mm2...) */"
         " paddusw      %%mm2, %%mm0;         /* to mm0 */"::"X" (*(fs)), "X" (*(rs)));

      asm volatile
        (" movq        %%mm0, %0;                /* make mm0 available to gcc ... */ ":"=X"
         (*a):);

      d = a[0] + a[1] + a[2] + a[3];

      break;
    }

  return d;
}

/*****************************************************************************
 * SAD-function for U+V with SSE                                             *
 *****************************************************************************/

uint32_t
calc_SAD_uv_mmxe (uint8_t * frm,
                 uint8_t * ref, uint32_t frm_offs, uint32_t ref_offs, int div)
{
  uint32_t d __attribute__ ((aligned (32))) = 0;
  uint8_t *fs __attribute__ ((aligned (32))) = frm + frm_offs;
  uint8_t *rs __attribute__ ((aligned (32))) = ref + ref_offs;
  static uint16_t a[4] __attribute__ ((aligned (32))) = { 0, 0, 0, 0 };
    
  switch (div)
    {
    case 1:                    /* 4x4 --> subsampled chroma planes ! */
          asm volatile
            (
             " xorl        %%ebx  , %%ebx;          /* clear ebx                                          */"
             " movl         %3    , %%eax;          /* load framewidth to eax                             */"
             " movl         %1    , %%ecx;          /* load frameadress into ecx                          */"
             " movl         %2    , %%edx;          /* load frameadress into edx                          */"
             "                           ;          /*                                                    */"
             " movd  (%%ebx,%%ecx), %%mm0;          /* 4 Pixels from filtered frame to mm0                */"
             " movd  (%%ebx,%%edx), %%mm1;          /* 4 Pixels from reference frame to mm1               */"
             " addl         %%eax , %%ebx;          /* add framewidth to frameaddress (filtered)          */"
             " movd  (%%ebx,%%ecx), %%mm2;          /* 4 Pixels from filtered frame to mm2                */"
             " movd  (%%ebx,%%edx), %%mm3;          /* 4 Pixels from reference frame to mm3               */"
             " addl         %%eax , %%ebx;          /* add framewidth to frameaddress (filtered)          */"
             " movd  (%%ebx,%%ecx), %%mm4;          /* 4 Pixels from filtered frame to mm4                */"
             " movd  (%%ebx,%%edx), %%mm5;          /* 4 Pixels from reference frame to mm5               */"
             " addl         %%eax , %%ebx;          /* add framewidth to frameaddress (filtered)          */"
             " movd  (%%ebx,%%ecx), %%mm6;          /* 4 Pixels from filtered frame to mm6                */"
             " movd  (%%ebx,%%edx), %%mm7;          /* 4 Pixels from reference frame to mm7               */"
             "                           ;          /*                                                    */"
             " psadbw       %%mm0 , %%mm1;          /* Calculate SAD of that line of pixels, store in mm2 */"
             " psadbw       %%mm2 , %%mm3;          /* Calculate SAD of that line of pixels, store in mm3 */"
             " psadbw       %%mm4 , %%mm5;          /* Calculate SAD of that line of pixels, store in mm5 */"
             " psadbw       %%mm6 , %%mm7;          /* Calculate SAD of that line of pixels, store in mm7 */"
             "                           ;          /*                                                    */"
             " paddusw      %%mm3 , %%mm1;          /* add result to mm1                                  */"
             " paddusw      %%mm5 , %%mm1;          /* add result to mm1                                  */"
             " paddusw      %%mm7 , %%mm1;          /* add result to mm1                                  */"
             "                           ;          /*                                                    */"
             " movq         %%mm1 , %0   ;          /* make mm1 available to gcc ...                      */"
             :"=m" (a)     
             :"m" (fs), "m" (rs), "m" (uv_width)
             :"%eax", "%ebx", "%ecx", "%edx"
             );

              d = a[0];
      break;

    case 2:                    /* 2x2 */
          asm volatile
            (
             " xorl        %%ebx  , %%ebx;          /* clear ebx                                          */"
             " movl         %1    , %%ecx;          /* load frameadress into ecx                          */"
             " movl         %2    , %%edx;          /* load frameadress into edx                          */"
             "                           ;          /*                                                    */"
             " movd  (%%ebx,%%ecx), %%mm0;          /* 4 Pixels from filtered frame to mm0                */"
             " movd  (%%ebx,%%edx), %%mm1;          /* 4 Pixels from reference frame to mm1               */"
             " addl         %3    , %%ebx;          /* add framewidth to frameaddress (filtered)          */"
             " movd  (%%ebx,%%ecx), %%mm2;          /* 4 Pixels from filtered frame to mm2                */"
             " movd  (%%ebx,%%edx), %%mm3;          /* 4 Pixels from reference frame to mm3               */"
             "                           ;          /*                                                    */"
             " psrlq        $16,    %%mm0;          /* kick 2 Pixels  ...                                 */"
             " psrlq        $16,    %%mm1;          /* kick 2 Pixels  ...                                 */"
             " psrlq        $16,    %%mm2;          /* kick 2 Pixels  ...                                 */"
             " psrlq        $16,    %%mm3;          /* kick 2 Pixels  ...                                 */"
             "                           ;          /*                                                    */"
             " psadbw       %%mm0 , %%mm1;          /* Calculate SAD of that line of pixels, store in mm2 */"
             " psadbw       %%mm2 , %%mm3;          /* Calculate SAD of that line of pixels, store in mm3 */"
             "                           ;          /*                                                    */"
             " paddusw      %%mm3 , %%mm1;          /* add result to mm1                                  */"
             "                           ;          /*                                                    */"
             " movq         %%mm1 , %0   ;          /* make mm1 available to gcc ...                      */"
             :"=m" (a)     
             :"m" (fs), "m" (rs), "m" (uv_width)
             :"%ebx", "%ecx", "%edx"
             );

              d = a[0];

      break;
    }

  return d;
}

#endif

/* Motion estimation on 4*4 blocks... */


void
mb_search_44 (int x, int y, uint8_t * ref_frame[3], uint8_t * tgt_frame[3])
{
  int __attribute__ ((aligned (32))) qy, qx;
  uint32_t __attribute__ ((aligned (32))) d,d_uv=0;
  uint32_t __attribute__ ((aligned (32))) CAD = (256 * BLOCKSIZE * BLOCKSIZE);
  uint32_t __attribute__ ((aligned (32))) SAD = (256 * BLOCKSIZE * BLOCKSIZE);

  /* search_radius has to be devided by 2 as radius is given in */
  /* half-pixels ...                                            */

  for (qy = -(mod_radius >>1); qy <= (mod_radius >>1); qy += 4)
    for (qx = -(mod_radius >>1); qx <= (mod_radius >>1); qx += 4)
      {
        d = calc_SAD (tgt_frame[0],
                      ref_frame[0],
                      ((x + qx - BLOCKOFFSET) >>2) + ((y + qy - BLOCKOFFSET) >>2) * width,
                      ((x - BLOCKOFFSET) >>2) + ((y - BLOCKOFFSET) >>2) * width, 1);
        if((qx&7)==0 || d_uv==0)
        {
        d_uv = calc_SAD_uv (tgt_frame[1],
                          ref_frame[1],
                          ((x + qx - BLOCKOFFSET) >>3) + ((y + qy - BLOCKOFFSET) >>3) * uv_width,
                          ((x - BLOCKOFFSET) >>3) + ((y - BLOCKOFFSET) >>3) * uv_width, 1);

        d_uv += calc_SAD_uv (tgt_frame[2],
                          ref_frame[2],
                          ((x + qx - BLOCKOFFSET) >>3) + ((y + qy - BLOCKOFFSET) >>3) * uv_width,
                          ((x - BLOCKOFFSET) >>3) + ((y - BLOCKOFFSET) >>3) * uv_width, 1);

        }
        d += d_uv;

        if (d < SAD)
          {
            matrix[x][y][0] = qx * 2;
            matrix[x][y][1] = qy * 2;
            SAD = d;
          }

        if (qx == 0 && qy == 0)
          {
            CAD = d;
          }

      }

  if (SAD > CAD)
    {
      matrix[x][y][0] = 0;
      matrix[x][y][1] = 0;
    }
}

void
mb_search_22 (int x, int y, uint8_t * ref_frame[3], uint8_t * tgt_frame[3])
{
  int __attribute__ ((aligned (32))) qy, qx;
  uint32_t __attribute__ ((aligned (32))) d,d_uv;
  uint32_t __attribute__ ((aligned (32))) CAD = (256 * BLOCKSIZE * BLOCKSIZE);
  uint32_t __attribute__ ((aligned (32))) SAD = (256 * BLOCKSIZE * BLOCKSIZE);
  int __attribute__ ((aligned (32))) bx;
  int __attribute__ ((aligned (32))) by;

  bx = matrix[x][y][0] / 2;
  by = matrix[x][y][1] / 2;

  for (qy = (by - 2); qy <= (by + 2); qy += 2)
    for (qx = (bx - 2); qx <= (bx + 2); qx += 2)
      {
        d = calc_SAD (tgt_frame[0],
                      ref_frame[0],
                      ((x + qx - BLOCKOFFSET) >>1) + ((y + qy - BLOCKOFFSET) >>1) * width,
                      ((x - BLOCKOFFSET) >>1) + ((y - BLOCKOFFSET) >>1) * width, 1);

        if((qx&3)==0)
        {
        d_uv = calc_SAD_uv (tgt_frame[1],
                          ref_frame[1],
                          ((x + qx - BLOCKOFFSET) >>2) + ((y + qy - BLOCKOFFSET) >>2) * uv_width,
                          ((x - BLOCKOFFSET) >>2) + ((y - BLOCKOFFSET) >>2) * uv_width, 1);

        d_uv += calc_SAD_uv (tgt_frame[2],
                          ref_frame[2],
                          ((x + qx - BLOCKOFFSET) >>2) + ((y + qy - BLOCKOFFSET) >>2) * uv_width,
                          ((x - BLOCKOFFSET) >>2) + ((y - BLOCKOFFSET) >>2) * uv_width, 1);
        }
        d+=d_uv;

        if (d < SAD)
          {
            matrix[x][y][0] = qx *2 ;
            matrix[x][y][1] = qy *2 ;
            SAD = d;
          }
        if (qx == 0 && qy == 0)
          {
            CAD = d;
          }
      }

  if (SAD > CAD)
    {
      matrix[x][y][0] = bx *2;
      matrix[x][y][1] = by *2;
    }
}

void
mb_search (int x, int y, uint8_t * ref_frame[3], uint8_t * tgt_frame[3])
{
  int qy, qx;
  uint32_t d,d_uv;
  uint32_t CAD = (256 * BLOCKSIZE * BLOCKSIZE);
  uint32_t SAD = (256 * BLOCKSIZE * BLOCKSIZE);
  int bx;
  int by;

  bx = matrix[x][y][0] >>1;
  by = matrix[x][y][1] >>1;

  for (qy = (by - 1); qy <= (by + 1); qy++)
    for (qx = (bx - 1); qx <= (bx + 1); qx++)
      {
        d = calc_SAD (tgt_frame[0],
                      ref_frame[0],
                      (x + qx - BLOCKOFFSET) + (y + qy - BLOCKOFFSET) * width,
                      (x - BLOCKOFFSET) + (y - BLOCKOFFSET) * width, 1);

        
        if (d < SAD)
          {
            matrix[x][y][0] = qx *2;
            matrix[x][y][1] = qy *2;
            SAD = d;
          }

        if (qx == 0 && qy == 0)
          {
            CAD = d;
          }
      }

  if (SAD > CAD)
    {
      matrix[x][y][0] = bx*2;
      matrix[x][y][1] = by*2;
      SAD = CAD;
    }

}

void
mb_search_half (int x, int y, uint8_t * ref_frame[3], uint8_t * tgt_frame[3])
{
  int qy, qx;
  uint32_t d,d_uv;
  uint32_t CAD = (256 * BLOCKSIZE * BLOCKSIZE);
  uint32_t SAD = (256 * BLOCKSIZE * BLOCKSIZE);
  int bx;
  int by;

  bx = matrix[x][y][0] / 2;
  by = matrix[x][y][1] / 2;

  for (qy = -1; qy <= +1; qy++)
    for (qx = -1; qx <= +1; qx++)
      {
        d = calc_SAD_half (tgt_frame[0],
			   ref_frame[0],
			   (x + bx - BLOCKOFFSET) + (y + by - BLOCKOFFSET) * width,
			   (x - BLOCKOFFSET) + (y - BLOCKOFFSET) * width, qx, qy);

        
        if (d < SAD)
          {
            matrix[x][y][0] = qx+bx;
            matrix[x][y][1] = qy+by;
            SAD = d;
          }

        if (qx == 0 && qy == 0)
	{
            CAD = d;
	}
      }

  if (SAD > CAD)
  {
      matrix[x][y][0] = bx*2;
      matrix[x][y][1] = by*2;
      SAD = CAD;
  }
}


/* half pixel search seem's definetly not to be needed ... hmm */
#if 0
void
mb_search_half (int x, int y)
{
  int dy, dx, qy, qx, xx, yy, x0, x1, y0, y1;
  uint32_t d;
  int Y, U, V;
  int bx;
  int by;
  float sx, sy;
  float a0, a1, a2, a3;
  uint32_t old_SQD;
  int i;

  SAD[0] = 10000000;
  SAD[1] = 10000000;
  SAD[2] = 10000000;
  SAD[3] = 10000000;

  for (i = 0; i < 4; i++)
    {
      bx = best_match_x[i];
      by = best_match_y[i];

      for (qy = (by - 1); qy <= (by + 1); qy++)
        for (qx = (bx - 1); qx <= (bx + 1); qx++)
          {
            sx = (qx - (qx & ~1)) * 0.5;
            sy = (qy - (qy & ~1)) * 0.5;

            a0 = (1 - sx) * (1 - sy);
            a1 = (sx) * (1 - sy);
            a2 = (1 - sx) * (sy);
            a3 = (sx) * (sy);

            xx = x + qx / 2;    /* Keeps some calc. out of the MB-search-loop... */
            yy = y + qy / 2;

            d = 0;
            for (dy = -2; dy < 6; dy++)
              for (dx = -2; dx < 6; dx++)
                {
                  x0 = xx + dx;
                  x1 = x0 - 1;
                  y0 = (yy + dy) * width;
                  y1 = (yy + dy - 1) * width;

                  /* we only need Y for half-pels... */
                  Y = (AVRG[(x0) + (y0)] * a0 +
                       AVRG[(x1) + (y0)] * a1 +
                       AVRG[(x0) + (y1)] * a2 +
                       AVRG[(x1) + (y1)] * a3) - YUV1[(x + dx) + (y + dy) * width];

                  d += (Y < 0) ? -Y : Y;        /* doch mal einen SAD... nur testweise ... */
                }

            if (d < SAD[i])
              {
                best_match_x[i] = qx;
                best_match_y[i] = qy;
                SAD[i] = d;
              }
          }
    }
}
#endif

void
copy_filtered_block (int x, int y, uint8_t * dest[3], uint8_t * srce[3])
{
  int dx,dy;
  int qx = matrix[x][y][0]/2;
  int qy = matrix[x][y][1]/2;
//int sx = matrix[x][y][0]-(qx*2);
//int sy = matrix[x][y][1]-(qy*2);
  int sy=0;
  int sx=0;
  int q2=(int)(block_quality*(float)256);
  int q1=256-q2;
  uint8_t * dest_addr = dest[0]+(x)+(y)*width;
  uint8_t * src_addr1 = srce[0]+(x+qx)+(y+qy)*width;
  uint8_t * src_addr2 = yuv[0] +(x)+(y)*width;

  for (dy=0;dy<(BLOCKSIZE>>1);dy++)
  {
    for (dx=0;dx<(BLOCKSIZE>>1);dx++)
    {
      *(dest_addr+dx)=
	  ( *(src_addr1+dx)*q1)+
	  ( *(src_addr2+dx)*q2)>>8;
    }
      dest_addr += width;
      src_addr1 += width;
      src_addr2 += width;
  }

  dest_addr = dest[1]+(x>>1)+(y>>1)*uv_width;
  src_addr1 = srce[1]+((x+qx)>>1)+((y+qy)>>1)*uv_width;
  src_addr2 = yuv[1] +(x>>1)+(y>>1)*uv_width;

  for (dy=0;dy<(BLOCKSIZE>>2);dy++)
  {
    for (dx=0;dx<(BLOCKSIZE>>2);dx++)
    {
      *(dest_addr+dx)=
	  ( *(src_addr1+dx) * q1 )+
	  ( *(src_addr2+dx) * q2 )>>8;
    }
      dest_addr += uv_width;
      src_addr1 += uv_width;
      src_addr2 += uv_width;
  }

  dest_addr = dest[2]+(x>>1)+(y>>1)*uv_width;
  src_addr1 = srce[2]+((x+qx)>>1)+((y+qy)>>1)*uv_width;
  src_addr2 = yuv[2] +(x>>1)+(y>>1)*uv_width;

  for (dy=0;dy<(BLOCKSIZE>>2);dy++)
  {
    for (dx=0;dx<(BLOCKSIZE>>2);dx++)
    {
      *(dest_addr+dx)=
	  ( *(src_addr1+dx) * q1 )+
	  ( *(src_addr2+dx) * q2 )>>8;
    }
      dest_addr += uv_width;
      src_addr1 += uv_width;
      src_addr2 += uv_width;
  }
}  

void
copy_filtered_block_old (int x, int y, uint8_t * dest[3], uint8_t * srce[3])
{
  int dx, dy;
  int qx = matrix[x][y][0];
  int qy = matrix[x][y][1];
  int xx, yy, x2, y2;
  float sx, sy;
  float a0, a1, a2, a3;

  sx = (qx - (qx & ~1)) * 0.5;
  sy = (qy - (qy & ~1)) * 0.5;
  qx /= 2;
  qy /= 2;

  a0 = (1 - sx) * (1 - sy);
  a1 = (sx) * (1 - sy);
  a2 = (1 - sx) * (sy);
  a3 = (sx) * (sy);

  for (dy = 0; dy < (BLOCKSIZE / 2); dy++)
    for (dx = 0; dx < (BLOCKSIZE / 2); dx++)
      {
        xx = x + dx;
        yy = y + dy;
        x2 = xx >> 1;
        y2 = yy / 2 * uv_width;

        if (sx != 0 || sy != 0)

          *(dest[0] + (xx) + (yy) * width) =
            (*(srce[0] + (xx + qx - 0) + (yy + qy - 0) * width) * a0 +
             *(srce[0] + (xx + qx - 1) + (yy + qy - 0) * width) * a1 +
             *(srce[0] + (xx + qx - 0) + (yy + qy - 1) * width) * a2 +
             *(srce[0] + (xx + qx - 1) + (yy + qy - 1) * width) * a3) * (1 -
                                                                         block_quality) +
            *(yuv[0] + (xx) + (yy) * width) * block_quality;

        else
          *(dest[0] + (xx) + (yy) * width) =
            *(srce[0] + (xx + qx) + (yy + qy) * width) * (1 - block_quality) +
            *(yuv[0] + (xx) + (yy) * width) * block_quality;

        /* U */
        *(dest[1] + (xx) / 2 + (yy) / 2 * uv_width) =
          *(srce[1] + (xx + qx) / 2 + (yy + qy) / 2 * uv_width) * (1 - block_quality) +
          *(yuv[1] + (xx) / 2 + (yy) / 2 * uv_width) * block_quality;

        /* V */
        *(dest[2] + (xx) / 2 + (yy) / 2 * uv_width) =
          *(srce[2] + (xx + qx) / 2 + (yy + qy) / 2 * uv_width) * (1 - block_quality) +
          *(yuv[2] + (xx) / 2 + (yy) / 2 * uv_width) * block_quality;
      }
}

void
antiflicker_reference (uint8_t * frame[3])
{
  int x, y;

  for (y = 0; y < (uv_height - 1); y++)
    for (x = 0; x < (uv_width - 1); x++)
      {
        *(frame[1] + x + y * uv_width) =
          (*(frame[1] + (x + 0) + (y + 0) * uv_width) +
           *(frame[1] + (x + 1) + (y + 0) * uv_width) +
           *(frame[1] + (x + 0) + (y + 1) * uv_width) +
           *(frame[1] + (x + 1) + (y + 1) * uv_width)) / 4;

        *(frame[2] + x + y * uv_width) =
          (*(frame[2] + (x + 0) + (y + 0) * uv_width) +
           *(frame[2] + (x + 1) + (y + 0) * uv_width) +
           *(frame[2] + (x + 0) + (y + 1) * uv_width) +
           *(frame[2] + (x + 1) + (y + 1) * uv_width)) / 4;

      }
}


void
despeckle_frame_hard (uint8_t * frame[3])
{
  int x, y;

  for (y = 1; y < (height - 1); y++)
    for (x = 1; x < (width - 1); x++)
      {
        /* generate a mean-filtered version of the frames Y in yuv2[0] */
	  *(yuv2[0] + (x + 0) + (y + 0) * width) =((
          (*(frame[0] + (x - 1) + (y - 1) * width) +
           *(frame[0] + (x + 0) + (y - 1) * width) +
           *(frame[0] + (x + 1) + (y - 1) * width) +
           *(frame[0] + (x - 1) + (y + 0) * width) +
           *(frame[0] + (x + 1) + (y + 0) * width) +
           *(frame[0] + (x - 1) + (y + 1) * width) +
           *(frame[0] + (x + 0) + (y + 1) * width) +
           *(frame[0] + (x + 1) + (y + 1) * width)) >>3) * 2 +
          *(frame[0] + (x + 0) + (y + 0) * width) * 8)/10;
      }
  /* copy yuv2[0] to frame[0] */
  memcpy (frame[0], yuv2[0], width * height);
}

void
despeckle_frame_soft (uint8_t * frame[3])
{
  int x, y;
  long int v1, v2;
  float delta;
  
  memcpy (yuv2[0], frame[0], width * height);

  for (y = 1; y < (height - 1); y++)
    for (x = 1; x < (width - 1); x++)
      {
        /* generate a mean-filtered version of the frames Y in yuv2[0] */
        v1 =
          (*(frame[0] + (x - 1) + (y - 1) * width) +
           *(frame[0] + (x + 0) + (y - 1) * width) +
           *(frame[0] + (x + 1) + (y - 1) * width) +
           *(frame[0] + (x - 1) + (y + 0) * width) +
           *(frame[0] + (x + 1) + (y + 0) * width) +
           *(frame[0] + (x - 1) + (y + 1) * width) +
           *(frame[0] + (x + 0) + (y + 1) * width) +
           *(frame[0] + (x + 1) + (y + 1) * width)) >>3;

        v2 = *(frame[0] + (x + 0) + (y + 0) * width);

        delta=fabs(v1-v2);
        
        if (delta <= 3) /* range -3...+3 ~= 2 bits ... */
          *(yuv2[0] + (x + 0) + (y + 0) * width) = v1;
        else
          *(yuv2[0] + (x + 0) + (y + 0) * width) = v2;

      }
  /* copy yuv2[0] to frame[0] */
  memcpy (frame[0], yuv2[0], width * height);
}


void
line (uint8_t * image, int x0, int y0, int x1, int y1)
{
  int x, y;
  float sx, sy;

  sx = ((x1 - x0) > 0) ? 1 : -1;
  sy = (float) (y1 - y0) / (float) (x1 - x0);

  for (x = x0; x != x1; x += sx)
    {
      y = sy * (x - x0) + y0;
      *(image + x + y * width) = 200;
    }
}


int block_detailed(int x, int y, uint8_t * yuv[3])
{
  int xx, yy;
  uint32_t avg;
  int d,dy;

  avg=0;
  for (yy=0;yy<(BLOCKSIZE);yy++)
      for (xx=0;xx<(BLOCKSIZE);xx++)
      {
	  avg+=*(yuv[0]+xx+x+(yy+y)*width);
      }
  avg/=BLOCKSIZE*BLOCKSIZE;

  dy=0;
  for (yy=0;yy<(BLOCKSIZE);yy++)
      for (xx=0;xx<(BLOCKSIZE);xx++)
      {
	  d=fabs(avg-*(yuv[0]+xx+x+(yy+y)*width));
	  dy= (dy>d)? dy:d;
      }

  if(dy<16)
      return 0;
  else
      return 1;
}

/*********************************************************** 
 *                                                         * 
 * Hirarchical Motion-Search-Algorithm                     * 
 *                                                         * 
 * The resulting motion vectors are stored in the global   * 
 * array matrix[x][y][d].                                  * 
 *                                                         * 
 * Hirarchical means, that a so called full-search is per- * 
 * formed on one or more subsampled frames first, to re-   * 
 * duce the computational effort. The result is known to   * 
 * be as good as if a real full-search was done.           * 
 *                                                         * 
 ***********************************************************/

void
calculate_motion_vectors (uint8_t * ref_frame[3], uint8_t * target[3])
{
  int x, y, dx, dy;
  uint32_t center_SAD;
  uint32_t vector_SAD;
  uint32_t sum_of_SADs;
  uint32_t avrg_SAD;
  int nr_of_blocks = 0;
  static uint32_t last_mean_SAD=100;
  float mod=1;
  
  /* subsample reference frame by 2 and by 4                 */
  /* store subsampled frame in sub_r[3]                      */
  subsample_frame (sub_r2, ref_frame);
  subsample_frame (sub_r4, sub_r2);

  /* subsample target frame by 2 and by 4                    */
  /* store subsampled frame in sub_t[3]                      */
  subsample_frame (sub_t2, target);
  subsample_frame (sub_t4, sub_t2);

 /***********************************************************
  * finally we calculate the motion-vectors                 *
  *                                                         *
  * the search is performed with a searchblock of BLOCKSIZE *
  * and is overlapped by a factor of 2 (BLOCKSIZE/2)        *
  ***********************************************************/

  /* reset SAD accumulator ... */
  sum_of_SADs = 0;

  /* devide the frame in blocks ... */
  for (y = 0; y < height; y += (BLOCKSIZE>>1))
    for (x = 0; x < width; x += (BLOCKSIZE>>1))
      if(x>(BX0-4) && x<(BX1+4) && y>(BY0-4) && y<(BY1+4))
      {
        nr_of_blocks++;

        /* calculate center's SAD 
         * if there isn't a reasonable change, then don't
         * do a motion-search ...
         */

        center_SAD = calc_SAD (target[0],
                               ref_frame[0], (x) + (y) * width, (x) + (y) * width, 2);


        center_SAD += calc_SAD_uv (target[1],
                                   ref_frame[1],
                                   ((x) >>1) + ((y) >>1) * uv_width,
                                   ((x) >>1) + ((y) >>1) * uv_width, 1)>>1;

        center_SAD += calc_SAD_uv (target[2],
                                   ref_frame[2],
                                   ((x) >>1) + ((y) >>1) * uv_width,
                                   ((x) >>1) + ((y) >>1) * uv_width, 1)>>1;
                                   
        /* depending on the quality of the center SAD modulate
         * the search radius ...
         */
        if (center_SAD<mean_SAD*2)
        {
          mod_radius = 8;
        }
        else
          mod_radius = RADIUS;
        
//        if(mod_radius<8)
//          mod_radius=8;
        
        if(fixed_search_radius)
          mod_radius=radius;

          {
            /* search best matching block in the 4x4 subsampled
             * image and store the result in matrix[x][y][d]
             */
            mb_search_44 (x, y, sub_r4, sub_t4);

            /* search best matching block in the 2x2 subsampled
             * image and store the result in matrix[x][y][d]
             */
            mb_search_22 (x, y, sub_r2, sub_t2);

            /* based on that 2x2 search we start a search in the
             * real frame and store the result in matrix[x][y][d]
             */
            mb_search (x, y, ref_frame, target);
	    //mb_search_half (x, y, ref_frame, target);

            /* now, we need(!!) to check if the vector is valid...  
             * to do so, the center-SAD is calculated and compared  
             * with the vectors-SAD. If it's not better by at least 
             * a factor of 2 it's discarded...                      
             */
          }

        dx = matrix[x][y][0] >>1;
        dy = matrix[x][y][1] >>1;


        vector_SAD = calc_SAD (target[0],
                               ref_frame[0],
                               (x + dx) + (y + dy) * width, (x) + (y) * width, 2);

        vector_SAD += calc_SAD_uv (target[1],
                                   ref_frame[1],
                                   ((x + dx) >>1) + ((y + dy)>>1) * uv_width,
                                   ((x)      >>1) + ((y)     >>1) * uv_width, 1);

        vector_SAD += calc_SAD_uv (target[2],
                                   ref_frame[2],
                                   ((x + dx) >>1) + ((y + dy)>>1) * uv_width,
                                   ((x)      >>1) + ((y)     >>1) * uv_width, 1);

#if 0
        if ( vector_SAD > (center_SAD >> 4) )
	{
            vector_SAD = center_SAD;
            matrix[x][y][0] = 0;
            matrix[x][y][1] = 0;
	}
#endif

        SAD_matrix[x][y] = vector_SAD;
        sum_of_SADs += vector_SAD;
      }

  avrg_SAD = sum_of_SADs / nr_of_blocks;

  last_mean_SAD = mean_SAD;
  mean_SAD = (mean_SAD * 63 + avrg_SAD + 1)>>6;

  if (mean_SAD < 50)            /* don't go too low !!! */
    mean_SAD = 50;              /* a SAD of 100 is nearly noisefree source material */

  if (avrg_SAD > (mean_SAD *1.5) && framenr++ > 12)
    {
	framenr=0;
      mean_SAD = last_mean_SAD;
      scene_change = 1;
      if (verbose)
        {
          mjpeg_log (LOG_INFO, " *** scene change or very high activity detected  ***\n");
          mjpeg_log (LOG_INFO, " *** its better to freeze MSAD and copy reference ***\n");
        }
    }
  else
    {
      scene_change = 0;
    }

  if (verbose)
  {
    mjpeg_log (LOG_INFO, " MSAD : %d  --- Noiselevel below %2.1f Pixel (approximate) \n",
               mean_SAD, ((float) mean_SAD / 16.0 / 3.0));
    mjpeg_log (LOG_INFO, "lMSAD : %d  --- Noiselevel below %2.1f Pixel (approximate) \n",
               last_mean_SAD, ((float) last_mean_SAD / 16.0 / 3.0));
    mjpeg_log (LOG_INFO, " ASAD : %d  \n\n",avrg_SAD);
  }
}

void
preblocking_frame (uint8_t * yuv[3])
{
  int x, y, xx, yy;
  int distance;
  uint32_t avg,avgu,avgv;
  int d,dy;

  for (y = 0; y < height; y += (BLOCKSIZE/2))
    for (x = 0; x < width; x += (BLOCKSIZE/2))
    {
	avg=0;
	for (yy=0;yy<(BLOCKSIZE/2);yy++)
	    for (xx=0;xx<(BLOCKSIZE/2);xx++)
	    {
		avg+=*(yuv[0]+xx+x+(yy+y)*width);
	    }
	avg/=BLOCKSIZE*BLOCKSIZE/4;

	dy=0;
	for (yy=-BLOCKSIZE;yy<(BLOCKSIZE*2);yy++)
	    for (xx=-BLOCKSIZE;xx<(BLOCKSIZE*2);xx++)
	    {
		d=fabs(avg-*(yuv[0]+xx+x+(yy+y)*width));
		dy= (dy>d)? dy:d;
	    }

	if(dy<4)
	{
	    avgu=avgv=0;
	    for (yy=0;yy<(BLOCKSIZE/4);yy++)
		for (xx=0;xx<(BLOCKSIZE/4);xx++)
		{
		    avgu+=*(yuv[1]+(xx+x/2)+(yy+y/2)*uv_width);
		}
	    for (yy=0;yy<(BLOCKSIZE/4);yy++)
		for (xx=0;xx<(BLOCKSIZE/4);xx++)
		{
		    avgv+=*(yuv[2]+(xx+x/2)+(yy+y/2)*uv_width);
		}

	    avgu/=BLOCKSIZE*BLOCKSIZE/16;
	    avgv/=BLOCKSIZE*BLOCKSIZE/16;

	    for (yy=0;yy<(BLOCKSIZE/2);yy++)
		for (xx=0;xx<(BLOCKSIZE/2);xx++)
		{
		    *(yuv[0]+xx+x+(yy+y)*width)=avg;
		    *(yuv[1]+xx/2+x/2+(yy/2+y/2)*uv_width)=avgu;
		    *(yuv[2]+xx/2+x/2+(yy/2+y/2)*uv_width)=avgv;
		}
	}
    }
}

void
transform_frame (uint8_t * avg[3])
{
  int x, y;

  for (y = 0; y < height; y += (BLOCKSIZE >>1))
    for (x = 0; x < width; x += (BLOCKSIZE >>1))
      if(x>(BX0-4) && x<(BX1+4) && y>(BY0-4) && y<(BY1+4))
      {
        if (SAD_matrix[x][y] < (mean_SAD))
	    block_quality = 0;
        else if (SAD_matrix[x][y] < (mean_SAD * 3))
	    block_quality = 0.25;
        else
	    block_quality = 0.5;

        copy_filtered_block (x, y, yuv1, avg);

#if DEBUG
        mjpeg_log (LOG_DEBUG, "block_quality : %f \n", block_quality);
#endif

      }

  memcpy (avg[0], yuv1[0], width * height);
  memcpy (avg[1], yuv1[1], width * height / 4);
  memcpy (avg[2], yuv1[2], width * height / 4);
}

void generate_black_border(int BX0, int BY0, int BX1, int BY1, uint8_t *frame[3])
{
  int dx,dy;
  
  for(dy=0;dy<BY0;dy++)
    for(dx=0;dx<width;dx++)
    {
      *(frame[0]+dx+dy*width)=0;
      *(frame[1]+dx/2+dy/2*uv_width)=128;
      *(frame[2]+dx/2+dy/2*uv_width)=128;
    }
    
  for(dy=BY1;dy<height;dy++)
    for(dx=0;dx<width;dx++)
    {
      *(frame[0]+dx+dy*width)=0;
      *(frame[1]+dx/2+dy/2*uv_width)=128;
      *(frame[2]+dx/2+dy/2*uv_width)=128;
    }
    
  for(dy=0;dy<height;dy++)
    for(dx=0;dx<BX0;dx++)
    {
      *(frame[0]+dx+dy*width)=0;
      *(frame[1]+dx/2+dy/2*uv_width)=128;
      *(frame[2]+dx/2+dy/2*uv_width)=128;
    }

  for(dy=0;dy<height;dy++)
    for(dx=BX1;dx<width;dx++)
    {
      *(frame[0]+dx+dy*width)=0;
      *(frame[1]+dx/2+dy/2*uv_width)=128;
      *(frame[2]+dx/2+dy/2*uv_width)=128;
    }
}

void
denoise_frame (uint8_t * frame[3])
{

 /***********************************************************
  *                                                         *
  * ! MAIN PROCESSING LOOP !                                *
  *                                                         *
  * The denoising process is done the following way:        *
  *                                                         *
  * 1. Take the reference frame and calculate the motion-   *
  *    vectors to the corresponding previously filtered     *
  *    frame blocks in avrg[x].                             *
  *                                                         *
  * 2. Transform the previous filtered image with these     *
  *    motion vectors to match the current reference frame. *
  *                                                         *
  * 3. Blend over the transformed image with the actual     *
  *    reference frame.                                     *
  *                                                         *
  ***********************************************************/

  static int uninitialized = 1;

  /* if uninitialized copy reference frame into average buffer */
  if (uninitialized)
    {
      uninitialized = 0;

      memcpy (avrg[0], frame[0], width * height);
      memcpy (avrg[1], frame[1], width * height / 4);
      memcpy (avrg[2], frame[2], width * height / 4);
    }

  /* Find the motion vectors between reference frame (ref[x]) */
  /* and previous averaged frame (avrg[x]) and store the     */
  /* resulting vectors in matrix[x][y][d].                   */

  calculate_motion_vectors (frame, avrg);

  /* Transform the previously filtered image to match the    */
  /* reference frame                                         */

  transform_frame (avrg);

  /* Blend reference frame with average frame                */
  average_frames (frame, avrg);

  /* Scene Change detected ? */
  if (scene_change)
    {
      memcpy (avrg[0], frame[0], width * height);
      memcpy (avrg[1], frame[1], width * height / 4);
      memcpy (avrg[2], frame[2], width * height / 4);
    }
  else
    {
      /* Copy filtered frame into frame[x] ... */
      memcpy (frame[0], avrg[0], width * height);
      memcpy (frame[1], avrg[1], width * height / 4);
      memcpy (frame[2], avrg[2], width * height / 4);
    }
}


/* deactivating my own functions for cpu detection and switching over to 
 * cpu_accel.h . This is a far better solution, as changes on cpu_accel.h
 * will make a benefit for all programmers using MMX/MMXext/SSE/SSE2/3Dnow!
 * and 3Dnow!ext... All further changes on the code will go to cpu_accel.h!
 */

#if 0
/* well, as I have seen, now... these are pretty much the same than cpu_accel()
 * [:*] It' important to reinvent the wheel ... *argh* Hours of ... *sigh*
 * OK, if these do not work, i'll switch to cpu_accel.h 
 */

void rec_SIGILL(int sig)
{
  mjpeg_log ( LOG_INFO, "SSE/SSE2 causes SIGILL (illegal instruction) on this\n");
  mjpeg_log ( LOG_INFO, "operating system. Sorry, can't use SSE.\n");
  siglongjmp( jmp_buffer, 1 );
}

int test_SSE()
{
  /* assume SSE operations are valid on this system... */
 	int SSE_is_illegal=0; 
  
  /* Structure to save old signal handler */
  struct sigaction old_SIGILL;
    
  /* the same for the new one */
  struct sigaction recover_SIGILL;
  
  /* setup our own signal handler function */
  recover_SIGILL.sa_handler = rec_SIGILL;
  sigemptyset(&recover_SIGILL.sa_mask);
  recover_SIGILL.sa_flags = 0;
  sigaction(SIGILL, &recover_SIGILL, &old_SIGILL );
  
  /* set jump-back-address and just try it... */
  if( sigsetjmp( jmp_buffer, 1 ) == 0 )
	{
    /* a little SSE fragment which doesn't do sensful things*/
		asm  
    (
    "psadbw    %mm1, %mm0 ; /* do SSE nonsense ... ;-) */"
    );

    /* if it doesn't fail, we're here ...*/
		SSE_is_illegal = 0;
	}
	else
  {
    /* we never should have reached this point ... Sh*t happend */
		SSE_is_illegal = 1;
  }
  
  /* restore old SIGILL handler */
	sigaction(SIGSEGV, & old_SIGILL, NULL );
  
  /* return value nicely represents 0 if SSE can be used */
	return SSE_is_illegal;
}

int
test_CPU ()
{
#ifndef HAVE_X86CPU
  return 0;
#else

  /**************************************************************************
   *                                                                        *
   * The following assembly code uses the CPUID opcode as described in      *
   * Intel IA32-Chapter II.                                                 *
   *                                                                        *
   * It's assumed that this code never gets executed on IA16 machines.      *
   * Therefor we just can use this method of detecting and using CPUID.     *
   *                                                                        *
   **************************************************************************/

  uint32_t feature=0;
  uint32_t regs[4]={0,0,0,0};
  int cap = 0;

  asm volatile
    (
     "                            ;                                                "
     " pushfl                     ;/* do we have CPUID ? Bit 21 of Eflags can be */"
     " popl                 %%eax ;/* set and unset ? Copy Eflags to EAX.        */"
     " movl          %%eax, %%ebx ;/* copy Eflags to EBX                         */"
     " xorl      $0x200000, %%eax ;/* switch Bit 21 in EAX                       */"
     " pushl                %%eax ;/* push EAX to stack                          */"
     " popfl                      ;/* pop  EAX into Eflags                       */"
     " pushfl                     ;/* push Eflags on stack                       */"
     " popl                 %%eax ;/* pop  Eflags to EAX                         */"
     " cmpl          %%eax, %%ebx ;/* DO EAX and EBX differ ?                    */"
     " jz                    L991 ;/* no, CPU is too old ...                     */"
     "                            ;                                                "
     "                            ;                                                "
     " movl             $0, %%eax ;/* yes, CPU knows CPUID Opcode                */"
     " cpuid                      ;/* read identification                        */"
     " movl             %1, %%eax ;                                                "
     " movl         %%ebx,0(%%eax);/* copy identif. information to array         */"
     " movl         %%ecx,4(%%eax);/* copy identif. information to array         */"
     " movl         %%edx,8(%%eax);/* copy identif. information to array         */"
     " movl             $1, %%eax ;/* now read feature flags                     */"
     " cpuid                      ;                                                "
     " movl          %%edx,    %0 ;/* store in (feature)                         */"
     " movl       $0x0387f9ff, %0 ;/* store in (feature)                         */"
     " jmp                   L992 ;                                                "
     "                            ;                                                "
     "L991:                       ;                                                "
     "                            ;                                                "
     " movl       $0x00000000, %0 ;/* clear (i)                                  */"
     "                            ;                                                "
     "L992:                       ;                                                "
     "                            ;                                                "
     :"=X" (feature)
     :"m" (regs)
     : "eax", "ebx", "ecx", "edx", "cc"
     );
     
/* 0x756e65476c65746e49656e69 */

  mjpeg_log (LOG_INFO, "\n");
  mjpeg_log (LOG_INFO, "compiled for an IA32 CPU.\n");
  
  if(regs[0]==0x68747541 && regs[1]==0x444d4163 && regs[2]==0x69746e65)
  {
    mjpeg_log (LOG_INFO, "CPU-identifies as : Authentic AMD(TM)\n");
  }
  else
  {
    if(regs[0]==0x756e6547 && regs[1]==0x6c65746e && regs[2]==0x49656e69)
    {
      mjpeg_log (LOG_INFO, "CPU-identifies as : Genuine Intel(TM)\n");
    }
    else
    {
      mjpeg_log (LOG_INFO, "CPU-identifies as : unknown Vendor\n");
      mjpeg_log (LOG_INFO, "                    0x%08x%08x%08x\n",regs[0],regs[1],regs[2]);
    }
  }
  
  mjpeg_log (LOG_INFO, "found following accelerations:\n");
  mjpeg_log (LOG_INFO, "feature Flags (EDX) : 0x%08x\n", feature);

  if (feature & 0x00800000)
    {
      mjpeg_log (LOG_INFO, "--> CPU states to have MMX  ... good.\n");
      cap++;

      if (feature & 0x02000000)
        {
          mjpeg_log (LOG_INFO, "--> CPU states to have SSE  ... better.\n");
          
          if (feature & 0x04000000)
            {
              mjpeg_log (LOG_INFO, "--> CPU states to have SSE2 ... best!\n");
            }
            
          /* now the only one who could kill us, is the Operating System not allowing
           * SSE/SSE2 usage ... Let's see if we get sigill...
           */
  
          if(test_SSE()==0);
          {
            /* only increase cap if SSE is allowed to use ! */
            cap++;

            if (feature & 0x04000000)
              {
                /* increase cap again if SSE2 is valid */
                cap++;
              }
          }
        }
    }
    
  return cap;
#endif
}

#if 0
" pushfl                     ; /* do we have CPUID ?*/"
  " popl                  %%eax; "
  " movl           %%eax, %%ebx; "
  " xorl       $0x200000, %%eax; "
  " pushl                 %%eax; "
  " popfl                      ; "
  " pushfl                     ; "
  " popl                  %%eax; "
  " cmpl           %%eax, %%ebx; "
  " je                 no_cpuid; /* no, CPU is too old ...*/"
  "                            ; " " no_cpuid:                  ; "
#endif
#endif
