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

#include "mjpeg_types.h"
#include "synchrolib.h"
#include "picture.hh"

class MPEG2Encoder;
class EncoderParams;
class MPEG2Coder;
class PictureReader;

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

	void Encode();
private:
    void CreateThreads( pthread_t *threads,
                        int num, void *(*start_routine)(void *),
                        SeqEncoder *seqencoder );
	int FindGopLength( int gop_start_frame, 
					   int I_frame_temp_ref,
					   int gop_min_len, int gop_max_len,
					   int min_b_grp);
	void GopStart( StreamState *ss );
	void NextSeqState( StreamState *ss );
	void LinkPictures( Picture *ref_pictures[], 
					   Picture *b_pictures[] );
	static void *ParallelEncodeWrapper( void *seqencoder );
	void ParallelEncodeWorker();
	void ParallelEncode( Picture *picture );
	void SequentialEncode( Picture *picture );
    EncoderParams &encparams;
    PictureReader &reader;
    Quantizer &quantizer;
    ElemStrmWriter &writer;
    MPEG2Coder &coder;
    RateCtl    &ratecontroller;

	mp_semaphore_t worker_available;
	mp_semaphore_t picture_available;
	mp_semaphore_t picture_started;
	
	/*
	  Ohh, lovely C type syntax... more or less have to introduce a named
	  typed here to bind the "volatile" correctly - to the pointer not the
	  data it points to. K&R: hang your heads in shame...
	*/
	
	typedef Picture * pict_data_ptr;

	volatile pict_data_ptr picture_to_encode;
};


/* 
 * Local variables:
 *  c-file-style: "stroustrup"
 *  tab-width: 4
 *  indent-tabs-mode: nil
 * End:
 */
#endif
