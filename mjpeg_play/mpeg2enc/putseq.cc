/* putseq.c MPEG1/2 Sequence encoding loop */

//#define SEQ_DEBUG 1
/* (C) Andrew Stevens 2000, 2001
 *  This file is free software; you can redistribute it
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
/*
 * Disclaimer of Warranty
 *
 * These software programs are available to the user without any license fee or
 * royalty on an "as is" basis.  The MPEG Software Simulation Group disclaims
 * any and all warranties, whether express, implied, or statuary, including any
 * implied warranties or merchantability or of fitness for a particular
 * purpose.  In no event shall the copyright-holder be liable for any
 * incidental, punitive, or consequential damages of any kind whatsoever
 * arising from the use of these programs.
 *
 * This disclaimer of warranty extends to the user of these programs and user's
 * customers, employees, agents, transferees, successors, and assigns.
 *
 * The MPEG Software Simulation Group does not represent or warrant that the
 * programs furnished hereunder are free of infringement of any third-party
 * patents.
 *
 * Commercial implementations of MPEG-1 and MPEG-2 video, including shareware,
 * are subject to royalty fees to patent holders.  Many of these patents are
 * general enough such that they are unavoidable regardless of implementation
 * design.
 *
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "mjpeg_types.h"
#include "mjpeg_logging.h"
#include "simd.h"
#include "mpeg2syntaxcodes.h"
#include "mpeg2encoder.hh"
#include "picturereader.hh"
#include "mpeg2coder.hh"
#include "seqencoder.hh"
#include "ratectl.hh"
#include "tables.h"



SeqEncoder::SeqEncoder( EncoderParams &_encparams,
                        PictureReader &_reader,
                        Quantizer &_quantizer,
                        ElemStrmWriter &_writer,
                        MPEG2Coder &_coder,
                        RateCtl    &_ratecontroller ) :
    encparams( _encparams ),
    reader( _reader ),
    quantizer( _quantizer ),
    writer( _writer ),
    coder( _coder ),
    ratecontroller( _ratecontroller )

{
    mp_semaphore_init( &worker_available, 0 );
    mp_semaphore_init( &picture_available, 0 );
    mp_semaphore_init( &picture_started, 0 );
}

SeqEncoder::~SeqEncoder()
{
}

/************************************
 *
 * FindGopLength - Finds a sensible spot for a GOP to end based on
 * the mean luminance strategy developed in Dirk Farin's sampeg-2
 *
 * Basically it looks for a frame whose mean luminance is above
 * certain threshold distance from that of its predecessor and adjust the
 * suggested length of the GOP to make that the I frame of the *next* GOP.
 * 
 * A slight trick is that search commences at the end of the legal range
 * backwards.  The reason is that longer GOP's are more efficient.  Thus if
 * two scence changes come close together it is better to have a change in
 * the middle of a long GOP (with plenty of bits to play with) rather than
 * a short GOP followed by another potentially very short GOP.
 *
 * Another complication is that the temporal reference of the I frame
 * of the successor frame need not be 0, so the length calculation has to
 * take into account this offset.
 *
 * N.b. This is experimental and could be highly bogus
 *
 ************************************/

#define SCENE_CHANGE_THRESHOLD 4

