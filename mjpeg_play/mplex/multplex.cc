
#include "main.hh"

#include <math.h>
#include <stdlib.h>
#include <unistd.h>

#include <format_codes.h>



/******************************************************************* 
	Find the timecode corresponding to given position in the system stream
   (assuming the SCR starts at 0 at the beginning of the stream 
   
****************************************************************** */

void OutputStream::ByteposTimecode(bitcount_t bytepos, clockticks &ts)
{
	ts = (bytepos*CLOCKS)/static_cast<bitcount_t>(dmux_rate);
}


/**********
 *
 * NextPosAndSCR - Update nominal (may be >= actual) byte count
 * and SCR to next output sector.
 *
 ********/

void OutputStream::NextPosAndSCR()
{
	bytes_output += sector_transport_size;
	ByteposTimecode( bytes_output, current_SCR );
}


/**********
 *
 * NextPosAndSCR - Update nominal (may be >= actual) byte count
 * and SCR to next output sector.
 *
 ********/

void OutputStream::SetPosAndSCR( bitcount_t bytepos )
{
	bytes_output = bytepos;
	ByteposTimecode( bytes_output, current_SCR );
}

/* 
   Stream syntax parameters.
*/
		
	


unsigned int audio_buffer_size = 0;
unsigned int video_buffer_size = 0;


typedef enum { start_segment, mid_segment, 
			   last_vau_segment, last_aaus_segment }
segment_state;


static PaddingStream pstrm;
static EndMarkerStream estrm;

/******************************************************************

	Initialisation of stream syntax paramters based on selected
	user options.
******************************************************************/


// TODO: this mixes class member parameters with opt_ globals...

void OutputStream::InitSyntaxParameters()
{

	switch( opt_mux_format  )
	{
	case MPEG_FORMAT_VCD :
		opt_data_rate = 75*2352;  			 /* 75 raw CD sectors/sec */ 
	  	video_buffer_size = 46*1024;
	  	opt_VBR = 0;
 
	case MPEG_FORMAT_VCD_NSR : /* VCD format, non-standard rate */
		mjpeg_info( "Selecting VCD output profile\n");
		if( video_buffer_size == 0 )
			video_buffer_size = opt_buffer_size * 1024;
		opt_mpeg = 1;
	 	packets_per_pack = 1;
	  	sys_header_in_pack1 = 0;
	  	always_sys_header_in_pack = 0;
	  	sector_transport_size = 2352;	      /* Each 2352 bytes with 2324 bytes payload */
	  	transport_prefix_sectors = 30;
	  	sector_size = 2324;
		buffers_in_video = 1;
		always_buffers_in_video = 0;
		buffers_in_audio = 1;
		always_buffers_in_audio = 1;
		zero_stuffing = 1;
		dtspts_for_all_vau = 1;
		sector_align_iframeAUs = false;
		break;
		
	case  MPEG_FORMAT_MPEG2 : 
		mjpeg_info( "Selecting generic MPEG2 output profile\n");
		opt_mpeg = 2;
	 	packets_per_pack = 1;
	  	sys_header_in_pack1 = 1;
	  	always_sys_header_in_pack = 0;
	  	sector_transport_size = 2048;	      /* Each 2352 bytes with 2324 bytes payload */
	  	transport_prefix_sectors = 0;
	  	sector_size = 2048;
	  	opt_VBR = 0;
	  	video_buffer_size = 234*1024;
		buffers_in_video = 1;
		always_buffers_in_video = 0;
		buffers_in_audio = 1;
		always_buffers_in_audio = 1;
		zero_stuffing = 0;
		audio_packet_data_limit = 0;
        dtspts_for_all_vau = 0;
		sector_align_iframeAUs = false;
		break;

	case MPEG_FORMAT_SVCD :
		opt_data_rate = 150*2324;
	  	video_buffer_size = 230*1024;

	case  MPEG_FORMAT_SVCD_NSR :		/* Non-standard data-rate */
		mjpeg_info( "Selecting SVCD output profile\n");
		if( video_buffer_size == 0 )
			video_buffer_size = opt_buffer_size * 1024;
		opt_mpeg = 2;
		/* TODO should test specified data-rate is < 2*CD
		   = 150 sectors/sec * (mode 2 XA payload) */ 
	 	packets_per_pack = 1;
	  	sys_header_in_pack1 = 0;
	  	always_sys_header_in_pack = 0;
	  	sector_transport_size = 2324;
	  	transport_prefix_sectors = 0;
	  	sector_size = 2324;
	  	opt_VBR = 1;

		buffers_in_video = 0;
		always_buffers_in_video = 0;
		buffers_in_audio = 0;
		always_buffers_in_audio = 0;
		zero_stuffing = 0;
		audio_packet_data_limit = 0;
        dtspts_for_all_vau = 0;
		sector_align_iframeAUs = true;
		break;

	case MPEG_FORMAT_VCD_STILL :
		opt_data_rate = 75*2352;  			 /* 75 raw CD sectors/sec */ 
	  	video_buffer_size = 46*1024;
	  	opt_VBR = 0;
		opt_mpeg = 1;
	 	packets_per_pack = 1;
	  	sys_header_in_pack1 = 0;
	  	always_sys_header_in_pack = 0;
	  	sector_transport_size = 2352;	      /* Each 2352 bytes with 2324 bytes payload */
	  	transport_prefix_sectors = 30;
	  	sector_size = 2324;
		buffers_in_video = 1;
		always_buffers_in_video = 0;
		buffers_in_audio = 1;
		always_buffers_in_audio = 1;
		zero_stuffing = 1;
		dtspts_for_all_vau = 1;
		break;
			 
	default : /* MPEG_FORMAT_MPEG1 - auto format MPEG1 */
		mjpeg_info( "Selecting generic MPEG1 output profile\n");
		opt_mpeg = 1;
	  	packets_per_pack = opt_packets_per_pack;
	  	always_sys_header_in_pack = opt_always_system_headers;
		sys_header_in_pack1 = 1;
		sector_transport_size = opt_sector_size;
		transport_prefix_sectors = 0;
        sector_size = opt_sector_size;
		video_buffer_size = opt_buffer_size * 1024;
		buffers_in_video = 1;
		always_buffers_in_video = 1;
		buffers_in_audio = 0;
		always_buffers_in_audio = 1;
		zero_stuffing = 1;
		audio_packet_data_limit = 0;		/* Fill packet completely */
        dtspts_for_all_vau = 0;
		sector_align_iframeAUs = false;
		break;
	}
	

	audio_buffer_size = 4 * 1024;
}


