
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
		
	



typedef enum { start_segment, mid_segment, 
			   runout_segment }
segment_state;



/******************************************************************

	Initialisation of stream syntax paramters based on selected
	user options.
******************************************************************/


// TODO: this mixes class member parameters with opt_ globals...

void OutputStream::InitSyntaxParameters()
{
	video_buffer_size = 0;
	seg_starts_with_video = false;
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
		vbr = opt_VBR;
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
		vcd_zero_stuffing = 20;
		dtspts_for_all_vau = 1;
		sector_align_iframeAUs = false;
		seg_starts_with_video = true;
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
	  	video_buffer_size = 234*1024;
		buffers_in_video = 1;
		always_buffers_in_video = 0;
		buffers_in_audio = 1;
		always_buffers_in_audio = 1;
		vcd_zero_stuffing = 0;
        dtspts_for_all_vau = 0;
		sector_align_iframeAUs = false;
		vbr = opt_VBR;
		break;

	case MPEG_FORMAT_SVCD :
		opt_data_rate = 150*2324;
	  	video_buffer_size = 230*1024;
	  	opt_VBR = 1;

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
		vbr = opt_VBR;
		buffers_in_video = 1;
		always_buffers_in_video = 0;
		buffers_in_audio = 1;
		always_buffers_in_audio = 0;
		vcd_zero_stuffing = 0;
        dtspts_for_all_vau = 0;
		sector_align_iframeAUs = true;
		seg_starts_with_video = true;
		break;

	case MPEG_FORMAT_VCD_STILL :
		opt_data_rate = 75*2352;  			 /* 75 raw CD sectors/sec */ 
	  	vbr = opt_VBR = 0;
		opt_mpeg = 1;
	 	packets_per_pack = 1;
	  	sys_header_in_pack1 = 0;
	  	always_sys_header_in_pack = 0;
	  	sector_transport_size = 2352;	      /* Each 2352 bytes with 2324 bytes payload */
	  	transport_prefix_sectors = 0;
	  	sector_size = 2324;
		buffers_in_video = 1;
		always_buffers_in_video = 0;
		buffers_in_audio = 1;
		always_buffers_in_audio = 1;
		vcd_zero_stuffing = 20;
		dtspts_for_all_vau = 1;
		sector_align_iframeAUs = true;
		/* TODO: Really need to allow mixed streams to each have different
		   buffer sizes. */
		if( opt_buffer_size == 0 )
			opt_buffer_size = 46;
		else if( opt_buffer_size > 220 )
		{
			mjpeg_error_exit1("VCD stills has max. permissible video buffer size of 220KB\n");
		}
		else
		{
			/* Add a margin for sequence header overheads for HR stills */
			/* So the user simply specifies the nominal size... */
			opt_buffer_size += 4;
		}		
		video_buffer_size = opt_buffer_size*1024;
		break;

	case MPEG_FORMAT_SVCD_STILL :
		mjpeg_info( "Selecting SVCD output profile\n");
		if( opt_data_rate == 0 )
			opt_data_rate = 150*2324;
	  	video_buffer_size = 230*1024;
		opt_mpeg = 2;
	 	packets_per_pack = 1;
	  	sys_header_in_pack1 = 0;
	  	always_sys_header_in_pack = 0;
	  	sector_transport_size = 2324;
	  	transport_prefix_sectors = 0;
	  	sector_size = 2324;
		vbr = opt_VBR = 1;
		buffers_in_video = 1;
		always_buffers_in_video = 0;
		buffers_in_audio = 1;
		always_buffers_in_audio = 0;
		vcd_zero_stuffing = 0;
        dtspts_for_all_vau = 0;
		sector_align_iframeAUs = true;
		break;
			 
	default : /* MPEG_FORMAT_MPEG1 - auto format MPEG1 */
		mjpeg_info( "Selecting generic MPEG1 output profile\n");
		opt_mpeg = 1;
		vbr = opt_VBR;
	  	packets_per_pack = opt_packets_per_pack;
	  	always_sys_header_in_pack = opt_always_system_headers;
		sys_header_in_pack1 = 1;
		sector_transport_size = opt_sector_size;
		transport_prefix_sectors = 0;
        sector_size = opt_sector_size;
		if( opt_buffer_size == 0 )
		{
			opt_buffer_size = 46;
		}
		video_buffer_size = opt_buffer_size * 1024;
		buffers_in_video = 1;
		always_buffers_in_video = 1;
		buffers_in_audio = 0;
		always_buffers_in_audio = 1;
		vcd_zero_stuffing = 0;
        dtspts_for_all_vau = 0;
		sector_align_iframeAUs = false;
		break;
	}
	
	audio_buffer_size = 4 * 1024;
}


