
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
#include "videostrm.hh"
#include "outputstream.hh"


VideoStream::VideoStream(OutputStream &into )
	:
	ElementaryStream(into, ElementaryStream::video ),
	num_sequence(0),
	num_seq_end(0),
	num_pictures(0),
	num_groups(0),
	dtspts_for_all_au( into.dtspts_for_all_vau ),
    gop_control_packet( false )
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
    FRAME_CHUNK = 6;
		
}

void VideoStream::InitAUbuffer()
{
	int i;
	for( i = 0; i < aunits.BUF_SIZE; ++i )
		aunits.init( new VAunit );
}


/*********************************
 * Signals when video stream has completed mux run-out specified
 * in associated mux stream.   Run-out is always to complete GOP's.
 *********************************/

bool VideoStream::RunOutComplete()
{
	return (au_unsent == 0 || 
			( muxinto.running_out &&
			  au->type == IFRAME && au->PTS >= muxinto.runout_PTS));
}


/*********************************
 * Work out the timestamps to be set in the header of sectors starting
 * new AU's.
 *********************************/

uint8_t VideoStream::NewAUTimestamps( int AUtype )
{
	uint8_t timestamps;
    if( AUtype == BFRAME)
        timestamps=TIMESTAMPBITS_PTS;
    else 
        timestamps=TIMESTAMPBITS_PTS_DTS;

    if( muxinto.timestamp_iframe_only && AUtype != IFRAME)
        timestamps=TIMESTAMPBITS_NO;
    return timestamps;
}

/*********************************
 * Work out the buffer records to be set in the header of sectors
 * starting new AU's.
 *********************************/

bool VideoStream::NewAUBuffers( int AUtype )
{
    return buffers_in_header & 
        !(muxinto.video_buffers_iframe_only && AUtype != IFRAME);
}

/******************************************************************
	Output_Video
	generiert Pack/Sys_Header/Packet Informationen aus dem
	Video Stream und speichert den so erhaltenen Sektor ab.

	generates Pack/Sys_Header/Packet information from the
	video stream and writes out the new sector
******************************************************************/

void VideoStream::OutputSector ( )

{

	unsigned int max_packet_payload; 	 
	unsigned int actual_payload;
	unsigned int prev_au_tail;
	VAunit *vau;
	unsigned int old_au_then_new_payload;
	clockticks  DTS,PTS;
    int autype;

	max_packet_payload = 0;	/* 0 = Fill sector */
  	/* 	
	   We're now in the last AU of a segment. 
		So we don't want to go beyond it's end when filling
		sectors. Hence we limit packet payload size to (remaining) AU length.
		The same applies when we wish to ensure sequence headers starting
		ACCESS-POINT AU's in (S)VCD's etc are sector-aligned.
	*/
	int nextAU = NextAUType();
	if( (muxinto.running_out && nextAU == IFRAME && 
		 Lookahead()->PTS >= muxinto.runout_PTS) ||
		(muxinto.sector_align_iframeAUs && nextAU == IFRAME )
		) 
	{
		max_packet_payload = au_unsent;
	}

	/* Figure out the threshold payload size below which we can fit more
	   than one AU into a packet N.b. because fitting more than one in
	   imposses an overhead of additional header fields so there is a
	   dead spot where we *have* to stuff the packet rather than start
	   fitting in an extra AU.  Slightly over-conservative in the case
	   of the last packet...  */

	old_au_then_new_payload = muxinto.PacketPayload( *this,
													 buffers_in_header, 
													 true, true);
	PTS = au->PTS + timestamp_delay;
	DTS = au->DTS + timestamp_delay;


	/* CASE: Packet starts with new access unit			*/
	if (new_au_next_sec  )
	{
        autype = AUType();
        //
        // Some types of output format (e.g. DVD) require special
        // control sectors before the sector starting a new GOP
        // N.b. this implies muxinto.sector_align_iframeAUs
        //
        if( gop_control_packet && autype == IFRAME )
        {
            OutputGOPControlSector();
        }

		if( dtspts_for_all_au && max_packet_payload == 0 )
			max_packet_payload = au_unsent;
		actual_payload =
			muxinto.WritePacket ( max_packet_payload,
								  *this,
								  NewAUBuffers(autype), 
                                  PTS, DTS,
								  NewAUTimestamps(autype) );
		Muxed( actual_payload);

	}

	/* CASE: Packet begins with old access unit, no new one	*/
	/*	     begins in the very same packet					*/

	else if ( ! new_au_next_sec &&
			  (au_unsent >= old_au_then_new_payload))
	{
		actual_payload = 
			muxinto.WritePacket( au_unsent,
								  *this,
								  false, 0, 0,
								  TIMESTAMPBITS_NO );
		Muxed ( actual_payload );

	}

	/* CASE: Packet begins with old access unit, a new one	*/
	/*	     could begin in the very same packet			*/
	else /* if ( !new_au_next_sec  && 
			(au_unsent < old_au_then_new_payload)) */
	{
		prev_au_tail = au_unsent;

		Muxed( au_unsent );
		/* is there a new access unit anyway? */
		if( !MuxCompleted() )
		{
            autype = AUType();
            /* No timestamps if sector restricted to only current */
            /* AU (new Au though possible is not muxed)           */
            uint8_t timestamps = 
                max_packet_payload != 0 && prev_au_tail >= max_packet_payload
                ? TIMESTAMPBITS_NO 
                : NewAUTimestamps(autype) ;

			if(  dtspts_for_all_au  && max_packet_payload == 0 )
				max_packet_payload = prev_au_tail + au_unsent;

			PTS = au->PTS + timestamp_delay;
			DTS = au->DTS + timestamp_delay;

			actual_payload = 
				muxinto.WritePacket ( max_packet_payload,
									  *this,
									  NewAUBuffers(autype), 
                                      PTS, DTS,
									  timestamps );
			Muxed( actual_payload - prev_au_tail );
		} 
		else
		{
			(void) muxinto.WritePacket ( 0,
										 *this,
										 false, 0, 0,
										 TIMESTAMPBITS_NO);
		};


	}
	++nsec;
	buffers_in_header = always_buffers_in_header;
}


/***********************************************
   OutputControlSector - Write control sectors prefixing a GOP
   For "normal" video streams this doesn't happen and so represents
   a bug and triggers an abort.

   In DVD's these sectors carry a system header and what is
   presumably indexing and/or sub-title information in
   private_stream_2 packets.  I have no idea what to put in here so we
   simply pad the sector out.
***********************************************/

void VideoStream::OutputGOPControlSector()
{
    abort();
}

void DVDVideoStream::OutputGOPControlSector()
{
    uint8_t dummy[1];
    muxinto.WriteRawSector( muxinto.SystemHeader(), dummy, 0U );
}


/* 
 * Local variables:
 *  c-file-style: "stroustrup"
 *  tab-width: 4
 *  indent-tabs-mode: nil
 * End:
 */
