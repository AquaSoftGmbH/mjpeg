/*
 * y4mspatialfilter.c
 *
 * written by Dan Scholnik <scholnik@ieee.org>
 * spatial FIR filter for noise/bandwidth reduction without scaling
 * takes yuv4mpeg in and spits the same out
 *
 * Usage: y4mspatialfilter [-h] [-v] [-L luma_Xtaps,luma_XBW,luma_Ytaps,luma_YBW] 
 *                                   [-C chroma_Xtaps,chroma_XBW,chroma_Ytaps,chroma_YBW]
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "yuv4mpeg.h"

#define MIN(a,b) ((a)<(b))?(a):(b)
#define MAX(a,b) ((a)>(b))?(a):(b)

void *my_malloc(size_t);
void get_coeff(float **, int, float);
void convolve1D(u_char [], int, int, float **, int, u_char []);
static void usage(void);

int main(int argc, char **argv)
{
    int    i, n, c, interlace, frames, err;
    int    ywidth, yheight, uvwidth, uvheight, ylen, uvlen;
    int    verbose = 0, fdin;
    int    SS_H = 2, SS_V = 2;
    int    NlumaX = 4, NlumaY = 4, NchromaX = 4, NchromaY = 4;
    float  BWlumaX = 0.75, BWlumaY = 0.75, BWchromaX = 0.6, BWchromaY = 0.6;
    float  **lumaXtaps, **lumaYtaps, **chromaXtaps, **chromaYtaps;
    u_char *yuvinout[3], *yuvtmp[3];
    y4m_stream_info_t istream, ostream;
    y4m_frame_info_t iframe;

    fdin = fileno(stdin);
    
    /* read command line */
    opterr = 0;
    while   ((c = getopt(argc, argv, "hvL:C:")) != EOF)
	{
	    switch  (c)
		{
		case    'L':
		    sscanf(optarg,"%d,%f,%d,%f",&NlumaX,&BWlumaX,&NlumaY,&BWlumaY);
		    break;
		case    'C':
		    sscanf(optarg,"%d,%f,%d,%f",&NchromaX,&BWchromaX,&NchromaY,&BWchromaY);
		    break;
		case    'v':
		    verbose++;
		    break;
		case    '?':
		case    'h':
		default:
		    usage();
		}
	}
    
    /* initialize input stream and check chroma subsampling and interlacing */
    y4m_init_stream_info(&istream);
    y4m_init_frame_info(&iframe);
    err = y4m_read_stream_header(fdin, &istream);
    if (err != Y4M_OK)
	mjpeg_error_exit1("Input stream error: %s\n", y4m_strerr(err));
    else
	{
	    /* try to find a chromass xtag... */
	    y4m_xtag_list_t *xtags = y4m_si_xtags(&istream);
	    const char *tag = NULL;
	    int n;
	    
	    for	(n = y4m_xtag_count(xtags) - 1; n >= 0; n--) 
		{
		    tag = y4m_xtag_get(xtags, n);
		    if (!strncmp("XYSCSS=", tag, 7)) break;
		}
	    if	((tag != NULL) && (n >= 0))
		{
		    /* parse the tag */
		    tag += 7;
		    if	(!strcmp("411", tag))
			{
			    SS_H = 4;
			    SS_V = 1;
			} 
		    else if (!strcmp(tag, "420") || 
			     !strcmp(tag, "420MPEG2") || 
			     !strcmp(tag, "420PALDV") || 
			     !strcmp(tag,"420JPEG"))
			{
			    SS_H = 2;
			    SS_V = 2;
			} 
		}
	}
    i = y4m_si_get_interlace(&istream);
    switch (i)
        {
        case Y4M_ILACE_NONE:
	    interlace = 0;
	    break;
        case Y4M_ILACE_BOTTOM_FIRST:
        case Y4M_ILACE_TOP_FIRST:
	    interlace = 1;
	    break;
        default:
	    mjpeg_warn("Unknown interlacing '%d', assuming non-interlaced", i);
	    interlace = 0;
	    break;
        }
    ywidth = y4m_si_get_width(&istream);
    yheight = y4m_si_get_height(&istream);
    ylen = ywidth * yheight;
    uvwidth = ywidth/SS_H;
    uvheight = yheight/SS_V;
    uvlen = uvwidth * uvheight;
    
    /* initialize output stream */
    y4m_init_stream_info(&ostream);
    y4m_copy_stream_info(&ostream, &istream);
    y4m_write_stream_header(fileno(stdout), &ostream);
    
    /* allocate input and output buffers */
    yuvinout[0] = my_malloc(ylen*sizeof(u_char));
    yuvinout[1] = my_malloc(uvlen*sizeof(u_char));
    yuvinout[2] = my_malloc(uvlen*sizeof(u_char));
    yuvtmp[0] = my_malloc(ylen*sizeof(u_char));
    yuvtmp[1] = my_malloc(uvlen*sizeof(u_char));
    yuvtmp[2] = my_malloc(uvlen*sizeof(u_char));

    /* allocate filters */
    lumaXtaps = my_malloc((NlumaX+1)*sizeof(float *));
    for(n=0;n<=NlumaX;n++)
	lumaXtaps[n]=my_malloc((NlumaX+1)*sizeof(float));
    lumaYtaps = my_malloc((NlumaY+1)*sizeof(float *));
    for(n=0;n<=NlumaY;n++)
	lumaYtaps[n]=my_malloc((NlumaY+1)*sizeof(float));
    chromaXtaps = my_malloc((NchromaX+1)*sizeof(float *));
    for(n=0;n<=NchromaX;n++)
	chromaXtaps[n]=my_malloc((NchromaX+1)*sizeof(float));
    chromaYtaps = my_malloc((NchromaY+1)*sizeof(float *));
    for(n=0;n<=NchromaY;n++)
	chromaYtaps[n]=my_malloc((NchromaY+1)*sizeof(float));
    /* get filter taps */
    get_coeff(lumaXtaps, NlumaX, BWlumaX);
    get_coeff(lumaYtaps, NlumaY, BWlumaY);
    get_coeff(chromaXtaps, NchromaX, BWchromaX);
    get_coeff(chromaYtaps, NchromaY, BWchromaY);

    if (verbose)
	mjpeg_log(LOG_INFO, "W %d H %d R %d/%d Fsize %d\n", 
		ywidth, yheight, istream.framerate.n,
		istream.framerate.d, istream.framelength);
    
    /* main processing loop */
    for (frames=0; y4m_read_frame(fdin,&istream,&iframe,yuvinout) == Y4M_OK; frames++)
	{
	    if (verbose && ((frames % 100) == 0))
		mjpeg_log(LOG_INFO, "Frame %d\n", frames);
	    
	    /* do the horizontal filtering */
	    for(n=0;n<yheight;n++)
		convolve1D(yuvinout[0]+n*ywidth, ywidth, 1, lumaXtaps, NlumaX, yuvtmp[0]+n*ywidth);
	    for(n=0;n<uvheight;n++)
		{
		    convolve1D(yuvinout[1]+n*uvwidth, uvwidth, 1, chromaXtaps, NchromaX, yuvtmp[1]+n*uvwidth);
		    convolve1D(yuvinout[2]+n*uvwidth, uvwidth, 1, chromaXtaps, NchromaX, yuvtmp[2]+n*uvwidth);
		}
		
	    /* do the vertical filtering */
	    if(interlace)
		{
		    for(n=0;n<ywidth;n++)
			{
			    convolve1D(yuvtmp[0]+n, yheight/2, 2*ywidth, lumaYtaps, NlumaY, yuvinout[0]+n);
			    convolve1D(yuvtmp[0]+ywidth+n, yheight/2, 2*ywidth, lumaYtaps, NlumaY, yuvinout[0]+ywidth+n);
			}
		    for(n=0;n<uvwidth;n++)
			{
			    convolve1D(yuvtmp[1]+n, uvheight/2, 2*uvwidth, chromaYtaps, NchromaY, yuvinout[1]+n);
			    convolve1D(yuvtmp[1]+uvwidth+n, uvheight/2, 2*uvwidth, chromaYtaps, NchromaY, yuvinout[1]+uvwidth+n);
			    convolve1D(yuvtmp[2]+n, uvheight/2, 2*uvwidth, chromaYtaps, NchromaY, yuvinout[2]+n);
			    convolve1D(yuvtmp[2]+uvwidth+n, uvheight/2, 2*uvwidth, chromaYtaps, NchromaY, yuvinout[2]+uvwidth+n);
			}
		}
	    else
		{
		    for(n=0;n<ywidth;n++)
			convolve1D(yuvtmp[0]+n, yheight, ywidth, lumaYtaps, NlumaY, yuvinout[0]+n);
		    for(n=0;n<uvwidth;n++)
			{
			    convolve1D(yuvtmp[1]+n, uvheight, uvwidth, chromaYtaps, NchromaY, yuvinout[1]+n);
			    convolve1D(yuvtmp[2]+n, uvheight, uvwidth, chromaYtaps, NchromaY, yuvinout[2]+n);
			}
		}
	    
	    y4m_write_frame(fileno(stdout), &ostream, &iframe, yuvinout);

	}
    
    /* clean up */
    y4m_fini_frame_info(&iframe);
    y4m_fini_stream_info(&istream);
    y4m_fini_stream_info(&ostream);
    
    exit(0);
}


