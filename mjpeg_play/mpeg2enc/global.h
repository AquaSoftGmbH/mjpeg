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


#include "mpeg2enc.h"

/* choose between declaration (GLOBAL undefined)
 * and definition (GLOBAL defined)
 * GLOBAL is defined in exactly one file (mpeg2enc.c)
 */

#ifndef GLOBAL
#define EXTERN extern
#else
#define EXTERN
#endif

#define BITCOUNT_OFFSET  800000000LL

/* prototypes of global functions */

/* conform.c */
void range_checks _ANSI_ARGS_((void));
void profile_and_level_checks _ANSI_ARGS_(());

/* fdctref.c */
void init_fdct _ANSI_ARGS_((void));
void fdct _ANSI_ARGS_((short *block));

/* idct.c */
void idct _ANSI_ARGS_((short *block));
void init_idct _ANSI_ARGS_((void));

/* motion.c */

void init_motion ();

void set_motion_search_limits(	pict_data_s *picture,motion_comp_s *mc_data  );
void record_range_of_motion( pict_data_s *picture,motion_comp_s *mc_data  );

void motion_estimation _ANSI_ARGS_((
	pict_data_s *picture,
	motion_comp_s *mc_data,
	int secondfield, int ipflag));

void fast_motion_data _ANSI_ARGS_((unsigned char *mcompdata, int pict_struct));
void check_fast_motion_data _ANSI_ARGS_((unsigned char *blk, char *label ));

void reset_thresholds _ANSI_ARGS_( (int macroblocks_per_frame ) );
/* mpeg2enc.c */
void error _ANSI_ARGS_((char *text));

/* predict.c */
void predict _ANSI_ARGS_((pict_data_s *picture,
						  unsigned char *reff[], unsigned char *refb[],
						  unsigned char *cur[3], 
						  int secondfield));

/* putbits.c */
void initbits _ANSI_ARGS_((void));
void putbits _ANSI_ARGS_((int val, int n));
void alignbits _ANSI_ARGS_((void));
bitcount_t bitcount _ANSI_ARGS_((void));

/* puthdr.c */
void putseqhdr _ANSI_ARGS_((void));
void putseqext _ANSI_ARGS_((void));
void putseqdispext _ANSI_ARGS_((void));
void putuserdata _ANSI_ARGS_((char *userdata));
void putgophdr _ANSI_ARGS_((int frame, int closed_gop));
void putpicthdr _ANSI_ARGS_((pict_data_s *picture));
void putpictcodext _ANSI_ARGS_((pict_data_s *picture));
void putseqend _ANSI_ARGS_((void));

/* putmpg.c */
void putintrablk _ANSI_ARGS_((pict_data_s *picture, short *blk, int cc));
void putnonintrablk _ANSI_ARGS_((pict_data_s *picture,short *blk));
void putmv _ANSI_ARGS_((int dmv, int f_code));

/* putpic.c */
void putpict _ANSI_ARGS_((pict_data_s *picture, short (*quant_blocks)[64]));

/* putseq.c */
void putseq _ANSI_ARGS_((void));

/* putvlc.c */
void putDClum _ANSI_ARGS_((int val));
void putDCchrom _ANSI_ARGS_((int val));
void putACfirst _ANSI_ARGS_((int run, int val));
void putAC _ANSI_ARGS_((int run, int signed_level, int vlcformat));
void putaddrinc _ANSI_ARGS_((int addrinc));
void putmbtype _ANSI_ARGS_((int pict_type, int mb_type));
void putmotioncode _ANSI_ARGS_((int motion_code));
void putdmv _ANSI_ARGS_((int dmv));
void putcbp _ANSI_ARGS_((int cbp));

/* quantize.c */

void quant_intra _ANSI_ARGS_((
	pict_data_s *picture,
	short *src, short *dst, 
	unsigned short *quant_mat,
	unsigned short *iquant_mat, 
	int mquant, int *nonsat_mquant));
int quant_non_intra _ANSI_ARGS_((
	pict_data_s *picture,
	short *src, short *dst,
  unsigned short *quant_mat,  unsigned short *iquant_mat, int mquant, int *nonsat_mquant));
