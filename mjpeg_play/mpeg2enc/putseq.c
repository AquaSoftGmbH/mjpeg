/* putseq.c, sequence level routines                                        */

/* Original 
   Copyright (C) 1996, MPEG Software Simulation Group. All Rights Reserved. 

*/

/* Subsequent modification (C) Andrew Stevens 2000, 2001
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
#include "global.h"



static void set_pic_params( int pict_struct,
							int decode,
							int b_index,
							int secondfield,
							pict_data_s *picture )
{
	picture->decode = decode;
	picture->pict_struct = pict_struct;
	picture->dc_prec = opt_dc_prec;
	picture->prog_frame = opt_prog_frame;
	picture->repeatfirst = 0;

	/* Handle topfirst and ipflag special-case for field pictures */
	if( pict_struct != FRAME_PICTURE )
	{
		picture->topfirst = (!!secondfield) ^ (pict_struct == TOP_FIELD);
		picture->ipflag = (picture->pict_type==I_TYPE) && secondfield;
		if( picture->ipflag)
		picture->pict_type = P_TYPE;
		if (!quiet)
		{
			fprintf(stderr,"\nField %s (%s) ",
					secondfield ? "second" : "first ",
					picture->topfirst ? "top" : "bot");
			fflush(stderr);
		}

	}
	else
	{
		picture->topfirst = 0;
		picture->ipflag = 0;
	}

	switch ( picture->pict_type )
	{
	case I_TYPE :
		picture->forw_hor_f_code = 15;
		picture->forw_vert_f_code = 15;
		picture->back_hor_f_code = 15; 
		picture->back_vert_f_code = 15;
		break;
	case P_TYPE :
		picture->forw_hor_f_code = motion_data[0].forw_hor_f_code;
		picture->forw_vert_f_code = motion_data[0].forw_vert_f_code;
		picture->back_hor_f_code = 15;
		picture->back_vert_f_code = 15;
		picture->sxf = motion_data[0].sxf;
		picture->syf = motion_data[0].syf;
		break;
	case B_TYPE :
		picture->forw_hor_f_code = motion_data[b_index].forw_hor_f_code;
		picture->forw_vert_f_code = motion_data[b_index].forw_vert_f_code;
		picture->back_hor_f_code = motion_data[b_index].back_hor_f_code;
		picture->back_vert_f_code = motion_data[b_index].back_vert_f_code;
		picture->sxf = motion_data[b_index].sxf;
		picture->syf = motion_data[b_index].syf;
		picture->sxb = motion_data[b_index].sxb;
		picture->syb = motion_data[b_index].syb;

		break;
	}

		picture->frame_pred_dct = frame_pred_dct_tab[picture->pict_type-1];
		picture->q_scale_type = qscale_tab[picture->pict_type-1];
		picture->intravlc = intravlc_tab[picture->pict_type-1];
		picture->altscan = altscan_tab[picture->pict_type-1];

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

int find_gop_length( int gop_start_frame, 
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
				printf( "++++ WARNING: GOP min length too small to permit scene-change on GOP boundary%d\n", j);
				i = gop_min_len;
			}
		}
		else
			i = gop_max_len;
	}

	if( i != gop_max_len )
		printf( "DEBUG: GOP nonstandard size %d\n", i );

	/* last GOP may contain less frames */
	if (i > nframes-gop_start_frame)
		i = nframes-gop_start_frame;
	return i;

}


void encodepict(pict_data_s *picture)
{
	if (!quiet)
	{
		printf("Frame start %d %d %d\n",
			   picture->decode, 
			   pict_type_char[picture->pict_type],
			   picture->temp_ref);
	}
	
	/* TODO: fast_motion_data need not be repeated for the
	   second field as it is already done in the first... */
	fast_motion_data(picture);
	motion_estimation(picture);
	predict(picture);
	dct_type_estimation(picture);
	transform(picture);
	putpict(picture);
	
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
	#ifdef DEBUG
	if (!quiet)
	{
		printf("Frame end %d %c %3.2f %.2f %2.1f %.2f\n",
			   picture->decode, 
			   picture->pad ? 'P' : ' ',
			   picture->avg_act, picture->sum_avg_act,
			   picture->AQ, picture->SQ);
	}
#endif
	

}



