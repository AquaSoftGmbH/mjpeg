/* build_sub22_mests.c, this file is part of the
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

/* pack data {{{ */
const static vector unsigned char pack_data[] = {
    /* pack_bits: condensed vector information that with a few
     * instructions can be expanded to full form.
     */
    (vector unsigned char)(
	/* pack_id */
	0x08, 0x04, 0x02, 0x01, /* (0,1,0,8, 0,1,0,4, 0,1,0,2, 0,1,0,1) */
	/* pack_select */
	0xC0, 0x30, 0x0C, 0x03, /* 0x(C0C0C0C0,30303030,0C0C0C0C,03030303) */
	/* pack_shift */
	0x04, 0x02, 0x00, 0x00, /* 0x(04040404,02020202,00000000,00000000) */
	/* xy22 */
	0x00, 0x40, 0x40, 0x50  /* (0,0,0,0, 0,0,2,0, 0,0,0,2, 0,0,2,2) */
    ),
    /* pack_table:
     * pack code     |    0    4    8    C    Z |(don't
     * pack data     |   00   01   10   11   00 | care
     * decoded value | 0000 0100 1000 1100 0000 |about Z)
     */
    (vector unsigned char)(
	/*  # | id  |  pack code  | pack data |    value  */
	/*  0  0000 = ( Z Z Z Z ) |00 00 00 00| */ 0x00,
	/*  1  0001 = ( C Z Z Z ) |11 00 00 00| */ 0xC0,
	/*  2  0010 = ( 8 Z Z Z ) |10 00 00 00| */ 0x80,
	/*  3  0011 = ( 8 8 Z Z ) |10 10 00 00| */ 0xA0,
	/*  4  0100 = ( 4 Z Z Z ) |01 00 00 00| */ 0x40,
	/*  5  0101 = ( 4 8 Z Z ) |01 10 00 00| */ 0x60,
	/*  6  0110 = ( 4 4 Z Z ) |01 01 00 00| */ 0x50,
	/*  7  0111 = ( 4 4 4 Z ) |01 01 01 00| */ 0x54,
	/*  8  1000 = ( 0 Z Z Z ) |00 00 00 00| */ 0x00,
	/*  9  1001 = ( 0 8 Z Z ) |00 10 00 00| */ 0x20,
	/* 10  1010 = ( 0 4 Z Z ) |00 01 00 00| */ 0x10,
	/* 11  1011 = ( 0 4 4 Z ) |00 01 01 00| */ 0x14,
	/* 12  1100 = ( 0 0 Z Z ) |00 00 00 00| */ 0x00,
	/* 13  1101 = ( 0 0 4 Z ) |00 00 01 00| */ 0x04,
	/* 14  1110 = ( 0 0 0 Z ) |00 00 00 00| */ 0x00,
	/* 15  1111 = ( 0 0 0 0 ) |00 00 00 00| */ 0x00
    )
}; /* }}} */

/*
 * Get SAD for 2*2 subsampled macroblocks:
 *  (0,0) (+2,0) (0,+2) (+2,+2) pixel-space coordinates
 *  (0,0) (+1,0) (0,+1) (+1,+1) 2*2 subsampled coordinates
 *
 *   blk         (blk)
 *   blk(+2,  0) (blk += 1)
 *   blk( 0, +2) (blk += rowstride-1)
 *   blk(+2, +2) (blk += 1)
 *
 * Iterate through all rows 2 at a time, calculating all 4 sads as we go.
 *
 * Hints regarding input:
 *   a) blk may be vector aligned, mostly not aligned
 *   b) ref is about 50% vector aligned and 50% 8 byte aligned
 *   c) rowstride is always a multiple of 16
 *   d) h == 4 or 8
 *
 * NOTES: Since ref is always 8 byte aligned and we are only interested in
 *        the first 8 bytes, the data can always be retreived with one vec_ld.
 *        This "one vec_ld" optimization is also attempted for blk.
 *
 *        The permutation vectors only need to be calculated once since
 *        rowstride is always a multiple of 16.
 */


