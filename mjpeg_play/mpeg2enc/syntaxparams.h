/* syntaxparams.h, Global flags and constants controlling MPEG syntax */
#ifndef _SYNTAXPARAMS_H
#define _SYNTAXPARAMS_H


#include "config.h"
#include "mjpeg_types.h"
#include "syntaxconsts.h"

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



/*
  How many frames to read in one go
*/

#define READ_CHUNK_SIZE 1

/*
  How many frames encoding may be concurrently under way.
  N.b. there is no point setting this greater than M.
  Additional parallelism can be exposed at a finer grain by
  parallelising per-macro-block computations.
 */

#define MAX_WORKER_THREADS 4



/* motion data */
struct motion_data {
	unsigned int forw_hor_f_code,forw_vert_f_code; /* vector range */
	unsigned int sxf,syf; /* search range */
	unsigned int back_hor_f_code,back_vert_f_code;
	unsigned int sxb,syb;
};



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


EXTERN double ctl_act_boost;		/* Quantisation reduction factor for blocks
									 with little texture (low variance) */

EXTERN double ctl_boost_var_ceil;		/* Variance below which
									 * quantisation boost cuts in */


EXTERN int ctl_max_encoding_frames; /* Maximum number of concurrent
									   frames to be concurrently encoded 
									   Used to control multi_threading.
									*/

EXTERN int ctl_max_active_ref_frames;
EXTERN int ctl_max_active_b_frames;

EXTERN bool ctl_parallel_read; /* Does the input reader / bufferer
								 run as a seperate thread?
							  */
EXTERN int ctl_unit_coeff_elim;	/* Threshold of unit coefficient
									density below which unit
									coefficient blocks should be
									zeroed.  < 0 implies DCT
									coefficient should be included. */


/* *************************
 * input stream attributes
 ************************* */

EXTERN int istrm_nframes;				/* total number of frames to encode
								   Note: this may start enormous and shrink
								   down later if the input stream length is
								   unknown at the start of encoding.
								   */
EXTERN int istrm_fd;

struct RateCtl;


struct EncoderParams
{
	/**************
	 *
	 * Global encoding parameters (set by supplied stream, never
	 * change in a run) 
	 *
     *************/

	unsigned int horizontal_size, vertical_size;
	
	unsigned int aspectratio;	/* aspect ratio information (pel or display) */
	unsigned int frame_rate_code;	/* coded value of playback display
									 * frame rate */
	int dctsatlim;			/* Value to saturated DCT coeffs to */
	double frame_rate;		/* Playback display frames per second
							   N.b. when 3:2 pullback is active
							   this is higher than the frame
							   decode rate.  */
	double bit_rate;			/* bits per second */
	bool seq_hdr_every_gop;
	bool seq_end_every_gop;	/* Useful for Stills sequences... */
	bool svcd_scan_data;
	unsigned int vbv_buffer_code;      /* Code for size of VBV buffer (*
										* 16 kbit) */
	double vbv_buffer_size;

	unsigned int still_size; /* If non-0 encode a stills sequence: 1
								I-frame per sequence pseudo VBR. Each
								frame sized to still_size KB */
	unsigned int vbv_buffer_still_size;  /* vbv_buffer_size holds
											still size.  Ensure still
											size matches. */

	bool constrparms;         /* constrained parameters flag, MPEG-1 only */
	bool load_iquant; 
	bool load_niquant;        /* use non-default quant. matrices */



	unsigned int profile, level; /* syntax / parameter constraints */
	bool ignore_constraints;	/* Disabled conformance checking of
									 * hor_size, vert_size and
									 * samp_rate */
			

	bool prog_seq; /* progressive sequence */
	int low_delay; /* no B pictures, skipped pictures */

    /*******
	 *
	 * sequence specific data (sequence display extension)
	 *
	 ******/
	
	unsigned int video_format; /* component, PAL, NTSC, SECAM or MAC */
	unsigned int color_primaries; /* source primary chromaticity coordinates */
	unsigned int transfer_characteristics; /* opto-electronic transfer char. (gamma) */
	unsigned int matrix_coefficients; /* Eg,Eb,Er / Y,Cb,Cr matrix coefficients */
	unsigned int display_horizontal_size;  /* display size */
	unsigned int display_vertical_size;


	bool mpeg1;				/* ISO/IEC IS 11172-2 sequence */
	bool fieldpic;			/* use field pictures */
	bool pulldown_32;		/* 3:2 Pulldown of movie material */
	bool topfirst;


	/************
	 *
	 * Picture kind specific informatino (picture header flags)
     *
     ***********/
	
	int frame_pred_dct_tab[3]; /* use only frame prediction and frame
								  DCT (I,P,B) */
	int qscale_tab[3];			/* linear/non-linear quantizaton table */
	int intravlc_tab[3];		/* intra vlc format (I,P,B) */
	int altscan_tab[3];			/* alternate scan (I,P,B */
	unsigned int dc_prec;

    /****************************
	 * Encoder internal derived values and parameters
	 *************************** */

	int enc_width, 
		enc_height;   /* encoded frame size (pels) multiples of 16 or 32 */
	
	int phy_width, 
		phy_height;   /* Physical Frame buffer size (pels) may differ
						 from encoded size due to alignment
						 constraints */

	int lum_buffer_size, chrom_buffer_size;


	int phy_chrom_width,phy_chrom_height;
	int mb_width, mb_height;	/* frame size (macroblocks) */

	/* Picture dimensioning (allowing for interlaced/non-interlaced coding) */
	int phy_width2, phy_height2, enc_height2,
		mb_height2, phy_chrom_width2;
	int qsubsample_offset, 
		fsubsample_offset;
	int mb_per_pict;			/* Number of macro-blocks in a picture */  

	
	struct motion_data *motion_data;


    /* Selected intra/non_intra quantization matrices both ordinary*/
	/* and inverted */
	uint16_t *intra_q, *inter_q;
	
	struct RateCtl *bitrate_controller;	/* Ick struct for use in .c files */

};


#endif
