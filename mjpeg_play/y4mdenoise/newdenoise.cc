// This file (C) 2004 Steven Boswell.  All rights reserved.
// Released to the public under the GNU General Public License.
// See the file COPYING for more information.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include "mjpeg_types.h"
#include <stdio.h>
#include "newdenoise.hh"
#include "MotionSearcher.hh"

// The denoisers.  (We have to make these classes, in order to keep gdb
// from crashing.  I didn't even know one could crash gdb. ;-)
#if 0
typedef MotionSearcher<uint8_t, 1, int32_t, int16_t, int32_t, 4, 2,
		uint16_t, PixelY, ReferencePixelY, ReferenceFrameY>
	MotionSearcherY;
typedef MotionSearcher<uint8_t, 2, int32_t, int16_t, int32_t, 2, 2,
		uint16_t, PixelCbCr, ReferencePixelCbCr, ReferenceFrameCbCr>
	MotionSearcherCbCr;
#else
class MotionSearcherY
	: public MotionSearcher<uint8_t, 1, int32_t, int16_t, int32_t, 4, 2,
		uint16_t, PixelY, ReferencePixelY, ReferenceFrameY> {};
class MotionSearcherCbCr
	: public MotionSearcher<uint8_t, 2, int32_t, int16_t, int32_t, 2, 2,
		uint16_t, PixelCbCr, ReferencePixelCbCr, ReferenceFrameCbCr> {};
#endif
MotionSearcherY g_oMotionSearcherY;
MotionSearcherCbCr g_oMotionSearcherCbCr;

// Whether the denoisers should be used.
bool g_bMotionSearcherY;
bool g_bMotionSearcherCbCr;

// Pixel buffers, used to translate provided input into the form the
// denoiser needs.
int g_nPixelsY, g_nWidthY, g_nHeightY;
MotionSearcherY::Pixel_t *g_pPixelsY;
int g_nPixelsCbCr, g_nWidthCbCr, g_nHeightCbCr;
MotionSearcherCbCr::Pixel_t *g_pPixelsCbCr;

// An internal method to output a frame.
static void output_frame
	(const MotionSearcherY::ReferenceFrame_t *a_pFrameY,
	const MotionSearcherCbCr::ReferenceFrame_t *a_pFrameCbCr,
	uint8_t *a_pOutputY, uint8_t *a_pOutputCb, uint8_t *a_pOutputCr);
static void output_field
	(int a_nMask, const MotionSearcherY::ReferenceFrame_t *a_pFrameY,
	const MotionSearcherCbCr::ReferenceFrame_t *a_pFrameCbCr,
	uint8_t *a_pOutputY, uint8_t *a_pOutputCb, uint8_t *a_pOutputCr);

