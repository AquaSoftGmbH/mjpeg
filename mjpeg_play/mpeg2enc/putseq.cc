/* putseq.c, sequence level routines                                        */

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
#include "global.h"
#include "motionsearch.h"
#include "ratectl.hh"


void Picture::Init( EncoderParams &_encparams )
{
    encparams = &_encparams;
	int i,j;
	/* Allocate buffers for picture transformation */
	blocks = 
        static_cast<DCTblock*>(
            bufalloc(encparams->mb_per_pict*BLOCK_COUNT*sizeof(DCTblock)));
	qblocks =
		static_cast<DCTblock *>(
            bufalloc(encparams->mb_per_pict*BLOCK_COUNT*sizeof(DCTblock)));
    DCTblock *block = blocks;
    DCTblock *qblock = qblocks;
    for (j=0; j<encparams->enc_height2; j+=16)
    {
        for (i=0; i<encparams->enc_width; i+=16)
        {
            mbinfo.push_back(MacroBlock(*this, i,j, block,qblock ));
            block += BLOCK_COUNT;
            qblock += BLOCK_COUNT;
        }
    }


	curref = new (uint8_t *)[3];
	curorg = new (uint8_t *)[3];
	pred   = new (uint8_t *)[3];

	for( i = 0 ; i<3; i++)
	{
		int size =  (i==0) ? encparams->lum_buffer_size : encparams->chrom_buffer_size;
		curref[i] = static_cast<uint8_t *>(bufalloc(size));
		curorg[i] = NULL;
		pred[i]   = static_cast<uint8_t *>(bufalloc(size));
	}

	/* The (non-existent) previous encoding using an as-yet un-used
	   picture encoding data buffers is "completed"
	*/
	sync_guard_init( &completion, 1 );
}