#define BUILD_SUB22_MESTS_PDECL /* {{{ */                                    \
  me_result_set *sub44set,                                                   \
  me_result_set *sub22set,                                                   \
  int i0,  int j0, int ihigh, int jhigh,                                     \
  int null_ctl_sad,                                                          \
  uint8_t *s22org,  uint8_t *s22blk,                                         \
  int rowstride, int h,                                                      \
  int reduction                                                              \
  /* }}} */

#define BUILD_SUB22_MESTS_ARGS /* {{{ */                                     \
  sub44set, sub22set,                                                        \
  i0,  j0, ihigh, jhigh,                                                     \
  null_ctl_sad,                                                              \
  s22org,  s22blk,                                                           \
  rowstride, h,                                                              \
  reduction                                                                  \
  /* }}} */

/* int build_sub22_mests_altivec(BUILD_SUB22_MESTS_PDECL) {{{ */
#if defined(ALTIVEC_VERIFY) && ALTIVEC_TEST_FUNCTION(build_sub22_mests)
#define VERIFY_BUILD_SUB22_MESTS

static void verify_sads(uint8_t *blk1, uint8_t *blk2, int stride, int h,
			int *sads, int count);

static int _build_sub22_mests_altivec(BUILD_SUB22_MESTS_PDECL, int verify);
int build_sub22_mests_altivec(BUILD_SUB22_MESTS_PDECL)
{
  return _build_sub22_mests_altivec(BUILD_SUB22_MESTS_ARGS, 0 /* no verify */);
}

