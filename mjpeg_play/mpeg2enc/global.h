/* global.h, global variables, function prototypes                          */

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
/* Modifications and enhancements (C) 2000,2001,2002,2003 Andrew Stevens */

/* These modifications are free software; you can redistribute it
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

#include "stdio.h"
#include "syntaxparams.h"
#include "mpeg2enc.h"
#include "tables.h"

#include "macroblock.hh"
#include "picture.hh"

class MPEG2Encoder;

/* prototypes of global functions */

/* conform.c */
void range_checks (void);
void profile_and_level_checks (void);

/* motionest.c */

void init_motion (void);
void motion_estimation (Picture *picture);
void motion_subsampled_lum( Picture *picture );


/* predict.c */

void predict (Picture *picture);

/* putbits.c */
void initbits (void);
void putbits (uint32_t val, int n);
void alignbits (void);
int64_t bitcount (void);


/* putmpg.c */
void putintrablk (Picture *picture, int16_t *blk, int cc);
void putnonintrablk (Picture *picture,int16_t *blk);
void putmv (int dmv, int f_code);



/* putvlc.c */
void putDClum (int val);
void putDCchrom (int val);
void putACfirst (int run, int val);
void putAC (int run, int signed_level, int vlcformat);
void putaddrinc (int addrinc);
void putmbtype (int pict_type, int mb_type);
void putmotioncode (int motion_code);
void putdmv (int dmv);
void putcbp (int cbp);

/* quantize.c */

void iquantize( Picture *picture );

/* ratectl.c */
double inv_scale_quant( int q_scale_type, int raw_code );
int scale_quant( int q_scale_type, double quant );

/* readpic.c */
//void init_reader();
//int readframe (int frame_num, uint8_t *frame[]);
//int frame_lum_mean(int frame_num);
//void read_stream_params( unsigned int *hsize, unsigned int *vsize, 
//						 unsigned int *frame_rate_code,
//						 unsigned int  *interlacing_code, 
//						 unsigned int *aspect_code );

/* stats.c */
void calcSNR (Picture *picture);
void stats (void);

/* transfrm.c */
void transform (Picture *picture);

void itransform ( Picture *picture);


/* writepic.c */
void writeframe (int frame_num, uint8_t *frame[]);




EXTERN FILE *outfile, *statfile; /* file descriptors */
EXTERN int inputtype; /* format of input frames */

EXTERN int frame_num;			/* Useful for triggering debug information */


/* 
 * Local variables:
 *  c-file-style: "stroustrup"
 *  tab-width: 4
 *  indent-tabs-mode: nil
 * End:
 */

 


