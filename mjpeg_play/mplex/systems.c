#include "main.h"
#include <string.h>

/* Packet payload
	compute how much payload a sector-sized packet with the specified headers could carry...
	*/
	
		/* TODO: are the packet payload calculations correct if some stream is missing? */
		
int packet_payload( Sys_header_struc *sys_header, Pack_struc *pack_header, int buffers, int PTSstamp, int DTSstamp )
{
	int payload = sector_size - PACKET_HEADER_SIZE ;
	if( sys_header != NULL )
		payload -= sys_header->length;
	if( buffers )
		payload -=  BUFFERINFO_LENGTH;
	if( opt_mpeg == 2 )
	{
		payload -= MPEG2_AFTER_PACKET_LENGTH_MIN;
		if( pack_header != NULL )
			payload -= pack_header->length;
		if( DTSstamp )
			payload -= DTS_PTS_TIMESTAMP_LENGTH;
		if ( PTSstamp )
			payload -= DTS_PTS_TIMESTAMP_LENGTH;
	}
	else
	{
		payload -= MPEG1_AFTER_PACKET_LENGTH_MIN;
		if( pack_header != NULL )
			payload -= pack_header->length;
		if( DTSstamp )
			payload -= DTS_PTS_TIMESTAMP_LENGTH;
		if (PTSstamp )
			payload -= DTS_PTS_TIMESTAMP_LENGTH;
		if( DTSstamp || PTSstamp )
			payload += 1;  /* No need for nostamp marker ... */

	}
	
	return payload;
}

/*************************************************************************
	Create_Sector
	erstellt einen gesamten Sektor.
	Kopiert in dem Sektorbuffer auch die eventuell vorhandenen
	Pack und Sys_Header Informationen und holt dann aus der
	Inputstreamdatei einen Packet voll von Daten, die im
	Sektorbuffer abgelegt werden.

	creates a complete sector.
	Also copies Pack and Sys_Header informations into the
	sector buffer, then reads a packet full of data from
	the input stream into the sector buffer.
	
	N.b. note that we allow for situations where want to
	deliberately reduce the payload carried by stuffing.
	This allows us to deal with tricky situations where the
	header overhead of adding in additional information
	would exceed the remaining payload capacity.
*************************************************************************/

void create_sector (Sector_struc 	 *sector,
					Pack_struc	 *pack,
					Sys_header_struc *sys_header,
					unsigned int     max_packet_data_size,
					FILE		 *inputstream,

					unsigned char 	 type,
					unsigned char 	 buffer_scale,
					unsigned int 	 buffer_size,
					unsigned char 	 buffers,
					Timecode_struc   *PTS,
					Timecode_struc   *DTS,
					unsigned char 	 timestamps
	)

