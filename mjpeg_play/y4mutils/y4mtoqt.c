/*
 * $Id$
 *
 * Simple utility to place 4:2:2 YUV4MPEG2 data in a quicktime wrapper.   An
 * audio track can also be added by specifying '-a wavfile' (16bit pcm only).
 * The interlacing, frame rate, frame size and field order are extracted 
 * from the YUV4MPEG2 header.  This is the reverse of 'qttoy4m' which dumps
 * 4:2:2 planar data from a Quicktime 2vuy file (and optionally a specified
 * audio track to a wav file).
 *
 * Usage: y4mtoqt [-a wavfile] -o outputfile < 422yuv4mpeg2stream
*/

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <lqt.h>
#include <mjpegtools/yuv4mpeg.h>
#include <mjpegtools/avilib.h>

static	void	usage(void);
static void do_audio(quicktime_t *, uint8_t *, int, int,  int);

int
main(int argc, char **argv)
	{
	char	*outfilename = NULL;
	char	*audiofilename = NULL;
	u_char	*frame_buf, *yuv[3], *p, *Y_p, *Cb_p, *Cr_p;
	int	fdin, y_len, uv_len, nfields = 1, dominance = 0, afd = -1;
	int64_t	framesize;
	int	err, i, c, frames, channels = 0;
	char	*qtchroma;
	quicktime_t *qtf;
	quicktime_pasp_t pasp;
#if 0
	quicktime_colr_t colr;
#endif
	quicktime_clap_t clap;
	y4m_stream_info_t istream;
	y4m_frame_info_t iframe;
	y4m_ratio_t rate, sar;
	struct wave_header wavhdr;
	y4m_accept_extensions(1);
	
	fdin = fileno(stdin);

	opterr = 0;
	while	((c = getopt(argc, argv, "o:a:")) != -1)
		{
		switch	(c)
			{
			case	'o':
				outfilename = optarg;
				break;
			case	'a':
				audiofilename = optarg;
				break;
			case	'?':
			default:
				usage();
			}
		}
	argc -= optind;
	argv += optind;
	if	(argc)
		usage();

	if	(!outfilename)
		usage();

	if	(audiofilename)
		{
		afd = open(audiofilename, O_RDONLY);
		if	(afd < 0)
			mjpeg_error_exit1("Can not open audio file '%s'", 
						audiofilename);
		if	(AVI_read_wave_header(afd, &wavhdr) == -1)
			mjpeg_error_exit1("'%s' is not a valid WAV file",
						audiofilename);
		channels = wavhdr.common.wChannels;
		}

	y4m_init_stream_info(&istream);
	y4m_init_frame_info(&iframe);

	err = y4m_read_stream_header(fdin, &istream);
	if	(err != Y4M_OK)
		mjpeg_error_exit1("Input header error: %s", y4m_strerr(err));

	if	(y4m_si_get_plane_count(&istream) != 3)
		mjpeg_error_exit1("Only 3 plane formats supported");

	rate = y4m_si_get_framerate(&istream);

	switch	(y4m_si_get_chroma(&istream))
		{
		case	Y4M_CHROMA_422:
			qtchroma = QUICKTIME_2VUY;	/* 2vuy */
			break;
#ifdef	notnow
/*
 * Need to lookup the packing order, etc.  Not a commonly used
 * format so it's not critical to support.
*/
		case	Y4M_CHROMA_444:			/* v308 */
			qtchroma = QUICKTIME_YUV444;
			break;
#endif
		default:
			mjpeg_error_exit1("unsupported chroma sampling: %d",
				y4m_si_get_chroma(&istream));
			break;
		}

	y_len = y4m_si_get_plane_length(&istream, 0);
	uv_len = y4m_si_get_plane_length(&istream, 1);
	yuv[0] = malloc(y_len);
	yuv[1] = malloc(uv_len);
	yuv[2] = malloc(uv_len);

	framesize = y_len + uv_len + uv_len;
	frame_buf = malloc((size_t)framesize);

	qtf = quicktime_open(outfilename, 0, 1);
	if	(!qtf)
		mjpeg_error_exit1("quicktime_open(%s,0,1) failed", outfilename);

	quicktime_set_video(qtf, 1, y4m_si_get_width(&istream),
				y4m_si_get_height(&istream),
				(double) rate.n / rate.d,
				qtchroma);

	if	(audiofilename)
		quicktime_set_audio(qtf, channels,
					wavhdr.common.dwSamplesPerSec,
					wavhdr.common.wBitsPerSample,
					QUICKTIME_TWOS);
/*
 * http://developer.apple.com/quicktime/icefloe/dispatch019.html#fiel
 *
 * "dominance" is what Apple calls "detail".  From what I can figure out
 * the "bottom field" first corresponds to a "detail" setting of 14 and
 * "top field first" is a "detail" setting of 9.
*/
	switch	(y4m_si_get_interlace(&istream))
		{
		case	Y4M_ILACE_BOTTOM_FIRST:
			dominance = 14;	/* Weird but that's what Apple says */
			nfields = 2;
			break;
		case	Y4M_ILACE_TOP_FIRST:
			dominance = 9;
			nfields = 2;
			break;
		case	Y4M_UNKNOWN:
		case	Y4M_ILACE_NONE:
			dominance = 0;
			nfields = 1;
			break;
		case	Y4M_ILACE_MIXED:
			mjpeg_error_exit1("Mixed field dominance unsupported");
			break;
		default:
			mjpeg_error_exit1("UNKNOWN field dominance %d",
				y4m_si_get_interlace(&istream));
			break;
		}

	if	(lqt_set_fiel(qtf, 0, nfields, dominance) == 0)
		mjpeg_error_exit1("lqt_set_fiel(qtf, 0, %d, %d) failed",
				nfields, dominance);

	sar = y4m_si_get_sampleaspect(&istream);
	if	(Y4M_RATIO_EQL(sar, y4m_sar_UNKNOWN))
		pasp.hSpacing = pasp.vSpacing = 1;
	else
		{
		pasp.hSpacing = sar.n;
		pasp.vSpacing = sar.d;
		}
	if	(lqt_set_pasp(qtf, 0, &pasp) == 0)
		mjpeg_error_exit1("lqt_set_pasp(qtf, 0, %d/%d) failed",
			pasp.hSpacing, pasp.vSpacing);

/*
 * Don't do this for now - it can crash FinalCutPro if the colr atom is
 * not exactly correct.
*/
#if	0
	colr.colorParamType = 'nclc';
	colr.primaries = 2;
	colr.transferFunction = 2;
	colr.matrix = 2;
	if	(lqt_set_colr(qtf, 0, &colr) == 0)
		mjpeg_error_exit1("lqt_set_colr(qtf, 0,...) failed");
#endif
	clap.cleanApertureWidthN = y4m_si_get_width (&istream);;
	clap.cleanApertureWidthD = 1;
	clap.cleanApertureHeightN = y4m_si_get_height (&istream);
	clap.cleanApertureHeightD = 1;
	clap.horizOffN = 0;
	clap.horizOffD = 1;
	clap.vertOffN = 0;
	clap.vertOffD = 1;
	if	(lqt_set_clap(qtf, 0, &clap) == 0)
		mjpeg_error_exit1("lqt_set_clap(qtf, 0, ...) failed");

	for	(;y4m_read_frame(fdin,&istream,&iframe,yuv) == Y4M_OK; frames++)
		{
		p = frame_buf;
		Y_p = yuv[0];
		Cb_p = yuv[1];
		Cr_p = yuv[2];
		for	(i = 0; i < framesize; i += 4)
			{
			*p++ = *Cb_p++;
			*p++ = *Y_p++;
			*p++ = *Cr_p++;
			*p++ = *Y_p++;
			}
		if	(quicktime_write_frame(qtf, frame_buf, framesize, 0))
			mjpeg_error_exit1("quicktime_write_frame failed.");
		}

	if	(audiofilename)
		{
		uint8_t *buffer;
		int bufsize, n, bps;

		mjpeg_info("channels %d SamplesPerSec %d bits_sample %d",
			channels, wavhdr.common.dwSamplesPerSec,
				  wavhdr.common.wBitsPerSample);

		bps = (wavhdr.common.wBitsPerSample + 7)/8;
		bufsize = 8192 * channels * bps;
		buffer = malloc(bufsize);
		while	((n = AVI_read_wave_pcm_data(afd, buffer, bufsize)) > 0)
			do_audio(qtf, buffer, channels, bps, n / (channels * bps));
		}
	quicktime_close(qtf);
	exit(0);
	}

static void 
do_audio(quicktime_t *qtf, uint8_t *buff, int channels, int bps, int samps)
	{
	int	res;
	int	i, j;
	int16_t *qt_audio = (int16_t *)buff, **qt_audion;

mjpeg_info("channels %d bps %d samps %d", channels, bps, samps);

	qt_audion = malloc(channels * sizeof (int16_t **));
	for	(i = 0; i < channels; i++)
		qt_audion[i] = (int16_t *)malloc(samps * bps);

	/* Deinterleave the audio into separate channel buffers */
	for	(i = 0; i < samps; i++)
		{
		for	(j = 0; j < channels; j++)
			qt_audion[j][i] = qt_audio[(channels*i) + j];
		}
	res = lqt_encode_audio_track(qtf, qt_audion, NULL, samps, 0);
	for	(j = 0; j < channels; j++)
		free(qt_audion[j]);
	free(qt_audion);
	}

static void
usage()
	{
	mjpeg_error_exit1("usage: [-a inputwavfile] -o outfile");
	}
