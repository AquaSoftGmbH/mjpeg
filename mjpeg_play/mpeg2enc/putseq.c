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

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "global.h"



static void set_pic_params( int decode,
							int b_index,
							pict_data_s *picture )
{
	picture->decode = decode;
	picture->dc_prec = opt_dc_prec;
	picture->prog_frame = opt_prog_frame;
	picture->secondfield = 0;
	picture->ipflag = 0;

		
	/* Handle picture structure... */
	if( opt_fieldpic )
	{
		picture->pict_struct = opt_topfirst ? TOP_FIELD : BOTTOM_FIELD;
		picture->topfirst = 0;
		picture->repeatfirst = 0;
	}

	/* Handle 3:2 pulldown frame pictures */
	else if( opt_pulldown_32 )
	{
		picture->pict_struct = FRAME_PICTURE;
		switch( picture->present % 4 )
		{
		case 0 :
			picture->repeatfirst = 1;
			picture->topfirst = opt_topfirst;			
			break;
		case 1 :
			picture->repeatfirst = 0;
			picture->topfirst = !opt_topfirst;
			break;
		case 2 :
			picture->repeatfirst = 1;
			picture->topfirst = !opt_topfirst;
			break;
		case 3 :
			picture->repeatfirst = 0;
			picture->topfirst = opt_topfirst;
			break;
		}
	}
	
	/* Handle ordinary frame pictures */
	else

	{
		picture->pict_struct = FRAME_PICTURE;
		picture->repeatfirst = 0;
		picture->topfirst = opt_topfirst;
	}


	switch ( picture->pict_type )
	{
	case I_TYPE :
		picture->forw_hor_f_code = 15;
		picture->forw_vert_f_code = 15;
		picture->sxf = opt_motion_data[0].sxf;
		picture->syf = opt_motion_data[0].syf;
		break;
	case P_TYPE :
		picture->forw_hor_f_code = opt_motion_data[0].forw_hor_f_code;
		picture->forw_vert_f_code = opt_motion_data[0].forw_vert_f_code;
		picture->back_hor_f_code = 15;
		picture->back_vert_f_code = 15;
		picture->sxf = opt_motion_data[0].sxf;
		picture->syf = opt_motion_data[0].syf;
		break;
	case B_TYPE :
		picture->forw_hor_f_code = opt_motion_data[b_index].forw_hor_f_code;
		picture->forw_vert_f_code = opt_motion_data[b_index].forw_vert_f_code;
		picture->back_hor_f_code = opt_motion_data[b_index].back_hor_f_code;
		picture->back_vert_f_code = opt_motion_data[b_index].back_vert_f_code;
		picture->sxf = opt_motion_data[b_index].sxf;
		picture->syf = opt_motion_data[b_index].syf;
		picture->sxb = opt_motion_data[b_index].sxb;
		picture->syb = opt_motion_data[b_index].syb;

		break;
	}

	picture->frame_pred_dct = opt_frame_pred_dct_tab[picture->pict_type-1];
	picture->q_scale_type = opt_qscale_tab[picture->pict_type-1];
	picture->intravlc = opt_intravlc_tab[picture->pict_type-1];
	picture->altscan = opt_altscan_tab[picture->pict_type-1];

#ifdef OUTPUT_STAT
		fprintf(statfile,"\nFrame %d (#%d in display order):\n",decode,display);
		fprintf(statfile," picture_type=%c\n",pict_type_char[picture->pict_type]);
		fprintf(statfile," temporal_reference=%d\n",picture->temp_ref);
		fprintf(statfile," frame_pred_frame_dct=%d\n",picture->frame_pred_dct);
		fprintf(statfile," q_scale_type=%d\n",picture->q_scale_type);
		fprintf(statfile," intra_vlc_format=%d\n",picture->intravlc);
		fprintf(statfile," alternate_scan=%d\n",picture->altscan);

		if (picture->pict_type!=I_TYPE)
		{
			fprintf(statfile," forward search window: %d...%d / %d...%d\n",
					-sxf,sxf,-syf,syf);
			fprintf(statfile," forward vector range: %d...%d.5 / %d...%d.5\n",
					-(4<<picture->forw_hor_f_code),(4<<picture->forw_hor_f_code)-1,
					-(4<<picture->forw_vert_f_code),(4<<picture->forw_vert_f_code)-1);
		}

		if (picture->pict_type==B_TYPE)
		{
			fprintf(statfile," backward search window: %d...%d / %d...%d\n",
					-sxb,sxb,-syb,syb);
			fprintf(statfile," backward vector range: %d...%d.5 / %d...%d.5\n",
					-(4<<picture->back_hor_f_code),(4<<picture->back_hor_f_code)-1,
					-(4<<picture->back_vert_f_code),(4<<picture->back_vert_f_code)-1);
		}
#endif


}