int SeqEncoder::FindGopLength( int gop_start_frame, 
                               int I_frame_temp_ref,
                               int gop_min_len, int gop_max_len,
                               int min_b_grp)
{
	int i,j;
    int max_change = 0;
    int gop_len;
	int pred_lum_mean;

    if( gop_min_len >= gop_max_len )
        gop_len = gop_max_len;
    else
    {
        int cur_lum_mean = 
            reader.FrameLumMean( gop_start_frame+gop_min_len-min_b_grp+I_frame_temp_ref );

        /* Search forwards from min gop length for I-frame candidate
           which has the largest change in mean luminance.
           BUGBUGBUG: The frame ring buffer size must be large enough to
           accomodate this read-ahead without losing frames currently being
           encoded!!!
        */
        gop_len = 0;
        for( i = gop_min_len; i <= gop_max_len; i += min_b_grp )
        {
            pred_lum_mean = reader.FrameLumMean( gop_start_frame+i+I_frame_temp_ref);
            if( abs(cur_lum_mean-pred_lum_mean ) >= SCENE_CHANGE_THRESHOLD 
                && abs(cur_lum_mean-pred_lum_mean ) > max_change )
            {
                max_change = abs(cur_lum_mean-pred_lum_mean );
                gop_len = i;
            }
            cur_lum_mean = pred_lum_mean;
        }

        if( gop_len == 0 )
        {

            /* No scene change detected within maximum GOP length.
               Now do a look-ahead check to see if running a maximum length
               GOP next would put scene change into the GOP after-next
               that would need a GOP smaller than minimum to handle...
            */
            gop_len = gop_max_len;
            for( j = gop_max_len+min_b_grp; j < gop_max_len+gop_min_len; j += min_b_grp )
            {
                cur_lum_mean = 
                    reader.FrameLumMean( gop_start_frame+j+I_frame_temp_ref );
                if( abs(cur_lum_mean-pred_lum_mean ) >= SCENE_CHANGE_THRESHOLD
                    &&  abs(cur_lum_mean-pred_lum_mean > max_change )
                    )
                {
                    max_change = abs(cur_lum_mean-pred_lum_mean );
                    gop_len = j;
                }
            }
		
            /* If a max length GOP next would cause a scene change in
               a place where the GOP after-next would be under-size to
               handle it *and* making the current GOP minimum size
               would allow an acceptable GOP after-next size the next
               GOP to give it and after-next roughly equal sizes
               otherwise simply set the GOP to maximum length
            */

            if( gop_len > gop_max_len 
                && gop_len < gop_max_len+gop_min_len )
            {
                if( gop_len < 2*gop_min_len )
                {
                    mjpeg_info("GOP min length too small to permit scene-change on GOP boundary %d", j);
                    /* Its better to put the missed transition at the end
                       of a GOP so any eventual artefacting is soon washed
                       out by an I-frame*/
                    gop_len = gop_min_len;
                }
                else
                {
                    /* If we can make the two GOPs near the transition
                       similarly sized */
                    i = gop_len /2;
                    if( i%min_b_grp != 0 )
                        i+=(min_b_grp-i%min_b_grp);
                    if( gop_len-i < gop_min_len )
                        gop_len = gop_min_len;
                    if( gop_len > gop_max_len )
                        gop_len = gop_max_len;
                }
				
            }
            else
                gop_len = gop_max_len;
        }
    }
	/* last GOP may contain less frames! */
    int istrm_nframes = reader.NumberOfFrames();
	if (gop_len > istrm_nframes-gop_start_frame)
		gop_len = istrm_nframes-gop_start_frame;
	mjpeg_info( "GOP start (%d frames)", gop_len);
	return gop_len;

}





void SeqEncoder::CreateThreads( pthread_t *threads,
                                int num, void *(*start_routine)(void *),
                                SeqEncoder *seqencoder )
{
	int i;
	pthread_attr_t *pattr = NULL;

	/* For some Unixen we get a ridiculously small default stack size.
	   Hence we need to beef this up if we can.
	*/
#ifdef HAVE_PTHREADSTACKSIZE
#define MINSTACKSIZE 200000
	pthread_attr_t attr;
	size_t stacksize;

	pthread_attr_init(&attr);
	pthread_attr_getstacksize(&attr, &stacksize);

	if (stacksize < MINSTACKSIZE) {
		pthread_attr_setstacksize(&attr, MINSTACKSIZE);
	}

	pattr = &attr;
#endif
	for(i = 0; i < num; ++i )
	{
		if( pthread_create( &threads[i], pattr, start_routine, seqencoder ) != 0 )
		{
			mjpeg_error_exit1( "worker thread creation failed: %s", strerror(errno) );
		}
	}
}

