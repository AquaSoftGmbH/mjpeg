/* build_sub44_mests.c, this file is part of the
 * AltiVec optimized library for MJPEG tools MPEG-1/2 Video Encoder
 * Copyright (C) 2002  James Klicman <james@klicman.org>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_ALTIVEC_H
#include <altivec.h>
#endif

#include "altivec_motion.h"
#include "vectorize.h"
#include "../fastintfns.h"
#include "../mjpeg_logging.h"

/* #define AMBER_ENABLE */
#include "amber.h"

/* Do multi-row processing? This will generate slightly different results than
 * the C version. Initial tests indicate better results with multi-row
 * processing (and faster too, of course). Since the results are different than
 * the C version, the verification code will generate checksum error messages.
 * A new method of verifying the results needs to be implemented.
 */
#define MULTI_ROW 16

/* C skim rule */
#define SKIM(weight,threshold) (weight < threshold)

/* MMX skim rule */
/* #define SKIM(weight,threshold) (weight <= threshold) */


/* Rough and-ready absolute distance penalty
 * NOTE: This penalty is *vital* to correct operation
 * as otherwise the sub-mean filtering won't work on very
 * uniform images.
 */
/* C penalty calculation */
#define DISTANCE_PENALTY(x,y) (intmax(intabs(x - i0),intabs(y - j0))<<1)

/* MMX penalty calculation */
/* #define DISTANCE_PENALTY(x,y) (intmax(intabs(x),intabs(y))<<2) */

/* old MMX penalty calculation */
/* #define DISTANCE_PENALTY(x,y) (intabs(x)+intabs(y)) */


/* Do threshold lookahead? This also generates different results than
 * the C version, also slightly worse output. (should be faster though)
 */
#undef THRESHOLD_LOOKAHEAD
#define THRESHOLD

#ifdef THRESHOLD_LOOKAHEAD
#define UPDATE_THRESHOLD
#else
#  ifdef THRESHOLD
#    define UPDATE_THRESHOLD  threshold = intmin(weight<<2, threshold)
#  else
#    define UPDATE_THRESHOLD
#  endif
#endif

#ifdef THRESHOLD
#define SKIM_RESULT(i,Y) /* {{{ */                                           \
	if (SKIM(weight, threshold)) {                                       \
	  UPDATE_THRESHOLD;                                                  \
	  mres.weight = (uint16_t)(weight + DISTANCE_PENALTY(x,Y));          \
	  mres.x = (int8_t)x;                                                \
	  mres.y = (int8_t)Y;                                                \
	  *cres = mres;                                                      \
	  cres++;                                                            \
	}                                                                    \
	/* }}} */
#else
#define SKIM_RESULT(i,Y) /* {{{ */                                           \
	  mres.weight = (uint16_t)(weight + DISTANCE_PENALTY(x,Y));          \
	  mres.x = (int8_t)x;                                                \
	  mres.y = (int8_t)Y;                                                \
	  *cres = mres;                                                      \
	  cres++;                                                            \
	/* }}} */
#endif




#if 0
/* addition benchmark info {{{ */
#if defined(ALTIVEC_BENCHMARK) && ALTIVEC_TEST_FUNCTION(build_sub44_mests)
#define BUILD_SUB44_MESTS_BENCHMARK
static int xlow, xhigh, ylow, yhigh;
static unsigned long x15, x16, x31, x32, x47, x48, x63, x64, x79, x80, x95;
static unsigned long y15, y16, y31, y32, y47, y48, y63, y64, y79, y80, y95;
#endif /* }}} */
#endif


static inline vector unsigned char
splat_p32(register uint8_t *pblk, register vector unsigned char perm)
{
    vector unsigned int v;
    v = vec_ld(0, (unsigned int*)pblk);
    v = vec_perm(v, v, perm);
    v = vec_splat(v, 0);
    return (vector unsigned char)v;
}

static inline vector unsigned char
ld16_perm(register uint8_t *pblk, register vector unsigned char vl, 
          register vector unsigned char perm)
{
    vector unsigned char v;
    v = vec_ld(16, (unsigned char*)pblk);
    v = vec_perm(vl, v, perm);
    return v;
}


/* find min value of vector and splat it to result
 * Note: v can be also used for min
 * TODO: replace vec_sro with vec_sld
 */
#define vis_min_u32(v,min,t1) \
    vu8(t1) = vec_splat_u8(4); \
    vu8(t1) = vec_sl(vu8(t1), vu8(t1)); /* t1 = (64) */ \
    vu8(t1) = vec_sro(vu8(v), vu8(t1)); \
    vu32(t1) = vec_min(vu32(v), vu32(t1)); \
    vu32(min) = vec_splat(vu32(t1), 2); \
    vu32(t1) = vec_splat(vu32(t1), 3); \
    vu32(min) = vec_min(vu32(min), vu32(t1)); \


/*
 *  s44org % 16 == 0
 *  s44blk % 4 == 0
 *  h == 2 or 4
 *  (rowstride % 16) == 0
 *  (ihigh-ilow)+1 % 16 == 0 very often
 *  (jhigh-jlow)+1 % 16 == 0 very often
 */
#define BUILD_SUB44_MESTS_PDECL /* {{{ */                                    \
  me_result_set *sub44set,                                                   \
  int ilow, int jlow, int ihigh, int jhigh,                                  \
  int i0, int j0,                                                            \
  int null_ctl_sad,                                                          \
  uint8_t *s44org, uint8_t *s44blk,                                          \
  int rowstride, int h,                                                      \
  int reduction                                                              \
  /* }}} */

#define BUILD_SUB44_MESTS_ARGS /* {{{ */                                     \
  sub44set,                                                                  \
  ilow, jlow, ihigh, jhigh,                                                  \
  i0, j0,                                                                    \
  null_ctl_sad,                                                              \
  s44org, s44blk,                                                            \
  rowstride, h,                                                              \
  reduction                                                                  \
  /* }}} */


/* int build_sub44_mests_altivec(BUILD_SUB44_MESTS_PDECL) {{{ */
#if defined(ALTIVEC_VERIFY) && ALTIVEC_TEST_FUNCTION(build_sub44_mests)
#define VERIFY_BUILD_SUB44_MESTS

#define VERIFY_SADS(orgblk,s44blk,rowstride,h,sads,count) if (verify) \
      { verify_sads(orgblk,s44blk,rowstride,h,sads,count); }

/* declarations */
static int _build_sub44_mests_altivec(BUILD_SUB44_MESTS_PDECL, int verify);
static void verify_sads(unsigned char *orgblk, unsigned char* s44blk,
			int rowstride, int h, unsigned int *sads, int count);

int build_sub44_mests_altivec(BUILD_SUB44_MESTS_PDECL)
{
  return _build_sub44_mests_altivec(BUILD_SUB44_MESTS_ARGS, 0 /* no verify */);
}

static int _build_sub44_mests_altivec(BUILD_SUB44_MESTS_PDECL, int verify)
#else
#define VERIFY_SADS(orgblk,s44blk,rowstride,h,sads,count) /* no verify */

