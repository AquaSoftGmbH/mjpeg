/* Copyright (C) 2000 Oregon Graduate Institute of Science & Technology
 *
 * See the file COPYRIGHT, which should have been supplied with this
 * package, for licensing details.  We may be contacted through email
 * at <quasar-help@cse.ogi.edu>.
 */

#ifndef __QUANT_H
#define __QUANT_H

#include "dv.h"

void quant_88(dv_coeff_t *block,int qno,int class);
void quant_248(dv_coeff_t *block,int qno,int class);
void quant_88_inverse(dv_coeff_t *block,int qno,int class);
void quant_248_inverse(dv_coeff_t *block,int qno,int class);

#endif // __QUANT_H