void SeqEncoder::GopStart( StreamState *ss )
{

	int nb, np;
	uint64_t bits_after_mux;
	double frame_periods;
	/* If	we're starting a GOP and have gone past the current
	   sequence splitting point split the sequence and
	   set the next splitting point.
	*/
			
	ss->g = 0;
	ss->b = 0;
	ss->new_seq = false;
	
	if( encparams.pulldown_32 )
		frame_periods = (double)(ss->seq_start_frame + ss->i)*(5.0/4.0);
	else
		frame_periods = (double)(ss->seq_start_frame + ss->i);
    
    /*
      For VBR we estimate total bits based on actual stream size and
      an estimate for the other streams based on time.
      For CBR we do *both* based on time to account for padding during
      muxing.
      TODO: This should be placed in the bitrate controller....
    */
    
    if( encparams.quant_floor > 0.0 ) bits_after_mux = writer.BitCount() + 
            (uint64_t)((frame_periods / encparams.frame_rate) * encparams.nonvid_bit_rate);
    else
        bits_after_mux = (uint64_t)((frame_periods / encparams.frame_rate) * 
                                    (encparams.nonvid_bit_rate + encparams.bit_rate));
	if( (ss->next_split_point != 0ULL && bits_after_mux > ss->next_split_point)
		|| (ss->i != 0 && encparams.seq_end_every_gop)
		)
	{
		mjpeg_info( "Splitting sequence this GOP start" );
		ss->next_split_point += ss->seq_split_length;
		/* This is the input stream display order sequence number of
		   the frame that will become frame 0 in display
		   order in  the new sequence */
		ss->seq_start_frame += ss->i;
		ss->i = 0;
		ss->new_seq = true;
	}

    ss->closed_gop = ss->i == 0 || encparams.closed_GOPs;
	ss->gop_start_frame = ss->seq_start_frame + ss->i;
	
	/*
	  Compute GOP length based on min and max sizes specified
	  and scene changes detected.  
	  First GOP in a sequence has I frame with a 0 temp_ref
	  and (M-1) less frames (no initial B frames).
	  In all other GOPs I-frame has a temp_ref of M-1
	*/
	
    ss->gop_length =  
        FindGopLength( ss->gop_start_frame, 
                       ss->closed_gop ? 0 : encparams.M-1, 
                       encparams.N_min, encparams.N_max,
                       encparams.M_min);
			
	/* First figure out how many B frames we're short from
	   being able to achieve an even M-1 B's per I/P frame.
	   
	   To avoid peaks in necessary data-rate we try to
	   lose the B's in the middle of the GOP. We always
	   *start* with M-1 B's (makes choosing I-frame breaks simpler).
	   A complication is the extra I-frame in the initial
	   closed GOP of a sequence.
	*/
	if( encparams.M-1 > 0 )
	{
		ss->bs_short = (encparams.M - ((ss->gop_length-(ss->i==0)) % encparams.M))%encparams.M;
		ss->next_b_drop = ((double)ss->gop_length) / (double)(ss->bs_short+1)-1.0 ;
	}
	else
	{
		ss->bs_short = 0;
		ss->next_b_drop = 0.0;
	}
	
	/* We aim to spread the dropped B's evenly across the GOP */
	ss->bigrp_length = (encparams.M-1);
	
	/* number of P frames */
	if( encparams.M == 0 ) 
	{
		ss->bigrp_length = 0;
		np = 0;
	}
	else if (ss->closed_gop )
	{
		ss->bigrp_length = 1;
		np = (ss->gop_length + 2*(encparams.M-1))/encparams.M - 1; /* Closed GOP */
	}
	else
	{
		ss->bigrp_length = encparams.M;
		np = (ss->gop_length + (encparams.M-1))/encparams.M - 1;
	}
	/* number of B frames */
	nb = ss->gop_length - np - 1;

	ss->np = np;
	ss->nb = nb;
	if( np+nb+1 != ss->gop_length )
	{
		mjpeg_error_exit1( "****INTERNAL: inconsistent GOP %d %d %d", 
						   ss->gop_length, np, nb);
	}

}


/*
  Update ss to the next sequence state.
*/

void SeqEncoder::NextSeqState( StreamState *ss )
{
	++(ss->i);
	++(ss->g);
	++(ss->b);	

	/* Are we starting a new B group */
	if( ss->b >= ss->bigrp_length )
	{
		ss->b = 0;
		/* Does this need to be a short B group to make the GOP length
		   come out right ? */
		if( ss->bs_short != 0 && ss->g > (int)ss->next_b_drop )
		{
			ss->bigrp_length = encparams.M - 1;
			if( ss->bs_short )
				ss->next_b_drop += ((double)ss->gop_length) / (double)(ss->bs_short+1) ;
		}
		else
			ss->bigrp_length = encparams.M;
	}

    /* Are we starting a new GOP? */
	if( ss->g == ss->gop_length )
	{
		GopStart( ss);
	}

}