{
    int i,j;
    unsigned char *index;
    unsigned char *size_offset;
	unsigned char *packet_header_start_offset;
	unsigned char *pes_header_len_offset = 0; /* Silence compiler... */
	int target_packet_data_size;
	int actual_packet_data_size;
	int packet_data_to_read;
	int bytes_short;

    index = sector->buf;
    sector->length_of_sector=0;

    /* soll ein Pack Header mit auf dem Sektor gespeichert werden? */
    /* Should we copy Pack Header information ? */

    if (pack != NULL)
    {
		bcopy (pack->buf, index, pack_header_size);
		index += pack_header_size;
		sector->length_of_sector += pack_header_size;
    }

    /* soll ein System Header mit auf dem Sektor gespeichert werden? */
    /* Should we copy System Header information ? */

    if (sys_header != NULL)
    {
		bcopy (sys_header->buf, index, sys_header->length);
		index += sys_header->length;
		sector->length_of_sector += sys_header->length;
    }

    /* konstante Packet Headerwerte eintragen */
    /* write constant packet header data */
	packet_header_start_offset = index;
    *(index++) = (unsigned char)(PACKET_START)>>16;
    *(index++) = (unsigned char)(PACKET_START & 0x00ffff)>>8;
    *(index++) = (unsigned char)(PACKET_START & 0x0000ff);
    *(index++) = type;	

    /* wir merken uns diese Position, falls sich an der Paketlaenge noch was tut */
    /* we remember this offset so we can fill in the packet size field once
	   we know the actual size... */
    size_offset = index;   
	index += 2;
 
 	if( PTS != NULL && DTS != NULL )
		printf( "V DTS=%lld PTS=%lld\n", DTS->thetime/300, PTS->thetime/300);


	if( opt_mpeg == 1 )
	{
		/* MPEG-1: buffer information */
		if (buffers)
		{
			*(index++) = (unsigned char) (0x40 |
										  (buffer_scale << 5) | (buffer_size >> 8));
			*(index++) = (unsigned char) (buffer_size & 0xff);
		}

		/* MPEG-1: PTS, PTS & DTS, oder gar nichts? */
		/* should we write PTS, PTS & DTS or nothing at all ? */

		switch (timestamps)
		{
		case TIMESTAMPBITS_NO:
			*(index++) = MARKER_NO_TIMESTAMPS;
			break;
		case TIMESTAMPBITS_PTS:
			buffer_dtspts_mpeg1scr_timecode (PTS, MARKER_JUST_PTS, &index);
			copy_timecode (PTS, &sector->TS);
			break;
		case TIMESTAMPBITS_PTS_DTS:
			buffer_dtspts_mpeg1scr_timecode (PTS, MARKER_PTS, &index);
			buffer_dtspts_mpeg1scr_timecode (DTS, MARKER_DTS, &index);
			copy_timecode (DTS, &sector->TS);
			break;
		}
	}
	else
	{
	  	/* MPEG-2 packet syntax header flags. */
		/* First byte:
		   <1,0><PES_scrambling_control:2=0><data_alignment_ind.=0>
		   <copyright=0><original=0> */
		*(index++) = 0x80;
		/* Second byte: PTS PTS_DTS or neither?  Buffer info?
		   <PTS_DTS:2><ESCR=0><ES_rate=0>
		   <DSM_trick_mode:2=0><PES_CRC=0><PES_extension=(!!buffers)>
		*/
		*(index++) = (timestamps << 6) | (!!buffers);
		/* Third byte:
		   <PES_header_length:8> */
		pes_header_len_offset = index;  /* To fill in later! */
		index++;
		/* MPEG-2: the timecodes if required */
		switch (timestamps)
		{
		case TIMESTAMPBITS_PTS:
			buffer_dtspts_mpeg1scr_timecode(PTS, MARKER_JUST_PTS, &index);
			copy_timecode(PTS, &sector->TS);
			break;

		case TIMESTAMPBITS_PTS_DTS:
			buffer_dtspts_mpeg1scr_timecode(PTS, MARKER_PTS, &index);
			buffer_dtspts_mpeg1scr_timecode(DTS, MARKER_DTS, &index);
			copy_timecode(DTS, &sector->TS);
			break;
		}

		/* MPEG-2 The buffer information in a PES_extension */
		if( buffers )
		{
			/* MPEG-2 PES extension header
			   <PES_private_data:1=0><pack_header_field=0>
			   <program_packet_sequence_counter=0>
			   <P-STD_buffer=1><reserved:3=1><{PES_extension_flag_2=0> */
			*(index++) = (unsigned char) (0x40 | (buffer_scale << 5) | 
										  (buffer_size >> 8));
			*(index++) = (unsigned char) (buffer_size & 0xff);
		}
	}

	/* MPEG-1, MPEG-2: data available to be filled is packet_size less header and MPEG-1 trailer... */

	target_packet_data_size = sector_size - (index - sector->buf);
	
	/* If a maximum payload data size is specified (!=0) and is smaller than the space available
	   thats all we read (the remaining space is stuffed) */
	if( max_packet_data_size != 0 && max_packet_data_size < target_packet_data_size )
		packet_data_to_read = max_packet_data_size;
	else
		packet_data_to_read = target_packet_data_size;


	/* TODO DEBUG: */		
	if( target_packet_data_size != packet_payload( sys_header, pack, buffers,
												   timestamps & TIMESTAMPBITS_PTS, timestamps & TIMESTAMPBITS_DTS) )
	{ 
		printf("\nPacket size calculation error %d S%d P%d B%d %d %d!\n ", timestamps,
			   sys_header!=0, pack!=0, buffers,
			   target_packet_data_size , 
			   packet_payload( sys_header, pack, buffers,
							   timestamps & TIMESTAMPBITS_PTS, timestamps & TIMESTAMPBITS_DTS));
		exit(1);
	}
	
	/* MPEG-1, MPEG-2: read in available packet data ... */
    
    if (type == PADDING_STR)
    {
		for (j=0; j<target_packet_data_size; j++)
			*(index+j)=(unsigned char) STUFFING_BYTE;
		actual_packet_data_size = target_packet_data_size;
		bytes_short = 0;
    } 
    else
    {   
		actual_packet_data_size = fread (index, sizeof (unsigned char), 
										 packet_data_to_read, 
										 inputstream);
		bytes_short = target_packet_data_size - actual_packet_data_size;

	}
	
	/* Handle the situations where we don't have enough data to fill
	   the packet size fully at the end of the stream... */


	/* The case where we have fallen short, but only so much that we
	   can deal with it by stuffing (max 255) */
	/* TODO: MPEG-1: is it o.k. to merrily stuff like this...?        */
	if( bytes_short < 25 && bytes_short > 0 )
	{
		memmove( index+bytes_short, index,  actual_packet_data_size );
		for( j=0; j< bytes_short; ++j)
			*(index+j)=(unsigned char) STUFFING_BYTE;

		actual_packet_data_size = target_packet_data_size;
		bytes_short = 0;
	}
	  
	/* MPEG-2: we now know the header length... */
	if (opt_mpeg == 2 )
	{
		*pes_header_len_offset = 
			(unsigned char)(index-(pes_header_len_offset+1));
	}
	
	index += actual_packet_data_size;	 

	/* The case where we have fallen short enough to allow it to be dealt with by
	   inserting a stuffing packet... */	
	if ( bytes_short != 0 )
	{
		*(index++) = (unsigned char)(PACKET_START)>>16;
		*(index++) = (unsigned char)(PACKET_START & 0x00ffff)>>8;
		*(index++) = (unsigned char)(PACKET_START & 0x0000ff);
		*(index++) = PADDING_STR;
		*(index++) = (unsigned char)((bytes_short - 6) >> 8);
		*(index++) = (unsigned char)((bytes_short - 6) & 0xff);
		if (opt_mpeg == 2)
		{
			for (i = 0; i < bytes_short - 6; i++)
				*(index++) = (unsigned char) STUFFING_BYTE;
		}
		else
		{
			*(index++) = 0x0F;  /* TODO: A.Stevens 2000 Why is this here? */
			for (i = 0; i < bytes_short - 7; i++)
				*(index++) = (unsigned char) STUFFING_BYTE;
		}
		actual_packet_data_size = target_packet_data_size;
		bytes_short = 0;
	}
	  
	/* MPEG-1, MPEG-2: Now we know that actual packet size */
	size_offset[0] = (unsigned char)((index-size_offset-2)>>8);
	size_offset[1] = (unsigned char)((index-size_offset-2)&0xff);

    sector->length_of_sector = sector_size - bytes_short;
    sector->length_of_packet_data = actual_packet_data_size;

}