void OutputStream::Init( VideoStream 	&vstrm,
						 AudioStream 	&astrm,
						 char *multi_file)
{
	int num_video = vstrm.init ? 1 : 0;
	int num_audio = astrm.init ? 1 : 0;
	unsigned int video_rate=0;
	unsigned int audio_rate=0;
	clockticks delay;
	unsigned int sectors_delay;

	Pack_struc 			dummy_pack;
	Sys_header_struc 	dummy_sys_header;	
	
	mjpeg_info("SYSTEMS/PROGRAM stream:\n");
	psstrm = new PS_Stream(opt_mpeg, sector_size, zero_stuffing );

	psstrm->Init( multi_file,
				  opt_max_segment_size );
	/* These are used to make (conservative) decisions
	   about whether a packet should fit into the recieve buffers... 
	   Audio packets always have PTS fields, video packets needn't.	
	*/ 
	
	astrm.max_packet_data = psstrm->PacketPayload( NULL, NULL, false, true, false );
	vstrm.max_packet_data = psstrm->PacketPayload( NULL, NULL, false, false, false );
	psstrm->CreatePack (&dummy_pack, 0, mux_rate);
	if( always_sys_header_in_pack )
	{
		psstrm->CreateSysHeader (&dummy_sys_header, mux_rate,  !opt_VBR, 1, 
						   1, 1, num_audio, num_video,
						   astrm, 
						   vstrm 
			);

		vstrm.min_packet_data = 
			psstrm->PacketPayload( &dummy_sys_header, &dummy_pack, 
							always_buffers_in_video, true, true );
		astrm.min_packet_data = 
			psstrm->PacketPayload( &dummy_sys_header, &dummy_pack, 
							true, true, false );

	}
	else
	{
		vstrm.min_packet_data = 
			psstrm->PacketPayload( NULL, &dummy_pack, 
							always_buffers_in_video, true, true );
		astrm.min_packet_data = 
			psstrm->PacketPayload( NULL, &dummy_pack, 
							true, true, false );

	}
      
     
	/* Set the mux rate depending on flags and the paramters of the specified streams */
     
	if (vstrm.init)
	{
		video_rate = vstrm.NominalBitRate();
	}
  
	// TODO We need to specify a *peak* rate if rate guessing is to work
	// for VBR.
	if (astrm.init)
	{
		audio_rate = astrm.NominalBitRate();
	}
    
	/* Attempt to guess a sensible mux rate for the given video and audio streams 	*/
	/* TODO: This is a pretty inexact guess and may need tweaking for different stream formats	 */
	 
	dmux_rate = static_cast<int>(
		1.01 * (video_rate + audio_rate) * 
		( (1.0  *   sector_size)/vstrm.min_packet_data +
		  (packets_per_pack-1) * sector_size/vstrm.max_packet_data
			)
		/ packets_per_pack
		);
	dmux_rate = (dmux_rate/50 + 25)*50;
	
	mjpeg_info ("best-guess multiplexed stream data rate    : %07d\n",dmux_rate * 8);
	if( opt_data_rate != 0 )
		mjpeg_info ("target data-rate specified               : %7d\n", opt_data_rate*8 );

	if( opt_data_rate == 0 )
	{
		mjpeg_info( "Setting best-guess data rate.\n");
	}
	else if ( opt_data_rate >= dmux_rate)
	{
		mjpeg_info( "Setting specified specified data rate: %7d\n", opt_data_rate*8 );
		dmux_rate = opt_data_rate;
	}
	else if ( opt_data_rate < dmux_rate )
	{
		mjpeg_warn( "Target data rate lower than computed requirement!\n");
		mjpeg_warn( "N.b. a 20%% or so discrepancy in variable bit-rate\n");
		mjpeg_warn( "streams is common and harmless provided no time-outs will occur\n"); 
		dmux_rate = opt_data_rate;
	}

	/* TODO: redundant ? */
	mux_rate = dmux_rate/50;

	/* To avoid Buffer underflow, the DTS of the first video and audio AU's
	   must be offset sufficiently	forward of the SCR to allow the buffer 
	   time to fill before decoding starts. Calculate the necessary delays...
	*/

	/* Calculate start delay in SCR units */
	if( opt_VBR )
		sectors_delay = 3*video_buffer_size / ( 4 * sector_size );
	else
		sectors_delay = 5 * video_buffer_size / ( 6 * sector_size );


	delay = (clockticks)(sectors_delay +
						 (vstrm.au_unsent+vstrm.min_packet_data-1)/vstrm.min_packet_data  +
						 (astrm.au_unsent+astrm.min_packet_data-1)/vstrm.min_packet_data) *
		(clockticks)sector_transport_size*(clockticks)CLOCKS/dmux_rate;

	/* Debugging aid - allows easy comparison with streams generated by vcdmplex */
	if( opt_mux_format == 1 && opt_emul_vcdmplex)
		delay = (36000-2400) * 300;
	video_delay = (clockticks)opt_video_offset*(clockticks)CLOCKS/1000;
	audio_delay = (clockticks)opt_audio_offset*(clockticks)CLOCKS/1000;
	audio_delay += delay;
	video_delay += delay;
	
}