int newdenoise_init (int a_nFrames, int a_nWidthY, int a_nHeightY,
	int a_nWidthCbCr, int a_nHeightCbCr)
{
	Status_t eStatus;
		// An error that may occur.
	int nInterlace;
		// A factor to apply to frames/frame-height because of
		// interlacing.
	
	// No errors yet.
	eStatus = g_kNoError;

	// Save the width and height.
	g_nWidthY = a_nWidthY;
	g_nHeightY = a_nHeightY;
	g_nWidthCbCr = a_nWidthCbCr;
	g_nHeightCbCr = a_nHeightCbCr;

	// If the video is interlaced, that means the denoiser will see
	// twice as many frames, half their original height.
	nInterlace = (denoiser.interlaced) ? 2 : 1;

	// If intensity should be denoised, set it up.
	if (a_nWidthY != 0 && a_nHeightY != 0)
	{
		g_bMotionSearcherY = true;
		g_nPixelsY = a_nWidthY * a_nHeightY / nInterlace;
		g_pPixelsY = new MotionSearcherY::Pixel_t [g_nPixelsY];
		if (g_pPixelsY == NULL)
			return -1;
		g_oMotionSearcherY.Init (eStatus, nInterlace * a_nFrames,
			a_nWidthY, a_nHeightY / nInterlace,
			denoiser.radiusY, denoiser.radiusY,
			denoiser.thresholdY, denoiser.matchCountThrottle,
			denoiser.matchSizeThrottle);
		if (eStatus != g_kNoError)
		{
			delete[] g_pPixelsY;
			return -1;
		}
	}
	else
		g_bMotionSearcherY = false;

	// If color should be denoised, set it up.
	if (a_nWidthCbCr != 0 && a_nHeightCbCr != 0)
	{
		g_bMotionSearcherCbCr = true;
		g_nPixelsCbCr = a_nWidthCbCr * a_nHeightCbCr / nInterlace;
		g_pPixelsCbCr = new MotionSearcherCbCr::Pixel_t [g_nPixelsCbCr];
		if (g_pPixelsCbCr == NULL)
			return -1;
		g_oMotionSearcherCbCr.Init (eStatus, nInterlace * a_nFrames,
			a_nWidthCbCr, a_nHeightCbCr / nInterlace,
			denoiser.radiusCbCr / denoiser.frame.ss_h,
			denoiser.radiusCbCr / denoiser.frame.ss_v,
			denoiser.thresholdCbCr,
			denoiser.matchCountThrottle, denoiser.matchSizeThrottle);
		if (eStatus != g_kNoError)
			return -1;
	}
	else
		g_bMotionSearcherCbCr = false;

	// Initialization was successful.
	return 0;
}

int
newdenoise_frame (const uint8_t *a_pInputY, const uint8_t *a_pInputCb,
	const uint8_t *a_pInputCr, uint8_t *a_pOutputY,
	uint8_t *a_pOutputCb, uint8_t *a_pOutputCr)
{
	Status_t eStatus;
		// An error that may occur.
	const MotionSearcherY::ReferenceFrame_t *pFrameY;
	const MotionSearcherCbCr::ReferenceFrame_t *pFrameCbCr;
		// Denoised frame data, ready for output.
	int i;
		// Used to loop through pixels.

	// No errors yet.
	eStatus = g_kNoError;

	// No output frames have been received yet.
	pFrameY = NULL;
	pFrameCbCr = NULL;

	// If it's time to purge, do so.
	{
		extern int frame;
		if (frame % 10 == 0)
		{
			g_oMotionSearcherY.Purge();
			g_oMotionSearcherCbCr.Purge();
		}
	}

	// If the end of input has been reached, then return the next
	// remaining denoised frame, if available.
	if ((g_bMotionSearcherY && a_pInputY == NULL)
		|| (g_bMotionSearcherCbCr && a_pInputCr == NULL))
	{
		// Get any remaining frame.
		if (g_bMotionSearcherY)
			pFrameY = g_oMotionSearcherY.GetRemainingFrames();
		if (g_bMotionSearcherCbCr)
			pFrameCbCr = g_oMotionSearcherCbCr.GetRemainingFrames();
	
		// Output it.
		output_frame (pFrameY, pFrameCbCr, a_pOutputY, a_pOutputCb,
			a_pOutputCr);
	}

	// Otherwise, if there is more input, feed the frame into the
	// denoiser & possibly get a frame back.
	else
	{
		// Get any frame that's ready for output.
		if (g_bMotionSearcherY)
			pFrameY = g_oMotionSearcherY.GetFrameReadyForOutput();
		if (g_bMotionSearcherCbCr)
			pFrameCbCr = g_oMotionSearcherCbCr.GetFrameReadyForOutput();
	
		// Output it.
		output_frame (pFrameY, pFrameCbCr, a_pOutputY, a_pOutputCb,
			a_pOutputCr);
	
		// Pass the input frame to the denoiser.
		if (g_bMotionSearcherY)
		{
			// Convert the input frame into the format needed by the
			// denoiser.  (This step is a big waste of time & cache.
			// I wish there was another way.)
			for (i = 0; i < g_nPixelsY; ++i)
				g_pPixelsY[i] = PixelY (a_pInputY + i);

			// Pass the frame to the denoiser.
			g_oMotionSearcherY.AddFrame (eStatus, g_pPixelsY);
			if (eStatus != g_kNoError)
				return -1;
		}
		if (g_bMotionSearcherCbCr)
		{
			PixelCbCr::Num_t aCbCr[2];

			// Convert the input frame into the format needed by the
			// denoiser.  (This step is a big waste of time & cache.
			// I wish there was another way.)
			for (i = 0; i < g_nPixelsCbCr; ++i)
			{
				aCbCr[0] = a_pInputCb[i];
				aCbCr[1] = a_pInputCr[i];
				g_pPixelsCbCr[i] = PixelCbCr (aCbCr);
			}

			// Pass the frame to the denoiser.
			g_oMotionSearcherCbCr.AddFrame (eStatus, g_pPixelsCbCr);
			if (eStatus != g_kNoError)
				return -1;
		}
	}

	// If we're denoising both color & intensity, make sure we
	// either got two reference frames or none at all.
	assert (!g_bMotionSearcherY || !g_bMotionSearcherCbCr
		|| (pFrameY == NULL && pFrameCbCr == NULL)
		|| (pFrameY != NULL && pFrameCbCr != NULL));

	// Return whether there was an output frame this time.
	return (g_bMotionSearcherY && pFrameY != NULL
		|| g_bMotionSearcherCbCr && pFrameCbCr != NULL) ? 0 : 1;
}

