/* mpg2enc.h, defines and types                                             */

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
#include <inttypes.h>
#include "synchrolib.h"
#include "mjpeg_logging.h"

#define PICTURE_START_CODE 0x100L
#define SLICE_MIN_START    0x101L
#define SLICE_MAX_START    0x1AFL
#define USER_START_CODE    0x1B2L
#define SEQ_START_CODE     0x1B3L
#define EXT_START_CODE     0x1B5L
#define SEQ_END_CODE       0x1B7L
#define GOP_START_CODE     0x1B8L
#define ISO_END_CODE       0x1B9L
#define PACK_START_CODE    0x1BAL
#define SYSTEM_START_CODE  0x1BBL

/* picture coding type */
#define I_TYPE 1
#define P_TYPE 2
#define B_TYPE 3
#define D_TYPE 4

/* picture structure */
#define TOP_FIELD     1
#define BOTTOM_FIELD  2
#define FRAME_PICTURE 3

/* macroblock type */
#define MB_INTRA    1
#define MB_PATTERN  2
#define MB_BACKWARD 4
#define MB_FORWARD  8
#define MB_QUANT    16

/* motion_type */
#define MC_FIELD 1
#define MC_FRAME 2
#define MC_16X8  2
#define MC_DMV   3

/* mv_format */
#define MV_FIELD 0
#define MV_FRAME 1

/* chroma_format */
#define CHROMA420 1
#define CHROMA422 2
#define CHROMA444 3

/* extension start code IDs */

#define SEQ_ID       1
#define DISP_ID      2
#define QUANT_ID     3
#define SEQSCAL_ID   5
#define PANSCAN_ID   7
#define CODING_ID    8
#define SPATSCAL_ID  9
#define TEMPSCAL_ID 10

/* inputtype */
#define T_Y_U_V 0
#define T_YUV   1
#define T_PPM   2

/*
  Some enumerated types to give legible indices into motion vector arrays
*/

typedef enum coord { x_crd, y_crd} coord_e;
typedef enum mc_dir { fwd, bwd } mc_dir_e;
typedef enum field { top, bot }  field_e;



/* macroblock information */
struct mbinfo {
	int mb_type; /* intra/forward/backward/interpolated */
	int motion_type; /* frame/field/16x8/dual_prime */
	int dct_type; /* field/frame DCT */
	int mquant; /* quantization parameter */
	int cbp; /* coded block pattern */
	int skipped; /* skipped macroblock */
	int MV[2][2][2]; /* motion vectors */
	int mv_field_sel[2][2]; /* motion vertical field select */
	int dmvector[2]; /* dual prime vectors */
	double act; /* activity measure */
	int i_act;  /* Activity measure if intra coded (I/P-frame) */
	int p_act;  /* Activity measure for *forward* prediction (P-frame) */
	int b_act;	/* Activity measure if bi-directionally coded (B-frame) */
	int var; 	/* Macroblock luminance variance (measure of activity) */
	short (*dctblocks)[64];
#ifdef OUTPUT_STAT
  double N_act;
#endif

};

typedef struct mbinfo mbinfo_s;
 
/* motion data */
struct motion_data {
  int forw_hor_f_code,forw_vert_f_code; /* vector range */
  int sxf,syf; /* search range */
  int back_hor_f_code,back_vert_f_code;
  int sxb,syb;
};

/* Transformed per-picture data  */

struct pict_data
{
	int decode;					/* Number of frame in stream */
	int present;				/* Number of frame in playbakc order */
	/* multiple-reader/single-writer channels Synchronisation  
	   sync only: no data is "read"/"written"
	 */
	sync_guard_t *ref_frame_completion;
	sync_guard_t *prev_frame_completion;
	sync_guard_t completion;

	/* picture encoding source data  */
	uint8_t **oldorg, **neworg;	/* Images for Old and new reference picts */
	uint8_t **oldref, **newref;	/* original and reconstructed */
	uint8_t **curorg, **curref;	/* Images for current pict orginal and*/
								/* reconstructed */
	uint8_t **pred;			/* Prediction based on MC (if any) */
	int sxf, syf, sxb, syb;		/* MC search limits. */
	int secondfield;			/* Second field of field frame */
	int ipflag;					/* P pict in IP frame (FIELD pics only)*/

	/* picture structure (header) data */

	int temp_ref; /* temporal reference */
	int pict_type; /* picture coding type (I, P or B) */
	int vbv_delay; /* video buffering verifier delay (1/90000 seconds) */
	int forw_hor_f_code, forw_vert_f_code;
	int back_hor_f_code, back_vert_f_code; /* motion vector ranges */
	int dc_prec;				/* DC coefficient prec for intra blocks */
	int pict_struct;			/* picture structure (frame, top / bottom) */
	int topfirst;				/* display top field first */
	int frame_pred_dct;			/* Use only frame prediction... */
	int intravlc;				/* Intra VLC format */
	int q_scale_type;			/* Quantiser scale... */
	int altscan;				/* Alternate scan  */
	int repeatfirst;			/* repeat first field after second field */
	int prog_frame;				/* progressive frame */

	/* 8*8 block data, raw (unquantised) and quantised, and (eventually but
	   not yet inverse quantised */
	int16_t (*blocks)[64];
	int16_t (*qblocks)[64];

	/* macroblock side information array */
	struct mbinfo *mbinfo;

	/* Information for GOP start frames */
	int gop_start;
	int nb;						/* B frames in GOP */
	int np;						/* P frames in GOP */
	int new_seq;				/* GOP starts new sequence */

	/* Statistics... */
	int pad;
	int split;
	double AQ;
	double SQ;
	double avg_act;
	double sum_avg_act;
};

typedef struct pict_data pict_data_s;

struct mc_result
{
	uint16_t weight;
	int8_t x;
	int8_t y;
};

typedef struct mc_result mc_result_s;


/* 4*4 sub-sampled pel Threshold below which initial 8*8 grid motion
   compensation matches are always discarded.
*/

#define COARSE_44_SAD_THRESHOLD 4*4*64

/* SCale factor for fast integer arithmetic routines */
/* Changed this and you *must* change the quantisation routines as they depend on its absolute
	value */
#define IQUANT_SCALE_POW2 16
#define IQUANT_SCALE (1<<IQUANT_SCALE_POW2)
#define COEFFSUM_SCALE (1<<16)

/* Byte alignment of buffers for picture data.  Very important for
	performance with MMX etc 
*/


#define BUFFER_ALIGN 16