/******************************************************************
    Program start-up packets.  Generate any irregular packets						needed at the start of the stream...
	Note: *must* leave a sensible in-stream system header in
	sys_header.
	TODO: get rid of this grotty sys_header global.
    TODO: VIDEO_STR_0 should depend on video stream!
******************************************************************/

void OutputStream::OutputPrefix( VideoStream &vstrm,
								 AudioStream &astrm)
{
	int num_video = vstrm.init ? 1 : 0;
	int num_audio = astrm.init ? 1 : 0;
	int vcd_2nd_syshdr_data_limit;

	Pack_struc 			dummy_pack;

	/* Deal with transport padding */
	SetPosAndSCR( bytes_output + 
				  transport_prefix_sectors*sector_transport_size );
	
	/* VCD: Two padding packets with video and audio system headers */

	switch (opt_mux_format)
	{
	case MPEG_FORMAT_VCD :
	case MPEG_FORMAT_VCD_NSR :
		/* First packet carries video-info-only sys_header */
		psstrm->CreateSysHeader (&sys_header, mux_rate, false, true, 
						   true, true, 0, num_video,
						   astrm, /*0, audio_buffer_size/128,*/
						   vstrm /*1, video_buffer_size/1024 */ );
	  	OutputPadding( current_SCR,  true, true, false,  0);					 
		
		/* Second packet carries audio-info-only sys_header */
		psstrm->CreateSysHeader (&sys_header, mux_rate,  false, true, 
						   true, true, num_audio, 0,
						   astrm, /* 0, audio_buffer_size/128,*/
						   vstrm /*1, video_buffer_size/1024 */ );
		psstrm->CreatePack (&dummy_pack, 0, mux_rate);

		vcd_2nd_syshdr_data_limit = 
			psstrm->PacketPayload( &sys_header, &dummy_pack,false, 0, 0)-20;
										   
	  	OutputPadding( current_SCR, true, true, false,
					   vcd_2nd_syshdr_data_limit
				   );
		/* Calculate the payload of an audio packet... currently 2279 */
		audio_packet_data_limit = 
			psstrm->PacketPayload( NULL, &dummy_pack,true,TIMESTAMPBITS_PTS,0 )-20;
		
        /* Ugh... what *were* they thinking of? */

		break;
		
	case MPEG_FORMAT_SVCD :
	case MPEG_FORMAT_SVCD_NSR :
		/* First packet carries sys_header */
		psstrm->CreateSysHeader (&sys_header, mux_rate,  !opt_VBR, true, 
						   true, true, num_audio, num_video,
						   astrm, 
						   vstrm  );
	  	OutputPadding( current_SCR, true,	true, false, 0);					 		break;

	case MPEG_FORMAT_VCD_STILL :

		/* First packet carries small-still sys_header */
		/* TODO COMPLETELY BOGUS!!!! */
		psstrm->CreateSysHeader (&sys_header, mux_rate, false, true,
						   true, true, 0, num_video,
						   astrm, /* 0, audio_buffer_size/128,*/
						   vstrm /* , 1, 46 */ );
		OutputPadding( current_SCR, true, true, false, 0);					
		/* Second packet carries large-still sys_header */
		psstrm->CreateSysHeader (&sys_header, mux_rate, false, true, 
						   true, true, 0, num_video,
						   astrm, 
						   vstrm );
		OutputPadding( current_SCR, true,true, false, 0);					 
		break;
			
	case MPEG_FORMAT_SVCD_STILL :
		mjpeg_error_exit1("SVCD STILLS NOT YET IMPLEMENTED!\n");
		break;
	}

	/* Create the in-stream header if needed */
	psstrm->CreateSysHeader (&sys_header, mux_rate, !opt_VBR, true, 
					   true, true, num_audio, num_video,
					   astrm, 
					   vstrm );


}



