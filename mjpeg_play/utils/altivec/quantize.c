/* quantize.c, this file is part of the
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

#include "altivec_quantize.h"
#include "vectorize.h"
#include "../mjpeg_logging.h"


extern int (*pquant_non_intra)(int16_t *src, int16_t *dst, int q_scale_type,
                               int dctsatlim, int *nonsat_mquant);
extern int (*pquant_weight_coeff_intra)(int16_t *blk);
extern int (*pquant_weight_coeff_inter)(int16_t *blk);
extern void (*piquant_non_intra)(int16_t *src, int16_t *dst, int mquant);


vector unsigned short *intra_q_altivec;
vector unsigned short *inter_q_altivec;


void enable_altivec_quantization(int opt_mpeg1, uint16_t *intra_q,
                                 uint16_t *inter_q)
{
#if ALTIVEC_TEST_QUANTIZE
#  if defined(ALTIVEC_BENCHMARK)
    mjpeg_info("SETTING AltiVec BENCHMARK for QUANTIZER!");
#  elif defined(ALTIVEC_VERIFY)
    mjpeg_info("SETTING AltiVec VERIFY for QUANTIZER!");
#  endif
#else
    mjpeg_info("SETTING AltiVec for QUANTIZER!");
#endif

#ifdef ALTIVEC_VERIFY /* {{{ */
    if (NOT_VECTOR_ALIGNED(intra_q))
	mjpeg_error_exit1("AltiVec quantize: intra_q %% 16 != 0, (%d)",
	    intra_q);
    if (NOT_VECTOR_ALIGNED(inter_q))
	mjpeg_error_exit1("AltiVec quantize: inter_q %% 16 != 0, (%d)",
	    inter_q);
#endif /* }}} */

    intra_q_altivec = (vector unsigned short*)intra_q;
    inter_q_altivec = (vector unsigned short*)inter_q;

#if ALTIVEC_TEST_FUNCTION(quant_non_intra)
    pquant_non_intra = ALTIVEC_TEST_SUFFIX(quant_non_intra);
#else
    pquant_non_intra = ALTIVEC_SUFFIX(quant_non_intra);
#endif

#if ALTIVEC_TEST_FUNCTION(quant_weight_coeff_intra)
    pquant_weight_coeff_intra = ALTIVEC_TEST_SUFFIX(quant_weight_coeff_intra);
#else
    pquant_weight_coeff_intra = ALTIVEC_SUFFIX(quant_weight_coeff_intra);
#endif

#if ALTIVEC_TEST_FUNCTION(quant_weight_coeff_inter)
    pquant_weight_coeff_inter = ALTIVEC_TEST_SUFFIX(quant_weight_coeff_inter);
#else
    pquant_weight_coeff_inter = ALTIVEC_SUFFIX(quant_weight_coeff_inter);
#endif

    if (opt_mpeg1) {
#if ALTIVEC_TEST_FUNCTION(iquant_non_intra_m1)
	piquant_non_intra = ALTIVEC_TEST_SUFFIX(iquant_non_intra_m1);
#else
	piquant_non_intra = ALTIVEC_SUFFIX(iquant_non_intra_m1);
#endif
#if ALTIVEC_TEST_FUNCTION(iquant_intra_m1)
	piquant_intra = ALTIVEC_TEST_SUFFIX(iquant_intra_m1);
#else
	piquant_intra = ALTIVEC_SUFFIX(iquant_intra_m1);
#endif
    } else {
#if ALTIVEC_TEST_FUNCTION(iquant_non_intra_m2)
	piquant_non_intra = ALTIVEC_TEST_SUFFIX(iquant_non_intra_m2);
#else
	piquant_non_intra = ALTIVEC_SUFFIX(iquant_non_intra_m2);
#endif
#if ALTIVEC_TEST_FUNCTION(iquant_intra_m2)
	piquant_intra = ALTIVEC_TEST_SUFFIX(iquant_intra_m2);
#else
	piquant_intra = ALTIVEC_SUFFIX(iquant_intra_m2);
#endif
    }
}

/* vim:set foldmethod=marker foldlevel=0: */
