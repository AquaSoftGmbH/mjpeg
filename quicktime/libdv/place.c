/* 
 *  place.c
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

#include "dv.h"
#include "place.h"

#define DEBUG_HIGHLIGHT_MBS 0

void dv_place_init(void) {
  return;
} // dv_place_init

size_t dv_place_411_macroblock(dv_macroblock_t *mb, size_t pel_size) {
  size_t result; // offset in bytes from start of image buffer
  gint mb_num; // mb number withing the 6 x 5 zig-zag pattern 
  gint mb_num_mod_6, mb_num_div_6; // temporaries
  gint mb_row;    // mb row within sb (de-zigzag)
  gint mb_col;    // mb col within sb (de-zigzag)
  // Column offset of superblocks in macroblocks.  
  static guint column_offset[] = {0, 4, 9, 13, 18};  

  // Consider the area spanned super block as 30 element macroblock
  // grid (6 rows x 5 columns).  The macroblocks are laid out in a in
  // a zig-zag down and up the columns of the grid.  Of course,
  // superblocks are not perfect rectangles, since there are only 27
  // blocks.  The missing three macroblocks are either at the start
  // or end depending on the superblock column.

  // Within a superblock, the macroblocks start at the topleft corner
  // for even-column superblocks, and 3 down for odd-column
  // superblocks.
  mb_num = ((mb->j % 2) == 1) ? mb->k + 3: mb->k;  
  mb_num_mod_6 = mb_num % 6;
  mb_num_div_6 = mb_num / 6;
  // Compute superblock-relative row position (de-zigzag)
  mb_row = ((mb_num_div_6 % 2) == 0) ? mb_num_mod_6 : (5 - mb_num_mod_6); 
  // Compute macroblock's frame-relative column position (in blocks)
  mb_col = (mb_num_div_6 + column_offset[mb->j]) * 4;
  // Compute frame-relative byte offset of macroblock's top-left corner
  // with special case for right-edge macroblocks
  if(mb_col < (22 * 4)) {
    // Convert from superblock-relative row position to frame relative (in blocks).
    mb_row += (mb->i * 6); // each superblock is 6 blocks high 
    // Normal case
  } else { 
    // Convert from superblock-relative row position to frame relative (in blocks).
    mb_row = mb_row * 2 + mb->i * 6; // each right-edge macroblock is 2 blocks high, and each superblock is 6 blocks high
  } // else
  result = mb_row * 8 * 720 * pel_size // block is 8 pels high, row is 720 pels wide, pel_size bytes per pel
    + mb_col * 8 * pel_size;       // macroblock is 4 blocks wide, 8 pels wide per block, pel_size bytes per pel
  return(result);
} // dv_place_411_macroblock

size_t dv_place_420_macroblock(dv_macroblock_t *mb, size_t pel_size) {
  size_t result; // offset in bytes from start of image buffer
  gint mb_num; // mb number withing the 6 x 5 zig-zag pattern 
  gint mb_num_mod_3, mb_num_div_3; // temporaries
  gint mb_row;    // mb row within sb (de-zigzag)
  gint mb_col;    // mb col within sb (de-zigzag)
  // Column offset of superblocks in macroblocks.  
  static guint column_offset[] = {0, 9, 18, 27, 36};  

  // Consider the area spanned super block as 30 element macroblock
  // grid (6 rows x 5 columns).  The macroblocks are laid out in a in
  // a zig-zag down and up the columns of the grid.  Of course,
  // superblocks are not perfect rectangles, since there are only 27
  // blocks.  The missing three macroblocks are either at the start
  // or end depending on the superblock column.

  // Within a superblock, the macroblocks start at the topleft corner
  // for even-column superblocks, and 3 down for odd-column
  // superblocks.
  mb_num = mb->k;  
  mb_num_mod_3 = mb_num % 3;
  mb_num_div_3 = mb_num / 3;
  // Compute superblock-relative row position (de-zigzag)
  mb_row = ((mb_num_div_3 % 2) == 0) ? mb_num_mod_3 : (2 - mb_num_mod_3); 
  // Compute macroblock's frame-relative column position (in blocks)
  mb_col = mb_num_div_3 + column_offset[mb->j];
  // Compute frame-relative byte offset of macroblock's top-left corner
  // Convert from superblock-relative row position to frame relative (in blocks).
  mb_row += (mb->i * 3); // each right-edge macroblock is 2 blocks high, and each superblock is 6 blocks high
  result = mb_row * 16 * 720 * pel_size // macro block is 16 pels high, row is 720 pels wide, pel_size bytes per pel
    + mb_col * 16 * pel_size;       // macroblock is 2 blocks wide, 8 pels wide per block, pel_size bytes per pel
  return(result);
} // dv_place_420_macroblock


