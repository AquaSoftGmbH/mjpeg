/* quant_non_intra.c, this file is part of the
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

#include "altivec_quantize.h"

#if defined(ALTIVEC_VERIFY) && ALTIVEC_TEST_FUNCTION(quant_non_intra)
#include <stdlib.h>
#endif

#include "vectorize.h"
#include "../fastintfns.h"
#include "../mjpeg_logging.h"

/* #define AMBER_ENABLE */
#include "amber.h"


extern uint16_t *opt_inter_q;
extern int opt_dctsatlim;
extern int block_count;


/*
 * The original C version would start-over from the beginning each time
 * clipping occurred (until saturated) which resulted in the possibility of
 * most dst[] values being re-calculated many times. This version, if clipping
 * is detected, restarts calculating from the current block. Once it's finished
 * it will re-calculate blocks that need it starting with block 0.
 */

#define QUANT_NON_INTRA_PDECL                                                \
    int16_t *src, int16_t *dst,                                              \
    int q_scale_type, int *nonsat_mquant                         \

#define  QUANT_NON_INTRA_ARGS src, dst, q_scale_type,  nonsat_mquant


int quant_non_intra_altivec(QUANT_NON_INTRA_PDECL)
{
    int mquant = *nonsat_mquant;
    int i, j, N, nzblockbits, last_block, recalc_blocks;
    unsigned short *popt_inter_q, *pqm;
    signed short *ps, *pd;
    vector unsigned short zero, four;
    vector float one;
    vector unsigned short qmA, qmB;		/* quant matrix */
    vector signed short srcA, srcB;		/* source */
    vector signed short dstA, dstB;		/* destination */
    vector float sA0, sA1, sB0, sB1;		/* dividend */
    vector float dA0, dA1, dB0, dB1;		/* divisor */
    vector float reA0, reA1, reB0, reB1;	/* reciprocal */
    vector float qtA0, qtA1, qtB0, qtB1;	/* quotient */
    vector float rmA0, rmA1, rmB0, rmB1;	/* remainder */
    vector bool short selA, selB;		/* bool selector */
    vector bool short nz;			/* non-zero */
    vector unsigned short max;			/* max value */
    vector unsigned short t1, t2, t3, t4;
    /* vuv & vu are used to share values between vector and scalar code.
     * vu lives on the stack and vuv is a vector register. Using vuv
     * instead of vu.v allows control over when read/writes to vu are done.
     */
    vector unsigned short vuv;
    union {
	/* do not use v, load vu into vuv for vector access. */
	vector unsigned short v;
	struct {
	    unsigned short mquant;
	    unsigned short clipvalue;
	    unsigned int nz;
	} s;
    } vu;


#ifdef ALTIVEC_VERIFY /* {{{ */
    if (NOT_VECTOR_ALIGNED(opt_inter_q))
	mjpeg_error_exit1("quant_non_intra: opt_inter_q %% 16 != 0, (%d)",
	    opt_inter_q);

    if (NOT_VECTOR_ALIGNED(src))
	mjpeg_error_exit1("quant_non_intra: src %% 16 != 0, (%d)", src);

    if (NOT_VECTOR_ALIGNED(dst))
	mjpeg_error_exit1("quant_non_intra: dst %% 16 != 0, (%d)", dst);
#endif /* }}} */


#define QUANT_NON_INTRA_AB /* {{{ */                                         \
    qmA = vec_ld(0, pqm);                                                    \
    pqm += 8;                                                                \
    qmB = vec_ld(0, pqm);                                                    \
    pqm += 8;                                                                \
    srcA = vec_ld(0, ps);                                                    \
    ps += 8;                                                                 \
    srcB = vec_ld(0, ps);                                                    \
    ps += 8;                                                                 \
									     \
    /* calculate divisor */                                                  \
    vu16(dA0) = vec_mergeh(zero, qmA);                                       \
    vu16(dA1) = vec_mergel(zero, qmA);                                       \
    vu16(dB0) = vec_mergeh(zero, qmB);                                       \
    vu16(dB1) = vec_mergel(zero, qmB);                                       \
    vuv = vec_ld(0, (unsigned short*)&vu);                                   \
    vuv = vec_splat(vuv, 0); /* splat mquant */                              \
    vu32(dA0)  = vec_mulo(vu16(dA0), vuv);                                   \
    vu32(dA1)  = vec_mulo(vu16(dA1), vuv);                                   \
    vu32(dB0)  = vec_mulo(vu16(dB0), vuv);                                   \
    vu32(dB1)  = vec_mulo(vu16(dB1), vuv);                                   \
    dA0  = vec_ctf(vu32(dA0), 0);                                            \
    dA1  = vec_ctf(vu32(dA1), 0);                                            \
    dB0  = vec_ctf(vu32(dB0), 0);                                            \
    dB1  = vec_ctf(vu32(dB1), 0);                                            \
    reA0 = vec_re(dA0);                                                      \
    reA1 = vec_re(dA1);                                                      \
    reB0 = vec_re(dB0);                                                      \
    reB1 = vec_re(dB1);                                                      \
									     \
    /* refinement #1 */                                                      \
    vfp(t1) = vec_nmsub(reA0, vfp(dA0), vfp(one));                           \
    vfp(t2) = vec_nmsub(reA1, vfp(dA1), vfp(one));                           \
    vfp(t3) = vec_nmsub(reB0, vfp(dB0), vfp(one));                           \
    vfp(t4) = vec_nmsub(reB1, vfp(dB1), vfp(one));                           \
    reA0 = vec_madd(reA0, vfp(t1), reA0);                                    \
    reA1 = vec_madd(reA1, vfp(t2), reA1);                                    \
    reB0 = vec_madd(reB0, vfp(t3), reB0);                                    \
    reB1 = vec_madd(reB1, vfp(t4), reB1);                                    \
									     \
    /* refinement #2 */                                                      \
    vfp(t1) = vec_nmsub(reA0, vfp(dA0), vfp(one));                           \
    vfp(t2) = vec_nmsub(reA1, vfp(dA1), vfp(one));                           \
    vfp(t3) = vec_nmsub(reB0, vfp(dB0), vfp(one));                           \
    vfp(t4) = vec_nmsub(reB1, vfp(dB1), vfp(one));                           \
    reA0 = vec_madd(reA0, vfp(t1), reA0);                                    \
    reA1 = vec_madd(reA1, vfp(t2), reA1);                                    \
    reB0 = vec_madd(reB0, vfp(t3), reB0);                                    \
    reB1 = vec_madd(reB1, vfp(t4), reB1);                                    \
									     \
    /* (sA0,sB0) = abs(ps[n],ps[n+1]) << 4 {{{ */                            \
    vs16(t1) = vec_subs(vs16(zero), srcA);                                   \
    vs16(t2) = vec_subs(vs16(zero), srcB);                                   \
    vs16(t3) = vec_max(srcA, vs16(t1));                                      \
    vs16(t4) = vec_max(srcB, vs16(t2));                                      \
    four = vec_splat_u16(4);                                                 \
    vu16(t1) = vec_sl(vu16(t3), four);                                       \
    vu16(t2) = vec_sl(vu16(t4), four);                                       \
    /* }}} */                                                                \
									     \
    vu16(sA0) = vec_mergeh(zero, vu16(t1));                                  \
    vu16(sA1) = vec_mergel(zero, vu16(t1));                                  \
    vu16(sB0) = vec_mergeh(zero, vu16(t2));                                  \
    vu16(sB1) = vec_mergel(zero, vu16(t2));                                  \
    vfp(sA0) = vec_ctf(vu32(sA0), 0);                                        \
    vfp(sA1) = vec_ctf(vu32(sA1), 0);                                        \
    vfp(sB0) = vec_ctf(vu32(sB0), 0);                                        \
    vfp(sB1) = vec_ctf(vu32(sB1), 0);                                        \
									     \
    /* calculate quotient */                                                 \
    vfp(qtA0)  = vec_madd(vfp(sA0), reA0, vfp(zero));                        \
    vfp(qtA1)  = vec_madd(vfp(sA1), reA1, vfp(zero));                        \
    vfp(qtB0)  = vec_madd(vfp(sB0), reB0, vfp(zero));                        \
    vfp(qtB1)  = vec_madd(vfp(sB1), reB1, vfp(zero));                        \
									     \
    /* calculate remainder */                                                \
    vfp(rmA0)  = vec_nmsub(vfp(dA0), vfp(qtA0), vfp(sA0));                   \
    vfp(rmA1)  = vec_nmsub(vfp(dA1), vfp(qtA1), vfp(sA1));                   \
    vfp(rmB0)  = vec_nmsub(vfp(dB0), vfp(qtB0), vfp(sB0));                   \
    vfp(rmB1)  = vec_nmsub(vfp(dB1), vfp(qtB1), vfp(sB1));                   \
									     \
    /* round quotient with remainder */                                      \
    vfp(qtA0)  = vec_madd(vfp(rmA0), reA0, vfp(qtA0));                       \
    vfp(qtA1)  = vec_madd(vfp(rmA1), reA1, vfp(qtA1));                       \
    vfp(qtB0)  = vec_madd(vfp(rmB0), reB0, vfp(qtB0));                       \
    vfp(qtB1)  = vec_madd(vfp(rmB1), reB1, vfp(qtB1));                       \
									     \
    /* convert to integer */                                                 \
    vu32(qtA0) = vec_ctu(vfp(qtA0), 0);                                      \
    vu32(qtA1) = vec_ctu(vfp(qtA1), 0);                                      \
    vu32(qtB0) = vec_ctu(vfp(qtB0), 0);                                      \
    vu32(qtB1) = vec_ctu(vfp(qtB1), 0);                                      \
									     \
    vu16(dstA) = vec_pack(vu32(qtA0), vu32(qtA1));                           \
    vu16(dstB) = vec_pack(vu32(qtB0), vu32(qtB1));                           \
									     \
    /* test for non-zero values */                                           \
    selA = vec_cmpgt(vu16(dstA), zero);                                      \
    selB = vec_cmpgt(vu16(dstB), zero);                                      \
    nz = vec_or(nz, selA);                                                   \
    nz = vec_or(nz, selB);                                                   \
    /* }}} */


#define SIGN_AND_STORE /* {{{ */                                             \
    /* sign dst blocks */                                                    \
    selA = vec_cmpgt(vs16(zero), srcA);                                      \
    selB = vec_cmpgt(vs16(zero), srcB);                                      \
    vs16(t1) = vec_subs(vs16(zero), dstA);                                   \
    vs16(t2) = vec_subs(vs16(zero), dstB);                                   \
    dstA = vec_sel(dstA, vs16(t1), selA);                                    \
    dstB = vec_sel(dstB, vs16(t2), selB);                                    \
									     \
    /* store dst blocks */                                                   \
    vec_st(dstA, 0, pd);                                                     \
    pd += 8;                                                                 \
    vec_st(dstB, 0, pd);                                                     \
    pd += 8;                                                                 \
    /* }}} */


#define UPDATE_NZBLOCKBITS /* {{{ */                                         \
    /* quasi-count the non-zero values and store to vu.s.nz */               \
    vs32(nz) = vec_sums(vs32(nz), vs32(zero));                               \
    vu32(nz) = vec_splat(vu32(nz), 3);                                       \
    vuv = vec_ld(0, (unsigned short*)&vu);                                   \
    /* vuv = ( vuv(mquant, clipvalue), nz, (), () ) */                       \
    vu32(vuv) = vec_mergeh(vu32(vuv), vu32(nz));                             \
    vec_st(vuv, 0, (unsigned short*)&vu); /* store for scalar access */      \
    nzblockbits |= ((!!vu.s.nz) << i);    /* set non-zero block bit */       \
    /* }}} */


    AMBER_START;

/* 0x01080010 = ((1<<24) & 0x1F000000)|((8<<16) & 0x00FF0000)|(16 & 0xFFFF) */
    popt_inter_q = (unsigned short*)opt_inter_q;
    ps = src;
    pd = dst;

#ifdef ALTIVEC_DST
    vec_dst(popt_inter_q, 0x01080010, 0);
    vec_dst(ps, 0x01040010, 1);
    vec_dstst(pd, 0x01040010, 2);
#endif

    vu.s.mquant = mquant;
    vu.s.clipvalue = opt_dctsatlim;

    zero = vec_splat_u16(0);
    vu32(one) = vec_splat_u32(1);
    one = vec_ctf(vu32(one), 0);

    nzblockbits = 0;
    N = block_count;
    i = N;
    last_block = 0; /* counting down from i */
    recalc_blocks = 0;

    do {
recalc:
	pqm = popt_inter_q;
	vu16(nz) = vec_splat_u16(0);
	max = vec_splat_u16(0);
	j = 4;
	do {
#ifdef ALTIVEC_DST
	    vec_dst(ps, 0x01040010, 1);
	    vec_dstst(pd, 0x01040010, 2);
#endif
	    QUANT_NON_INTRA_AB;

	    max = vec_max(max, vu16(dstA));
	    max = vec_max(max, vu16(dstB));

	    SIGN_AND_STORE;
	} while (--j);

	/* check for clipping/saturation {{{ */
	vuv = vec_ld(0, (unsigned short*)&vu);
	vuv = vec_splat(vuv, 1); /* splat clipvalue */
	if (vec_any_gt(max, vuv)) {
	    int next_mquant = next_larger_quant(q_scale_type, mquant);
    	    if (next_mquant == mquant) {
		/* saturation has occured, clip values then
		 * goto saturated jump point.
		 */
		pd -= 8*8; /* reset pointer to beginning of block */
		j = 4;
		do {
		    srcA = vec_ld(0, pd);
		    pd += 8;
		    srcB = vec_ld(0, pd);
		    /* (dstA,dstB) = abs(srcA,srcB) {{{ */
		    vs16(t1) = vec_subs(vs16(zero), srcA);
		    vs16(t2) = vec_subs(vs16(zero), srcB);
		    dstA = vec_max(srcA, vs16(t1));
		    dstB = vec_max(srcB, vs16(t2));
		    /* }}} */
		    /* (dstA,dstB) = clip(dstA,dstB, vuv) {{{ */
		    vu16(dstA) = vec_min(vu16(dstA), vuv);
		    vu16(dstB) = vec_min(vu16(dstB), vuv);
		    /* }}} */
		    /* restore sign {{{ */
		    selA = vec_cmpgt(vs16(zero), srcA);
		    selB = vec_cmpgt(vs16(zero), srcB);
		    vs16(t1) = vec_subs(vs16(zero), dstA);
		    vs16(t2) = vec_subs(vs16(zero), dstB);
		    dstA = vec_sel(dstA, vs16(t1), selA);
		    dstB = vec_sel(dstB, vs16(t2), selB);
		    /* }}} */
		    pd -= 8;
		    vec_st(dstA, 0, pd);
		    pd += 8;
		    vec_st(dstB, 0, pd);
		    pd += 8;
		} while (--j);

		goto saturated;
	    }

	    /* load new (int)mquant into (short)vu.s.mquant */
	    mquant = next_mquant;
	    vu.s.mquant = next_mquant;

	    nzblockbits = 0;
	    recalc_blocks = N - i;

	    /* reset pointers to beginning of block */
	    ps -= 8*8;
	    pd -= 8*8;

	    goto recalc;
	}
	/* }}} */

	i--;
	UPDATE_NZBLOCKBITS;

    } while (i);


    /* recalculate blocks if necessary. this branch of code does not
     * need to worry about saturation or clipping.
     */
    if (recalc_blocks > 0) {
	i = N;
	last_block = N - recalc_blocks;
	ps = src;
	pd = dst;

	do {
	    pqm = popt_inter_q;
	    vu16(nz) = vec_splat_u16(0);
	    j = 4;
	    do {
#ifdef ALTIVEC_DST
		vec_dst(ps, 0x01040010, 1);
		vec_dstst(pd, 0x01040010, 2);
#endif
		QUANT_NON_INTRA_AB;
		SIGN_AND_STORE;
	    } while (--j);

	    i--;
	    UPDATE_NZBLOCKBITS;

	} while (i > last_block);
    }

    goto done;

    /*
     * the following code is entered at the label 'saturated'.
     * this branch of code clips the destination values.
     */
    do {
	i = N;
	last_block = N - recalc_blocks;
	ps = src;
	pd = dst;
	recalc_blocks = 0; /* no more blocks after next loop */

	do {
	    pqm = popt_inter_q;
	    vu16(nz) = vec_splat_u16(0);
	    j = 4;
	    do {
#ifdef ALTIVEC_DST
		vec_dst(ps, 0x01040010, 1);
		vec_dstst(pd, 0x01040010, 2);
#endif
		QUANT_NON_INTRA_AB;
		/* clip {{{ */
		vuv = vec_ld(0, (unsigned short*)&vu);
		vuv = vec_splat(vuv, 1); /* splat clipvalue */
		vu16(dstA) = vec_min(vu16(dstA), vuv);
		vu16(dstB) = vec_min(vu16(dstB), vuv);
		/* }}} */
		SIGN_AND_STORE;
	    } while (--j);
saturated:
	    i--;
	    UPDATE_NZBLOCKBITS;

	} while (i > last_block);

    } while (recalc_blocks > 0);

done:

#ifdef ALTIVEC_DST
    vec_dssall();
#endif

    *nonsat_mquant = mquant;

    AMBER_STOP;

    return nzblockbits;
}