void iquant_intra _ANSI_ARGS_((short *src, short *dst, int dc_prec,
  unsigned short *quant_mat,  unsigned short *i_quant_mat, int mquant));
void iquant_non_intra _ANSI_ARGS_((short *src, short *dst,
  unsigned short *quant_mat, unsigned short *iquant_mat,  int mquant));
void init_quantizer();

#ifdef X86_CPU

int quantize_ni_mmx(short *dst, short *src, short *quant_mat, 
						   short *i_quant_mat, 
						   int imquant, int mquant, int sat_limit);
int quant_weight_coeff_sum_mmx (short *blk, unsigned short*i_quant_mat );
int cpuid_flags();
extern int use_mmx_quantizer;
extern int (*pquant_weight_coeff_sum)(short *blk, unsigned short*i_quant_mat );
#endif


int quant_weight_coeff_sum(short *blk, unsigned short*i_quant_mat );


/* ratectl.c */
void rc_init_seq _ANSI_ARGS_((void));
void rc_init_GOP _ANSI_ARGS_((int np, int nb));
void rc_init_pict _ANSI_ARGS_((pict_data_s *picture));
void rc_update_pict _ANSI_ARGS_((pict_data_s *picture));
int rc_start_mb _ANSI_ARGS_((pict_data_s *picture));
int rc_calc_mquant _ANSI_ARGS_((pict_data_s *picture,int j));
void vbv_end_of_picture _ANSI_ARGS_((pict_data_s *picture));
void calc_vbv_delay _ANSI_ARGS_((pict_data_s *picture));

/* readpic.c */
int readframe _ANSI_ARGS_((int frame_num, unsigned char *frame[]));

/* stats.c */
void calcSNR _ANSI_ARGS_((unsigned char *org[3], unsigned char *rec[3]));
void stats _ANSI_ARGS_((void));

/* transfrm.c */
void transform _ANSI_ARGS_((
	pict_data_s *picture,
	unsigned char *pred[], 
	unsigned char *cur[]));

void itransform _ANSI_ARGS_((
	pict_data_s *picture,
	unsigned char *pred[], unsigned char *cur[],
	short blocks[][64]));
void dct_type_estimation _ANSI_ARGS_((
	pict_data_s *picture,
	unsigned char *pred, unsigned char *cur
	));

void init_transform();

/* writepic.c */
void writeframe _ANSI_ARGS_((int frame_num, unsigned char *frame[]));


/* global variables */

EXTERN char version[]
#ifdef GLOBAL
  ="MSSG+ 1.01 (development of mpeg2encode V1.2, 96/07/19)"
#endif
;

EXTERN char author[]
#ifdef GLOBAL
  ="(C) 1996, MPEG Software Simulation Group, (C) 2000 Andrew Stevens, Rainer Johanni"
#endif
;

/* zig-zag scan */
EXTERN unsigned char zig_zag_scan[64]
#ifdef GLOBAL
=
{
  0,1,8,16,9,2,3,10,17,24,32,25,18,11,4,5,
  12,19,26,33,40,48,41,34,27,20,13,6,7,14,21,28,
  35,42,49,56,57,50,43,36,29,22,15,23,30,37,44,51,
  58,59,52,45,38,31,39,46,53,60,61,54,47,55,62,63
}
#endif
;

/* alternate scan */
EXTERN unsigned char alternate_scan[64]
#ifdef GLOBAL
=
{
  0,8,16,24,1,9,2,10,17,25,32,40,48,56,57,49,
  41,33,26,18,3,11,4,12,19,27,34,42,50,58,35,43,
  51,59,20,28,5,13,6,14,21,29,36,44,52,60,37,45,
  53,61,22,30,7,15,23,31,38,46,54,62,39,47,55,63
}
#endif
;

