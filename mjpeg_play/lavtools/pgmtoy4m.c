/*
 * $Id$
 *
 * pgmtoy4m converts the PGM output of "mpeg2dec -o pgmpipe" to YUV4MPEG2 on
 * stdout.
 *
 * Note: mpeg2dec uses a perversion of the PGM format - they're reall not
 * "Grey Maps" but rather a catenation of the 420P data (commonly called
 * "YUV").    The type is P5 ("raw") and the number of rows is really
 * the total of the Y', Cr and Cb heights.   The U and V data is "joined"
 * together - you have 1 row of U and 1 row of V per "row" of PGM data.
 *
 * NOTE: You MAY need to know the field order (top or bottom field first),
 *	sample aspect ratio and frame rate because the PGM format makes
 *	none of that information available!  There are defaults provided
 *	that hopefully will do the right thing in the common cases.  The
 *
 *	The defaults provided are: top field first, NTSC rate of 30000/1001
 *	frames/second, and a sample aspect of 10:11.
*/

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <sys/types.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "yuv4mpeg.h"

extern	char	*__progname;

static	void	usage(void);
static	int	getint(int);

#define	P5MAGIC	(('P' * 256) + '5')

static int
getmagicnumber(int fd)
	{
	char	ch1 = -1, ch2 = -1;

	read(fd, &ch1, 1);
	read(fd, &ch2, 1);
	return((ch1 * 256) + ch2);
	}

static int
piperead(int fd, u_char *buf, int len)
	{
	int n = 0;
	int r = 0;

	while	(r < len)
		{
		n = read (fd, buf + r, len - r);
		if	(n <= 0)
			return(r);
		r += n;
		}
	return(r);
	}

