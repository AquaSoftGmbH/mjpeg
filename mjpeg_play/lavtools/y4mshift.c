
/*
 * $Id$
 *
 * written by Steven M. Schultz <sms@2BSD.COM>
 *
 * 2003/5/8 - added the ability to place a black border around the frame.
 *	The -b usage is the same as for yuvdenoise.   This is a useful 
 *	feature when yuvdenoise or other programs are not being used but a
 *	border is still desired.
 *
 * Simple program to shift the data an even number of pixels.   The shift count
 * is positive for shifting to the right and negative for a left shift.   
 *
 * Usage: y4mfill -n N [ -b xoffset,yoffset,xsize,ysize ]
 *
 * No file arguments are needed since this is a filter only program.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "yuv4mpeg.h"

extern  char    *__progname;

#define HALFSHIFT (shiftnum / 2)

	void	black_border(u_char **, char *, int, int);
static  void    usage();

int main(int argc, char **argv)
        {
        int     i, c, width, height, frames, err, uvlen;
        int     shiftnum = 0, rightshift;
        int     verbose = 0, fdin;
        u_char  *yuv[3], *line;
	char	*borderarg = NULL;
        y4m_stream_info_t istream, ostream;
        y4m_frame_info_t iframe;

        fdin = fileno(stdin);

        opterr = 0;
        while   ((c = getopt(argc, argv, "hvn:b:")) != EOF)
                {
                switch  (c)
                        {
			case	'b':
				borderarg = strdup(optarg);
				break;
                        case    'n':
                                shiftnum = atoi(optarg);
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

        if      (shiftnum & 1)
                usage();

        if      (shiftnum > 0)
                rightshift = 1;
        else
                {
                rightshift = 0;
                shiftnum = abs(shiftnum);
                }

        y4m_init_stream_info(&istream);
        y4m_init_frame_info(&iframe);

        err = y4m_read_stream_header(fdin, &istream);
        if      (err != Y4M_OK)
                mjpeg_error_exit1("Input stream error: %s\n", y4m_strerr(err));

        width = y4m_si_get_width(&istream);
        height = y4m_si_get_height(&istream);
        uvlen = (height / 2) * (width / 2);

        if      (shiftnum > width / 2)
                {
                fprintf(stderr, "%s: nonsense to shift %d out of %d\n",
                        __progname, shiftnum, width);
                exit(1);
                }

        y4m_init_stream_info(&ostream);
        y4m_copy_stream_info(&ostream, &istream);
        y4m_write_stream_header(fileno(stdout), &ostream);

        yuv[0] = malloc(height * width);
        if      (yuv[0] == NULL)
                {
                fprintf(stderr, "%s: malloc(%d) failed\n", __progname,
                        width * height);
                exit(1);
                }
        yuv[1] = malloc(uvlen);
        if      (yuv[1] == NULL)
                {
                fprintf(stderr, "%s: malloc(%d) failed\n", __progname, uvlen);
                exit(1);
                }
        yuv[2] = malloc(uvlen);
        if      (yuv[2] == NULL)
                {
                fprintf(stderr, "%s: malloc(%d) failed\n", __progname, uvlen);
                exit(1);
                }

        if      (verbose)
                fprintf(stderr, "%s: W %d H %d R %d/%d Fsize %d\n", 
                        __progname, width, height, istream.framerate.n,
                        istream.framerate.d, istream.framelength);

        frames = 0;
        for     (;y4m_read_frame(fdin,&istream,&iframe,yuv) == Y4M_OK; frames++)
                {
                if      (verbose && ((frames % 100) == 0))
                        fprintf(stderr, "%s: Frame %d\n", __progname, frames);

                if      (shiftnum == 0)
                        goto outputframe;
                for     (i = 0; i < height; i++)
                        {
/*
 * Y
*/
                        line = &yuv[0][i * width];
                        if      (rightshift)
                                {
                                bcopy(line, line + shiftnum, width - shiftnum);
                                memset(line, 16, shiftnum); /* black */
                                }
                        else 
                                {
                                bcopy(line + shiftnum, line, width - shiftnum);
                                memset(line + width - shiftnum, 16, shiftnum);
                                }
                        }
/*
 * U
*/
                for     (i = 0; i < height / 2; i++)
                        {
                        line = &yuv[1][i * (width / 2)];
                        if      (rightshift)
                                {
                                bcopy(line, line+HALFSHIFT, (width-shiftnum)/2);
                                memset(line, 128, HALFSHIFT); /* black */
                                }
                        else
                                {
                                bcopy(line+HALFSHIFT, line, (width-shiftnum)/2);
                                memset(line+(width-shiftnum)/2, 128, HALFSHIFT);
                                }
                        }
