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
									 int display,
									 motion_comp_s *mc_data, 
									 pict_data_s *picture )
{
	int n;

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
		n = (decode-2)%M + 1; /* first B: n=1, second B: n=2, ... */
		picture->forw_hor_f_code = motion_data[n].forw_hor_f_code;
		picture->forw_vert_f_code = motion_data[n].forw_vert_f_code;
		picture->back_hor_f_code = motion_data[n].back_hor_f_code;
		picture->back_vert_f_code = motion_data[n].back_vert_f_code;
		mc_data->sxf = motion_data[n].sxf;
		mc_data->syf = motion_data[n].syf;
		mc_data->sxb = motion_data[n].sxb;
		mc_data->syb = motion_data[n].syb;

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
							  
void putseq()
{
	/* this routine assumes (N % M) == 0 */
	int i, j, f, f0, n, np, nb;
	int ipflag;
	/*int sxf = 0, sxb = 0, syf = 0, syb = 0;*/
	motion_comp_s mc_data;
	unsigned char **curorg, **curref;
	int sequence_start = 1;
	int seq_start_frame = 0;
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
	/* loop through all frames in encoding/decoding order */
	while( frame_num<nframes )
	{

		/* Are we starting a new sequence? If so initialise
		   per-sequence quantities and generate headers..
		*/
			
		/* f0: lowest frame number in current GOP
		 *
		 * first GOP contains N-(M-1) frames,
		 * all other GOPs contain N frames
		 */

		 f0 = N*((i+(M-1))/N) - (M-1);
		 if (f0<0)
			 f0=0;

		 /* If we're starting a GOP and have gone past the current
			sequence splitting point split the sequence and
			set the next splitting point */
		 if( i == f0 && next_split_point != 0LL && 
			 bitcount() > next_split_point )
		 {
			 printf( "\nSplitting sequence\n" );
			 next_split_point += seq_split_length;
			 putseqend();
			 init_seq(1);
			 /* This is the input stream display order sequence number of
				the frame that will become frame 0 in display
				order in  the new sequence */
			 seq_start_frame += i;
			 f0 = i = 0;
			 sequence_start = 1;
		 }

		if ( sequence_start || (i-1)%M==0)
		{

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


			/* f: frame number in display order */
			f = sequence_start ? i : i+M-1;
			if (f >= (nframes-seq_start_frame))
				f = (nframes-seq_start_frame) - 1;

			if (i==f0) /* first displayed frame in GOP is I */
			{

				cur_picture.pict_type = I_TYPE;
				
				/* 
				   First GOP in sequence is closed and thus contains
				   M-1 less B frames..
				*/
				if( sequence_start )
				{
					n = N-(M-1);
				}
				else
					n = N;

				/* last GOP may contain less frames */
				if (n > nframes-(f0+seq_start_frame))
					n = nframes-(f0+seq_start_frame);

				/* number of P frames */
				if (sequence_start)
					np = (n + 2*(M-1))/M - 1; /* first GOP */
				else
					np = (n + (M-1))/M - 1;

				/* number of B frames */
				nb = n - np - 1;

				rc_init_GOP(np,nb);
				
				/* set closed_GOP in first GOP only 
				   No need for per-GOP seqhdr in first GOP as one
				   has already been created.
				 */
				putgophdr(f0,
						  sequence_start, 
						  !sequence_start && seq_header_every_gop);


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
			curorg = auxorgframe;
			curref = auxframe;

			/* f: frame number in display order */
			f = i - 1;
			cur_picture.pict_type = B_TYPE;
			n = (i-2)%M + 1; /* first B: n=1, second B: n=2, ... */
		}

		cur_picture.temp_ref = f - f0;
		frame_mc_and_pic_params( i, f,  &mc_data, &cur_picture );
		printf( "(%d) ", f+seq_start_frame );
		if( readframe(f+seq_start_frame,curorg) )
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
		sequence_start = 0;
		++i;
		++frame_num;
	}
	putseqend();
}
