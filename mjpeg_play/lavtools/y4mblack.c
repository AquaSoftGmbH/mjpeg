/*
 * $Id$
 *
 * Used to generate YUV4MPEG2 frames with the specific interlace, 
 * dimensions, pixel aspect  and Y/U/V values.  By default the frames 
 * generated are 16/128/128 or pure black.
 *
 * This is strictly an  output program, stdin is ignored.
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
	int	sts, c, width = 720, height = 480, uvlen, noheader = 0;
	int	Y = 16, U = 128, V = 128;
	int	numframes = 1, force = 0, xcss411 = 0;
	y4m_ratio_t	rate_ratio = y4m_fps_NTSC;
	y4m_ratio_t	aspect_ratio = y4m_sar_SQUARE;
	u_char	*yuv[3];
	y4m_stream_info_t ostream;
	y4m_frame_info_t oframe;
	char	interlace = Y4M_ILACE_NONE;

	opterr = 0;

	while	((c = getopt(argc, argv, "Hfxw:h:r:i:a:Y:U:V:n:")) != EOF)
		{
		switch	(c)
			{
			case	'H':
				noheader = 1;
				break;
			case	'a':
				sts = y4m_parse_ratio(&aspect_ratio, optarg);
				if	(sts != Y4M_OK)
					{
					mjpeg_error("Invalid aspect: %s",
						optarg);
					exit(1);
					}
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
					{
					mjpeg_error("Invalid rate: %s", optarg);
					exit(1);
					}
				break;
			case	'Y':
				Y = atoi(optarg);
				break;
			case	'U':
				U = atoi(optarg);
				break;
			case	'V':
				V = atoi(optarg);
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
			case	'x':
				xcss411 = 1;
				break;
			case	'f':
				force = 1;
				break;
			case	'n':
				numframes = atoi(optarg);
				break;
			case	'?':
			default:
				usage();
			}
		}

	if	(width <= 0)
		{
		mjpeg_error("Invalid Width: %d", width);
		exit(1);
		}
	if	(height <= 0)
		{
		mjpeg_error("Invalid Height: %d", height);
		exit(1);
		}
	if	(!force && (Y < 16 || Y > 235))
		{
		mjpeg_error("16 < Y < 235");
		exit(1);
		}
	if	(!force && (U < 16 || U > 240))
		{
		mjpeg_error("16 < U < 240");
		exit(1);
		}
	if	(!force && (V < 16 || V > 240))
		{
		mjpeg_error("16 < V < 240");
		exit(1);
		}

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
 * For 4:1:1 and 4:2:0 the sizes of the U and V arrays are different but the
 * total amount of data is the same.
*/
	if	(xcss411)
		uvlen = (width / 4) * height;
	else
		uvlen = (height / 2) * (width / 2);
	yuv[0] = malloc(height * width);
	yuv[1] = malloc(uvlen);
	yuv[2] = malloc(uvlen);

/*
 * Now fill the array once with black but use the provided Y, U and V values
*/
	memset(yuv[0], Y, width * height);
	memset(yuv[1], U, uvlen);
	memset(yuv[2], V, uvlen);

	if	(noheader == 0)
		y4m_write_stream_header(fileno(stdout), &ostream);
	while	(numframes--)
		y4m_write_frame(fileno(stdout), &ostream, &oframe, yuv);

	free(yuv[0]);
	free(yuv[1]);
	free(yuv[2]);
	y4m_fini_stream_info(&ostream);
	y4m_fini_frame_info(&oframe);
	exit(0);
	}

void usage()
	{

	fprintf(stderr, "%s usage: [-H] [-f] [-n numframes] [-w width] [-h height] [-Y val] [-U val] [-V val] [-a pixel aspect] [-i p|t|b] [-r rate]\n", __progname);
	fprintf(stderr, "\n  Omit the YUV4MPEG2 header [-H]");
	fprintf(stderr, "\n  Generate 4:1:1 rather than 4:2:0 output [-x]");
	fprintf(stderr, "\n  Allow out of range Y/U/V values [-f]");
	fprintf(stderr, "\n  Numframes [-n num] (1)");
	fprintf(stderr, "\n  Width [-w width] (720)");
	fprintf(stderr, "\n  Height [-h height] (480)");
	fprintf(stderr, "\n  Interlace codes [-i X] p (none/progressive) t (top first) b (bottom first) (p)");
	fprintf(stderr, "\n  Rate (as ratio) [-r N:M] (30000:1001)");
	fprintf(stderr, "\n  Pixel aspect (as ratio) [-a N:M] (1:1)");
	fprintf(stderr, "\n  Y: [-Y val] (16)");
	fprintf(stderr, "\n  U: [-U val] (128)");
	fprintf(stderr, "\n  V: [-V val] (128)");
	fprintf(stderr, "\n");
	exit(1);
	}
