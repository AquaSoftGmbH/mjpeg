/* transfrm_ref.h, Low-level Architecture neutral transformation
 * (DCT/iDCT) routines */

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


int field_dct_best( uint8_t *cur_lum_mb, uint8_t *pred_lum_mb);

void add_pred (uint8_t *pred, uint8_t *cur,
               int lx, int16_t *blk);
void sub_pred (uint8_t *pred, uint8_t *cur,
               int lx, int16_t *blk);


void fdct( int16_t *blk );
void idct( int16_t *blk );
void init_fdct (void);
void init_idct (void);

/* 
 * Local variables:
 *  c-file-style: "stroustrup"
 *  tab-width: 4
 *  indent-tabs-mode: nil
 * End:
 */
