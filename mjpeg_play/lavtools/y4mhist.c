/*
 * $Id$
 *
 * Simple program to print a crude histogram of the Y'CbCr values for YUV4MPEG2
 * stream.  Usually used with a small number (single) of frames but that's not
 * a requirement.  Reads stdin until end of stream and then prints the Y'CbCr
 * counts.
*/

#include "config.h"
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>

#include <yuv4mpeg.h>

	unsigned long long y_stats[256], u_stats[256], v_stats[256];

int
main(int argc, char **argv)
	{
	int	i, fdin, ss_v, ss_h, chroma_ss;
	int	plane0_l, plane1_l, plane2_l;
	u_char	*yuv[3], *cp;
	y4m_stream_info_t istream;
	y4m_frame_info_t iframe;

	fdin = fileno(stdin);

	y4m_accept_extensions(1);

	y4m_init_stream_info(&istream);
	y4m_init_frame_info(&iframe);

	if	(y4m_read_stream_header(fdin, &istream) != Y4M_OK)
		mjpeg_error_exit1("stream header error");

        if      (y4m_si_get_plane_count(&istream) != 3)
                mjpeg_error_exit1("Only 3 plane formats supported");

	chroma_ss = y4m_si_get_chroma(&istream);
	ss_h = y4m_chroma_ss_x_ratio(chroma_ss).d;
	ss_v = y4m_chroma_ss_y_ratio(chroma_ss).d;

	plane0_l = y4m_si_get_plane_length(&istream, 0);
	plane1_l = y4m_si_get_plane_length(&istream, 1);
	plane2_l = y4m_si_get_plane_length(&istream, 2);

	yuv[0] = malloc(plane0_l);
	if	(yuv[0] == NULL)
		mjpeg_error_exit1("malloc(%d) plane 0", plane0_l);
	yuv[1] = malloc(plane1_l);
	if	(yuv[1] == NULL)
		mjpeg_error_exit1(" malloc(%d) plane 1", plane1_l);
	yuv[2] = malloc(plane2_l);
	if	(yuv[2] == NULL)
		mjpeg_error_exit1(" malloc(%d) plane 2\n", plane2_l);

	while	(y4m_read_frame(fdin,&istream,&iframe,yuv) == Y4M_OK)
		{
		for	(i = 0, cp = yuv[0]; i < plane0_l; i++, cp++)
			y_stats[*cp]++; /* Y' */
		for	(i = 0, cp = yuv[1]; i < plane1_l; i++, cp++)
			u_stats[*cp]++;	/* U */
		for	(i = 0, cp = yuv[2]; i < plane2_l; i++, cp++)
			v_stats[*cp]++;	/* V */
		}

	y4m_fini_frame_info(&iframe);
	y4m_fini_stream_info(&istream);

	for	(i = 0; i < 255; i++)
		printf("Y %d %lld\n", i, y_stats[i]);
	for	(i = 0; i < 255; i++)
		printf("U %d %lld\n", i, u_stats[i]);
	for	(i = 0; i < 255; i++)
		printf("V %d %lld\n", i, v_stats[i]);
	exit(0);
	}
