/***********************************************************
 * A new denoiser for the mjpegtools project               *
 * ------------------------------------------------------- *
 * (C) 2004 Steven Boswell.                                *
 * Based on yuvdenoise/main.c, (C) 2001,2002 Stefan Fendt  *
 *                                                         *
 * Licensed and protected by the GNU-General-Public-       *
 * License version 2 or if you prefer any later version of *
 * that license). See the file COPYING for detailed infor- *
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
#include "config.h"
#include "newdenoise.hh"

// HACK
#include <sys/stat.h>
#include <fcntl.h>

void process_commandline(int argc, char *argv[]);
void alloc_buffers (void);

DNSR_GLOBAL denoiser;
int frame = 0;
char *g_pszInputFile = NULL;		// HACK

/***********************************************************
 *                                                         *
 ***********************************************************/

int main(int argc, char *argv[])
{
  int fd_in  = 0;
  int fd_out = 1;
  int errno  = 0;
  
  y4m_frame_info_t frameinfo;
  y4m_stream_info_t streaminfo;
  int chroma_mode;

  frame = 0;

  /* initialize stream-information */
  y4m_accept_extensions (1);
  y4m_init_stream_info (&streaminfo);
  y4m_init_frame_info (&frameinfo);
  
  /* setup denoiser's global variables */
  denoiser.skip               = 0;
  denoiser.frames             = 10;
  denoiser.interlaced         = -1;
  denoiser.bwonly             = 0;
  denoiser.radiusY            = 16;
  denoiser.radiusCbCr         = -1;
  denoiser.thresholdY         = 3; /* assume medium noise material */
  denoiser.thresholdCbCr      = -1;
  denoiser.matchCountThrottle = 5;
  denoiser.matchSizeThrottle  = 2;
  
  /* process commandline */
  process_commandline(argc, argv);
  if (denoiser.radiusCbCr == -1)
  	denoiser.radiusCbCr = denoiser.radiusY;
  if (denoiser.thresholdCbCr == -1)
  	denoiser.thresholdCbCr = denoiser.thresholdY;

	/* HACK: open input file. */
	if (g_pszInputFile != NULL)
	{
		fd_in = open (g_pszInputFile, O_RDONLY);
		if (fd_in == -1)
		{
			mjpeg_log (LOG_ERROR, "Couldn't open input file %s: %s!",
				g_pszInputFile, y4m_strerr (errno));
			exit (1);
		}
	}

	/* open input stream */
	if ((errno = y4m_read_stream_header (fd_in, &streaminfo)) != Y4M_OK)
		mjpeg_error_exit1 ("Couldn't read YUV4MPEG header: %s!",
			y4m_strerr (errno));
	if (y4m_si_get_plane_count (&streaminfo) != 3)
		mjpeg_error_exit1 ("Only 3-plane formats supported.");
	denoiser.frame.w = y4m_si_get_width (&streaminfo);
	denoiser.frame.h = y4m_si_get_height (&streaminfo);
	chroma_mode = y4m_si_get_chroma (&streaminfo);
	denoiser.frame.ss_h = y4m_chroma_ss_x_ratio (chroma_mode).d;
	denoiser.frame.ss_v = y4m_chroma_ss_y_ratio (chroma_mode).d;
	denoiser.frame.Cw=denoiser.frame.w/denoiser.frame.ss_h;
	denoiser.frame.Ch=denoiser.frame.h/denoiser.frame.ss_v;

  /* write the outstream header */
  y4m_write_stream_header (fd_out, &streaminfo); 

  /* allocate memory for frames */
  alloc_buffers();
	
  /* set interlacing, if it hasn't been yet. */
  if (denoiser.interlaced == -1)
  {
    int n = y4m_si_get_interlace(&streaminfo);
    switch (n)
    {
    case Y4M_ILACE_NONE:
         denoiser.interlaced = 0;
         break;
    case Y4M_ILACE_TOP_FIRST:
         denoiser.interlaced = 1;
         break;
    case Y4M_ILACE_BOTTOM_FIRST:
         denoiser.interlaced = 2;
         break;
    default:
         mjpeg_warn("Unknown interlacing '%d', assuming non-interlaced", n);
         denoiser.interlaced = 0;
         break;
    }
  }

	/* initialize the denoiser */
	errno = newdenoise_init (denoiser.frames, denoiser.frame.w,
		denoiser.frame.h, (denoiser.bwonly) ? 0 : denoiser.frame.Cw,
		(denoiser.bwonly) ? 0 : denoiser.frame.Ch);
	if (errno == -1)
		mjpeg_error_exit1( "Could not initialize denoiser");

  /* read every single frame until the end of the input stream */
  while 
    (
          Y4M_OK == ( errno = y4m_read_frame (fd_in, 
                                              &streaminfo, 
                                              &frameinfo, 
                                              denoiser.frame.in) )
    )
    {
	  frame++;
	  //if (frame < 5395) continue;	// MAJOR HACK MAJOR HACK MAJOR HACK
	  //if (frame == 5455) break;	// MAJOR HACK MAJOR HACK MAJOR HACK
	  // fprintf (stderr, "Frame %d\r", frame);	// HACK
	  // MAJOR HACK: cut out commercials
	  //if ((frame < 54692)
	  //|| (frame >= 20829 && frame <= 28231)
	  //|| (frame >= 42737 && frame <= 45764)
	  //|| (frame >= 68541 && frame <= 74895)
	  //|| (frame >= 92217 && frame <= 95814))
	  if (frame <= denoiser.skip)
	  {
         y4m_write_frame ( fd_out, 
                        &streaminfo, 
                        &frameinfo, 
                        denoiser.frame.in);
		 continue;
	  }
      
		/* denoise the current frame */
		errno = ((denoiser.interlaced == 0) ? newdenoise_frame
			: newdenoise_interlaced_frame) (denoiser.frame.in[0],
			denoiser.frame.in[1], denoiser.frame.in[2],
			denoiser.frame.out[0], denoiser.frame.out[1],
			denoiser.frame.out[2]);
		if (errno == -1)
			mjpeg_error_exit1( "Could not denoise frame %d", frame);
	      
		/* if b/w was selected, set the frame color to white */
		if (denoiser.bwonly)
		{
			int i, iExtent = denoiser.frame.Cw * denoiser.frame.Ch;

			for (i = 0; i < iExtent; ++i)
				denoiser.frame.out[1][i] = 128;
			for (i = 0; i < iExtent; ++i)
				denoiser.frame.out[2][i] = 128;
		}

		/* if there was some output, write it. */
		if (errno == 0)
	    	y4m_write_frame ( fd_out, 
	                        &streaminfo, 
	                        &frameinfo, 
	                        denoiser.frame.out);
    }
    
  /* did stream end unexpectedly ? */
  if(errno != Y4M_ERR_EOF && errno != Y4M_OK )
          mjpeg_error_exit1( "%s", y4m_strerr( errno ) );
  
  /* write out any remaining frames */
  for (;;)
  {
	frame++;
	// fprintf (stderr, "Frame %d\r", frame);	// HACK
	errno = ((denoiser.interlaced == 0) ? newdenoise_frame
		: newdenoise_interlaced_frame) (NULL, NULL, NULL,
		denoiser.frame.out[0], denoiser.frame.out[1],
		denoiser.frame.out[2]);
	if (errno == -1)
		mjpeg_error_exit1( "Could not denoise frame %d", frame);
    /* if there was no output, leave. */
	if (errno == 1)
		break;

	/* if b/w was selected, set the frame color to white */
	if (denoiser.bwonly)
	{
		int i, iExtent = denoiser.frame.Cw * denoiser.frame.Ch;

		for (i = 0; i < iExtent; ++i)
			denoiser.frame.out[1][i] = 128;
		for (i = 0; i < iExtent; ++i)
			denoiser.frame.out[2][i] = 128;
	}

	/* if there was some output, write it. */
	if (errno == 0)
    	y4m_write_frame ( fd_out, 
                        &streaminfo, 
                        &frameinfo, 
                        denoiser.frame.out);
  }

  /* Exit gently */
  return(0);
}



