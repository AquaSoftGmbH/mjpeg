/************************
 *
 * Class member function definitions for input stream class hierarchy
 *
 ***********************/

#include <config.h>
#include <assert.h>
#include "inputstrm.hh"

void MuxStream::SetMuxParams( unsigned int buf_size )
{
	bufmodel.init( buf_size );
	buffer_size = buf_size;
	init = true;
}


void MuxStream::SetSyncOffset( clockticks sync_offset )
{
	timestamp_delay = sync_offset;
}

unsigned int MuxStream::BufferSizeCode()
{
	assert(init);
	if( buffer_scale == 1 )
		return buffer_size / 1024;
	else if( buffer_scale == 0 )
		return buffer_size / 128;
	else
		assert(false);
}


void VideoStream::InitAUbuffer()
{
	int i;
	mjpeg_info( "aunits.cur_wr = %d\n", aunits.cur_wr );
	for( i = 0; i < aunits.BUF_SIZE; ++i )
		aunits.init( new VAunit );
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
