/* syntaxparams.h, Global flags and constants controlling MPEG syntax */
#ifndef _SYNTAXPARAMS_H
#define _SYNTAXPARAMS_H


#include "config.h"
#include "mjpeg_types.h"

/* choose between declaration (GLOBAL undefined) and definition
 * (GLOBAL defined) GLOBAL is defined in exactly one file (mpeg2enc.c)
 * TODO: Get rid of this and do it cleanly....
 */

#ifndef GLOBAL
#define EXTERN extern
#else
#define EXTERN
#endif


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


/* SCale factor for fast integer arithmetic routines */
/* Changed this and you *must* change the quantisation routines as they depend on its absolute
	value */
#define IQUANT_SCALE_POW2 16
#define IQUANT_SCALE (1<<IQUANT_SCALE_POW2)
#define COEFFSUM_SCALE (1<<16)

#define BITCOUNT_OFFSET  0LL

/*
  How many frames to read in one go and the size of the frame data buffer.
*/

#define READ_CHUNK_SIZE 5

/*
  How many frames encoding may be concurrently under way.
  N.b. there is no point setting this greater than M.
  Additional parallelism can be exposed at a finer grain by
  parallelising per-macro-block computations.
 */

#define MAX_WORKER_THREADS 4


EXTERN unsigned int opt_horizontal_size, opt_vertical_size; /* frame size (pels) */

EXTERN unsigned int opt_aspectratio;			/* aspect ratio information (pel or display) */
EXTERN unsigned int opt_frame_rate_code;		/* coded value of playback display
									 * frame rate */
EXTERN int opt_dctsatlim;			/* Value to saturated DCT coeffs to */
EXTERN double opt_frame_rate;		/* Playback display frames per
									   second N.b. when 3:2 pullback
									   is active this is higher than
									   the frame decode rate.
									*/
EXTERN double opt_bit_rate;			/* bits per second */
EXTERN bool opt_seq_hdr_every_gop;
EXTERN bool opt_seq_end_every_gop;   /* Useful for Stills
									 * sequences... */
EXTERN bool opt_svcd_scan_data;
EXTERN unsigned int opt_vbv_buffer_code;      /* Code for size of VBV buffer (*
									  * 16 kbit) */
EXTERN double opt_vbv_buffer_size;
EXTERN unsigned int opt_still_size;		     /* If non-0 encode a stills
									  * sequence: 1 I-frame per
									  * sequence pseudo VBR. Each
									  * frame sized to opt_still_size
									  * KB */
EXTERN unsigned int opt_vbv_buffer_still_size;  /* vbv_buffer_size holds still
										  size.  Ensure still size
										  matches. */

EXTERN bool opt_constrparms;         /* constrained parameters flag (MPEG-1 only) */
EXTERN bool opt_load_iquant, 
	        opt_load_niquant;        /* use non-default quant. matrices */



EXTERN unsigned int opt_profile, opt_level; /* syntax / parameter constraints */
EXTERN bool opt_prog_seq; /* progressive sequence */
EXTERN int opt_chroma_format;
EXTERN int opt_low_delay; /* no B pictures, skipped pictures */


/* sequence specific data (sequence display extension) */

EXTERN unsigned int opt_video_format; /* component, PAL, NTSC, SECAM or MAC */
EXTERN unsigned int opt_color_primaries; /* source primary chromaticity coordinates */
EXTERN unsigned int opt_transfer_characteristics; /* opto-electronic transfer char. (gamma) */
EXTERN unsigned int opt_matrix_coefficients; /* Eg,Eb,Er / Y,Cb,Cr matrix coefficients */
EXTERN unsigned int opt_display_horizontal_size, 
	       opt_display_vertical_size; /* display size */




/* picture specific data (currently controlled by global flags) */

EXTERN unsigned int opt_dc_prec;
EXTERN bool opt_topfirst;


EXTERN bool opt_mpeg1;				/* ISO/IEC IS 11172-2 sequence */
EXTERN bool opt_fieldpic;			/* use field pictures */
EXTERN bool opt_pulldown_32;		/* 3:2 Pulldown of movie material */



