/* 
 *  parce.c
 *
 *     Copyright (C) Charles 'Buck' Krasic - April 2000
 *     Copyright (C) Erik Walthinsen - April 2000
 *
 *  This file is part of libdv, a free DV (IEC 61834/SMPTE 314M)
 *  decoder.
 *
 *  libdv is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your
 *  option) any later version.
 *   
 *  libdv is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *   
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 
 *
 *  The libdv homepage is http://libdv.sourceforge.net/.  
 */

#include <glib.h>
#include <stdio.h>
#include <assert.h>

#include "bitstream.h"
#include "dv.h"
#include "vlc.h"
#include "parse.h"

#define STRICT_SYNTAX 0
#define VLC_BOUNDS_CHECK 0

#define PARSE_VLC_TRACE 0
#if PARSE_VLC_TRACE
#define vlc_trace(format,args...) fprintf(stdout,format,##args)
#else 
#define vlc_trace(format,args...) 
#endif 

// Assign coefficient in zigzag order without indexing multiply
#define ZERO_MULT_ZIGZAG 1
#if ZERO_MULT_ZIGZAG
#define SET_COEFF(COEFFS,REORDER,VALUE) (*((dv_coeff_t *)(((guint8 *)(COEFFS)) + *(REORDER)++)) = (VALUE))
#else
#define SET_COEFF(COEFFS,REORDER,VALUE) COEFFS[*REORDER++] = VALUE
#endif

gint    dv_parse_bit_start[6] = { 4*8+12,  18*8+12, 32*8+12, 46*8+12, 60*8+12, 70*8+12 };
gint    dv_parse_bit_end[6]   = { 18*8,    32*8,    46*8,    60*8,    70*8,    80*8 };

gint     dv_super_map_vertical[5] = { 2, 6, 8, 0, 4 };
gint     dv_super_map_horizontal[5] = { 2, 1, 3, 0, 4 };

static gint8  dv_88_reorder_prime[64] = {
0, 1, 8, 16, 9, 2, 3, 10,		17, 24, 32, 25, 18, 11, 4, 5,
12, 19, 26, 33, 40, 48, 41, 34,		27, 20, 13, 6, 7, 14, 21, 28,
35, 42, 49, 56, 57, 50, 43, 36,		29, 22, 15, 23, 30, 37, 44, 51,
58, 59, 52, 45, 38, 31, 39, 46,		53, 60, 61, 54, 47, 55, 62, 63 
};

gint8  dv_reorder[2][64] = {
  { 0 },
  {
    0, 32, 1, 33, 8, 40, 2, 34,		9, 41, 16, 48, 24, 56, 17, 49,
    10, 42, 3, 35, 4, 36, 11, 43,		18, 50, 25, 57, 26, 58, 19, 51,
    12, 44, 5, 37, 6, 38, 13, 45,		20, 52, 27, 59, 28, 60, 21, 53,
    14, 46, 7, 39, 15, 47, 22, 54,		29, 61, 30, 62, 23, 55, 31, 63 }
};


void dv_parse_init(void) 
{
	gint i;

// Recursive math.
	for(i = 0; i < 64; i++)
	{
    	dv_reorder[DV_DCT_88][i] = ((dv_88_reorder_prime[i] % 8) * 8) + (dv_88_reorder_prime[i] / 8);
	} // for 

	for(i = 0; i < 64; i++)
	{
#if ZERO_MULT_ZIGZAG
    	dv_reorder[DV_DCT_88][i] = (dv_reorder[DV_DCT_88][i]) * sizeof(dv_coeff_t);
    	dv_reorder[DV_DCT_248][i] = (dv_reorder[DV_DCT_248][i]) * sizeof(dv_coeff_t);
#endif
	} // for
} // dv_parse_init

