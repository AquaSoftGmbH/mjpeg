
/*
 *  inptstrm.c:  Members of audi stream classes related to muxing out into
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
#include "audiostrm.hh"
#include "outputstream.hh"



AudioStream::AudioStream(OutputStream &into) : 
	ElementaryStream( into,  ElementaryStream::audio ),
	num_syncword(0)
{
    FRAME_CHUNK = 24;
	for( int i = 0; i <2 ; ++i )
		num_frames[i] = size_frames[i] = 0;
}

void AudioStream::InitAUbuffer()
{
	int i;
	for( i = 0; i < aunits.BUF_SIZE; ++i )
		aunits.init( new AAunit );
}



/*********************************
 * Signals when audio stream has completed mux run-out specified
 * in associated mux stream. 
 *********************************/

bool AudioStream::RunOutComplete()
{
	return (au_unsent == 0 || 
			( muxinto.running_out && au->PTS >= muxinto.runout_PTS));
}


/******************************************************************
	Output_Audio
	generates Pack/Sys Header/Packet information from the
	audio stream and saves them into the sector
******************************************************************/

void AudioStream::OutputSector ( )

{
	clockticks   PTS;
	unsigned int max_packet_data; 	 
	unsigned int actual_payload;
	unsigned int bytes_sent;
	AAunit *aau;
	Pack_struc pack;
	unsigned int old_au_then_new_payload;

	PTS = au->DTS + timestamp_delay;
	old_au_then_new_payload = 
		muxinto.PacketPayload( *this, buffers_in_header, false, false );

	max_packet_data = 0;
	if( muxinto.running_out && 
		(Lookahead() != 0 && Lookahead()->PTS > muxinto.runout_PTS) )
	{
		/* We're now in the last AU of a segment.  So we don't want to
		   go beyond it's end when writing sectors. Hence we limit
		   packet payload size to (remaining) AU length.
		*/
		max_packet_data = au_unsent;
	}
  
    
	/* CASE: packet starts with new access unit			*/
	
	if (new_au_next_sec)
    {
		actual_payload = 
			muxinto.WritePacket ( max_packet_data,
								  *this,
								  buffers_in_header, PTS, 0,
								  TIMESTAMPBITS_PTS);

		Muxed( actual_payload );
    }


	/* CASE: packet starts with old access unit, no new one	*/
	/*       starts in this very same packet			*/
	else if (!(new_au_next_sec) && 
			 (au_unsent >= old_au_then_new_payload))
    {
		actual_payload = 
			muxinto.WritePacket ( max_packet_data,
								  *this,
								  buffers_in_header, 0, 0,
								  TIMESTAMPBITS_NO );
		Muxed( actual_payload );
    }


	/* CASE: packet starts with old access unit, a new one	*/
	/*       starts in this very same packet			*/
	else /* !(new_au_next_sec) &&  (au_unsent < old_au_then_new_payload)) */
    {
		bytes_sent = au_unsent;
		Muxed(bytes_sent);
		/* gibt es ueberhaupt noch eine Access Unit ? */
		/* is there another access unit anyway ? */
		if( !MuxCompleted()  )
		{
			PTS = au->DTS + timestamp_delay;
			actual_payload = 
				muxinto.WritePacket ( max_packet_data,
									  *this,
									  buffers_in_header, PTS, 0,
									  TIMESTAMPBITS_PTS );

			Muxed( actual_payload - bytes_sent );
		} 
		else
		{
			muxinto.WritePacket ( 0,
								  *this,
								  buffers_in_header, 0, 0,
								  TIMESTAMPBITS_NO );
		};
		
    }

    ++nsec;

	buffers_in_header = always_buffers_in_header;
	
}



/* 
 * Local variables:
 *  c-file-style: "stroustrup"
 *  tab-width: 4
 *  indent-tabs-mode: nil
 * End:
 */