void Picture::SetSeqPos(int _decode,int b_index )
{
	decode = _decode;
	dc_prec = encparams->dc_prec;
	secondfield = false;
	ipflag = 0;

		
	/* Handle picture structure... */
	if( encparams->fieldpic )
	{
		pict_struct = encparams->topfirst ? TOP_FIELD : BOTTOM_FIELD;
		topfirst = 0;
		repeatfirst = 0;
	}

	/* Handle 3:2 pulldown frame pictures */
	else if( encparams->pulldown_32 )
	{
		pict_struct = FRAME_PICTURE;
		switch( present % 4 )
		{
		case 0 :
			repeatfirst = 1;
			topfirst = encparams->topfirst;			
			break;
		case 1 :
			repeatfirst = 0;
			topfirst = !encparams->topfirst;
			break;
		case 2 :
			repeatfirst = 1;
			topfirst = !encparams->topfirst;
			break;
		case 3 :
			repeatfirst = 0;
			topfirst = encparams->topfirst;
			break;
		}
	}
	
	/* Handle ordinary frame pictures */
	else

	{
		pict_struct = FRAME_PICTURE;
		repeatfirst = 0;
		topfirst = encparams->topfirst;
	}


	switch ( pict_type )
	{
	case I_TYPE :
		forw_hor_f_code = 15;
		forw_vert_f_code = 15;
		back_hor_f_code = 15;
		back_vert_f_code = 15;
		sxf = encparams->motion_data[0].sxf;
		syf = encparams->motion_data[0].syf;
		break;
	case P_TYPE :
		forw_hor_f_code = encparams->motion_data[0].forw_hor_f_code;
		forw_vert_f_code = encparams->motion_data[0].forw_vert_f_code;
		back_hor_f_code = 15;
		back_vert_f_code = 15;
		sxf = encparams->motion_data[0].sxf;
		syf = encparams->motion_data[0].syf;
		break;
	case B_TYPE :
		forw_hor_f_code = encparams->motion_data[b_index].forw_hor_f_code;
		forw_vert_f_code = encparams->motion_data[b_index].forw_vert_f_code;
		back_hor_f_code = encparams->motion_data[b_index].back_hor_f_code;
		back_vert_f_code = encparams->motion_data[b_index].back_vert_f_code;
		sxf = encparams->motion_data[b_index].sxf;
		syf = encparams->motion_data[b_index].syf;
		sxb = encparams->motion_data[b_index].sxb;
		syb = encparams->motion_data[b_index].syb;

		break;
	}

	/* We currently don't support frame-only DCT/Motion Est.  for non
	   progressive frames */
	prog_frame = encparams->frame_pred_dct_tab[pict_type-1];
	frame_pred_dct = encparams->frame_pred_dct_tab[pict_type-1];
	q_scale_type = encparams->qscale_tab[pict_type-1];
	intravlc = encparams->intravlc_tab[pict_type-1];
	altscan = encparams->altscan_tab[pict_type-1];
    scan_pattern = (altscan ? alternate_scan : zig_zag_scan);

    /* If we're using B frames then we reserve unit coefficient
       dropping for them as B frames have no 'knock on' information
       loss */
    if( pict_type == B_TYPE || ctl_M == 1 )
    {
        unit_coeff_threshold = abs( ctl_unit_coeff_elim );
        unit_coeff_first = ctl_unit_coeff_elim < 0 ? 0 : 1;
    }
    else
    {
        unit_coeff_threshold = 0;
        unit_coeff_first = 0;
    }
        

#ifdef OUTPUT_STAT
	fprintf(statfile,"\nFrame %d (#%d in display order):\n",decode,display);
	fprintf(statfile," picture_type=%c\n",pict_type_char[pict_type]);
	fprintf(statfile," temporal_reference=%d\n",temp_ref);
	fprintf(statfile," frame_pred_frame_dct=%d\n",frame_pred_dct);
	fprintf(statfile," q_scale_type=%d\n",q_scale_type);
	fprintf(statfile," intra_vlc_format=%d\n",intravlc);
	fprintf(statfile," alternate_scan=%d\n",altscan);

	if (pict_type!=I_TYPE)
	{
		fprintf(statfile," forward search window: %d...%d / %d...%d\n",
				-sxf,sxf,-syf,syf);
		fprintf(statfile," forward vector range: %d...%d.5 / %d...%d.5\n",
				-(4<<forw_hor_f_code),(4<<forw_hor_f_code)-1,
				-(4<<forw_vert_f_code),(4<<forw_vert_f_code)-1);
	}

	if (pict_type==B_TYPE)
	{
		fprintf(statfile," backward search window: %d...%d / %d...%d\n",
				-sxb,sxb,-syb,syb);
		fprintf(statfile," backward vector range: %d...%d.5 / %d...%d.5\n",
				-(4<<back_hor_f_code),(4<<back_hor_f_code)-1,
				-(4<<back_vert_f_code),(4<<back_vert_f_code)-1);
	}
#endif


}

/*
 * Adjust picture parameters for the second field in a pair of field
 * pictures.
 *
 */

void Picture::Set2ndField()
{
	secondfield = true;
    gop_start = false;
	if( pict_struct == TOP_FIELD )
		pict_struct =  BOTTOM_FIELD;
	else
		pict_struct =  TOP_FIELD;
	
	if( pict_type == I_TYPE )
	{
		ipflag = 1;
		pict_type = P_TYPE;
		
		forw_hor_f_code = encparams->motion_data[0].forw_hor_f_code;
		forw_vert_f_code = encparams->motion_data[0].forw_vert_f_code;
		back_hor_f_code = 15;
		back_vert_f_code = 15;
		sxf = encparams->motion_data[0].sxf;
		syf = encparams->motion_data[0].syf;	
	}
}


