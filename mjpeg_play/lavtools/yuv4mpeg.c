/*
 * $Id$
 *
 * A simple progam that's quite useful when dealing with raw/headerless data 
 * from programs such as ffmpeg or fxtv.
 *
 * Used to convert standard EYUV to YUV4MPEG2 which is the format used by
 * mpeg2enc from the mjpeg-tools suite.   The main difference between EYUV
 * and YUV4MPEG2 is a header line giving the dimensions and framerate code
 * plus a FRAME marker.
 *
 * If the -x option is given the input data can be in 4:1:1 format and the
 * necessary X tag will be added to the YUV4MPEG2 header.   This is useful
 * with programs such as ffmpeg that can produce raw 411 output from NTSC DV
 * files.
 *
 * This is strictly a filter, the use is in a pipeline such as the following
 * which extracts the PPM data from a video capture, pipes that thru ppmtoeyuv
 * and then on to this program and finally either into mpeg2enc or to a file.
 *
 * fxtv ... | ppmtoeyuv | yuv4mpeg -w W -h H -r M:N | yuvscaler | mpeg2enc ...
 *
 * To use ffmpeg to decode DV files, fast denoise, convert to 4:2:0 and
 * display (or encode):
 *
 * ffmpeg -i file.dv -an -f rawvideo -pix_fmt yuv411p /dev/stdout | 
 *    yuv4mpeg -x -a 10:11 -w 720 -h 480 -r 30000:1001 -i b |
 *    yuvdenoise -S 0 -f |
 *    y4mscaler -O chromass=420_MPEG2 |
 *    yuvplay  (or mpeg2enc -o -f 8 ...)
 *
 *   The -a, -r, -w and -h values given are the defaults and need not have been
 *   given.    The -i (interlace) will default to NONE ('p' or progressive) and
 *   must usually be specified on the command line.
 *
 * yuv4mpeg can also be used to catenate multiple pure (headerless) EYUV files:
 *
 *	cat *.yuv | yuv4mpeg -w W -h H -r M:N | mpeg2enc ...
 *
 * The U and V planes can be swapped by using '-k'.  This is used to convert
 * from YV12 which the transcode utilities generate and the IYUV (aka YUV420)
 * which mpeg2enc wants.
 *
 * NOTE: Obviously when catenating multiple files together they ALL must have 
 *       the same attributes (dimensions,  framerate, etc)!
*/

#ifdef	HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>

#include "yuv4mpeg.h"

extern	char	*__progname;

static	void	usage(void);

int
main(int argc, char **argv)
	{
	int	sts, c, width = 720, height = 480, swapuv = 0, uvlen;
	int	xcss411 = 0;
	y4m_ratio_t	rate_ratio = y4m_fps_NTSC;
	y4m_ratio_t	aspect_ratio = y4m_sar_NTSC_CCIR601;
	u_char	*yuv[3];
	y4m_stream_info_t ostream;
	y4m_frame_info_t oframe;
	char	interlace = Y4M_ILACE_NONE;

	opterr = 0;

	while	((c = getopt(argc, argv, "kw:h:r:i:a:x")) != EOF)
		{
		switch	(c)
			{
			case	'x':
				xcss411 = 1;
				break;
			case	'k':
				swapuv = 1;
				break;
			case	'a':
				sts = y4m_parse_ratio(&aspect_ratio, optarg);
				if	(sts != Y4M_OK)
					mjpeg_error_exit1("Invalid aspect: %s",
						optarg);
				break;
			case	'w':
				width = atoi(optarg);
				break;
			case	'h':
				height = atoi(optarg);
				break;
			case	'r':
				sts = y4m_parse_ratio(&rate_ratio, optarg);
				if	(sts != Y4M_OK)
					mjpeg_error_exit1("Invalid rate: %s", optarg);
				break;
			case	'i':
				switch	(optarg[0])
					{
					case	'p':
						interlace = Y4M_ILACE_NONE;
						break;
					case	't':
						interlace = Y4M_ILACE_TOP_FIRST;
						break;
					case	'b':
						interlace = Y4M_ILACE_BOTTOM_FIRST;
						break;
					default:
						usage();
					}
				break;
			case	'?':
			default:
				usage();
			}
		}

	if	(width <= 0)
		mjpeg_error_exit1("Invalid Width: %d", width);
	if	(height <= 0)
		mjpeg_error_exit1("Invalid Height: %d", height);

	y4m_init_stream_info(&ostream);
	y4m_init_frame_info(&oframe);
	y4m_si_set_width(&ostream, width);
	y4m_si_set_height(&ostream, height);
	y4m_si_set_interlace(&ostream, interlace);
	y4m_si_set_framerate(&ostream, rate_ratio);
	y4m_si_set_sampleaspect(&ostream, aspect_ratio);
	if	(xcss411)
		y4m_xtag_add(&(ostream.x_tags), "XYSCSS=411");

/*
 *                    Y                    U                      V
 * YUV size = (height * width) + (height/2 * width/2) + (height/2 * width/2)
*/
	uvlen = (height / 2) * (width / 2);
	yuv[0] = malloc(height * width);
	yuv[1] = malloc(uvlen);
	yuv[2] = malloc(uvlen);

	y4m_write_stream_header(fileno(stdout), &ostream);
	while	(1)
		{
		if	(y4m_read(fileno(stdin), yuv[0], height * width))
			break;
		if	(swapuv)
			{
			if	(y4m_read(fileno(stdin), yuv[2], uvlen))
				break;
			if	(y4m_read(fileno(stdin), yuv[1], uvlen))
				break;
			}
		else
			{
			if	(y4m_read(fileno(stdin), yuv[1], uvlen))
				break;
			if	(y4m_read(fileno(stdin), yuv[2], uvlen))
				break;
			}
		y4m_write_frame(fileno(stdout), &ostream, &oframe, yuv);
		}

	free(yuv[0]);
	free(yuv[1]);
	free(yuv[2]);
	y4m_fini_stream_info(&ostream);
	y4m_fini_frame_info(&oframe);
	exit(0);
	}

static void usage()
	{

	fprintf(stderr, "%s usage: [-k] -w width -h height [-a pixel aspect] [-i p|t|b] -r rate\n", __progname);
	fprintf(stderr, "\n  Swap U and V: -k\n");
	fprintf(stderr, "\n  Interlace codes [-i X]: p (none) t (top first) b (bottom first)\n");
	fprintf(stderr, "\n  Rate (as ratio) [-r N:M] (30000:1001):\n");
	fprintf(stderr, "\n  Pixel aspect (as ratio) [-a N:M] (10:11):\n");
	fprintf(stderr, "\n");
	exit(1);
	}
