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

#include <stdio.h>
#include <string.h>
#include "config.h"
#include "global.h"




void putseq()
{
	/* this routine assumes (N % M) == 0 */
	int i, j, k, f, f0, n, np, nb;
	int ipflag;
	int sxf = 0, sxb = 0, syf = 0, syb = 0;
	motion_comp_s mc_data;
	unsigned char **curorg, **curref;

	rc_init_seq(); /* initialize rate control */

  /* sequence header, sequence extension and sequence display extension */
	putseqhdr();
	if (!mpeg1)
	{
		putseqext();
		putseqdispext();
	}

	/* optionally output some text data (description, copyright or whatever) */
	if (strlen(id_string) > 1)
		putuserdata(id_string);

  /* A.Stevens Jul 2000 - we initialise sliding motion 
	 compensation window thresholding values - we need to
	 know how many macroblocks per frame to choose sensible
	 parameters. */

	reset_thresholds( mb_height2*mb_width );

	/* loop through all frames in encoding/decoding order */
	for (i=0; i<nframes; i++)
	{
		frame_num = i;
		/* f0: lowest frame number in current GOP
		 *
		 * first GOP contains N-(M-1) frames,
		 * all other GOPs contain N frames
		 */
		f0 = N*((i+(M-1))/N) - (M-1);

		if (f0<0)
			f0=0;

		if (i==0 || (i-1)%M==0)
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
			f = (i==0) ? 0 : i+M-1;
			if (f>=nframes)
				f = nframes - 1;

			if (i==f0) /* first displayed frame in GOP is I */
			{
				/* I frame */
				cur_picture.pict_type = I_TYPE;

				cur_picture.forw_hor_f_code = 
					cur_picture.forw_vert_f_code = 15;
				cur_picture.back_hor_f_code = 
					cur_picture.back_vert_f_code = 15;

				/* n: number of frames in current GOP
				 *
				 * first GOP contains (M-1) less (B) frames
				 */
				n = (i==0) ? N-(M-1) : N;

				/* last GOP may contain less frames */
				if (n > nframes-f0)
					n = nframes-f0;

				/* number of P frames */
				if (i==0)
					np = (n + 2*(M-1))/M - 1; /* first GOP */
				else
					np = (n + (M-1))/M - 1;

				/* number of B frames */
				nb = n - np - 1;

				rc_init_GOP(np,nb);

				putgophdr(f0,i==0); /* set closed_GOP in first GOP only */


			}
			else
			{
				/* P frame */
				cur_picture.pict_type = P_TYPE;
				cur_picture.forw_hor_f_code = motion_data[0].forw_hor_f_code;
				cur_picture.forw_vert_f_code = motion_data[0].forw_vert_f_code;
				cur_picture.back_hor_f_code = 
					cur_picture.back_vert_f_code = 15;
				sxf = motion_data[0].sxf;
				syf = motion_data[0].syf;
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
			cur_picture.forw_hor_f_code = motion_data[n].forw_hor_f_code;
			cur_picture.forw_vert_f_code = motion_data[n].forw_vert_f_code;
			cur_picture.back_hor_f_code = motion_data[n].back_hor_f_code;
			cur_picture.back_vert_f_code = motion_data[n].back_vert_f_code;
			sxf = motion_data[n].sxf;
			syf = motion_data[n].syf;
			sxb = motion_data[n].sxb;
			syb = motion_data[n].syb;
		}

		cur_picture.temp_ref = f - f0;
		cur_picture.frame_pred_dct = frame_pred_dct_tab[cur_picture.pict_type-1];
		cur_picture.q_scale_type = qscale_tab[cur_picture.pict_type-1];
		cur_picture.intravlc = intravlc_tab[cur_picture.pict_type-1];
		cur_picture.altscan = altscan_tab[cur_picture.pict_type-1];

#ifdef OUTPUT_STAT
		fprintf(statfile,"\nFrame %d (#%d in display order):\n",i,f);
		fprintf(statfile," picture_type=%c\n",pict_type_char[cur_picture.pict_type]);
		fprintf(statfile," temporal_reference=%d\n",cur_picture.temp_ref);
		fprintf(statfile," frame_pred_frame_dct=%d\n",cur_picture.frame_pred_dct);
		fprintf(statfile," q_scale_type=%d\n",cur_picture.q_scale_type);
		fprintf(statfile," intra_vlc_format=%d\n",cur_picture.intravlc);
		fprintf(statfile," alternate_scan=%d\n",cur_picture.altscan);

		if (cur_picture.pict_type!=I_TYPE)
		{
			fprintf(statfile," forward search window: %d...%d / %d...%d\n",
					-sxf,sxf,-syf,syf);
			fprintf(statfile," forward vector range: %d...%d.5 / %d...%d.5\n",
					-(4<<cur_picture.forw_hor_f_code),(4<<cur_picture.forw_hor_f_code)-1,
					-(4<<cur_picture.forw_vert_f_code),(4<<cur_picture.forw_vert_f_code)-1);
		}

		if (cur_picture.pict_type==B_TYPE)
		{
			fprintf(statfile," backward search window: %d...%d / %d...%d\n",
					-sxb,sxb,-syb,syb);
			fprintf(statfile," backward vector range: %d...%d.5 / %d...%d.5\n",
					-(4<<cur_picture.back_hor_f_code),(4<<cur_picture.back_hor_f_code)-1,
					-(4<<cur_picture.back_vert_f_code),(4<<cur_picture.back_vert_f_code)-1);
		}
#endif
		if (!quiet)
		{
			printf("Frame %d %d %c ",i, cur_picture.pict_type,  pict_type_char[cur_picture.pict_type]);
			fflush(stdout);
		}


		if( readframe(f+frame0,curorg) )
		{
			fprintf( stderr, "Corrupt frame data aborting!\n" );
			exit(1);
		}

		mc_data.oldorg = oldorgframe[0];
		mc_data.neworg = neworgframe[0];
		mc_data.oldref = oldrefframe[0];
		mc_data.newref = newrefframe[0];
		mc_data.cur    = curorg[0];
		mc_data.curref = curref[0];
		mc_data.sxf = sxf;
		mc_data.syf = syf;
		mc_data.sxb = sxb;
		mc_data.syb = syb;

        if (fieldpic)
		{
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

			putpict(&cur_picture, qblocks);		/* Quantisation: blocks -> qblocks */

			for (k=0; k<mb_height2*mb_width; k++)
			{
				if (cur_picture.mbinfo[k].mb_type & MB_INTRA)
					for (j=0; j<block_count; j++)
						iquant_intra(qblocks[k*block_count+j],
									 qblocks[k*block_count+j],
									 cur_picture.dc_prec,
									 cur_picture.mbinfo[k].mquant);
				else
					for (j=0;j<block_count;j++)
						iquant_non_intra(qblocks[k*block_count+j],
										 qblocks[k*block_count+j],
										 cur_picture.mbinfo[k].mquant);
			}

			itransform(&cur_picture,predframe,curref,qblocks);
			fast_motion_data(curref[0], cur_picture.pict_struct);
			calcSNR(curorg,curref);
			stats();

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
				sxf = motion_data[0].sxf;
				syf = motion_data[0].syf;
			}

			motion_estimation(&cur_picture, &mc_data ,1,ipflag);

			predict(&cur_picture,oldrefframe,newrefframe,predframe,1);
			dct_type_estimation(&cur_picture,predframe[0],curorg[0]);
			transform(&cur_picture,predframe,curorg);

			putpict(&cur_picture, qblocks);	/* Quantisation: blocks -> qblocks */

			for (k=0; k<mb_height2*mb_width; k++)
			{
				if (cur_picture.mbinfo[k].mb_type & MB_INTRA)
					for (j=0; j<block_count; j++)
						iquant_intra(qblocks[k*block_count+j],
									 qblocks[k*block_count+j],
									 cur_picture.dc_prec,
									 cur_picture.mbinfo[k].mquant);
				else
					for (j=0;j<block_count;j++)
						iquant_non_intra(qblocks[k*block_count+j],
										 qblocks[k*block_count+j],
										 cur_picture.mbinfo[k].mquant);
			}

			itransform(&cur_picture,predframe,curref,qblocks);
			fast_motion_data(curref[0], cur_picture.pict_struct);
			calcSNR(curorg,curref);
			stats();
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
			putpict(&cur_picture,qblocks);	

			for (k=0; k<mb_height*mb_width; k++)
			{
				if (cur_picture.mbinfo[k].mb_type & MB_INTRA)
					for (j=0; j<block_count; j++)
						iquant_intra(qblocks[k*block_count+j],
									 qblocks[k*block_count+j],
									 cur_picture.dc_prec,
									 cur_picture.mbinfo[k].mquant);
				else
				{
					for (j=0;j<block_count;j++)
					{
						iquant_non_intra(qblocks[k*block_count+j],
										 qblocks[k*block_count+j],
										 cur_picture.mbinfo[k].mquant);
					}
				}
			}

			itransform(&cur_picture,predframe,curref,qblocks);
			fast_motion_data(curref[0], cur_picture.pict_struct); 
			calcSNR(curorg,curref);

			stats();
		}

		writeframe(f+frame0,curref);

	}

	putseqend();
}