struct _stream_state 
 {
	 int i;						/* Index in current sequence */
	 int g;						/* Index in current GOP */
	 int b;						/* Index in current B frame group */
	 int gop_start_frame;		/* Index start current sequence in
								   input stream */
	 int seq_start_frame;		/* Index start current gop in input stream */
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

void create_threads( pthread_t *threads, int num, void *(*start_routine)(void *) )
{
	int i;
	for(i = 0; i < num; ++i )
			{
		if( pthread_create( &threads[i], NULL, start_routine, NULL ) != 0 )
			{
			perror( "worker thread creation failed: " );
			exit(1);
		}
	}
}


void gop_start( stream_state_s *ss )
{

	int nb, np;
	/* If	we're starting a GOP and have gone past the current
	   sequence splitting point split the sequence and
	   set the next splitting point.
	*/
			
	ss->g = 0;
	ss->b = 0;
	ss->new_seq = 0;
	if( ss->next_split_point != 0LL && 	bitcount() > ss->next_split_point )
	{
		printf( "\nSplitting sequence this GOP start\n" );
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
		ss->gop_length =  find_gop_length( ss->gop_start_frame, 0, 
										  N_min-(M-1), N_max-(M-1));
	else
		ss->gop_length = 
			find_gop_length( ss->gop_start_frame, M-1, 
							 N_min, N_max);
	
			
	/* First figure out how many B frames we're short from
	   being able to achieve an even M-1 B's per I/P frame.
	   
	   To avoid peaks in necessary data-rate we try to
	   lose the B's in the middle of the GOP. We always
	   *start* with M-1 B's (makes choosing I-frame breaks simpler).
	   A complication is the extra I-frame in the initial
	   closed GOP of a sequence.
	*/
	if( M-1 > 0 )
	{
		ss->bs_short = (M - ((ss->gop_length-(ss->i==0)) % M))%M;
		ss->next_b_drop = ((double)ss->gop_length) / (double)(ss->bs_short+1)-1.0 ;
	}
	else
	{
		ss->bs_short = 0;
		ss->next_b_drop = 0.0;
	}
	
	/* We aim to spread the dropped B's evenly across the GOP */
	ss->bigrp_length = (M-1);
	
	/* number of P frames */
	if (ss->i == 0)
	{
		ss->bigrp_length = 1;
		np = (ss->gop_length + 2*(M-1))/M - 1; /* first GOP */
	}
	else
	{
		ss->bigrp_length = M;
		np = (ss->gop_length + (M-1))/M - 1;
	}
			/* number of B frames */
	nb = ss->gop_length - np - 1;
	
	ss->np = np;
	ss->nb = nb;
#ifdef GOP_MODIFICATIONS
	rc_init_GOP(np,nb);
	
	/* set closed_GOP in first GOP only 
	   No need for per-GOP seqhdr in first GOP as one
	   has already been created.
	*/
	putgophdr(ss->i == 0 ? ss->i : ss->i+(M-1),
			  ss->i == 0, 
			  ss->i != 0 && seq_header_every_gop);
#endif


}


void flip_ref_images( pict_data_s *picture )
{
	uint8_t **tmp;

	/* I or P frame: Somewhat complicated buffer handling! 

	   The "current frame" is simply an alias
	  for the new new reference frame. Saves the need to copy
	  stuff around once the frame has been processed.

	  The original reference frame image data is actually held in
	  the frame input buffers.  In input read-ahead buffer
	  management code worries about rotating them for use.
	  So to make the new old one the current new one we
	  simply move the pointers. However it is *not* enough
	  to move the pointer to the pointer block. This would
	  cause us to lose track of one pointer block so that next time
	  around the pointers would be over-written.
	  
	*/

	tmp = picture->oldorg;
	picture->oldorg = picture->neworg;
	picture->neworg = tmp;

	tmp = picture->oldref;
	picture->oldref = picture->newref;
	picture->newref = tmp;

	picture->curorg = picture->neworg;
	picture->curref = picture->newref;

}


/* Set the sequencing structure information
   of a picture (type and temporal reference)
   based on the specified sequence state
*/

void I_or_P_frame_struct( stream_state_s *ss,
                         pict_data_s *picture )
{
	/* Temp ref of I frame in initial closed GOP of sequence is 0 */
	picture->temp_ref = (ss->i == 0 ) ? ss->i : ss->g+(ss->bigrp_length-1);
	if (picture->temp_ref >= (nframes-ss->gop_start_frame))
		picture->temp_ref = (nframes-ss->gop_start_frame) - 1;
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


void B_frame_struct(  stream_state_s *ss,
					  pict_data_s *picture )
{
	picture->temp_ref = ss->g - 1;
	picture->pict_type = B_TYPE;
	picture->gop_start = 0;
	picture->new_seq = 0;
}

/*
  Update ss to the next sequence state.
 */

void next_seq_state( stream_state_s *ss )
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
			ss->bigrp_length = M - 1;
			if( ss->bs_short )
				ss->next_b_drop += ((double)ss->gop_length) / (double)(ss->bs_short+1) ;
		}
		else
			ss->bigrp_length = M;
	}

    /* Are we starting a new GOP? */
	if( ss->g == ss->gop_length )
	{
		gop_start(ss);
	}

}