/* 
   Initialize picture data buffers for reference and B pictures

   There *must* be 2 more of each than active frame encoding threads.
   Fortunately, there is no point having very large numbers of threads
   as concurency is limited by each frame's encoding depending (at
   the very minimum) on the completed encoding of the preceding reference
   frame.

   In practice the current bit allocation algorithm means that a
   frame cannot be completed until its predecessor is completed.

   Thus for reasonable M (bigroup length) there is no point in having
   more than M threads.   
*/


void SeqEncoder::LinkPictures( Picture *ref_pictures[], 
                               Picture *b_pictures[] )
{

	int i,j;

	for( i = 0; i < encparams.max_active_ref_frames; ++i )
	{
		j = (i + 1) % encparams.max_active_ref_frames;

		ref_pictures[j]->oldorg = ref_pictures[i]->curorg;
		ref_pictures[j]->oldref = ref_pictures[i]->curref;
		ref_pictures[j]->neworg = ref_pictures[j]->curorg;
		ref_pictures[j]->newref = ref_pictures[j]->curref;
	}

}



#ifdef DEBUG_BLOCK_STRIPED
static unsigned int checksum( uint8_t *buf, unsigned int len )
{
    unsigned int acc = 0;
    unsigned int i;
    for( i = 0; i < len; ++i )
        acc += buf[i];
    return acc;
}
#endif


void SeqEncoder::SequentialEncode(Picture *picture)
{
	/* ALWAYS do-able */
	mjpeg_debug("Frame start %d %c %d %d",
			   picture->decode, 
			   pict_type_char[picture->pict_type],
			   picture->temp_ref,
			   picture->present);


	if( picture->pict_struct != FRAME_PICTURE )
		mjpeg_info("Field %s (%d)",
				   (picture->pict_struct == TOP_FIELD) ? "top" : "bot",
				   picture->pict_struct
			);

	picture->MotionSubSampledLum();

		
    picture->EncodeMacroBlocks();
	picture->PutHeadersAndEncoding(ratecontroller);
	picture->Reconstruct();

	/* Handle second field of a frame that is being field encoded */
	if( encparams.fieldpic )
	{
		picture->Set2ndField();
		mjpeg_info("Field %s (%d)",
				   (picture->pict_struct == TOP_FIELD) ? "top" : "bot",
				   picture->pict_struct
			);

        picture->EncodeMacroBlocks();
		picture->PutHeadersAndEncoding(ratecontroller);
		picture->Reconstruct();

	}


	mjpeg_info("Frame end %d %c quant=%3.2f total act=%8.5f %s", 
               picture->decode, 
			   pict_type_char[picture->pict_type],
               picture->AQ,
               picture->sum_avg_act,
               picture->pad ? "PAD" : "   "
        );
			
}



void *SeqEncoder::ParallelEncodeWrapper(void *seqencoder)
{
    static_cast<SeqEncoder *>(seqencoder)->ParallelEncodeWorker();
    return 0;
}

