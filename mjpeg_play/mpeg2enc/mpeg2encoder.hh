#ifndef _MPEG2ENCODER_HH
#define _MPEG2ENCODER_HH

/* readpic.h Picture reader base class and basic file I/O based reader */
/*  (C) 2000/2001 Andrew Stevens */

/*  This Software is free software; you can redistribute it
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


#include "syntaxparams.h"
#include "quantize_ref.h"
 
class EncoderParams;
class PictureReader;
class RateController;
class SeqEncoder;
class Quantizer;
class Transformer;

class MPEG2Encoder
{
public:
    MPEG2Encoder( int istrm_fd );
    ~MPEG2Encoder();
    EncoderParams &parms;
    PictureReader &reader;
    Quantizer &quantizer;
    RateCtl &bitrate_controller;
    SeqEncoder &seqencoder;

    // Function Pointers and Private Workspaces for
    // SIMD low-level functions
    struct QuantizerWorkSpace *quant_wsp;
    struct QuantizerCalls quant_calls;
};

extern MPEG2Encoder *enc;





/* 
 * Local variables:
 *  c-file-style: "stroustrup"
 *  tab-width: 4
 *  indent-tabs-mode: nil
 * End:
 */
#endif