// Scan the blocks of a macroblock.  We're looking to find the next
// block from which unused space was borrowed
static gboolean dv_find_mb_unused_bits(dv_macroblock_t *mb, dv_block_t **lender) {
  dv_block_t *bl;
  gint b;

  for(b=0,bl=mb->b;
      b<6;
      b++,bl++) {
    if((bl->eob) &&    /* an incomplete block can only "borrow" bits
			* from other blocks that are themselves
                        * already completely decoded */
       (bl->end > bl->offset) && // the lender must have unused bits 
       (!bl->mark)) {  // the lender musn't already be lending...
      bl->mark = TRUE;
      *lender = bl;
      return(TRUE);
    } // if 
  } // for b
  return(FALSE);
} // dv_find_mb_unused_bits

// After parsing vlcs from borrowed space, we must clear the trail of
// marks we used to track lenders.  found_vlc indicates whether the
// scanning process successfully found a complete vlc.  If it did,
// then we update all blocks that lent bits as having no bits left.
// If so, the last block gets fixed in the caller.  
static void dv_clear_mb_marks(dv_macroblock_t *mb, gboolean found_vlc) { 
  dv_block_t *bl; gint b;

  for(b=0,bl=mb->b;
      b<6;
      b++,bl++) {
    if(bl->mark) {
      bl->mark = FALSE;
      if(found_vlc) bl->offset = bl->end;
    }
  }
} // dv__clear_mb_marks

// For pass 3, we scan all blocks of a video segment for unused bits 
static gboolean dv_find_vs_unused_bits(dv_videosegment_t *seg, dv_block_t **lender) {
  dv_macroblock_t *mb;
  gint m;
  
  for(m=0,mb=seg->mb;
      m<5;
      m++,mb++) {
    if((mb->eob_count == 6) && // We only borrow bits from macroblocks that are themselves complete
       dv_find_mb_unused_bits(mb,lender)) 
      return(TRUE);
  } // for
  return(FALSE);
} // dv_find_vs_unused_bits

// For pass 3, the trail of lenders can span the whole video segment
static void dv_clear_vs_marks(dv_videosegment_t *seg,gboolean found_vlc) {
  dv_macroblock_t *mb;
  gint m;
  
  for(m=0,mb=seg->mb;
      m<5;
      m++,mb++) 
    dv_clear_mb_marks(mb,found_vlc);
} // dv_clear_vs_marks

// For passes 2 and 3, vlc data that didn't fit in the area of a block
// are put in space borrowed from other blocks.  Pass 2 borrows from
// blocks of the same macroblock.  Pass 3 uses space from blocks of
// other macroblocks of the videosegment.
static gint dv_find_spilled_vlc(dv_videosegment_t *seg, dv_macroblock_t *mb, dv_block_t **bl_lender, gint pass) {
  dv_vlc_t vlc;
  dv_block_t *bl_new_lender;
  gboolean found_vlc, found_bits;
  gint bits_left, found_bits_left;
  gint save_offset = 0;
  gint broken_vlc = 0;
  bitstream_t *bs;

  bs = seg->bs;
  if((bits_left = (*bl_lender)->end - (*bl_lender)->offset))
    broken_vlc=bitstream_get(bs,bits_left);
  found_vlc = FALSE;
  found_bits = dv_find_mb_unused_bits(mb,&bl_new_lender);
  while(found_bits && (!found_vlc)) {
    bitstream_seek_set(bs, bl_new_lender->offset);
    found_bits_left = bl_new_lender->end - bl_new_lender->offset;
    if(bits_left) // prepend broken vlc if there is one
      bitstream_unget(bs, broken_vlc, bits_left);
    dv_peek_vlc(bs, found_bits_left + bits_left, &vlc);
    if(vlc.len >= 0) {
      // found a vlc, set things up to return to the main coeff loop
      save_offset = bl_new_lender->offset - bits_left;
      found_vlc = TRUE;
    } else if(vlc.len == VLC_NOBITS) {
      // still no vlc
      bits_left += found_bits_left;
      broken_vlc = bitstream_get(bs, bits_left);
      if(pass == 1) 
	found_bits = dv_find_mb_unused_bits(mb,&bl_new_lender);
      else if(!(found_bits = dv_find_mb_unused_bits(mb,&bl_new_lender)))
	found_bits = dv_find_vs_unused_bits(seg,&bl_new_lender);
    } else {
      if(pass == 1) dv_clear_mb_marks(mb,found_vlc); 
      else dv_clear_vs_marks(seg,found_vlc);
      return(-1);
    } // else
  } // while
  if(pass == 1) dv_clear_mb_marks(mb,found_vlc); 
  else dv_clear_vs_marks(seg,found_vlc);
  if(found_vlc) {
    bl_new_lender->offset = save_offset; // fixup offset clobbered by clear marks 
    *bl_lender = bl_new_lender;
  }
  return(found_vlc);
} // dv_find_spilled_vlc


