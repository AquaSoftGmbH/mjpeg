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

#include <stdio.h>  /* FILE required by global.h */
#include <stdint.h>
#include "../../mpeg2enc/mpeg2enc.h" /* pict_data_s */

#include "altivec_conf.h"

void enable_altivec_quantization();

#define ALTIVEC_TEST_QUANTIZE /* {{{ */                                      \
    ( ( defined(ALTIVEC_BENCHMARK) || defined(ALTIVEC_VERIFY) ) &&           \
      ( ALTIVEC_TEST_FUNCTION(quant_non_intra) ||                            \
        ALTIVEC_TEST_FUNCTION(quant_wieght_coeff_sum) ) )                    \
    /* }}} */

ALTIVEC_FUNCTION(quant_non_intra, int,
	(pict_data_s *picture, int16_t *src, int16_t *dst,
	 int mquant, int *nonsat_mquant));

ALTIVEC_FUNCTION(quant_weight_coeff_sum, int,
	(int16_t *blk, uint16_t *i_quant_mat));