void OutputStream::Init( char *multi_file)
{
	vector<ElementaryStream *>::iterator str;
	clockticks delay;
	unsigned int sectors_delay;

	Pack_struc 			dummy_pack;
	Sys_header_struc 	dummy_sys_header;	
	Sys_header_struc *sys_hdr;
	unsigned int nominal_rate_sum;
	
	mjpeg_info("SYSTEMS/PROGRAM stream:\n");
	psstrm = new PS_Stream(opt_mpeg, sector_size );

	psstrm->Init( multi_file,
				  opt_max_segment_size );
	
    /* These are used to make (conservative) decisions
	   about whether a packet should fit into the recieve buffers... 
	   Audio packets always have PTS fields, video packets needn'.	
	   TODO: Really this should be encapsulated in Elementary stream...?
	*/ 
	psstrm->CreatePack (&dummy_pack, 0, mux_rate);
	if( always_sys_header_in_pack )
	{
		// TODO: I don't think the locks should always be set...
		psstrm->CreateSysHeader (&dummy_sys_header, mux_rate,  
								 !vbr, 1,  true, true, estreams);
		sys_hdr = &dummy_sys_header;
	}
	else
		sys_hdr = NULL;

	nominal_rate_sum = 0;
	for( str = estreams.begin(); str < estreams.end(); ++str )
	{
		switch( (*str)->Kind() )
		{
		case ElementaryStream::audio :
			(*str)->SetMaxPacketData( 
				psstrm->PacketPayload( **str, NULL, NULL, 
									   false, true, false ) 
				); 
			(*str)->SetMinPacketData(
				psstrm->PacketPayload( **str, sys_hdr, &dummy_pack, 
									   always_buffers_in_audio, true, false )
				);
				
			break;
		case ElementaryStream::video :
			(*str)->SetMaxPacketData( 
				psstrm->PacketPayload( **str, NULL, NULL, 
									   false, false, false ) 
				); 
			(*str)->SetMinPacketData( 
				psstrm->PacketPayload( **str, sys_hdr, &dummy_pack, 
									   always_buffers_in_video, true, true )
				);
			break;
		default :
			mjpeg_error_exit1("INTERNAL: Only audio and video payload calculations implemented!\n");
			
		}
		if( (*str)->NominalBitRate() == 0 && opt_data_rate == 0)
			mjpeg_error_exit1( "Variable bit-rate stream present: output stream (max) data-rate *must* be specified!\n");
		nominal_rate_sum += (*str)->NominalBitRate();

	}
		
	
	/* Attempt to guess a sensible mux rate for the given video and *
	 audio estreams. This is a rough and ready guess for MPEG-1 like
	 formats. */
	   
	 
	dmux_rate = static_cast<int>(1.015 * nominal_rate_sum);
	dmux_rate = (dmux_rate/50 + 25)*50;
	
	mjpeg_info ("rough-guess multiplexed stream data rate    : %07d\n",dmux_rate * 8);
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

	mux_rate = dmux_rate/50;

	/* To avoid Buffer underflow, the DTS of the first video and audio AU's
	   must be offset sufficiently	forward of the SCR to allow the buffer 
	   time to fill before decoding starts. Calculate the necessary delays...
	*/

	/* Set a start delay in SCR units that guarantees the buffers should
	   be nicely filled up.
	*/

	if( MPEG_STILLS_FORMAT( opt_mux_format ) )
		sectors_delay =  (unsigned int)(1.02*video_buffer_size) / sector_size+2;
	else if( vbr )
		sectors_delay = 3*video_buffer_size / ( 4 * sector_size );
	else
		sectors_delay = 5 * video_buffer_size / ( 6 * sector_size );

	ByteposTimecode( 
		static_cast<bitcount_t>(sectors_delay*sector_transport_size),
		delay );

	video_delay = delay + 
		static_cast<clockticks>(opt_video_offset*CLOCKS/1000);
	audio_delay = delay + 
		static_cast<clockticks>(opt_audio_offset*CLOCKS/1000);
	mjpeg_info( "Sectors = %d Video delay = %lld Audio delay = %lld\n",
				sectors_delay,
				 video_delay / 300,
				 audio_delay / 300 );

	//
	// Now that all mux parameters are set we can trigger parsing
	// of actual input stream data and calculation of associated 
	// PTS/DTS by causing the read of the first AU's...
	//
	for( str = estreams.begin(); str < estreams.end(); ++str )
	{
		(*str)->NextAU();
	}
				 
}

