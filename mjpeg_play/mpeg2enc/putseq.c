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
	if (strlen(id_string) > 1)
		putuserdata(id_string);

}


static void frame_mc_and_pic_params( int decode,
									 int b_index,
									 motion_comp_s *mc_data, 
									 pict_data_s *picture )
{

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
		mc_data->sxf = motion_data[0].sxf;
		mc_data->syf = motion_data[0].syf;
		break;
	case B_TYPE :
		picture->forw_hor_f_code = motion_data[b_index].forw_hor_f_code;
		picture->forw_vert_f_code = motion_data[b_index].forw_vert_f_code;
		picture->back_hor_f_code = motion_data[b_index].back_hor_f_code;
		picture->back_vert_f_code = motion_data[b_index].back_vert_f_code;
		mc_data->sxf = motion_data[b_index].sxf;
		mc_data->syf = motion_data[b_index].syf;
		mc_data->sxb = motion_data[b_index].sxb;
		mc_data->syb = motion_data[b_index].syb;

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
 * certain threshold distance over that of its predecessor.
 * 
 * A slight trick is that search commences at the end of the legal range
 * backwards.  The reason is that longer GOP's are more efficient.  Thus if
 * two scence changes come close together it is better to have a change in
 * the middle of a long GOP (with plenty of bits to play with) rather than
 * a short GOP followed by another potentially very short GOP.
 *
 * N.b. This is experimental and could be highly bogus
 *
 ************************************/

#ifdef NEW_CODE
#define SCENE_CHANGE_THRESHOLD 4

static int gop_end_frame( int gop_start_frame, int gop_min_len, int gop_max_len )
{
	int i;
	int prev_lum_mean = frame_lum_mean( gop_start_frame+gop_max_len-1 );
	int cur_lum_mean;
	for( i = gop_start_frame+gop_max_len-2; i >= gop_min_len; --i )
	{
		cur_lum_mean =  frame_lum_mean( i );
		if( abs(cur_lum_mean-prev_lum_mean ) > SCENE_CHANGE_THRESHOLD )
			break;
	}

	if( i < gop_min_len )
		i = gop_max_len;
	return i;
}
#endif
void putseq()
{
	/* this routine assumes (N % M) == 0 */
	int i, j, f, g,  b, np, nb;
	int ipflag;
	/*int sxf = 0, sxb = 0, syf = 0, syb = 0;*/
	motion_comp_s mc_data;
	unsigned char **curorg, **curref;
	int gop_start_frame;
	int seq_start_frame;
	int temp_ref;
	int gop_length;
	int bs_short;
	int bigrp_length;
	int b_drop;
	pict_data_s cur_picture;

	/* Length limit parameter is specied in MBytes */
	int64_t seq_split_length = ((int64_t)seq_length_limit)*(8*1024*1024);
	int64_t next_split_point = BITCOUNT_OFFSET + seq_split_length;

	fprintf( stderr, "DEBUG: split len = %lld\n", seq_split_length );
	
	/* Allocate buffers for picture transformation */
	cur_picture.qblocks =
		(int16_t (*)[64])bufalloc(mb_per_pict*block_count*sizeof(int16_t [64]));
	cur_picture.mbinfo = 
		(struct mbinfo *)bufalloc(mb_per_pict*sizeof(struct mbinfo));

	cur_picture.blocks =
		(int16_t (*)[64])bufalloc(mb_per_pict*block_count*sizeof(int16_t [64]));

	init_seq(0);
	
	i = 0;		                /* Index in current MPEG sequence */
	frame_num = 0;              /* Encoding number */
	g = 0;						/* Index in current GOP */
	b = 0;						/* B frames since last I/P */
	gop_length = 0;				/* Length of current GOP init 0
								   0 force new GOP at start 1st sequence */
	seq_start_frame = 0;		/* Index start current sequence in
								 input stream */
	gop_start_frame = 0;		/* Index start current gop in input stream */

	/* loop through all frames in encoding/decoding order */
	while( frame_num<nframes )
	{
		/* Are we starting a new GOP? */
		if( g == gop_length )
		{
			/* If	we're starting a GOP and have gone past the current
			   sequence splitting point split the sequence and
			   set the next splitting point.
			*/
			
			g = 0;
			if( next_split_point != 0LL && 	bitcount() > next_split_point )
			{
				printf( "\nSplitting sequence\n" );
				next_split_point += seq_split_length;
				putseqend();
				init_seq(1);
				/* This is the input stream display order sequence number of
				   the frame that will become frame 0 in display
				   order in  the new sequence */
				seq_start_frame += i;
				i = 0;
			}
			
			gop_start_frame = seq_start_frame + i;

			/*
			  First GOP in a sequence contains N-(M-1) frames,
			  all other GOPs contain N frames
			*/
			if( i == 0 )
				gop_length = N-(M-1);
			else
				gop_length = N;

			/* last GOP may contain less frames */
			if (gop_length > nframes-gop_start_frame)
				gop_length = nframes-gop_start_frame;

			/* First figure out how many B frames we're short from
			   being able to achieve an even M-1 B's per I/P frame.

			   To avoid peaks in necessary data-rate we try to
			   lose the B's in the middle of the GOP. We always
			   *start* with M-1 B's.
			*/
			if( M-1 > 0 )
				bs_short = (M - (gop_length % M))%M;
			else
				bs_short = 0;

			/* We aim to spread the dropped B's evenly across the GOP */

			if( bs_short )
				b_drop = (M - bs_short) * gop_length / M;

			bigrp_length = (M-1);
			b = bigrp_length;

			/* number of P frames */
			if (i == 0)
				np = (gop_length + 2*(M-1))/M - 1; /* first GOP */
			else
				np = (gop_length + (M-1))/M - 1;
			
			/* number of B frames */
			nb = gop_length - np - 1;

			rc_init_GOP(np,nb);
			
			/* set closed_GOP in first GOP only 
			   No need for per-GOP seqhdr in first GOP as one
			   has already been created.
			*/
			
			putgophdr(i == 0 ? i : i+(M-1),
					  i == 0, 
					  i != 0 && seq_header_every_gop);

			fprintf( stderr, "GOP len = %d GOP start =%d\n", 
					 gop_length, gop_start_frame);
		}

		/* Each bigroup starts once all the B frames of its predecessor
		   has finished.
		*/
		if ( b == bigrp_length)
		{

			/* The first GOP of a sequence is closed with a 0 length
			   initial bigroup... */
			if( i == 0 )
				b = bigrp_length;
			else
				b = 0;
			
			
			/* I or P frame: Somewhat complicated buffer handling.
			   The original reference frame data is actually held in
			   the frame input buffers.  In input read-ahead buffer
			   management code worries about rotating them for use.
			   So to make the new old one the current new one we
			   simply move the pointers.  However for the
			   reconstructed "ref" data we are managing our a seperate
			   pair of buffers. We need to swap these to avoid losing
			   one!  */

			for (j=0; j<3; j++)
			{
				unsigned char *tmp;
				oldorgframe[j] = neworgframe[j];
				tmp = oldrefframe[j];
				oldrefframe[j] = newrefframe[j];
				newrefframe[j] = tmp;
			}

			/* For an I or P frame the "current frame" is simply an alias
			   for the new new reference frame. Saves the need to copy
			   stuff around once the frame has been processed.
			*/

			curorg = neworgframe;
			curref = newrefframe;

			bigrp_length = M-1;
			if( bs_short != 0 && g >= b_drop )
			{
				--bs_short;
				bigrp_length = M - 2;
				if( bs_short )
					b_drop = (M - bs_short) * gop_length / M;
			}

			/* f: frame number in sequence display order */
			temp_ref = (i == 0 ) ? i : g+(bigrp_length);
			if (temp_ref >= (nframes-gop_start_frame))
				temp_ref = (nframes-gop_start_frame) - 1;

			if (g==0) /* first displayed frame in GOP is I */
			{

				cur_picture.pict_type = I_TYPE;
			}
			else 
			{
				cur_picture.pict_type = P_TYPE;
			}
		}
		else
		{
			/* B frame: no need to change the reference frames.
			   The current frame data pointers are a 3rd set
			   seperate from the reference data pointers.
			*/
			b++;

			curorg = auxorgframe;
			curref = auxframe;

			/* f: frame number in sequence display order */
			temp_ref = g - 1;
			cur_picture.pict_type = B_TYPE;
		}

		frame_mc_and_pic_params( i, b,  &mc_data, &cur_picture );
		printf( "(%d %d %d) ", i-g+temp_ref, temp_ref+gop_start_frame, temp_ref );
		if( readframe(temp_ref+gop_start_frame,curorg) )
		{
			fprintf( stderr, "Corrupt frame data aborting!\n" );
			exit(1);
		}

		mc_data.oldorg = oldorgframe;
		mc_data.neworg = neworgframe;
		mc_data.oldref = oldrefframe;
		mc_data.newref = newrefframe;
		mc_data.cur    = curorg;
		mc_data.curref = curref;

        if (fieldpic)
		{
			cur_picture.topfirst = opt_topfirst;
			if (!quiet)
			{
				fprintf(stderr,"\nfirst field  (%s) ",
						cur_picture.topfirst ? "top" : "bot");
				fflush(stderr);
			}

			cur_picture.pict_struct = cur_picture.topfirst ? TOP_FIELD : BOTTOM_FIELD;
			/* A.Stevens 2000: Append fast motion compensation data for new frame */
			fast_motion_data(curorg[0], cur_picture.pict_struct);
			motion_estimation(&cur_picture, &mc_data,0,0);

			predict(&cur_picture,oldrefframe,newrefframe,predframe,0);
			dct_type_estimation(&cur_picture,predframe[0],curorg[0]);
			transform(&cur_picture,predframe,curorg);

			putpict(&cur_picture);		/* Quantisation: blocks -> qblocks */
#ifndef OUTPUT_STAT
			if( cur_picture.pict_type!=B_TYPE)
			{
#endif
				iquantize( &cur_picture );
				itransform(&cur_picture,predframe,curref);
				/* No use of FM data in ref frames...
				fast_motion_data(curref[0], cur_picture.pict_struct);
				*/
				calcSNR(curorg,curref);
				stats();
#ifndef OUTPUT_STAT
			}
#endif
			if (!quiet)
			{
				fprintf(stderr,"second field (%s) ",cur_picture.topfirst ? "bot" : "top");
				fflush(stderr);
			}

			cur_picture.pict_struct = cur_picture.topfirst ? BOTTOM_FIELD : TOP_FIELD;

			ipflag = (cur_picture.pict_type==I_TYPE);
			if (ipflag)
			{
				/* first field = I, second field = P */
				cur_picture.pict_type = P_TYPE;
				cur_picture.forw_hor_f_code = motion_data[0].forw_hor_f_code;
				cur_picture.forw_vert_f_code = motion_data[0].forw_vert_f_code;
				cur_picture.back_hor_f_code = 
					cur_picture.back_vert_f_code = 15;
				mc_data.sxf = motion_data[0].sxf;
				mc_data.syf = motion_data[0].syf;
			}

			motion_estimation(&cur_picture, &mc_data ,1,ipflag);

			predict(&cur_picture,oldrefframe,newrefframe,predframe,1);
			dct_type_estimation(&cur_picture,predframe[0],curorg[0]);
			transform(&cur_picture,predframe,curorg);

			putpict(&cur_picture);	/* Quantisation: blocks -> qblocks */

#ifndef OUTPUT_STAT
			if( cur_picture.pict_type!=B_TYPE)
			{
#endif			iquantize( &cur_picture );
				itransform(&cur_picture,predframe,curref);
				/* No use of FM data in ref frames
				fast_motion_data(curref[0], cur_picture.pict_struct);
				*/
				calcSNR(curorg,curref);
				stats();
#ifndef OUTPUT_STAT
			}
#endif
				
		}
		else
		{
			cur_picture.pict_struct = FRAME_PICTURE;
			fast_motion_data(curorg[0], cur_picture.pict_struct);

			/* do motion_estimation
			 *
			 * uses source frames (...orgframe) for full pel search
			 * and reconstructed frames (...refframe) for half pel search
			 */

			motion_estimation(&cur_picture,&mc_data,0,0);

			predict(&cur_picture, oldrefframe,newrefframe,predframe,0);
			dct_type_estimation(&cur_picture,predframe[0],curorg[0]);

			transform(&cur_picture,predframe,curorg);

			/* Side-effect: quantisation blocks -> qblocks */
			putpict(&cur_picture);	

#ifndef OUTPUT_STAT
			if( cur_picture.pict_type!=B_TYPE)
			{
#endif
				iquantize( &cur_picture );
				itransform(&cur_picture,predframe,curref);
				/* All FM is now based on org frame...
				fast_motion_data(curref[0], cur_picture.pict_struct); 
				*/
				calcSNR(curorg,curref);

				stats();
#ifndef OUTPUT_STAT
			}
#endif
		}
		writeframe(f+seq_start_frame,curref);
		++i;
		++frame_num;
		++g;
	}
	putseqend();
}
