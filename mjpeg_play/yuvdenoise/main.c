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

int spat_Y_thres = 5;
int spat_U_thres = 10;
int spat_V_thres = 10;

int temp_Y_thres = 10;
int temp_U_thres = 20;
int temp_V_thres = 20;

uint8_t *inframe[3];
uint8_t *frame1[3];
uint8_t *frame2[3];
uint8_t *frame3[3];
uint8_t *frame4[3];
uint8_t *frame5[3];
uint8_t *frame6[3];
uint8_t *frame7[3];
uint8_t *pixellocked1[3];
uint8_t *pixellocked2[3];
uint8_t *reconstructed[3];

int buff_offset;
int buff_size;

/***********************************************************
 * helper-functions                                        *
 ***********************************************************/
void
transform_block (uint8_t * a1, uint8_t * a2, int rowstride)
{
  int x, y;

  for (y = 0; y < 8; y++)
    {
      for (x = 0; x < 8; x++)
	{
	  *(a1) = *(a2);
	  a1++;
	  a2++;
	}
      a1 += rowstride - 8;
      a2 += rowstride - 8;
    }
}

void spatial_filter_frame ( uint8_t * dst, uint8_t * src, int w, int h, int t )
{
    int j;
    int r;
    int m;
    int d, da;
    uint8_t * s0 = src;
    uint8_t * s1 = src-2-2*w;
    uint8_t * s2 = src-1-w;

    for(j=0;j<(w*h);j++)
        {
        // calculate our (lowpass-filtered) reference pixel
        s2 = s0-1-w;
        r  = *(s2);
        s2++;
        r += *(s2);
        s2++;
        r += *(s2);
        s2+=w-2;
        
        r += *(s2);
        s2++;
        r += *(s2)*8;
        s2++;
        r += *(s2);
        s2+=w-2;
        
        r += *(s2);
        s2++;
        r += *(s2);
        s2++;
        r += *(s2);      
        r >>= 4;
        
        // first pixel row
        d   = r - *(s1);
        d   = abs(d);
        d   = (d<t)? (t-d):(0);
        m   = *(s1) * d;
        da  = d;
        s1++;
        
        d   = r - *(s1);
        d   = abs(d);
        d   = (d<t)? (t-d):(0);
        m  += *(s1) * d;
        da += d;
        s1++;
        
        d   = r - *(s1);
        d   = abs(d);
        d   = (d<t)? (t-d):(0);
        m  += *(s1) * d;
        da += d;
        s1++;
        
        d   = r - *(s1);
        d   = abs(d);
        d   = (d<t)? (t-d):(0);
        m  += *(s1) * d;
        da += d;
        s1++;
        
        d   = r - *(s1);
        d   = abs(d);
        d   = (d<t)? (t-d):(0);
        m  += *(s1) * d;
        da += d;
        s1++;
        
        // second pixel row
        s1 += w-5; // set s1 to the first pixel of the second row
        
        d   = r - *(s1);
        d   = abs(d);
        d   = (d<t)? (t-d):(0);
        m  += *(s1) * d;
        da += d;
        s1++;
        
        d   = r - *(s1);
        d   = abs(d);
        d   = (d<t)? (t-d):(0);
        m  += *(s1) * d;
        da += d;
        s1++;
        
        d   = r - *(s1);
        d   = abs(d);
        d   = (d<t)? (t-d):(0);
        m  += *(s1) * d;
        da += d;
        s1++;
        
        d   = r - *(s1);
        d   = abs(d);
        d   = (d<t)? (t-d):(0);
        m  += *(s1) * d;
        da += d;
        s1++;
        
        d   = r - *(s1);
        d   = abs(d);
        d   = (d<t)? (t-d):(0);
        m  += *(s1) * d;
        da += d;
        s1++;
       
        // third pixel row
        s1 += w-5; // set s1 to the first pixel of the third row
        
        d   = r - *(s1);
        d   = abs(d);
        d   = (d<t)? (t-d):(0);
        m  += *(s1) * d;
        da += d;
        s1++;
        
        d   = r - *(s1);
        d   = abs(d);
        d   = (d<t)? (t-d):(0);
        m  += *(s1) * d;
        da += d;
        s1++;
        
        #if 0
        d   = r - *(s1);
        d   = abs(d);
        d   = (d<t)? (t-d):(0);
        m  += *(s1) * d;
        da += d;
        #else
        m  += *(s1) * t;
        da += t;
        #endif
        s1++;
        
        d   = r - *(s1);
        d   = abs(d);
        d   = (d<t)? (t-d):(0);
        m  += *(s1) * d;
        da += d;
        s1++;
        
        d   = r - *(s1);
        d   = abs(d);
        d   = (d<t)? (t-d):(0);
        m  += *(s1) * d;
        da += d;
        s1++;
       
        // fourth pixel row
        s1 += w-5; // set s1 to the first pixel of the fourth row
        
        d   = r - *(s1);
        d   = abs(d);
        d   = (d<t)? (t-d):(0);
        m  += *(s1) * d;
        da += d;
        s1++;
        
        d   = r - *(s1);
        d   = abs(d);
        d   = (d<t)? (t-d):(0);
        m  += *(s1) * d;
        da += d;
        s1++;
        
        d   = r - *(s1);
        d   = abs(d);
        d   = (d<t)? (t-d):(0);
        m  += *(s1) * d;
        da += d;
        s1++;
        
        d   = r - *(s1);
        d   = abs(d);
        d   = (d<t)? (t-d):(0);
        m  += *(s1) * d;
        da += d;
        s1++;
        
        d   = r - *(s1);
        d   = abs(d);
        d   = (d<t)? (t-d):(0);
        m  += *(s1) * d;
        da += d;
        s1++;
       
        // fifth pixel row
        s1 += w-5; // set s1 to the first pixel of the fifth row
        
        d   = r - *(s1);
        d   = abs(d);
        d   = (d<t)? (t-d):(0);
        m  += *(s1) * d;
        da += d;
        s1++;
        
        d   = r - *(s1);
        d   = abs(d);
        d   = (d<t)? (t-d):(0);
        m  += *(s1) * d;
        da += d;
        s1++;
        
        d   = r - *(s1);
        d   = abs(d);
        d   = (d<t)? (t-d):(0);
        m  += *(s1) * d;
        da += d;
        s1++;
        
        d   = r - *(s1);
        d   = abs(d);
        d   = (d<t)? (t-d):(0);
        m  += *(s1) * d;
        da += d;
        s1++;
        
        d   = r - *(s1);
        d   = abs(d);
        d   = (d<t)? (t-d):(0);
        m  += *(s1) * d;
        da += d;
        s1++;
       
        // finally divide sum of wightened pixels by sum of wights
        // fprintf(stderr,"da:%i\n",da);
        m /= da;
        // and write the destination pixel
        *(dst) = m;

        // go one pixel further
        dst++;
        s0 ++;
        s1 = s0-2-2*w;
        }
}