/* default intra quantization matrix */
EXTERN unsigned short default_intra_quantizer_matrix[64]
#ifdef GLOBAL
=
{
   8, 16, 19, 22, 26, 27, 29, 34,
  16, 16, 22, 24, 27, 29, 34, 37,
  19, 22, 26, 27, 29, 34, 34, 38,
  22, 22, 26, 27, 29, 34, 37, 40,
  22, 26, 27, 29, 32, 35, 40, 48,
  26, 27, 29, 32, 35, 40, 48, 58,
  26, 27, 29, 34, 38, 46, 56, 69,
  27, 29, 35, 38, 46, 56, 69, 83
}
#endif
;

/* default non intra quantization matrix */
EXTERN unsigned short default_nonintra_quantizer_matrix[64]
#ifdef GLOBAL
=
{
  16, 17, 18, 19, 20, 21, 22, 23,
  17, 18, 19, 20, 21, 22, 23, 24,
  18, 19, 20, 21, 22, 23, 24, 25,
  19, 20, 21, 22, 23, 24, 26, 27,
  20, 21, 22, 23, 25, 26, 27, 28,
  21, 22, 23, 24, 26, 27, 28, 30,
  22, 23, 24, 26, 27, 28, 30, 31,
  23, 24, 25, 27, 28, 30, 31, 33  
 
}
#endif
;

/* non-linear quantization coefficient table */
EXTERN unsigned char non_linear_mquant_table[32]
#ifdef GLOBAL
=
{
   0, 1, 2, 3, 4, 5, 6, 7,
   8,10,12,14,16,18,20,22,
  24,28,32,36,40,44,48,52,
  56,64,72,80,88,96,104,112
}
#endif
;

/* non-linear mquant table for mapping from scale to code
 * since reconstruction levels are not bijective with the index map,
 * it is up to the designer to determine most of the quantization levels
 */

EXTERN unsigned char map_non_linear_mquant[113] 
#ifdef GLOBAL
=
{
0,1,2,3,4,5,6,7,8,8,9,9,10,10,11,11,12,12,13,13,14,14,15,15,16,16,
16,17,17,17,18,18,18,18,19,19,19,19,20,20,20,20,21,21,21,21,22,22,
22,22,23,23,23,23,24,24,24,24,24,24,24,25,25,25,25,25,25,25,26,26,
26,26,26,26,26,26,27,27,27,27,27,27,27,27,28,28,28,28,28,28,28,29,
29,29,29,29,29,29,29,29,29,30,30,30,30,30,30,30,31,31,31,31,31
}
#endif
;

EXTERN char pict_type_char[6]
#ifdef GLOBAL
= {'X', 'I', 'P', 'B', 'D', 'X'}
#endif
;

int video_buffer_size;

/* Buffers frame data */
EXTERN unsigned char ***frame_buffers;

EXTERN pict_data_s cur_picture;

/* Buffers for econstructed frames */
EXTERN unsigned char *newrefframe[3], *oldrefframe[3], *auxframe[3];
/* Pointers to original frames in frame_buffers */
EXTERN unsigned char *neworgframe[3], *oldorgframe[3], *auxorgframe[3];
/* prediction of current frame */
EXTERN unsigned char *predframe[3];
/* Buffer for filter pre-processing */

EXTERN struct motion_data *motion_data;
EXTERN short (*qblocks)[64];

/* intra / non_intra quantization matrices */
EXTERN unsigned short intra_q[64], inter_q[64];
EXTERN unsigned short i_intra_q[64], i_inter_q[64];
EXTERN unsigned short chrom_intra_q[64],chrom_inter_q[64];
/* prediction values for DCT coefficient (0,0) */
EXTERN int dc_dct_pred[3];
/* clipping (=saturation) table */
EXTERN unsigned char *clp;

/* name strings */
EXTERN char id_string[256], tplorg[256], tplref[256];
EXTERN char iqname[256], niqname[256];
EXTERN char statname[256];
EXTERN char errortext[256];

EXTERN FILE *outfile, *statfile; /* file descriptors */
EXTERN int inputtype; /* format of input frames */

EXTERN int quiet; /* suppress warnings */


/* coding model parameters */

EXTERN int N; /* number of frames in Group of Pictures */
EXTERN int M; /* distance between I/P frames */
EXTERN int P; /* intra slice refresh interval */
EXTERN int nframes; /* total number of frames to encode */
EXTERN int frame0, tc0; /* number and timecode of first frame */
EXTERN int mpeg1; /* ISO/IEC IS 11172-2 sequence */
EXTERN int fieldpic; /* use field pictures */



