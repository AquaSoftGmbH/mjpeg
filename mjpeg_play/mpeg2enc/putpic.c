/* putpic.c, block and motion vector encoding routines                      */

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
#include "config.h"
#include "global.h"




/* output motion vectors (6.2.5.2, 6.3.16.2)
 *
 * this routine also updates the predictions for motion vectors (PMV)
 */
static void putmvs(
	pict_data_s *picture,
	mbinfo_s *mb,
	int PMV[2][2][2],
	int back
	)

{
	int hor_f_code;
	int vert_f_code;

	if( back )
	{
		hor_f_code = picture->back_hor_f_code;
		vert_f_code = picture->back_vert_f_code;
	}
	else
	{
		hor_f_code = picture->forw_hor_f_code;
		vert_f_code = picture->forw_vert_f_code;
	}

	if (picture->pict_struct==FRAME_PICTURE)
	{
		if (mb->motion_type==MC_FRAME)
		{
			/* frame prediction */
			putmv(mb->MV[0][back][0]-PMV[0][back][0],hor_f_code);
			putmv(mb->MV[0][back][1]-PMV[0][back][1],vert_f_code);
			PMV[0][back][0]=PMV[1][back][0]=mb->MV[0][back][0];
			PMV[0][back][1]=PMV[1][back][1]=mb->MV[0][back][1];
		}
		else if (mb->motion_type==MC_FIELD)
		{
			/* field prediction */

			putbits(mb->mv_field_sel[0][back],1);
			putmv(mb->MV[0][back][0]-PMV[0][back][0],hor_f_code);
			putmv((mb->MV[0][back][1]>>1)-(PMV[0][back][1]>>1),vert_f_code);
			putbits(mb->mv_field_sel[1][back],1);
			putmv(mb->MV[1][back][0]-PMV[1][back][0],hor_f_code);
			putmv((mb->MV[1][back][1]>>1)-(PMV[1][back][1]>>1),vert_f_code);
			PMV[0][back][0]=mb->MV[0][back][0];
			PMV[0][back][1]=mb->MV[0][back][1];
			PMV[1][back][0]=mb->MV[1][back][0];
			PMV[1][back][1]=mb->MV[1][back][1];

		}
		else
		{
			/* dual prime prediction */
			putmv(mb->MV[0][back][0]-PMV[0][back][0],hor_f_code);
			putdmv(mb->dmvector[0]);
			putmv((mb->MV[0][back][1]>>1)-(PMV[0][back][1]>>1),vert_f_code);
			putdmv(mb->dmvector[1]);
			PMV[0][back][0]=PMV[1][back][0]=mb->MV[0][back][0];
			PMV[0][back][1]=PMV[1][back][1]=mb->MV[0][back][1];
		}
	}
	else
	{
		/* field picture */
		if (mb->motion_type==MC_FIELD)
		{
			/* field prediction */
			putbits(mb->mv_field_sel[0][back],1);
			putmv(mb->MV[0][back][0]-PMV[0][back][0],hor_f_code);
			putmv(mb->MV[0][back][1]-PMV[0][back][1],vert_f_code);
			PMV[0][back][0]=PMV[1][back][0]=mb->MV[0][back][0];
			PMV[0][back][1]=PMV[1][back][1]=mb->MV[0][back][1];
		}
		else if (mb->motion_type==MC_16X8)
		{
			/* 16x8 prediction */
			putbits(mb->mv_field_sel[0][back],1);
			putmv(mb->MV[0][back][0]-PMV[0][back][0],hor_f_code);
			putmv(mb->MV[0][back][1]-PMV[0][back][1],vert_f_code);
			putbits(mb->mv_field_sel[1][back],1);
			putmv(mb->MV[1][back][0]-PMV[1][back][0],hor_f_code);
			putmv(mb->MV[1][back][1]-PMV[1][back][1],vert_f_code);
			PMV[0][back][0]=mb->MV[0][back][0];
			PMV[0][back][1]=mb->MV[0][back][1];
			PMV[1][back][0]=mb->MV[1][back][0];
			PMV[1][back][1]=mb->MV[1][back][1];
		}
		else
		{
			/* dual prime prediction */
			putmv(mb->MV[0][back][0]-PMV[0][back][0],hor_f_code);
			putdmv(mb->dmvector[0]);
			putmv(mb->MV[0][back][1]-PMV[0][back][1],vert_f_code);
			putdmv(mb->dmvector[1]);
			PMV[0][back][0]=PMV[1][back][0]=mb->MV[0][back][0];
			PMV[0][back][1]=PMV[1][back][1]=mb->MV[0][back][1];
		}
	}
}