void stputseq()
{
	stream_state_s ss;
	pict_data_s cur_picture;
	/* Pointer blocks pointing to original frame data in frame data buffers
	 */
	uint8_t *neworgframe[3], *oldorgframe[3], *auxorgframe[3];
	
	/* Allocate buffers for picture transformation */
	cur_picture.qblocks =
		(int16_t (*)[64])bufalloc(mb_per_pict*block_count*sizeof(int16_t [64]));
	cur_picture.mbinfo = 
		(struct mbinfo *)bufalloc(mb_per_pict*sizeof(struct mbinfo));

	cur_picture.blocks =
		(int16_t (*)[64])bufalloc(mb_per_pict*block_count*sizeof(int16_t [64]));

	/* Initialise picture buffers for simple single-threaded encoding 
	   loop */
	cur_picture.oldorg = oldorgframe;
	cur_picture.neworg = neworgframe;
	cur_picture.oldref = oldrefframe;
	cur_picture.newref = newrefframe;
	cur_picture.pred = predframe;

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
	ss.seq_split_length = ((int64_t)seq_length_limit)*(8*1024*1024);
	ss.next_split_point = BITCOUNT_OFFSET + ss.seq_split_length;
	fprintf( stderr, "DEBUG: split len = %lld\n", ss.seq_split_length );

	frame_num = 0;              /* Encoding number */

	gop_start(&ss);

	/* loop through all frames in encoding/decoding order */
	while( frame_num<nframes )
	{

		/* Each bigroup starts once all the B frames of its predecessor
		   have finished.
		*/
		if ( ss.b == 0)
		{
			flip_ref_images( &cur_picture );
			I_or_P_frame_struct(&ss, &cur_picture);
		}
		else
		{
			/* B frame: no need to change the reference frames.
			   The current frame data pointers are a 3rd set
			   seperate from the reference data pointers.
			*/
			cur_picture.curorg = auxorgframe;
			cur_picture.curref = auxframe;

			B_frame_struct( &ss, &cur_picture );
		}


		printf( "(%d %d) ",
				cur_picture.temp_ref+ss.gop_start_frame, 
				cur_picture.temp_ref );
		if( readframe(cur_picture.temp_ref+ss.gop_start_frame,cur_picture.curorg) )
		{
			fprintf( stderr, "Corrupt frame data aborting!\n" );
			exit(1);
		}


        if (fieldpic)
		{
			set_pic_params( opt_topfirst ? TOP_FIELD : BOTTOM_FIELD,
							ss.i, ss.b,   0, &cur_picture );
			encodepict( &cur_picture );

			set_pic_params( opt_topfirst ? BOTTOM_FIELD : TOP_FIELD,
							ss.i, ss.b,  1, &cur_picture );
			encodepict( &cur_picture );
		}
		else
		{
			set_pic_params( FRAME_PICTURE,
							ss.i, ss.b,   0, &cur_picture );
			encodepict( &cur_picture );
		}

#ifdef DEBUG
		writeframe(cur_picture.temp_ref+ss.gop_start_frame,cur_picture.curref);
#endif

		next_seq_state( &ss );
		++frame_num;
	}
	putseqend();
}


void init_pict_data( pict_data_s *picture )
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
	sync_guard_init( &picture->ipcompletion, 0 );
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

void init_pictures( pict_data_s *ref_pictures, pict_data_s *b_pictures )
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


