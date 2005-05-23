/***********************************************************
 * YUVDENOISER for the mjpegtools                          *
 * ------------------------------------------------------- *
 * (C) 2001-2004 Stefan Fendt                              *
 *                                                         *
 * Licensed and protected by the GNU-General-Public-       *
 * License version 2 or if you prefer any later version of *
 * that license). See the file LICENSE for detailed infor- *
 * mation.                                                 *
 *                                                         *
 * FILE: main.c                                            *
 *                                                         *
 ***********************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "config.h"
#include "mjpeg_types.h"
#include "yuv4mpeg.h"
#include "mjpeg_logging.h"
#include "cpu_accel.h"
#include "motionsearch.h"

int verbose = 1;
int width = 0;
int height = 0;
int lwidth = 0;
int lheight = 0;
int cwidth = 0;
int cheight = 0;
int input_chroma_subsampling = 0;
int input_interlaced = 0;

int Y_radius = 5;
int U_radius = 5;
int V_radius = 5;

int spat_Y_thres = 4;
int spat_U_thres = 16;	// chroma-planes usually are a lot noisier than luma!
int spat_V_thres = 16;

int temp_Y_thres = 16;
int temp_U_thres = 32;
int temp_V_thres = 32;

uint8_t *frame1[3];
uint8_t *frame2[3];
uint8_t *frame3[3];
uint8_t *frame4[3];
uint8_t *frame5[3];
uint8_t *mc1[3];
uint8_t *mc2[3];
uint8_t *mc3[3];
uint8_t *mc4[3];
uint8_t *mc5[3];

uint8_t *pixlock[3];
uint8_t *pixlock2[3];
uint8_t *pixlock3[3];
uint8_t *lockcnt[3];
uint8_t *output[3];

int buff_offset;
int buff_size;

/***********************************************************
 * helper-functions                                        *
 ***********************************************************/

/***********************************************************
 * Main Loop                                               *
 ***********************************************************/