static void output_frame
	(const MotionSearcherY::ReferenceFrame_t *a_pFrameY,
	const MotionSearcherCbCr::ReferenceFrame_t *a_pFrameCbCr,
	uint8_t *a_pOutputY, uint8_t *a_pOutputCb, uint8_t *a_pOutputCr)
{
	int i;
		// Used to loop through pixels.

	// Convert any denoised intensity frame into the format expected
	// by our caller.
	if (a_pFrameY != NULL)
	{
		ReferencePixelY *pY;
			// The pixel, as it's being converted to the output format.

		// Make sure our caller gave us somewhere to write output.
		assert (a_pOutputY != NULL);

		// Loop through all the pixels, convert them to the output
		// format.
		for (i = 0; i < g_nPixelsY; ++i)
		{
			pY = a_pFrameY->GetPixel (i);
			assert (pY != NULL);
			a_pOutputY[i] = pY->GetValue()[0];
		}
	}
	if (a_pFrameCbCr != NULL)
	{
		ReferencePixelCbCr *pCbCr;
			// The pixel, as it's being converted to the output format.

		// Make sure our caller gave us somewhere to write output.
		assert (a_pOutputCb != NULL && a_pOutputCr != NULL);

		// Loop through all the pixels, convert them to the output
		// format.
		for (i = 0; i < g_nPixelsCbCr; ++i)
		{
			pCbCr = a_pFrameCbCr->GetPixel (i);
			assert (pCbCr != NULL);
			const PixelCbCr &rCbCr = pCbCr->GetValue();
			a_pOutputCb[i] = rCbCr[0];
			a_pOutputCr[i] = rCbCr[1];
		}
	}
}