/* Move memory allocation error checking here to clean up code */
void *my_malloc(size_t size)
{
    void *tmp = malloc(size);
    if (tmp == NULL)
	    mjpeg_error_exit1("malloc(%d) failed\n", size);
    return tmp;
}


/* Compute 1D filter coefficients (center and right-side taps only) */
/* To minimize artifacts at the boundaries, compute filters of all sizes */
/* from 1 to length and use shorter filters near the edge of the frame */
/* Also, normalize to a DC gain of 1 */
void get_coeff(float **taps, int length, float bandwidth)
{
    int n,k;
    float sum;
    /* C*sinc(C*n).*sinc(2*n/N); Lanczos-weighted */    
    for(k=0;k<=length;k++)
	{
	    taps[k][0]=bandwidth;
	    sum=taps[k][0];
	    for(n=1;n<=k;n++)
		{
		    taps[k][n] = bandwidth * sin(M_PI*bandwidth*n)/(M_PI*bandwidth*n)
			                   * sin(M_PI*2*n/(2*k+1))/(M_PI*2.0*n/(2*k+1));
		    sum+=2*taps[k][n];
		}
	    for(n=0;n<=k;n++)
		taps[k][n]/=sum;
	}
}


/* Routine to perform a 1-dimensional convolution with the result 
   symmetrically truncated to match the input length.  
   Filter is odd and linear phase, only center and right-side taps are specified */