/*************************************************************************
	Create_Pack
	erstellt in einem Buffer die spezifischen Pack-Informationen.
	Diese werden dann spaeter von der Sector-Routine nochmals
	in dem Sektor kopiert.

	writes specifical pack header information into a buffer
	later this will be copied from the sector routine into
	the sector buffer
*************************************************************************/

void create_pack (
	Pack_struc	 *pack,
	Timecode_struc   *SCR,
	unsigned int 	 mux_rate
	)
{
    unsigned char *index;

    index = pack->buf;

    *(index++) = (unsigned char)((PACK_START)>>24);
    *(index++) = (unsigned char)((PACK_START & 0x00ff0000)>>16);
    *(index++) = (unsigned char)((PACK_START & 0x0000ff00)>>8);
    *(index++) = (unsigned char)(PACK_START & 0x000000ff);
        
	if (opt_mpeg == 2)
    {
    	/* Annoying: MPEG-2's SCR pack header time is different from
		   all the rest... */
		buffer_mpeg2scr_timecode(SCR, &index);
		*(index++) = (unsigned char)(mux_rate >> 14);
		*(index++) = (unsigned char)(0xff & (mux_rate >> 6));
		*(index++) = (unsigned char)(0x03 | ((mux_rate & 0x3f) << 2));
		*(index++) = (unsigned char)(RESERVED_BYTE << 3 | 0); /* No pack stuffing */
    }
    else
    {
		buffer_dtspts_mpeg1scr_timecode(SCR, MARKER_MPEG1_SCR, &index);
		*(index++) = (unsigned char)(0x80 | (mux_rate >> 15));
		*(index++) = (unsigned char)(0xff & (mux_rate >> 7));
		*(index++) = (unsigned char)(0x01 | ((mux_rate & 0x7f) << 1));
    }
    copy_timecode(SCR, &pack->SCR);
    /* TODO: Replace with dynamic calculation */
    pack->length = pack_header_size;
}