/* *****************
 *
 * putpict - Quantisation and coding of picture.
 *
 * ARGS:
 * picture - The transformed picture data (DCT blocks and motion comp. data)
 * quant_blocks - Buffer for quantised DCT blocks for the picture.
 *
 * NOTE: It may seem perverse to quantise in the sample as coding. However,
 * actually makes (limited) sense: feedback from the *actual* bit-allocation
 * may be used to adjust quantisation "on the fly".  We, of course, need the
 * quantised DCT blocks to constructed the reference picture for future
 * motion compensation etc.
 *
 ******************/


void putpict(pict_data_s *picture )
{
	int i, j, k, comp, cc;
	int mb_type;
	int PMV[2][2][2];
	int prev_mquant;
	int cbp, MBAinc;
	mbinfo_s *cur_mb;
	int cur_mb_blocks;
	short (*quant_blocks)[64] = picture->qblocks;
	MBAinc = 0;          /* Annoying warning otherwise... */

	calc_vbv_delay(picture);
	rc_init_pict(picture); /* set up rate control */

	/* picture header and picture coding extension */
	putpicthdr(picture);

	if ( !mpeg1 )
		putpictcodext(picture);

	prev_mquant = rc_start_mb(picture); /* initialize quantization parameter */

	k = 0;

	for (j=0; j<mb_height2; j++)
	{
		/* macroblock row loop */

		for (i=0; i<mb_width; i++)
		{
			cur_mb = &picture->mbinfo[k];
			cur_mb_blocks = k*block_count;
			/* macroblock loop */
			if (i==0)
			{
				/* slice header (6.2.4) */
				alignbits();

				if (mpeg1 || vertical_size<=2800)
					putbits(SLICE_MIN_START+j,32); /* slice_start_code */
				else
				{
					putbits(SLICE_MIN_START+(j&127),32); /* slice_start_code */
					putbits(j>>7,3); /* slice_vertical_position_extension */
				}
  
				/* quantiser_scale_code */
				putbits(picture->q_scale_type ? 
						map_non_linear_mquant[prev_mquant]
						: prev_mquant >> 1, 5);
  
				putbits(0,1); /* extra_bit_slice */
  
				/* reset predictors */

				for (cc=0; cc<3; cc++)
					dc_dct_pred[cc] = 0;

				PMV[0][0][0]=PMV[0][0][1]=PMV[1][0][0]=PMV[1][0][1]=0;
				PMV[0][1][0]=PMV[0][1][1]=PMV[1][1][0]=PMV[1][1][1]=0;
  
				MBAinc = i + 1; /* first MBAinc denotes absolute position */

			}

			mb_type = cur_mb->mb_type;

			/* determine mquant (rate control) */
			cur_mb->mquant = rc_calc_mquant(picture,k);

			/* quantize macroblock */
			if (mb_type & MB_INTRA)
			{
				quant_intra( picture,
					         picture->blocks[cur_mb_blocks],
							 quant_blocks[cur_mb_blocks],
							 cur_mb->mquant, 
							 &cur_mb->mquant );
		
				cur_mb->cbp = cbp = (1<<block_count) - 1;
			}
			else
			{
				cbp = (*pquant_non_intra)(picture,
									  picture->blocks[cur_mb_blocks],
									  quant_blocks[cur_mb_blocks],
									  cur_mb->mquant,
									  &cur_mb->mquant );
				cur_mb->cbp = cbp;
				if (cbp)
					mb_type|= MB_PATTERN;
			}

			/* output mquant if it has changed */
			if (cbp && prev_mquant!=cur_mb->mquant)
				mb_type|= MB_QUANT;

			/* check if macroblock can be skipped */
			if (i!=0 && i!=mb_width-1 && !cbp)
			{
				/* no DCT coefficients and neither first nor last macroblock of slice */

				if (picture->pict_type==P_TYPE && !(mb_type&MB_FORWARD))
				{
					/* P picture, no motion vectors -> skip */

					/* reset predictors */

					for (cc=0; cc<3; cc++)
						dc_dct_pred[cc] = 0;

					PMV[0][0][0]=PMV[0][0][1]=PMV[1][0][0]=PMV[1][0][1]=0;
					PMV[0][1][0]=PMV[0][1][1]=PMV[1][1][0]=PMV[1][1][1]=0;

					cur_mb->mb_type = mb_type;
					cur_mb->skipped = 1;
					MBAinc++;
					k++;
					continue;
				}

				if (picture->pict_type==B_TYPE &&
					picture->pict_struct==FRAME_PICTURE
					&& cur_mb->motion_type==MC_FRAME
					&& ((picture->mbinfo[k-1].mb_type^mb_type)&(MB_FORWARD|MB_BACKWARD))==0
					&& (!(mb_type&MB_FORWARD) ||
						(PMV[0][0][0]==cur_mb->MV[0][0][0] &&
						 PMV[0][0][1]==cur_mb->MV[0][0][1]))
					&& (!(mb_type&MB_BACKWARD) ||
						(PMV[0][1][0]==cur_mb->MV[0][1][0] &&
						 PMV[0][1][1]==cur_mb->MV[0][1][1])))
				{
					/* conditions for skipping in B frame pictures:
					 * - must be frame predicted
					 * - must be the same prediction type (forward/backward/interp.)
					 *   as previous macroblock
					 * - relevant vectors (forward/backward/both) have to be the same
					 *   as in previous macroblock
					 */

					cur_mb->mb_type = mb_type;
					cur_mb->skipped = 1;
					MBAinc++;
					k++;
					continue;
				}

				if (picture->pict_type==B_TYPE 
					&& picture->pict_struct!=FRAME_PICTURE
					&& cur_mb->motion_type==MC_FIELD
					&& ((picture->mbinfo[k-1].mb_type^mb_type)&(MB_FORWARD|MB_BACKWARD))==0
					&& (!(mb_type&MB_FORWARD) ||
						(PMV[0][0][0]==cur_mb->MV[0][0][0] &&
						 PMV[0][0][1]==cur_mb->MV[0][0][1] &&
						 cur_mb->mv_field_sel[0][0]==(picture->pict_struct==BOTTOM_FIELD)))
					&& (!(mb_type&MB_BACKWARD) ||
						(PMV[0][1][0]==cur_mb->MV[0][1][0] &&
						 PMV[0][1][1]==cur_mb->MV[0][1][1] &&
						 cur_mb->mv_field_sel[0][1]==(picture->pict_struct==BOTTOM_FIELD))))
				{
					/* conditions for skipping in B field pictures:
					 * - must be field predicted
					 * - must be the same prediction type (forward/backward/interp.)
					 *   as previous macroblock
					 * - relevant vectors (forward/backward/both) have to be the same
					 *   as in previous macroblock
					 * - relevant motion_vertical_field_selects have to be of same
					 *   parity as current field
					 */

					cur_mb->mb_type = mb_type;
					cur_mb->skipped = 1;
					MBAinc++;
					k++;
					continue;
				}
			}

			/* macroblock cannot be skipped */
			cur_mb->skipped = 0;

			/* there's no VLC for 'No MC, Not Coded':
			 * we have to transmit (0,0) motion vectors
			 */
			if (picture->pict_type==P_TYPE && !cbp && !(mb_type&MB_FORWARD))
				mb_type|= MB_FORWARD;

			putaddrinc(MBAinc); /* macroblock_address_increment */
			MBAinc = 1;

			putmbtype(picture->pict_type,mb_type); /* macroblock type */

			if (mb_type & (MB_FORWARD|MB_BACKWARD) && !picture->frame_pred_dct)
				putbits(cur_mb->motion_type,2);

			if (picture->pict_struct==FRAME_PICTURE 
				&& cbp && !picture->frame_pred_dct)
				putbits(cur_mb->dct_type,1);

			if (mb_type & MB_QUANT)
			{
				putbits(picture->q_scale_type ? 
						map_non_linear_mquant[cur_mb->mquant]
						: cur_mb->mquant>>1,5);
				prev_mquant = cur_mb->mquant;
			}


			if (mb_type & MB_FORWARD)
			{
				/* forward motion vectors, update predictors */
				putmvs(picture, cur_mb, PMV,  0 );
			}

			if (mb_type & MB_BACKWARD)
			{
				/* backward motion vectors, update predictors */
				putmvs(picture,  cur_mb, PMV, 1 );
			}

			if (mb_type & MB_PATTERN)
			{
				putcbp((cbp >> (block_count-6)) & 63);
				if (chroma_format!=CHROMA420)
					putbits(cbp,block_count-6);
			}

			for (comp=0; comp<block_count; comp++)
			{

				/* block loop */
				if (cbp & (1<<(block_count-1-comp)))
				{

					if (mb_type & MB_INTRA)
					{
						cc = (comp<4) ? 0 : (comp&1)+1;
						putintrablk(picture,quant_blocks[cur_mb_blocks+comp],cc);
					}
					else
					{
						putnonintrablk(picture,quant_blocks[cur_mb_blocks+comp]);
					}
				}
			}

			/* reset predictors */
			if (!(mb_type & MB_INTRA))
				for (cc=0; cc<3; cc++)
					dc_dct_pred[cc] = 0;

			if (mb_type & MB_INTRA || 
				(picture->pict_type==P_TYPE && !(mb_type & MB_FORWARD)))
			{
				PMV[0][0][0]=PMV[0][0][1]=PMV[1][0][0]=PMV[1][0][1]=0;
				PMV[0][1][0]=PMV[0][1][1]=PMV[1][1][0]=PMV[1][1][1]=0;
			}

			cur_mb->mb_type = mb_type;
			k++;
		}
	}

	rc_update_pict(picture);
	vbv_end_of_picture(picture);
}