void convolve1D(u_char data[], int datalength, int datastride, float **filter, int filterlength, u_char output[])
{

    int   n, k;
    float tempout;

    /* leading edge, use filters of increasing width */
    for(n=0;n<filterlength;n++)
	{
	    tempout=filter[n][0]*data[n*datastride];
	    for(k=1; k<=n; k++)
		tempout+=filter[n][k]*(data[(n-k)*datastride]+data[(n+k)*datastride]);
	    /* uncomment if using asymmetric filters at the edges */
	    /*	    for(k=n+1; k<=filterlength; k++)
		    tempout+=filter[n][k]*data[(n+k)*datastride]; */
	    /* clip and cast to integer */
	    output[n*datastride]=MIN(MAX(tempout,0),255);
	}
    /* center, use full-width filter */
    for(n=filterlength; n<datalength-filterlength; n++)
	{
	    tempout=filter[filterlength][0]*data[n*datastride];
	    for(k=1; k<=filterlength; k++)
		tempout+=filter[filterlength][k]*(data[(n-k)*datastride]+data[(n+k)*datastride]);
	    /* clip and cast to integer */
	    output[n*datastride]=MIN(MAX(tempout,0),255);
	}
    /* trailing edge, use filters of decreasing width */
    for(n=datalength-filterlength;n<datalength;n++)
	{
	    tempout=filter[datalength-n-1][0]*data[n*datastride];
	    for(k=1; k<datalength-n; k++)
		tempout+=filter[datalength-n-1][k]*(data[(n-k)*datastride]+data[(n+k)*datastride]);
	    /* uncomment if using asymmetric filters at the edges */
	    /*	    for(k=datalength-n; k<=filterlength; k++)
		    tempout+=filter[datalength-n-1][k]*data[(n-k)*datastride]; */
	    /* clip and cast to integer */
	    output[n*datastride]=MIN(MAX(tempout,0),255);
	}
}


static void usage(void)
{
    fprintf(stderr, "usage: y4mspatialfilter [-h] [-v] [-L lumaXtaps,lumaXBW,lumaYtaps,lumaYBW] \n");
    fprintf(stderr, "                                  [-C chromaXtaps,chromaXBW,chromaYtaps,chromaYBW]\n");
    fprintf(stderr, "\t-v be somewhat verbose\n");
    fprintf(stderr, "\t-h print this usage summary\n");
    fprintf(stderr, "\tlumaXtaps: length of horizontal luma filter (0 to disable)\n");
    fprintf(stderr, "\tlumaXBW: fractional bandwidth of horizontal luma filter [0-1.0]\n");
    fprintf(stderr, "\tlumaYtaps: length of vertical luma filter (0 to disable)\n");
    fprintf(stderr, "\tlumaYBW: fractional bandwidth of vertical luma filter [0-1.0]\n");
    fprintf(stderr, "\tchromaXtaps: length of horizontal chroma filter (0 to disable)\n");
    fprintf(stderr, "\tchromaXBW: fractional bandwidth of horizontal chroma filter [0-1.0]\n");
    fprintf(stderr, "\tchromaYtaps: length of vertical chroma filter (0 to disable)\n");
    fprintf(stderr, "\tchromaYBW: fractional bandwidth of vertical chroma filter [0-1.0]\n");
    exit(1);
}
