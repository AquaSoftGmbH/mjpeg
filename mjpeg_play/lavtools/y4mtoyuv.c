/*
 * $Id$
 *
 * Simple program to convert the YUV4MPEG2 format used by the 
 * mjpeg.sourceforge.net suite of programs into pure EYUV format used
 * by the mpeg4ip project and other programs. 
 *
 * 2001/10/19 - Rewritten to use the y4m_* routines from mjpegtools.
 *
 * 2004/1/1 - Added XYSCSS tag handling to deal with 411, 422, 444 data 
*/

#ifdef	HAVE_CONFIG_H
#include "config.h"
#else
#define	HAVE_STDINT_H
#endif

#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>

#include <mjpegtools/yuv4mpeg.h>
#include <mjpegtools/mjpeg_logging.h>

extern	char	*__progname;

static	void	usage();

main(int argc, char **argv)
	{
	int	c, n, width, height, frames, err, uvlen;
	int	fd_in = fileno(stdin), fd_out = fileno(stdout);
	int	ss_v, ss_h;
	const	char *tag;
	u_char	*yuv[3];
	y4m_xtag_list_t *xtags;
	y4m_stream_info_t istream;
	y4m_frame_info_t iframe;

	opterr = 0;
	while	((c = getopt(argc, argv, "h")) != EOF)
		{
		switch	(c)
			{
			case	'h':
			case	'?':
			default:
				usage();
			}
		}

	y4m_init_stream_info(&istream);
	y4m_init_frame_info(&iframe);

	err = y4m_read_stream_header(fd_in, &istream);
	if	(err != Y4M_OK)
		mjpeg_error_exit1("Input stream error: %s\n", y4m_strerr(err));

	width = y4m_si_get_width(&istream);
	height = y4m_si_get_height(&istream);

	xtags = y4m_si_xtags(&istream);
	tag = NULL;
	for	(n = y4m_xtag_count(xtags) - 1; n >= 0; n++)
		{
		tag = y4m_xtag_get(xtags, n);
		if	(strncmp(tag, "XYSCSS=", 7) == 0)
			break;
		}
	if	(tag && (n >= 0))
		{
		tag += 7;
		if	(!strcmp(tag, "411"))
			{
			ss_h = 4;
			ss_v = 1;
			}
		else if	(!strcmp(tag, "420") || !strcmp(tag, "420MPEG2") ||
			 !strcmp(tag, "420PALDV") || !strcmp(tag,"420JPEG"))
			{
			ss_h = 2;
			ss_v = 2;
			}
		else if	(!strcmp(tag, "422"))
			{
			ss_h = 2;
			ss_v = 1;
			}
		else if	(!strcmp(tag, "444"))
			{
			ss_h = 1;
			ss_v = 1;
			}
		else
			{
			mjpeg_error_exit1("Unknown XYSCSS tag: '%s'", tag);
			/* NOTREACHED */
			}
		}

	uvlen = (width / ss_h) * (height / ss_v);

	yuv[0] = malloc(height * width);
	yuv[1] = malloc(uvlen);
	yuv[2] = malloc(uvlen);

	mjpeg_log(LOG_INFO,"Width: %d Height: %d Rate: %d/%d Framesize: %d\n",
		width, height, istream.framerate.n,
		istream.framerate.d,
		istream.framelength);

	frames = 0;
	while	(y4m_read_frame_header(fd_in, &iframe) == Y4M_OK)
		{
		if	(y4m_read(fd_in, yuv[0], height * width) != Y4M_OK)
			break;
		if	(y4m_read(fd_in, yuv[1], uvlen) != Y4M_OK)
			break;
		if	(y4m_read(fd_in, yuv[2], uvlen) != Y4M_OK)
			break;
		frames++;
		if	(y4m_write(fd_out, yuv[0], height * width) != Y4M_OK)
			break;
		if	(y4m_write(fd_out, yuv[1], uvlen) != Y4M_OK)
			break;
		if	(y4m_write(fd_out, yuv[2], uvlen) != Y4M_OK)
			break;
		}
	y4m_fini_frame_info(&iframe);
	y4m_fini_stream_info(&istream);

	mjpeg_log(LOG_INFO, "Frames processed: %d\n", frames);
	exit(0);
	}

static void usage()
	{

	mjpeg_error_exit1("<file.y4m > file.yuv");
	/* NOTREACHED */
	}
