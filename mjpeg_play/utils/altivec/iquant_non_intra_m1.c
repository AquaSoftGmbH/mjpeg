/* iquant_non_intra_m1.c, this file is part of the
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_ALTIVEC_H
#include <altivec.h>
#endif

#ifdef ALTIVEC_VERIFY
#  if ALTIVEC_TEST_FUNCTION(iquant_non_intra_m1))
#    include <stdlib.h>
#  endif
#endif

#include "altivec_quantize.h"
#include "vectorize.h"
#include "../mjpeg_logging.h"

/* #define AMBER_ENABLE */
#include "amber.h"


#define IQUANT_NON_INTRA_M1_PDECL int16_t *src, int16_t *dst, uint16_t *qmat

#define IQUANT_NON_INTRA_M1_ARGS src, dst, qmat

void iquant_non_intra_m1_altivec(IQUANT_NON_INTRA_M1_PDECL)
{
    int i;
    vector signed short vsrc;
    vector unsigned short vqmat;
    vector signed short val, t0, t1;
    vector bool short ltzero, eqzero;
    vector signed short zero, one;
    vector unsigned int five;
    vector signed short saturation;
#ifdef ALTIVEC_DST
    DataStreamControl dsc;
#endif

#ifdef ALTIVEC_VERIFY /* {{{ */
    if (NOT_VECTOR_ALIGNED(qmat))
	mjpeg_error_exit1("iquant_non_intra_m1: qmat %% 16 != 0, (%d)", qmat);

    if (NOT_VECTOR_ALIGNED(src))
	mjpeg_error_exit1("iquant_non_intra_m1: src %% 16 != 0, (%d)", src);

    if (NOT_VECTOR_ALIGNED(dst))
	mjpeg_error_exit1("iquant_non_intra_m1: dst %% 16 != 0, (%d)", dst);
#endif /* }}} */

    AMBER_START;

#ifdef ALTIVEC_DST
    dsc.control = DATA_STREAM_CONTROL(64/8,1,0);
    vec_dst(src, dsc.control, 0);
    vec_dst(qmat, dsc.control, 1);
#endif

    vsrc = vec_ld(0, (signed short*)src);
    vqmat = vec_ld(0, (unsigned short*)qmat);
    zero = vec_splat_s16(0);
    one = vec_splat_s16(1);
    five = vec_splat_u32(5);
    vu8(t0) = vec_splat_u8(0x7);
    vs8(t1) = vec_splat_s8(-1); /* 0xff */
    vu8(saturation) = vec_mergeh(vu8(t0), vu8(t1)); /* 0x07ff == 2047 */

    i = (64/8)-1;
    do {
	ltzero = vec_cmplt(vsrc, zero);
	eqzero = vec_cmpeq(vsrc, zero);

	/* val = abs(src) */
	t0 = vec_sub(zero, vsrc);
	val = vec_max(t0, vsrc);

	/* val = val + val + 1 */
	val = vec_add(val, val);
	val = vec_add(val, one);

	/* val = (val * quant) >> 5 */
	vu32(t0) = vec_mule(vu16(val), vqmat);
	vu32(t0) = vec_sra(vu32(t0), five);
	vu16(t0) = vec_pack(vu32(t0), vu32(t0));
	vu32(t1) = vec_mulo(vu16(val), vqmat);
	vu32(t1) = vec_sra(vu32(t1), five);
	vu16(t1) = vec_pack(vu32(t1), vu32(t1));
	vu16(val) = vec_mergeh(vu16(t0), vu16(t1));

	src += 8;
	vsrc = vec_ld(0, (signed short*)src);
	qmat += 8;
	vqmat = vec_ld(0, (unsigned short*)qmat);

	/* val =  val - ((~(val & 1)) & (val != 0)) */
	t0 = vec_andc(one, eqzero);
	t1 = vec_and(val, one);
	t0 = vec_andc(t0, t1);
	val = vec_sub(val, t0);

	/* val = min(val, 2047) */
	val = vec_min(val, saturation);

	/* val = samesign(src, val) */
	t0 = vec_sub(zero, val);
	val = vec_sel(val, t0, ltzero);

	/* if (src[i] == 0) dst[i] = 0 */
	val = vec_andc(val, eqzero);

	vec_st(val, 0, dst);
	dst += 8;
    } while (--i);

    ltzero = vec_cmplt(vsrc, zero);
    eqzero = vec_cmpeq(vsrc, zero);

    /* val = abs(src) */
    t0 = vec_sub(zero, vsrc);
    val = vec_max(t0, vsrc);

    /* val = val + val + 1 */
    val = vec_add(val, val);
    val = vec_add(val, one);

    /* val = (val * quant) >> 5 */
    vu32(t0) = vec_mule(vu16(val), vqmat);
    vu32(t0) = vec_sra(vu32(t0), five);
    vu16(t0) = vec_pack(vu32(t0), vu32(t0));
    vu32(t1) = vec_mulo(vu16(val), vqmat);
    vu32(t1) = vec_sra(vu32(t1), five);
    vu16(t1) = vec_pack(vu32(t1), vu32(t1));
    vu16(val) = vec_mergeh(vu16(t0), vu16(t1));

    /* val =  val - ((~(val & 1)) & (val != 0)) */
    t0 = vec_andc(one, eqzero);
    t1 = vec_and(val, one);
    t0 = vec_andc(t0, t1);
    val = vec_sub(val, t0);

    /* val = min(val, 2047) */
    val = vec_min(val, saturation);

    /* val = samesign(src, val) */
    t0 = vec_sub(zero, val);
    val = vec_sel(val, t0, ltzero);

    /* if (src[i] == 0) dst[i] = 0 */
    val = vec_andc(val, eqzero);

    vec_st(val, 0, dst);

    AMBER_STOP;
}