/*
 * V
*/
                for     (i = 0; i < height / 2; i++)
                        {
                        line = &yuv[2][i  * (width / 2)];
                        if      (rightshift)
                                {
                                bcopy(line, line+HALFSHIFT, (width-shiftnum)/2);
                                memset(line, 128, HALFSHIFT); /* black */
                                }
                        else
                                {
                                bcopy(line+HALFSHIFT, line, (width-shiftnum)/2);
                                memset(line+(width-shiftnum)/2, 128, HALFSHIFT);
                                }
                        }
outputframe:
		if	(borderarg)
			black_border(yuv, borderarg, width, height);
                y4m_write_frame(fileno(stdout), &ostream, &iframe, yuv);
                }
        y4m_fini_frame_info(&iframe);
        y4m_fini_stream_info(&istream);
        y4m_fini_stream_info(&ostream);

        exit(0);
        }

/*
 * -b Xoff,Yoff,Xsize,YSize
*/

void black_border (u_char *yuv[], char *borderstring, int W, int H)
	{
	static	int parsed = -1;
	static	int BX0, BX1;	/* Left, Right border columns */
	static	int BY0, BY1;	/* Top, Bottom border rows */
	int	i1, i2, i, dy, W2, H2;
  
	if	(parsed == -1)
		{
		parsed = 0;
		i = sscanf(borderstring, "%d,%d,%d,%d", &BX0, &BY0, &i1, &i2);
		if	(i != 4 || (BX0 % 2) || (BY0 % 4) || i1 < 0 || i2 < 0 ||
			 (BX0 + i1 > W) || (BY0 + i2 > H))
			{
			mjpeg_log(LOG_WARN, " border args invalid - ignored");
			return;
			}
		BX1 = BX0 + i1;
		BY1 = BY0 + i2;
		parsed = 1;
		}
/*
 * If a borderstring was seen but declared invalid then it
 * is being ignored so just return.
*/
	if	(parsed == 0)
		return;

	W2 = W / 2;
	H2 = H / 2;

/*
 * Yoff Lines at the top.   If the vertical offset is 0 then no top border
 * is being requested and there is nothing to do.
*/
	if	(BY0 != 0)
		{
		memset(yuv[0], 16,  W  * BY0);
		memset(yuv[1], 128, W2 * BY0/2);
		memset(yuv[2], 128, W2 * BY0/2);
		}
/*
 * Height - (Ysize + Yoff) lines at bottom.   If the bottom coincides with
 * the frame size then there is nothing to do.
*/
	if	(H != BY1)
		{
		memset(&yuv[0][BY1 * W], 16, W  * (H - BY1));
		memset(&yuv[1][(BY1 / 2) * W2], 128, W2 * (H - BY1)/2);
		memset(&yuv[2][(BY1 / 2) * W2], 128, W2 * (H - BY1)/2);
		}
/*
 * Now the partial lines in the middle.   Go from rows BY0 thru BY1 because
 * the whole rows (0 thru BY0) and (BY1 thru H) have been done above.
*/
	for	(dy = BY0; dy < BY1; dy++)
		{
/*
 * First the columns on the left (x = 0 thru BX0).   If the X offset is 0
 * then there is nothing to do.
*/
		if	(BX0 != 0)
			{
			memset(&yuv[0][dy * W], 16, BX0);
			memset(&yuv[1][dy / 2 * W2], 128, BX0 / 2);
			memset(&yuv[2][dy / 2 * W2], 128, BX0 / 2);
			}
/*
 * Then the columns on the right (x = BX1 thru W).   If the right border
 * start coincides with the frame size then there is nothing to do.
*/
		if	(W != BX1)
			{
			memset(&yuv[0][(dy * W) + BX1], 16, W - BX1);
			memset(&yuv[1][(dy/2 * W2) + BX1/2], 128, (W - BX1)/2);
			memset(&yuv[2][(dy/2 * W2) + BX1/2], 128, (W - BX1)/2);
			}
		}

	}
static void usage()
        {

        fprintf(stderr, "%s: usage: [-v] [-h] [-b xoff,yoff,xsize,ysize] -n N\n", __progname);
        fprintf(stderr, "%s:\tN = number of pixels to shift - must be even!!\n",
                __progname);
        fprintf(stderr, "%s:\t\tpositive count means shift right\n",__progname);
        fprintf(stderr, "%s:\t\t0 passes the data thru unchanged\n",__progname);
        fprintf(stderr, "%s:\t\tnegative count means shift left\n", __progname);
	fprintf(stderr, "%s:\t-b creates black border\n", __progname);
	fprintf(stderr, "%s:\t\tusage is same as yuvdenoise.\n", __progname);
        fprintf(stderr, "%s:\t-v progress report every 100 frames\n", 
                __progname);
        fprintf(stderr, "%s:\t-h print this usage summary\n", __progname);
        exit(1);
        }