/************************************
 *
 * gop_end_frame - Finds a sensible spot for a GOP to end based on
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

static int find_gop_length( int gop_start_frame, 
							int I_frame_temp_ref,
							int gop_min_len, int gop_max_len,
							int min_b_grp)
{
	int i,j;
    /*
      Checking the luminance mean of the frame also updates
      istrm_nframes if the number of frames in the stream if the end
      of stream has is reached before this frame...
    */
    int max_change = 0;
    int gop_len;
	int pred_lum_mean;

    if( gop_min_len >= gop_max_len )
        gop_len = gop_max_len;
    else
    {
        int cur_lum_mean = 
            frame_lum_mean( gop_start_frame+gop_min_len-min_b_grp+I_frame_temp_ref );

        /* Search forwards from min gop length for I-frame candidate
           which has the largest change in mean luminance.
           BUGBUGBUG: The frame ring buffer size must be large enough to
           accomodate this read-ahead without losing frames currently being
           encoded!!!
        */
        gop_len = 0;
        for( i = gop_min_len; i <= gop_max_len; i += min_b_grp )
        {
            pred_lum_mean = frame_lum_mean( gop_start_frame+i+I_frame_temp_ref);
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
                    frame_lum_mean( gop_start_frame+j+I_frame_temp_ref );
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
	if (gop_len > istrm_nframes-gop_start_frame)
		gop_len = istrm_nframes-gop_start_frame;
	mjpeg_info( "GOP start (%d frames)", gop_len);
	return gop_len;

}



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


static void create_threads( pthread_t *threads, int num, void *(*start_routine)(void *) )
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
		if( pthread_create( &threads[i], pattr, start_routine, NULL ) != 0 )
		{
			mjpeg_error_exit1( "worker thread creation failed: %s", strerror(errno) );
		}
	}
}