int
main (int argc, char *argv[])
{
  char c;
  int fd_in = 0;
  int fd_out = 1;
  int errno = 0;
  y4m_frame_info_t iframeinfo;
  y4m_stream_info_t istreaminfo;
  y4m_frame_info_t oframeinfo;
  y4m_stream_info_t ostreaminfo;

  while ((c = getopt (argc, argv, "hiY:U:V:y:u:v:")) != -1)
    {
      switch (c)
	{
	case 'h':
	  {
	    mjpeg_log (LOG_INFO, " Usage of the denoiser (very brief this time... :)");
	    mjpeg_log (LOG_INFO, " will be fixed ASAP...");
	    mjpeg_log (LOG_INFO, " -Y [n]   spatial Y-filter-threshold");
	    mjpeg_log (LOG_INFO, " -U [n]   spatial U-filter-threshold");
	    mjpeg_log (LOG_INFO, " -V [n]   spatial V-filter-threshold");
	    mjpeg_log (LOG_INFO, " -y [n]   temporal Y-filter-threshold");
	    mjpeg_log (LOG_INFO, " -u [n]   temporal U-filter-threshold");
	    mjpeg_log (LOG_INFO, " -v [n]   temporal V-filter-threshold");
	    exit (0);
	    break;
	  }
	case 'i':
	  {
	    verbose = 1;
	    break;
	  }
	case 'Y':
	  {
        spat_Y_thres = atoi(optarg);
	    break;
	  }
	case 'U':
	  {
        spat_U_thres = atoi(optarg);
	    break;
	  }
	case 'V':
	  {
        spat_V_thres = atoi(optarg);
	    break;
	  }
	case 'y':
	  {
        temp_Y_thres = atoi(optarg);
	    break;
	  }
	case 'u':
	  {
        temp_U_thres = atoi(optarg);
	    break;
	  }
	case 'v':
	  {
        temp_V_thres = atoi(optarg);
	    break;
	  }
	case '?':
        default:
          exit(1);
	}
    }

  mjpeg_log (LOG_INFO, "yuvdenoise version %s",VERSION);

  /* initialize stream-information */
  y4m_accept_extensions (1);
  y4m_init_stream_info (&istreaminfo);
  y4m_init_frame_info (&iframeinfo);
  y4m_init_stream_info (&ostreaminfo);
  y4m_init_frame_info (&oframeinfo);

  /* open input stream */
  if ((errno = y4m_read_stream_header (fd_in, &istreaminfo)) != Y4M_OK)
    {
      mjpeg_log (LOG_ERROR, "Couldn't read YUV4MPEG header: %s!",
		 y4m_strerr (errno));
      exit (1);
    }

  /* get format information */
  width = y4m_si_get_width (&istreaminfo);
  height = y4m_si_get_height (&istreaminfo);
  input_chroma_subsampling = y4m_si_get_chroma (&istreaminfo);
  input_interlaced = y4m_si_get_interlace (&istreaminfo);
  mjpeg_log (LOG_INFO, "Y4M-Stream is flagged to be %ix%i(%s)",
	     width,
	     height,
	     input_chroma_subsampling == Y4M_CHROMA_420JPEG ? "4:2:0 MPEG1" :
	     input_chroma_subsampling == Y4M_CHROMA_420MPEG2 ? "4:2:0 MPEG2" :
	     input_chroma_subsampling ==
	     Y4M_CHROMA_420PALDV ? "4:2:0 PAL-DV" : input_chroma_subsampling
	     == Y4M_CHROMA_444 ? "4:4:4" : input_chroma_subsampling ==
	     Y4M_CHROMA_422 ? "4:2:2" : input_chroma_subsampling ==
	     Y4M_CHROMA_411 ? "4:1:1 NTSC-DV" : input_chroma_subsampling ==
	     Y4M_CHROMA_MONO ? "MONOCHROME" : input_chroma_subsampling ==
	     Y4M_CHROMA_444ALPHA ? "4:4:4:4 ALPHA" : "unknown");
  mjpeg_log (LOG_INFO, " ");

  // Setup the denoiser to use the appropriate chroma processing
  if (input_chroma_subsampling == Y4M_CHROMA_420JPEG   ||
      input_chroma_subsampling == Y4M_CHROMA_420MPEG2  ||
      input_chroma_subsampling == Y4M_CHROMA_420PALDV  )
    {
    lwidth = width;
    lheight = height;
    cwidth = width/2;
    cheight = height/2;
    
    mjpeg_log (LOG_INFO,"Processing Mode : 4:2:0 %s", 
               (input_interlaced==Y4M_ILACE_NONE)? "progressive":"interlaced");
    mjpeg_log (LOG_INFO,"Luma-Plane      : %ix%i pixels",lwidth,lheight);
    mjpeg_log (LOG_INFO,"Chroma-Plane    : %ix%i pixels",cwidth,cheight);
    }
    else
  if ( input_chroma_subsampling == Y4M_CHROMA_411 )
    {
    lwidth = width;
    lheight = height;
    cwidth = width/4;
    cheight = height;
    
    mjpeg_log (LOG_INFO,"Processing Mode : 4:1:1 %s", 
               (input_interlaced==Y4M_ILACE_NONE)? "progressive":"interlaced");
    mjpeg_log (LOG_INFO,"Luma-Plane      : %ix%i pixels",lwidth,lheight);
    mjpeg_log (LOG_INFO,"Chroma-Plane    : %ix%i pixels",cwidth,cheight);
    }
    else
  if ( input_chroma_subsampling == Y4M_CHROMA_422 )
    {
    lwidth = width;
    lheight = height;
    cwidth = width/2;
    cheight = height;
    
    mjpeg_log (LOG_INFO,"Processing Mode : 4:2:2 %s", 
               (input_interlaced==Y4M_ILACE_NONE)? "progressive":"interlaced");
    mjpeg_log (LOG_INFO,"Luma-Plane      : %ix%i pixels",lwidth,lheight);
    mjpeg_log (LOG_INFO,"Chroma-Plane    : %ix%i pixels",cwidth,cheight);
    }
    else
  if ( input_chroma_subsampling == Y4M_CHROMA_444 )
    {
    lwidth = width;
    lheight = height;
    cwidth = width;
    cheight = height;
    
    mjpeg_log (LOG_INFO,"Processing Mode : 4:4:4 %s", 
               (input_interlaced==Y4M_ILACE_NONE)? "progressive":"interlaced");
    mjpeg_log (LOG_INFO,"Luma-Plane      : %ix%i pixels",lwidth,lheight);
    mjpeg_log (LOG_INFO,"Chroma-Plane    : %ix%i pixels",cwidth,cheight);
    }
    else
        {
    mjpeg_log (LOG_INFO," ");
    mjpeg_log (LOG_INFO," ### This is an unsupported Y4M-Video-Mode ### ");
    mjpeg_log (LOG_INFO," ");
        exit(-1);
        }

    if(input_interlaced != Y4M_ILACE_NONE)
        {
        // process the fields as images side by side
        lwidth *= 2;
        cwidth *= 2;
        lheight /= 2;
        cheight /= 2;
        }
    
  /* the output is progressive 4:2:0 MPEG 1 */
  y4m_si_set_interlace (&ostreaminfo, y4m_si_get_interlace (&istreaminfo));
  y4m_si_set_chroma (&ostreaminfo, y4m_si_get_chroma (&istreaminfo));
  y4m_si_set_width (&ostreaminfo, width);
  y4m_si_set_height (&ostreaminfo, height);
  y4m_si_set_framerate (&ostreaminfo, y4m_si_get_framerate (&istreaminfo));
  y4m_si_set_sampleaspect (&ostreaminfo,
			   y4m_si_get_sampleaspect (&istreaminfo));

  /* write the outstream header */
  y4m_write_stream_header (fd_out, &ostreaminfo);

  /* now allocate the needed buffers */
  {
    /* calculate the memory offset needed to allow the processing
     * functions to overshot. The biggest overshot is needed for the
     * MC-functions, so we'll use 8*width...
     */
    buff_offset = width * 8;
    buff_size = buff_offset * 2 + width * height;

    frame1[0] = buff_offset + (uint8_t *) malloc (buff_size);
    frame1[1] = buff_offset + (uint8_t *) malloc (buff_size);
    frame1[2] = buff_offset + (uint8_t *) malloc (buff_size);

    frame2[0] = buff_offset + (uint8_t *) malloc (buff_size);
    frame2[1] = buff_offset + (uint8_t *) malloc (buff_size);
    frame2[2] = buff_offset + (uint8_t *) malloc (buff_size);

    frame3[0] = buff_offset + (uint8_t *) malloc (buff_size);
    frame3[1] = buff_offset + (uint8_t *) malloc (buff_size);
    frame3[2] = buff_offset + (uint8_t *) malloc (buff_size);

    frame4[0] = buff_offset + (uint8_t *) malloc (buff_size);
    frame4[1] = buff_offset + (uint8_t *) malloc (buff_size);
    frame4[2] = buff_offset + (uint8_t *) malloc (buff_size);

    frame5[0] = buff_offset + (uint8_t *) malloc (buff_size);
    frame5[1] = buff_offset + (uint8_t *) malloc (buff_size);
    frame5[2] = buff_offset + (uint8_t *) malloc (buff_size);

    mc1[0] = buff_offset + (uint8_t *) malloc (buff_size);
    mc1[1] = buff_offset + (uint8_t *) malloc (buff_size);
    mc1[2] = buff_offset + (uint8_t *) malloc (buff_size);

    mc2[0] = buff_offset + (uint8_t *) malloc (buff_size);
    mc2[1] = buff_offset + (uint8_t *) malloc (buff_size);
    mc2[2] = buff_offset + (uint8_t *) malloc (buff_size);

    mc3[0] = buff_offset + (uint8_t *) malloc (buff_size);
    mc3[1] = buff_offset + (uint8_t *) malloc (buff_size);
    mc3[2] = buff_offset + (uint8_t *) malloc (buff_size);

    mc4[0] = buff_offset + (uint8_t *) malloc (buff_size);
    mc4[1] = buff_offset + (uint8_t *) malloc (buff_size);
    mc4[2] = buff_offset + (uint8_t *) malloc (buff_size);

    mc5[0] = buff_offset + (uint8_t *) malloc (buff_size);
    mc5[1] = buff_offset + (uint8_t *) malloc (buff_size);
    mc5[2] = buff_offset + (uint8_t *) malloc (buff_size);

    pixlock[0] = buff_offset + (uint8_t *) malloc (buff_size);
    pixlock[1] = buff_offset + (uint8_t *) malloc (buff_size);
    pixlock[2] = buff_offset + (uint8_t *) malloc (buff_size);

    pixlock2[0] = buff_offset + (uint8_t *) malloc (buff_size);
    pixlock2[1] = buff_offset + (uint8_t *) malloc (buff_size);
    pixlock2[2] = buff_offset + (uint8_t *) malloc (buff_size);

    pixlock3[0] = buff_offset + (uint8_t *) malloc (buff_size);
    pixlock3[1] = buff_offset + (uint8_t *) malloc (buff_size);
    pixlock3[2] = buff_offset + (uint8_t *) malloc (buff_size);

    lockcnt[0] = buff_offset + (uint8_t *) malloc (buff_size);
    lockcnt[1] = buff_offset + (uint8_t *) malloc (buff_size);
    lockcnt[2] = buff_offset + (uint8_t *) malloc (buff_size);

    output[0] = buff_offset + (uint8_t *) malloc (buff_size);
    output[1] = buff_offset + (uint8_t *) malloc (buff_size);
    output[2] = buff_offset + (uint8_t *) malloc (buff_size);

  mjpeg_log (LOG_INFO, "Buffers allocated.");
  }

  /* initialize motion_library */
  init_motion_search ();

  /* read every frame until the end of the input stream and process it */
  while (Y4M_OK == (errno = y4m_read_frame (fd_in,
					    &istreaminfo,
					    &iframeinfo, frame1 )))
    {
	static uint32_t frame_nr=0;
	uint8_t * temp[3];

	// motion-compensate frames to the reference frame
	{
		int x,y,vx,vy,sx,sy;
		int bx,by;
		uint32_t sad;
		uint32_t min;

		int r=8;

		// blocksize may not(!) be to small for this type of denoiser
		for(y=0;y<lheight;y+=32)
			for(x=0;x<lwidth;x+=32)
			{

			// search the best vector for frame2
			min = psad_00 (	frame3[0]+(x)+(y)*lwidth,
					frame2[0]+(x)+(y)*lwidth,
					lwidth,32,0x00ffffff );
			min += psad_00 (frame3[0]+(x+16)+(y)*lwidth,
					frame2[0]+(x+16)+(y)*lwidth,
					lwidth,32,0x00ffffff );
			vx=vy=bx=by=0;
			for(sy=-r;sy<=r;sy++)
				for(sx=-r;sx<=r;sx++)
				{
				sad  = psad_00 (	frame3[0]+(x   )+(y   )*lwidth,
							frame2[0]+(x+sx)+(y+sy)*lwidth,
							lwidth,32,0x00ffffff );
				sad += psad_00 (	frame3[0]+(x   +16)+(y   )*lwidth,
							frame2[0]+(x+sx+16)+(y+sy)*lwidth,
							lwidth,32,0x00ffffff );
				if(sad<min)
					{
					min = sad;
					vx=bx=sx;
					vy=by=sy;
					}
				}			

			// compensate block for frame2
			{
			uint8_t *p0 = mc2[0]+(x)+(y)*lwidth;
			uint8_t *p1 = frame2[0]+(x+vx)+(y+vy)*lwidth;
			for(sy=0;sy<32;sy++)
				{
					for(sx=0;sx<32;sx++)
					{
					*(p0) = *(p1);
					p0++;
					p1++;
					}
				p0 += lwidth-32;
				p1 += lwidth-32;
				}
			}

			// search the best vector for frame1 (arround the match of frame2)
			min = psad_00 (	frame3[0]+(x)+(y)*lwidth,
					frame1[0]+(x+bx)+(y+by)*lwidth,
					lwidth,32,0x00ffffff );
			min += psad_00 (frame3[0]+(x+16)+(y)*lwidth,
					frame1[0]+(x+bx+16)+(y+by)*lwidth,
					lwidth,32,0x00ffffff );
			vx=vy=0;
			for(sy=(by-r);sy<=(by+r);sy++)
				for(sx=(bx-r);sx<=(bx+r);sx++)
				{
				sad  = psad_00 (	frame3[0]+(x   )+(y   )*lwidth,
							frame1[0]+(x+sx)+(y+sy)*lwidth,
							lwidth,32,0x00ffffff );
				sad += psad_00 (	frame3[0]+(x   )+(y   )*lwidth,
							frame1[0]+(x+sx)+(y+sy)*lwidth,
							lwidth,32,0x00ffffff );
				if(sad<min)
					{
					min = sad;
					vx=sx;
					vy=sy;
					}
				}			

			// compensate block for frame1
			{
			uint8_t *p0 = mc1[0]+(x)+(y)*lwidth;
			uint8_t *p1 = frame1[0]+(x+vx)+(y+vy)*lwidth;
			for(sy=0;sy<32;sy++)
				{
					for(sx=0;sx<32;sx++)
					{
					*(p0) = *(p1);
					p0++;
					p1++;
					}
				p0 += lwidth-32;
				p1 += lwidth-32;
				}
			}

			// search the best vector for frame4
			min = psad_00 (	frame3[0]+(x)+(y)*lwidth,
					frame4[0]+(x)+(y)*lwidth,
					lwidth,32,0x00ffffff );
			min += psad_00 (frame3[0]+(x+16)+(y)*lwidth,
					frame4[0]+(x+16)+(y)*lwidth,
					lwidth,32,0x00ffffff );
			vx=vy=bx=by=0;
			for(sy=-r;sy<=r;sy++)
				for(sx=-r;sx<=r;sx++)
				{
				sad  = psad_00 (	frame3[0]+(x   )+(y   )*lwidth,
							frame4[0]+(x+sx)+(y+sy)*lwidth,
							lwidth,32,0x00ffffff );
				sad += psad_00 (	frame3[0]+(x   +16)+(y   )*lwidth,
							frame4[0]+(x+sx+16)+(y+sy)*lwidth,
							lwidth,32,0x00ffffff );
				if(sad<min)
					{
					min = sad;
					vx=bx=sx;
					vy=by=sy;
					}
				}			

			// compensate block for frame4
			{
			uint8_t *p0 = mc4[0]+(x)+(y)*lwidth;
			uint8_t *p1 = frame4[0]+(x+vx)+(y+vy)*lwidth;
			for(sy=0;sy<32;sy++)
				{
					for(sx=0;sx<32;sx++)
					{
					*(p0) = *(p1);
					p0++;
					p1++;
					}
				p0 += lwidth-32;
				p1 += lwidth-32;
				}
			}

			// search the best vector for frame5 (arround the match of frame4)
			min = psad_00 (	frame3[0]+(x)+(y)*lwidth,
					frame5[0]+(x+bx)+(y+by)*lwidth,
					lwidth,32,0x00ffffff );
			min += psad_00 (frame3[0]+(x+16)+(y)*lwidth,
					frame5[0]+(x+bx+16)+(y+by)*lwidth,
					lwidth,32,0x00ffffff );
			vx=vy=0;
			for(sy=(by-r);sy<=(by+r);sy++)
				for(sx=(bx-r);sx<=(bx+r);sx++)
				{
				sad  = psad_00 (	frame3[0]+(x   )+(y   )*lwidth,
							frame5[0]+(x+sx)+(y+sy)*lwidth,
							lwidth,32,0x00ffffff );
				sad += psad_00 (	frame3[0]+(x   )+(y   )*lwidth,
							frame5[0]+(x+sx)+(y+sy)*lwidth,
							lwidth,32,0x00ffffff );
				if(sad<min)
					{
					min = sad;
					vx=sx;
					vy=sy;
					}
				}			

			// compensate block for frame5
			{
			uint8_t *p0 = mc5[0]+(x)+(y)*lwidth;
			uint8_t *p1 = frame5[0]+(x+vx)+(y+vy)*lwidth;
			for(sy=0;sy<32;sy++)
				{
					for(sx=0;sx<32;sx++)
					{
					*(p0) = *(p1);
					p0++;
					p1++;
					}
				p0 += lwidth-32;
				p1 += lwidth-32;
				}
			}
	}
	}

#if 1
	// mix frames
	{
	int x,y;
	int mean;
	int delta;
	int delta_sum;
	uint32_t interpolated_pixel;
	int t=20;
	int ref;

	uint8_t *p1 = mc1[0];
	uint8_t *p2 = mc2[0];
	uint8_t *p3 = frame3[0];
	uint8_t *p4 = mc4[0];
	uint8_t *p5 = mc5[0];
	uint8_t *dst = mc3[0];

	// denoise the luma-plane
	for(y=0;y<lheight;y++)
		for(x=0;x<lwidth;x++)
		{

		ref  = *(p3-lwidth);
		ref += *(p3);
		ref += *(p3+lwidth);
		ref /= 3;

		interpolated_pixel = *(p3)*t;
		delta_sum = t;

		delta = abs( *(p1)-ref );
		delta = (delta<t)? t-delta:0;
		delta_sum += delta;
		interpolated_pixel += *(p1)*delta;

		delta = abs( *(p2)-ref );
		delta = (delta<t)? t-delta:0;
		delta_sum += delta;
		interpolated_pixel += *(p2)*delta;

		delta = abs( *(p4)-ref );
		delta = (delta<t)? t-delta:0;
		delta_sum += delta;
		interpolated_pixel += *(p4)*delta;

		delta = abs( *(p5)-ref );
		delta = (delta<t)? t-delta:0;
		delta_sum += delta;
		interpolated_pixel += *(p5)*delta;

		mean  = *(p1);
		mean += *(p2);
		mean += *(p4);
		mean += *(p5);
		mean /= 4;
		delta = abs( mean-ref );
		delta = (delta<t)? t-delta:0;
		delta_sum += delta;
		interpolated_pixel += mean*delta;

		mean  = *(p1);
		mean += *(p2);
		mean /= 2;
		delta = abs( mean-ref );
		delta = (delta<t)? t-delta:0;
		delta_sum += delta;
		interpolated_pixel += mean*delta;

		mean  = *(p4);
		mean += *(p5);
		mean /= 2;
		delta = abs( mean-ref );
		delta = (delta<t)? t-delta:0;
		delta_sum += delta;
		interpolated_pixel += mean*delta;

		interpolated_pixel /= delta_sum;
		*(dst)=interpolated_pixel;

		p1++;
		p2++;
		p3++;
		p4++;
		p5++;
		dst++;
		}

	// denoise chroma1-plane
	p1 = frame1[1];
	p2 = frame2[1];
	p3 = frame3[1];
	p4 = frame4[1];
	p5 = frame5[1];
	dst = mc3[1];
	for(y=0;y<cheight;y++)
		for(x=0;x<cwidth;x++)
		{

		ref  = *(p3-cwidth);
		ref += *(p3);
		ref += *(p3+cwidth);
		ref /= 3;

		interpolated_pixel = *(p3)*t;
		delta_sum = t;

		delta = abs( *(p1)-ref );
		delta = (delta<t)? t-delta:0;
		delta_sum += delta;
		interpolated_pixel += *(p1)*delta;

		delta = abs( *(p2)-ref );
		delta = (delta<t)? t-delta:0;
		delta_sum += delta;
		interpolated_pixel += *(p2)*delta;

		delta = abs( *(p4)-ref );
		delta = (delta<t)? t-delta:0;
		delta_sum += delta;
		interpolated_pixel += *(p4)*delta;

		delta = abs( *(p5)-ref );
		delta = (delta<t)? t-delta:0;
		delta_sum += delta;
		interpolated_pixel += *(p5)*delta;

		mean  = *(p1);
		mean += *(p2);
		mean += *(p4);
		mean += *(p5);
		mean /= 4;
		delta = abs( mean-ref );
		delta = (delta<t)? t-delta:0;
		delta_sum += delta;
		interpolated_pixel += mean*delta;

		mean  = *(p1);
		mean += *(p2);
		mean /= 2;
		delta = abs( mean-ref );
		delta = (delta<t)? t-delta:0;
		delta_sum += delta;
		interpolated_pixel += mean*delta;

		mean  = *(p4);
		mean += *(p5);
		mean /= 2;
		delta = abs( mean-ref );
		delta = (delta<t)? t-delta:0;
		delta_sum += delta;
		interpolated_pixel += mean*delta;

		interpolated_pixel /= delta_sum;
		*(dst)=interpolated_pixel;

		p1++;
		p2++;
		p3++;
		p4++;
		p5++;
		dst++;
		}
	// denoise chroma2-plane
	p1 = frame1[2];
	p2 = frame2[2];
	p3 = frame3[2];
	p4 = frame4[2];
	p5 = frame5[2];
	dst = mc3[2];
	for(y=0;y<cheight;y++)
		for(x=0;x<cwidth;x++)
		{

		ref  = *(p3-cwidth);
		ref += *(p3);
		ref += *(p3+cwidth);
		ref /= 3;

		interpolated_pixel = *(p3)*t;
		delta_sum = t;

		delta = abs( *(p1)-ref );
		delta = (delta<t)? t-delta:0;
		delta_sum += delta;
		interpolated_pixel += *(p1)*delta;

		delta = abs( *(p2)-ref );
		delta = (delta<t)? t-delta:0;
		delta_sum += delta;
		interpolated_pixel += *(p2)*delta;

		delta = abs( *(p4)-ref );
		delta = (delta<t)? t-delta:0;
		delta_sum += delta;
		interpolated_pixel += *(p4)*delta;

		delta = abs( *(p5)-ref );
		delta = (delta<t)? t-delta:0;
		delta_sum += delta;
		interpolated_pixel += *(p5)*delta;

		mean  = *(p1);
		mean += *(p2);
		mean += *(p4);
		mean += *(p5);
		mean /= 4;
		delta = abs( mean-ref );
		delta = (delta<t)? t-delta:0;
		delta_sum += delta;
		interpolated_pixel += mean*delta;

		mean  = *(p1);
		mean += *(p2);
		mean /= 2;
		delta = abs( mean-ref );
		delta = (delta<t)? t-delta:0;
		delta_sum += delta;
		interpolated_pixel += mean*delta;

		mean  = *(p4);
		mean += *(p5);
		mean /= 2;
		delta = abs( mean-ref );
		delta = (delta<t)? t-delta:0;
		delta_sum += delta;
		interpolated_pixel += mean*delta;

		interpolated_pixel /= delta_sum;
		*(dst)=interpolated_pixel;

		p1++;
		p2++;
		p3++;
		p4++;
		p5++;
		dst++;
		}
	}
#endif
#if 1
	// smooth pixels a little further ...
	{
	int x,y;
	int delta;
	int t=7;

	for(y=0;y<lheight;y++)
		for(x=0;x<lwidth;x++)
		{
			delta = abs( *(pixlock[0]+x+y*lwidth) -  *(mc3[0]+x+y*lwidth) );
			if(delta>t)
				{
				*(lockcnt[0]+x+y*lwidth)=2;
				}
			delta = abs( *(pixlock[0]+x+y*lwidth) -  *(pixlock3[0]+x+y*lwidth) );
			if(delta>t)
				{
				*(lockcnt[0]+x+y*lwidth)=2;
				}
			delta = abs( *(pixlock[0]+x+y*lwidth) -  *(pixlock2[0]+x+y*lwidth) );
			if(delta>t)
				{
				*(lockcnt[0]+x+y*lwidth)=2;
				}

			if(*(lockcnt[0]+x+y*lwidth)>0)
				{
				*(lockcnt[0]+x+y*lwidth)=*(lockcnt[0]+x+y*lwidth)-1;
				*(pixlock[0]+x+y*lwidth) = *(pixlock3[0]+x+y*lwidth);
				}
			else
				*(pixlock[0]+x+y*lwidth) = ( *(pixlock[0]+x+y*lwidth)+
							     *(pixlock2[0]+x+y*lwidth)+
							     *(pixlock3[0]+x+y*lwidth) )/3;

			delta = abs( *(pixlock[1]+x+y*lwidth) -  *(mc3[1]+x+y*lwidth) );
			if(delta>t)
				{
				*(lockcnt[1]+x+y*lwidth)=2;
				}
			delta = abs( *(pixlock[1]+x+y*lwidth) -  *(pixlock3[1]+x+y*lwidth) );
			if(delta>t)
				{
				*(lockcnt[1]+x+y*lwidth)=2;
				}
			delta = abs( *(pixlock[1]+x+y*lwidth) -  *(pixlock2[1]+x+y*lwidth) );
			if(delta>t)
				{
				*(lockcnt[1]+x+y*lwidth)=2;
				}

			if(*(lockcnt[1]+x+y*lwidth)>0)
				{
				*(lockcnt[1]+x+y*lwidth)=*(lockcnt[1]+x+y*lwidth)-1;
				*(pixlock[1]+x+y*lwidth)=*(pixlock3[1]+x+y*lwidth);
				}
			else
				*(pixlock[1]+x+y*lwidth) = ( *(pixlock[1]+x+y*lwidth)+
							     *(pixlock2[1]+x+y*lwidth)+
							     *(pixlock3[1]+x+y*lwidth) )/3;


			delta = abs( *(pixlock[2]+x+y*lwidth) -  *(mc3[2]+x+y*lwidth) );
			if(delta>t)
				{
				*(lockcnt[2]+x+y*lwidth)=2;
				}
			delta = abs( *(pixlock[2]+x+y*lwidth) -  *(pixlock3[2]+x+y*lwidth) );
			if(delta>t)
				{
				*(lockcnt[2]+x+y*lwidth)=2;
				}
			delta = abs( *(pixlock[2]+x+y*lwidth) -  *(pixlock2[2]+x+y*lwidth) );
			if(delta>t)
				{
				*(lockcnt[2]+x+y*lwidth)=2;
				}

			if(*(lockcnt[2]+x+y*lwidth)>0)
				{
				*(lockcnt[2]+x+y*lwidth)=*(lockcnt[2]+x+y*lwidth)-1;
				*(pixlock[2]+x+y*lwidth)=*(pixlock3[2]+x+y*lwidth);
				}
			else
				*(pixlock[2]+x+y*lwidth) = ( *(pixlock[2]+x+y*lwidth)+
							     *(pixlock2[2]+x+y*lwidth)+
							     *(pixlock3[2]+x+y*lwidth) )/3;

		}
	memcpy (pixlock3[0],pixlock2[0],lwidth*lheight);
	memcpy (pixlock3[1],pixlock2[1],cwidth*cheight);
	memcpy (pixlock3[2],pixlock2[2],cwidth*cheight);
	memcpy (pixlock2[0],mc3[0],lwidth*lheight);
	memcpy (pixlock2[1],mc3[1],cwidth*cheight);
	memcpy (pixlock2[2],mc3[2],cwidth*cheight);
	}
#endif
	// increase frame-counter
    frame_nr++;
    if(frame_nr>4)
	    y4m_write_frame (fd_out, &ostreaminfo, &oframeinfo, pixlock);

	// rotate buffer pointers to rotate input-buffers
	temp[0] = frame5[0];
	temp[1] = frame5[1];
	temp[2] = frame5[2];

	frame5[0] = frame4[0];
	frame5[1] = frame4[1];
	frame5[2] = frame4[2];

	frame4[0] = frame3[0];
	frame4[1] = frame3[1];
	frame4[2] = frame3[2];

	frame3[0] = frame2[0];
	frame3[1] = frame2[1];
	frame3[2] = frame2[2];

	frame2[0] = frame1[0];
	frame2[1] = frame1[1];
	frame2[2] = frame1[2];

	frame1[0] = temp[0];
	frame1[1] = temp[1];
	frame1[2] = temp[2];
    }

  /* free allocated buffers */
  {
    free (frame1[0] - buff_offset);
    free (frame1[1] - buff_offset);
    free (frame1[2] - buff_offset);

    free (frame2[0] - buff_offset);
    free (frame2[1] - buff_offset);
    free (frame2[2] - buff_offset);

    free (frame3[0] - buff_offset);
    free (frame3[1] - buff_offset);
    free (frame3[2] - buff_offset);

    free (frame4[0] - buff_offset);
    free (frame4[1] - buff_offset);
    free (frame4[2] - buff_offset);

    free (frame5[0] - buff_offset);
    free (frame5[1] - buff_offset);
    free (frame5[2] - buff_offset);

    free (mc1[0] - buff_offset);
    free (mc1[1] - buff_offset);
    free (mc1[2] - buff_offset);

    free (mc2[0] - buff_offset);
    free (mc2[1] - buff_offset);
    free (mc2[2] - buff_offset);

    free (mc3[0] - buff_offset);
    free (mc3[1] - buff_offset);
    free (mc3[2] - buff_offset);

    free (mc4[0] - buff_offset);
    free (mc4[1] - buff_offset);
    free (mc4[2] - buff_offset);

    free (mc5[0] - buff_offset);
    free (mc5[1] - buff_offset);
    free (mc5[2] - buff_offset);

    free (pixlock[0] - buff_offset);
    free (pixlock[1] - buff_offset);
    free (pixlock[2] - buff_offset);

    free (pixlock2[0] - buff_offset);
    free (pixlock2[1] - buff_offset);
    free (pixlock2[2] - buff_offset);

    free (lockcnt[0] - buff_offset);
    free (lockcnt[1] - buff_offset);
    free (lockcnt[2] - buff_offset);

    free (output[0] - buff_offset);
    free (output[1] - buff_offset);
    free (output[2] - buff_offset);

    mjpeg_log (LOG_INFO, "Buffers freed.");
  }

  /* did stream end unexpectedly ? */
  if (errno != Y4M_ERR_EOF)
    mjpeg_error_exit1 ("%s", y4m_strerr (errno));

  /* Exit gently */
  return (0);
}
