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

extern  char    *__progname;

#define MIN(a,b) ((a)<(b))?(a):(b)
#define MAX(a,b) ((a)>(b))?(a):(b)

static void *my_malloc(size_t);
static void get_coeff(float **, int, float);
static void convolveFrame(u_char *src,u_char *tmp,int w,int h,int interlace,float **xtap,int numx,float **ytap,int numy);
static void usage(void);

int main(int argc, char **argv)
{
    int    i, n, c, interlace, frames, err;
    int    ywidth, yheight, uvwidth, uvheight, ylen, uvlen;
    int    verbose = 0, fdin;
    int    NlumaX = 4, NlumaY = 4, NchromaX = 4, NchromaY = 4;
    float  BWlumaX = 0.75, BWlumaY = 0.75, BWchromaX = 0.6, BWchromaY = 0.6;
    float  **lumaXtaps, **lumaYtaps, **chromaXtaps, **chromaYtaps;
    u_char *yuvinout[3], *yuvtmp;
    y4m_stream_info_t istream, ostream;
    y4m_frame_info_t iframe;

    fdin = fileno(stdin);
    
    y4m_accept_extensions(1);

    /* read command line */
    opterr = 0;
    while   ((c = getopt(argc, argv, "hvL:C:x:X:y:Y:")) != EOF)
	{
	    switch  (c)
		{
		case    'L':
		    sscanf(optarg,"%d,%f,%d,%f",&NlumaX,&BWlumaX,&NlumaY,&BWlumaY);
		    break;
		case    'C':
		    sscanf(optarg,"%d,%f,%d,%f",&NchromaX,&BWchromaX,&NchromaY,&BWchromaY);
		    break;
		case    'x':
		    sscanf(optarg,"%d,%f",&NchromaX,&BWchromaX);
		    break;
		case    'X':
		    sscanf(optarg,"%d,%f",&NlumaX,&BWlumaX);
		    break;
		case    'y':
		    sscanf(optarg,"%d,%f",&NchromaY,&BWchromaY);
		    break;
		case    'Y':
		    sscanf(optarg,"%d,%f",&NlumaY,&BWlumaY);
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
    
    if (BWlumaX <= 0.0 || BWlumaX > 1.0)
       mjpeg_error_exit1("Horizontal luma bandwidth '%f' not >0 and <=1.0", BWlumaX);
    if (BWlumaY <= 0.0 || BWlumaY > 1.0)
       mjpeg_error_exit1("Vertical luma bandwidth '%f' not >0 and <=1.0", BWlumaY);
    if (BWchromaX <= 0.0 || BWchromaX > 1.0)
       mjpeg_error_exit1("Horizontal chroma bandwidth '%f' not >0 and <=1.0", BWchromaX);
    if (BWchromaY <= 0.0 || BWchromaY > 1.0)
       mjpeg_error_exit1("Vertical chroma bandwidth '%f' not >0 and <=1.0", BWchromaY);

    /* initialize input stream and check chroma subsampling and interlacing */
    y4m_init_stream_info(&istream);
    y4m_init_frame_info(&iframe);
    err = y4m_read_stream_header(fdin, &istream);
    if (err != Y4M_OK)
	mjpeg_error_exit1("Input stream error: %s\n", y4m_strerr(err));

    if	(y4m_si_get_plane_count(&istream) != 3)
	mjpeg_error_exit1("Only the 3 plane formats supported");

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

    ywidth = y4m_si_get_width(&istream);	/* plane 0 = Y */
    yheight = y4m_si_get_height(&istream);
    ylen = ywidth * yheight;
    uvwidth = y4m_si_get_plane_width(&istream, 1);	/* planes 1&2 = U+V */
    uvheight = y4m_si_get_plane_height(&istream, 1);
    uvlen = y4m_si_get_plane_length(&istream, 1);
    
    /* initialize output stream */
    y4m_init_stream_info(&ostream);
    y4m_copy_stream_info(&ostream, &istream);
    y4m_write_stream_header(fileno(stdout), &ostream);
    
    /* allocate input and output buffers */
    yuvinout[0] = my_malloc(ylen*sizeof(u_char));
    yuvinout[1] = my_malloc(uvlen*sizeof(u_char));
    yuvinout[2] = my_malloc(uvlen*sizeof(u_char));
    yuvtmp = my_malloc(MAX(ylen,uvlen)*sizeof(u_char));

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
	y4m_log_stream_info(LOG_INFO, "", &istream);
    
    /* main processing loop */
    for (frames=0; y4m_read_frame(fdin,&istream,&iframe,yuvinout) == Y4M_OK; frames++)
	{
	    if (verbose && ((frames % 100) == 0))
		mjpeg_log(LOG_INFO, "Frame %d\n", frames);
	    
            convolveFrame(yuvinout[0],yuvtmp,ywidth,yheight,interlace,lumaXtaps,NlumaX,lumaYtaps,NlumaY);
            convolveFrame(yuvinout[1],yuvtmp,uvwidth,uvheight,interlace,chromaXtaps,NchromaX,chromaYtaps,NchromaY);
            convolveFrame(yuvinout[2],yuvtmp,uvwidth,uvheight,interlace,chromaXtaps,NchromaX,chromaYtaps,NchromaY);

	    y4m_write_frame(fileno(stdout), &ostream, &iframe, yuvinout);

	}
    
    /* clean up */
    y4m_fini_frame_info(&iframe);
    y4m_fini_stream_info(&istream);
    y4m_fini_stream_info(&ostream);
    exit(0);
}


/* Move memory allocation error checking here to clean up code */
static void *my_malloc(size_t size)
{
    void *tmp = malloc(size);
    if (tmp == NULL)
	    mjpeg_error_exit1("malloc(%ld) failed\n", (long)size);
    return tmp;
}


/* Compute 1D filter coefficients (center and right-side taps only) */
/* To minimize artifacts at the boundaries, compute filters of all sizes */
/* from 1 to length and use shorter filters near the edge of the frame */
/* Also, normalize to a DC gain of 1 */
static void get_coeff(float **taps, int length, float bandwidth)
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

static void convolveLine(u_char *data,int width,int datastride,int outstride,float *filter,int flen,u_char *out)
{
    int i;
    for( i=0; i<width; i++ ) {
        int k,r;
        u_char *d1=data,*d2=data;
        float tempout=filter[0]*data[0];
        for( k=1; k<=flen; k++) {
            d1-=datastride;
            d2+=datastride;
            tempout+=filter[k]*(d1[0]+d2[0]);
        }
        r=tempout+0.5;
        /* clip and cast to integer */
        out[0]=MIN(MAX(r,0),255);
        out+=outstride;
        data+=1;
    }
}

static void convolveField(u_char *data, int width, int height, int outstride, float **filter, int filterlength, u_char *output)
{

    int n,datastride,datalength;

    datalength=height/outstride;
    datastride=width*outstride;

    /* leading edge, use filters of increasing width */
    for(n=0;n<filterlength;n++)
	{
            convolveLine(data,width,datastride,height,filter[n],n,output);
            data+=datastride;
            output+=outstride;
	}
    /* center, use full-width filter */
    for(n=filterlength; n<datalength-filterlength; n++)
	{
            convolveLine(data,width,datastride,height,filter[filterlength],filterlength,output);
            data+=datastride;
            output+=outstride;
	}
    /* trailing edge, use filters of decreasing width */
    for(n=datalength-filterlength;n<datalength;n++)
	{
            convolveLine(data,width,datastride,height,filter[datalength-n-1],datalength-n-1,output);
            data+=datastride;
            output+=outstride;
	}
}

static void convolveFrame(u_char *src,u_char *tmp,int w,int h,int interlace,float **xtap,int numx,float **ytap,int numy)
{
    if (interlace ) {
        convolveField(src,  w,h,2,ytap,numy,tmp);
        convolveField(src+w,w,h,2,ytap,numy,tmp+1);
    }
    else
        convolveField(src,w,h,1,ytap,numy,tmp);

    convolveField(tmp,h,w,1,xtap,numx,src);
}

static void usage(void)
{
    fprintf(stderr, "usage: %s [-h] [-v] [-L lumaXtaps,lumaXBW,lumaYtaps,lumaYBW] ", __progname);
    fprintf(stderr, "[-C chromaXtaps,chromaXBW,chromaYtaps,chromaYBW] ");
    fprintf(stderr, "[-x chromaXtaps,chromaXBW] [-X lumaXtaps,lumaXBW] ");
    fprintf(stderr, "[-y chromaYtaps,chromaYBW] [-Y lumaYtaps,lumaYBW]\n");
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
    fprintf(stderr, "\n\t-x/-X/-y/-Y change a vertical/horizontal parameter without affecting the other dimension's value\n");
    exit(1);
}