static void gop_start( StreamState *ss )
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
    */
    
    if( ctl_quant_floor > 0.0 )
        bits_after_mux = bitcount() + 
            (uint64_t)((frame_periods / encparams.frame_rate) * ctl_nonvid_bit_rate);
    else
        bits_after_mux = (uint64_t)((frame_periods / encparams.frame_rate) * 
                                    (ctl_nonvid_bit_rate + encparams.bit_rate));
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

    ss->closed_gop = ss->i == 0 || ctl_closed_GOPs;
	ss->gop_start_frame = ss->seq_start_frame + ss->i;
	
	/*
	  Compute GOP length based on min and max sizes specified
	  and scene changes detected.  
	  First GOP in a sequence has I frame with a 0 temp_ref
	  and (M-1) less frames (no initial B frames).
	  In all other GOPs I-frame has a temp_ref of M-1
	*/
	
    ss->gop_length =  
        find_gop_length( ss->gop_start_frame, 
                         ss->closed_gop ? 0 : ctl_M-1, 
                         ctl_N_min, ctl_N_max,
                         ctl_M_min);
			
	/* First figure out how many B frames we're short from
	   being able to achieve an even M-1 B's per I/P frame.
	   
	   To avoid peaks in necessary data-rate we try to
	   lose the B's in the middle of the GOP. We always
	   *start* with M-1 B's (makes choosing I-frame breaks simpler).
	   A complication is the extra I-frame in the initial
	   closed GOP of a sequence.
	*/
	if( ctl_M-1 > 0 )
	{
		ss->bs_short = (ctl_M - ((ss->gop_length-(ss->i==0)) % ctl_M))%ctl_M;
		ss->next_b_drop = ((double)ss->gop_length) / (double)(ss->bs_short+1)-1.0 ;
	}
	else
	{
		ss->bs_short = 0;
		ss->next_b_drop = 0.0;
	}
	
	/* We aim to spread the dropped B's evenly across the GOP */
	ss->bigrp_length = (ctl_M-1);
	
	/* number of P frames */
	if( ctl_M == 0 ) 
	{
		ss->bigrp_length = 0;
		np = 0;
	}
	else if (ss->closed_gop )
	{
		ss->bigrp_length = 1;
		np = (ss->gop_length + 2*(ctl_M-1))/ctl_M - 1; /* Closed GOP */
	}
	else
	{
		ss->bigrp_length = ctl_M;
		np = (ss->gop_length + (ctl_M-1))/ctl_M - 1;
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



/* Set the sequencing structure information
   of a picture (type and temporal reference)
   based on the specified sequence state
*/

void Picture::Set_IP_Frame( StreamState *ss )
{
	/* Temp ref of I frame in closed GOP of sequence is 0 We have to
	   be a little careful with the end of stream special-case.
	*/
	if( ss->g == 0 && ss->closed_gop )
	{
		temp_ref = 0;
	}
	else 
	{
		temp_ref = ss->g+(ss->bigrp_length-1);
	}

	if (temp_ref >= (istrm_nframes-ss->gop_start_frame))
		temp_ref = (istrm_nframes-ss->gop_start_frame) - 1;

	present = (ss->i-ss->g)+temp_ref;
	if (ss->g==0) /* first displayed frame in GOP is I */
	{
		pict_type = I_TYPE;
	}
	else 
	{
		pict_type = P_TYPE;
	}

	/* Start of GOP - set GOP data for picture */
	if( ss->g == 0 )
	{
		gop_start = true;
        closed_gop = ss->closed_gop;
		new_seq = ss->new_seq;
		nb = ss->nb;
		np = ss->np;
	}		
	else
	{
		gop_start = false;
        closed_gop = false;
		new_seq = false;
	}
}


void Picture::Set_B_Frame(  StreamState *ss )
{
	temp_ref = ss->g - 1;
	present = ss->i-1;
	pict_type = B_TYPE;
	gop_start = false;
	new_seq = false;
}

/*
  Update ss to the next sequence state.
*/

static void next_seq_state( StreamState *ss )
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
			ss->bigrp_length = ctl_M - 1;
			if( ss->bs_short )
				ss->next_b_drop += ((double)ss->gop_length) / (double)(ss->bs_short+1) ;
		}
		else
			ss->bigrp_length = ctl_M;
	}

    /* Are we starting a new GOP? */
	if( ss->g == ss->gop_length )
	{
		gop_start(ss);
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


static void link_pictures( Picture *ref_pictures, 
                           Picture *b_pictures )
{

	int i,j;

	
	for( i = 0; i < ctl_max_active_ref_frames; ++i )
		ref_pictures[i].Init( encparams);

	for( i = 0; i < ctl_max_active_ref_frames; ++i )
	{
		j = (i + 1) % ctl_max_active_ref_frames;

		ref_pictures[j].oldorg = ref_pictures[i].curorg;
		ref_pictures[j].oldref = ref_pictures[i].curref;
		ref_pictures[j].neworg = ref_pictures[j].curorg;
		ref_pictures[j].newref = ref_pictures[j].curref;
	}

	for( i = 0; i < ctl_max_active_b_frames; ++i )
	{
		b_pictures[i].Init( encparams );
	}	

}

/*
 *
 * Reconstruct the decoded image for references images and
 * for statistics
 *
 */


static void reconstruct( Picture *picture)
{

#ifndef OUTPUT_STAT
	if( picture->pict_type!=B_TYPE)
	{
#endif
		iquantize( picture );
		itransform(picture);
		calcSNR(picture);
		stats();
#ifndef OUTPUT_STAT
	}
#endif
}

static mp_semaphore_t worker_available =  SEMAPHORE_INITIALIZER;
static mp_semaphore_t picture_available = SEMAPHORE_INITIALIZER;
static mp_semaphore_t picture_started = SEMAPHORE_INITIALIZER;

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

static void encodembs(Picture *picture)
{ 
    vector<MacroBlock>::iterator mbi = picture->mbinfo.begin();

	for( mbi = picture->mbinfo.begin(); mbi < picture->mbinfo.end(); ++mbi)
	{
        mbi->MotionEstimate();
        // TODO: Eventually we will allow alternative selectors to be used!
        mbi->SelectCodingModeOnVariance();
        mbi->Predict();
        mbi->Transform();
	}

}

static void stencodeworker(Picture *picture)
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

	motion_subsampled_lum(picture);

		
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

#ifdef ORIGINAL_PHASE_BASED_PROC
	motion_estimation(picture);
	predict(picture);
	transform(picture);
#else
    encodembs(picture);
#endif
	/* Depends on previous frame completion for IB and P */

	picture->PutHeadersAndEncoding(*encparams.bitrate_controller);

	reconstruct(picture);

	/* Handle second field of a frame that is being field encoded */
	if( encparams.fieldpic )
	{
		picture->Set2ndField();
		mjpeg_info("Field %s (%d)",
				   (picture->pict_struct == TOP_FIELD) ? "top" : "bot",
				   picture->pict_struct
			);

		motion_estimation(picture);
		predict(picture);
		transform(picture);
		picture->PutHeadersAndEncoding(*encparams.bitrate_controller);
		reconstruct(picture);

	}


	mjpeg_info("Frame end %d %c quant=%3.2f total act=%8.5f %s", 
               picture->decode, 
			   pict_type_char[picture->pict_type],
               picture->AQ,
               picture->sum_avg_act,
               picture->pad ? "PAD" : "   "
        );
			
}

