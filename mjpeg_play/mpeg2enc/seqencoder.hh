#ifndef _SEQENCODER_HH
#define _SEQENCODER_HH


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

#include <config.h>
#include "mjpeg_types.h"
#include "picture.hh"

class MPEG2Encoder;
class EncoderParams;
class MPEG2Coder;
class PictureReader;
class Despatcher;

struct StreamState 
{
	int i;						/* Index in current sequence */
	int g;						/* Index in current GOP */
	int b;						/* Index in current B frame group */
	int seq_start_frame;		/* Index start current sequence in
								   input stream */
	int gop_start_frame;		/* Index start current gop in input stream */
	int gop_length;			/* Length of current gop */
	int bigrp_length;			/* Length of current B-frame group */
	int bs_short;				/* Number of B frame GOP is short of
								   having M-1 B's for each I/P frame
								*/
	int np;					/* P frames in current GOP */
	int nb;					/* B frames in current GOP */
	double next_b_drop;		/* When next B frame drop is due in GOP */
	bool new_seq;				/* Current GOP/frame starts new sequence */
    bool closed_gop;            /* Current GOP is closed */
	uint64_t next_split_point;
	uint64_t seq_split_length;
};


class SeqEncoder
{
public:
	SeqEncoder( EncoderParams &encparams,
                PictureReader &reader,
                Quantizer &quantizer,
                ElemStrmWriter &writer,
                MPEG2Coder &coder,
                RateCtl    &ratecontroller
        );
	~SeqEncoder();


    /**********************************
     *
     * Setup ready to start encoding once parameter objects have
     * (where relevant) been Init-ed.
     * Spawns worker threads for parallel prociessing.
     *
     *********************************/

    void Init();
   
    /**********************************
     *
     * EncodeFrmae - encode and output exactly one MPEG frame.
     * Between zero and many frames may be read (lots of internal
     * look-ahead and buffering).  Internal parallelism via
     * POSIXworker threads.
     *
     * RETURN: true if more frames remain to be encoded.
     *
     *********************************/
	bool EncodeFrame();
    
    
private:

	int FindGopLength( int gop_start_frame, 
					   int I_frame_temp_ref,
					   int gop_min_len, int gop_max_len,
					   int min_b_grp);
	void GopStart( StreamState *ss );
	void NextSeqState( StreamState *ss );
	void LinkPictures( Picture *ref_pictures[], 
					   Picture *b_pictures[] );
	void EncodePicture( Picture *picture );
    EncoderParams &encparams;
    PictureReader &reader;
    Quantizer &quantizer;
    ElemStrmWriter &writer;
    MPEG2Coder &coder;
    RateCtl    &ratecontroller;
    
    Despatcher &despatcher;
	
	// Internal state of encoding...

	StreamState ss;
	int cur_ref_idx;
	int cur_b_idx;


    /* DEBUG */
	uint64_t bits_after_mux;
	double frame_periods;
    /* END DEBUG */

	Picture **b_pictures;
	Picture **ref_pictures;

	Picture *cur_picture, *old_picture;
	Picture *new_ref_picture, *old_ref_picture;

	int frame_num;              // Encoding number - not
                                // necessarily the one that gets
                                // output!!

};


/* 
 * Local variables:
 *  c-file-style: "stroustrup"
 *  tab-width: 4
 *  indent-tabs-mode: nil
 * End:
 */
#endif
