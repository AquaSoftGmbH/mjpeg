/************************
 *
 * Class member function definitions for input stream class hierarchy
 *
 ***********************/

#include <config.h>
#include <assert.h>
#include "inputstrm.hh"
#include "outputstream.hh"

MuxStream::MuxStream( const int strm_id, 
					  const unsigned int _buf_scale,
					  const unsigned int buf_size,
					  const unsigned int _zero_stuffing ) : 
	stream_id(strm_id), 
	nsec(0),
	zero_stuffing( _zero_stuffing ),
	buffer_scale( _buf_scale ),
	buffer_size( buf_size )
{
	bufmodel.init( buf_size );
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



ElementaryStream::ElementaryStream( OutputStream &into, const int stream_id,
									const unsigned int zero_stuffing,
									const unsigned int buf_scale,
									const unsigned int buf_size,
									bool bufs_in_first, bool always_bufs,
									stream_kind _kind) : 
	MuxStream( stream_id,  buf_scale, buf_size, zero_stuffing ),
	muxinto( into ),
	buffers_in_header( bufs_in_first ),
	always_buffers_in_header( always_bufs ),
	kind(_kind),
	new_au_next_sec(true)

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
	SetSyncOffset( baseTS - au->PTS );
}

void 
ElementaryStream::SetSyncOffset( clockticks sync_offset )
{
	timestamp_delay = sync_offset;
}

Aunit *ElementaryStream::next()
{
	if( !eoscan && aunits.current()+FRAME_CHUNK > last_buffered_AU  )
	{
		FillAUbuffer(FRAME_CHUNK);
	}
			
	return aunits.next();
}



VideoStream::VideoStream(OutputStream &into, const int stream_num)
	:
	ElementaryStream(into, VIDEO_STR_0+stream_num,
					 0,  // Zero stuffing
					 1,  // Buffer scale
					 into.video_buffer_size,
					 into.buffers_in_video,
					 into.always_buffers_in_video,
					 ElementaryStream::video
					 ),
	num_sequence(0),
	num_seq_end(0),
	num_pictures(0),
	num_groups(0),
	dtspts_for_all_au( into.dtspts_for_all_vau )
{
	mjpeg_debug( "SETTING video buffer to %d\n", into.video_buffer_size );

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

AudioStream::AudioStream(OutputStream &into, const int stream_num) : 
	ElementaryStream( into, AUDIO_STR_0 + stream_num, 
					  into.vcd_zero_stuffing,
					  0,  // Buffer scale
					  into.audio_buffer_size,
					  into.buffers_in_audio,
					  into.always_buffers_in_audio,
					  ElementaryStream::audio
		),
	num_syncword(0)
{
	mjpeg_debug( "SETTING zero stuff to %d\n", into.vcd_zero_stuffing );
	mjpeg_debug( "SETTING audio buffer to %d\n", into.audio_buffer_size );

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


//
//  Generator for end-of-stream marker packets...
// 

unsigned int EndMarkerStream::ReadStrm(uint8_t *dst, unsigned int to_read)
{
	uint8_t *end_marker = &dst[to_read-4];
	memset( dst, STUFFING_BYTE, to_read-4 );
	end_marker[0] = static_cast<uint8_t>((ISO11172_END)>>24);
	end_marker[1] = static_cast<uint8_t>((ISO11172_END & 0x00ff0000)>>16);
	end_marker[2] = static_cast<uint8_t>((ISO11172_END & 0x0000ff00)>>8);
	end_marker[3] = static_cast<uint8_t>(ISO11172_END & 0x000000ff);
	
	return to_read;
}