void OutputStream::MuxStatus(log_level_t level)
{
	vector<ElementaryStream *>::iterator str;
	for( str = estreams.begin(); str < estreams.end(); ++str )
	{
		switch( (*str)->Kind()  )
		{
		case ElementaryStream::video :
			mjpeg_log( level,
					   "Video %02x: buf=%7d frame=%06d sector=%08d\n",
					   (*str)->stream_id,
					   (*str)->bufmodel.space(),
					   (*str)->au->dorder,
					   (*str)->nsec
				);
			break;
		case ElementaryStream::audio :
			mjpeg_log( level,
					   "Audio %02x: buf=%7d frame=%06d sector=%08d\n",
					   (*str)->stream_id,
					   (*str)->bufmodel.space(),
					   (*str)->au->dorder,
					   (*str)->nsec
				);
			break;
		default :
			mjpeg_log( level,
					   "Other %02x: buf=%7d sector=%08d\n",
					   (*str)->stream_id,
					   (*str)->bufmodel.space(),
					   (*str)->nsec
				);
			break;
		}
	}
	if( !vbr )
		mjpeg_log( level,
				   "Padding : sector=%08d\n",
				   pstrm.nsec
			);
	
	
}

/******************************************************************
    Program start-up packets.  Generate any irregular packets						needed at the start of the stream...
	Note: *must* leave a sensible in-stream system header in
	sys_header.
	TODO: get rid of this grotty sys_header global.
    TODO: VIDEO_STR_0 should depend on video stream!
******************************************************************/

