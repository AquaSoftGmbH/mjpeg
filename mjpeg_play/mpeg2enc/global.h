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

/* puthdr.c */
void putseqhdr (void);
void putseqext (void);
void putseqdispext (void);
void putuserdata (const uint8_t *userdata, int len);
void putgophdr (int frame, int closed_gop);
void putseqend (void);

/* putmpg.c */
void putintrablk (Picture *picture, int16_t *blk, int cc);
void putnonintrablk (Picture *picture,int16_t *blk);
void putmv (int dmv, int f_code);


/* putseq.c */
void putseq (MPEG2Encoder &encoder);

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


 


