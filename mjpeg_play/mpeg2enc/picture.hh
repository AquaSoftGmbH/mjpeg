
/*  (C) 2000/2001/2002 Andrew Stevens */

/*  This is free software; you can redistribute it
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


#ifndef _PICTURE_HH
#define _PICTURE_HH
/* picture.hh picture class... */

#include "config.h"
#include "mjpeg_types.h"
#include "encoderparams.hh"
#include "synchrolib.h"
#include "macroblock.hh"
#include <vector>

using namespace std;


/* Transformed per-picture data  */

typedef int MotionVecPred[2][2][2];
typedef int DC_DctPred[3];

class CodingPredictors
{
public:

    void Reset_DC_DCT_Pred()
        {
            int cc;
            for (cc=0; cc<3; cc++)
                dc_dct_pred[cc] = 0;
                
        }
    void Reset_MV_Pred()
        {
            int *base=&PMV[0][0][0];
            int v;
            for( v = 0; v < 2*2*2; ++v)
                base[v]=0;
        }

    DC_DctPred dc_dct_pred;
    MotionVecPred PMV;
    MacroBlock *prev_mb;
    int mquant_pred;
    
};

class MPEG2Encoder;
class RateCtl;
class MPEG2Coder;
class Quantizer;
class StreamState;



// TODO: Nasty hack to keep interface to some old routines the same
// Allocation should be done with ImagePlaneArray
typedef uint8_t **ImagePlanes;
typedef uint8_t *ImagePlaneArray[5];

class Picture : public CodingPredictors
{
public:
    Picture( EncoderParams &_encparams, 
             MPEG2Coder &_coder, 
             Quantizer &_quantizer );
    ~Picture();

    // In putseq.cc
    void MotionSubSampledLum();
    void EncodeMacroBlocks();
    void ITransform();
    void IQuantize();
    void CalcSNR();
    void Stats();
    void Reconstruct();

    void SetSeqPos( int decode, int b_index );
    void Set2ndField();
    void Set_IP_Frame( StreamState *ss, int num_frames );
    void Set_B_Frame( StreamState *ss );

    // In putpic..c
    void PutHeadersAndEncoding( RateCtl &ratecontrol );
    bool TryEncoding(RateCtl &ratecontrol);
    void PutHeader(); 

    // In ratectl.cc
    void ActivityMeasures( double &act_sum, double &var_sum);

    //
    //
    //
    inline bool Legal( const MotionVector &mv ) const
        {
            return mv[Dim::X] >= -sxf && mv[Dim::X] < sxf 
                && mv[Dim::Y] >= -syf && mv[Dim::Y] < syf;
        }


    inline bool InRangeFrameMVRef( const Coord &crd ) const
        {
            return  crd.x >= 0 && crd.x <= (encparams.enc_width-16)*2
                && crd.y >= 0 && crd.y <= (encparams.enc_height-16)*2;
            
        }

    inline bool InRangeFieldMVRef( const Coord &crd ) const
        {
            return  crd.x >= 0 && crd.x <= (encparams.enc_width-16)*2
                && crd.y >= 0 && crd.y <= (encparams.enc_height/2-16)*2;
            
        }


private:

    void PutSliceHdr( int slice_mb_y );
    void PutMVs( MotionEst &me, bool back );
    void PutCodingExt(); 

public:

    /***************
     *
     * Data initialised at construction
     *
     **************/

    //MPEG2Encoder &encoder;
    EncoderParams &encparams;
    MPEG2Coder &coder;
    Quantizer &quantizer;

	/* 8*8 block data, raw (unquantised) and quantised, and (eventually but
	   not yet inverse quantised */
	DCTblock *blocks;
	DCTblock *qblocks;

	/* Macroblocks of picture */
	vector<MacroBlock> mbinfo;


    /***************
     *
     * Data update as picture is re-used for different images in streams
     *
     **************/

	int decode;					/* Number of frame in stream */
	int present;				/* Number of frame in playback order */
	/* multiple-reader/single-writer channels Synchronisation  
	   sync only: no data is "read"/"written"
	 */
    Picture *ref_frame;
    Picture *prev_frame;

	/* picture encoding source data  */
	ImagePlanes oldorg, neworg;	/* Images for Old and new reference picts */
	ImagePlanes oldref, newref;	/* original and reconstructed */
	ImagePlanes curorg, curref;	/* Images for current pict orginal and*/
                                    /* reconstructed */
	ImagePlanes pred;			/* Prediction based on MC (if any) */
	int sxf, syf, sxb, syb;		/* MC search limits. */
	bool secondfield;			/* Second field of field frame */
	bool ipflag;				/* P pict in IP frame (FIELD pics only)*/

	/* picture structure (header) data */

	int temp_ref; /* temporal reference */
	int pict_type; /* picture coding type (I, P or B) */
	int vbv_delay; /* video buffering verifier delay (1/90000 seconds) */
	int forw_hor_f_code, forw_vert_f_code;
	int back_hor_f_code, back_vert_f_code; /* motion vector ranges */
	int dc_prec;				/* DC coefficient prec for intra blocks */
	int pict_struct;			/* picture structure (frame, top / bottom) */
	int topfirst;				/* display top field first */
	bool frame_pred_dct;			/* Use only frame prediction... */
	int intravlc;				/* Intra VLC format */
	int q_scale_type;			/* Quantiser scale... */
	bool altscan;				/* Alternate scan  pattern selected */
    const uint8_t *scan_pattern; /* The scan pattern itself */
	bool repeatfirst;			/* repeat first field after second field */
	bool prog_frame;				/* progressive frame */

    int unit_coeff_threshold;   // Unit coefficient density weight
                                // below which zeroing should be applied.
    int unit_coeff_first;       // First coefficient for zeroing purposes...
                                // either 1 or 0.
	/* Information for GOP start frames */
	bool gop_start;             /* GOP Start frame */
    bool closed_gop;            /* GOP is closed   */
	int nb;						/* B frames in GOP */
	int np;						/* P frames in GOP */
	bool new_seq;				/* GOP starts new sequence */

	/* Statistics... */
	int pad;
	int split;
	double AQ;
	double SQ;
	double avg_act;
	double sum_avg_act;

    
};


/* 
 * Local variables:
 *  c-file-style: "stroustrup"
 *  tab-width: 4
 *  indent-tabs-mode: nil
 * End:
 */
#endif