void OutputStream::OutputPrefix( )
{
	vector<ElementaryStream *>::iterator str;

	/* Deal with transport padding */
	SetPosAndSCR( bytes_output + 
				  transport_prefix_sectors*sector_transport_size );
	/* Pack header for system header packs */
	psstrm->CreatePack (&pack_header, 0, mux_rate);
	
	/* VCD: Two padding packets with video and audio system headers */
	split_at_seq_end = true;
	switch (opt_mux_format)
	{
	case MPEG_FORMAT_VCD :
	case MPEG_FORMAT_VCD_NSR :
		/* Annoyingly VCD generates seperate system headers for
		   audio and video ... DOH... */
		if( astreams.size() > 1 || vstreams.size() > 1 ||
			astreams.size() + vstreams.size() != estreams.size() )
		{
				mjpeg_error_exit1("VCD man only have max. 1 audio and 1 video stream\n");
		}

		/* First packet carries video-info-only sys_header */
		psstrm->CreateSysHeader (&sys_header, mux_rate, 
								 false, true, 
								 true, true, vstreams  );
		sys_header_ptr = &sys_header;
		pack_header_ptr = &pack_header;
	  	OutputPadding( current_SCR,  true, true, false,  false);		

		/* Second packet carries audio-info-only sys_header */
		psstrm->CreateSysHeader (&sys_header, mux_rate,  false, true, 
								 true, true, astreams );

		
	  	OutputPadding( current_SCR, true, true, false, true );
		break;
		
	case MPEG_FORMAT_SVCD :
	case MPEG_FORMAT_SVCD_NSR :
		/* First packet carries sys_header */
		psstrm->CreateSysHeader (&sys_header, mux_rate,  !vbr, true, 
						   true, true, estreams );
		sys_header_ptr = &sys_header;
		pack_header_ptr = &pack_header;
	  	OutputPadding( current_SCR, true,	true, false, 0);					 		break;

	case MPEG_FORMAT_VCD_STILL :
		split_at_seq_end = false;
		/* First packet carries small-still sys_header */
		/* TODO No support mixed-mode stills sequences... */
		psstrm->CreateSysHeader (&sys_header, mux_rate, false, false,
								 true, true, estreams );
		sys_header_ptr = &sys_header;
		pack_header_ptr = &pack_header;
		OutputPadding( current_SCR, true, true, false, false);							break;
			
	case MPEG_FORMAT_SVCD_STILL :
		/* TODO: Video only at present */
		/* First packet carries video-info-only sys_header */
		psstrm->CreateSysHeader (&sys_header, mux_rate, 
								 false, true, 
								 true, true, vstreams  );
		sys_header_ptr = &sys_header;
		pack_header_ptr = &pack_header;
	  	OutputPadding( current_SCR,  true, true, false,  false);		


		break;
	}

	/* Create the in-stream header if needed */
	psstrm->CreateSysHeader (&sys_header, mux_rate, !vbr, true, 
					   true, true, estreams );


}



/******************************************************************
    Program shutdown packets.  Generate any irregular packets
    needed at the end of the stream...
   
******************************************************************/

void OutputStream::OutputSuffix()
{
	unsigned char *index;
	psstrm->CreatePack (&pack_header, current_SCR, mux_rate);
	psstrm->CreateSector (&pack_header, NULL,
						  0,
						  pstrm, 
						  false,
						  true,
						  0, 0,
						  TIMESTAMPBITS_NO );
}

/******************************************************************

	Main multiplex iteration.
	Opens and closes all needed files and manages the correct
	call od the respective Video- and Audio- packet routines.
	The basic multiplexing is done here. Buffer capacity and 
	Timestamp checking is also done here, decision is taken
	wether we should genereate a Video-, Audio- or Padding-
	packet.
******************************************************************/


	
void OutputStream::OutputMultiplex ( VideoStream *e_vstrm,
									 AudioStream *e_astrm,
									 char *multi_file)

