/* quantize_ref.h, Low-level Architecture neutral quantization /
 * inverse quantization routines */

/*  (C) 2003 Andrew Stevens */

/* These modifications are free software; you can redistribute it
 *  and/or modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2 of
 *  the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 */

#ifndef _QUANTIZE_H
#define _QUANTIZE_H

#include "config.h"
#include "mjpeg_types.h"


extern int (*pquant_non_intra)( int16_t *src, int16_t *dst,
                                int q_scale_type, 
                                int *nonsat_mquant);
extern int (*pquant_weight_coeff_sum)(int16_t *blk, uint16_t*i_quant_mat );

extern void (*piquant_non_intra)(int16_t *src, int16_t *dst, int mquant );


#ifdef  __cplusplus
extern "C" {
#endif

void init_quantizer(void);

int next_larger_quant( int q_scale_type, int quant );
int quant_code(  int q_scale_type, int mquant );
void quant_intra( int16_t *src, int16_t *dst, 
				  int q_scale_type, int dc_prec,
				  int *nonsat_mquant);
int quant_non_intra( int16_t *src, int16_t *dst,
					 int q_scale_type,
					 int *nonsat_mquant);
void iquant_intra ( int16_t *src, int16_t *dst, int dc_prec, int mquant);
void iquant_non_intra (int16_t *src, int16_t *dst, int mquant);

int quant_weight_coeff_sum(int16_t *blk, uint16_t * i_quant_mat);
void iquant_non_intra_m1(int16_t *src, int16_t *dst, uint16_t *quant_mat);



#ifdef  __cplusplus
}
#endif

#endif /*  _QUANTIZE_H */

/* 
 * Local variables:
 *  c-file-style: "stroustrup"
 *  tab-width: 4
 *  indent-tabs-mode: nil
 * End:
 */
