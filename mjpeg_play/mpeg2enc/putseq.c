/* putseq.c, sequence level routines                                        */

/* Copyright (C) 1996, MPEG Software Simulation Group. All Rights Reserved. */

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


/*************
   Start encoding a new sequence.
*************/

static void init_seq(int reinit)
{
	rc_init_seq(reinit); /* initialize rate control */

	/* If we're not doing sequence header, sequence extension and
	   sequence display extension every GOP at least has to be one at the
	   start of the sequence.
	*/
	
	putseqhdr();


	/* optionally output some text data (description, copyright or whatever) */
	/* if (strlen(id_string) > 1)
	   putuserdata(id_string);*/

}


static void set_pic_params( int pict_struct,
							int decode,
							int b_index,
							int secondfield,
							pict_data_s *picture )
{
	picture->pict_struct = pict_struct;
	picture->dc_prec = opt_dc_prec;
	picture->prog_frame = opt_prog_frame;
	picture->repeatfirst = 0;

	/* Handle topfirst and ipflag special-case for field pictures */
	if( pict_struct != FRAME_PICTURE )
	{
		picture->topfirst = (!!secondfield) ^ (pict_struct == TOP_FIELD);
		picture->ipflag = (picture->pict_type==I_TYPE) && secondfield;
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

		if (!quiet)
		{
			printf("Frame %d %c ",decode, pict_type_char[picture->pict_type]);
			fflush(stdout);
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

	/* TODO DEBUG remove....
	for( j= gop_start_frame; j<= gop_start_frame+gop_max_len+gop_min_len; ++j)
		printf( "%03d ", frame_lum_mean( j+I_frame_temp_ref ));
		printf( "\n");
	*/
	if( i != gop_max_len )
		printf( "DEBUG: GOP nonstandard size %d\n", i );

	/* last GOP may contain less frames */
	if (i > nframes-gop_start_frame)
		i = nframes-gop_start_frame;
	return i;

}


void encodepict(pict_data_s *picture)
{
	
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
	 double next_b_drop;		/* When next B frame drop is due in GOP */

	int64_t next_split_point;
	int64_t seq_split_length;
};

typedef struct _stream_state stream_state_s;

void oldputseq()
{
	stream_state_s ss;
	int  np, nb;
	/*uint8_t **curorg, **curref;*/
	int pict_type;
	int temp_ref;
	pict_data_s cur_picture;
	uint8_t **tmp;

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

	init_seq(0);
	
	ss.i = 0;		                /* Index in current MPEG sequence */
	ss.g = 0;						/* Index in current GOP */
	ss.b = 0;						/* B frames since last I/P */
	ss.gop_length = 0;				/* Length of current GOP init 0
								   0 force new GOP at start 1st sequence */
	ss.seq_start_frame = 0;		/* Index start current sequence in
								 input stream */
	ss.gop_start_frame = 0;		/* Index start current gop in input stream */
	/* Length limit parameter is specied in MBytes */
	ss.seq_split_length = ((int64_t)seq_length_limit)*(8*1024*1024);
	ss.next_split_point = BITCOUNT_OFFSET + ss.seq_split_length;
	fprintf( stderr, "DEBUG: split len = %lld\n", ss.seq_split_length );

	frame_num = 0;              /* Encoding number */

	/* loop through all frames in encoding/decoding order */
	while( frame_num<nframes )
	{
		/* Are we starting a new GOP? */
		if( ss.g == ss.gop_length )
		{
			/* If	we're starting a GOP and have gone past the current
			   sequence splitting point split the sequence and
			   set the next splitting point.
			*/
			
			ss.g = 0;
			if( ss.next_split_point != 0LL && 	bitcount() > ss.next_split_point )
			{
				printf( "\nSplitting sequence\n" );
				ss.next_split_point += ss.seq_split_length;
				putseqend();
				init_seq(1);
				/* This is the input stream display order sequence number of
				   the frame that will become frame 0 in display
				   order in  the new sequence */
				ss.seq_start_frame += ss.i;
				ss.i = 0;
			}
			
			ss.gop_start_frame = ss.seq_start_frame + ss.i;

			/*
			  Compute GOP length based on min and max sizes specified
			  and scene changes detected.  
			  First GOP in a sequence has I frame with a 0 temp_ref
			  nad (M-1) less frames (no initial B frames).
			  all other GOPs have a temp_ref of M-1
			*/

			if( ss.i == 0 )
				ss.gop_length =  find_gop_length( ss.gop_start_frame, 0, 
												  N_min-(M-1), N_max-(M-1));
			else
				ss.gop_length = 
					find_gop_length( ss.gop_start_frame, M-1, 
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
				ss.bs_short = (M - ((ss.gop_length-(ss.i==0)) % M))%M;
				ss.next_b_drop = ((double)ss.gop_length) / (double)(ss.bs_short+1)-1.0 ;
			}
			else
			{
				ss.bs_short = 0;
				ss.next_b_drop = 0.0;
			}

			/* We aim to spread the dropped B's evenly across the GOP */
			ss.bigrp_length = (M-1);
			ss.b = ss.bigrp_length;

			/* number of P frames */
			if (ss.i == 0)
				np = (ss.gop_length + 2*(M-1))/M - 1; /* first GOP */
			else
				np = (ss.gop_length + (M-1))/M - 1;
			
			/* number of B frames */
			nb = ss.gop_length - np - 1;

			rc_init_GOP(np,nb);
			
			/* set closed_GOP in first GOP only 
			   No need for per-GOP seqhdr in first GOP as one
			   has already been created.
			*/
			putgophdr(ss.i == 0 ? ss.i : ss.i+(M-1),
					  ss.i == 0, 
					  ss.i != 0 && seq_header_every_gop);

		}

		/* Each bigroup starts once all the B frames of its predecessor
		   has finished.
		*/
		if ( ss.b == ss.bigrp_length)
		{

			/* I or P frame: Somewhat complicated buffer handling.
			   The original reference frame data is actually held in
			   the frame input buffers.  In input read-ahead buffer
			   management code worries about rotating them for use.
			   So to make the new old one the current new one we
			   simply move the pointers.  However for the
			   reconstructed "ref" data we are managing our a seperate
			   pair of buffers. We need to swap these to avoid losing
			   one!  

			   The "current frame" is simply an alias
			   for the new new reference frame. Saves the need to copy
			   stuff around once the frame has been processed.
			*/

			/*
			cur_picture.oldorg = cur_picture.neworg;
			tmp = cur_picture.oldref;
			cur_picture.oldref = cur_picture.newref;
			cur_picture.newref = tmp;
			cur_picture.curorg = cur_picture.neworg;
			cur_picture.curref = cur_picture.newref;
			*/

			/*
			for (j=0; j<3; j++)
			{
				cur_picture.oldorg[j] = cur_picture.neworg[j];
			}
			*/
			tmp = cur_picture.oldorg;
			cur_picture.oldorg = cur_picture.neworg;
			cur_picture.neworg = cur_picture.oldorg;
			tmp = cur_picture.oldref;
			cur_picture.oldref = cur_picture.newref;
			cur_picture.newref = tmp;

			cur_picture.curorg = cur_picture.neworg;
			cur_picture.curref = cur_picture.newref;


			/* The first GOP of a sequence is closed with a 0 length
			   initial bigroup... */
			if( ss.i == 0 )
				ss.b = ss.bigrp_length;
			else
				ss.b = 0;
			

			ss.bigrp_length = M-1;
			if( ss.bs_short != 0 && ss.g > (int)ss.next_b_drop )
			{
				ss.bigrp_length = M - 2;
				if( ss.bs_short )
					ss.next_b_drop += ((double)ss.gop_length) / (double)(ss.bs_short+1) ;
			}

			/* f: frame number in sequence display order */
			temp_ref = (ss.i == 0 ) ? ss.i : ss.g+(ss.bigrp_length);
			if (temp_ref >= (nframes-ss.gop_start_frame))
				temp_ref = (nframes-ss.gop_start_frame) - 1;

			if (ss.g==0) /* first displayed frame in GOP is I */
			{

				pict_type = I_TYPE;
			}
			else 
			{
				pict_type = P_TYPE;
			}
		}
		else
		{
			/* B frame: no need to change the reference frames.
			   The current frame data pointers are a 3rd set
			   seperate from the reference data pointers.
			*/
			ss.b++;

			cur_picture.curorg = auxorgframe;
			cur_picture.curref = auxframe;

			/* f: frame number in sequence display order */
			temp_ref = ss.g - 1;
			pict_type = B_TYPE;
		}


		printf( "(%d %d %d) ",
				ss.i-ss.g+temp_ref, temp_ref+ss.gop_start_frame, temp_ref );
		if( readframe(temp_ref+ss.gop_start_frame,cur_picture.curorg) )
		{
			fprintf( stderr, "Corrupt frame data aborting!\n" );
			exit(1);
		}

		cur_picture.pict_type = pict_type;
		cur_picture.temp_ref = temp_ref;

        if (fieldpic)
		{
			set_pic_params( opt_topfirst ? TOP_FIELD : BOTTOM_FIELD,
							ss.i, ss.b,  0, &cur_picture );
			encodepict( &cur_picture );

			set_pic_params( opt_topfirst ? BOTTOM_FIELD : TOP_FIELD,
							ss.i, ss.b,   1, &cur_picture );
			encodepict( &cur_picture );
		}
		else
		{
			set_pic_params( FRAME_PICTURE,
							ss.i, ss.b,   0, &cur_picture );
			encodepict( &cur_picture );
		}

#ifdef DEBUG
		writeframe(temp_ref+ss.gop_start_frame,cur_picture.curref);
#endif
		++ss.i;
		++frame_num;
		++ss.g;
	}
	putseqend();
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

	if( ss->next_split_point != 0LL && 	bitcount() > ss->next_split_point )
	{
		printf( "\nSplitting sequence\n" );
		ss->next_split_point += ss->seq_split_length;
		putseqend();
		init_seq(1);
		/* This is the input stream display order sequence number of
		   the frame that will become frame 0 in display
		   order in  the new sequence */
		ss->seq_start_frame += ss->i;
		ss->i = 0;
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
	
	rc_init_GOP(np,nb);
	
	/* set closed_GOP in first GOP only 
	   No need for per-GOP seqhdr in first GOP as one
	   has already been created.
	*/
	putgophdr(ss->i == 0 ? ss->i : ss->i+(M-1),
			  ss->i == 0, 
			  ss->i != 0 && seq_header_every_gop);



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
}


void B_frame_struct(  stream_state_s *ss,
					  pict_data_s *picture )
{
	picture->temp_ref = ss->g - 1;
	picture->pict_type = B_TYPE;
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

void putseq()
{
	stream_state_s ss;
	int f;
	pict_data_s cur_picture;
	/* Pointer blocks pointing to original frame data in frame data buffers
	 */
	uint8_t *neworgframe[3], *oldorgframe[3], *auxorgframe[3];
	

	fprintf( stderr, "DEBUG: split len = %lld\n", ss.seq_split_length );
	
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

	init_seq(0);
	
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


		printf( "(%d %d %d) ",
				ss.i-ss.g+cur_picture.temp_ref, 
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
		writeframe(f+ss.seq_start_frame,cur_picture.curref);
#endif

		next_seq_state( &ss );
		++frame_num;
	}
	putseqend();
}
