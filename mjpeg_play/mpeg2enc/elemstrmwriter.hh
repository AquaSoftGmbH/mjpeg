#ifndef _ELEMSTREAMWRITER_HH
#define _ELEMSTREAMWRITER_HH

/*  (C) 2003 Andrew Stevens */

/*  This Software is free software; you can redistribute it
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
#include "mjpeg_types.h"

class EncoderParams;

class ElemStrmWriter
{
public:
	ElemStrmWriter( EncoderParams &encoder );
    // TODO eventually byte write will be virtual and buffering
    // TODO will be part of base class...

    /**************
     *
     * Write rightmost n (0<=n<=32) bits of val to outfile 
     *
     *************/
    virtual void PutBits( uint32_t val, int n) = 0;
    virtual void FrameBegin() = 0;
    virtual void FrameFlush() = 0;
    virtual void FrameDiscard() = 0;
    void AlignBits();
    inline int64_t BitCount() { return 8LL*bytecnt + (8-outcnt); }

protected:
	EncoderParams &encparams;
    uint32_t outbfr;
    int64_t bytecnt;
    int outcnt;

};


class FILE_StrmWriter : public ElemStrmWriter
{
public:
    FILE_StrmWriter( EncoderParams &encoder, const char *ofile_ptr ); 
    virtual ~FILE_StrmWriter();       
    void PutBits( uint32_t val, int n);
    void FrameBegin();
    void FrameFlush();
    void FrameDiscard();
    
private:
    FILE *outfile;
};

/* 
 * Local variables:
 *  c-file-style: "stroustrup"
 *  tab-width: 4
 *  indent-tabs-mode: nil
 * End:
 */
#endif