void temporal_filter_frame 
(
uint8_t * dst,
uint8_t * s1,
uint8_t * s2,
uint8_t * s3,
uint8_t * s4,
uint8_t * s5,
uint8_t * s6,
uint8_t * s7,
int w,
int h,
int t
)
{
int j;
int r;
int m;
int l1,l2,l3;
int d,da;

    for(j=0;j<(w*h);j++)
        {
        r  = s4[j-w-1]; 
        r += s4[j-w]; 
        r += s4[j-w+1]; 
        r += s4[j-1]; 
        r += 8*s4[j]; 
        r += s4[j+1]; 
        r += s4[j+w-1]; 
        r += s4[j+w]; 
        r += s4[j+w+1]; 
        r >>= 4;
        
        m  = t * *(s4+j);
        da = t;
        
        // linear interpolations for blends
        l1  = ( *(s1+j)+*(s2+j)+*(s3+j)+*(s4+j)+*(s5+j)+*(s6+j)+*(s7+j) )/7;
        d   = abs(l1-r);
        d   = d<t? t-d:0;
        m  += d*l1;
        da += d;
        
        l2 = ( *(s2+j)+*(s3+j)+*(s4+j)+*(s5+j)+*(s6+j) )/5;
        d   = abs(l2-r);
        d   = d<t? t-d:0;
        m  += d*l2;
        da += d;
        
        l3 = ( *(s3+j)+*(s4+j)+*(s5+j) )/3;
        d   = abs(l3-r);
        d   = d<t? t-d:0;
        m  += d*l3;
        da += d;
        
        // single pixel checks
        d   = abs(s1[j]-r);
        d   = d<t? t-d:0;
        m  += d*s1[j];
        da += d;

        d   = abs(s2[j]-r);
        d   = d<t? t-d:0;
        m  += d*s2[j];
        da += d;

        d   = abs(s3[j]-r);
        d   = d<t? t-d:0;
        m  += d*s3[j];
        da += d;

        d   = abs(s5[j]-r);
        d   = d<t? t-d:0;
        m  += d*s5[j];
        da += d;

        d   = abs(s6[j]-r);
        d   = d<t? t-d:0;
        m  += d*s6[j];
        da += d;

        d   = abs(s7[j]-r);
        d   = d<t? t-d:0;
        m  += d*s7[j];
        da += d;
        
        m /= da;
        dst[j]=m;
        }
}

