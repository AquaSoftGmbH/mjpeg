
/*
 * $Id$
 *
 * written by Steven M. Schultz <sms@2BSD.COM>
 * Simple program to shift the data an even number of pixels.   The shift count
 * is positive for shifting to the right and negative for a left shift.
 *
 * Usage: y4mfill -n N
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

static  void    usage();

int main(int argc, char **argv)
        {
        int     i, c, width, height, frames, err, uvlen;
        int     shiftnum = 0, rightshift;
        int     verbose = 0, fdin;
        u_char  *yuv[3], *line;
        y4m_stream_info_t istream, ostream;
        y4m_frame_info_t iframe;

        fdin = fileno(stdin);

        opterr = 0;
        while   ((c = getopt(argc, argv, "hvn:")) != EOF)
                {
                switch  (c)
                        {
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
                y4m_write_frame(fileno(stdout), &ostream, &iframe, yuv);
                }
        y4m_fini_frame_info(&iframe);
        y4m_fini_stream_info(&istream);
        y4m_fini_stream_info(&ostream);

        exit(0);
        }

static void usage()
        {

        fprintf(stderr, "%s: usage: [-v] [-h] -n N< in > out\n", __progname);
        fprintf(stderr, "%s:\tN = number of pixels to shift - must be even!!\n",
                __progname);
        fprintf(stderr, "%s:\t\tpositive count means shift right\n",__progname);
        fprintf(stderr, "%s:\t\t0 passes the data thru unchanged\n",__progname);
        fprintf(stderr, "%s:\t\tnegative count means shift left\n", __progname);
        fprintf(stderr, "%s:\t-v progress report every 100 frames\n", 
                __progname);
        fprintf(stderr, "%s:\t-h print this usage summary\n", __progname);
        exit(1);
        }