int
main(int argc, char **argv)
	{
	int	width, height, uvlen, verbose = 1, fdout, fdin, c, i;
	int	w2, columns, rows, maxval, magicn, frameno;
	u_char	*yuv[3];
	y4m_ratio_t	rate_ratio = y4m_fps_UNKNOWN;
	y4m_ratio_t	aspect_ratio = y4m_sar_UNKNOWN;
	char	ilace = Y4M_ILACE_TOP_FIRST;
	y4m_frame_info_t  oframe;
	y4m_stream_info_t ostream;

	fdin = fileno(stdin);
	fdout = fileno(stdout);

	opterr = 0;
	while	((c = getopt(argc, argv, "i:a:r:hv:")) != EOF)
		{
		switch	(c)
			{
			case	'i':
				switch	(optarg[0])
					{
					case	'p':
						ilace = Y4M_ILACE_NONE;
						break;
					case	't':
						ilace = Y4M_ILACE_TOP_FIRST;
						break;
					case	'b':
						ilace = Y4M_ILACE_BOTTOM_FIRST;
						break;
					default:
						usage();
					}
				break;
			case	'v':
				verbose = atoi(optarg);
				if	(verbose < 0 || verbose > 3)
					mjpeg_error_exit1("Verbosity [0..2]");
				break;
			case	'a':
				i = y4m_parse_ratio(&aspect_ratio, optarg);
				if	(i != Y4M_OK)
					mjpeg_error_exit1("Invalid aspect: %s",
						optarg);
				break;
			case	'r':
				i = y4m_parse_ratio(&rate_ratio, optarg);
				if	(i != Y4M_OK)
					mjpeg_error_exit1("Invalid rate: %s",
						optarg);
				break;
			case	'h':
			case	'?':
			default:
				usage();
				/* NOTREACHED */
			}
		}

	if	(isatty(fdout))
		mjpeg_error_exit1("stdout must not be a terminal");

	mjpeg_default_handler_verbosity(verbose);

/*
 * Check that the input stream is really a P5 (rawbits PGM) stream, then if it
 * is retrieve the "rows" and "columns" (width and height) and the maxval.
 * The maxval will be 255 if the data was generated from mpeg2dec!
*/
	magicn = getmagicnumber(fdin);
	if	(magicn != P5MAGIC)
		mjpeg_error_exit1("Input not P5 PGM data, got %x", magicn);
	columns = getint(fdin);
	rows = getint(fdin);
	maxval = getint(fdin);
	mjpeg_log(LOG_INFO, "P5 cols: %d rows: %d maxval: %d", 
		columns, rows, maxval);

	if	(maxval != 255)
		mjpeg_error_exit1("Maxval (%d) != 255, not mpeg2dec output?",
			maxval);
/*
 * Put some sanity checks on the sizes - handling up to 4096x4096 should be 
 * enough for now :)
*/
	if	(columns < 0 || columns > 4096 || rows < 0 || rows > 4096)
		mjpeg_error_exit1("columns (%d) or rows(%d) < 0 or > 4096",
			columns, rows);

	width = columns;
	w2 = width / 2;
	height = (rows * 2 ) / 3;
	uvlen = w2 * (height / 2);
	yuv[0] = malloc(height * width);
	yuv[1] = malloc(uvlen);
	yuv[2] = malloc(uvlen);
	if	(yuv[0] == NULL || yuv[1] == NULL || yuv[2] == NULL)
		mjpeg_error_exit1("malloc yuv[]");

/*
 * If the (sample) aspect ratio and frame rate were not specified on the
 * command line try to intuit the video norm of NTSC or PAL by looking at the
 * height of the frame.
*/
	if	(Y4M_RATIO_EQL(aspect_ratio, y4m_sar_UNKNOWN))
		{
		if	(height == 480 || height == 240)
			{
			aspect_ratio = y4m_sar_NTSC_CCIR601;
			mjpeg_log(LOG_WARN, "sample aspect not specified, using NTSC CCIR601 value based on frame height of %d", height);
			}
		else if	(height == 576 || height == 288)
			{
			aspect_ratio = y4m_sar_PAL_CCIR601;
			mjpeg_log(LOG_WARN, "sample aspect not specified, using PAL CCIR601 value based on frame height of %d", height);
			}
		else
			{
			mjpeg_log(LOG_WARN, "sample aspect not specified and can not be inferred from frame height of %d (leaving sar as unknown", height);
			}
		}
	if	(Y4M_RATIO_EQL(rate_ratio, y4m_fps_UNKNOWN))
		{
		if	(height == 480 || height == 240)
			{
			rate_ratio = y4m_fps_NTSC;
			mjpeg_log(LOG_WARN, "frame rate not specified, using NTSC value based on frame height of %d", height);
			}
		else if	(height == 576 || height == 288)
			{
			rate_ratio = y4m_fps_PAL;
			mjpeg_log(LOG_WARN, "frame rate not specified, using PAL value based on frame height of %d", height);
			}
		else
			{
			mjpeg_log(LOG_WARN, "frame rate not specified and can not be inferred from frame height of %d (using NTSC value)", height);
			rate_ratio = y4m_fps_NTSC;
			}
		}
	y4m_init_stream_info(&ostream);
	y4m_init_frame_info(&oframe);
	y4m_si_set_interlace(&ostream, ilace);
	y4m_si_set_framerate(&ostream, rate_ratio);
	y4m_si_set_sampleaspect(&ostream, aspect_ratio);
	y4m_si_set_width(&ostream, width);
	y4m_si_set_height(&ostream, height);

	y4m_write_stream_header(fdout, &ostream);

/*
 * Now continue reading P5 frames as long as the magic number appears.  Corrupt
 * (malformed) input or EOF will terminate the loop.   The columns, rows and
 * maxval must be read to get to the raw data.  It's probably not worth doing
 * anything (in fact it's unclear what could be done ;)) with the values 
 * if they're different from the first header which was used to set the
 * output stream parameters.
*/
	frameno = 0;
	while	(1)	
		{
		piperead(fdin, yuv[0], width * height);
		for	(i = 0; i < height / 2; i++)
			{
			piperead(fdin, yuv[1] + (i * w2), w2);
			piperead(fdin, yuv[2] + (i * w2), w2);
			}
		y4m_write_frame(fdout, &ostream, &oframe, yuv);

		magicn = getmagicnumber(fdin);
		if	(magicn != P5MAGIC)
			{
			mjpeg_log(LOG_DEBUG, "frame: %d got magic: %x\n",
				frameno, magicn);
			break;
			}
		columns = getint(fdin);
		rows = getint(fdin);
		maxval = getint(fdin);
		frameno++;
		mjpeg_log(LOG_DEBUG, "frame: %d P5MAGIC cols: %d rows: %d maxval: %d", frameno, columns, rows, maxval);
		}
	y4m_fini_frame_info(&oframe);
	y4m_fini_stream_info(&ostream);

	return 0;
	}

static void
usage(void)
	{

	fprintf(stderr, "%s usage: [-v n] [-i t|b|p] [-a sample aspect] [-r rate]\n",
		__progname);
	fprintf(stderr, "%s\taspect and rate in ratio form: -a  10:11 and -r 30000:1001 or -r 25:1 for example\n", __progname);
	exit(0);
	}

static int
getint(int fd)
	{
	char	ch;
	int	i;

	do
		{
		if	(read(fd, &ch, 1) == -1)
			return(-1);
		if	(ch == '#')
			{
			while	(ch != '\n')
				{
				if	(read(fd, &ch, 1) == -1)
					return(-1);
				}
			}
		} while (isspace(ch));

	if	(!isdigit(ch))
		return(-1);

	i = 0;
	do
		{
		i = i * 10 + (ch - '0');
		if	(read(fd, &ch, 1) == -1)
			break;
		} while (isdigit(ch));
	return(i);
	}