void pixellock_frame 
(
uint8_t * dst,
uint8_t * ref1,
uint8_t * ref2,
int w,
int h,
int t
)
{
int j;
int d1,d2;
static int cnt=0;

cnt++;

for(j=0;j<(w*h);j++)
    {
    d1  = *(dst+j)-*(ref1+j);
    d1 += *(dst+j+w)-*(ref1+j+w);
    d1 += *(dst+j-w)-*(ref1+j-w);
    d1 = d1<0? -d1:d1;
    
    d2  = *(dst+j)-*(ref2+j);
    d2 += *(dst+j+w)-*(ref2+j+w);
    d2 += *(dst+j-w)-*(ref2+j-w);
    d2 = d2<0? -d2:d2;
    
    
    if(d1>t || d2>t || (cnt&31)==0 )
        *(dst+j)=*(ref1+j);
    else
        if(d1>(t/2) || d2>(t/2) || (cnt&7)==0 )
            *(dst+j)=(*(dst+j)+*(ref1+j))/2;
    }
}

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

    inframe[0] = buff_offset + (uint8_t *) malloc (buff_size);
    inframe[1] = buff_offset + (uint8_t *) malloc (buff_size);
    inframe[2] = buff_offset + (uint8_t *) malloc (buff_size);

    reconstructed[0] = buff_offset + (uint8_t *) malloc (buff_size);
    reconstructed[1] = buff_offset + (uint8_t *) malloc (buff_size);
    reconstructed[2] = buff_offset + (uint8_t *) malloc (buff_size);

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

    frame6[0] = buff_offset + (uint8_t *) malloc (buff_size);
    frame6[1] = buff_offset + (uint8_t *) malloc (buff_size);
    frame6[2] = buff_offset + (uint8_t *) malloc (buff_size);
    
    frame7[0] = buff_offset + (uint8_t *) malloc (buff_size);
    frame7[1] = buff_offset + (uint8_t *) malloc (buff_size);
    frame7[2] = buff_offset + (uint8_t *) malloc (buff_size);

    pixellocked1[0] = buff_offset + (uint8_t *) malloc (buff_size);
    pixellocked1[1] = buff_offset + (uint8_t *) malloc (buff_size);
    pixellocked1[2] = buff_offset + (uint8_t *) malloc (buff_size);

    pixellocked2[0] = buff_offset + (uint8_t *) malloc (buff_size);
    pixellocked2[1] = buff_offset + (uint8_t *) malloc (buff_size);
    pixellocked2[2] = buff_offset + (uint8_t *) malloc (buff_size);

    
  mjpeg_log (LOG_INFO, "Buffers allocated.");
  }

  /* read every frame until the end of the input stream and process it */
  while (Y4M_OK == (errno = y4m_read_frame (fd_in,
					    &istreaminfo,
					    &iframeinfo, inframe)))
    {
    static uint32_t fcount=0;
    uint8_t * tmpptr[3];
    
    // fill the first chroma lines above and below the image to get rid of
    // green artifacts...
    memset( inframe[1]-cwidth*2,128,cwidth*2 );
    memset( inframe[2]-cwidth*2,128,cwidth*2 );
    memset( inframe[1]+cwidth*cheight,128,cwidth*2 );
    memset( inframe[2]+cwidth*cheight,128,cwidth*2 );

    // pointer rotation to rotate buffers
    tmpptr[0]        = pixellocked1[0];
    pixellocked1[0]  = reconstructed[0];
    reconstructed[0] = tmpptr[0];
    tmpptr[1]        = pixellocked1[1];
    pixellocked1[1]  = reconstructed[1];
    reconstructed[1] = tmpptr[1];
    tmpptr[2]        = pixellocked1[2];
    pixellocked1[2]  = reconstructed[2];
    reconstructed[2] = tmpptr[2];

    tmpptr[0] = frame7[0];
    frame7[0] = frame6[0];
    frame6[0] = frame5[0];
    frame5[0] = frame4[0];
    frame4[0] = frame3[0];
    frame3[0] = frame2[0];
    frame2[0] = frame1[0];
    frame1[0] = tmpptr[0];

    tmpptr[1] = frame7[1];
    frame7[1] = frame6[1];
    frame6[1] = frame5[1];
    frame5[1] = frame4[1];
    frame4[1] = frame3[1];
    frame3[1] = frame2[1];
    frame2[1] = frame1[1];
    frame1[1] = tmpptr[1];

    tmpptr[2] = frame7[2];
    frame7[2] = frame6[2];
    frame6[2] = frame5[2];
    frame5[2] = frame4[2];
    frame4[2] = frame3[2];
    frame3[2] = frame2[2];
    frame2[2] = frame1[2];
    frame1[2] = tmpptr[2];

    spatial_filter_frame ( frame1[0],inframe[0],lwidth,lheight,spat_Y_thres);
    spatial_filter_frame ( frame1[1],inframe[1],cwidth,cheight,spat_U_thres);
    spatial_filter_frame ( frame1[2],inframe[2],cwidth,cheight,spat_V_thres);
    
    temporal_filter_frame ( reconstructed[0],
                            frame1[0],
                            frame2[0],
                            frame3[0],
                            frame4[0],
                            frame5[0],
                            frame6[0],
                            frame7[0],
                            lwidth,
                            lheight,
                            temp_Y_thres);
    temporal_filter_frame ( reconstructed[1],
                            frame1[1],
                            frame2[1],
                            frame3[1],
                            frame4[1],
                            frame5[1],
                            frame6[1],
                            frame7[1],
                            cwidth,
                            cheight,
                            temp_U_thres);
    temporal_filter_frame ( reconstructed[2],
                            frame1[2],
                            frame2[2],
                            frame3[2],
                            frame4[2],
                            frame5[2],
                            frame6[2],
                            frame7[2],
                            cwidth,
                            cheight,
                            temp_V_thres);
                            
    pixellock_frame ( pixellocked2[0],pixellocked1[0],reconstructed[0],lwidth,lheight,4 );
    pixellock_frame ( pixellocked2[1],pixellocked1[1],reconstructed[1],cwidth,cheight,4 );
    pixellock_frame ( pixellocked2[2],pixellocked1[2],reconstructed[2],cwidth,cheight,4 );

    if(fcount>=4)
        {
            y4m_write_frame (fd_out, &ostreaminfo, &oframeinfo, pixellocked2 );
            mjpeg_log (LOG_INFO,"Frame : %08d",fcount-4);
        }
    fcount++;
    }
  // write the missing frames
  y4m_write_frame (fd_out, &ostreaminfo, &oframeinfo, reconstructed );
  y4m_write_frame (fd_out, &ostreaminfo, &oframeinfo, frame3 );
  y4m_write_frame (fd_out, &ostreaminfo, &oframeinfo, frame2 );
  y4m_write_frame (fd_out, &ostreaminfo, &oframeinfo, frame1 );

  /* free allocated buffers */
  {
    free (inframe[0] - buff_offset);
    free (inframe[1] - buff_offset);
    free (inframe[2] - buff_offset);

    free (reconstructed[0] - buff_offset);
    free (reconstructed[1] - buff_offset);
    free (reconstructed[2] - buff_offset);

    free (frame1[0] - buff_offset);
    free (frame1[1] - buff_offset);
    free (frame1[2] - buff_offset);

    free (frame2[0] - buff_offset);
    free (frame2[1] - buff_offset);
    free (frame2[2] - buff_offset);

    free (frame3[0] - buff_offset);
    free (frame3[1] - buff_offset);
    free (frame3[2] - buff_offset);

    free (pixellocked1[0] - buff_offset);
    free (pixellocked1[1] - buff_offset);
    free (pixellocked1[2] - buff_offset);

    free (pixellocked2[0] - buff_offset);
    free (pixellocked2[1] - buff_offset);
    free (pixellocked2[2] - buff_offset);

    free (frame4[0] - buff_offset);
    free (frame4[1] - buff_offset);
    free (frame4[2] - buff_offset);

    free (frame5[0] - buff_offset);
    free (frame5[1] - buff_offset);
    free (frame5[2] - buff_offset);

    free (frame6[0] - buff_offset);
    free (frame6[1] - buff_offset);
    free (frame6[2] - buff_offset);

    free (frame7[0] - buff_offset);
    free (frame7[1] - buff_offset);
    free (frame7[2] - buff_offset);
    
    mjpeg_log (LOG_INFO, "Buffers freed.");
  }

  /* did stream end unexpectedly ? */
  if (errno != Y4M_ERR_EOF)
    mjpeg_error_exit1 ("%s", y4m_strerr (errno));

  /* Exit gently */
  return (0);
}