int build_sub44_mests_altivec(BUILD_SUB44_MESTS_PDECL)
#endif
/* }}} */
{
  int x, y;
  int yN, yNlow, yhighN, yhigh;
  int xlow, xhigh;
  int loadmark;
  int rowstrideN;
  uint8_t *curblk, *pblk;
  uint8_t *currowblk;
  int threshold;
  int weight;
  int mean_weight;
  me_result_s *cres = sub44set->mests;
  me_result_s mres;
  vector unsigned char t1, t2, t3, t4, t5, perm;
  vector unsigned char shift, shifter, increment;
  vector unsigned char vr0, vr1, vr2, vr3;
  vector unsigned char vy0, vy1, vy2,  vy3,  vy4,  vy5,  vy6,  vy7;
  vector unsigned char vy8, vy9, vy10, vy11, vy12, vy13, vy14, vy15;
  vector unsigned char vy16;
  vector signed int sads0, sads1;
  union {
    vector signed int v[4];
    signed int i[16];
  } sads;
  unsigned int *psad;
  int sy;

#ifdef ALTIVEC_VERIFY /* {{{ */
  if (NOT_VECTOR_ALIGNED(s44org))
    mjpeg_error_exit1("build_sub44_mests: s44org %% 16 != 0 (0x%X)", s44org);

  if (((unsigned long)s44blk) & 0x3 != 0)
    mjpeg_error_exit1("build_sub44_mests: s44blk %% 4 != 0 (0x%X)", s44blk);

  if (NOT_VECTOR_ALIGNED(rowstride))
    mjpeg_error_exit1("build_sub44_mests: rowstride %% 16 != 0 (%d)",
	rowstride);

  if (h != 2 && h != 4)
    mjpeg_error_exit1("build_sub44_mests: h != [2|4], (%d)", h);
#endif /* }}} */

#ifdef BUILD_SUB44_MESTS_BENCHMARK /* {{{ */
  xlow = ilow-i0;
  ylow = jlow-j0;
  xhigh = ihigh-i0;
  yhigh = jhigh-j0;

  switch (ihigh-ilow) {
    case 15: x15++; break;
    case 16: x16++; break;
    case 31: x31++; break;
    case 32: x32++; break;
    case 47: x47++; break;
    case 48: x48++; break;
    case 63: x63++; break;
    case 64: x64++; break;
    case 79: x79++; break;
    case 80: x80++; break;
    case 95: x95++; break;
    default:
      mjpeg_debug("build_sub44_mests: ihigh-ilow = %d", ihigh-ilow);
  }

  switch (jhigh-jlow) {
    case 15: y15++; break;
    case 16: y16++; break;
    case 31: y31++; break;
    case 32: y32++; break;
    case 47: y47++; break;
    case 48: y48++; break;
    case 63: y63++; break;
    case 64: y64++; break;
    case 79: y79++; break;
    case 80: y80++; break;
    case 95: y95++; break;
    default:
      mjpeg_debug("build_sub44_mests: jhigh-jlow = %d", jhigh-jlow);
  }
#endif /* }}} */

  AMBER_START;

  threshold = 6*null_ctl_sad / (4*4*reduction);
  currowblk = s44org+(ilow>>2)+rowstride*(jlow>>2);

  /* initialize shift {{{
   * this will be used to increment the permutation mask that shifts the data.
   * 0x( 00 01 02 03 | 10 11 12 13 | 04 05 06 07 | 14 15 16 17 )
   */
  t1 = vec_lvsl(0, (unsigned char*)0);
  t2 = vec_lvsr(0, (unsigned char*)0);
  shift = vu8(vec_mergeh(vu32(t1) /*lvsl*/, vu32(t2) /*lvsr*/));
  increment = vec_splat_u8(1);
  /* }}} */

  y     = jlow  - j0;
  yNlow = y - 1;
  yN    = jhigh - jlow;
  yhigh = jhigh - j0;
  xlow  = ilow  - i0;
  xhigh = ihigh - i0;

  if (h < 4) {
    /* {{{ */
    pblk = s44blk;
    perm = vec_lvsl(0, (unsigned int*)pblk);
    vr0 = splat_p32(pblk, perm); pblk += rowstride;
    vr1 = splat_p32(pblk, perm);

    /* 
     * AC-- =  A~C  == (A0, C0, A1, C1)
     * BD-- =  B~D  == (B0, D0, B1, D1)
     * CE-- =  C~E  == (C0, E0, C1, E1)
     * ABCD = AC~BD == (A0, B0, C0, D0)
     * BCDE = BD~CE == (B0, C0, D0, E0)
     *
     * sad  = ABCD~vr0
     * sad += BCDE~vr1
     */
#define SAD(sad,v0,v1,v2,v3,v4) /* {{{ */                                    \
    t1 = vec_perm(v0, v2, shifter); /* AC-- = A~C == (A0,C0,A1,C1) */        \
    t2 = vec_perm(v1, v3, shifter); /* BD-- = B~D == (B0,D0,B1,D1) */        \
    t3 = vec_perm(v2, v4, shifter); /* CE-- = C~E == (C0,E0,C1,E1) */        \
								             \
    vu32(t1) = vec_mergeh(vu32(t1), vu32(t2)); /* ABCD = AC~BD */            \
    vu32(t3) = vec_mergeh(vu32(t2), vu32(t3)); /* BCDE = BD~CE */            \
								             \
    /* calculate sad */                                                      \
    vu8(sad) = vec_splat_u8(0); /* zero sad                      */          \
    t2  = vec_max(t1, vr0);     /* find largest of two           */          \
    t4  = vec_min(t1, vr0);     /* find smaller of two           */          \
    t2  = vec_sub(t2, t4);      /* find absolute difference      */          \
    vu32(sad) = vec_sum4s(t2, vu32(sad)); /* accumulate          */          \
								             \
    t2  = vec_max(t3, vr1);     /* find largest of two           */          \
    t4  = vec_min(t3, vr1);     /* find smaller of two           */          \
    t2  = vec_sub(t2, t4);      /* find absolute difference      */          \
    vu32(sad) = vec_sum4s(t2, vu32(sad)); /* accumulate          */          \
    /* }}} */

#if MULTI_ROW >= 16 /* {{{ */
    /* Calculate 16 rows at a time
     */

    yhighN = yNlow + (yN & (~0x3f));
    if (y <= yhighN)
    {
      rowstrideN = rowstride << 4;
      do
      {
	curblk = currowblk;

	for (x = xlow; x <= xhigh;)
	{
	  /* load a 16*2 block of 4*4 pels for every 12 iterations */

	  /* load vy0-16 {{{ */
	  pblk = curblk;
	  vy0 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy1 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy2 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy3 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy4 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy5 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy6 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy7 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy8 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy9 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy10 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy11 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy12 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy13 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy14 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy15 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy16 = vec_ld(0, (unsigned char*)pblk);

	  if (NOT_VECTOR_ALIGNED(curblk)) {
	    pblk = curblk;
	    perm = vec_lvsl(0, (unsigned char*)pblk);
	    vy0 = ld16_perm(pblk, vy0, perm); pblk += rowstride;
	    vy1 = ld16_perm(pblk, vy1, perm); pblk += rowstride;
	    vy2 = ld16_perm(pblk, vy2, perm); pblk += rowstride;
	    vy3 = ld16_perm(pblk, vy3, perm); pblk += rowstride;
	    vy4 = ld16_perm(pblk, vy4, perm); pblk += rowstride;
	    vy5 = ld16_perm(pblk, vy5, perm); pblk += rowstride;
	    vy6 = ld16_perm(pblk, vy6, perm); pblk += rowstride;
	    vy7 = ld16_perm(pblk, vy7, perm); pblk += rowstride;
	    vy8 = ld16_perm(pblk, vy8, perm); pblk += rowstride;
	    vy9 = ld16_perm(pblk, vy9, perm); pblk += rowstride;
	    vy10 = ld16_perm(pblk, vy10, perm); pblk += rowstride;
	    vy11 = ld16_perm(pblk, vy11, perm); pblk += rowstride;
	    vy12 = ld16_perm(pblk, vy12, perm); pblk += rowstride;
	    vy13 = ld16_perm(pblk, vy13, perm); pblk += rowstride;
	    vy14 = ld16_perm(pblk, vy14, perm); pblk += rowstride;
	    vy15 = ld16_perm(pblk, vy15, perm); pblk += rowstride;
	    vy16 = ld16_perm(pblk, vy16, perm);
	  }
	  /* }}} */

	  shifter = shift; /* reset shifter */

	  loadmark = intmin(x + (4 * (12 - 1)), xhigh);
	  do
	  {
	    SAD(sads0,vy0,vy1,vy2,vy3,vy4);
	    vec_st(sads0, 0, &sads.v[0]);

	    SAD(sads1,vy4,vy5,vy6,vy7,vy8);
	    vec_st(sads1, 0, &sads.v[1]);

#ifdef THRESHOLD_LOOKAHEAD /* {{{ */
	    /* temporarily store the lowest sads in perm */
	    vu32(perm) = vec_min(sads0, sads1);
#endif /* }}} */

	    SAD(sads0,vy8,vy9,vy10,vy11,vy12);
	    vec_st(sads0, 0, &sads.v[2]);

	    SAD(sads1,vy12,vy13,vy14,vy15,vy16);
	    vec_st(sads1, 0, &sads.v[3]);

	    /* increment permute for next iteration */
	    shifter = vec_add(shifter, increment);

#ifdef THRESHOLD_LOOKAHEAD /* {{{ */
	    /* find lowest sad for potential new threshold.
	     * doing this here allows 16 sad lookahead for threshold skimming.
	     */
	    vu32(t1) = vec_min(sads0, sads1);
	    vu32(t2) = vec_min(vu32(perm) /*first set*/, vu32(t1));
	    vis_min_u32(t2, t1, t3); /* t2=input, t1=output, t3=temp */
	    /* store lowest sad to weight */
	    vec_ste(vu32(t1), 0, (unsigned int*)&weight);

	    threshold = intmin(weight<<2, threshold);
#endif /* }}} */

	    VERIFY_SADS(curblk, s44blk, rowstride, h, sads.i, 16);

	    psad = sads.i; sy = y; weight = *psad;
	    SKIM_RESULT(0,sy);  psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(1,sy);  psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(2,sy);  psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(3,sy);  psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(4,sy);  psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(5,sy);  psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(6,sy);  psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(7,sy);  psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(8,sy);  psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(9,sy);  psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(10,sy); psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(11,sy); psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(12,sy); psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(13,sy); psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(14,sy); psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(15,sy);              

	    curblk++;

	    x += 4;
	  } while (x <= loadmark);
	}
	currowblk += rowstrideN;

	y += (4 * 16);
      } while (y <= yhighN);
    }
#endif /* }}} */

#if MULTI_ROW >= 12 /* {{{ */
    /* Calculate 12 rows at a time
     */

    yhighN = yNlow + ((yN / (4*12)) * (4*12));
    if (y <= yhighN)
    {
      rowstrideN = rowstride * 12;
      do
      {
	curblk = currowblk;

	for (x = xlow; x <= xhigh;)
	{
	  /* load a 16*2 block of 4*4 pels for every 12 iterations */

	  /* load vy0-12 {{{ */
	  pblk = curblk;
	  vy0 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy1 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy2 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy3 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy4 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy5 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy6 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy7 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy8 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy9 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy10 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy11 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy12 = vec_ld(0, (unsigned char*)pblk);

	  if (NOT_VECTOR_ALIGNED(curblk)) {
	    pblk = curblk;
	    perm = vec_lvsl(0, (unsigned char*)pblk);
	    vy0 = ld16_perm(pblk, vy0, perm); pblk += rowstride;
	    vy1 = ld16_perm(pblk, vy1, perm); pblk += rowstride;
	    vy2 = ld16_perm(pblk, vy2, perm); pblk += rowstride;
	    vy3 = ld16_perm(pblk, vy3, perm); pblk += rowstride;
	    vy4 = ld16_perm(pblk, vy4, perm); pblk += rowstride;
	    vy5 = ld16_perm(pblk, vy5, perm); pblk += rowstride;
	    vy6 = ld16_perm(pblk, vy6, perm); pblk += rowstride;
	    vy7 = ld16_perm(pblk, vy7, perm); pblk += rowstride;
	    vy8 = ld16_perm(pblk, vy8, perm); pblk += rowstride;
	    vy9 = ld16_perm(pblk, vy9, perm); pblk += rowstride;
	    vy10 = ld16_perm(pblk, vy10, perm); pblk += rowstride;
	    vy11 = ld16_perm(pblk, vy11, perm); pblk += rowstride;
	    vy12 = ld16_perm(pblk, vy12, perm);
	  }
	  /* }}} */

	  shifter = shift; /* reset shifter */

	  loadmark = intmin(x + (4 * (12 - 1)), xhigh);
	  do
	  {
	    SAD(sads0,vy0,vy1,vy2,vy3,vy4);
	    vec_st(sads0, 0, &sads.v[0]);

	    SAD(sads1,vy4,vy5,vy6,vy7,vy8);
	    vec_st(sads1, 0, &sads.v[1]);

#ifdef THRESHOLD_LOOKAHEAD /* {{{ */
	    /* temporarily store the lowest sads in perm */
	    vu32(perm) = vec_min(sads0, sads1);
#endif /* }}} */

	    SAD(sads0,vy8,vy9,vy10,vy11,vy12);
	    vec_st(sads0, 0, &sads.v[2]);

	    /* increment permute for next iteration */
	    shifter = vec_add(shifter, increment);

#ifdef THRESHOLD_LOOKAHEAD /* {{{ */
	    /* find lowest sad for potential new threshold.
	     * doing this here allows 16 sad lookahead for threshold skimming.
	     */
	    vu32(t1) = vec_min(sads0, vu32(perm));
	    vis_min_u32(t1, t2, t3); /* t1=input, t2=output, t3=temp */
	    /* store lowest sad to weight */
	    vec_ste(vu32(t2), 0, (unsigned int*)&weight);

	    threshold = intmin(weight<<2, threshold);
#endif /* }}} */

	    VERIFY_SADS(curblk, s44blk, rowstride, h, sads.i, 12);

	    psad = sads.i; sy = y; weight = *psad;
	    SKIM_RESULT(0,sy);  psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(1,sy);  psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(2,sy);  psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(3,sy);  psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(4,sy);  psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(5,sy);  psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(6,sy);  psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(7,sy);  psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(8,sy);  psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(9,sy);  psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(10,sy); psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(11,sy);

	    curblk++;

	    x += 4;
	  } while (x <= loadmark);
	}
	currowblk += rowstrideN;

	y += (4 * 12);
      } while (y <= yhighN);

      /* skip over multi-row 8, this must be done since we've just finished
       * processing rows by 12==8+4. Y could be offset by 4 causing the
       * multi-row 8 to process extra rows.
       */
      goto h2_multi_row_4;
    }
#endif /* }}} */

#if MULTI_ROW >= 8 /* {{{ */
    /* Calculate 8 rows at a time
     */
    yhighN = yNlow + (yN & (~0x1f));
    if (y <= yhighN)
    {
      rowstrideN = rowstride << 3;
      do
      {
	curblk = currowblk;

	for (x = xlow; x <= xhigh;)
	{
	  /* load a 16*2 block of 4*4 pels for every 12 iterations */

	  /* load vy0-8 {{{ */
	  pblk = curblk;
	  vy0 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy1 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy2 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy3 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy4 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy5 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy6 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy7 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy8 = vec_ld(0, (unsigned char*)pblk);

	  if (NOT_VECTOR_ALIGNED(curblk)) {
	    pblk = curblk;
	    perm = vec_lvsl(0, (unsigned char*)pblk);
	    vy0 = ld16_perm(pblk, vy0, perm); pblk += rowstride;
	    vy1 = ld16_perm(pblk, vy1, perm); pblk += rowstride;
	    vy2 = ld16_perm(pblk, vy2, perm); pblk += rowstride;
	    vy3 = ld16_perm(pblk, vy3, perm); pblk += rowstride;
	    vy4 = ld16_perm(pblk, vy4, perm); pblk += rowstride;
	    vy5 = ld16_perm(pblk, vy5, perm); pblk += rowstride;
	    vy6 = ld16_perm(pblk, vy6, perm); pblk += rowstride;
	    vy7 = ld16_perm(pblk, vy7, perm); pblk += rowstride;
	    vy8 = ld16_perm(pblk, vy8, perm);
	  }
	  /* }}} */

	  shifter = shift; /* reset shifter */

	  loadmark = intmin(x + (4 * (12 - 1)), xhigh);
	  do
	  {
	    SAD(sads0,vy0,vy1,vy2,vy3,vy4);
	    vec_st(sads0, 0, &sads.v[0]);

	    SAD(sads1,vy4,vy5,vy6,vy7,vy8);
	    vec_st(sads1, 0, &sads.v[1]);

	    /* increment permute for next iteration */
	    shifter = vec_add(shifter, increment);

#ifdef THRESHOLD_LOOKAHEAD /* {{{ */
	    /* find lowest sad for potential new threshold. */
	    vu32(t1) = vec_min(sads0, sads1);
	    vis_min_u32(t1, t2, t3); /* t1=input, t2=output, t3=temp */
	    /* store lowest sad to weight */
	    vec_ste(vu32(t2), 0, (unsigned int*)&weight);

	    threshold = intmin(weight<<2, threshold);
#endif /* }}} */

	    VERIFY_SADS(curblk, s44blk, rowstride, h, sads.i, 8);

	    psad = sads.i; sy = y; weight = *psad;
	    SKIM_RESULT(0,sy); psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(1,sy); psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(2,sy); psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(3,sy); psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(4,sy); psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(5,sy); psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(6,sy); psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(7,sy);

	    curblk++;

	    x += 4;
	  } while (x <= loadmark);
	}
	currowblk += rowstrideN;

	y += (4 * 8);
      } while (y <= yhighN);
    }
#endif /* }}} */

#if MULTI_ROW >= 4 /* {{{ */
    /* Calculate 4 rows at a time
     */
h2_multi_row_4:
    yhighN = yNlow + (yN & (~0x0f));
    if (y <= yhighN)
    {
      rowstrideN = rowstride << 2;
      do
      {
	curblk = currowblk;

	for (x = xlow; x <= xhigh;)
	{
	  /* load a 16*2 block of 4*4 pels for every 12 iterations */

	  /* load vy0-4 {{{ */
	  pblk = curblk;
	  vy0 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy1 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy2 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy3 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy4 = vec_ld(0, (unsigned char*)pblk);

	  if (NOT_VECTOR_ALIGNED(curblk)) {
	    pblk = curblk;
	    perm = vec_lvsl(0, (unsigned char*)pblk);
	    vy0 = ld16_perm(pblk, vy0, perm); pblk += rowstride;
	    vy1 = ld16_perm(pblk, vy1, perm); pblk += rowstride;
	    vy2 = ld16_perm(pblk, vy2, perm); pblk += rowstride;
	    vy3 = ld16_perm(pblk, vy3, perm); pblk += rowstride;
	    vy4 = ld16_perm(pblk, vy4, perm);
	  }
	  /* }}} */

	  shifter = shift; /* reset shifter */

	  loadmark = intmin(x + (4 * (12 - 1)), xhigh);
	  do
	  {
	    SAD(sads0,vy0,vy1,vy2,vy3,vy4);
	    vec_st(sads0, 0, &sads.v[0]);

	    /* increment permute for next iteration */
	    shifter = vec_add(shifter, increment);

#ifdef THRESHOLD_LOOKAHEAD /* {{{ */
	    /* find lowest sad for potential new threshold. */
	    vis_min_u32(sads0, t2, t3); /* sads0=input, t2=output, t3=temp */
	    /* store lowest sad to weight */
	    vec_ste(vu32(t2), 0, (unsigned int*)&weight);

	    threshold = intmin(weight<<2, threshold);
#endif /* }}} */

	    VERIFY_SADS(curblk, s44blk, rowstride, h, sads.i, 4);

	    psad = sads.i; sy = y; weight = *psad;
	    SKIM_RESULT(0,sy); psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(1,sy); psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(2,sy); psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(3,sy);

	    curblk++;

	    x += 4;
	  } while (x <= loadmark);
	}
	currowblk += rowstrideN;

	y += (4 * 4);
      } while (y <= yhighN);
    }
#endif /* }}} */

    /* finish any remaining rows one at a time {{{ */
#if MULTI_ROW > 0
    if (y <= yhigh) {
#endif
      /* zero vy2 and vy3 */
      vy2 = vec_splat_u8(0);
      vy3 = vec_splat_u8(0);

      /* re-arrange vr0 into (vr0[0-3], vr1[0-3], 0,0,0,0, 0,0,0,0) */
      vu32(t2) = vec_mergeh(vu32(vr0), vu32(vy2) /*zero*/);
      vu32(t3) = vec_mergeh(vu32(vr1), vu32(vy2) /*zero*/);
      vu32(vr0) = vec_mergeh(vu32(t2), vu32(t3));

      do {
	curblk = currowblk;

	for (x = xlow; x <= xhigh;)
	{
	  /* load a 16*2 block of 4*4 pels for every 12 iterations */

	  /* load vy0-1 {{{ */
	  pblk = curblk;
	  vy0 = vec_ld(0, (unsigned char*)pblk);
	  pblk += rowstride;
	  vy1 = vec_ld(0, (unsigned char*)pblk);

	  if (NOT_VECTOR_ALIGNED(curblk)) {
	    pblk = curblk;
	    perm = vec_lvsl(0, (unsigned char*)pblk);
	    vy0 = ld16_perm(pblk, vy0, perm); pblk += rowstride;
	    vy1 = ld16_perm(pblk, vy1, perm);
	  }
	  /* }}} */

	  shifter = shift; /* reset shifter */

	  loadmark = intmin(x + (4 * (12 - 1)), xhigh);
	  do
	  {
	    /* 
	     * AZ-- =  A~Z  == (A0, C0, A1, C1)
	     * BZ-- =  B~Z  == (B0, D0, B1, D1)
	     * ABZZ = AC~ZZ == (A0, B0, C0, D0)
	     *
	     * sad  = ABZZ~vr0
	     */
	    t1 = vec_perm(vy0, vy2, shifter); /* AZ-- = A~Z == (A0,Z0,A1,Z1) */
	    t2 = vec_perm(vy1, vy3, shifter); /* BZ-- = B~Z == (B0,Z0,B1,Z1) */
	    vu32(vr1) = vec_mergeh(vu32(t1), vu32(t2)); /* ABZZ = AZ~BZ */

	    /* increment permute for next iteration */
	    shifter = vec_add(shifter, increment);

	    /* calculate sad */                                              
	    t1 = vec_max(vr1, vr0);  /* find largest of two      */  
	    t2 = vec_min(vr1, vr0);  /* find smaller of two      */  
	    t3 = vec_sub(t1, t2);    /* find absolute difference */  
	    vu32(sads0) = vec_sum4s(t3, vu32(vy2) /*zero*/);

	    /* sum all parts of difference into one 32 bit quantity */
	    sads0 = vec_sums(sads0, vs32(vy2) /*zero*/);

	    vec_st(sads0, 0, &sads.v[0]);
	    weight = sads.i[3];

	    VERIFY_SADS(curblk, s44blk, rowstride, h, &weight, 1);

	    SKIM_RESULT(0,y);

	    curblk++;

	    x += 4;
	  } while (x <= loadmark);
	}
	currowblk += rowstride;

	y += 4;
      } while (y <= yhigh);
#if MULTI_ROW > 0
    }
#endif /* }}} */
#undef SAD
    /* }}} */
  } else /* h == 4 */ {
    /* {{{ */
    pblk = s44blk;
    perm = vec_lvsl(0, (unsigned int*)pblk);
    vr0 = splat_p32(pblk, perm); pblk += rowstride;
    vr1 = splat_p32(pblk, perm); pblk += rowstride;
    vr2 = splat_p32(pblk, perm); pblk += rowstride;
    vr3 = splat_p32(pblk, perm);

    /* 
     * AC-- =  A~C  == (A0, C0, A1, C1)
     * BD-- =  B~D  == (B0, D0, B1, D1)
     * CE-- =  C~E  == (C0, E0, C1, E1)
     * DF-- =  D~F  == (D0, F0, D1, F1)
     * EG-- =  E~G  == (E0, G0, E1, G1)
     * ABCD = AC~BD == (A0, B0, C0, D0)
     * BCDE = BD~CE == (B0, C0, D0, E0)
     * CDEF = CE~DF == (C0, D0, E0, F0)
     * DEFG = DF~EG == (D0, E0, F0, G0)
     *
     * sad  = ABCD~vr0
     * sad += BCDE~vr1
     * sad += CDEF~vr2
     * sad += DEFG~vr3
     */
#define SAD(sad,vA,vB,vC,vD,vE,vF,vG) /* {{{ */                              \
    vu8(sad) = vec_splat_u8(0); /* zero sad */                               \
								             \
    t1 = vec_perm(vA, vC, shifter); /* AC-- = A~C == (A0,C0,A1,C1) */        \
    t2 = vec_perm(vB, vD, shifter); /* BD-- = B~D == (B0,D0,B1,D1) */        \
    t3 = vec_perm(vC, vE, shifter); /* CE-- = C~E == (C0,E0,C1,E1) */        \
								             \
    vu32(t1) = vec_mergeh(vu32(t1), vu32(t2));   /* ABCD = AC~BD */          \
    vu32(t2) = vec_mergeh(vu32(t2), vu32(t3));   /* BCDE = BD~CE */          \
								             \
    t4  = vec_max(t1, vr0);     /* find largest of two  (ABCD)   */          \
    t5  = vec_min(t1, vr0);     /* find smaller of two  (ABCD)   */          \
    t4  = vec_sub(t4, t5);      /* find absolute difference      */          \
    vu32(sad) = vec_sum4s(t4, vu32(sad)); /* accumulate          */          \
								             \
    t4  = vec_max(t2, vr1);     /* find largest of two  (BCDE)   */          \
    t5  = vec_min(t2, vr1);     /* find smaller of two  (BCDE)   */          \
    t4  = vec_sub(t4, t5);      /* find absolute difference      */          \
    vu32(sad) = vec_sum4s(t4, vu32(sad)); /* accumulate          */          \
								             \
    t4 = vec_perm(vD, vF, shifter); /* DF-- = D~F == (D0,F0,D1,F1) */        \
    t5 = vec_perm(vE, vG, shifter); /* EG-- = E~G == (E0,G0,E1,G1) */        \
								             \
    vu32(t1) = vec_mergeh(vu32(t3), vu32(t4));   /* CDEF = CE~DF */          \
    vu32(t2) = vec_mergeh(vu32(t4), vu32(t5));   /* DEFG = DF~EG */          \
								             \
    t4  = vec_max(t1, vr2);     /* find largest of two  (CDEF)   */          \
    t5  = vec_min(t1, vr2);     /* find smaller of two  (CDEF)   */          \
    t4  = vec_sub(t4, t5);      /* find absolute difference      */          \
    vu32(sad) = vec_sum4s(t4, vu32(sad)); /* accumulate          */          \
								             \
    t4  = vec_max(t2, vr3);     /* find largest of two  (DEFG)   */          \
    t5  = vec_min(t2, vr3);     /* find smaller of two  (DEFG)   */          \
    t4  = vec_sub(t4, t5);      /* find absolute difference      */          \
    vu32(sad) = vec_sum4s(t4, vu32(sad)); /* accumulate          */          \
    /* }}} */


#if MULTI_ROW >= 12 /* {{{ */
    /* Calculate 12 rows at a time
     */
    yhighN = yNlow + ((yN / (4*12)) * (4*12));
    if (y <= yhighN)
    {
      rowstrideN = rowstride * 12;
      do
      {
	curblk = currowblk;

	for (x = xlow; x <= xhigh;)
	{
	  /* load a 16*2 block of 4*4 pels for every 12 iterations */

	  /* load vy0-14 {{{ */
	  pblk = curblk;
	  vy0 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy1 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy2 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy3 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy4 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy5 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy6 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy7 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy8 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy9 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy10 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy11 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy12 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy13 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy14 = vec_ld(0, (unsigned char*)pblk);

	  if (NOT_VECTOR_ALIGNED(curblk)) {
	    pblk = curblk;
	    perm = vec_lvsl(0, (unsigned char*)pblk);
	    vy0 = ld16_perm(pblk, vy0, perm); pblk += rowstride;
	    vy1 = ld16_perm(pblk, vy1, perm); pblk += rowstride;
	    vy2 = ld16_perm(pblk, vy2, perm); pblk += rowstride;
	    vy3 = ld16_perm(pblk, vy3, perm); pblk += rowstride;
	    vy4 = ld16_perm(pblk, vy4, perm); pblk += rowstride;
	    vy5 = ld16_perm(pblk, vy5, perm); pblk += rowstride;
	    vy6 = ld16_perm(pblk, vy6, perm); pblk += rowstride;
	    vy7 = ld16_perm(pblk, vy7, perm); pblk += rowstride;
	    vy8 = ld16_perm(pblk, vy8, perm); pblk += rowstride;
	    vy9 = ld16_perm(pblk, vy9, perm); pblk += rowstride;
	    vy10 = ld16_perm(pblk, vy10, perm); pblk += rowstride;
	    vy11 = ld16_perm(pblk, vy11, perm); pblk += rowstride;
	    vy12 = ld16_perm(pblk, vy12, perm); pblk += rowstride;
	    vy13 = ld16_perm(pblk, vy13, perm); pblk += rowstride;
	    vy14 = ld16_perm(pblk, vy14, perm);
	  }
	  /* }}} */

	  shifter = shift; /* reset shifter */

	  loadmark = intmin(x + (4 * (12 - 1)), xhigh);
	  do
	  {
	    SAD(sads0,vy0,vy1,vy2,vy3,vy4,vy5,vy6);
	    vec_st(sads0, 0, &sads.v[0]);

	    SAD(sads1,vy4,vy5,vy6,vy7,vy8,vy9,vy10);
	    vec_st(sads1, 0, &sads.v[1]);

#ifdef THRESHOLD_LOOKAHEAD /* {{{ */
	    /* temporarily store the lowest sads in perm */
	    vu32(perm) = vec_min(sads0, sads1);
#endif /* }}} */

	    SAD(sads0,vy8,vy9,vy10,vy11,vy12,vy13,vy14);
	    vec_st(sads0, 0, &sads.v[2]);

	    /* increment permute for next iteration */
	    shifter = vec_add(shifter, increment);

#ifdef THRESHOLD_LOOKAHEAD /* {{{ */
	    vu32(t1) = vec_min(sads0, vu32(perm) /*first set*/);
	    vis_min_u32(t1, t2, t3); /* t1=input, t2=output, t3=temp */
	    /* store lowest sad to weight */
	    vec_ste(vu32(t2), 0, (unsigned int*)&weight);

	    threshold = intmin(weight<<2, threshold);
#endif /* }}} */

	    VERIFY_SADS(curblk, s44blk, rowstride, h, sads.i, 12);

	    psad = sads.i; sy = y; weight = *psad;
	    SKIM_RESULT(0,sy); psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(1,sy); psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(2,sy); psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(3,sy); psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(4,sy); psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(5,sy); psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(6,sy); psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(7,sy); psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(8,sy); psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(9,sy); psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(10,sy); psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(11,sy);

	    curblk++;

	    x += 4;
	  } while (x <= loadmark);
	}
	currowblk += rowstrideN;

	y += (4 * 12);
      } while (y <= yhighN);

      /* skip over multi-row 8, this must be done since we've just finished
       * processing rows by 12==8+4. Y could be offset by 4 causing the
       * multi-row 8 to process extra rows.
       */
      goto h4_multi_row_4;
    }
#endif /* }}} */

#if MULTI_ROW >= 8 /* {{{ */
    /* Calculate 8 rows at a time
     */
    yhighN = yNlow + (yN & (~0x1f));
    if (y <= yhighN)
    {
      rowstrideN = rowstride << 3; /* rowstride*8 */
      do
      {
	curblk = currowblk;

	for (x = xlow; x <= xhigh;)
	{
	  /* load a 16*2 block of 4*4 pels for every 12 iterations */

	  /* load vy0-10 {{{ */
	  pblk = curblk;
	  vy0 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy1 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy2 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy3 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy4 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy5 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy6 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy7 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy8 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy9 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy10 = vec_ld(0, (unsigned char*)pblk);

	  if (NOT_VECTOR_ALIGNED(curblk)) {
	    pblk = curblk;
	    perm = vec_lvsl(0, (unsigned char*)pblk);
	    vy0 = ld16_perm(pblk, vy0, perm); pblk += rowstride;
	    vy1 = ld16_perm(pblk, vy1, perm); pblk += rowstride;
	    vy2 = ld16_perm(pblk, vy2, perm); pblk += rowstride;
	    vy3 = ld16_perm(pblk, vy3, perm); pblk += rowstride;
	    vy4 = ld16_perm(pblk, vy4, perm); pblk += rowstride;
	    vy5 = ld16_perm(pblk, vy5, perm); pblk += rowstride;
	    vy6 = ld16_perm(pblk, vy6, perm); pblk += rowstride;
	    vy7 = ld16_perm(pblk, vy7, perm); pblk += rowstride;
	    vy8 = ld16_perm(pblk, vy8, perm); pblk += rowstride;
	    vy9 = ld16_perm(pblk, vy9, perm); pblk += rowstride;
	    vy10 = ld16_perm(pblk, vy10, perm);
	  }
	  /* }}} */

	  shifter = shift; /* reset shifter */

	  loadmark = intmin(x + (4 * (12 - 1)), xhigh);
	  do
	  {
	    SAD(sads0,vy0,vy1,vy2,vy3,vy4,vy5,vy6);
	    vec_st(sads0, 0, &sads.v[0]);

	    SAD(sads1,vy4,vy5,vy6,vy7,vy8,vy9,vy10);
	    vec_st(sads1, 0, &sads.v[1]);

	    /* increment permute for next iteration */
	    shifter = vec_add(shifter, increment);

#ifdef THRESHOLD_LOOKAHEAD /* {{{ */
	    /* find lowest sad for potential new threshold. */
	    vu32(t1) = vec_min(sads0, sads1);
	    vis_min_u32(t1, t2, t3); /* t1=input, t2=output, t3=temp */
	    /* store lowest sad to weight */
	    vec_ste(vu32(t2), 0, (unsigned int*)&weight);

	    threshold = intmin(weight<<2, threshold);
#endif /* }}} */

	    VERIFY_SADS(curblk, s44blk, rowstride, h, sads.i, 8);

	    psad = sads.i; sy = y; weight = *psad;
	    SKIM_RESULT(0,sy); psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(1,sy); psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(2,sy); psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(3,sy); psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(4,sy); psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(5,sy); psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(6,sy); psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(7,sy);

	    curblk++;

	    x += 4;
	  } while (x <= loadmark);
	}
	currowblk += rowstrideN;

	y += (4 * 8);
      } while (y <= yhighN);
    }
#endif /* }}} */

#if MULTI_ROW >= 4 /* {{{ */
    /* Calculate 4 rows at a time
     */
h4_multi_row_4:
    yhighN = yNlow + (yN & (~0x0f));
    /* yhighN = yNlow + ((yN / (4*12)) * (4*12)); */
    /* yhighN = y + ((yN / (4*4)) * (4*4)); */
    if (y <= yhighN)
    {
      rowstrideN = rowstride << 2; /* rowstride*4 */
      do
      {
	curblk = currowblk;

	for (x = xlow; x <= xhigh;)
	{
	  /* load a 16*2 block of 4*4 pels for every 12 iterations */

	  /* load vy0-6 {{{ */
	  pblk = curblk;
	  vy0 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy1 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy2 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy3 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy4 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy5 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy6 = vec_ld(0, (unsigned char*)pblk);

	  if (NOT_VECTOR_ALIGNED(curblk)) {
	    pblk = curblk;
	    perm = vec_lvsl(0, (unsigned char*)pblk);
	    vy0 = ld16_perm(pblk, vy0, perm); pblk += rowstride;
	    vy1 = ld16_perm(pblk, vy1, perm); pblk += rowstride;
	    vy2 = ld16_perm(pblk, vy2, perm); pblk += rowstride;
	    vy3 = ld16_perm(pblk, vy3, perm); pblk += rowstride;
	    vy4 = ld16_perm(pblk, vy4, perm); pblk += rowstride;
	    vy5 = ld16_perm(pblk, vy5, perm); pblk += rowstride;
	    vy6 = ld16_perm(pblk, vy6, perm);
	  }
	  /* }}} */

	  shifter = shift; /* reset shifter */

	  loadmark = intmin(x + (4 * (12 - 1)), xhigh);
	  do
	  {
	    SAD(sads0,vy0,vy1,vy2,vy3,vy4,vy5,vy6);
	    vec_st(sads0, 0, &sads.v[0]);

	    /* increment permute for next iteration */
	    shifter = vec_add(shifter, increment);

#ifdef THRESHOLD_LOOKAHEAD /* {{{ */
	    /* find lowest sad for potential new threshold. */
	    vis_min_u32(sads0, t2, t3); /* sads0=input, t2=output, t3=temp */
	    /* store lowest sad to weight */
	    vec_ste(vu32(t2), 0, (unsigned int*)&weight);

	    threshold = intmin(weight<<2, threshold);
#endif /* }}} */

	    VERIFY_SADS(curblk, s44blk, rowstride, h, sads.i, 4);

	    psad = sads.i; sy = y; weight = *psad;
	    SKIM_RESULT(0,sy); psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(1,sy); psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(2,sy); psad++; sy += 4; weight = *psad;
	    SKIM_RESULT(3,sy);

	    curblk++;

	    x += 4;
	  } while (x <= loadmark);
	}
	currowblk += rowstrideN;

	y += (4 * 4);
      } while (y <= yhighN);
    }
#endif /* }}} */

    /* finish any remaining rows one at a time {{{ */
#if MULTI_ROW > 0
    if (y <= yhigh) {
#endif
      /* re-arrange vr0 into (vr0[0-3], vr1[0-3], vr2[0-3], vr3[0-3]) */
      vu32(t1) = vec_mergeh(vu32(vr0), vu32(vr2));
      vu32(t2) = vec_mergeh(vu32(vr1), vu32(vr3));
      vu32(vr0) = vec_mergeh(vu32(t1), vu32(t2));

      do {
	curblk = currowblk;

	for (x = xlow; x <= xhigh;)
	{
	  /* load a 16*4 block of 4*4 pels for every 12 iterations */

	  /* load vy0-3 {{{ */
	  pblk = curblk;
	  vy0 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy1 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy2 = vec_ld(0, (unsigned char*)pblk); pblk += rowstride;
	  vy3 = vec_ld(0, (unsigned char*)pblk);

	  if (NOT_VECTOR_ALIGNED(curblk)) {
	    pblk = curblk;
	    perm = vec_lvsl(0, (unsigned char*)pblk);
	    vy0 = ld16_perm(pblk, vy0, perm); pblk += rowstride;
	    vy1 = ld16_perm(pblk, vy1, perm); pblk += rowstride;
	    vy2 = ld16_perm(pblk, vy2, perm); pblk += rowstride;
	    vy3 = ld16_perm(pblk, vy3, perm);
	  }
	  /* }}} */

	  shifter = shift; /* reset shifter */

	  loadmark = intmin(x + (4 * (12 - 1)), xhigh);
	  do
	  {
	    /* load next block
	     * t4 = (vy0[0-3<<], vy1[0-3<<], vy2[0-3<<], vy3[0-3<<])
	     */
	    t1 = vec_perm(vy0, vy2, shifter);
	    t2 = vec_perm(vy1, vy3, shifter);
	    vu32(t4) = vec_mergeh(vu32(t1), vu32(t2));

	    /* increment permute for next iteration */
	    shifter = vec_add(shifter, increment);

	    /* calculate sad */
	    t1 = vec_splat_u8(0);
	    t2 = vec_max(t4, vr0); /* find largest of two      */
	    t3 = vec_min(t4, vr0); /* find smaller of two      */
	    t2 = vec_sub(t2, t3);  /* find absolute difference */
	    vu32(sads0) = vec_sum4s(t2, vu32(t1) /*zero*/);

	    /* sum all parts of difference into one 32 bit quantity */
	    sads0 = vec_sums(sads0, vs32(t1) /*zero*/);

	    vec_st(sads0, 0, &sads.v[0]);
	    weight = sads.i[3];

	    VERIFY_SADS(curblk, s44blk, rowstride, h, &weight, 1);

	    SKIM_RESULT(0,y);

	    curblk++;

	    x += 4;
	  } while (x <= loadmark);
	}
	currowblk += rowstride;

	y += 4;
      } while (y <= yhigh);
#if MULTI_ROW > 0
    }
#endif /* }}} */
    /* }}} */
  }

  sub44set->len = cres - sub44set->mests;

  AMBER_STOP;

#if ALTIVEC_TEST_FUNCTION(sub_mean_reduction)
    ALTIVEC_TEST_SUFFIX(sub_mean_reduction)(sub44set, 1+(reduction>1), &mean_weight);
#else
    ALTIVEC_SUFFIX(sub_mean_reduction)(sub44set, 1+(reduction>1), &mean_weight);
#endif

  return sub44set->len;
}