static int _build_sub22_mests_altivec(BUILD_SUB22_MESTS_PDECL, int verify)
#else
int build_sub22_mests_altivec(BUILD_SUB22_MESTS_PDECL)
#endif /* }}} */
{
    int i, ih;
    int min_weight;
    int x,y;
    uint8_t *s22orgblk;
    int len;
    me_result_s *sub44mests;
    me_result_s *cres;
    me_result_s mres;
    int count;
    /* */
    uint8_t *pblk, *pref;
    int dst;
    vector unsigned char t0, t1, t2, perm, perm2;
    vector unsigned char l0, l1;
    vector unsigned char v8x1a, v8x1b;
    vector unsigned char align8x2, align8x2_0, align8x2_2;
    vector unsigned char vblk8x2;
    vector unsigned char vref8x2;
    vector unsigned int zero, sad00, sad20, sad02, sad22;
    vector unsigned char lvsl;
    vector unsigned char pack_table;
    vector unsigned int pack_bits;
    vector unsigned char pack_id, pack_select, pack_shift;
    vector signed char xy22, xylim;
    vector signed char xy;
    vector bool char xymask;
    vector bool int vsel;
    vector unsigned char id;
    vector unsigned int hold;
    vector unsigned int viv; /* vector input */
    union {
	vector unsigned int v;
	struct {
	    me_result_s xy;
	    me_result_s xylim;
	    signed int threshold;
	} s;
    } vi;
    union {
	vector unsigned short v;
	struct {
	    unsigned short i[6];
	    unsigned short count;
	    unsigned short id;
	} s;
    } vo;

#ifdef ALTIVEC_VERIFY /* {{{ */
  if (((unsigned long)s22blk & 0x7) != 0)
   mjpeg_error_exit1("build_sub22_mests: s22blk %% 8 != 0, (0x%X)", s22blk);

  if (NOT_VECTOR_ALIGNED(rowstride))
    mjpeg_error_exit1("build_sub22_mests: rowstride %% 16 != 0, (%d)",
		rowstride);

  if (h != 4 && h != 8)
    mjpeg_error_exit1("build_sub22_mests: h != [4|8], (%d)", h);

#if 0
  if (NOT_VECTOR_ALIGNED(cres))
    mjpeg_warn("build_sub22_mests: cres %% 16 != 0, (0x%X)",cres);
#endif

#endif /* }}} */

    AMBER_START;

    len = sub44set->len;
    if (len < 1) {	    /* sub44set->len is sometimes zero. we can */
	sub22set->len = 0;  /* save a lot of effort if we stop short.  */
	return 0;
    }

#ifdef ALTIVEC_DST
    dst = DSTCB(1,h,rowstride);
    vec_dst(s22blk, dst, 0);
    dst += (1<<24)+(1<<16); /* increase size to 2 and increment count */
#endif

    sub44mests = sub44set->mests;
    cres = sub22set->mests;

    /* start loading pack_bits first */
    vu8(pack_bits) = vec_ld(0, &pack_data[0]);

    /* load pack_table */
    pack_table = vec_ld(0, &pack_data[1]);

    /* execute instructions that are not dependent on pack_bits */
    zero  = vec_splat_u32(0); /* initialize to zero */
    /* lvsl = 0x(00,01,02,03,04,05,06,07,08,09,0A,0B,0C,0D,0E,0F) {{{ */
    lvsl = vec_lvsl(0, (unsigned char*) 0);
    /* }}} */
    /* perm = (0,0,0,0, 1,1,1,1, 2,2,2,2, 3,3,3,3) {{{ */
    perm = vec_splat_u8(2);
    perm = vec_sr(lvsl, perm);
    /* }}} */

    /* 8*8 or 8*4 calculated in 8*2 chunks */
    /* align8x2 = 0x(00 01 02 03 04 05 06 07 10 11 12 13 14 15 16 17) {{{ */
    align8x2_0 = vec_sld(lvsl, lvsl, 8);
    align8x2_2 = vec_lvsr(0, (unsigned char*)0);
    align8x2 = vec_sld(align8x2_0, align8x2_2, 8);
    /* }}} */

    mres.x = ihigh - i0;    /* x <= ilim */
    mres.y = jhigh - j0;    /* y <= jlim */
    mres.weight = 0;	    /* clear weight, not needed */
    vi.s.xylim = mres;
    vi.s.threshold = 6 * null_ctl_sad / (reduction << 2); /* SAD threshold */


    /* pack_id = (0,1,0,8, 0,1,0,4, 0,1,0,2, 0,1,0,1) {{{ */
    vu32(pack_id) = vec_splat(vu32(pack_bits), 0);
    vs16(pack_id) = vec_unpackh(vs8(pack_id));
    vu16(t1) = vec_splat_u16(1);
    vu16(pack_id) = vec_mergeh(vu16(t1), vu16(pack_id));
    /* }}} */
    /* pack_select = 0x(C0C0C0C0,30303030,0C0C0C0C,03030303) {{{ */
    vu32(pack_select) = vec_splat(vu32(pack_bits), 1);
    pack_select = vec_perm(pack_select, vu8(zero), perm);
    /* }}} */
    /* pack_shift = 0x(04040404,02020202,00000000,00000000) {{{ */
    vu32(pack_shift) = vec_splat(vu32(pack_bits), 2);
    pack_shift = vec_perm(vu8(pack_shift), vu8(zero), perm);
    /* }}} */
    /* xy22 = (0,0,0,0, 0,0,2,0, 0,0,0,2, 0,0,2,2) {{{ */
    vu32(xy22) = vec_splat(vu32(pack_bits), 3);
    vu32(xy22) = vec_unpackh((vector pixel)xy22);
    vu32(xy22) = vec_unpackh((vector pixel)xy22);
    vu8(t0) = vec_splat_u8(3);
    vu32(xy22) = vec_srl(vu32(xy22), vu8(t0) /* (3) */);
    /* }}} */


    perm2 = vec_lvsl(0, s22blk);
    perm2 = vec_splat(perm2, 0);
    perm2 = vec_add(perm2, align8x2);

    ih = (h >> 1) - 1;

    /* the result array is not vector aligned, we must copy the preceding
     * result memory to avoid trashing it when results are stored.
     */
    hold = vec_ld(0, (unsigned int*)cres);

    do { /* while (--len) */
	mres = *sub44mests;
	x = mres.x;
	y = mres.y;

	s22orgblk = s22org + ((y+j0)>>1)*rowstride + ((x+i0)>>1);
#ifdef ALTIVEC_DST
	vec_dst(s22orgblk, dst, 1);
#endif

	mres.weight = 0; /* clear weight, not needed */
	vi.s.xy = mres;
	sub44mests++;



	/*
	 * Get SAD for 2*2 subsampled macroblocks: orgblk,orgblk(+2,0),
	 * orgblk(0,+2), and orgblk(+2,+2) Done all in one go to reduce
	 * memory bandwidth demand
	 */

	/* sad{00,20,02,22} = 0 {{{ */
	sad00 = vec_splat_u32(0);
	sad20 = vec_splat_u32(0);
	sad02 = vec_splat_u32(0);
	sad22 = vec_splat_u32(0);
	/* }}} */

	pblk = s22orgblk;
	pref = s22blk;

	perm = vec_lvsl(0, pblk); /* initialize permute vector */

	if (((unsigned long)pblk & 0xf) < 8) {
	    v8x1a = vec_ld(0, pblk);
	    pblk += rowstride;
	    v8x1b = vec_ld(0, pblk);

	    vref8x2 = vec_ld(0, pref);
	    pref += rowstride;
	    t2 = vec_ld(0, pref);

	    align8x2_0 = vec_splat(perm, 0);
	    align8x2_0 = vec_add(align8x2_0, align8x2);
	    align8x2_2 = vec_splat(perm, 1);
	    align8x2_2 = vec_add(align8x2_2, align8x2);

	    vref8x2 = vec_perm(vref8x2, t2, perm2);

	    i = ih;
	    do { /* while (--i) */
		/* load next row */
		pblk += rowstride;
		l0 = vec_ld(0, pblk);

		/* calculate (0,0) */
		vblk8x2 = vec_perm(v8x1a, v8x1b, align8x2_0);
		t0 = vec_max(vblk8x2, vref8x2);
		t1 = vec_min(vblk8x2, vref8x2);
		t0 = vec_sub(t0, t1);
		sad00 = vec_sum4s(t0, sad00);

		/* calculate (2,0) */
		vblk8x2 = vec_perm(v8x1a, v8x1b, align8x2_2);
		t0 = vec_max(vblk8x2, vref8x2);
		t1 = vec_min(vblk8x2, vref8x2);
		t0 = vec_sub(t0, t1);
		sad20 = vec_sum4s(t0, sad20);

		/* load into v8x1a, then v8x1b will be the top row */
		v8x1a = vec_sld(l0, l0, 0); /* v8x1a = l0; */
		/* load next row */
		pblk += rowstride;
		l0 = vec_ld(0, pblk);

		/* calculate (0,2) */
		vblk8x2 = vec_perm(v8x1b, v8x1a, align8x2_0);
		t0 = vec_max(vblk8x2, vref8x2);
		t1 = vec_min(vblk8x2, vref8x2);
		t0 = vec_sub(t0, t1);
		sad02 = vec_sum4s(t0, sad02);

		/* calculate (2,2) */
		vblk8x2 = vec_perm(v8x1b, v8x1a, align8x2_2);
		t0 = vec_max(vblk8x2, vref8x2);
		t1 = vec_min(vblk8x2, vref8x2);

		pref += rowstride;
		vref8x2 = vec_ld(0, pref);
		pref += rowstride;
		t2 = vec_ld(0, pref);

		t0 = vec_sub(t0, t1);
		sad22 = vec_sum4s(t0, sad22);

		v8x1b = vec_sld(l0, l0, 0); /* v8x1b = l0; */

		vref8x2 = vec_perm(vref8x2, t2, perm2);
	    } while (--i);

	    /* load next row */
	    pblk += rowstride;
	    l0 = vec_ld(0, pblk);

	    /* calculate (0,0) */
	    vblk8x2 = vec_perm(v8x1a, v8x1b, align8x2_0);
	    t0 = vec_max(vblk8x2, vref8x2);
	    t1 = vec_min(vblk8x2, vref8x2);
	    t0 = vec_sub(t0, t1);
	    sad00 = vec_sum4s(t0, sad00);

	    /* calculate (2,0) */
	    vblk8x2 = vec_perm(v8x1a, v8x1b, align8x2_2);
	    /* load into v8x1a, then v8x1b will be the top row */
	    v8x1a = vec_sld(l0, l0, 0); /* v8x1a = l0; */

	} else {
	    v8x1a = vec_ld(0, pblk);
	    l0 = vec_ld(16, pblk);

	    pblk += rowstride;
	    v8x1b = vec_ld(0, pblk);
	    l1 = vec_ld(16, pblk);

	    /* align8x2_0 = align8x2 */
	    align8x2_0 = vec_sld(align8x2, align8x2, 0);
	    align8x2_2 = vec_splat_u8(1); 
	    align8x2_2 = vec_add(align8x2, align8x2_2 /* (1) */ );

	    vref8x2 = vec_ld(0, pref);
	    pref += rowstride;
	    t2 = vec_ld(0, pref);

	    v8x1a = vec_perm(v8x1a, l0, perm);
	    v8x1b = vec_perm(v8x1b, l1, perm);

	    vref8x2 = vec_perm(vref8x2, t2, perm2);

	    i = ih;
	    do { /* while (--i) */
		/* load next row */
		pblk += rowstride;
		l0 = vec_ld(0, pblk);
		l1 = vec_ld(16, pblk);

		/* calculate (0,0) */
		vblk8x2 = vec_perm(v8x1a, v8x1b, align8x2_0);
		t0 = vec_max(vblk8x2, vref8x2);
		t1 = vec_min(vblk8x2, vref8x2);
		t0 = vec_sub(t0, t1);
		sad00 = vec_sum4s(t0, sad00);

		/* calculate (2,0) */
		vblk8x2 = vec_perm(v8x1a, v8x1b, align8x2_2);
		t0 = vec_max(vblk8x2, vref8x2);
		t1 = vec_min(vblk8x2, vref8x2);
		t0 = vec_sub(t0, t1);
		sad20 = vec_sum4s(t0, sad20);

		/* load into v8x1a, then v8x1b will be the top row */
		v8x1a = vec_perm(l0, l1, perm);
		/* load next row */
		pblk += rowstride;
		l0 = vec_ld(0, pblk);
		l1 = vec_ld(16, pblk);

		/* calculate (0,2) */
		vblk8x2 = vec_perm(v8x1b, v8x1a, align8x2_0);
		t0 = vec_max(vblk8x2, vref8x2);
		t1 = vec_min(vblk8x2, vref8x2);
		t0 = vec_sub(t0, t1);
		sad02 = vec_sum4s(t0, sad02);

		/* calculate (2,2) */
		vblk8x2 = vec_perm(v8x1b, v8x1a, align8x2_2);
		t0 = vec_max(vblk8x2, vref8x2);
		t1 = vec_min(vblk8x2, vref8x2);

		pref += rowstride;
		vref8x2 = vec_ld(0, pref);
		pref += rowstride;
		t2 = vec_ld(0, pref);

		t0 = vec_sub(t0, t1);
		sad22 = vec_sum4s(t0, sad22);

		v8x1b = vec_perm(l0, l1, perm);

		vref8x2 = vec_perm(vref8x2, t2, perm2);
	    } while (--i);

	    /* load next row */
	    pblk += rowstride;
	    l0 = vec_ld(0, pblk);
	    l1 = vec_ld(16, pblk);

	    /* calculate (0,0) */
	    vblk8x2 = vec_perm(v8x1a, v8x1b, align8x2_0);
	    t0 = vec_max(vblk8x2, vref8x2);
	    t1 = vec_min(vblk8x2, vref8x2);
	    t0 = vec_sub(t0, t1);
	    sad00 = vec_sum4s(t0, sad00);

	    /* calculate (2,0) */
	    vblk8x2 = vec_perm(v8x1a, v8x1b, align8x2_2);
	    /* load into v8x1a, then v8x1b will be the top row */
	    v8x1a = vec_perm(l0, l1, perm);
	}

	/* calculate (2,0) */
	t0 = vec_max(vblk8x2, vref8x2);
	t1 = vec_min(vblk8x2, vref8x2);
	t0 = vec_sub(t0, t1);
	sad20 = vec_sum4s(t0, sad20);

	/* calculate (0,2) */
	vblk8x2 = vec_perm(v8x1b, v8x1a, align8x2_0);
	t0 = vec_max(vblk8x2, vref8x2);
	t1 = vec_min(vblk8x2, vref8x2);
	t0 = vec_sub(t0, t1);
	sad02 = vec_sum4s(t0, sad02);

	/* calculate (2,2) */
	vblk8x2 = vec_perm(v8x1b, v8x1a, align8x2_2);
	t0 = vec_max(vblk8x2, vref8x2);
	t1 = vec_min(vblk8x2, vref8x2);
	t0 = vec_sub(t0, t1);
	sad22 = vec_sum4s(t0, sad22);


	/* calculate final sums {{{ */
	vs32(sad00) = vec_sums(vs32(sad00), vs32(zero));
	vs32(sad20) = vec_sums(vs32(sad20), vs32(zero));
	vs32(sad02) = vec_sums(vs32(sad02), vs32(zero));
	vs32(sad22) = vec_sums(vs32(sad22), vs32(zero));
	/* }}} */

	/* sads = {sad00, sad20, sad02, sad22} {{{ */
	vu32(sad00) = vec_mergel(vu32(sad00), vu32(sad02));
	vu32(sad20) = vec_mergel(vu32(sad20), vu32(sad22));
	vu32(sad00) = vec_mergel(vu32(sad00), vu32(sad20));
	/* }}} */


#ifdef VERIFY_BUILD_SUB22_MESTS
	if (verify) {
	    vec_st(vs32(sad00), 0, (signed int*)&vo);
	    verify_sads(s22orgblk, s22blk, rowstride, h, (signed int*)&vo, 4);
	}
#endif


	/* load vector scalar input */
	viv = vec_ld(0, (unsigned int*) &vi);

	/* mask sads  x <= ilim && y <= jlim {{{ */
	/* the first cmpgt (u8) will flag any x and/or y coordinates ... {{{
	 * as out of bounds. the second cmpgt (u32) will complete the
	 * mask if the x or y flag for that result is set.
	 *
	 * Example:
	 *        X  Y         X  Y         X  Y         X  Y
	 * [0  0  <  <] [0  0  <  <] [0  0  >  <] [0  0  <  >]
	 * vu8(xymask)  = vec_cmpgt(vu8(xy), xymax)
	 * [0  0  0  0] [0  0  0  0] [0  0  1  0] [0  0  0  1]
	 * vu32(xymask) = vec_cmpgt(vu32(xymask), vu32(zero))
	 * [0  0  0  0] [0  0  0  0] [1  1  1  1] [1  1  1  1]
	 *
	 * Legend: 0=0x00  (<)=(xy[n] <= xymax[n])
	 *         1=0xff  (>)=(xy[n] >  xymax[n])
	 */ /* }}} */
	vu32(xy) = vec_splat(viv, 0);    /* splat vi.s.xy */
	xy = vec_add(xy, xy22);          /* adjust xy values for elements 1-3 */
	vu32(xylim) = vec_splat(viv, 1); /* splat vi.s.xylim */
	xymask = vec_cmpgt(xy, xylim);			    /* u8  compare */
	vb32(xymask) = vec_cmpgt(vu32(xymask), vu32(zero)); /* u32 compare */
	/* }}} */

	/* add distance penalty {{{ */
	/* penalty = (intmax(intabs(x),intabs(y))<<3) */
	/* t0 = abs(xy) {{{ */
	vs8(t0) = vec_subs(vs8(zero), xy);
	vs8(t0) = vec_max(vs8(t0), xy);
	/* }}} */
	/* t1 = (x, x, x, x), t2 = (y, y, y, y) {{{ */
	/* (0,0,x,y, 0,0,x,y, 0,0,x,y, 0,0,x,y) */
	/* (0000000F)+lvsl (0,0,0,x, 0,0,0,x, 0,0,0,x, 0,0,0,x) */
	/* (00000010)+lvsl (0,0,0,y, 0,0,0,y, 0,0,0,y, 0,0,0,y) */
	vu32(perm) = vec_splat_u32(0xF);
	perm = vec_add(perm, lvsl);
	t1 = vec_perm(vu8(zero), t0, perm);
	vu32(t2) = vec_splat_u32(1);
	vu32(perm) = vec_add(vu32(perm), vu32(t2));
	t2 = vec_perm(vu8(zero), t0, perm);
	/* }}} */
	/* penalty = max(t1, t2) << 3 {{{ */
	/*   x|y                 x         y */
	vu32(t1) = vec_max(vu32(t1), vu32(t2));
	vu32(t0) = vec_splat_u32(3);
	vu32(t0) = vec_sl(vu32(t1), vu32(t0) /* (3) */ );
	/* }}} */
	vs32(sad00) = vec_adds(vs32(sad00), vs32(t0) /* penalty */ );
	/* }}} */

	/* find sads below threshold {{{ */
	vs32(t0) = vec_splat(viv, 2);    /* splat vi.s.threshold */
	vsel = vec_cmplt(vs32(sad00), vs32(t0));
	/* }}} */

	/* adjust vsel to exclude sads that were x/y clipped */
	vsel = vec_sel(vsel, vb32(zero), vb32(xymask));


	/* arrange sad and xy into me_result_s form {{{ */
	/* (   0, sad,   0, sad,   0, sad,   0, sad )
	 * ( sad, sad, sad, sad, sad, sad, sad, sad )
	 *
	 * (   0,  xy,   0,  xy,   0,  xy,   0,  xy )
	 * (  xy,  xy,  xy,  xy,  xy,  xy,  xy,  xy )
	 *
	 * ( sad,  xy, sad,  xy, sad,  xy, sad,  xy )
	 */
	vu16(xy) = vec_pack(vu32(xy), vu32(xy));
	vu16(sad00) = vec_pack(vu32(sad00), vu32(sad00));
	vu16(sad00) = vec_mergeh(vu16(sad00), vu16(xy));
	/* }}} */


	/* pack the result elements to the left. {{{  */
	/* identify vsel {{{ */
	/* id will be used to select correct pack_table data */
	id = vec_and(vu8(vsel), pack_id);
	vs32(id) = vec_sums(vs32(id), vs32(zero));
	/* store {count, id} for scalar access */
	vec_st(id, 0, (unsigned char*)&vo);
	id = vec_splat(id, 15);
	/* }}} */

	/* generate permute vector using pack id {{{ */
	perm = vec_perm(pack_table, vu8(zero), id);
	perm = vec_and(perm, pack_select);
	perm = vec_sr(perm, pack_shift);
	perm = vec_add(perm, lvsl);
	/* }}} */

	/* pack our results */
	vu32(sad00) = vec_perm(vu32(sad00), vu32(zero), perm);
	/* }}} */


	/* 1. add as many elements as we can to our hold buffer
	 * 2. if our hold buffer contains four elements, store to memory
	 *    then move any remaining elements to our hold buffer.
	 *
	 * [##,##,##,##]->[z0,z1,z2,z3]->[m0,m1,m2,m3]
	 * [h0,##,##,##]->[z1,z2,z3,h0]->[h0,m0,m1,m2]
	 * [h0,h1,##,##]->[z2,z3,h0,h1]->[h0,h1,m0,m1]
	 * [h0,h1,h2,##]->[z3,h0,h1,h2]->[h0,h1,h2,m0]
	 */
	perm = vec_lvsl(0, (unsigned int*)cres);
	vu32(hold) = vec_perm(vu32(zero), vu32(hold), perm);
	perm = vec_lvsr(0, (unsigned int*)cres);
	vu32(hold) = vec_perm(vu32(hold), vu32(sad00), perm);

	/* store to memory if necessary and move extra mests to hold */
	count = vo.s.count;
	if (((unsigned int)cres & 0x0f) + (count << 2) >= 16) {
	    vec_st(hold, 0, (unsigned int*)cres);
	    vu32(hold) = vec_perm(vu32(sad00), vu32(zero), perm);
	}
	cres += count;
    } while (--len);

    /* write any remaining results */
    vec_st(hold, 0, (unsigned int*)cres);

    sub22set->len = cres - sub22set->mests;

#if ALTIVEC_TEST_FUNCTION(sub_mean_reduction)
    ALTIVEC_TEST_SUFFIX(sub_mean_reduction)(sub22set, reduction, &min_weight);
#else
    ALTIVEC_SUFFIX(sub_mean_reduction)(sub22set, reduction, &min_weight);
#endif

    AMBER_STOP;

    return sub22set->len;
}