gint dv_parse_ac_coeffs(dv_videosegment_t *seg) {
  dv_vlc_t         vlc;
  gint             m, b, pass;
  gint             bits_left;
  gboolean         vlc_error;
  gint8           **reorder, *reorder_sentinel;
  dv_coeff_t      *coeffs;
  dv_macroblock_t *mb;
  dv_block_t      *bl, *bl_bit_source;
  bitstream_t     *bs;

  bs = seg->bs;
  // Phase 2:  do the 3 pass AC vlc decode
  vlc_error = FALSE;
  for (pass=1;pass<3;pass++) {
    vlc_trace("P%d",pass);
    if((pass == 2) && vlc_error) break;
    for (m=0,mb=seg->mb;
	 m<5;
	 m++,mb++) {
      // if(vlc_error) goto abort_segment;
      vlc_trace("\nM%d",m);
      if((pass == 1) && mb->vlc_error) continue;
      for (b=0,bl=mb->b;
	   b<6;
	   b++,bl++) {
	if(bl->eob) continue;
	coeffs = bl->coeffs;
	vlc_trace("\nB%d",b);
	bitstream_seek_set(bs,bl->offset);
	reorder = &bl->reorder;
	reorder_sentinel = bl->reorder_sentinel;
	bl_bit_source = bl;
	// Main coeffient parsing loop
	while(1) {
	  bits_left = bl_bit_source->end - bl_bit_source->offset;
	  dv_peek_vlc(bs,bits_left,&vlc);
	  if(vlc.run < 0) goto bh;
	  // complete, valid vlc found
	  vlc_trace("(%d,%d,%d)",vlc.run,vlc.amp,vlc.len);
	  bl_bit_source->offset += vlc.len;
	  bitstream_flush(bs,vlc.len);
	  *reorder += vlc.run;
	  if((*reorder) < reorder_sentinel) {
	    SET_COEFF(coeffs,(*reorder),vlc.amp);
	  } else {
	    vlc_trace("! vlc code overran coeff block");
	    goto vlc_error;
	  } // else 
	  continue;
	bh:
	  if(vlc.amp == 0) {
	    // found eob vlc
	    vlc_trace("#");
	    // EOb -- zero out the rest of block
	    *reorder = reorder_sentinel;
	    bl_bit_source->offset += 4;
	    bitstream_flush(bs,4);
	    bl->eob = 1;
	    mb->eob_count++;
	    break;
	  } else if(vlc.len == VLC_NOBITS) {
	    bl_bit_source->mark = TRUE;
	    switch(dv_find_spilled_vlc(seg,mb,&bl_bit_source,pass)) {
	    case 1: // found: keep parsing
	      break;
	    case 0: // not found: move on to next block
	      if(pass==1) goto mb_done;
	      else goto seg_done;
	      break;
	    case -1: 
	      goto vlc_error;
	      break;
	    } // switch
	  } else if(vlc.len == VLC_ERROR) {
	    goto vlc_error;
	  } // else (used == ?) 
	} // while !bl->eob
	goto bl_done;
  vlc_error:
	vlc_trace("X\n");
	vlc_error = TRUE;
	if(pass == 1) goto mb_done;
	else goto abort_segment;
  bl_done:
      } // for b 
  mb_done: 
    } // for m 
    vlc_trace("\n");
  } // for pass 
  vlc_trace("Segment OK.  ");
  goto seg_done;
 abort_segment:
  vlc_trace("Segment aborted. ");
 seg_done:
  return(0);
} // dv_parse_ac_coeffs

