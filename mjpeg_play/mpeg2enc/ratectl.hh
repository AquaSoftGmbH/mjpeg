#ifndef _RATECTL_HH
#define _RATECTL_HH

/*  (C) 2003 Andrew Stevens */

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

#include <config.h>
#include <deque>
#include "mjpeg_types.h"
#include "mpeg2syntaxcodes.h"

class MacroBlock;
class EncoderParams;
class Picture;



/*
	Base class for state of rate-controller.
	Factory allows us to generate appropriate state
	objects for a given rate-controller object.

	Set allow us to set set of a rate-controller from
	a previously copied state...
*/
class RateCtlState
{
public:
	virtual ~RateCtlState() {}
	virtual RateCtlState *New() const = 0;
	virtual void Set( const RateCtlState &state) = 0;
	virtual const RateCtlState &Get() const = 0;
};

class RateCtl 
{
public:
    RateCtl( EncoderParams &_encparams, RateCtlState &_state );
    virtual void InitSeq( bool reinit ) = 0;
    virtual void InitNewPict (Picture &picture) = 0;
    virtual void UpdatePict (Picture &picture, int &padding_needed ) = 0;
    virtual int MacroBlockQuant(  const MacroBlock &mb) = 0;
    virtual int  InitialMacroBlockQuant(Picture &picture) = 0;
    virtual void CalcVbvDelay (Picture &picture) = 0;
    
    inline RateCtlState *NewState() const { return state.New(); }
    inline void SetState( const RateCtlState &toset) { state.Set( toset ); }
    inline const RateCtlState &GetState() const { return state.Get(); }

    // TODO DEBUG
    virtual double SumAvgActivity() = 0;

    static double InvScaleQuant(  int q_scale_type, int raw_code );
    static int ScaleQuant( int q_scale_type, double quant );
protected:
	virtual void VbvEndOfPict (Picture &picture) = 0;
    double ScaleQuantf( int q_scale_type, double quant );
    EncoderParams &encparams;
    RateCtlState &state;
};

class Pass1RateCtl : public RateCtl
{    
public:
    Pass1RateCtl( EncoderParams &encoder, RateCtlState &state );
    virtual void InitGOP( int nb, int np ) = 0;
};

class Pass2RateCtl : public RateCtl
{
public:
    Pass2RateCtl( EncoderParams &encoder, RateCtlState &state );
    virtual void InitGOP( std::deque<Picture *>::iterator gop_pics, int gop_len ) = 0;
};

/* 
 * Local variables:
 *  c-file-style: "stroustrup"
 *  tab-width: 4
 *  indent-tabs-mode: nil
 * End:
 */
#endif