#if ALTIVEC_TEST_FUNCTION(build_sub22_mests) /* {{{ */

#define BUILD_SUB22_MESTS_PFMT                                               \
  "sub44set=0x%X, sub22set=0x%X, i0=%d, j0=%d, ihigh=%d, jhigh=%d, "         \
  "null_ctl_sad=%d, s22org=0x%X, s22blk=0x%X, rowstride=%d, h=%d, "          \
  "reduction=%d"

#  ifdef ALTIVEC_VERIFY
int build_sub22_mests_altivec_verify(BUILD_SUB22_MESTS_PDECL)
{
  int i, len1, len2;
  unsigned long checksum1, checksum2;

  len1 = _build_sub22_mests_altivec(BUILD_SUB22_MESTS_ARGS, 1 /*verify*/);
  for (checksum1 = i = 0; i < len1; i++) {
    checksum1 += sub22set->mests[i].weight;
    checksum1 += intabs(sub22set->mests[i].x);
    checksum1 += intabs(sub22set->mests[i].y);
  }

  len2 = ALTIVEC_TEST_WITH(build_sub22_mests)(BUILD_SUB22_MESTS_ARGS);
  for (checksum2 = i = 0; i < len2; i++) {
    checksum2 += sub22set->mests[i].weight;
    checksum2 += intabs(sub22set->mests[i].x);
    checksum2 += intabs(sub22set->mests[i].y);
  }

  if (len1 != len2 || checksum1 != checksum2) {
    mjpeg_debug("build_sub22_mests(" BUILD_SUB22_MESTS_PFMT ")",
	BUILD_SUB22_MESTS_ARGS);
    mjpeg_debug("build_sub22_mests: sub44set->len=%d", sub44set->len);
    mjpeg_debug("build_sub22_mests: checksums differ %d[%d] != %d[%d]",
	checksum1, len1, checksum2, len2);
  }
#if 0
 else {
    mjpeg_info("build_sub22_mests(" BUILD_SUB22_MESTS_PFMT ")",
	BUILD_SUB22_MESTS_ARGS);
    mjpeg_info("build_sub22_mests: sub44set->len=%d", sub44set->len);
    mjpeg_info("build_sub22_mests: checksum %d[%d]",
	checksum1, len1);
  }
#endif

  return len2;
}