#if 0
int build_sub44_mests_altivec_test(me_result_set *sub44set,
	int ilow, int jlow, int ihigh, int jhigh, 
	int i0, int j0,
	int null_ctl_sad,
	uint8_t *s44org, uint8_t *s44blk, 
	int qrowstride, int qh,
	int reduction)
{
    uint8_t *s44orgblk;
    me_result_s *sub44_mests = sub44set->mests;
    int istrt = ilow-i0;
    int jstrt = jlow-j0;
    int iend = ihigh-i0;
    int jend = jhigh-j0;
    int mean_weight;
    int threshold;

    int i,j;
    int s1;
    uint8_t *old_s44orgblk;
    int sub44_num_mests;

    threshold = 6*null_ctl_sad / (4*4*reduction);
    s44orgblk = s44org+(ilow>>2)+qrowstride*(jlow>>2);

    sub44_num_mests = 0;

    s44orgblk = s44org+(ilow>>2)+qrowstride*(jlow>>2);
    for (j = jstrt; j <= jend; j += 4)
    {
	old_s44orgblk = s44orgblk;
	for (i = istrt; i <= iend; i += 4)
	{
	    s1 = ((*psad_sub44)( s44orgblk,s44blk,qrowstride,qh) & 0xffff);
#ifdef THRESHOLD
	    if (s1 < threshold)
	    {
		threshold = intmin(s1<<2,threshold);
#endif
		sub44_mests[sub44_num_mests].x = i;
		sub44_mests[sub44_num_mests].y = j;
		sub44_mests[sub44_num_mests].weight = s1 + 
		    (intmax(intabs(i-i0),intabs(j-j0))<<1);
		++sub44_num_mests;
#ifdef THRESHOLD
	    }
#endif
	    s44orgblk += 1;
	}
	s44orgblk = old_s44orgblk + qrowstride;
    }
    sub44set->len = sub44_num_mests;

#if 1
    sub_mean_reduction(sub44set, 1+(reduction>1),  &mean_weight);
#endif

    return sub44set->len;
}
#endif