void SeqEncoder::ParallelEncodeWorker()
{
	pict_data_ptr picture;
	mjpeg_debug( "Worker thread started" );

	for(;;)
	{
		mp_semaphore_signal( &worker_available, 1);
		mp_semaphore_wait( &picture_available );
		/* Precisely *one* worker is started after update of
		   picture_for_started_worker, so no need for handshake.  */
		picture = picture_to_encode;
		mp_semaphore_signal( &picture_started, 1);

		/* ALWAYS do-able */
		mjpeg_info("Frame %d  %c %d %d",  
				   picture->decode,  
                   pict_type_char[picture->pict_type],
				   picture->temp_ref,
				   picture->present);

		if( picture->pict_struct != FRAME_PICTURE )
			mjpeg_info("Field %s (%d)",
					   (picture->pict_struct == TOP_FIELD) ? "top" : "bot",
					   picture->pict_struct
				);

		picture->MotionSubSampledLum();

		
		/* DEPEND on completion previous Reference frames (P) or on old
		   and new Reference frames B.  However, since new reference frame
		   cannot complete until old reference frame completed (see below)
		   suffices just to check new reference frame... 
		   N.b. completion guard of picture is always reset to false
		   before this function is called...
		   
		   In field picture encoding the P field of an I frame is
		   a special case.  We have to wait for completion of the I field
		   before starting the P field
		*/

#ifdef SEQ_DEBUG
        printf( "Frame %d %08x Waiting for ref: %d %08x\n",
                picture->decode, 
                picture, 
                picture->ref_frame->decode,
                picture->ref_frame );
#endif
        sync_guard_test( &picture->ref_frame->completion );
        picture->EncodeMacroBlocks();

		/* Depends on previous frame completion for IB and P */
#ifdef SEQ_DEBUG
        printf( "Frame %d %08x Waiting for compl: %d %08x\n",
                picture->decode, 
                picture, 
                picture->prev_frame->decode,
                picture->prev_frame );
#endif
		sync_guard_test( &picture->prev_frame->completion );
		picture->PutHeadersAndEncoding(ratecontroller);

		picture->Reconstruct();
		/* Handle second field of a frame that is being field encoded */
		if( encparams.fieldpic )
		{
			picture->Set2ndField();

			mjpeg_info("Field %s (%d)",
					   (picture->pict_struct == TOP_FIELD) ? "top" : "bot",
					   picture->pict_struct
				);

            picture->EncodeMacroBlocks();
            picture->PutHeadersAndEncoding(ratecontroller);
			picture->Reconstruct();

		}


		mjpeg_info("Frame end %d %s %3.2f %.2f %2.1f %.2f",
					picture->decode, 
					picture->pad ? "PAD" : "   ",
					picture->avg_act, picture->sum_avg_act,
					picture->AQ, picture->SQ);

		/* We're finished - let anyone depending on us know...
		 */
		sync_guard_update( &picture->completion, 1 );
			
	}
}


void SeqEncoder::ParallelEncode( Picture *picture )
{

	mp_semaphore_wait( &worker_available );
	picture_to_encode = picture;
	mp_semaphore_signal( &picture_available, 1 );
	mp_semaphore_wait( &picture_started );
}


/*********************
 *
 *  Multi-threading capable putseq 
 *
 *  N.b. It is *critical* that the reference and b picture buffers are
 *  at least two larger than the number of encoding worker threads.
 *  The despatcher thread *must* have to halt to wait for free
 *  worker threads *before* it re-uses the record for the
 *  Picture record for the oldest frame that could still be needed
 *  by active worker threads.
 * 
 *  Buffers sizes are given by encparams.max_active_ref_frames and
 *  encparams.max_active_b_frames
 *
 ********************/
 