void dv_parse_ac_coeffs_pass0(bitstream_t *bs,
			      dv_macroblock_t *mb,
			      dv_block_t *bl);

__inline__ void dv_parse_ac_coeffs_pass0(bitstream_t *bs,
						dv_macroblock_t *mb,
						dv_block_t *bl) {
  dv_vlc_t         vlc;
  gint             bits_left;
  guint32 bits;

  vlc_trace("\nB%d",b);
  // Main coeffient parsing loop
  memset(&bl->coeffs[1],'\0',sizeof(bl->coeffs)-sizeof(bl->coeffs[0]));
  while(1) {
    bits_left = bl->end - bl->offset;
    bits = bitstream_show(bs,16);
    if(bits_left >= 16) 
      __dv_decode_vlc(bits, &vlc);
    else
      dv_decode_vlc(bits, bits_left,&vlc);
    if(vlc.run < 0) break;
    // complete, valid vlc found
    vlc_trace("(%d,%d,%d)",vlc.run,vlc.amp,vlc.len);
    bl->offset += vlc.len;
    bitstream_flush(bs,vlc.len);
    bl->reorder += vlc.run;
    SET_COEFF(bl->coeffs,bl->reorder,vlc.amp);
  } // while
  if(vlc.amp == 0) {
    // found eob vlc
    vlc_trace("#");
    // EOb -- zero out the rest of block
    bl->reorder = bl->reorder_sentinel;
    bl->offset += 4;
    bitstream_flush(bs,4);
    bl->eob = 1;
    mb->eob_count++;
  } else if(vlc.len == VLC_ERROR) {
    vlc_trace("X\n");
    mb->vlc_error = TRUE;
  } // else
  vlc_trace("\n");
} // dv_parse_ac_coeffs_pass0

/* DV requires vlc decode of AC coefficients for each block in three passes:
 *    Pass1 : decode coefficient vlc bits from their own block's area
 *    Pass2 : decode coefficient vlc bits spilled into areas of other blocks of the same macroblock
 *    Pass3 : decode coefficient vlc bits spilled into other macroblocks of the same video segment
 *
 * What to do about VLC errors?  This is tricky.
 *
 * The most conservative is to assume that any further spilled data is suspect.
 * So, on pass 1, a VLC error means do the rest, and skip passes 2 & 3
 * On passes 2 & 3, just abort.  This seems to drop a lot more coefficients, 21647 
 * in a single frame, that more tolerant aproaches.
 *
 *  */
gint dv_parse_video_segment(dv_videosegment_t *seg) {
  gint             m, b;
  gint             mb_start;
  gint             dc;
  dv_macroblock_t *mb;
  dv_block_t      *bl;
  bitstream_t     *bs;

  vlc_trace("S[%d,%d]\n", seg->i,seg->k);
// Phase 1:  initialize data structures, and get the DC
	bs = seg->bs;
	for(m = 0, mb = seg->mb, mb_start = 0; 
		m < 5; 
		m++, mb++, mb_start += (80 * 8))
	{
/* #if STRICT_SYNTAX */
    	bitstream_seek_set(bs,mb_start);
    	if(bitstream_get(bs, 3) != DV_SCT_VIDEO) return 6 * 64; // SCT
//    	g_return_val_if_fail(((bitstream_get(bs, 3) == DV_SCT_VIDEO)), 6 * 64); // SCT
    	bitstream_flush(bs,5);

		if(bitstream_get(bs, 4) != seg->i) return 6 * 64;
		if(bitstream_get(bs, 1) != DV_FSC_0) return 6 * 64;
/*
 *     	g_return_val_if_fail((bitstream_get(bs, 4) == seg->i), 6 * 64);
 *     	g_return_val_if_fail((bitstream_get(bs, 1) == DV_FSC_0), 6 * 64);
 */
    	bitstream_flush(bs, 3);

    	bitstream_flush(bs,8); // DBN -- could check this
    	mb->sta = bitstream_get(bs, 4);
// Error in macroblock so skip it
//		if(mb->sta)
//			continue;
/*
 * #else
 *     bitstream_seek_set(bs, mb_start + 28);
 * #endif
 */
    // first get the macroblock-wide data
    mb->qno = bitstream_get(bs, 4);
    mb->vlc_error = 0;
    mb->eob_count = 0;
    mb->i = (seg->i + dv_super_map_vertical[m]) % (seg->isPAL ? 12 : 10);
    mb->j = dv_super_map_horizontal[m];
    mb->k = seg->k;
    for(b=0,bl=mb->b;
	b<6;
	b++,bl++) {
      // Pass 0, read bits from individual blocks 
      // Get DC coeff, mode, and class from start of block
      dc = bitstream_get(bs,9);  // DC coefficient (twos complement)
      if(dc > 256) dc -= 513;
      bl->coeffs[0] = dc;
      vlc_trace("DC [%d,%d,%d,%d] = %d\n",mb->i,mb->j,mb->k,b,dc);
      bl->dct_mode = bitstream_get(bs,1);
      bl->class_no = bitstream_get(bs,2);
      bl->eob=0;
      bl->offset= mb_start + dv_parse_bit_start[b];
      bl->end= mb_start + dv_parse_bit_end[b];
      bl->reorder = &dv_reorder[bl->dct_mode][1];
      bl->reorder_sentinel = bl->reorder + 63;
      dv_parse_ac_coeffs_pass0(bs,mb,bl);
      bitstream_seek_set(bs,bl->end);
    } // for b
  } // for m
  // Phase 2:  do the 3 pass AC vlc decode
  return(dv_parse_ac_coeffs(seg));
} // dv_parse_video_segment 