int
newdenoise_interlaced_frame (const uint8_t *a_pInputY,
	const uint8_t *a_pInputCb, const uint8_t *a_pInputCr,
	uint8_t *a_pOutputY, uint8_t *a_pOutputCb, uint8_t *a_pOutputCr)
{
	Status_t eStatus;
		// An error that may occur.
	const MotionSearcherY::ReferenceFrame_t *pFrameY;
	const MotionSearcherCbCr::ReferenceFrame_t *pFrameCbCr;
		// Denoised frame data, ready for output.
	int i, x, y;
		// Used to loop through pixels.
	int nMask;
		// Used to switch between top-field interlacing and bottom-field
		// interlacing.

	// No errors yet.
	eStatus = g_kNoError;

	// No output frames have been received yet.
	pFrameY = NULL;
	pFrameCbCr = NULL;

	// If it's time to purge, do so.
	{
		extern int frame;
		if (frame % 10 == 0)
		{
			g_oMotionSearcherY.Purge();
			g_oMotionSearcherCbCr.Purge();
		}
	}

	// Set up for the type of interlacing.
	nMask = (denoiser.interlaced == 2) ? 1 : 0;

	// If the end of input has been reached, then return the next
	// remaining denoised frame, if available.
	if ((g_bMotionSearcherY && a_pInputY == NULL)
		|| (g_bMotionSearcherCbCr && a_pInputCr == NULL))
	{
		// Get 1/2 any remaining frame.
		if (g_bMotionSearcherY)
			pFrameY = g_oMotionSearcherY.GetRemainingFrames();
		if (g_bMotionSearcherCbCr)
			pFrameCbCr = g_oMotionSearcherCbCr.GetRemainingFrames();
	
		// Output it.
		output_field (nMask ^ 0, pFrameY, pFrameCbCr, a_pOutputY,
			a_pOutputCb, a_pOutputCr);

		// Get 1/2 any remaining frame.
		if (g_bMotionSearcherY)
			pFrameY = g_oMotionSearcherY.GetRemainingFrames();
		if (g_bMotionSearcherCbCr)
			pFrameCbCr = g_oMotionSearcherCbCr.GetRemainingFrames();
	
		// Output it.
		output_field (nMask ^ 1, pFrameY, pFrameCbCr, a_pOutputY,
			a_pOutputCb, a_pOutputCr);
	}

	// Otherwise, if there is more input, feed the frame into the
	// denoiser & possibly get a frame back.
	else
	{
		// Get 1/2 any frame that's ready for output.
		if (g_bMotionSearcherY)
			pFrameY = g_oMotionSearcherY.GetFrameReadyForOutput();
		if (g_bMotionSearcherCbCr)
			pFrameCbCr = g_oMotionSearcherCbCr.GetFrameReadyForOutput();
	
		// Output it.
		output_field (nMask ^ 0, pFrameY, pFrameCbCr, a_pOutputY,
			a_pOutputCb, a_pOutputCr);
	
		// Pass the input frame to the denoiser.
		if (g_bMotionSearcherY)
		{
			// Convert the input frame into the format needed by the
			// denoiser.
			for (i = 0, y = (nMask ^ 0); y < g_nHeightY; y += 2)
				for (x = 0; x < g_nWidthY; ++x, ++i)
					g_pPixelsY[i]
						= PixelY (a_pInputY + (y * g_nWidthY + x));
			assert (i == g_nPixelsY);

			// Pass the frame to the denoiser.
			g_oMotionSearcherY.AddFrame (eStatus, g_pPixelsY);
			if (eStatus != g_kNoError)
				return -1;
		}
		if (g_bMotionSearcherCbCr)
		{
			PixelCbCr::Num_t aCbCr[2];

			// Convert the input frame into the format needed by the
			// denoiser.
			for (i = 0, y = (nMask ^ 0); y < g_nHeightCbCr; y += 2)
			{
				for (x = 0; x < g_nWidthCbCr; ++x, ++i)
				{
					aCbCr[0] = a_pInputCb[y * g_nWidthCbCr + x];
					aCbCr[1] = a_pInputCr[y * g_nWidthCbCr + x];
					g_pPixelsCbCr[i] = PixelCbCr (aCbCr);
				}
			}
			assert (i == g_nPixelsCbCr);

			// Pass the frame to the denoiser.
			g_oMotionSearcherCbCr.AddFrame (eStatus, g_pPixelsCbCr);
			if (eStatus != g_kNoError)
				return -1;
		}
	
		// Get 1/2 any frame that's ready for output.
		if (g_bMotionSearcherY)
			pFrameY = g_oMotionSearcherY.GetFrameReadyForOutput();
		if (g_bMotionSearcherCbCr)
			pFrameCbCr = g_oMotionSearcherCbCr.GetFrameReadyForOutput();
	
		// Output it.
		output_field (nMask ^ 1, pFrameY, pFrameCbCr, a_pOutputY,
			a_pOutputCb, a_pOutputCr);
	
		// Pass the input frame to the denoiser.
		if (g_bMotionSearcherY)
		{
			// Convert the input frame into the format needed by the
			// denoiser.
			for (i = 0, y = (nMask ^ 1); y < g_nHeightY; y += 2)
				for (x = 0; x < g_nWidthY; ++x, ++i)
					g_pPixelsY[i]
						= PixelY (a_pInputY + (y * g_nWidthY + x));
			assert (i == g_nPixelsY);

			// Pass the frame to the denoiser.
			g_oMotionSearcherY.AddFrame (eStatus, g_pPixelsY);
			if (eStatus != g_kNoError)
				return -1;
		}
		if (g_bMotionSearcherCbCr)
		{
			PixelCbCr::Num_t aCbCr[2];

			// Convert the input frame into the format needed by the
			// denoiser.
			for (i = 0, y = (nMask ^ 1); y < g_nHeightCbCr; y += 2)
			{
				for (x = 0; x < g_nWidthCbCr; ++x, ++i)
				{
					aCbCr[0] = a_pInputCb[y * g_nWidthCbCr + x];
					aCbCr[1] = a_pInputCr[y * g_nWidthCbCr + x];
					g_pPixelsCbCr[i] = PixelCbCr (aCbCr);
				}
			}
			assert (i == g_nPixelsCbCr);

			// Pass the frame to the denoiser.
			g_oMotionSearcherCbCr.AddFrame (eStatus, g_pPixelsCbCr);
			if (eStatus != g_kNoError)
				return -1;
		}
	}

	// If we're denoising both color & intensity, make sure we
	// either got two reference frames or none at all.
	assert (!g_bMotionSearcherY || !g_bMotionSearcherCbCr
		|| (pFrameY == NULL && pFrameCbCr == NULL)
		|| (pFrameY != NULL && pFrameCbCr != NULL));

	// Return whether there was an output frame this time.
	return (g_bMotionSearcherY && pFrameY != NULL
		|| g_bMotionSearcherCbCr && pFrameCbCr != NULL) ? 0 : 1;
}