/******************************************************************
    Program shutdown packets.  Generate any irregular packets
    needed at the end of the stream...
   
******************************************************************/

void OutputStream::OutputSuffix()
{
	unsigned char *index;
	Pack_struc 	pack;

	psstrm->CreatePack (&pack, current_SCR, mux_rate);
	psstrm->CreateSector (&pack, NULL,
						  0,
						  estrm, 
						  false, 0, 0,
						  TIMESTAMPBITS_NO );
}

/******************************************************************
	Hauptschleife Multiplexroutinenaufruf
	Kuemmert sich um oeffnen und schliessen alles beteiligten
	Dateien und um den korrekten Aufruf der jeweils
	noetigen Video- und Audio-Packet Routinen.
	Gewissermassen passiert hier das Wesentliche des 
	Multiplexens. Die Bufferkapazitaeten und die TimeStamps
	werden ueberprueft und damit entschieden, ob ein Video-
	Audio- oder Padding-Packet erstellt und geschrieben
	werden soll.

	Main multiplex iteration.
	Opens and closes all needed files and manages the correct
	call od the respective Video- and Audio- packet routines.
	The basic multiplexing is done here. Buffer capacity and 
	Timestamp checking is also done here, decision is taken
	wether we should genereate a Video-, Audio- or Padding-
	packet.
******************************************************************/


	
void OutputStream::OutputMultiplex ( VideoStream &vstrm,
									 AudioStream &astrm,
									 char *multi_file)

