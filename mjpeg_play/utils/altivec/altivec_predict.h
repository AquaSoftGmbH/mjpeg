/* altivec_predict.h, this file is part of the
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

#ifndef __INCLUDE_FROM_MPEG2ENC_PREDICT_C__
#include <stdio.h>  /* FILE required by global.h */
#include <stdint.h>
#include "../../mpeg2enc/mpeg2enc.h" /* pict_data_s */
#endif

#include "altivec_conf.h"

#define ALTIVEC_TEST_PREDICT /* {{{ */                                       \
    ( ( defined(ALTIVEC_BENCHMARK) || defined(ALTIVEC_VERIFY) ) &&           \
    ALTIVEC_TEST_FUNCTION(pred_comp) )                                       \
    /* }}} */

ALTIVEC_FUNCTION(pred_comp, void,
	(pict_data_s *picture, uint8_t *src, uint8_t *dst, int lx,
	 int w, int h, int x, int y, int dx, int dy, int addflag));