#if ALTIVEC_TEST_FUNCTION(quant_non_intra) /* {{{ */
#define QUANT_NON_INTRA_PFMT \
  "src=0x%X, dst=0x%X, q_scale_type=%d, mquant=%d, nonsat_mquant=0x%X"

#  ifdef ALTIVEC_VERIFY
int quant_non_intra_altivec_verify(QUANT_NON_INTRA_PDECL)
{
    int mquant = *nonsat_mquant;
    int i, len, nzb1, nzb2, nsmq, nsmq1, nsmq2;
    unsigned long checksum1, checksum2;
    int16_t *dstcpy;

    len = 64 * block_count;

    dstcpy = (int16_t*)malloc(len*sizeof(int16_t));
    if (dstcpy == NULL)
	mjpeg_error_exit1("quant_non_intra_verify: unable to malloc");

    /* save nonsat_mquant */
    nsmq = *nonsat_mquant;

    nzb1 = quant_non_intra_altivec(QUANT_NON_INTRA_ARGS);
    nsmq1 = *nonsat_mquant;
    for (checksum1 = i = 0; i < len; i++)
	checksum1 += intabs(dst[i]);

    memcpy(dstcpy, dst, len*sizeof(int16_t));

    /* restore nonsat_mquant */
    *nonsat_mquant = nsmq;

    nzb2 = ALTIVEC_TEST_WITH(quant_non_intra)(QUANT_NON_INTRA_ARGS);
    nsmq2 = *nonsat_mquant;
    for (checksum2 = i = 0; i < len; i++)
	checksum2 += intabs(dst[i]);

    if (nzb1 != nzb2 || checksum1 != checksum2 || nsmq1 != nsmq2) {
	mjpeg_debug("quant_non_intra(" QUANT_NON_INTRA_PFMT ")",
	    QUANT_NON_INTRA_ARGS);
	mjpeg_debug("quant_non_intra: results differ "
		    "{nzb=%d, checksum=%d, mquant=%d} != "
		    "{nzb=%d, checksum=%d, mquant=%d}",
		    nzb1, checksum1, nsmq1, nzb2, checksum2, nsmq2);
    }

    for (i = 0; i < len; i++) {
	if (dstcpy[i] != dst[i]) {
	    mjpeg_debug("quant_non_intra: src[%d]=%d, qmat=%d, "
			"dst %d != %d",
			i, src[i], opt_inter_q[i&63], dstcpy[i], dst[i]);
	}
    }

    free(dstcpy);

    return nzb2;
}
#  else
ALTIVEC_TEST(quant_non_intra, int, (QUANT_NON_INTRA_PDECL),
    QUANT_NON_INTRA_PFMT, QUANT_NON_INTRA_ARGS);
#  endif
#endif /* }}} */
/* vim:set foldmethod=marker foldlevel=0: */