/*
 * Adjust picture parameters for the second field in a pair of field
 * pictures.
 *
 */

static void set_2nd_field_params(pict_data_s *picture)
{
	picture->secondfield = 1;
	if( picture->pict_struct == TOP_FIELD )
		picture->pict_struct =  BOTTOM_FIELD;
	else
		picture->pict_struct =  TOP_FIELD;
	
	if( picture->pict_type == I_TYPE )
	{
		picture->ipflag = 1;
		picture->pict_type = P_TYPE;
		
		picture->forw_hor_f_code = opt_motion_data[0].forw_hor_f_code;
		picture->forw_vert_f_code = opt_motion_data[0].forw_vert_f_code;
		picture->back_hor_f_code = 15;
		picture->back_vert_f_code = 15;
		picture->sxf = opt_motion_data[0].sxf;
		picture->syf = opt_motion_data[0].syf;	
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
							int gop_min_len, int gop_max_len )
{
	int i,j;

	int cur_lum_mean = 
		frame_lum_mean( gop_start_frame+gop_max_len+I_frame_temp_ref );
	int pred_lum_mean;

	/* Search backwards from max gop length for I-frame candidate starting 
	   adjusting for I-frame temporal reference
	 */
	for( i = gop_max_len; i >= gop_min_len; --i )
	{
		pred_lum_mean =  frame_lum_mean( gop_start_frame+i+I_frame_temp_ref-1 );
		if( abs(cur_lum_mean-pred_lum_mean ) >= SCENE_CHANGE_THRESHOLD )
			break;
		cur_lum_mean = pred_lum_mean;
	}

	if( i < gop_min_len )
	{

		/* No scene change detected within maximum GOP length.
		   Now do a look-ahead check to see if running a maximum length
		   GOP next would put scene change into the GOP after-next
		   that would need a GOP smaller than minum to handle...
		*/
		pred_lum_mean = 
			frame_lum_mean( gop_start_frame+gop_max_len+I_frame_temp_ref );
		for( j = gop_max_len+1; j < gop_max_len+gop_min_len; ++j )
		{
			cur_lum_mean = frame_lum_mean( gop_start_frame+j+I_frame_temp_ref );
			if(  abs(cur_lum_mean-pred_lum_mean ) >= SCENE_CHANGE_THRESHOLD )
				break;
		}
		
		/* If a max length GOP next would cause a scene change in a
		   place where the GOP after-next would be under-size to handle it
		   *and* making the current GOP minimum size would allow an acceptable
		   GOP after-next size the next GOP to give it and after-next
		   roughly equal sizes
		   otherwise simply set the GOP to maximum length
		*/

		if( j < gop_max_len+gop_min_len && j >= gop_min_len+gop_min_len )
		{
			i = j/2;
			if( i < gop_min_len )
			{
				mjpeg_debug("GOP min length too small to permit scene-change on GOP boundary %d\n", j);
				i = gop_min_len;
			}
		}
		else
			i = gop_max_len;
	}

	if( i != gop_max_len )
		mjpeg_debug( "GOP nonstandard size %d\n", i );

	/* last GOP may contain less frames */
	if (i > istrm_nframes-gop_start_frame)
		i = istrm_nframes-gop_start_frame;
	return i;

}



struct _stream_state 
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
	 int new_seq;				/* Current GOP/frame starts new sequence */
	int64_t next_split_point;
	int64_t seq_split_length;
};

typedef struct _stream_state stream_state_s;

#ifndef SINGLE_THREADED
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
			perror( "worker thread creation failed: " );
			exit(1);
		}
	}
}
#endif