/*
  Ohh, lovely C type syntax... more or less have to introduce a named
  typed here to bind the "volatile" correctly - to the pointer not the
  data it points to. K&R: hang your heads in shame...
*/

typedef Picture * pict_data_ptr;

volatile static pict_data_ptr picture_to_encode;


static void *parencodeworker(void *start_arg)
{
	pict_data_ptr picture;
	mjpeg_debug( "Worker thread started" );

	for(;;)
	{
		mp_semaphore_signal( &worker_available, 1);
		mp_semaphore_wait( &picture_available );
		/* Precisely *one* worker is started after update of
		   picture_for_started_worker, so no need for handshake.  */
		picture = (Picture *)picture_to_encode;
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

		motion_subsampled_lum(picture);

		
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
#ifdef ORIGINAL_PHASE_BASED_PROC
		if( ctl_refine_from_rec )
		{
			sync_guard_test( &picture->ref_frame->completion );
			motion_estimation(picture);
		}
		else
		{
			motion_estimation(picture);
			sync_guard_test( &picture->ref_frame->completion );
		}
		predict(picture);

		/* No dependency */
		transform(picture);
#else
        sync_guard_test( &picture->ref_frame->completion );
        encodembs(picture);
#endif
		/* Depends on previous frame completion for IB and P */
		sync_guard_test( &picture->prev_frame->completion );
		picture->PutHeadersAndEncoding(*encparams.bitrate_controller);

		reconstruct(picture);
		/* Handle second field of a frame that is being field encoded */
		if( encparams.fieldpic )
		{
			picture->Set2ndField();

			mjpeg_info("Field %s (%d)",
					   (picture->pict_struct == TOP_FIELD) ? "top" : "bot",
					   picture->pict_struct
				);

			motion_estimation(picture);
			predict(picture);
			transform(picture);
            picture->PutHeadersAndEncoding(*encparams.bitrate_controller);
			reconstruct(picture);

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

	return NULL;
}


static void parencodepict( Picture *picture )
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
 *  Buffers sizes are given by ctl_max_active_ref_frames and ctl_max_active_b_frames
 *
 ********************/
 
void putseq(void)
{
    /* DEBUG */
	uint64_t bits_after_mux;
	double frame_periods;
    /* END DEBUG */
	StreamState ss;
	int cur_ref_idx = 0;
	int cur_b_idx = 0;

    int i;
	Picture b_pictures[ctl_max_active_b_frames];
	Picture ref_pictures[ctl_max_active_ref_frames];

	pthread_t worker_threads[ctl_max_encoding_frames];
	Picture *cur_picture, *old_picture;
	Picture *new_ref_picture, *old_ref_picture;

	link_pictures( ref_pictures, b_pictures );
	if( ctl_max_encoding_frames > 1 )
		create_threads( worker_threads, ctl_max_encoding_frames, parencodeworker );

	/* Initialize image dependencies and synchronisation.  The
	   first frame encoded has no predecessor whose completion it
	   must wait on.
	*/
	old_ref_picture = &ref_pictures[ctl_max_active_ref_frames-1];
	new_ref_picture = &ref_pictures[cur_ref_idx];
	cur_picture = old_ref_picture;
	
	encparams.bitrate_controller->InitSeq(false);
	
	ss.i = 0;		                /* Index in current MPEG sequence */
	ss.g = 0;						/* Index in current GOP */
	ss.b = 0;						/* B frames since last I/P */
	ss.gop_length = 0;				/* Length of current GOP init 0
									   0 force new GOP at start 1st sequence */
	ss.seq_start_frame = 0;		/* Index start current sequence in
								   input stream */
	ss.gop_start_frame = 0;		/* Index start current gop in input stream */
	ss.seq_split_length = ((int64_t)ctl_seq_length_limit)*(8*1024*1024);
	ss.next_split_point = BITCOUNT_OFFSET + ss.seq_split_length;
	mjpeg_debug( "Split len = %" PRId64 "", ss.seq_split_length );

	frame_num = 0;              /* Encoding number */

	gop_start(&ss);

	/* loop through all frames in encoding/decoding order */
	while( frame_num<istrm_nframes )
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
			cur_ref_idx = (cur_ref_idx + 1) % ctl_max_active_ref_frames;
            index = cur_ref_idx;
			old_ref_picture = new_ref_picture;
			new_ref_picture = &ref_pictures[cur_ref_idx];
			new_ref_picture->ref_frame = old_ref_picture;
			new_ref_picture->prev_frame = cur_picture;
			new_ref_picture->Set_IP_Frame(&ss);
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
			cur_b_idx = ( cur_b_idx + 1) % ctl_max_active_b_frames;
            index = cur_b_idx;
			new_b_picture = &b_pictures[cur_b_idx];
			new_b_picture->oldorg = new_ref_picture->oldorg;
			new_b_picture->oldref = new_ref_picture->oldref;
			new_b_picture->neworg = new_ref_picture->neworg;
			new_b_picture->newref = new_ref_picture->newref;
			new_b_picture->ref_frame = new_ref_picture;
			new_b_picture->prev_frame = cur_picture;
			new_b_picture->Set_B_Frame( &ss );
			cur_picture = new_b_picture;
		}

		sync_guard_update( &cur_picture->completion, 0 );

		if( readframe(cur_picture->temp_ref+ss.gop_start_frame,cur_picture->curorg) )
		{
		    mjpeg_error_exit1("Corrupt frame data in frame %d aborting!",
							  cur_picture->temp_ref+ss.gop_start_frame );
		}


		cur_picture->SetSeqPos( ss.i, ss.b );
		if( ctl_max_encoding_frames > 1 )
			parencodepict( cur_picture );
		else
			stencodeworker( cur_picture );

#ifdef DEBUG
		writeframe(cur_picture->temp_ref+ss.gop_start_frame,cur_picture->curref);
#endif

		next_seq_state( &ss );
		++frame_num;
	}
	
        
	/* Wait for final frame's encoding to complete */
	if( ctl_max_encoding_frames > 1 )
		sync_guard_test( &cur_picture->completion );
	putseqend();

	if( encparams.pulldown_32 )
		frame_periods = (double)(ss.seq_start_frame + ss.i)*(5.0/4.0);
	else
		frame_periods = (double)(ss.seq_start_frame + ss.i);
    
    /* DEBUG */
    
    if( ctl_quant_floor > 0.0 )
        bits_after_mux = bitcount() + 
            (uint64_t)((frame_periods / encparams.frame_rate) * ctl_nonvid_bit_rate);
    else
        bits_after_mux = (uint64_t)((frame_periods / encparams.frame_rate) * 
                                    (ctl_nonvid_bit_rate + encparams.bit_rate));

    mjpeg_info( "Guesstimated final muxed size = %lld\n", bits_after_mux/8 );
    /* END DEBUG */

}


/* 
 * Local variables:
 *  c-file-style: "stroustrup"
 *  tab-width: 4
 *  indent-tabs-mode: nil
 * End:
 */
