
/*
 *  inptstrm.c:  Members of input stream classes related to muxing out into
 *               the output stream.
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


#include <config.h>
#include <assert.h>
#include "fastintfns.h"
#include "inputstrm.hh"
#include "outputstream.hh"

MuxStream::MuxStream() : init(false) 
{
}

void MuxStream::Init( const int strm_id, 
					  const unsigned int _buf_scale,
					  const unsigned int buf_size,
					  const unsigned int _zero_stuffing,
					  bool bufs_in_first, 
					  bool always_bufs
	) 
{
	stream_id = strm_id;
	nsec = 0;
	zero_stuffing = _zero_stuffing;
	buffer_scale = _buf_scale;
	buffer_size = buf_size;
	bufmodel.init( buf_size );
	buffers_in_header = bufs_in_first;
	always_buffers_in_header = always_bufs;
	new_au_next_sec = true;
	init = true;
}



unsigned int 
MuxStream::BufferSizeCode()
{
	if( buffer_scale == 1 )
		return buffer_size / 1024;
	else if( buffer_scale == 0 )
		return buffer_size / 128;
	else
		assert(false);
}



ElementaryStream::ElementaryStream( OutputStream &into, 
									stream_kind _kind) : 
//	MuxStream( stream_id,  buf_scale, buf_size, zero_stuffing ),
	muxinto( into ),
	kind(_kind),
    buffer_min(INT_MAX),
    buffer_max(1)
{
}


bool 
ElementaryStream::NextAU()
{
	Aunit *p_au = next();
	if( p_au != NULL )
	{
		au = p_au;
		au_unsent = p_au->length;
		return true;
	}
	else
	{
		au_unsent = 0;
		return false;
	}
}


Aunit *
ElementaryStream::Lookahead( )
{
	if( eoscan )
		return 0;
	else
		return aunits.lookahead();
}

unsigned int 
ElementaryStream::BytesToMuxAUEnd(unsigned int sector_transport_size)
{
	return (au_unsent/min_packet_data)*sector_transport_size +
		(au_unsent%min_packet_data)+(sector_transport_size-min_packet_data);
}

//
//  Read the (parsed and spliced) stream data from the stream
//  buffer.
//
unsigned int 
ElementaryStream::ReadStrm(uint8_t *dst, unsigned int to_read)
{
	return bs.read_buffered_bytes( dst, to_read );
}

bool ElementaryStream::MuxPossible()
{
	return (!RunOutComplete() &&
			bufmodel.space() > max_packet_data);
}

void ElementaryStream::UpdateBufferMinMax()
{
    buffer_min =  buffer_min < bufmodel.space() ? 
        buffer_min : bufmodel.space();
    buffer_max = buffer_max > bufmodel.space() ? 
        buffer_max : bufmodel.space();
}



clockticks ElementaryStream::RequiredDTS()
{
	return au->DTS+timestamp_delay;
}

void ElementaryStream::AllDemuxed()
{
	bufmodel.flushed();
}

void ElementaryStream::DemuxedTo( clockticks SCR )
{
	bufmodel.cleaned( SCR );
}

bool ElementaryStream::MuxCompleted()
{
	return au_unsent == 0;
}

void ElementaryStream::SetTSOffset( clockticks baseTS )
{
	SetSyncOffset( baseTS - au->DTS );
}

void 
ElementaryStream::SetSyncOffset( clockticks sync_offset )
{
	timestamp_delay = sync_offset;
}

Aunit *ElementaryStream::next()
{
	if( eoscan )
		return 0;
	else if( aunits.current()+FRAME_CHUNK > last_buffered_AU  )
	{
		FillAUbuffer(FRAME_CHUNK);
	}
			
	return aunits.next();
}



VideoStream::VideoStream(OutputStream &into )
	:
	ElementaryStream(into, ElementaryStream::video ),
	num_sequence(0),
	num_seq_end(0),
	num_pictures(0),
	num_groups(0),
	dtspts_for_all_au( into.dtspts_for_all_vau )
{
	prev_offset=0;
    decoding_order=0;
	fields_presented=0;
    group_order=0;
    group_start_pic=0;
	group_start_field=0;
    temporal_reference=0;
	pulldown_32 = 0;
	last_buffered_AU=0;
	max_bits_persec = 0;
	AU_hdr = SEQUENCE_HEADER;  /* GOP or SEQ Header starting AU? */
	for( int i =0; i<4; ++i )
		num_frames[i] = avg_frames[i] = 0;
			
}

void VideoStream::InitAUbuffer()
{
	int i;
	for( i = 0; i < aunits.BUF_SIZE; ++i )
		aunits.init( new VAunit );
}




AudioStream::AudioStream(OutputStream &into) : 
	ElementaryStream( into,  ElementaryStream::audio ),
	num_syncword(0)
{
	for( int i = 0; i <2 ; ++i )
		num_frames[i] = size_frames[i] = 0;
}

void AudioStream::InitAUbuffer()
{
	int i;
	for( i = 0; i < aunits.BUF_SIZE; ++i )
		aunits.init( new AAunit );
}

//
// Generator for padding packets in a padding stream...
//


unsigned int PaddingStream::ReadStrm(uint8_t *dst, unsigned int to_read)
{
	memset( dst, STUFFING_BYTE, to_read );
	return to_read;
}

unsigned int VCDAPadStream::ReadStrm(uint8_t *dst, unsigned int to_read)
{
	memset( dst, STUFFING_BYTE, to_read );
	return to_read;
}



/* 
 * Local variables:
 *  c-file-style: "stroustrup"
 *  tab-width: 4
 *  indent-tabs-mode: nil
 * End:
 */
