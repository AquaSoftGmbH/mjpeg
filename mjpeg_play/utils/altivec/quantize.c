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

#ifdef HAVE_ALTIVEC_H
#include <altivec.h>
#endif

#include "altivec_quantize.h"
#include "../mjpeg_logging.h"


extern int (*pquant_non_intra)(int16_t *src, int16_t *dst, int q_scale_type,
				int mquant, int *nonsat_mquant);
extern int (*pquant_weight_coeff_sum)(int16_t *blk, uint16_t *i_quant_mat);

extern void (*piquant_non_intra)(int16_t *src, int16_t *dst, uint16_t *qmat);


void enable_altivec_quantization(int opt_mpeg1)
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

#if ALTIVEC_TEST_FUNCTION(quant_non_intra)
    pquant_non_intra = ALTIVEC_TEST_SUFFIX(quant_non_intra);
#else
    pquant_non_intra = ALTIVEC_SUFFIX(quant_non_intra);
#endif

#if ALTIVEC_TEST_FUNCTION(quant_weight_coeff_sum)
    pquant_weight_coeff_sum = ALTIVEC_TEST_SUFFIX(quant_weight_coeff_sum);
#else
    pquant_weight_coeff_sum = ALTIVEC_SUFFIX(quant_weight_coeff_sum);
#endif

    if (opt_mpeg1) {
#if ALTIVEC_TEST_FUNCTION(iquant_non_intra_m1)
	piquant_non_intra = ALTIVEC_TEST_SUFFIX(iquant_non_intra_m1);
#else
	piquant_non_intra = ALTIVEC_SUFFIX(iquant_non_intra_m1);
#endif
    }
}