{
	segment_state seg_state;
	VAunit *next_vau;
	unsigned int audio_bytes;
	unsigned int video_bytes;

	unsigned int nsec_a=0;
	unsigned int nsec_v=0;
	unsigned int nsec_p=0;

	clockticks audio_next_SCR;
	clockticks video_next_SCR;

	bool video_ended = false;
	bool audio_ended = false;

	unsigned int packets_left_in_pack = 0; /* Suppress warning */
	bool padding_packet;
	bool start_of_new_pack;
	bool include_sys_header = false; /* Suppress warning */

	Init( vstrm, astrm, multi_file );

	/*  Let's try to read in unit after unit and to write it out into
		the outputstream. The only difficulty herein lies into the
		buffer management, and into the fact the the actual access
		unit *has* to arrive in time, that means the whole unit
		(better yet, packet data), has to arrive before arrival of
		DTS. If both buffers are full we'll generate a padding packet
	  
		Of course, when we start we're starting a new segment with no
		bytes output...
	*/

	
	seg_state = start_segment;

	while ((vstrm.au_unsent + astrm.au_unsent) > 0)
	{
	
		/* A little state-machine for handling the transition from one
		   segment to the next 
		*/
		switch( seg_state )
		{

			/* Audio access units at end of segment.  If there are any
			   audio AU's whose PTS implies they should be played *before*
			   the video AU starting the next segement is presented
			   we mux them out.  Once they're gone we've finished
			   this segment so we write the suffix switch file,
			   and start muxing a new segment.
			*/
		case last_aaus_segment :
			if( astrm.au_unsent>0 && astrm.au->PTS >= vstrm.au->PTS )
			{
				/* Current segment has been written out... 
				 */
				OutputSuffix();
				psstrm->NextFile();
				seg_state = start_segment;
				/* Start a new segment... */
			}
			else
				break;

			/* Starting a new segment.
			   We send the segment prefix, video and audio reciever
			   buffers are assumed to start empty.  We reset the segment
			   length count and hence the SCR.
			   
			*/

		case start_segment :
			mjpeg_info( "New sequence commences...\n" );
			SetPosAndSCR(0);
			status_info (nsec_a, nsec_v, nsec_p, bytes_output,
						 vstrm.bufmodel.space(),
						 astrm.bufmodel.space(),
						 LOG_INFO);

				
			vstrm.bufmodel.flushed();
			astrm.bufmodel.flushed();
			OutputPrefix( vstrm, astrm );
			
			/* The starting PTS/DTS of AU's may of course be
			   non-zero since this might not be the first segment
			   we've built. Hence we adjust the "delay" to
			   compensate for this as well as for any
			   synchronisation / start-up delay needed.  
			*/
				
			if (vstrm.init)
			{
				vstrm.SetSyncOffset( video_delay + current_SCR-astrm.au->PTS );
			}
  
			if (astrm.init)
			{

				astrm.SetSyncOffset( audio_delay + current_SCR-vstrm.au->DTS);
			}
	
 

			packets_left_in_pack = packets_per_pack;
			include_sys_header = sys_header_in_pack1;
			buffers_in_video = 1;
			nsec_a = nsec_v = nsec_p =0;
			seg_state = mid_segment;
			break;

		case mid_segment :
			/* Once we exceed our file size limit, we need to
			   start a new file soon.  If we want a single program we
			   simply switch.
				
			   Otherwise we're in the last gop of the current segment
			   (and need to start winding stuff down ready for a
			   clean continuation in the next segment).
				
			*/
			if( psstrm->FileLimReached() )
			{
				if( opt_multifile_segment )
					psstrm->NextFile();
				else
				{
					next_vau = vstrm.Lookahead( 1);
					if( next_vau->type != IFRAME)
						seg_state = last_vau_segment;
				}
			}
			else if( vstrm.EndSeq() )
			{
				next_vau = vstrm.Lookahead( 1);
				if( next_vau  )
				{
					if( ! next_vau->seq_header || next_vau->type != IFRAME)
					{
						mjpeg_error_exit1( "Sequence split detected %d but no following sequence found...\n", next_vau->seq_header);
					}
						
					seg_state = last_vau_segment;
				}
			}
			break;
			
			/* If we're the last video AU of the segment and the
			   current sector will start with a new IFRAME AU we have
			   just ended the last GOP of the segment.  We now run out
			   any remaining audio due to be scheduled before the 
			   current video AU (which will form the first video AU
			   of the next segement.
			*/
		case last_vau_segment :
			if( vstrm.AUType() == IFRAME )
				seg_state = last_aaus_segment;
			break;
		}

		padding_packet = false;
		start_of_new_pack = (packets_left_in_pack == packets_per_pack); 

		/* Calculate amount of data to be moved for the next AU's.
		   Slightly pessimistic - assumes worst-case packet data capacity
		   and the need to start a new packet.
		*/
		audio_bytes = astrm.BytesToMuxAUEnd(sector_transport_size);
		video_bytes = vstrm.BytesToMuxAUEnd(sector_transport_size);

	
		/* Calculate when the the next AU's will finish arriving under the assumption
		   that the next sector carries a *different* payload.  Using
		   this time we can see if a stream will definately under-run
		   if no sector is sent immediately. 		   
		*/
		
		
		//ByteposTimecode ( bytes_output, current_SCR);
		ByteposTimecode (bytes_output+(sector_transport_size+audio_bytes), audio_next_SCR);
		ByteposTimecode (bytes_output+(sector_transport_size+video_bytes), video_next_SCR);

		if (astrm.init)
			astrm.bufmodel.cleaned(current_SCR);
		if (vstrm.init)
			vstrm.bufmodel.cleaned(current_SCR);


		/* CASE: Audio Buffer OK, Audio Data ready
		   SEND An audio packet
		*/

		/* Heuristic... if we can we prefer to send audio rather than vstrm. 
		   Even a few uSec under-run are audible and in any case the data-rate
		   is trivial compared wth video. The only exception is if not
		   sending video would cause it to under-run but there's no danger of
		   and audio under-run
		   	   
		*/

		if ( (astrm.bufmodel.space()-AUDIO_BUFFER_FILL_MARGIN
			  > astrm.max_packet_data)
			 && (astrm.au_unsent>0)
			 && nsec_v != 0
			 &&  ! (  vstrm.au_unsent !=0 &&
					  seg_state != last_aaus_segment &&
					  video_next_SCR >= vstrm.au->DTS+vstrm.timestamp_delay &&
					  audio_next_SCR < astrm.au->PTS+astrm.timestamp_delay
				 )
			)
		{
			/* Calculate actual time current AU is likely to arrive. */
			ByteposTimecode (bytes_output+audio_bytes, audio_next_SCR);
			if( audio_next_SCR >= astrm.au->PTS+astrm.timestamp_delay )
				timeout_error (STATUS_AUDIO_TIME_OUT,astrm.au->dorder);
			OutputAudio (astrm,
						 start_of_new_pack,
						 include_sys_header,
						 seg_state == last_aaus_segment);
			++nsec_a;
		}

		/* CASE: Video Buffer OK, Video Data ready  (implicitly -  no audio packet to send 
		   SEND a video packet.
		*/

		else if( vstrm.bufmodel.space() >= vstrm.max_packet_data
				 && vstrm.au_unsent>0 && seg_state != last_aaus_segment
			)
		{

			/* Calculate actual time current AU is likely to arrive. */
			ByteposTimecode (bytes_output+video_bytes, video_next_SCR);
			if( video_next_SCR >= vstrm.au->DTS+vstrm.timestamp_delay )
				timeout_error (STATUS_VIDEO_TIME_OUT,vstrm.au->dorder);
			OutputVideo ( vstrm,  start_of_new_pack, include_sys_header);

			++nsec_v;

		}

		/* CASE: Audio Buffer and Video Buffers NOT OK (too full to send)
		   SEND padding packet */
		else
		{

			OutputPadding (current_SCR, 
							start_of_new_pack, include_sys_header, opt_VBR,
							0);
			padding_packet =true;
			if( ! opt_VBR )
				++nsec_p;
		}
	
		/* Update the counter for pack packets.  VBR is a tricky 
		   case as here padding packets are "virtual" */
		
		if( ! (opt_VBR && padding_packet) )
		{
			--packets_left_in_pack;
			if (packets_left_in_pack == 0) 
				packets_left_in_pack = packets_per_pack;
		}


		status_info (nsec_a, nsec_v, nsec_p, bytes_output,
					 vstrm.bufmodel.space(),
					 astrm.bufmodel.space(),
					 LOG_DEBUG);

		/* Unless sys headers are always required we turn them off after the first
		   packet has been generated */
		include_sys_header = always_sys_header_in_pack;

		if( !video_ended && vstrm.au_unsent == 0 )
		{
			mjpeg_info( "Video stream ended.\n" );
			status_info (nsec_a, nsec_v, nsec_p, bytes_output,
						 vstrm.bufmodel.space(),
						 astrm.bufmodel.space(),
						 LOG_INFO);
			video_ended = 1;
		}

		if( !audio_ended && astrm.au_unsent == 0 )
		{
			mjpeg_info( "Audio stream ended.\n" );
			status_info (nsec_a, nsec_v, nsec_p, bytes_output,
						 vstrm.bufmodel.space(),
						 astrm.bufmodel.space(),
						 LOG_INFO);
			audio_ended = 1;
		}
	}

	// Tidy up
	
	OutputSuffix( );
	psstrm->Close();
    
}


