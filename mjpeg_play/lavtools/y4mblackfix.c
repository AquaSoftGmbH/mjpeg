
/*
 * $Id$
 *
 * written by Steven M. Schultz <sms@2BSD.COM>
 * Simple filter to reduce the wandering grey blocks that can be seen in
 * dark scenes.   This is done by reducing the number of random shades of
 * "black".
 *
 * Usage: y4mblackfix [-y luma_center] [-Y luma_radius] ...
 *
 * No file arguments are needed since this is a filter only program.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#define HAVE_STDINT_H
#endif

#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "yuv4mpeg.h"

static  void    usage();

int main(int argc, char **argv)
        {
        int     i, j, c, width, height, nomodify = 0, frames, err, uvlen;
	int	y, u, v;
        int     verbose = 1, fdin;
	int	thresh_percentage = 5, highpass = 48, luma_center = 28;
	int	luma_radius = 4, u_chroma_center = 128, u_chroma_radius = 4;
	int	v_chroma_center = 128, v_chroma_radius = 4;
	int	pixel_thresh, num_dark, num_replaced;
        u_char  *yuv[3];
        y4m_stream_info_t istream, ostream;
        y4m_frame_info_t iframe;

        fdin = fileno(stdin);

        opterr = 0;
        while   ((c = getopt(argc, argv, "y:Y:u:U:v:V:n:d:p:Nh")) != EOF)
                {
                switch  (c)
                        {
			case	'N':
				nomodify = 1;
				break;
			case	'p':
				thresh_percentage = atoi(optarg);
				break;
                        case    'n':
                                highpass = atoi(optarg);
                                break;
			case	'y':
				luma_center = atoi(optarg);
				break;
			case	'Y':
				luma_radius = atoi(optarg);
				break;
			case	'u':
				u_chroma_center = atoi(optarg);
				break;
			case	'U':
				u_chroma_radius = atoi(optarg);
				break;
                        case    'v':
                                v_chroma_center = atoi(optarg);
                                break;
			case	'V':
				v_chroma_radius = atoi(optarg);
				break;
			case	'd':
				verbose = atoi(optarg);
				break;
                        case    '?':
                        case    'h':
                        default:
                                usage();
                        }
                }

	if	(verbose < 0 || verbose > 2)
		mjpeg_error_exit1("verbosity must be [0..2]");

	if	(thresh_percentage < 0 || thresh_percentage >= 100)
		mjpeg_error_exit1("-p value must be [0..99]%");

	if	(highpass < 0 || highpass > 255)
		mjpeg_error_exit1("-n value must be [0..255]");

	if	(luma_radius < 0 || luma_radius > 128)
		mjpeg_error_exit1("-Y value must be [1..128]");

	if	(u_chroma_radius < 0 || u_chroma_radius > 128)
		mjpeg_error_exit1("-U value must be [1..128]");

	if	(v_chroma_radius < 0 || v_chroma_radius > 128)
		mjpeg_error_exit1("-V value must be [1..128]");

	mjpeg_default_handler_verbosity(verbose);

        y4m_init_stream_info(&istream);
        y4m_init_frame_info(&iframe);

        err = y4m_read_stream_header(fdin, &istream);
        if      (err != Y4M_OK)
                mjpeg_error_exit1("Input stream error: %s\n", y4m_strerr(err));

        width = y4m_si_get_width(&istream);
        height = y4m_si_get_height(&istream);
        uvlen = (height / 2) * (width / 2);
	pixel_thresh = ((width * height) * thresh_percentage) / 100;

        y4m_init_stream_info(&ostream);
        y4m_copy_stream_info(&ostream, &istream);
        y4m_write_stream_header(fileno(stdout), &ostream);

        yuv[0] = malloc(height * width);
        if      (yuv[0] == NULL)
                mjpeg_error_exit1("malloc(%d) failed\n", width * height);

        yuv[1] = malloc(uvlen);
        if      (yuv[1] == NULL)
                mjpeg_error_exit1("malloc(%d) failed\n", uvlen);

        yuv[2] = malloc(uvlen);
        if      (yuv[2] == NULL)
                mjpeg_error_exit1("malloc(%d) failed\n", uvlen);

        if      (verbose)
                mjpeg_info("W %d H %d R %d/%d Fsize %d PixelThreshold %d", 
                        width, height, istream.framerate.n,
                        istream.framerate.d, istream.framelength,
			pixel_thresh);

        frames = 0;
        for     (;y4m_read_frame(fdin,&istream,&iframe,yuv) == Y4M_OK; frames++)
                {
		num_dark = 0;
		for	(i = 0; i < height; i++)
			{
			for	(j = 0; j < width; j++)
				{
				y = yuv[0][(i * width) + j];
				u = yuv[1][(i/2) * (width/2) + (j/2)];
				v = yuv[2][(i/2) * (width/2) + (j/2)];
				if	(y > highpass)
					continue;
				if	((luma_center - luma_radius) <= y &&
					  y <= (luma_center + luma_radius) &&
					 (u_chroma_center - u_chroma_radius) <= u &&
					 u <= (u_chroma_center + u_chroma_radius) &&
					 (v_chroma_center - v_chroma_radius) <= v &&
					 v <= (v_chroma_center + v_chroma_radius))
					{
					mjpeg_debug("DARK %d %d %d", y, u, v);
					num_dark++;
					if	(num_dark >= pixel_thresh)
						goto gotenough;
					}
				}
			}

gotenough:
		if	(num_dark < pixel_thresh || nomodify)
			goto outputframe;

/*
 * Frame exceeds the threshold for number of dark pixels.  Go thru the
 * data and replace the pixels with the 'center' values.
*/
		for	(num_replaced = 0, i = 0; i < height; i++)
			{
			for	(j = 0; j < width; j++)
				{
				y = yuv[0][(i * width) + j];
				u = yuv[1][(i/2) * (width/2) + (j/2)];
				v = yuv[2][(i/2) * (width/2) + (j/2)];
				if	(y > highpass)
					continue;
				if ((luma_center - luma_radius) <= y &&
				    y <= (luma_center + luma_radius) &&

				    (u_chroma_center - u_chroma_radius) <= u &&
				    u <= (u_chroma_center + u_chroma_radius) &&

				    (v_chroma_center - v_chroma_radius) <= v &&
				    v <= (v_chroma_center + v_chroma_radius))
					{
					yuv[0][(i * width) + j] = luma_center;
/*
 * It is unclear if the U/V components should be set to the center point or
 * to 128 to force the pixels to be a shade of grey instead of a (slightly)
 * different color.
*/

					yuv[1][(i/2) * (width/2) + (j/2)] = u_chroma_center;
					yuv[2][(i/2) * (width/2) + (j/2)] = v_chroma_center;
					num_replaced++;
					}
				}
			}
		if	(num_replaced)
			mjpeg_info("frame %d replaced %d", frames,num_replaced);
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

        fprintf(stderr, "usage: [-d N] [-h] [-n N] [-y N] [-Y N] [-u N] [-U N] [-v N] [-V N] [-p N] < in > out\n");
        fprintf(stderr, "\t-d N verbose level 0=quiet 1=normal 2=debug (1)\n");
        fprintf(stderr, "\t-N no modify, log info, leave data unchanged\n");
        fprintf(stderr, "\t-h print this usage summary\n");
	fprintf(stderr, "\t-n N highpass threshold (48)\n");
	fprintf(stderr, "\t-y N Y' center value (28)\n");
	fprintf(stderr, "\t-Y N Y' radius (4)\n");
	fprintf(stderr, "\t-u N Cb/U center value (128)\n");
	fprintf(stderr, "\t-U N Cb/U radius (4)\n");
	fprintf(stderr, "\t-v N Cr/V center value (128)\n");
	fprintf(stderr, "\t-V N Cr/V radius (4)\n");
	fprintf(stderr, "\t-p N dark pixel threshold in percent (5)\n");
	fprintf(stderr, "\t     0 = always do replacment\n");
        exit(1);
        }