/* use only frame prediction and frame DCT (I,P,B) */
EXTERN int opt_frame_pred_dct_tab[3];
EXTERN int opt_qscale_tab[3]; 	/* linear/non-linear quantizaton table */
EXTERN int opt_intravlc_tab[3]; /* intra vlc format (I,P,B) */
EXTERN int opt_altscan_tab[3]; 	/* alternate scan (I,P,B */

/* motion data */
struct motion_data {
	unsigned int forw_hor_f_code,forw_vert_f_code; /* vector range */
	unsigned int sxf,syf; /* search range */
	unsigned int back_hor_f_code,back_vert_f_code;
	unsigned int sxb,syb;
};

EXTERN struct motion_data *opt_motion_data;



/* Orginal intra / non_intra quantization matrices */
EXTERN uint16_t *opt_intra_q, *opt_inter_q;
EXTERN uint16_t *i_intra_q, *i_inter_q;


/* **************************
 * Global flags controlling encoding behaviour 
 ************************** */

EXTERN double ctl_decode_frame_rate;	/* Actual stream frame
										   decode-rate. This is lower
										   than playback rate if 3:2
										   pulldown is active.
										*/
EXTERN int ctl_video_buffer_size;    /* Video buffer requirement target */

EXTERN int ctl_N_max;				/* number of frames in Group of Pictures (max) */
EXTERN int ctl_N_min;				/* number of frames in Group of Pictures (min) */
EXTERN int ctl_M;					/* distance between I/P frames */

EXTERN int ctl_M_min;			    /* Minimum distance between I/P frames */

EXTERN bool ctl_closed_GOPs;	    /* Force all GOPs to be closed -
									 * useful for satisfying
									 * requirements for multi-angle
									 * DVD authoring */

EXTERN bool ctl_refine_from_rec;	/* Is final refinement of motion
									 * compensation computed from
									 * reconstructed reference frame
									 * image (slightly higher quality,
									 * bad for multi-threading) or
									 * original reference frame
									 * (opposite) */

EXTERN int ctl_44_red;			/* Sub-mean population reduction passes for 4x4 and 2x2 */
EXTERN int ctl_22_red;			/* Motion compensation stages						*/
EXTERN int ctl_seq_length_limit;
EXTERN double ctl_nonvid_bit_rate;	/* Bit-rate for non-video to assume for
								   sequence splitting calculations */

EXTERN double ctl_quant_floor;    /* quantisation floor [1..10] (0 for
										 * CBR) */


EXTERN double ctl_act_boost;		/* Quantisation reduction for highly active blocks */


EXTERN int ctl_max_encoding_frames; /* Maximum number of concurrent
									   frames to be concurrently encoded 
									   Used to control multi_threading.
									*/

EXTERN bool ctl_parallel_read; /* Does the input reader / bufferer
								 run as a seperate thread?
							  */


/* *************************
 * input stream attributes
 ************************* */

EXTERN int istrm_nframes;				/* total number of frames to encode
								   Note: this may start enormous and shrink
								   down later if the input stream length is
								   unknown at the start of encoding.
								   */
EXTERN int istrm_fd;

/* ***************************
 * Encoder internal derived values and parameters
 *************************** */


EXTERN int 
    opt_enc_width, 
	opt_enc_height;   /* encoded frame size (pels) multiples of 16 or 32 */

EXTERN int 
    opt_phy_width, 
	opt_phy_height;   /* Physical Frame buffer size (pels) may differ
					   * from encoded size due to alignment
					   * constraints */

EXTERN int lum_buffer_size, chrom_buffer_size;


EXTERN int opt_phy_chrom_width,opt_phy_chrom_height, block_count;
EXTERN int mb_width, mb_height; /* frame size (macroblocks) */
EXTERN int opt_phy_width2, opt_phy_height2, opt_enc_height2,
	mb_height2, opt_phy_chrom_width2; /* picture size */
EXTERN int qsubsample_offset, 
           fsubsample_offset,
	       rowsums_offset,
	       colsums_offset;		/* Offset from picture buffer start of sub-sampled data... */

EXTERN int mb_per_pict;			/* Number of macro-blocks in a picture */						

#endif