/******************************************************************
	Next_Video_Access_Unit 
    Updates video buffer and access unit
	information from the look-ahead scanning buffer, based on the
	assumption that bytes_muxed bytes were written out in the last
	packet.
******************************************************************/


void OutputStream::NextVideoAU (unsigned int bytes_muxed,
								VideoStream  &vstrm
	)
{
	clockticks   decode_time;
	VAunit *vau;
  
	if (bytes_muxed == 0)
		return;


	/* Work through what's left of the current AU and the following AU's
	   updating the info until we reach a point where an AU had to be
	   split between packets.
	   NOTE: It *is* possible for this loop to iterate. 

	   The DTS/PTS field for the packet in this case would have been
	   given the that for the first AU to start in the packet.
	   Whether Joe-Blow's hardware VCD player handles this properly is
	   another matter of course!
	*/

	decode_time = vstrm.au->DTS + vstrm.timestamp_delay;
	while (vstrm.au_unsent < bytes_muxed)
	{	  

		vstrm.bufmodel.queued (vstrm.au_unsent, decode_time);
		bytes_muxed -= vstrm.au_unsent;
		vstrm.NextAU();
		if( vstrm.au_unsent == 0 )
			return;
		vstrm.new_au_next_sec = true;
		decode_time = vstrm.au->DTS + vstrm.timestamp_delay;
	};

	// We've now reached a point where the current AU overran or
	// fitted exactly.  We need to distinguish the latter case
	// so we can record whether the next packet starts with an
	// existing AU or not - info we need to decide what PTS/DTS
	// info to write at the start of the next packet.
	
	if (vstrm.au_unsent > bytes_muxed)
	{

		vstrm.bufmodel.queued( bytes_muxed, decode_time);
		vstrm.au_unsent -= bytes_muxed;
		vstrm.new_au_next_sec = false;
	} 
	else //  if (vstrm.au_unsent == bytes_muxed)
	{
		vstrm.bufmodel.queued(bytes_muxed, decode_time);
		if( ! vstrm.NextAU() )
			return;
		vstrm.new_au_next_sec = true;
	}	   

}


/******************************************************************
	Output_Video
	generiert Pack/Sys_Header/Packet Informationen aus dem
	Video Stream und speichert den so erhaltenen Sektor ab.

	generates Pack/Sys_Header/Packet information from the
	video stream and writes out the new sector
******************************************************************/

void OutputStream::OutputVideo ( VideoStream &vstrm,
								 bool start_of_new_pack,
								 bool include_sys_header
	)