static void gop_start( stream_state_s *ss )
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
	ss->new_seq = 0;
	
	if( opt_pulldown_32 )
		frame_periods = (double)(ss->seq_start_frame + ss->i)*(5.0/4.0);
	else
		frame_periods = (double)(ss->seq_start_frame + ss->i);
	bits_after_mux = bitcount() + 
		(uint64_t)((frame_periods / opt_frame_rate) * ctl_nonvid_bit_rate);
	if( ss->next_split_point != 0LL && 	bits_after_mux > ss->next_split_point )
	{
		mjpeg_info( "Splitting sequence this GOP start\n" );
		ss->next_split_point += ss->seq_split_length;
		/* This is the input stream display order sequence number of
		   the frame that will become frame 0 in display
		   order in  the new sequence */
		ss->seq_start_frame += ss->i;
		ss->i = 0;
		ss->new_seq = 1;
	}
			
	ss->gop_start_frame = ss->seq_start_frame + ss->i;
	
	/*
	  Compute GOP length based on min and max sizes specified
	  and scene changes detected.  
	  First GOP in a sequence has I frame with a 0 temp_ref
	  nad (M-1) less frames (no initial B frames).
	  all other GOPs have a temp_ref of M-1
	*/
	
	if( ss->i == 0 )
		ss->gop_length =  
			find_gop_length( ss->gop_start_frame, 0, 
							 ctl_N_min-(ctl_M-1), ctl_N_max-(ctl_M-1));
	else
		ss->gop_length = 
			find_gop_length( ss->gop_start_frame, ctl_M-1, 
							 ctl_N_min, ctl_N_max);
	
			
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
	if (ss->i == 0)
	{
		ss->bigrp_length = 1;
		np = (ss->gop_length + 2*(ctl_M-1))/ctl_M - 1; /* first GOP */
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
		mjpeg_error_exit1( "****INTERNAL: inconsistent GOP %d %d %d\n", 
						   ss->gop_length, np, nb);
	}

}



/* Set the sequencing structure information
   of a picture (type and temporal reference)
   based on the specified sequence state
*/

static void I_or_P_frame_struct( stream_state_s *ss,
                         pict_data_s *picture )
{
	/* Temp ref of I frame in initial closed GOP of sequence is 0 
	   We have to be a little careful with the end of stream special-case.
	 */
	if( ss->i == 0 )
	{
		picture->temp_ref =  ss->i;
		picture->present = 0;
	}
	else 
	{
		picture->temp_ref = ss->g+(ss->bigrp_length-1);
		picture->present = ss->i+(ss->bigrp_length);
	}

	if (picture->temp_ref >= (istrm_nframes-ss->gop_start_frame))
		picture->temp_ref = (istrm_nframes-ss->gop_start_frame) - 1;

	picture->present = (ss->i-ss->g)+picture->temp_ref;
	if (ss->g==0) /* first displayed frame in GOP is I */
	{
		picture->pict_type = I_TYPE;
	}
	else 
	{
		picture->pict_type = P_TYPE;
	}

	/* Start of GOP - set GOP data for picture */
	if( ss->g == 0 )
	{
		picture->gop_start = 1;
		picture->nb = ss->nb;
		picture->np = ss->np;
		picture->new_seq = ss->new_seq;
	}		
	else
	{
		picture->gop_start = 0;
		picture->new_seq = 0;
	}
}


static void B_frame_struct(  stream_state_s *ss,
					  pict_data_s *picture )
{
	picture->temp_ref = ss->g - 1;
	picture->present = ss->i;
	picture->pict_type = B_TYPE;
	picture->gop_start = 0;
	picture->new_seq = 0;
}

/*
  Update ss to the next sequence state.
 */

