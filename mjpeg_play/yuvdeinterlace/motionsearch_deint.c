/***********************************************************
 * YUVDEINTERLACER for the mjpegtools                      *
 * ------------------------------------------------------- *
 * (C) 2001-2004 Stefan Fendt                              *
 *                                                         *
 * Licensed and protected by the GNU-General-Public-       *
 * License version 2 or if you prefer any later version of *
 * that license). See the file LICENSE for detailed infor- *
 * mation.                                                 *
 *                                                         *
 * FILE: motionsearch_deint.c                              *
 *                                                         *
 ***********************************************************/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "mjpeg_types.h"
#include "motionsearch_deint.h"
#include "mjpeg_logging.h"
#include "motionsearch.h"
#include "transform_block.h"
#include "vector.h"

int
median3 (int a, int b, int c)
{
  return ((a <= b && b <= c) ? b : (a <= c && c <= b) ? c : a);
}

uint32_t
median3_l (uint32_t a, uint32_t b, uint32_t c)
{
  return ((a <= b && b <= c) ? b : (a <= c && c <= b) ? c : a);
}

void
motion_compensate (uint8_t * r, uint8_t * f0, uint8_t * f1, uint8_t * f2,
		   int w, int h, int field)
{
  static int mv_fx[512][512];
  static int mv_fy[512][512];
  static uint32_t f_sum[512][512];
  static int mv_bx[512][512];
  static int mv_by[512][512];
  static uint32_t b_sum[512][512];

  int ix, iy;

  int x, y;
  int ox, oy;
  int dx, dy;
  int fx, fy;
  int bx, by;

  uint32_t sad;
  uint32_t fmin, bmin;
  int a, b, c, d, e, v, f;
  uint32_t thres;

  fx = fy = 0;

  for (y = 0; y < h; y += 8)
    for (x = 0; x < w; x += 8)
      {
	// create index numbers for motion-lookup-table
	ix = x / 8;
	iy = y / 8;

// -------------------------------------------------------------------------------------------------------- //
// -------------------------------------------------------------------------------------------------------- //
// ---[ start of field0-processing ]----------------------------------------------------------------------- //
// -------------------------------------------------------------------------------------------------------- //
// -------------------------------------------------------------------------------------------------------- //

	//if the index indicates that there do exist some previous motion-vectors
	// use them as candidates to do a fast estimation of the new ones...
	if (ix > 0 && iy > 0)
	  {
	    // define the early termination-threshold
	    thres =
	      median3_l (f_sum[ix - 1][iy - 1], f_sum[ix][iy - 1],
			 f_sum[ix - 1][iy]);

	    // its unlikely to find something better than Pixels_In_Block*Noise, so
	    // we limit 'thres' to the lower bound of 512. This is an extremly
	    // good match and even if it is not optimal, it is refined later, so we
	    // can leave all other estimates, if we find something below it...
	    thres = thres < 512 ? 512 : thres;

	    // test for the median-vector first (this usually is the best vector to be found...)
	    dx =
	      median3 (mv_fx[ix - 1][iy - 1], mv_fx[ix][iy - 1],
		       mv_fx[ix - 1][iy]);
	    dy =
	      median3 (mv_fy[ix - 1][iy - 1], mv_fy[ix][iy - 1],
		       mv_fy[ix - 1][iy]);
	    fx = dx;
	    fy = dy;

	    fmin = psad_sub22
	      (f1 + (x) + (y) * w, f0 + (x + dx) + (y + dy) * w, w, 8);

	    if (fmin > thres)	// Still not good enough...
	      {
		// test for the center
		dx = 0;
		dy = 0;

		sad = psad_sub22
		  (f1 + (x) + (y) * w, f0 + (x + dx) + (y + dy) * w, w, 8);

		if (sad < fmin)
		  {
		    fmin = sad;
		    fx = dx;
		    fy = dy;
		  }

		if (fmin > thres)	// Still not good enough...
		  {
		    // test for the left-neighbor
		    dx = mv_fx[ix - 1][iy];
		    dy = mv_fy[ix - 1][iy];

		    sad = psad_sub22
		      (f1 + (x) + (y) * w,
		       f0 + (x + dx) + (y + dy) * w, w, 8);

		    if (sad < fmin)
		      {
			fmin = sad;
			fx = dx;
			fy = dy;
		      }

		    if (fmin > thres)	// Still not good enough...
		      {
			// test for the top-neighbor
			dx = mv_fx[ix][iy - 1];
			dy = mv_fy[ix][iy - 1];

			sad = psad_sub22
			  (f1 + (x) + (y) * w,
			   f0 + (x + dx) + (y + dy) * w, w, 8);

			if (sad < fmin)
			  {
			    fmin = sad;
			    fx = dx;
			    fy = dy;
			  }

			if (fmin > thres)	// Still not good enough...
			  {
			    // test for the top-left-neighbor
			    dx = mv_fx[ix - 1][iy - 1];
			    dy = mv_fy[ix - 1][iy - 1];

			    sad = psad_sub22
			      (f1 + (x) + (y) * w,
			       f0 + (x + dx) + (y + dy) * w, w, 8);

			    if (sad < fmin)
			      {
				fmin = sad;
				fx = dx;
				fy = dy;
			      }

			    if (fmin > thres)	// Still not good enough...
			      {
				// test for the top-right-neighbor
				dx = mv_fx[ix + 1][iy - 1];
				dy = mv_fy[ix + 1][iy - 1];

				sad = psad_sub22
				  (f1 + (x) + (y) * w,
				   f0 + (x + dx) + (y + dy) * w, w, 8);

				if (sad < fmin)
				  {
				    fmin = sad;
				    fx = dx;
				    fy = dy;
				  }

				if (fmin > thres)	// Still not good enough...
				  {
				    // test for the mean-of-neighbors
				    dx = mv_fx[ix - 1][iy - 1];
				    dx += mv_fx[ix][iy - 1];
				    dx += mv_fx[ix + 1][iy - 1];
				    dx += mv_fx[ix - 1][iy];
				    dx /= 4;

				    dy = mv_fy[ix - 1][iy - 1];
				    dy += mv_fy[ix][iy - 1];
				    dy += mv_fy[ix + 1][iy - 1];
				    dy += mv_fy[ix - 1][iy];
				    dy /= 4;

				    sad = psad_sub22
				      (f1 + (x) + (y) * w,
				       f0 + (x + dx) + (y + dy) * w, w, 8);

				    if (sad < fmin)
				      {
					fmin = sad;
					fx = dx;
					fy = dy;
				      }

				    if (fmin > thres)	// Still not good enough...
				      {
					// test for the temporal-neighbor
					dx = mv_fx[ix][iy];
					dy = mv_fy[ix][iy];

					sad = psad_sub22
					  (f1 + (x) + (y) * w,
					   f0 + (x + dx) + (y + dy) * w, w,
					   8);

					if (sad < fmin)
					  {
					    fmin = sad;
					    fx = dx;
					    fy = dy;
					  }
				      }
				  }
			      }
			  }
		      }
		  }
	      }

	    if (fmin > thres)
	      {
		// the match is bad, do a large search arround the center.
		goto DO_LARGE_SEARCH;
	      }
	    else
	      {
		// the match is good, just do a small search arround to refine it.
		goto DO_SMALL_SEARCH;
	      }
	  }
	else
	  {
	    // as we don't have candidates, for these blocks on the image-rim, we do a multi-stage-search
	    // (to be replaced by something better...)
	    //
	    // Stage 1
	  DO_LARGE_SEARCH:
	    ox = oy = 0;	// center is the best guess we have, now
	    fmin = 0x00ffffff;	// reset min
	    for (dy = (oy - 8); dy <= (oy + 8); dy += 4)
	      for (dx = (ox - 8); dx <= (ox + 8); dx += 2)
		{
		  sad = psad_sub22
		    (f1 + (x) + (y) * w, f0 + (x + dx) + (y + dy) * w, w, 8);

		  if (sad < fmin)
		    {
		      fmin = sad;
		      fx = dx;
		      fy = dy;
		    }
		}
	    // Stage 2
	  DO_SMALL_SEARCH:
	    if (fmin > 256)
	      {
		ox = fx;	// take the best guess so far as the new center
		oy = fy;
		for (dy = (oy - 4); dy <= (oy + 4); dy += 2)
		  for (dx = (ox - 4); dx <= (ox + 4); dx += 1)
		    {
		      sad = psad_sub22
			(f1 + (x) + (y) * w,
			 f0 + (x + dx) + (y + dy) * w, w, 8);

		      if (sad < fmin)
			{
			  fmin = sad;
			  fx = dx;
			  fy = dy;
			}
		    }
	      }
	  }

	f_sum[ix][iy] = fmin;
	mv_fx[ix][iy] = fx;
	mv_fy[ix][iy] = fy;

// -------------------------------------------------------------------------------------------------------- //
// -------------------------------------------------------------------------------------------------------- //
// ---[ end of field0-processing ]------------------------------------------------------------------------- //
// -------------------------------------------------------------------------------------------------------- //
// -------------------------------------------------------------------------------------------------------- //

// -------------------------------------------------------------------------------------------------------- //
// -------------------------------------------------------------------------------------------------------- //
// ---[ start of field2-processing ]----------------------------------------------------------------------- //
// -------------------------------------------------------------------------------------------------------- //
// -------------------------------------------------------------------------------------------------------- //

	//if the index indicates that there do exist some previous motion-vectors
	// use them as candidates to do a fast estimation of the new ones...
	if (ix > 0 && iy > 0)
	  {
	    // define the early termination-threshold
	    thres =
	      median3_l (b_sum[ix - 1][iy - 1], b_sum[ix][iy - 1],
			 b_sum[ix - 1][iy]);

	    // its unlikely to find something better than Pixels_In_Block*Noise, so
	    // we limit 'thres' to the lower bound of 512. This is an extremly
	    // good match and even if it is not optimal, it is refined later, so we
	    // can leave all other estimates, if we find something below it...
	    thres = thres < 512 ? 512 : thres;

	    // test for the median-vector first (this usually is the best vector to be found...)
	    dx =
	      median3 (mv_bx[ix - 1][iy - 1], mv_bx[ix][iy - 1],
		       mv_bx[ix - 1][iy]);
	    dy =
	      median3 (mv_by[ix - 1][iy - 1], mv_by[ix][iy - 1],
		       mv_by[ix - 1][iy]);
	    bx = dx;
	    by = dy;

	    bmin = psad_sub22
	      (f1 + (x) + (y) * w, f2 + (x + dx) + (y + dy) * w, w, 8);

	    if (bmin > thres)	// Still not good enough...
	      {
		// test for the center
		dx = 0;
		dy = 0;

		sad = psad_sub22
		  (f1 + (x) + (y) * w, f2 + (x + dx) + (y + dy) * w, w, 8);

		if (sad < bmin)
		  {
		    bmin = sad;
		    bx = dx;
		    by = dy;
		  }

		if (bmin > thres)	// Still not good enough...
		  {
		    // test for the left-neighbor
		    dx = mv_bx[ix - 1][iy];
		    dy = mv_by[ix - 1][iy];

		    sad = psad_sub22
		      (f1 + (x) + (y) * w,
		       f2 + (x + dx) + (y + dy) * w, w, 8);

		    if (sad < bmin)
		      {
			bmin = sad;
			bx = dx;
			by = dy;
		      }

		    if (bmin > thres)	// Still not good enough...
		      {
			// test for the top-neighbor
			dx = mv_bx[ix][iy - 1];
			dy = mv_by[ix][iy - 1];

			sad = psad_sub22
			  (f1 + (x) + (y) * w,
			   f2 + (x + dx) + (y + dy) * w, w, 8);

			if (sad < bmin)
			  {
			    bmin = sad;
			    bx = dx;
			    by = dy;
			  }

			if (bmin > thres)	// Still not good enough...
			  {
			    // test for the top-left-neighbor
			    dx = mv_bx[ix - 1][iy - 1];
			    dy = mv_by[ix - 1][iy - 1];

			    sad = psad_sub22
			      (f1 + (x) + (y) * w,
			       f2 + (x + dx) + (y + dy) * w, w, 8);

			    if (sad < bmin)
			      {
				bmin = sad;
				bx = dx;
				by = dy;
			      }

			    if (bmin > thres)	// Still not good enough...
			      {
				// test for the top-right-neighbor
				dx = mv_bx[ix + 1][iy - 1];
				dy = mv_by[ix + 1][iy - 1];

				sad = psad_sub22
				  (f1 + (x) + (y) * w,
				   f2 + (x + dx) + (y + dy) * w, w, 8);

				if (sad < bmin)
				  {
				    bmin = sad;
				    bx = dx;
				    by = dy;
				  }

				if (bmin > thres)	// Still not good enough...
				  {
				    // test for the mean-of-neighbors
				    dx = mv_bx[ix - 1][iy - 1];
				    dx += mv_bx[ix][iy - 1];
				    dx += mv_bx[ix + 1][iy - 1];
				    dx += mv_bx[ix - 1][iy];
				    dx /= 4;

				    dy = mv_by[ix - 1][iy - 1];
				    dy += mv_by[ix][iy - 1];
				    dy += mv_by[ix + 1][iy - 1];
				    dy += mv_by[ix - 1][iy];
				    dy /= 4;

				    sad = psad_sub22
				      (f1 + (x) + (y) * w,
				       f2 + (x + dx) + (y + dy) * w, w, 8);

				    if (sad < bmin)
				      {
					bmin = sad;
					bx = dx;
					by = dy;
				      }

				    if (bmin > thres)	// Still not good enough...
				      {
					// test for the temporal-neighbor
					dx = mv_bx[ix][iy];
					dy = mv_by[ix][iy];

					sad = psad_sub22
					  (f1 + (x) + (y) * w,
					   f2 + (x + dx) + (y + dy) * w, w,
					   8);

					if (sad < bmin)
					  {
					    bmin = sad;
					    bx = dx;
					    by = dy;
					  }
				      }
				  }
			      }
			  }
		      }
		  }
	      }

	    if (bmin > thres)
	      {
		// the match is bad, do a large search arround the center.
		goto DO_LARGE_SEARCH_B;
	      }
	    else
	      {
		// the match is good, just do a small search arround to refine it.
		goto DO_SMALL_SEARCH_B;
	      }
	  }
	else
	  {
	    // as we don't have candidates, for these blocks on the image-rim, we do a multi-stage-search
	    // (to be replaced by something better...)
	    //
	    // Stage 1
	  DO_LARGE_SEARCH_B:
	    ox = oy = 0;	// center is the best guess we have, now
	    bmin = 0x00ffffff;	// reset min
	    for (dy = (oy - 8); dy <= (oy + 8); dy += 4)
	      for (dx = (ox - 8); dx <= (ox + 8); dx += 2)
		{
		  sad = psad_sub22
		    (f1 + (x) + (y) * w, f2 + (x + dx) + (y + dy) * w, w, 8);

		  if (sad < bmin)
		    {
		      bmin = sad;
		      bx = dx;
		      by = dy;
		    }
		}
	    // Stage 2
	  DO_SMALL_SEARCH_B:
	    if (bmin > 256)
	      {
		ox = bx;	// take the best guess so far as the new center
		oy = by;
		for (dy = (oy - 4); dy <= (oy + 4); dy += 2)
		  for (dx = (ox - 4); dx <= (ox + 4); dx += 1)
		    {
		      sad = psad_sub22
			(f1 + (x) + (y) * w,
			 f2 + (x + dx) + (y + dy) * w, w, 8);

		      if (sad < bmin)
			{
			  bmin = sad;
			  bx = dx;
			  by = dy;
			}
		    }
	      }
	  }

	b_sum[ix][iy] = bmin;
	mv_bx[ix][iy] = bx;
	mv_by[ix][iy] = by;

// -------------------------------------------------------------------------------------------------------- //
// -------------------------------------------------------------------------------------------------------- //
// ---[ end of field2-processing ]------------------------------------------------------------------------- //
// -------------------------------------------------------------------------------------------------------- //
// -------------------------------------------------------------------------------------------------------- //

	// clip invalid motion-vectors
	fx = (fx + x) > w ? 0 : fx;
	fx = (fx + x) < 0 ? 0 : fx;
	fy = (fy + y) > h ? 0 : fy;
	fy = (fy + y) < 0 ? 0 : fy;
	bx = (bx + x) > w ? 0 : bx;
	bx = (bx + x) < 0 ? 0 : bx;
	by = (by + y) > h ? 0 : by;
	by = (by + y) < 0 ? 0 : by;

	// process the matched block for video
	for (dy = 0; dy < 8; dy++)
	  for (dx = 0; dx < 8; dx++)
	    {
	      // we only need to reconstruct that lines missing in the current field
	      // the idea behind is, that the typical reconstruction-filter only lacks
	      // the high frequencies. We add these from the previous and past fields
	      // but leave all the other information intact.
              //
              // This will give a good reconstruction without visable motion-errors.
              // That is if the high-pass-information is out of phase it is graduately
	      // discarded, so in the worst case there just remains a linear-approximation
              // but where ever possible the full spectrum is reconstructed...
	      if (((dy + y) & 1) == field)
		{
		  // high-pass-information of previous field
		  a = 2 * *(f0 + (x + dx + fx) + (y + dy + fy) * w);
		  a -= *(f0 + (x + dx + fx) + (y + dy + fy - 1) * w);
		  a -= *(f0 + (x + dx + fx) + (y + dy + fy + 1) * w);
		  a /= 4;

		  // high-pass-information of next field
		  b = 2 * *(f2 + (x + dx + bx) + (y + dy + by) * w);
		  b -= *(f2 + (x + dx + bx) + (y + dy + by - 1) * w);
		  b -= *(f2 + (x + dx + bx) + (y + dy + by + 1) * w);
		  b /= 4;

		  // low-pass-information of current field
		  c = *(f1 + (x + dx) + (y + dy - 1) * w);
		  c += *(f1 + (x + dx) + (y + dy + 1) * w);
		  c /= 2;

		  // combine both HP-images with the LP-image
		  a = a + b + c;
		  a = a > 255 ? 255 : a;
		  a = a < 0 ? 0 : a;

		  *(r + (x + dx) + (y + dy) * w) = a;
		}
	      else
		{
		  *(r + (x + dx) + (y + dy) * w) =
		    *(f1 + (x + dx) + (y + dy) * w);
		}
	    }
      }
}