/*************************************************************************
	Create_Sys_Header
	erstelle in einem Buffer die spezifischen Sys_Header
	Informationen. Diese werden spaeter von der Sector-Routine
	nochmals zum Sectorbuffer kopiert.

	writes specifical system header information into a buffer
	later this will be copied from the sector routine into
	the sector buffer
	RETURN: Length of header created...
*************************************************************************/

void create_sys_header (
	Sys_header_struc *sys_header,
	unsigned int	 rate_bound,
	unsigned char	 audio_bound,
	unsigned char	 fixed,
	unsigned char	 CSPS,
	unsigned char	 audio_lock,
	unsigned char	 video_lock,
	unsigned char	 video_bound,

	unsigned char 	 stream1,
	unsigned char 	 buffer1_scale,
	unsigned int 	 buffer1_size,
	unsigned char 	 stream2,
	unsigned char 	 buffer2_scale,
	unsigned int 	 buffer2_size,
	unsigned int     which_streams
	)

{
    unsigned char *index;
	unsigned char *len_index;
	int system_header_size;
    index = sys_header->buf;

    /* if we are not using both streams, we should clear some
       options here */

    if (!(which_streams & STREAMS_AUDIO))
		audio_bound = 0;
    if (!(which_streams & STREAMS_VIDEO))
		video_bound = 0;

    *(index++) = (unsigned char)((SYS_HEADER_START)>>24);
    *(index++) = (unsigned char)((SYS_HEADER_START & 0x00ff0000)>>16);
    *(index++) = (unsigned char)((SYS_HEADER_START & 0x0000ff00)>>8);
    *(index++) = (unsigned char)(SYS_HEADER_START & 0x000000ff);

	len_index = index;	/* Skip length field for now... */
	index +=2;

    *(index++) = (unsigned char)(0x80 | (rate_bound >>15));
    *(index++) = (unsigned char)(0xff & (rate_bound >> 7));
    *(index++) = (unsigned char)(0x01 | ((rate_bound & 0x7f)<<1));
    *(index++) = (unsigned char)((audio_bound << 2)|(fixed << 1)|CSPS);
    *(index++) = (unsigned char)((audio_lock << 7)|
								 (video_lock << 6)|0x20|video_bound);

    *(index++) = (unsigned char)RESERVED_BYTE;

    if (which_streams & STREAMS_AUDIO) {
		*(index++) = stream1;
		*(index++) = (unsigned char) (0xc0 |
									  (buffer1_scale << 5) | (buffer1_size >> 8));
		*(index++) = (unsigned char) (buffer1_size & 0xff);
    }

	/* Special-case VCD headers do not specify the video buffer... */
    if ( which_streams & STREAMS_VIDEO ) {
		*(index++) = stream2;
		*(index++) = (unsigned char) (0xc0 |
									  (buffer2_scale << 5) | (buffer2_size >> 8));
		*(index++) = (unsigned char) (buffer2_size & 0xff);
    }

	system_header_size = (index - sys_header->buf);
	len_index[0] = (unsigned char)((system_header_size-6) >> 8);
	len_index[1] = (unsigned char)((system_header_size-6) & 0xff);
	sys_header->length = system_header_size;
}