{

	unsigned int max_packet_payload; 	 
	unsigned int actual_payload;
	unsigned int prev_au_tail;
	Pack_struc *pack_ptr = NULL;
	Sys_header_struc *sys_header_ptr = NULL;
	unsigned char timestamps;
	VAunit *vau;
	Pack_struc pack;
	unsigned int old_au_then_new_payload;
	clockticks  DTS,PTS;
  
	max_packet_payload = 0;	/* 0 = Fill sector */
  	/* 	
	   We're now in the last AU of a segment. 
		So we don't want to go beyond it's end when filling
		sectors. Hence we limit packet payload size to (remaining) AU length.
		The same applies when we wish to ensure sequence headers starting
		ACCESS-POINT AU's in (S)VCD's etc are sector-aligned.
	*/
	
	if( vstrm.EndSeq() ||  (sector_align_iframeAUs && vstrm.SeqHdrNext() )
		) 
	{
		max_packet_payload = vstrm.au_unsent;
	}

	if (start_of_new_pack)
    {
		/* Wir generieren den Pack Header				*/
		/* let's generate pack header					*/
		psstrm->CreatePack (&pack, current_SCR, mux_rate);
		pack_ptr = &pack;
		if( include_sys_header )
			sys_header_ptr = &sys_header;
    }
    
	
        
	/* Figure out the threshold payload size below which we can fit more
	   than one AU into a packet N.b. because fitting more than one in
	   imposses an overhead of additional header fields so there is a
	   dead spot where we *have* to stuff the packet rather than start
	   fitting in an extra AU.  Slightly over-conservative in the case
	   of the last packet...  */

	old_au_then_new_payload = psstrm->PacketPayload( sys_header_ptr, pack_ptr, 
											  buffers_in_video, true, true);

	PTS = vstrm.au->PTS + vstrm.timestamp_delay;
	DTS = vstrm.au->DTS + vstrm.timestamp_delay;

	/* CASE: Packet starts with new access unit			*/
	if (vstrm.new_au_next_sec  )
	{
		if(  dtspts_for_all_vau && max_packet_payload == 0 )
			max_packet_payload = vstrm.au_unsent;

		if (vstrm.AUType() == BFRAME)
			timestamps=TIMESTAMPBITS_PTS;
		else
			timestamps=TIMESTAMPBITS_PTS_DTS;

		actual_payload =
			psstrm->CreateSector ( pack_ptr, sys_header_ptr,
								   max_packet_payload,
								   vstrm,
								   buffers_in_video, PTS, DTS,
								   timestamps );
		NextVideoAU ( actual_payload, vstrm);

	}

	/* FALL: Packet beginnt mit alter Access Unit, es kommt	*/
	/*       keine neue im selben Packet vor					*/
	/* CASE: Packet begins with old access unit, no new one	*/
	/*	     begins in the very same packet					*/
	else if ( ! vstrm.new_au_next_sec &&
			  (vstrm.au_unsent >= old_au_then_new_payload))
	{
		actual_payload = 
		psstrm->CreateSector( pack_ptr, sys_header_ptr,
							  vstrm.au_unsent,
							  vstrm,
							  buffers_in_video, 0, 0,
							  TIMESTAMPBITS_NO );
		NextVideoAU ( actual_payload, vstrm);

	}

	/* FALL: Packet beginnt mit alter Access Unit, es kommt	*/
	/*       eine neue im selben Packet vor			*/
	/* CASE: Packet begins with old access unit, a new one	*/
	/*	     begins in the very same packet			*/
	else if ( !vstrm.new_au_next_sec  && 
			  (vstrm.au_unsent < old_au_then_new_payload))
	{
		prev_au_tail = vstrm.au_unsent;
		vstrm.bufmodel.queued (vstrm.au_unsent, DTS);

		/* is there a new access unit anyway? */

		if( vstrm.NextAU() )
		{
			if(  dtspts_for_all_vau  && max_packet_payload == 0 )
				max_packet_payload = vstrm.au_unsent+prev_au_tail;

			if (vstrm.AUType() == BFRAME)
				timestamps=TIMESTAMPBITS_PTS;
			else
				timestamps=TIMESTAMPBITS_PTS_DTS;
			vstrm.new_au_next_sec = true;
			PTS = vstrm.au->PTS + vstrm.timestamp_delay;
			DTS = vstrm.au->DTS + vstrm.timestamp_delay;
	
			actual_payload = 
				psstrm->CreateSector ( pack_ptr, sys_header_ptr,
									  max_packet_payload,
									  vstrm,
									  buffers_in_video, PTS, DTS,
									  timestamps );
			NextVideoAU ( actual_payload - prev_au_tail,  vstrm);
		} 
		else
		{
			psstrm->CreateSector ( pack_ptr, sys_header_ptr,
								   0,
								   vstrm,
								   buffers_in_video, 0, 0,
								   TIMESTAMPBITS_NO);
		};


	}


	buffers_in_video = always_buffers_in_video;
	NextPosAndSCR();
}


/******************************************************************
	Next_Audio_Access_Unit
	holt aus dem TMP File, der die Info's ueber die Access
	Units enthaelt, die jetzt gueltige Info her. Nach
	dem Erstellen des letzten Packs sind naemlich eine
	bestimmte Anzahl Bytes und damit AU's eingelesen worden.

	gets information on access unit from the tmp file
******************************************************************/

void OutputStream::NextAudioAU( unsigned int bytes_muxed,
								 AudioStream &astrm
	)

{
	AAunit *aau;
	clockticks   decode_time;
  
	if (bytes_muxed == 0)
		return;

	decode_time = astrm.au->PTS + astrm.timestamp_delay;
	while (astrm.au_unsent < bytes_muxed)
	{
		astrm.bufmodel.queued ( astrm.au_unsent, decode_time);
		bytes_muxed -= astrm.au_unsent;
		if( ! astrm.NextAU() )
			return;
		astrm.new_au_next_sec = true;
		decode_time = astrm.au->PTS + astrm.timestamp_delay;
	};

	if (astrm.au_unsent > bytes_muxed)
	{
		astrm.bufmodel.queued( bytes_muxed, decode_time);
		astrm.au_unsent -= bytes_muxed;
		astrm.new_au_next_sec = false;
	} else
		if (astrm.au_unsent == bytes_muxed)
		{
			astrm.bufmodel.queued( bytes_muxed, decode_time);
			if( ! astrm.NextAU() )
				return;
			astrm.new_au_next_sec = true;
		};

}

/******************************************************************
	Output_Audio
	generates Pack/Sys Header/Packet information from the
	audio stream and saves them into the sector
******************************************************************/

void OutputStream::OutputAudio ( AudioStream &astrm,
								 bool start_of_new_pack,
								 bool include_sys_header,
								 bool last_au_segment
	)