/*
 * // Return number of samples read
 * gint dv_parse_audio_segment(dv_audiosegment_t *seg, 
 * 		unsigned char *samples, 
 * 		int output_channels, 
 * 		unsigned char *data)
 * {
 * 	gint samples_read = 0;
 * 	unsigned long code;
 *   	bitstream_t *bs = seg->bs;
 * 	gint i, j;
 * 	gshort *output;
 * 	int samples_per_dif = 35;
 * 
 * 	if(bitstream_get(bs, 3) == DV_SCT_AUDIO)
 * 	{
 * 		bitstream_seek_set(bs, 24);
 * 		code = bitstream_get(bs, 8);
 * 		if(code == DV_AAUX_SOURCE)
 * 		{
 * 			seg->total_samples = bitstream_get(bs, 8) & 0x3f;
 * 			switch(seg->total_samples)
 * 			{
 * 				case 0x14:
 * 					seg->total_samples = 1600;
 * 					break;
 * 				case 0x16:
 * 					seg->total_samples = 1602;
 * 					break;
 * 				case 0x18:
 * 					seg->total_samples = 1920;
 * 					break;
 * 			}
 * 			seg->channel = bitstream_get(bs, 8) & 0xf;
 * 			switch(seg->channel)
 * 			{
 * 				case 0:
 * 					seg->channel = 0;
 * 					break;
 * 				case 1:
 * 					seg->channel = 1;
 * 					break;
 * 			}
 * 			bitstream_get(bs, 8);
 * 			seg->quantization = bitstream_get(bs, 8) & 0x07;
 * 			switch(seg->quantization)
 * 			{
 * 				case 0xb:
 * 					seg->quantization = 16;
 * 					break;
 * 			}
 * 		}
 * 
 * //printf("parse audio %d\n", seg->quantization);
 * //fwrite(data, 80, 1, stdout);
 * 		data += 8;
 * 		output = (gshort*)samples + 
 * 			seg->samples_read[seg->channel] * output_channels + 
 * 			seg->channel;
 * 		if(seg->channel < output_channels)
 * 		{
 * 			for(i = 0, j = 1; j < samples_per_dif * 2; i += 2, j += 2)
 * 			{
 * 				*output = (((unsigned int)(data[i])) << 8) | data[j];
 * //if(seg->channel == 0) fwrite(output, 2, 1, stdout);
 * 				output += output_channels;
 * 			}
 * 			seg->samples_read[seg->channel] += samples_per_dif;
 * 		}
 * 	}
 * 
 * 	return 0;
 * }
 */