static void output_field
	(int a_nMask, const MotionSearcherY::ReferenceFrame_t *a_pFrameY,
	const MotionSearcherCbCr::ReferenceFrame_t *a_pFrameCbCr,
	uint8_t *a_pOutputY, uint8_t *a_pOutputCb, uint8_t *a_pOutputCr)
{
	int i, x, y;
		// Used to loop through pixels.

	// Convert any denoised intensity frame into the format expected
	// by our caller.
	if (a_pFrameY != NULL)
	{
		ReferencePixelY *pY;
			// The pixel, as it's being converted to the output format.

		// Make sure our caller gave us somewhere to write output.
		assert (a_pOutputY != NULL);

		// Loop through all the pixels, convert them to the output
		// format.
		for (i = 0, y = a_nMask; y < g_nHeightY; y += 2)
		{
			for (x = 0; x < g_nWidthY; ++x, ++i)
			{
				pY = a_pFrameY->GetPixel (i);
				assert (pY != NULL);
				a_pOutputY[y * g_nWidthY + x] = pY->GetValue()[0];
			}
		}
	}
	if (a_pFrameCbCr != NULL)
	{
		ReferencePixelCbCr *pCbCr;
			// The pixel, as it's being converted to the output format.

		// Make sure our caller gave us somewhere to write output.
		assert (a_pOutputCb != NULL && a_pOutputCr != NULL);

		// Loop through all the pixels, convert them to the output
		// format.
		for (i = 0, y = a_nMask; y < g_nHeightCbCr; y += 2)
		{
			for (x = 0; x < g_nWidthCbCr; ++x, ++i)
			{
				pCbCr = a_pFrameCbCr->GetPixel (i);
				assert (pCbCr != NULL);
				const PixelCbCr &rCbCr = pCbCr->GetValue();
				a_pOutputCb[y * g_nWidthCbCr + x] = rCbCr[0];
				a_pOutputCr[y * g_nWidthCbCr + x] = rCbCr[1];
			}
		}
	}
}