#if ALTIVEC_TEST_FUNCTION(build_sub44_mests) /* {{{ */

#define BUILD_SUB44_MESTS_PFMT                                               \
  "sub44set=0x%X, ilow=%d, jlow=%d, ihigh=%d, jhigh=%d, i0=%d, j0=%d, "      \
  "null_ctl_sad=%d, s44org=0x%X, s44blk=0x%X, qrowstride=%d, qh=%d, "        \
  "reduction=%d" 

#  ifdef ALTIVEC_VERIFY
int build_sub44_mests_altivec_verify(BUILD_SUB44_MESTS_PDECL)
{
  int i, len1, len2;
  unsigned long checksum1, checksum2;

  len1 = _build_sub44_mests_altivec(BUILD_SUB44_MESTS_ARGS, 1 /*verify*/);
  for (checksum1 = i = 0; i < len1; i++) {
    checksum1 += sub44set->mests[i].weight;
    checksum1 += intabs(sub44set->mests[i].x);
    checksum1 += intabs(sub44set->mests[i].y);
  }

  len2 = ALTIVEC_TEST_WITH(build_sub44_mests)(BUILD_SUB44_MESTS_ARGS);
  for (checksum2 = i = 0; i < len2; i++) {
    checksum2 += sub44set->mests[i].weight;
    checksum2 += intabs(sub44set->mests[i].x);
    checksum2 += intabs(sub44set->mests[i].y);
  }

  if (len1 != len2 || checksum1 != checksum2) {
    mjpeg_debug("build_sub44_mests(" BUILD_SUB44_MESTS_PFMT ")",
	BUILD_SUB44_MESTS_ARGS);
    mjpeg_debug("build_sub44_mests: checksums differ %d[%d] != %d[%d]",
	checksum1, len1, checksum2, len2);
#if 0
      len1 = _build_sub44_mests_altivec(BUILD_SUB44_MESTS_ARGS, 0 /*verify*/);
      for (i = 0; i < len1; i++) {
	mjpeg_debug("A: %3d, %3d, %5d",
	    sub44set->mests[i].x,
	    sub44set->mests[i].y,
	    sub44set->mests[i].weight);
      }

      len2 = ALTIVEC_TEST_WITH(build_sub44_mests)(BUILD_SUB44_MESTS_ARGS);
      for (i = 0; i < len2; i++) {
	mjpeg_debug("C: %3d, %3d, %5d",
	    sub44set->mests[i].x,
	    sub44set->mests[i].y,
	    sub44set->mests[i].weight);
      }
#endif
  }

  return len2;
}