semaphore_t worker_available =  SEMAPHORE_INITIALIZER;
semaphore_t picture_available = SEMAPHORE_INITIALIZER;
semaphore_t picture_started = SEMAPHORE_INITIALIZER;
static volatile pict_data_s *picture_to_encode;

void *parencodeworker(void *start_arg)
{
	pict_data_s *picture;
	printf( "Worker thread started\n" );

	for(;;)
	{
		semaphore_signal( &worker_available, 1);
		semaphore_wait( &picture_available );
		/* Precisely *one* worker is started after update of
		   picture_for_started_worker, so no need for handshake.  */
		picture = (pict_data_s *)picture_to_encode;
		semaphore_signal( &picture_started, 1);

		/* ALWAYS do-able */
		if (!quiet)
		{
			printf("Frame start %d %c %d\n",
				   picture->decode, 
				   pict_type_char[picture->pict_type],
				   picture->temp_ref);
		}

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
		if( fieldpic && picture->ipflag )
			sync_guard_test( &picture->ipcompletion );
		else
			sync_guard_test( picture->ref_frame_completion );
		motion_estimation(picture);
		predict(picture);

		/* No dependency */
		dct_type_estimation(picture);
		transform(picture);
		/* Depends on previous frame completion for IB and P */
		sync_guard_test( picture->prev_frame_completion );
		putpict(picture);
	
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
#ifdef DEBUG
		if (!quiet)
		{
			printf("Frame end %d %c %3.2f %.2f %2.1f %.2f\n",
				   picture->decode, 
				   picture->pad ? 'P' : ' ',
				   picture->avg_act, picture->sum_avg_act,
				   picture->AQ, picture->SQ);
		}
#endif
		/* If it is a frame picture or the second field of a field picture
		   we're finished!
		*/
		if( picture->pict_struct == FRAME_PICTURE ||
			( picture->topfirst && picture->pict_struct == BOTTOM_FIELD ) ||
			( ! picture->topfirst && picture->pict_struct == TOP_FIELD )
			)
			sync_guard_update( &picture->completion, 1 );
		else if( picture->pict_struct != FRAME_PICTURE &&
				 picture->pict_type == I_TYPE )
			sync_guard_update( &picture->ipcompletion, 1 );
	}
	return NULL;
}

void parencodepict( pict_data_s *picture )
{
	semaphore_wait( &worker_available );
	picture_to_encode = picture;
	semaphore_signal( &picture_available, 1 );
	semaphore_wait( &picture_started );
}



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
	pthread_t worker_threads[MAX_WORKER_THREADS];
	pict_data_s *cur_picture, *old_picture;
	pict_data_s *new_ref_picture, *old_ref_picture;

	init_pictures( ref_pictures, b_pictures );
	create_threads( worker_threads, max_encoding_frames, parencodeworker );

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
	ss.seq_split_length = ((int64_t)seq_length_limit)*(8*1024*1024);
	ss.next_split_point = BITCOUNT_OFFSET + ss.seq_split_length;
	fprintf( stderr, "DEBUG: split len = %lld\n", ss.seq_split_length );

	frame_num = 0;              /* Encoding number */

	gop_start(&ss);

	/* loop through all frames in encoding/decoding order */
	while( frame_num<nframes )
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
		sync_guard_update( &cur_picture->ipcompletion, 0 );

		if( readframe(cur_picture->temp_ref+ss.gop_start_frame,cur_picture->curorg) )
		{
			fprintf( stderr, "Corrupt frame data aborting!\n" );
			exit(1);
		}


        if (fieldpic)
		{
			set_pic_params( opt_topfirst ? TOP_FIELD : BOTTOM_FIELD,
							ss.i, ss.b,   0, cur_picture );
			parencodepict( cur_picture );

			set_pic_params( opt_topfirst ? BOTTOM_FIELD : TOP_FIELD,
							ss.i, ss.b,  1, cur_picture );
			parencodepict( cur_picture );
		}
		else
		{
			set_pic_params( FRAME_PICTURE,
							ss.i, ss.b,   0, cur_picture );
			parencodepict( cur_picture );
		}

#ifdef DEBUG
		writeframe(cur_picture->temp_ref+ss.gop_start_frame,cur_picture->curref);
#endif

		next_seq_state( &ss );
		++frame_num;
	}
	
	/* Wait for final frame's encoding to complete */
	sync_guard_test( &cur_picture->completion );
	putseqend();
}
