/* altivec_quantize.h, this file is part of the
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

#include <stdint.h>
#include "../../mpeg2enc/quantize_ref.h" /* next_larger_quant */

#include "altivec_conf.h"


#define ALTIVEC_TEST_QUANTIZE /* {{{ */                                      \
    ( ( defined(ALTIVEC_BENCHMARK) || defined(ALTIVEC_VERIFY) ) &&           \
      ( ALTIVEC_TEST_FUNCTION(quant_non_intra) ||                            \
        ALTIVEC_TEST_FUNCTION(quant_weight_coeff_sum) ||                     \
        ALTIVEC_TEST_FUNCTION(iquant_non_intra_m1) ) )                       \
    /* }}} */


#ifdef __cplusplus
extern "C" {
#endif

void enable_altivec_quantization(int opt_mpeg1);

ALTIVEC_FUNCTION(quant_non_intra, int,
	(int16_t *src, int16_t *dst,
	 int q_scale_type, int *nonsat_mquant));

ALTIVEC_FUNCTION(quant_weight_coeff_sum, int,
	(int16_t *blk, uint16_t *i_quant_mat));

ALTIVEC_FUNCTION(iquant_non_intra_m1, void,
	(int16_t *src, int16_t *dst, uint16_t *qmat));

#ifdef __cplusplus
}
#endif