#if 0
/*---------------------------------------------------------------------------*/
void iquant_non_intra_m1_altivec(IQUANT_NON_INTRA_M1_PDECL) /* {{{ */
{
    int i;
    vector signed short vsrc;
    vector unsigned short vqmat;
    vector signed short t0;
    vector signed short val;
    vector bool short nonzero, sel;
    vector signed short zero, one;
    vector unsigned int vE, vO, five;
    vector signed short saturation;

#ifdef ALTIVEC_VERIFY /* {{{ */
    if (NOT_VECTOR_ALIGNED(qmat))
	mjpeg_error_exit1("iquant_non_intra_m1: qmat %% 16 != 0, (%d)", qmat);

    if (NOT_VECTOR_ALIGNED(src))
	mjpeg_error_exit1("iquant_non_intra_m1: src %% 16 != 0, (%d)", src);

    if (NOT_VECTOR_ALIGNED(dst))
	mjpeg_error_exit1("iquant_non_intra_m1: dst %% 16 != 0, (%d)", dst);
#endif /* }}} */

    AMBER_START;

    zero = vec_splat_s16(0);
    one = vec_splat_s16(1);
    five = vec_splat_u32(5);
    vs8(t0) = vec_splat_s8(-1); /* 0xff */
    vu8(saturation) = vec_splat_u8(0x7);
    vu8(saturation) = vec_mergeh(vu8(saturation), vu8(t0)); /* 0x07ff == 2047 */

    i = 64/8;
    do {
	vsrc = vec_ld(0, (signed short*)src);
	src += 8;
	vqmat = vec_ld(0, (unsigned short*)qmat);
	qmat += 8;

	/* val = abs(src) */
	t0 = vec_sub(zero, vsrc);
	val = vec_max(t0, vsrc);

	/* val = val + val + 1 */
	val = vec_add(val, val);
	val = vec_add(val, one);

	/* val = (val * quant) >> 5 */
	vE = vec_mule(vu16(val), vqmat);
	vE = vec_sra(vE, five);
	vu16(vE) = vec_pack(vE, vE);
	vO = vec_mulo(vu16(val), vqmat);
	vO = vec_sra(vO, five);
	vu16(vO) = vec_pack(vO, vO);
	vu16(val) = vec_mergeh(vu16(vE), vu16(vO));


	/* val =  val - ((~(val & 1)) & (val != 0)) */
	sel = vec_cmpgt(val, zero);
	vs16(sel) = vec_and(sel, one);
	t0 = vec_and(val, one);
	t0 = vec_andc(sel, t0);
	val = vec_sub(val, t0);

	/* val = min(val, 2047) */
	val = vec_min(val, saturation);

	/* val = samesign(src, val) */
	sel = vec_cmplt(vsrc, zero);
	t0 = vec_sub(zero, val);
	val = vec_sel(val, t0, sel);

	/* if (src[i] == 0) dst[i] = 0 */
	sel = vec_cmpeq(vsrc, zero);
	val = vec_andc(val, sel);

	vec_st(val, 0, dst);
	dst += 8;
    } while (--i);

    AMBER_STOP;
} /* }}} */
/*---------------------------------------------------------------------------*/
#endif

#if ALTIVEC_TEST_FUNCTION(iquant_non_intra_m1) /* {{{ */
#define IQUANT_NON_INTRA_M1_PFMT "src=0x%X, dst=0x%X, qmat=0x%X"

#  ifdef ALTIVEC_VERIFY
void iquant_non_intra_m1_altivec_verify(IQUANT_NON_INTRA_M1_PDECL)
{
    int i;
    unsigned long checksum1, checksum2;
    int16_t srccpy[64], dstcpy[64];

    /* in case src == dst */
    memcpy(srccpy, src, 64*sizeof(int16_t));

    iquant_non_intra_m1_altivec(IQUANT_NON_INTRA_M1_ARGS);
    for (checksum1 = i = 0; i < 64; i++)
	checksum1 += dst[i];

    memcpy(dstcpy, dst, 64*sizeof(int16_t));

    memcpy(src, srccpy, 64*sizeof(int16_t));

    ALTIVEC_TEST_WITH(iquant_non_intra_m1)(IQUANT_NON_INTRA_M1_ARGS);
    for (checksum2 = i = 0; i < 64; i++)
	checksum2 += dst[i];

    if (checksum1 != checksum2) {
	mjpeg_debug("iquant_non_intra_m1(" IQUANT_NON_INTRA_M1_PFMT ")",
	    IQUANT_NON_INTRA_M1_ARGS);
	mjpeg_debug("iquant_non_intra_m1: checksums differ %d != %d",
		    checksum1, checksum2);
    }

    for (i = 0; i < 64; i++) {
	if (dstcpy[i] != dst[i]) {
	    mjpeg_debug("iquant_non_intra_m1: src[%d]=%d, qmat=%d, "
			"dst %d != %d", i, srccpy[i], qmat[i],
			dstcpy[i], dst[i]);
	}
    }
}
#  else
ALTIVEC_TEST(iquant_non_intra_m1, void, (IQUANT_NON_INTRA_M1_PDECL),
    IQUANT_NON_INTRA_M1_PFMT, IQUANT_NON_INTRA_M1_ARGS);
#  endif
#endif /* }}} */
/* vim:set foldmethod=marker foldlevel=0: */
