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

int temp_Y_thres = 8;
int temp_U_thres = 16;
int temp_V_thres = 16;

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

uint8_t *pixlock1[3];
uint8_t *pixlock2[3];
uint8_t *pixlock3[3];
uint8_t *pixlock4[3];
uint8_t *pixlock5[3];
uint8_t *pixlock6[3];
uint8_t *pixlock7[3];
uint8_t *outframe[3];

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

  while ((c = getopt (argc, argv, "hiy:u:v:")) != -1)
    {
      switch (c)
	{
	case 'h':
	  {
	    mjpeg_log (LOG_INFO, " Usage of the denoiser (very brief this time... :)");
	    mjpeg_log (LOG_INFO, " will be fixed ASAP...");
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

    pixlock1[0] = buff_offset + (uint8_t *) malloc (buff_size);
    pixlock1[1] = buff_offset + (uint8_t *) malloc (buff_size);
    pixlock1[2] = buff_offset + (uint8_t *) malloc (buff_size);

    pixlock2[0] = buff_offset + (uint8_t *) malloc (buff_size);
    pixlock2[1] = buff_offset + (uint8_t *) malloc (buff_size);
    pixlock2[2] = buff_offset + (uint8_t *) malloc (buff_size);

    pixlock3[0] = buff_offset + (uint8_t *) malloc (buff_size);
    pixlock3[1] = buff_offset + (uint8_t *) malloc (buff_size);
    pixlock3[2] = buff_offset + (uint8_t *) malloc (buff_size);

    pixlock4[0] = buff_offset + (uint8_t *) malloc (buff_size);
    pixlock4[1] = buff_offset + (uint8_t *) malloc (buff_size);
    pixlock4[2] = buff_offset + (uint8_t *) malloc (buff_size);

    pixlock5[0] = buff_offset + (uint8_t *) malloc (buff_size);
    pixlock5[1] = buff_offset + (uint8_t *) malloc (buff_size);
    pixlock5[2] = buff_offset + (uint8_t *) malloc (buff_size);

    pixlock6[0] = buff_offset + (uint8_t *) malloc (buff_size);
    pixlock6[1] = buff_offset + (uint8_t *) malloc (buff_size);
    pixlock6[2] = buff_offset + (uint8_t *) malloc (buff_size);

    pixlock7[0] = buff_offset + (uint8_t *) malloc (buff_size);
    pixlock7[1] = buff_offset + (uint8_t *) malloc (buff_size);
    pixlock7[2] = buff_offset + (uint8_t *) malloc (buff_size);

    outframe[0] = buff_offset + (uint8_t *) malloc (buff_size);
    outframe[1] = buff_offset + (uint8_t *) malloc (buff_size);
    outframe[2] = buff_offset + (uint8_t *) malloc (buff_size);

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

		int r=7; 

		// blocksize may not(!) be to small for this type of denoiser
		for(y=0;y<lheight;y+=8)
			for(x=0;x<lwidth;x+=8)
			{

			// search the best vector for frame2
			min = psad_sub22 (	frame3[0]+(x)+(y)*lwidth,
					frame2[0]+(x)+(y)*lwidth,
					lwidth,8 )*0.8;
			vx=vy=bx=by=0;
			for(sy=-r;sy<=r;sy++)
				for(sx=-r;sx<=r;sx++)
				{
				sad  = psad_sub22 (	frame3[0]+(x   )+(y   )*lwidth,
							frame2[0]+(x+sx)+(y+sy)*lwidth,
							lwidth,8 );
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
			for(sy=0;sy<8;sy++)
				{
					for(sx=0;sx<8;sx++)
					{
					*(p0) = *(p1);
					p0++;
					p1++;
					}
				p0 += lwidth-8;
				p1 += lwidth-8;
				}
			}

			// search the best vector for frame1 (arround the match of frame2)
			min = psad_sub22 (	frame3[0]+(x)+(y)*lwidth,
					frame1[0]+(x+bx)+(y+by)*lwidth,
					lwidth,8 )*0.8;
			vx=vy=0;
			for(sy=(by-r);sy<=(by+r);sy++)
				for(sx=(bx-r);sx<=(bx+r);sx++)
				{
				sad  = psad_sub22 (	frame3[0]+(x   )+(y   )*lwidth,
							frame1[0]+(x+sx)+(y+sy)*lwidth,
							lwidth,8 );
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
			for(sy=0;sy<8;sy++)
				{
					for(sx=0;sx<8;sx++)
					{
					*(p0) = *(p1);
					p0++;
					p1++;
					}
				p0 += lwidth-8;
				p1 += lwidth-8;
				}
			}

			// search the best vector for frame4
			min = psad_sub22 (	frame3[0]+(x)+(y)*lwidth,
					frame4[0]+(x)+(y)*lwidth,
					lwidth,8 )*0.8;
			vx=vy=bx=by=0;
			for(sy=-r;sy<=r;sy++)
				for(sx=-r;sx<=r;sx++)
				{
				sad  = psad_sub22 (	frame3[0]+(x   )+(y   )*lwidth,
							frame4[0]+(x+sx)+(y+sy)*lwidth,
							lwidth,8 );
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
			for(sy=0;sy<8;sy++)
				{
					for(sx=0;sx<8;sx++)
					{
					*(p0) = *(p1);
					p0++;
					p1++;
					}
				p0 += lwidth-8;
				p1 += lwidth-8;
				}
			}

			// search the best vector for frame5 (arround the match of frame4)
			min = psad_sub22 (	frame3[0]+(x)+(y)*lwidth,
					frame5[0]+(x+bx)+(y+by)*lwidth,
					lwidth,8 )*0.8;
			vx=vy=0;
			for(sy=(by-r);sy<=(by+r);sy++)
				for(sx=(bx-r);sx<=(bx+r);sx++)
				{
				sad  = psad_sub22 (	frame3[0]+(x   )+(y   )*lwidth,
							frame5[0]+(x+sx)+(y+sy)*lwidth,
							lwidth,8 );
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
			for(sy=0;sy<8;sy++)
				{
					for(sx=0;sx<8;sx++)
					{
					*(p0) = *(p1);
					p0++;
					p1++;
					}
				p0 += lwidth-8;
				p1 += lwidth-8;
				}
			}
	}
	}

	// mix frames
	{
	int x,y;
	int mean;
	int delta;
	int delta_sum;
	uint32_t interpolated_pixel;
	int t=temp_Y_thres;
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
		ref += *(p3)*2;
		ref += *(p3+lwidth);
		ref /= 4;

		interpolated_pixel = ref;
		delta_sum = 1;

		delta = abs( *(p1)-ref );
		delta = (delta<t)? 1:0;
		delta_sum += delta;
		interpolated_pixel += *(p1)*delta;

		delta = abs( *(p2)-ref );
		delta = (delta<t)? 1:0;
		delta_sum += delta;
		interpolated_pixel += *(p2)*delta;

		delta = abs( *(p4)-ref );
		delta = (delta<t)? 1:0;
		delta_sum += delta;
		interpolated_pixel += *(p4)*delta;

		delta = abs( *(p5)-ref );
		delta = (delta<t)? 1:0;
		delta_sum += delta;
		interpolated_pixel += *(p5)*delta;
		
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
	t=temp_U_thres;
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

		interpolated_pixel = ref;
		delta_sum = 1;

		delta = abs( *(p1)-ref );
		delta = (delta<t)? 1:0;
		delta_sum += delta;
		interpolated_pixel += *(p1)*delta;

		delta = abs( *(p2)-ref );
		delta = (delta<t)? 1:0;
		delta_sum += delta;
		interpolated_pixel += *(p2)*delta;

		delta = abs( *(p4)-ref );
		delta = (delta<t)? 1:0;
		delta_sum += delta;
		interpolated_pixel += *(p4)*delta;

		delta = abs( *(p5)-ref );
		delta = (delta<t)? 1:0;
		delta_sum += delta;
		interpolated_pixel += *(p5)*delta;

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
	t=temp_V_thres;
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

		interpolated_pixel = ref;
		delta_sum = 1;

		delta = abs( *(p1)-ref );
		delta = (delta<t)? 1:0;
		delta_sum += delta;
		interpolated_pixel += *(p1)*delta;

		delta = abs( *(p2)-ref );
		delta = (delta<t)? 1:0;
		delta_sum += delta;
		interpolated_pixel += *(p2)*delta;

		delta = abs( *(p4)-ref );
		delta = (delta<t)? 1:0;
		delta_sum += delta;
		interpolated_pixel += *(p4)*delta;

		delta = abs( *(p5)-ref );
		delta = (delta<t)? 1:0;
		delta_sum += delta;
		interpolated_pixel += *(p5)*delta;

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
	
	// lock pixels and do this right (that is do it *without* producing visable artefacts!)
	{
	int x,y;
	int d1,d2,d3,d4,d5,d6,d7;

	for(y=0;y<lheight;y++)
		for(x=0;x<lwidth;x++)
		{
			d1  = abs( *(outframe[0]+x+y*lwidth)-*(pixlock1[0]+x+y*lwidth) );
			d2  = abs( *(outframe[0]+x+y*lwidth)-*(pixlock2[0]+x+y*lwidth) );
			d3  = abs( *(outframe[0]+x+y*lwidth)-*(pixlock3[0]+x+y*lwidth) );
			d4  = abs( *(outframe[0]+x+y*lwidth)-*(pixlock4[0]+x+y*lwidth) );
			d5  = abs( *(outframe[0]+x+y*lwidth)-*(pixlock5[0]+x+y*lwidth) );
			d6  = abs( *(outframe[0]+x+y*lwidth)-*(pixlock6[0]+x+y*lwidth) );
			d7  = abs( *(outframe[0]+x+y*lwidth)-*(pixlock7[0]+x+y*lwidth) );

			if (d1>6 || d2>6 || d3>6 || d4>6 || d5>6 || d6>6 || d7>6)
			{
				*(outframe[0]+x+y*lwidth) = *(pixlock4[0]+x+y*lwidth);
			}
			else
			{
//				if (d1>3 || d2>3 || d3>3 || d4>3 || d5>3 || d6>3 || d7>3)
				{
					*(outframe[0]+x+y*lwidth) = 
						(
						  *(pixlock1[0]+x+y*lwidth)+
						  *(pixlock2[0]+x+y*lwidth)+
						  *(pixlock3[0]+x+y*lwidth)+
						  *(pixlock4[0]+x+y*lwidth)+
						  *(pixlock5[0]+x+y*lwidth)+
						  *(pixlock6[0]+x+y*lwidth)+
						  *(pixlock7[0]+x+y*lwidth)
						)/7;
				}
			}
		}
	for(y=0;y<cheight;y++)
		for(x=0;x<cwidth;x++)
		{
			d1  = abs( *(outframe[1]+x+y*cwidth)-*(pixlock1[1]+x+y*cwidth) );
			d2  = abs( *(outframe[1]+x+y*cwidth)-*(pixlock2[1]+x+y*cwidth) );
			d3  = abs( *(outframe[1]+x+y*cwidth)-*(pixlock3[1]+x+y*cwidth) );
			d4  = abs( *(outframe[1]+x+y*cwidth)-*(pixlock4[1]+x+y*cwidth) );
			d5  = abs( *(outframe[1]+x+y*cwidth)-*(pixlock5[1]+x+y*cwidth) );
			d6  = abs( *(outframe[1]+x+y*cwidth)-*(pixlock6[1]+x+y*cwidth) );
			d7  = abs( *(outframe[1]+x+y*cwidth)-*(pixlock7[1]+x+y*cwidth) );

			if (d1>6 || d2>6 || d3>6 || d4>6 || d5>6 || d6>6 || d7>6)
			{
				*(outframe[1]+x+y*cwidth) = *(pixlock4[1]+x+y*cwidth);
			}
			else
			{
//				if (d1>4 || d2>4 || d3>4 || d4>4 || d5>4 || d6>4 || d7>4)
				{
					*(outframe[1]+x+y*cwidth) = 
						(
						  *(pixlock1[1]+x+y*cwidth)+
						  *(pixlock2[1]+x+y*cwidth)+
						  *(pixlock3[1]+x+y*cwidth)+
						  *(pixlock4[1]+x+y*cwidth)+
						  *(pixlock5[1]+x+y*cwidth)+
						  *(pixlock6[1]+x+y*cwidth)+
						  *(pixlock7[1]+x+y*cwidth)
						)/7;
				}
			}

			d1  = abs( *(outframe[2]+x+y*cwidth)-*(pixlock1[2]+x+y*cwidth) );
			d2  = abs( *(outframe[2]+x+y*cwidth)-*(pixlock2[2]+x+y*cwidth) );
			d3  = abs( *(outframe[2]+x+y*cwidth)-*(pixlock3[2]+x+y*cwidth) );
			d4  = abs( *(outframe[2]+x+y*cwidth)-*(pixlock4[2]+x+y*cwidth) );
			d5  = abs( *(outframe[2]+x+y*cwidth)-*(pixlock5[2]+x+y*cwidth) );
			d6  = abs( *(outframe[2]+x+y*cwidth)-*(pixlock6[2]+x+y*cwidth) );
			d7  = abs( *(outframe[2]+x+y*cwidth)-*(pixlock7[2]+x+y*cwidth) );

			if (d1>6 || d2>6 || d3>6 || d4>6 || d5>6 || d6>6 || d7>6)
			{
				*(outframe[2]+x+y*cwidth) = *(pixlock4[2]+x+y*cwidth);
			}
			else
			{
//				if (d1>4 || d2>4 || d3>4 || d4>4 || d5>4 || d6>4 || d7>4)
				{
					*(outframe[2]+x+y*cwidth) = 
						(
						  *(pixlock1[2]+x+y*cwidth)+
						  *(pixlock2[2]+x+y*cwidth)+
						  *(pixlock3[2]+x+y*cwidth)+
						  *(pixlock4[2]+x+y*cwidth)+
						  *(pixlock5[2]+x+y*cwidth)+
						  *(pixlock6[2]+x+y*cwidth)+
						  *(pixlock7[2]+x+y*cwidth)
						)/7;
				}
			}
		}
	}

	// increase frame-counter
	frame_nr++;
	if(frame_nr>=7)
		y4m_write_frame (fd_out, &ostreaminfo, &oframeinfo, outframe);

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

	// do the same for the pixlock-lookahead-buffers
	temp[0] = pixlock7[0];
	temp[1] = pixlock7[1];
	temp[2] = pixlock7[2];

	pixlock7[0] = pixlock6[0];
	pixlock7[1] = pixlock6[1];
	pixlock7[2] = pixlock6[2];

	pixlock6[0] = pixlock5[0];
	pixlock6[1] = pixlock5[1];
	pixlock6[2] = pixlock5[2];

	pixlock5[0] = pixlock4[0];
	pixlock5[1] = pixlock4[1];
	pixlock5[2] = pixlock4[2];

	pixlock4[0] = pixlock3[0];
	pixlock4[1] = pixlock3[1];
	pixlock4[2] = pixlock3[2];

	pixlock3[0] = pixlock2[0];
	pixlock3[1] = pixlock2[1];
	pixlock3[2] = pixlock2[2];

	pixlock2[0] = pixlock1[0];
	pixlock2[1] = pixlock1[1];
	pixlock2[2] = pixlock1[2];

	pixlock1[0] = temp[0];
	pixlock1[1] = temp[1];
	pixlock1[2] = temp[2];

	// and fill in the reconstructed image in mc3[x]
	memcpy(pixlock1[0],mc3[0],lwidth*lheight);
	memcpy(pixlock1[1],mc3[1],cwidth*cheight);
	memcpy(pixlock1[2],mc3[2],cwidth*cheight);
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

    free (pixlock1[0] - buff_offset);
    free (pixlock1[1] - buff_offset);
    free (pixlock1[2] - buff_offset);

    free (pixlock2[0] - buff_offset);
    free (pixlock2[1] - buff_offset);
    free (pixlock2[2] - buff_offset);

    free (pixlock3[0] - buff_offset);
    free (pixlock3[1] - buff_offset);
    free (pixlock3[2] - buff_offset);

    free (pixlock4[0] - buff_offset);
    free (pixlock4[1] - buff_offset);
    free (pixlock4[2] - buff_offset);

    free (pixlock5[0] - buff_offset);
    free (pixlock5[1] - buff_offset);
    free (pixlock5[2] - buff_offset);

    free (pixlock6[0] - buff_offset);
    free (pixlock6[1] - buff_offset);
    free (pixlock6[2] - buff_offset);

    free (pixlock7[0] - buff_offset);
    free (pixlock7[1] - buff_offset);
    free (pixlock7[2] - buff_offset);

    free (outframe[0] - buff_offset);
    free (outframe[1] - buff_offset);
    free (outframe[2] - buff_offset);

    mjpeg_log (LOG_INFO, "Buffers freed.");
  }

  /* did stream end unexpectedly ? */
  if (errno != Y4M_ERR_EOF)
    mjpeg_error_exit1 ("%s", y4m_strerr (errno));

  /* Exit gently */
  return (0);
}