/*
  How many frames to read ahead (eventually intended to support
  scene change based GOP structuring.  READ_LOOK_AHEAD/2 must be
  greater than M (otherwise buffers will be overwritten that are
  still in use).

  It should also be a multiple of 4 due to the way buffers are
  filled (in 1/4's).
*/

#define READ_LOOK_AHEAD 16 


/* sequence specific data (sequence header) */

EXTERN int horizontal_size, vertical_size; /* frame size (pels) */
EXTERN int width, height; /* encoded frame size (pels) multiples of 16 or 32 */
EXTERN int chrom_width,chrom_height,block_count;
EXTERN int mb_width, mb_height; /* frame size (macroblocks) */
EXTERN int width2, height2, mb_height2, chrom_width2; /* picture size */
EXTERN int qsubsample_offset, 
           fsubsample_offset,
	       rowsums_offset,
	       colsums_offset;   /* Offset from picture buffer start of sub-sampled data... */
						      
EXTERN int aspectratio; /* aspect ratio information (pel or display) */
EXTERN int frame_rate_code; /* coded value of frame rate */
EXTERN double frame_rate; /* frames per second */
EXTERN double bit_rate; /* bits per second */
EXTERN int    drop_lsb; /* Drop lsb's of samples */
EXTERN int    noise_filt; /* Do a crude Gaussian noise filter on samples */
EXTERN int    fast_mc_frac; /* inverse proportion of fast motion estimates
							   consider in detail */
EXTERN int    fast_mc_threshold; /* Use a sliding threshold technique to
									dynamical adjust motion compensation
									window size */
EXTERN int    pred_ratectl;      /* Use experimental predictive rate
									controller  */
EXTERN int vbv_buffer_size; /* size of VBV buffer (* 16 kbit) */
EXTERN int constrparms; /* constrained parameters flag (MPEG-1 only) */
EXTERN int load_iquant, load_niquant; /* use non-default quant. matrices */
EXTERN int load_ciquant,load_cniquant;
EXTERN int search_radius[2];		/* Radius for motion compensation search */

/* sequence specific data (sequence extension) */

EXTERN int profile, level; /* syntax / parameter constraints */
EXTERN int prog_seq; /* progressive sequence */
EXTERN int chroma_format;
EXTERN int low_delay; /* no B pictures, skipped pictures */


/* sequence specific data (sequence display extension) */

EXTERN int video_format; /* component, PAL, NTSC, SECAM or MAC */
EXTERN int color_primaries; /* source primary chromaticity coordinates */
EXTERN int transfer_characteristics; /* opto-electronic transfer char. (gamma) */
EXTERN int matrix_coefficients; /* Eg,Eb,Er / Y,Cb,Cr matrix coefficients */
EXTERN int display_horizontal_size, display_vertical_size; /* display size */




/* picture specific data (currently control by global flags) */

EXTERN int opt_dc_prec;
EXTERN int opt_prog_frame;
EXTERN int opt_repeatfirst;
EXTERN int opt_topfirst;

/* use only frame prediction and frame DCT (I,P,B) */
EXTERN int frame_pred_dct_tab[3];
EXTERN int qscale_tab[3]; 	/* linear/non-linear quantizaton table */
EXTERN int intravlc_tab[3]; /* intra vlc format (I,P,B) */
EXTERN int altscan_tab[3]; 	/* alternate scan (I,P,B */

/* Currently unused... */
EXTERN int conceal_tab[3]; 	/* use concealment motion vectors (I,P,B) */

/* Global flags controlling encoding behaviour */

EXTERN int fix_mquant;    		/* use fixed quant, range 1 ... 31 */
EXTERN int constant_bitrate;  	/* Original CBR encoding strategy  */
EXTERN int output_stats;	    /* Display debugging statistics during coding */
EXTERN double act_boost;		/* Quantisation reduction for highly active blocks */
/* Useful for triggering debug information */

EXTERN int frame_num;			

EXTERN int input_fd;