{
	segment_state seg_state;
	VAunit *next_vau;
	unsigned int audio_bytes;
	unsigned int video_bytes;

	int i;
	vector<ElementaryStream *>::iterator str;
	clockticks audio_next_SCR;
	clockticks video_next_SCR;
	vector<bool> completed;
	vector<bool>::iterator pcomp;
	bool video_ended = false;
	bool audio_ended = false;

	unsigned int packets_left_in_pack = 0; /* Suppress warning */
	bool padding_packet;
	bool start_of_new_pack;
	bool include_sys_header = false; /* Suppress warning */
	bool video_first = seg_starts_with_video;

	if( e_vstrm != 0 )
	{
		estreams.push_back(e_vstrm);
	}
	if( e_astrm != 0)
	{
		estreams.push_back(e_astrm);
	}

	for( str = estreams.begin(); str < estreams.end(); ++str )
	{
		switch( (*str)->Kind() )
		{
		case ElementaryStream::audio :
			astreams.push_back(*str);
			break;
		case ElementaryStream::video :
			vstreams.push_back(*str);
			break;
		}
		completed.push_back(false);
	}

	Init( multi_file );

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
	running_out = false;
	for(;;)
	{
		bool completion = true;

		for( str = estreams.begin(); str < estreams.end() ; ++str )
			completion &= (*str)->MuxCompleted();
		if( completion )
			break;

		/* A little state-machine for handling the transition from one
		   segment to the next 
		*/
		bool runout_incomplete;
		VideoStream *master;
		switch( seg_state )
		{

			/* Audio and slave video access units at end of segment.
			   If there are any audio AU's whose PTS implies they
			   should be played *before* the video AU starting the
			   next segement is presented we mux them out.  Once
			   they're gone we've finished this segment so we write
			   the suffix switch file, and start muxing a new segment.
			*/
		case runout_segment :
			runout_incomplete = false;
			for( str = estreams.begin(); str < estreams.end(); ++str )
			{
				runout_incomplete |= !(*str)->RunOutComplete();
			}

			running_out = false;

			if( runout_incomplete )
				break;

			/* Otherwise we write the stream suffix and start a new
			   stream file */
			OutputSuffix();
			psstrm->NextFile();


			seg_state = start_segment;

			/* Start a new segment... */

			/* Starting a new segment.
			   We send the segment prefix, video and audio reciever
			   buffers are assumed to start empty.  We reset the segment
			   length count and hence the SCR.
			   
			*/

		case start_segment :
			mjpeg_info( "New sequence commences...\n" );
			SetPosAndSCR(0);
			MuxStatus( LOG_INFO );

			for( str = estreams.begin(); str < estreams.end(); ++str )
			{
				(*str)->AllDemuxed();
			}

			OutputPrefix();

			/* The starting PTS/DTS of AU's may of course be
			   non-zero since this might not be the first segment
			   we've built. Hence we adjust the "delay" to
			   compensate for this as well as for any
			   synchronisation / start-up delay needed.  
			*/

			for( str = vstreams.begin(); str < vstreams.end(); ++str )
				(*str)->SetTSOffset(video_delay + current_SCR );
			for( str = astreams.begin(); str < astreams.end(); ++str )
				(*str)->SetTSOffset(audio_delay + current_SCR );
 
			packets_left_in_pack = packets_per_pack;
			include_sys_header = sys_header_in_pack1;
			buffers_in_video = always_buffers_in_video;
			video_first = seg_starts_with_video;
			pstrm.nsec = 0;
			for( str = estreams.begin(); str < estreams.end(); ++str )
				(*str)->nsec = 0;
			seg_state = mid_segment;
			break;

		case mid_segment :
			/* Once we exceed our file size limit, we need to
			   start a new file soon.  If we want a single stream we
			   simply switch.
				
			   Otherwise we're in the last gop of the current segment
			   (and need to start running streams out ready for a
			   clean continuation in the next segment).
			   TODO: runout_PTS really needs to be expressed in
			   sync delay adjusted units...
			*/
			
			master = 
				vstreams.size() > 0 ? 
				static_cast<VideoStream*>(vstreams[0]) : 0 ;
			if( psstrm->FileLimReached() )
			{
				if( opt_multifile_segment || master == 0 )
					psstrm->NextFile();
				else 
				{
					if( master->NextAUType() == IFRAME)
					{
						seg_state = runout_segment;
						runout_PTS = master->Lookahead()->PTS;
						mjpeg_info(" Runninng out to %lld\n", runout_PTS );
						running_out = true;
					}
				}
			}
			else if( master != 0 && master->EndSeq() )
			{
				if(  split_at_seq_end && master->Lookahead( ) != 0 )
				{
					if( ! master->SeqHdrNext() || 
						master->NextAUType() != IFRAME)
					{
						mjpeg_error_exit1( "Sequence split detected %d but no following sequence found...\n", master->NextAUType());
					}
						
					runout_PTS = master->Lookahead()->PTS;
					running_out = true;
					seg_state = runout_segment;
				}
			}
			break;
			
		}

		padding_packet = false;
		start_of_new_pack = (packets_left_in_pack == packets_per_pack); 

		for( str = estreams.begin(); str < estreams.end(); ++str )
		{
			(*str)->DemuxedTo(current_SCR);
		}

		if (start_of_new_pack)
		{
			/* Wir generieren den Pack Header				*/
			/* let's generate pack header					*/
			psstrm->CreatePack (&pack_header, current_SCR, mux_rate);
			pack_header_ptr = &pack_header;
			if( include_sys_header )
				sys_header_ptr = &sys_header;
			else
				sys_header_ptr = NULL;
				
		}
		else
			pack_header_ptr = NULL;

#ifdef ORIGINAL_CODE

		/* CASE: Audio Buffer OK, Audio Data ready
		   SEND An audio packet
		*/

		/* Heuristic... if we can we prefer to send audio rather than vstrm. 
		   Even a few uSec under-run are audible and in any case the data-rate
		   is trivial compared wth video. The only exception is if not
		   sending video would cause it to under-run but there's no danger of
		   and audio under-run
		   	   
		*/
		if ( (astrm->bufmodel.space()/*-AUDIO_BUFFER_FILL_MARGIN*/
			  > astrm->max_packet_data)
			 && !astrm->MuxCompleted()
			 && !(running_out && astrm->RunOutComplete())
			 && vstrm->nsec != 0
			 &&  ! (  !vstrm->MuxCompleted() &&
					  video_next_SCR >= vstrm->au->DTS+vstrm->timestamp_delay &&
					  audio_next_SCR < astrm->au->PTS+astrm->timestamp_delay
				 )
			)
		{
			/* Calculate actual time current AU is likely to arrive. */
			ByteposTimecode (bytes_output+audio_bytes, audio_next_SCR);
			if( audio_next_SCR >= astrm->au->PTS+astrm->timestamp_delay )
				timeout_error (STATUS_AUDIO_TIME_OUT,astrm->au->dorder);
			astrm->OutputSector();
			NextPosAndSCR();


		}

		/* CASE: Video Buffer OK, Video Data ready  (implicitly -  no audio packet to send 
		   SEND a video packet.
		*/

		else if( vstrm->bufmodel.space() >= vstrm->max_packet_data
				 && !vstrm->MuxCompleted() 
				 && !(running_out && vstrm->RunOutComplete())
			)
		{

			/* Calculate actual time current AU is likely to arrive. */
			ByteposTimecode (bytes_output+video_bytes, video_next_SCR);
			if( video_next_SCR >= vstrm->au->DTS+vstrm->timestamp_delay )
				timeout_error (STATUS_VIDEO_TIME_OUT,vstrm->au->dorder);
			vstrm->OutputSector ( );
			NextPosAndSCR();

		}

		/* CASE: Audio Buffer and Video Buffers NOT OK (too full to send)
		   SEND padding packet */
		else
		{

			OutputPadding (current_SCR, 
							start_of_new_pack, include_sys_header, vbr,
							false);
			padding_packet =true;
		}
#else
		
		//
		// Find the ready-to-mux stream with the most urgent DTS
		//
		ElementaryStream *despatch = 0;
		clockticks earliest;
		for( str = estreams.begin(); str < estreams.end(); ++str )
		{

/* DEBUG
			fprintf( stderr,"STREAM %02x: SCR=%lld mux=%d reqDTS=%lld\n", 
					 (*str)->stream_id,
					 current_SCR /300,
					 (*str)->MuxPossible(),
					 (*str)->RequiredDTS()/300
				);
*/
			if( (*str)->MuxPossible() && 
				( !video_first || (*str)->Kind() == ElementaryStream::video )
				 )
			{
				if( despatch == 0 || earliest > (*str)->RequiredDTS() )
				{
					despatch = *str;
					earliest = (*str)->RequiredDTS();
				}
			}
		}
		
		if( underrun_ignore > 0 )
			--underrun_ignore;

		if( despatch )
		{
			despatch->OutputSector();
			video_first = false;
			NextPosAndSCR();
			if( current_SCR >=  earliest && underrun_ignore == 0)
			{
				mjpeg_warn( "Stream %02x: Frame data under-run SCR=%lld DTS=%d (data will arrive too late to be useful)!\n", 
							despatch->stream_id, 
							current_SCR/300, 
							earliest/300 );
				MuxStatus( LOG_WARN );
				// Give the stream a chance to recover
				underrun_ignore = 300;
				++underruns;
				if( underruns > 10 && ! opt_ignore_underrun )
				{
					mjpeg_error_exit1("Too many frame drops -exiting\n" );
				}
			}
			padding_packet = false;

		}
		else
		{

			OutputPadding (current_SCR, 
							start_of_new_pack, include_sys_header, vbr,
							false);
			padding_packet =true;
		}

#endif
		/* Update the counter for pack packets.  VBR is a tricky 
		   case as here padding packets are "virtual" */
		
		if( ! (vbr && padding_packet) )
		{
			--packets_left_in_pack;
			if (packets_left_in_pack == 0) 
				packets_left_in_pack = packets_per_pack;
		}

		MuxStatus( LOG_DEBUG );
		/* Unless sys headers are always required we turn them off after the first
		   packet has been generated */
		include_sys_header = always_sys_header_in_pack;

#ifdef ORIGINAL_CODE
		if( !video_ended && vstrm->MuxCompleted() )
		{
			mjpeg_info( "Video stream ended.\n" );
			MuxStatus( LOG_DEBUG );
			video_ended = true;
		}

		if( !audio_ended && astrm->MuxCompleted() )
		{
			mjpeg_info( "Audio stream ended.\n" );
			MuxStatus( LOG_DEBUG );
			audio_ended = true;
		}
#else
		pcomp = completed.begin();
		str = estreams.begin();
		while( str < estreams.end() )
		{
			if( !(*pcomp) && (*str)->MuxCompleted() )
			{
				mjpeg_debug( "STREAM %d completed.\n", (*str)->stream_id );
				MuxStatus( LOG_DEBUG );
				(*pcomp) = true;
			}
			++str;
			++pcomp;
		}
#endif
	}
	// Tidy up
	
	OutputSuffix( );
	psstrm->Close();
	mjpeg_info( "Multiplex completion.\n");
	MuxStatus( LOG_INFO );
	for( str = estreams.begin(); str < estreams.end(); ++str )
	{
		(*str)->Close();
	}

    if( underruns> 0 )
	{
		mjpeg_error("\n");
		mjpeg_error_exit1( "MUX STATUS: Frame data under-runs detected!\n" );
	}
	else
	{
		mjpeg_info( "\n" );
		mjpeg_info( "MUX STATUS: no under-runs detected.\n");
	}
}

