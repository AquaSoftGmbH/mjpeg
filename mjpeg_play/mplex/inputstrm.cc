/************************
 *
 * Class member function definitions for input stream class hierarchy
 *
 ***********************/

#include <assert.h>
#include "inputstrm.hh"

void MuxStream::SetMuxParams( unsigned int buf_size )
{
	bufmodel.init( buf_size );
	buffer_size = buf_size;
	init = true;
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

//
//  Compute the number of bytes that would have to be sent to complete
//  transmission current access unit if it *another* stream is muxed
//  next.
//

unsigned int MuxStream::BytesToMuxAUEnd(unsigned int sector_transport_size)
{

	return (au_unsent/min_packet_data)*sector_transport_size +
		(au_unsent%min_packet_data)+(sector_transport_size-min_packet_data);
	
}

