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

#include <config.h>
#include "mjpeg_types.h"

class EncoderParams;

/******************************
 *
 * Elementry stream buffering state: used to hold the state of output buffer
 * at marked points prior to flushing.
 *
 *****************************/

class ElemStrmBufferState
{
protected:
    int64_t bytecnt;            // Bytes flushed and pending
    int32_t unflushed;          // Unflushed bytes in buffer
    int outcnt;                 // Bits unwritten in current output byte
                                // (buffer[unflushed]).
public:
    int serial_id;              // We count flushes so we can detect
                                // attempts to rewind back to flushed
                                // buffer states
};

class ElemStrmWriter : public ElemStrmBufferState
{
public:
	ElemStrmWriter( EncoderParams &encoder );
    ~ElemStrmWriter();
    
    /********************
     *
     * Return a buffer state that we can restore back to (provided no flush
     * has take place since then!)
     *
     * N.b. attempts to mark states that are not byte-aligned are illegal
     * and will abort
     *
     *******************/

    ElemStrmBufferState CurrentState();

    /**************
     *
     * Flush out buffer (buffer states recorded up to this point can no
     * longer be restored).
     * N.b. attempts to flush in non byte-aligned states are illegal
     * and will abort
     *
     *************/
    void FlushBuffer();

    /**************
     *
     * Restore output state (including the output bit count)
     * back to the specified state.
     *
     *************/

    void RestoreState( const ElemStrmBufferState &restore );

    /**************
     *
     * Write rightmost n (0<=n<=32) bits of val to outfile 
     *
     *************/
    void PutBits( uint32_t val, int n);

    void AlignBits();
    inline bool Aligned() { return outcnt == 8; }
    inline int64_t BitCount() { return 8LL*bytecnt + (8-outcnt); }

    
protected:
    virtual void WriteOutBufferUpto( const size_t flush_upto ) = 0;
private:
    void ExpandBuffer();

protected:
    uint8_t *buffer;            // Output buffer - used to hold byte
                                // aligned output before flushing or
                                // backing up and re-encoding
    uint32_t buffer_size;
private:
	EncoderParams &encparams;
    uint32_t pendingbits;
    int last_flushed_serial_id; // Serial Id of buffer state flushed last
};


/* 
 * Local variables:
 *  c-file-style: "stroustrup"
 *  tab-width: 4
 *  indent-tabs-mode: nil
 * End:
 */
#endif
