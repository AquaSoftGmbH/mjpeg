
/*
 *  inptstrm.hpp:  Input stream classes for MPEG multiplexing
 *
 *  Copyright (C) 2001 Andrew Stevens <andrew.stevens@philips.com>
 *
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU General Public License
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef __INPUTSTRM_H__
#define __INPUTSTRM_H__

#include <config.h>
#include <stdio.h>
#include <vector>
#include <sys/stat.h>

#include "mjpeg_types.h"
#include "mpegconsts.h"
#include "format_codes.h"
#include "mjpeg_logging.h"

#include "mplexconsts.hpp"
#include "bits.hpp"
#include "aunit.hpp"
#include "vector.hpp"
#include "buffer.hpp"

using std::vector;

class InputStream
{
public:
	InputStream( IBitStream &istream ) :
		stream_length(0),
        bs( istream ),
		eoscan(false),
		last_buffered_AU(0),
		decoding_order(0),
		old_frames(0)
		{}

	void SetBufSize( unsigned int buf_size )
		{
			bs.SetBufSize( buf_size );
		}

    bitcount_t stream_length;

protected:
	off_t      file_length;
    IBitStream &bs;
	bool eoscan;
	
	unsigned int last_buffered_AU;		// decode seq num of last buffered frame + 1
   	bitcount_t AU_start;
    uint32_t  syncword;
    bitcount_t prev_offset;
    unsigned int decoding_order;
    unsigned int old_frames;

};

//
// Abstract forward reference...
//

class Multiplexor;


class MuxStream 
{
public:
	MuxStream();

    void Init( const int strm_id,
               const unsigned int _buf_scale,
			   const unsigned int buf_size,
			   const unsigned int _zero_stuffing,
			   const bool bufs_in_first, 
			   const bool always_bufs
		  );

	unsigned int BufferSizeCode();
	inline unsigned int BufferSize() { return buffer_size; }
	inline unsigned int BufferScale() { return buffer_scale; }

	
	inline void SetMaxPacketData( unsigned int max )
		{
			max_packet_data = max;
		}
	inline void SetMinPacketData( unsigned int min )
		{
			min_packet_data = min;
		}
	inline unsigned int MaxPacketData() { return max_packet_data; }
	inline unsigned int MinPacketData() { return min_packet_data; }
    inline bool NewAUNextSector() { return new_au_next_sec; }

	//
	//  Read the next packet payload (sub-stream headers plus
	//  parsed and spliced stream data) for a packet with the
	//  specified payload capacity.  Update the AU info.
	//

	virtual unsigned int ReadPacketPayload(uint8_t *dst, unsigned int to_read) = 0;

    //
    // Return the size of the substream headers...
    //
    virtual unsigned int StreamHeaderSize() { return 0; }

public:  // TODO should go protected once encapsulation complete
	int        stream_id;
	unsigned int    buffer_scale;
	unsigned int 	buffer_size;
	BufferModel bufmodel;
	unsigned int 	max_packet_data;
	unsigned int	min_packet_data;
	unsigned int    zero_stuffing;
	unsigned int    nsec;
	bool buffers_in_header;
	bool always_buffers_in_header;
	bool new_au_next_sec;
	bool init;
};

class DummyMuxStream : public MuxStream
{
public:
    DummyMuxStream( const int strm_id,
                    const unsigned int buf_scale, 
                    unsigned int buf_size )
        {
            stream_id = strm_id;
            buffer_scale = buf_scale;
            buffer_size = buf_size;
        }

    unsigned int ReadPacketPayload(uint8_t *dst, unsigned int to_read)
        {
            abort();
            return 0;
        }
};


class ElementaryStream : public InputStream,
						 public MuxStream
{
public:
	enum stream_kind { audio, video, dummy };
	ElementaryStream( IBitStream &ibs,
                      Multiplexor &into, 
					  stream_kind kind
					  );
	virtual void Close() = 0;

	bool NextAU();
	Aunit *Lookahead();
	unsigned int BytesToMuxAUEnd(unsigned int sector_transport_size);
	bool MuxCompleted();
	virtual bool MuxPossible(clockticks currentSCR );
	void DemuxedTo( clockticks SCR );
	void SetTSOffset( clockticks baseTS );
	void AllDemuxed();
	inline stream_kind Kind() { return kind; }
    inline unsigned int BufferMin() { return buffer_min; }
    inline unsigned int BufferMax() { return buffer_max; }
    inline clockticks BaseDTS() { return au->DTS; };
    inline clockticks BasePTS() { return au->PTS; };
    inline clockticks RequiredDTS() { return au->DTS + timestamp_delay; };
    inline clockticks RequiredPTS() { return au->PTS + timestamp_delay; };
    inline int        DecodeOrder() { return au->dorder; }
    inline clockticks NextRequiredDTS()
        { 
            Aunit *next = Lookahead();
            if( next != 0 )
                return next->DTS + timestamp_delay; 
            else
                return 0;
        };
    inline clockticks NextRequiredPTS()
        { 
            Aunit *next = Lookahead();
            if( next != 0 )
                return next->PTS + timestamp_delay; 
            else
                return 0;
        };

    void UpdateBufferMinMax();

	void SetSyncOffset( clockticks timestamp_delay );

	void BufferAndOutputSector();
 
	inline bool BuffersInHeader() { return buffers_in_header; }
	virtual unsigned int NominalBitRate() = 0;
	virtual bool RunOutComplete() = 0;

	virtual unsigned int ReadPacketPayload(uint8_t *dst, unsigned int to_read);

    bitcount_t bytes_read;
protected:
	virtual void FillAUbuffer(unsigned int frames_to_buffer) = 0;
	virtual void InitAUbuffer() = 0;
    virtual bool AUBufferNeedsRefill() = 0;
    virtual void OutputSector() = 0;
	AUStream aunits;
	void Muxed( unsigned int bytes_muxed );
	Aunit *au;
	clockticks timestamp_delay;

	unsigned int au_unsent;
	Aunit *next();
	Multiplexor &muxinto;
	stream_kind kind;
    unsigned int buffer_min;
    unsigned int buffer_max;
    int FRAME_CHUNK;
									
};



#endif // __INPUTSTRM_H__


/* 
 * Local variables:
 *  c-file-style: "stroustrup"
 *  tab-width: 4
 *  indent-tabs-mode: nil
 * End:
 */