{
	clockticks   PTS;
	unsigned int max_packet_data; 	 
	unsigned int actual_payload;
	unsigned int bytes_sent;
	Pack_struc *pack_ptr = NULL;
	Sys_header_struc *sys_header_ptr = NULL;
	AAunit *aau;
	Pack_struc pack;
	unsigned int old_au_then_new_payload;

	PTS = astrm.au->PTS + astrm.timestamp_delay;
	old_au_then_new_payload = 
		psstrm->PacketPayload( sys_header_ptr, pack_ptr,
							   buffers_in_audio, false, false );

	max_packet_data= audio_packet_data_limit;
	if( last_au_segment )
	{
		/* We're now in the last AU of a segment.  So we don't want to go beyond it's end when willing
		   sectors. Hence we limit packet payload size to (remaining) AU length.
		*/
		max_packet_data = astrm.au_unsent;
	}
  
	if( audio_packet_data_limit && old_au_then_new_payload > max_packet_data )
		old_au_then_new_payload = max_packet_data;
 
	if (start_of_new_pack)
    {
		/* Wir generieren den Pack Header				*/
		/* let's generate pack header					*/
		psstrm->CreatePack (&pack, current_SCR, mux_rate);
		pack_ptr = &pack;
		if( include_sys_header )
			sys_header_ptr = &sys_header;
    }
    
	/* Wir generieren das Packet				*/
	/* Let's generate packet					*/

	/* faengt im Packet ein Audio Frame an?			*/
	/* does a audio frame start in this packet?			*/

	/* FALL: Packet beginnt mit neuer Access Unit			*/
	/* CASE: packet starts with new access unit			*/
	
	if (astrm.new_au_next_sec)
    {
		actual_payload = 
			psstrm->CreateSector ( pack_ptr, sys_header_ptr,
								  audio_packet_data_limit,
								  astrm,
								  buffers_in_audio, PTS, 0,
								  TIMESTAMPBITS_PTS);

		NextAudioAU ( actual_payload, astrm);
    }

	/* FALL: Packet beginnt mit alter Access Unit, es kommt	*/
	/*       keine neue im selben Packet vor			*/
	/* CASE: packet starts with old access unit, no new one	*/
	/*       starts in this very same packet			*/
	else if (!(astrm.new_au_next_sec) && 
			 (astrm.au_unsent >= old_au_then_new_payload))
    {
		actual_payload = 
			psstrm->CreateSector ( pack_ptr, sys_header_ptr,
								  audio_packet_data_limit,
								  astrm,
								  buffers_in_audio, 0, 0,
								  TIMESTAMPBITS_NO );
		NextAudioAU ( actual_payload, astrm);
    }


	/* CASE: packet starts with old access unit, a new one	*/
	/*       starts in this very same packet			*/
	else /* !(astrm.new_au_next_sec) &&  (astrm.au_unsent < old_au_then_new_payload)) */
    {
		bytes_sent = astrm.au_unsent;
		astrm.bufmodel.queued( astrm.au_unsent, PTS);

		/* gibt es ueberhaupt noch eine Access Unit ? */
		/* is there another access unit anyway ? */
		if( astrm.NextAU() )
		{
			astrm.new_au_next_sec = true;
			PTS = astrm.au->PTS + astrm.timestamp_delay;
			actual_payload = 
				psstrm->CreateSector ( pack_ptr, sys_header_ptr,
									  audio_packet_data_limit,
									  astrm,
									  buffers_in_audio, PTS, 0,
									  TIMESTAMPBITS_PTS );

			NextAudioAU ( actual_payload - bytes_sent, astrm );
		} 
		else
		{
			psstrm->CreateSector ( pack_ptr, sys_header_ptr,
						   0,
						   astrm,
						   buffers_in_audio, 0, 0,
						   TIMESTAMPBITS_NO );
		};

    }

	NextPosAndSCR();

	buffers_in_audio = always_buffers_in_audio;
	
}

/******************************************************************
	OutputPadding
	erstellt Pack/Sys_Header/Packet Informationen zu einem
	Padding-Stream und speichert den so erhaltenen Sector ab.

	generates Pack/Sys Header/Packet information for a 
	padding stream and saves the sector
	
	This is at the heart of a simple implementation of
	VBR multiplexing.  We treat VBR as CBR albeit with
	a bit-rate rather higher than the peak bit-rate observed
	in the stream.  
	
	The stream we generate is then simply a CBR stream
	for this bit-rate for a large buffer and *with
	padding blocks stripped*.

	We have to pass in a packet data limit to cope with
	appalling mess VCD makes of audio packets (the last 20
	bytes being dropped thing)
	0 = Fill the packet completetely...
******************************************************************/

void OutputStream::OutputPadding (	clockticks SCR,
									bool start_of_new_pack,
									bool include_sys_header,
									bool VBR_pseudo,
									int padding_limit
	)

{
	Pack_struc *pack_ptr = NULL;
	Sys_header_struc *sys_header_ptr = NULL;
	Pack_struc pack;

	if( ! VBR_pseudo  )
	{
		if (start_of_new_pack)
		{
			/* Wir generieren den Pack Header				*/
			/* let's generate the pack header				*/
			psstrm->CreatePack (&pack, SCR, mux_rate);
			pack_ptr = &pack;
			if( include_sys_header )
				sys_header_ptr = &sys_header;
		}


		/* Wir generieren das Packet				*/
		/* let's generate the packet				*/
		psstrm->CreateSector ( pack_ptr, sys_header_ptr,
							  padding_limit,
							  estrm,
							  false, 0, 0,
							  TIMESTAMPBITS_NO );
	}
	NextPosAndSCR();

}