unsigned int OutputStream::PacketPayload( MuxStream &strm, bool buffers, 
										  bool PTSstamp, bool DTSstamp )
{
	return psstrm->PacketPayload( strm, sys_header_ptr, pack_header_ptr, 
								  buffers, 
								  PTSstamp, DTSstamp);
}


unsigned int OutputStream::WritePacket( unsigned int     max_packet_data_size,
										MuxStream        &strm,
										bool 	 buffers,
										clockticks   	 PTS,
										clockticks   	 DTS,
										uint8_t 	 timestamps
	)
{
	return psstrm->CreateSector ( pack_header_ptr,
								  sys_header_ptr,
								  max_packet_data_size,
								  strm,
								  buffers,
								  false,
								  PTS,
								  DTS,
								  timestamps );
}


/******************************************************************
	ElementaryStream::Muxed
    Updates buffer model and current access unit
	information from the look-ahead scanning buffer
    to account for bytes_muxed bytes being muxed out.
******************************************************************/



void ElementaryStream::Muxed (unsigned int bytes_muxed)
{
	clockticks   decode_time;
	VAunit *vau;
  
	if (bytes_muxed == 0 || MuxCompleted() )
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

	decode_time = au->DTS + timestamp_delay;
	while (au_unsent < bytes_muxed)
	{	  

		bufmodel.queued (au_unsent, decode_time);
		bytes_muxed -= au_unsent;
		if( !NextAU() )
			return;
		new_au_next_sec = true;
		decode_time = au->DTS + timestamp_delay;
	};

	// We've now reached a point where the current AU overran or
	// fitted exactly.  We need to distinguish the latter case
	// so we can record whether the next packet starts with an
	// existing AU or not - info we need to decide what PTS/DTS
	// info to write at the start of the next packet.
	
	if (au_unsent > bytes_muxed)
	{

		bufmodel.queued( bytes_muxed, decode_time);
		au_unsent -= bytes_muxed;
		new_au_next_sec = false;
	} 
	else //  if (au_unsent == bytes_muxed)
	{
		bufmodel.queued(bytes_muxed, decode_time);
		if( ! NextAU() )
			return;
		new_au_next_sec = true;
	}	   

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
	unsigned char timestamps;
	VAunit *vau;
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

	if( (muxinto.running_out && NextAUType() == IFRAME && 
		 Lookahead()->PTS >= muxinto.runout_PTS) ||
		(muxinto.sector_align_iframeAUs && SeqHdrNext() )
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
		if( dtspts_for_all_au && max_packet_payload == 0 )
			max_packet_payload = au_unsent;

		if (AUType() == BFRAME)
			timestamps=TIMESTAMPBITS_PTS;
		else
			timestamps=TIMESTAMPBITS_PTS_DTS;

		actual_payload =
			muxinto.WritePacket ( max_packet_payload,
								  *this,
								  buffers_in_header, PTS, DTS,
								  timestamps );
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
								  buffers_in_header, 0, 0,
								  TIMESTAMPBITS_NO );
		Muxed ( actual_payload );

	}

	/* CASE: Packet begins with old access unit, a new one	*/
	/*	     could begin in the very same packet			*/
	else /* if ( !new_au_next_sec  && 
			(au_unsent < old_au_then_new_payload)) */
	{
		prev_au_tail = au_unsent;
		//bufmodel.queued (au_unsent, DTS);
		Muxed( au_unsent );
		/* is there a new access unit anyway? */
		if( !MuxCompleted() )
		{
			if(  dtspts_for_all_au  && max_packet_payload == 0 )
				max_packet_payload = au_unsent+prev_au_tail;

			/* Set time-stamp bits appropriately if new AU
			   starts in packet */
			if( max_packet_payload > 0 )
				timestamps = TIMESTAMPBITS_NO;
			else if (AUType() == BFRAME)
				timestamps=TIMESTAMPBITS_PTS;
			else
				timestamps=TIMESTAMPBITS_PTS_DTS;
			new_au_next_sec = true;
			PTS = au->PTS + timestamp_delay;
			DTS = au->DTS + timestamp_delay;
	
			actual_payload = 
				muxinto.WritePacket ( max_packet_payload,
									  *this,
									  buffers_in_header, PTS, DTS,
									  timestamps );
			Muxed( actual_payload - prev_au_tail );
		} 
		else
		{
			(void) muxinto.WritePacket ( 0,
										 *this,
										 buffers_in_header, 0, 0,
										 TIMESTAMPBITS_NO);
		};


	}
	++nsec;
	buffers_in_header = always_buffers_in_header;
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
			new_au_next_sec = true;
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

// TODO Make into an elementary Stream type?
void OutputStream::OutputPadding (	clockticks SCR,
									bool start_of_new_pack,
									bool include_sys_header,
									bool VBR_pseudo,
									bool vcd_audio_pad
	)

{
	if( ! VBR_pseudo  )
	{
		/* let's generate the packet				*/
		if( vcd_audio_pad )
			psstrm->CreateSector ( pack_header_ptr, sys_header_ptr,
								   0,
								   vcdapstrm,
								   false, false,
								   0, 0,
								   TIMESTAMPBITS_NO );
		else
			psstrm->CreateSector ( pack_header_ptr, sys_header_ptr,
								   0,
								   pstrm,
								   false, false,
								   0, 0,
								   TIMESTAMPBITS_NO );
		++pstrm.nsec;
	}
	NextPosAndSCR();

}