static void verify_sads(uint8_t *blk1, uint8_t *blk2, int stride, int h,
			int *sads, int count)
{
    int i, s, s2;
    uint8_t *pblk;

    pblk = blk1;
    for (i = 0; i < count; i++) {
	s2 = sads[i];
	/* s = sad_sub22(pblk, blk2, stride, h); {{{ */
#if ALTIVEC_TEST_FUNCTION(sad_sub22)
	s = ALTIVEC_TEST_WITH(sad_sub22)(pblk, blk2, stride, h);
#else
	s = sad_sub22(pblk, blk2, stride, h);
#endif /* }}} */
	if (s2 != s) {
	    mjpeg_debug("build_sub22_mests: sads[%d]=%d != %d"
			"=sad_sub22(blk1=0x%X(0x%X), blk2=0x%X, "
			"stride=%d, h=%d)",
			i, s2, s, pblk, blk1, blk2, stride, h);
	}

	if (i == 1)
	    pblk += stride - 1;
	else
	    pblk += 1;
    }
}

#  else

#undef BENCHMARK_EPILOG
#define BENCHMARK_EPILOG \
    mjpeg_info("build_sub22_mests: sub44set->len=%d", sub44set->len);

ALTIVEC_TEST(build_sub22_mests, int, (BUILD_SUB22_MESTS_PDECL),
  BUILD_SUB22_MESTS_PFMT, BUILD_SUB22_MESTS_ARGS);
#  endif
#endif /* }}} */
/* vim:set foldmethod=marker foldlevel=0: */