void SeqEncoder::Encode()
{
    /* DEBUG */
	uint64_t bits_after_mux;
	double frame_periods;
    /* END DEBUG */
	StreamState ss;
	int cur_ref_idx = 0;
	int cur_b_idx = 0;

    int i;
	Picture *b_pictures[encparams.max_active_b_frames];
	Picture *ref_pictures[encparams.max_active_ref_frames];
    for( i = 0; i < encparams.max_active_b_frames; ++i )
    {
        b_pictures[i] = new Picture(encparams, coder, quantizer);
    }
    for( i = 0; i < encparams.max_active_ref_frames; ++i )
    {
        ref_pictures[i] = new Picture(encparams, coder, quantizer);
    }

	LinkPictures( ref_pictures, b_pictures );

	pthread_t worker_threads[encparams.max_encoding_frames];
	if( encparams.max_encoding_frames > 1 )
		CreateThreads( worker_threads, encparams.max_encoding_frames, 
                       ParallelEncodeWrapper, this );

	/* Initialize image dependencies and synchronisation.  The
	   first frame encoded has no predecessor whose completion it
	   must wait on.
	*/
	Picture *cur_picture, *old_picture;
	Picture *new_ref_picture, *old_ref_picture;

	old_ref_picture = ref_pictures[encparams.max_active_ref_frames-1];
	new_ref_picture = ref_pictures[cur_ref_idx];
	cur_picture = new_ref_picture;
	
	ratecontroller.InitSeq(false);
	
	ss.i = 0;		                /* Index in current MPEG sequence */
	ss.g = 0;						/* Index in current GOP */
	ss.b = 0;						/* B frames since last I/P */
	ss.gop_length = 0;				/* Length of current GOP init 0
									   0 force new GOP at start 1st sequence */
	ss.seq_start_frame = 0;		/* Index start current sequence in
								   input stream */
	ss.gop_start_frame = 0;		/* Index start current gop in input stream */
	ss.seq_split_length = ((int64_t)encparams.seq_length_limit)*(8*1024*1024);
	ss.next_split_point = BITCOUNT_OFFSET + ss.seq_split_length;
	mjpeg_debug( "Split len = %" PRId64 "", ss.seq_split_length );

	int frame_num = 0;              /* Encoding number */

	GopStart( &ss);

	/* loop through all frames in encoding/decoding order */
	while( frame_num< reader.NumberOfFrames() )
	{
		old_picture = cur_picture;

		/* Each bigroup starts once all the B frames of its predecessor
		   have finished.
		*/
        int index;
        char type;
		if ( ss.b == 0)
		{
            type = 'R';
			cur_ref_idx = (cur_ref_idx + 1) % encparams.max_active_ref_frames;
            index = cur_ref_idx;
			old_ref_picture = new_ref_picture;
			new_ref_picture = ref_pictures[cur_ref_idx];
			new_ref_picture->ref_frame = old_ref_picture;
			new_ref_picture->prev_frame = cur_picture;
			new_ref_picture->Set_IP_Frame(&ss, reader.NumberOfFrames());
			cur_picture = new_ref_picture;
		}
		else
		{
            type = 'B';

			Picture *new_b_picture;
			/* B frame: no need to change the reference frames.
			   The current frame data pointers are a 3rd set
			   seperate from the reference data pointers.
			*/
			cur_b_idx = ( cur_b_idx + 1) % encparams.max_active_b_frames;
            index = cur_b_idx;
			new_b_picture = b_pictures[cur_b_idx];
			new_b_picture->oldorg = new_ref_picture->oldorg;
			new_b_picture->oldref = new_ref_picture->oldref;
			new_b_picture->neworg = new_ref_picture->neworg;
			new_b_picture->newref = new_ref_picture->newref;
			new_b_picture->ref_frame = new_ref_picture;
			new_b_picture->prev_frame = cur_picture;
			new_b_picture->Set_B_Frame( &ss );
			cur_picture = new_b_picture;
		}

#ifdef SEQ_DEBUG
        printf( "Mark incomplete: %d %08x prev = %08x ref = %08x\n", index, cur_picture, cur_picture->ref_frame, cur_picture->prev_frame );
#endif
		sync_guard_update( &cur_picture->completion, 0 );
		reader.ReadFrame( cur_picture->temp_ref+ss.gop_start_frame,
                          cur_picture->curorg );

		cur_picture->SetSeqPos( ss.i, ss.b );
		if( encparams.max_encoding_frames > 1 )
			ParallelEncode( cur_picture );
		else
			SequentialEncode( cur_picture );

#ifdef DEBUG
		writeframe(cur_picture->temp_ref+ss.gop_start_frame,cur_picture->curref);
#endif

		NextSeqState( &ss );
		++frame_num;
	}
	
	/* Wait for final frame's encoding to complete */
	if( encparams.max_encoding_frames > 1 )
		sync_guard_test( &cur_picture->completion );
	coder.PutSeqEnd();

	if( encparams.pulldown_32 )
		frame_periods = (double)(ss.seq_start_frame + ss.i)*(5.0/4.0);
	else
		frame_periods = (double)(ss.seq_start_frame + ss.i);
    
    
    // TODO Belongs in BitRateCtl
    if( encparams.quant_floor > 0.0 )
        bits_after_mux = writer.BitCount() + 
            (uint64_t)((frame_periods / encparams.frame_rate) * encparams.nonvid_bit_rate);
    else
        bits_after_mux = (uint64_t)((frame_periods / encparams.frame_rate) * 
                                    (encparams.nonvid_bit_rate + encparams.bit_rate));

    mjpeg_info( "Guesstimated final muxed size = %lld\n", bits_after_mux/8 );
    
    //
    // TODO: Really this ought to be a job for smart ptrs, but auto_ptr
    // gets messed with the copy constructor implicit in push_back...
    //
    for( i = 0; i < encparams.max_active_b_frames; ++i )
    {
        delete b_pictures[i];
    }
    for( i = 0; i < encparams.max_active_ref_frames; ++i )
    {
        delete ref_pictures[i];
    }
    
}


/* 
 * Local variables:
 *  c-file-style: "stroustrup"
 *  tab-width: 4
 *  indent-tabs-mode: nil
 * End:
 */
