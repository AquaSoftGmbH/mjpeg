#ifndef CODECS_H
#define CODECS_H

// =================================== codecs
// All new codecs store their variables in a quicktime_...._codec structure.
// The structure is statically allocated here even if the codec isn't used.

#include "dv.h"
#include "ima4.h"
#include "jpeg.h"
#include "qtpng.h"
#include "raw.h"
#include "rawaudio.h"
//#include "rtjpeg.h"
#include "twos.h"
#include "ulaw.h"
//#include "yuv9.h"
#include "yuv4.h"
#include "yuv2.h"
#include "wmx1.h"
#include "wmx2.h"

typedef struct
{
	quicktime_dv_codec_t dv_codec;
	quicktime_jpeg_codec_t jpeg_codec;
//	quicktime_yuv9_codec_t yuv9_codec;
	quicktime_yuv4_codec_t yuv4_codec;
	quicktime_yuv2_codec_t yuv2_codec;
	quicktime_png_codec_t png_codec;
	quicktime_raw_codec_t raw_codec;
//	quicktime_rtjpeg_codec_t rtjpeg_codec;
	quicktime_rawaudio_codec_t rawaudio_codec;
	quicktime_ima4_codec_t ima4_codec;
	quicktime_wmx2_codec_t wmx2_codec;
	quicktime_twos_codec_t twos_codec;
	quicktime_ulaw_codec_t ulaw_codec;
	quicktime_wmx1_codec_t wmx1_codec;
} quicktime_codecs_t;


#endif