void
process_commandline(int argc, char *argv[])
{
  char c;

  while ((c = getopt (argc, argv, "t:T:r:R:m:M:s:f:BI:i:")) != -1)	// HACK
  {
    switch (c)
    {
	  case 'i':	// HACK
	  {
	  	g_pszInputFile = optarg;
		break;
	  }
      case 'r':
      {
        denoiser.radiusY = atoi(optarg);
        if(denoiser.radiusY<4)
        {
          denoiser.radiusY=4;
  	      mjpeg_log (LOG_WARN, "Minimum allowed search radius is 4 pixels.");
        }
        break;
	  }
      case 'R':
      {
        denoiser.radiusCbCr = atoi(optarg);
        if(denoiser.radiusCbCr<4)
        {
          denoiser.radiusCbCr=4;
  	      mjpeg_log (LOG_WARN, "Minimum allowed color search radius is 4 pixel.");
        }
        break;
      }
      case 't':
      {
        denoiser.thresholdY = atoi(optarg);
        break;
      }
      case 'T':
      {
        denoiser.thresholdCbCr = atoi(optarg);
        break;
      }
      case 'm':
      {
        denoiser.matchCountThrottle = atoi(optarg);
        break;
      }
      case 'M':
      {
        denoiser.matchSizeThrottle = atoi(optarg);
        break;
      }
      case 's':
      {
        denoiser.skip = atoi(optarg);
        break;
      }
      case 'f':
      {
        denoiser.frames = atoi(optarg);
        break;
      }
      case 'B':
      {
        denoiser.bwonly = 1;
        break;
      }
      case 'I':
      {
	 	int interlaced = atoi (optarg);
		if (interlaced != 0 && interlaced != 1 && interlaced != 2)
		{
      		mjpeg_log (LOG_ERROR, "-I must be either 0, 1, or 2");
			exit (1);
		}
        denoiser.interlaced = interlaced;
        break;
      }
    }
  }
}


void alloc_buffers(void)
{
  int luma_buffsize; 
  int chroma_buffsize;

  luma_buffsize = denoiser.frame.w * denoiser.frame.h;
  chroma_buffsize = denoiser.frame.Cw * denoiser.frame.Ch;
  
  denoiser.frame.in[0] = (uint8_t *) malloc (luma_buffsize);
  denoiser.frame.in[1] = (uint8_t *) malloc (chroma_buffsize);
  denoiser.frame.in[2] = (uint8_t *) malloc (chroma_buffsize);
  
  denoiser.frame.out[0] = (uint8_t *) malloc (luma_buffsize);
  denoiser.frame.out[1] = (uint8_t *) malloc (chroma_buffsize);
  denoiser.frame.out[2] = (uint8_t *) malloc (chroma_buffsize);

	if ( denoiser.frame.in[0] == NULL
	|| denoiser.frame.in[1] == NULL
	|| denoiser.frame.in[2] == NULL
	|| denoiser.frame.out[0] == NULL
	|| denoiser.frame.out[1] == NULL
	|| denoiser.frame.out[2] == NULL)
		mjpeg_error_exit1( "Out of memory "
			"when allocating frame buffers");
}