static void next_seq_state( stream_state_s *ss )
{
	++(ss->i);
	++(ss->g);
	++(ss->b);	

	/* Are we starting a new B group */
	if( ss->b == ss->bigrp_length )
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



static void init_pict_data( pict_data_s *picture )
{
	int i;
		/* Allocate buffers for picture transformation */
	picture->qblocks =
		(int16_t (*)[64])bufalloc(mb_per_pict*block_count*sizeof(int16_t [64]));
	picture->mbinfo = 
		(struct mbinfo *)bufalloc(mb_per_pict*sizeof(struct mbinfo));

	picture->blocks =
		(int16_t (*)[64])bufalloc(mb_per_pict*block_count*sizeof(int16_t [64]));

	picture->curref = (uint8_t**)bufalloc( sizeof( uint8_t[3] ) );
	picture->curorg = (uint8_t**)bufalloc( sizeof( uint8_t[3] ) );
	picture->pred = (uint8_t**)bufalloc( sizeof( uint8_t[3] ) );

	for( i = 0 ; i<3; i++)
	{
		int size =  (i==0) ? lum_buffer_size : chrom_buffer_size;
		picture->curref[i] = bufalloc(size);
		picture->curorg[i] = NULL;
		picture->pred[i]   = bufalloc(size);
	}

	/* The (non-existent) previous encoding using an as-yet un-used
	   picture encoding data buffers is "completed"
	*/
	sync_guard_init( &picture->completion, 1 );
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

#define R_PICS (MAX_WORKER_THREADS+2)
#define B_PICS (MAX_WORKER_THREADS+2)

static void init_pictures( pict_data_s *ref_pictures, pict_data_s *b_pictures )
{

	int i,j;
	
	for( i = 0; i < R_PICS; ++i )
		init_pict_data( &ref_pictures[i] );

	for( i = 0; i < R_PICS; ++i )
	{
		j = (i + 1) % R_PICS;

		ref_pictures[j].oldorg = ref_pictures[i].curorg;
		ref_pictures[j].oldref = ref_pictures[i].curref;
		ref_pictures[j].neworg = ref_pictures[j].curorg;
		ref_pictures[j].newref = ref_pictures[j].curref;
	}

	for( i = 0; i < B_PICS; ++i )
	{
		init_pict_data( &b_pictures[i]);
	}
}

/*
 *
 * Reconstruct the decoded image for references images and
 * for statistics
 *
 */


static void reconstruct( pict_data_s *picture)
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

semaphore_t worker_available =  SEMAPHORE_INITIALIZER;
semaphore_t picture_available = SEMAPHORE_INITIALIZER;
semaphore_t picture_started = SEMAPHORE_INITIALIZER;

#ifdef SINGLE_THREADED
static void stencodeworker(pict_data_s *picture)
{
		/* ALWAYS do-able */
	mjpeg_info("Frame start %d %c %d\n",
			   picture->decode, 
			   pict_type_char[picture->pict_type],
			   picture->temp_ref);


	if( picture->pict_struct != FRAME_PICTURE )
		mjpeg_info("Field %s (%d)\n",
				   (picture->pict_struct == TOP_FIELD) ? "top" : "bot",
				   picture->pict_struct
			);

		fast_motion_data(picture);

		
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

		motion_estimation(picture);
		predict(picture);

		/* No dependency */
		dct_type_estimation(picture);
		transform(picture);
		/* Depends on previous frame completion for IB and P */

		putpict(picture);

		reconstruct(picture);

		/* Handle second field of a frame that is being field encoded */
		if( opt_fieldpic )
		{
			set_2nd_field_params(picture);
			mjpeg_info("Field %s (%d)\n",
					   (picture->pict_struct == TOP_FIELD) ? "top" : "bot",
					   picture->pict_struct
				);

			motion_estimation(picture);
			predict(picture);
			dct_type_estimation(picture);
			transform(picture);
			putpict(picture);
			reconstruct(picture);

		}


		mjpeg_debug("Frame end %d %s %3.2f %.2f %2.1f %.2f\n",
					picture->decode, 
					picture->pad ? "PAD" : "   ",
					picture->avg_act, picture->sum_avg_act,
					picture->AQ, picture->SQ);
			
}
#else

static volatile pict_data_s *picture_to_encode;


static void *parencodeworker(void *start_arg)
{
	pict_data_s *picture;
	mjpeg_debug( "Worker thread started\n" );

	for(;;)
	{
		mp_semaphore_signal( &worker_available, 1);
		mp_semaphore_wait( &picture_available );
		/* Precisely *one* worker is started after update of
		   picture_for_started_worker, so no need for handshake.  */
		picture = (pict_data_s *)picture_to_encode;
		mp_semaphore_signal( &picture_started, 1);

		/* ALWAYS do-able */
		mjpeg_info("Frame %d %c %d\n",  
				   picture->decode,  pict_type_char[picture->pict_type],
				   picture->temp_ref);

		if( picture->pict_struct != FRAME_PICTURE )
			mjpeg_info("Field %s (%d)\n",
					   (picture->pict_struct == TOP_FIELD) ? "top" : "bot",
					   picture->pict_struct
				);

		fast_motion_data(picture);

		
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
		if( ctl_refine_from_rec )
		{
			sync_guard_test( picture->ref_frame_completion );
			motion_estimation(picture);
		}
		else
		{
			motion_estimation(picture);
			sync_guard_test( picture->ref_frame_completion );
		}
		predict(picture);

		/* No dependency */
		dct_type_estimation(picture);
		transform(picture);
		/* Depends on previous frame completion for IB and P */
		sync_guard_test( picture->prev_frame_completion );
		putpict(picture);

		reconstruct(picture);
		/* Handle second field of a frame that is being field encoded */
		if( opt_fieldpic )
		{
			set_2nd_field_params(picture);

			mjpeg_info("Field %s (%d)\n",
					   (picture->pict_struct == TOP_FIELD) ? "top" : "bot",
					   picture->pict_struct
				);

			motion_estimation(picture);
			predict(picture);
			dct_type_estimation(picture);
			transform(picture);
			putpict(picture);
			reconstruct(picture);

		}


		mjpeg_debug("Frame end %d %s %3.2f %.2f %2.1f %.2f\n",
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


static void parencodepict( pict_data_s *picture )
{

	mp_semaphore_wait( &worker_available );
	picture_to_encode = picture;
	mp_semaphore_signal( &picture_available, 1 );
	mp_semaphore_wait( &picture_started );
}
#endif


/*********************
 *
 *  Multi-threading putseq 
 *
 *  N.b. It is *critical* that the reference and b picture buffers are
 *  at least two larger than the number of encoding worker threads.
 *  The despatcher thread *must* have to halt to wait for free
 *  worker threads *before* it re-uses the record for the
 *  pict_data_s record for the oldest frame that could still be needed
 *  by active worker threads.
 * 
 *  Buffers sizes are given by R_PICS and B_PICS
 *
 ********************/
 
void putseq()
{
	stream_state_s ss;
	int cur_ref_idx = 0;
	int cur_b_idx = 0;
	pict_data_s b_pictures[B_PICS];
	pict_data_s ref_pictures[R_PICS];
#ifndef SINGLE_THREADED
	pthread_t worker_threads[MAX_WORKER_THREADS];
#endif
	pict_data_s *cur_picture, *old_picture;
	pict_data_s *new_ref_picture, *old_ref_picture;

	init_pictures( ref_pictures, b_pictures );
#ifndef SINGLE_THREADED
	create_threads( worker_threads, ctl_max_encoding_frames, parencodeworker );
#endif
	/* Initialize image dependencies and synchronisation.  The
	   first frame encoded has no predecessor whose completion it
	   must wait on.
	*/
	old_ref_picture = &ref_pictures[R_PICS-1];
	new_ref_picture = &ref_pictures[cur_ref_idx];
	cur_picture = old_ref_picture;
	
	rc_init_seq(0);
	putseqhdr();
	
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
	mjpeg_debug( "Split len = %lld\n", ss.seq_split_length );

	frame_num = 0;              /* Encoding number */

	gop_start(&ss);

	/* loop through all frames in encoding/decoding order */
	while( frame_num<istrm_nframes )
	{
		old_picture = cur_picture;

		/* Each bigroup starts once all the B frames of its predecessor
		   have finished.
		*/
		if ( ss.b == 0)
		{
			cur_ref_idx = (cur_ref_idx + 1) % R_PICS;
			old_ref_picture = new_ref_picture;
			new_ref_picture = &ref_pictures[cur_ref_idx];
			new_ref_picture->ref_frame_completion = &old_ref_picture->completion;
			new_ref_picture->prev_frame_completion = &cur_picture->completion;
			I_or_P_frame_struct(&ss, new_ref_picture);
			cur_picture = new_ref_picture;
		}
		else
		{
			pict_data_s *new_b_picture;
			/* B frame: no need to change the reference frames.
			   The current frame data pointers are a 3rd set
			   seperate from the reference data pointers.
			*/
			cur_b_idx = ( cur_b_idx + 1) % B_PICS;
			new_b_picture = &b_pictures[cur_b_idx];
			new_b_picture->oldorg = new_ref_picture->oldorg;
			new_b_picture->oldref = new_ref_picture->oldref;
			new_b_picture->neworg = new_ref_picture->neworg;
			new_b_picture->newref = new_ref_picture->newref;
			new_b_picture->ref_frame_completion = &new_ref_picture->completion;
			new_b_picture->prev_frame_completion = &cur_picture->completion;
			B_frame_struct( &ss, new_b_picture );
			cur_picture = new_b_picture;
		}

		sync_guard_update( &cur_picture->completion, 0 );

		if( readframe(cur_picture->temp_ref+ss.gop_start_frame,cur_picture->curorg) )
		{
		    mjpeg_error_exit1("Corrupt frame data in frame %d aborting!\n",
							  cur_picture->temp_ref+ss.gop_start_frame );
		}


		set_pic_params( ss.i, ss.b,   cur_picture );
#ifdef  SINGLE_THREADED
		stencodeworker( cur_picture );
#else
		parencodepict( cur_picture );
#endif

#ifdef DEBUG
		writeframe(cur_picture->temp_ref+ss.gop_start_frame,cur_picture->curref);
#endif

		next_seq_state( &ss );
		++frame_num;
	}
	
	/* Wait for final frame's encoding to complete */
#ifndef SINGLE_THREADED
	sync_guard_test( &cur_picture->completion );
#endif
	putseqend();
}