static void verify_sads(unsigned char *orgblk, unsigned char* s44blk,
			int rowstride, int h,
                        unsigned int *sads, int count)
{
    unsigned int i, weight, cweight;
    unsigned char *pblk;

    for (i = 0; i < count; i++)
    {
      weight = sads[i];
      pblk = orgblk + (rowstride * i);
#if ALTIVEC_TEST_FUNCTION(sad_sub44)
      cweight = ALTIVEC_TEST_WITH(sad_sub44)(pblk,s44blk,rowstride,h) & 0xffff;
#else
      cweight = sad_sub44(pblk,s44blk,rowstride,h) & 0xffff;
#endif
      if (weight != cweight)
	mjpeg_debug("build_sub44_mests: %d != %d="
	  "sad_sub44(blk1=0x%X, blk2=0x%X, rowstride=%d, h=%d",
	  weight, cweight, pblk, s44blk, rowstride, h);
    }
}

#  else
#undef BENCHMARK_FREQUENCY
#define BENCHMARK_FREQUENCY  543   /* benchmark every (n) calls */

#ifdef BUILD_SUB44_MESTS_BENCHMARK
#undef BENCHMARK_EPILOG
#define BENCHMARK_EPILOG                                                     \
  mjpeg_info("build_sub44_mests: xlow=%d xhigh=%d xhigh-xlow=%d "            \
                                "ylow=%d yhigh=%d yhigh-ylow=%d",            \
             xlow, xhigh, xhigh-xlow, ylow, yhigh, yhigh-ylow);              \
  mjpeg_info("build_sub44_mests: x15=%d x16=%d x31=%d x32=%d x47=%d x48=%d " \
	    "x63=%d x64=%d x79=%d x80=%d x95=%d",                            \
	    x15, x16, x31, x32, x47, x48, x63, x64, x79, x80, x95);          \
  mjpeg_info("build_sub44_mests: y15=%d y16=%d y31=%d y32=%d y47=%d y48=%d " \
	    "y63=%d y64=%d y79=%d y80=%d y95=%d",                            \
	    y15, y16, y31, y32, y47, y48, y63, y64, y79, y80, y95);
#endif

ALTIVEC_TEST(build_sub44_mests, int, (BUILD_SUB44_MESTS_PDECL),
    BUILD_SUB44_MESTS_PFMT, BUILD_SUB44_MESTS_ARGS);
#  endif
#endif /* }}} */
/* vim:set foldmethod=marker foldlevel=0: */
