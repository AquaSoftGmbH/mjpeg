
#include <config.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>

#include <mjpeg_types.h>
#include <mjpeg_logging.h>
#include <format_codes.h>

#include "interact.hh"
#include "videostrm.hh"
#include "stillsstream.hh"
#include "audiostrm.hh"
#include "multiplexor.hh"

/****************
 *
 * Constructor - sets up per-run stuff and initialised parameters
 * that control syntax of generated stream from the job options set
 * by the user.
 *
 ***************/

Multiplexor::Multiplexor(MultiplexJob &job)
{
    underrun_ignore = 0;
    underruns = 0;
    InitSyntaxParameters(job);
    InitInputStreams(job);
}


/******************************************************************
 *
 * Initialisation of stream syntax paramters based on selected user
 * options.  Depending of mux_format some selections may only act as
 * defaults or may simply be ignored if they are inconsistent with the
 * selected output format.
 *
 ******************************************************************/


void Multiplexor::InitSyntaxParameters(MultiplexJob &job)
{
    output_filename_pattern = job.outfile_pattern;
	seg_starts_with_video = false;
	audio_buffer_size = 4 * 1024;
    mux_format = job.mux_format;
    vbr = job.VBR;
    packets_per_pack = job.packets_per_pack;
    data_rate = job.data_rate;
    mpeg = job.mpeg;
    always_sys_header_in_pack = job.always_system_headers;
    sector_transport_size = job.sector_size;
    sector_size = job.sector_size;
	split_at_seq_end = !job.multifile_segment;
    max_segment_size = job.max_segment_size;
    max_PTS = static_cast<clockticks>(job.max_PTS) * CLOCKS;
	video_delay = static_cast<clockticks>(job.video_offset*CLOCKS/1000);
	audio_delay = static_cast<clockticks>(job.audio_offset*CLOCKS/1000);
 	switch( mux_format  )
	{
	case MPEG_FORMAT_VCD :
		data_rate = 75*2352;  			 /* 75 raw CD sectors/sec */ 
	  	vbr = false;
 
	case MPEG_FORMAT_VCD_NSR : /* VCD format, non-standard rate */
		mjpeg_info( "Selecting VCD output profile");
		mpeg = 1;
	 	packets_per_pack = 1;
	  	sys_header_in_pack1 = 0;
	  	always_sys_header_in_pack = 0;
	  	sector_transport_size = 2352;	      /* Each 2352 bytes with 2324 bytes payload */
	  	transport_prefix_sectors = 30;
	  	sector_size = 2324;
		buffers_in_video = 1;
		always_buffers_in_video = 0;
		buffers_in_audio = 1;   		// This is needed as otherwise we have
		always_buffers_in_audio = 1;	//  to stuff the packer header which 
                                        // must be 13 bytes for VCD audio
		vcd_zero_stuffing = 20;         // The famous 20 zero bytes for VCD
                                        // audio sectors.
		dtspts_for_all_vau = false;
		sector_align_iframeAUs = false;
        timestamp_iframe_only = false;
		seg_starts_with_video = true;
		break;
		
	case  MPEG_FORMAT_MPEG2 : 
		mjpeg_info( "Selecting generic MPEG2 output profile");
		mpeg = 2;
	 	packets_per_pack = 1;
	  	sys_header_in_pack1 = 1;
	  	always_sys_header_in_pack = 0;
	  	sector_transport_size = 2048;	      /* Each 2352 bytes with 2324 bytes payload */
	  	transport_prefix_sectors = 0;
	  	sector_size = 2048;
		buffers_in_video = 1;
		always_buffers_in_video = 0;
		buffers_in_audio = 1;
		always_buffers_in_audio = 1;
		vcd_zero_stuffing = 0;
        dtspts_for_all_vau = 0;
		sector_align_iframeAUs = false;
        timestamp_iframe_only = false;
        video_buffers_iframe_only = false;
		break;

	case MPEG_FORMAT_SVCD :
		data_rate = 150*2324;

	case MPEG_FORMAT_SVCD_NSR :		/* Non-standard data-rate */
		mjpeg_info( "Selecting SVCD output profile");
		mpeg = 2;
	 	packets_per_pack = 1;
	  	sys_header_in_pack1 = 0;
	  	always_sys_header_in_pack = 0;
	  	sector_transport_size = 2324;
	  	transport_prefix_sectors = 0;
	  	sector_size = 2324;
		vbr = true;
		buffers_in_video = 1;
		always_buffers_in_video = 0;
		buffers_in_audio = 1;
		always_buffers_in_audio = 0;
		vcd_zero_stuffing = 0;
        dtspts_for_all_vau = 0;
		sector_align_iframeAUs = true;
		seg_starts_with_video = true;
        timestamp_iframe_only = false;
        video_buffers_iframe_only = false;
		break;

	case MPEG_FORMAT_VCD_STILL :
		data_rate = 75*2352;  			 /* 75 raw CD sectors/sec */ 
	  	vbr = false;
		mpeg = 1;
		split_at_seq_end = false;
	 	packets_per_pack = 1;
	  	sys_header_in_pack1 = 0;
	  	always_sys_header_in_pack = 0;
	  	sector_transport_size = 2352;	      /* Each 2352 bytes with 2324 bytes payload */
	  	transport_prefix_sectors = 0;
	  	sector_size = 2324;
		buffers_in_video = 1;
		always_buffers_in_video = 0;
		buffers_in_audio = 1;
		always_buffers_in_audio = 0;
		vcd_zero_stuffing = 20;
		dtspts_for_all_vau = 1;
		sector_align_iframeAUs = true;
        timestamp_iframe_only = false;
        video_buffers_iframe_only = false;
#ifdef JUNK
		if( opt_buffer_size == 0 )
			opt_buffer_size = 46;
		else if( opt_buffer_size > 220 )
		{
			mjpeg_error_exit1("VCD stills has max. permissible video buffer size of 220KB");
		}
		else
		{
			/* Add a margin for sequence header overheads for HR stills */
			/* So the user simply specifies the nominal size... */
			opt_buffer_size += 4;
		}		
		video_buffer_size = opt_buffer_size*1024;
#endif
		break;

	case MPEG_FORMAT_SVCD_STILL :
		mjpeg_info( "Selecting SVCD output profile");
		if( data_rate == 0 )
			data_rate = 150*2324;
		mpeg = 2;
	 	packets_per_pack = 1;
	  	sys_header_in_pack1 = 0;
	  	always_sys_header_in_pack = 0;
	  	sector_transport_size = 2324;
	  	transport_prefix_sectors = 0;
	  	sector_size = 2324;
		vbr = true;
		buffers_in_video = 1;
		always_buffers_in_video = 0;
		buffers_in_audio = 1;
		always_buffers_in_audio = 0;
		vcd_zero_stuffing = 0;
        dtspts_for_all_vau = 0;
		sector_align_iframeAUs = true;
        timestamp_iframe_only = false;
        video_buffers_iframe_only = false;
		break;

    case MPEG_FORMAT_DVD :
		mjpeg_info( "Selecting DVD output profile (INCOMPLETE!!!!)");
        data_rate = 1260000;
		mpeg = 2;
	 	packets_per_pack = 1;
	  	sys_header_in_pack1 = false; // Handle by control packets
	  	always_sys_header_in_pack = false;
	  	sector_transport_size = 2048;
	  	transport_prefix_sectors = 0;
	  	sector_size = 2048;
		buffers_in_video = true;
		always_buffers_in_video = false;
		buffers_in_audio = true;
		always_buffers_in_audio = false;
		vcd_zero_stuffing = 0;
        dtspts_for_all_vau = 0;
		sector_align_iframeAUs = true;
        timestamp_iframe_only = true;
        video_buffers_iframe_only = true;
		vbr = true;
        if( max_segment_size == 0 )
            max_segment_size = 2000*1024*1024;
        break;
			 
	default : /* MPEG_FORMAT_MPEG1 - auto format MPEG1 */
		mjpeg_info( "Selecting generic MPEG1 output profile");
		mpeg = 1;
		sys_header_in_pack1 = 1;
		transport_prefix_sectors = 0;
		buffers_in_video = 1;
		always_buffers_in_video = 1;
		buffers_in_audio = 0;
		always_buffers_in_audio = 1;
		vcd_zero_stuffing = 0;
        dtspts_for_all_vau = 0;
		sector_align_iframeAUs = false;
        timestamp_iframe_only = false;
        video_buffers_iframe_only = false;
		break;
	}
	
}

/**************************************
 *
 * Initialise the elementary stream readers / output sector formatter
 * objects for the various kinds of input stream.
 *
 *************************************/

void Multiplexor::InitInputStreams(MultiplexJob &job)
{
    //
    // S(VCD) Stills are sufficiently unusual to require their own
    // special initialisation
    //
	if( MPEG_STILLS_FORMAT(job.mux_format) )
        InitInputStreamsForStills( job );
    else
        InitInputStreamsForVideo( job );
}

void Multiplexor::InitInputStreamsForStills(MultiplexJob & job )
{
	vector<VideoParams *>::iterator vidparm = job.video_param.begin();
    unsigned int frame_interval;
    unsigned int i;

    switch( job.mux_format )
    {
    case MPEG_FORMAT_VCD_STILL :
        mjpeg_info( "Multiplexing VCD stills program stream!" );

        frame_interval = 30; // 30 Frame periods
        if( job.mpa_files.size() > 0 && job.video_files.size() > 2  )
            mjpeg_error_exit1("VCD stills: no more than two streams (one normal one hi-res) possible");
        {
            VCDStillsStream *str[2];
            ConstantFrameIntervals *intervals[2];

            for( i = 0; i<job.video_files.size(); ++i )
            {
                FrameIntervals *ints = 
                    new ConstantFrameIntervals( frame_interval );
                str[i] = 
                    new VCDStillsStream( *job.video_files[i],
                                         new StillsParams( *vidparm, ints),
                                         *this );
                estreams.push_back( str[i] );
                vstreams.push_back( str[i] );
                str[i]->Init();
                ++vidparm;
            }
            if( job.video_files.size() == 2 )
            {
                str[0]->SetSibling(str[1]);
                str[1]->SetSibling(str[0]);
            }
        }
        break;
    case MPEG_FORMAT_SVCD_STILL :
        mjpeg_info( "Multiplexing SVCD stills program stream!" );
        frame_interval = 30;

        if( job.video_files.size() > 1 )
        {
            mjpeg_error_exit1("SVCD stills streams may only contain a single video stream");
        }
        else if(  job.video_files.size() > 0 )
        {
            ConstantFrameIntervals *intervals;
            StillsStream *str;
            intervals = new ConstantFrameIntervals( frame_interval );
            str = new StillsStream( *job.video_files[0],
                                    new StillsParams( *vidparm, intervals ),
                                    *this );
            estreams.push_back( str );
            vstreams.push_back( str );
            str->Init();
        }
        for( i = 0 ; i < job.mpa_files.size() ; ++i )
        {
            AudioStream *audioStrm = new MPAStream( *job.mpa_files[i], *this);
            audioStrm->Init ( i);
            estreams.push_back(audioStrm);
            astreams.push_back(audioStrm);
        }
        break;
    default:
        mjpeg_error_exit1("Only VCD and SVCD stills format for the moment...");
    }

}

void Multiplexor::InitInputStreamsForVideo(MultiplexJob & job )
{
    mjpeg_info( "Multiplexing video program stream!" );

	vector<VideoParams *>::iterator vidparm = job.video_param.begin();
	vector<LpcmParams *>::iterator lpcmparm = job.lpcm_param.begin();
    unsigned int i;

    if( job.video_files.size() < 1 	&& job.mux_format == MPEG_FORMAT_VCD )
    {
        mjpeg_warn( "Multiplexing audio-only for a standard VCD is very inefficient");
    }

    for( i = 0 ; i < job.video_files.size() ; ++i )
    {
        VideoStream *videoStrm;
        //
        // The first DVD video stream is made the master stream...
        //
        if( i == 0 && job.mux_format ==  MPEG_FORMAT_DVD )
            videoStrm = new DVDVideoStream( *job.video_files[i], 
                                            *vidparm,
                                            *this);
        else
            videoStrm = new VideoStream( *job.video_files[i],
                                         *vidparm,
                                         *this);
        videoStrm->Init( i );
        estreams.push_back( videoStrm );
        vstreams.push_back( videoStrm );
        ++vidparm;

    }
    for( i = 0 ; i < job.mpa_files.size() ; ++i )
    {
        AudioStream *audioStrm = 
            new MPAStream( *job.mpa_files[i], *this);
        audioStrm->Init ( i );
        estreams.push_back(audioStrm);
        astreams.push_back(audioStrm);
        
    }
    for( i = 0 ; i < job.ac3_files.size() ; ++i )
    {
        AudioStream *audioStrm = 
            new AC3Stream( *job.ac3_files[i], *this);
        audioStrm->Init ( i );
        estreams.push_back(audioStrm);
        astreams.push_back(audioStrm);
    }
    for( i = 0 ; i < job.lpcm_files.size() ; ++i )
    {
        AudioStream *audioStrm = 
            new LPCMStream( *job.lpcm_files[i], *lpcmparm, *this);
        audioStrm->Init ( i );
        estreams.push_back(audioStrm);
        astreams.push_back(audioStrm);
        ++lpcmparm;
    }

}


/******************************************************************* 
	Find the timecode corresponding to given position in the system stream
   (assuming the SCR starts at 0 at the beginning of the stream 
@param bytepos byte position in the stream
@param ts returns the number of clockticks the bytepos is from the file start    
****************************************************************** */

void Multiplexor::ByteposTimecode(bitcount_t bytepos, clockticks &ts)
{
	ts = (bytepos*CLOCKS)/static_cast<bitcount_t>(dmux_rate);
}


/**********
 *
 * UpdateSectorHeaders - Update the sector headers after a change in stream
 *                     position / SCR
 **********

/**********
 *
 * NextPosAndSCR - Update nominal (may be >= actual) byte count
 * and SCR to next output sector.
 *
 ********/

void Multiplexor::NextPosAndSCR()
{
	bytes_output += sector_transport_size;
	ByteposTimecode( bytes_output, current_SCR );
    if (start_of_new_pack)
    {
        psstrm->CreatePack (&pack_header, current_SCR, mux_rate);
        pack_header_ptr = &pack_header;
        if( include_sys_header )
            sys_header_ptr = &sys_header;
        else
            sys_header_ptr = NULL;
        
    }
    else
        pack_header_ptr = NULL;
}


/**********
 *
 * SetPosAndSCR - Update nominal (may be >= actual) byte count
 * and SCR to next output sector.
 * @param bytepos byte position in the stream
 ********/

void Multiplexor::SetPosAndSCR( bitcount_t bytepos )
{
	bytes_output = bytepos;
	ByteposTimecode( bytes_output, current_SCR );
    if (start_of_new_pack)
    {
        psstrm->CreatePack (&pack_header, current_SCR, mux_rate);
        pack_header_ptr = &pack_header;
        if( include_sys_header )
            sys_header_ptr = &sys_header;
        else
            sys_header_ptr = NULL;
        
    }
    else
        pack_header_ptr = NULL;
}

/* 
   Stream syntax parameters.
*/
		
	



typedef enum { start_segment, mid_segment, 
			   runout_segment }
segment_state;


/**
 * Compute the number of run-in sectors needed to fill up the buffers to
 * suit the type of stream being muxed.
 *
 * For stills we have to ensure an entire buffer is loaded as we only
 * ever process one frame at a time.
 * @returns the number of run-in sectors needed to fill up the buffers to suit the type of stream being muxed.
 */

unsigned int Multiplexor::RunInSectors()
{
	vector<ElementaryStream *>::iterator str;
	unsigned int sectors_delay = 1;

	for( str = vstreams.begin(); str < vstreams.end(); ++str )
	{

		if( MPEG_STILLS_FORMAT( mux_format ) )
		{
			sectors_delay += (unsigned int)(1.02*(*str)->BufferSize()) / sector_size+2;
		}
		else if( vbr )
			sectors_delay += 3*(*str)->BufferSize() / ( 4 * sector_size );
		else
			sectors_delay += 5 *(*str)->BufferSize() / ( 6 * sector_size );
	}
    sectors_delay += astreams.size();
	return sectors_delay;
}

/**********************************************************************
 *
 *  Initializes the output stream proper. Traverses the input files
 *  and calculates their payloads.  Estimates the multiplex
 *  rate. Estimates the necessary stream delay for the different
 *  substreams.
 *
 *********************************************************************/


void Multiplexor::Init()
{
	vector<ElementaryStream *>::iterator str;
	clockticks delay;
	unsigned int sectors_delay;

	Pack_struc 			dummy_pack;
	Sys_header_struc 	dummy_sys_header;	
	Sys_header_struc *sys_hdr;
	unsigned int nominal_rate_sum;
	
	mjpeg_info("SYSTEMS/PROGRAM stream:");
	psstrm = new PS_Stream(mpeg, sector_size,
                           output_filename_pattern, max_segment_size );
	
    /* These are used to make (conservative) decisions
	   about whether a packet should fit into the recieve buffers... 
	   Audio packets always have PTS fields, video packets needn'.	
	   TODO: Really this should be encapsulated in Elementary stream...?
	*/ 
	psstrm->CreatePack (&dummy_pack, 0, mux_rate);
	if( always_sys_header_in_pack )
	{
        vector<MuxStream *> muxstreams;
        AppendMuxStreamsOf( estreams, muxstreams );
		psstrm->CreateSysHeader (&dummy_sys_header, mux_rate,  
								 !vbr, 1,  true, true, muxstreams);
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
			mjpeg_error_exit1("INTERNAL: Only audio and video payload calculations implemented!");
			
		}

		if( (*str)->NominalBitRate() == 0 && data_rate == 0)
			mjpeg_error_exit1( "Variable bit-rate stream present: output stream (max) data-rate *must* be specified!");
		nominal_rate_sum += (*str)->NominalBitRate();

	}
		
	/* Attempt to guess a sensible mux rate for the given video and *
	 audio estreams. This is a rough and ready guess for MPEG-1 like
	 formats. */
	   
	 
	dmux_rate = static_cast<int>(1.015 * nominal_rate_sum);
	dmux_rate = (dmux_rate/50 + 25)*50/8;
	
	mjpeg_info ("rough-guess multiplexed stream data rate    : %07d", dmux_rate*8 );
	if( data_rate != 0 )
		mjpeg_info ("target data-rate specified               : %7d", data_rate*8 );

	if( data_rate == 0 )
	{
		mjpeg_info( "Setting best-guess data rate.");
	}
	else if ( data_rate >= dmux_rate)
	{
		mjpeg_info( "Setting specified specified data rate: %7d", data_rate*8 );
		dmux_rate = data_rate;
	}
	else if ( data_rate < dmux_rate )
	{
		mjpeg_warn( "Target data rate lower than computed requirement!");
		mjpeg_warn( "N.b. a 20%% or so discrepancy in variable bit-rate");
		mjpeg_warn( "streams is common and harmless provided no time-outs will occur"); 
		dmux_rate = data_rate;
	}

	mux_rate = dmux_rate/50;

	/* To avoid Buffer underflow, the DTS of the first video and audio AU's
	   must be offset sufficiently	forward of the SCR to allow the buffer 
	   time to fill before decoding starts. Calculate the necessary delays...
	*/

	sectors_delay = RunInSectors();

	ByteposTimecode( 
		static_cast<bitcount_t>(sectors_delay*sector_transport_size),
		delay );
    video_delay += delay;
    audio_delay += delay;

	mjpeg_info( "Run-in Sectors = %d Video delay = %lld Audio delay = %lld",
				sectors_delay,
				 video_delay / 300,
				 audio_delay / 300 );

    if( max_PTS != 0 )
        
        mjpeg_info( "Multiplexed stream will be ended at %lld seconds playback time\n", max_PTS/CLOCKS );

	//
	// Now that all mux parameters are set we can trigger parsing
	// of actual input stream data and calculation of associated 
	// PTS/DTS by causing the read of the first AU's...
	//
	for( str = estreams.begin(); str < estreams.end(); ++str )
	{
		(*str)->NextAU();
	}

    //
    // Now that we have both output and input streams initialised and
    // data-rates set we can make a decent job of setting the maximum
    // STD buffer delay in video streams.
    //
   
    for( str = vstreams.begin(); str < vstreams.end(); ++str )
    {
        static_cast<VideoStream*>(*str)->SetMaxStdBufferDelay( dmux_rate );
    }				 
}

/**
   Prints the current status of the substreams. 
   @param level the desired log level 
 */
void Multiplexor::MuxStatus(log_level_t level)
{
	vector<ElementaryStream *>::iterator str;
	for( str = estreams.begin(); str < estreams.end(); ++str )
	{
		switch( (*str)->Kind()  )
		{
		case ElementaryStream::video :
			mjpeg_log( level,
					   "Video %02x: buf=%7d frame=%06d sector=%08d",
					   (*str)->stream_id,
					   (*str)->bufmodel.Space(),
					   (*str)->au->dorder,
					   (*str)->nsec
				);
			break;
		case ElementaryStream::audio :
			mjpeg_log( level,
					   "Audio %02x: buf=%7d frame=%06d sector=%08d",
					   (*str)->stream_id,
					   (*str)->bufmodel.Space(),
					   (*str)->au->dorder,
					   (*str)->nsec
				);
			break;
		default :
			mjpeg_log( level,
					   "Other %02x: buf=%7d sector=%08d",
					   (*str)->stream_id,
					   (*str)->bufmodel.Space(),
					   (*str)->nsec
				);
			break;
		}
	}
	if( !vbr )
		mjpeg_log( level,
				   "Padding : sector=%08d",
				   pstrm.nsec
			);
	
	
}


/**
   Append input substreams to the output multiplex stream.
 */
void Multiplexor::AppendMuxStreamsOf( vector<ElementaryStream *> &elem, 
                                       vector<MuxStream *> &mux )
{
    vector<ElementaryStream *>::iterator str;
    for( str = elem.begin(); str < elem.end(); ++str )
    {
        mux.push_back( static_cast<MuxStream *>( *str ) );
    }
}

/******************************************************************
    Program start-up packets.  Generate any irregular packets						
needed at the start of the stream...
	Note: *must* leave a sensible in-stream system header in
	sys_header.
	TODO: get rid of this grotty sys_header global.
******************************************************************/
void Multiplexor::OutputPrefix( )
{
	vector<ElementaryStream *>::iterator str;
    vector<MuxStream *> vmux,amux,emux;
    AppendMuxStreamsOf( vstreams, vmux );
    AppendMuxStreamsOf( astreams, amux );
    AppendMuxStreamsOf( estreams, emux );

	/* Deal with transport padding */
	SetPosAndSCR( bytes_output + 
				  transport_prefix_sectors*sector_transport_size );
	
	/* VCD: Two padding packets with video and audio system headers */

	switch (mux_format)
	{
	case MPEG_FORMAT_VCD :
	case MPEG_FORMAT_VCD_NSR :

		/* Annoyingly VCD generates seperate system headers for
		   audio and video ... DOH... */
		if( astreams.size() > 1 || vstreams.size() > 1 ||
			astreams.size() + vstreams.size() != estreams.size() )
		{
				mjpeg_error_exit1("VCD man only have max. 1 audio and 1 video stream");
		}
		/* First packet carries video-info-only sys_header */
		psstrm->CreateSysHeader (&sys_header, mux_rate, 
								 false, true, 
								 true, true, vmux  );
		sys_header_ptr = &sys_header;
		pack_header_ptr = &pack_header;
	  	OutputPadding( false);		

		/* Second packet carries audio-info-only sys_header */
		psstrm->CreateSysHeader (&sys_header, mux_rate,  
                                 false, true, 
								 true, true, amux );
		sys_header_ptr = &sys_header;
		pack_header_ptr = &pack_header;
	  	OutputPadding( true );
		break;
		
	case MPEG_FORMAT_SVCD :
	case MPEG_FORMAT_SVCD_NSR :
		/* First packet carries sys_header */
		psstrm->CreateSysHeader (&sys_header, mux_rate,  !vbr, true, 
                                 true, true, emux );
		sys_header_ptr = &sys_header;
		pack_header_ptr = &pack_header;
	  	OutputPadding(false);
        break;

	case MPEG_FORMAT_VCD_STILL :
		/* First packet carries small-still sys_header */
		/* TODO No support mixed-mode stills sequences... */
		psstrm->CreateSysHeader (&sys_header, mux_rate, false, false,
								 true, true, emux );
		sys_header_ptr = &sys_header;
		pack_header_ptr = &pack_header;
		OutputPadding(  false);	
        break;
			
	case MPEG_FORMAT_SVCD_STILL :
		/* TODO: Video only at present */
		/* First packet carries video-info-only sys_header */
		psstrm->CreateSysHeader (&sys_header, mux_rate, 
								 false, true, 
								 true, true, vmux );
		sys_header_ptr = &sys_header;
		pack_header_ptr = &pack_header;
	  	OutputPadding( false);		
		break;

    case MPEG_FORMAT_DVD :
        /* A DVD System header is a weird thing.  We seem to need to
           include buffer info about streams 0xb8, 0xb9, 0xbf even if
           they're not physically present but the buffers for the actual
           video streams aren't included.  
        */
    {
        DummyMuxStream dvd_0xb9_strm_dummy( 0xb9, 1, 230 );
        DummyMuxStream dvd_0xb8_strm_dummy( 0xb8, 0, 4096 );
        DummyMuxStream dvd_0xbf_strm_dummy( 0xbf, 1, 2048 );
        vector<MuxStream *> dvdmux;
        vector<MuxStream *>::iterator muxstr;
        dvdmux.push_back( &dvd_0xb9_strm_dummy );
        dvdmux.push_back( &dvd_0xb8_strm_dummy );
        unsigned int max_priv1_buffer = 0;
        for( muxstr = amux.begin(); muxstr < amux.end(); ++muxstr )
        {
            // We mux *many* substreams on PRIVATE_STR_1
            // we set the system header buffer size to the maximum
            // of all those we find
            if( (*muxstr)->stream_id == PRIVATE_STR_1 
                && (*muxstr)->BufferSize() > max_priv1_buffer )
            {
                max_priv1_buffer = (*muxstr)->BufferSize();
            }
            else
                dvdmux.push_back( *muxstr );
        }
        
        DummyMuxStream dvd_priv1_strm_dummy( PRIVATE_STR_1, 1, 
                                             max_priv1_buffer );
        if( max_priv1_buffer > 0 )
            dvdmux.push_back( &dvd_priv1_strm_dummy );

        dvdmux.push_back( &dvd_0xbf_strm_dummy );
        psstrm->CreateSysHeader (&sys_header, mux_rate, !vbr, false, 
                                 true, true, dvdmux );
        sys_header_ptr = &sys_header;
        pack_header_ptr = &pack_header;
        /* It is then followed up by a pair of PRIVATE_STR_2 packets which
            we keep empty 'cos we don't know what goes there...
        */
    }
    break;

    default :
        /* Create the in-stream header in case it is needed */
        psstrm->CreateSysHeader (&sys_header, mux_rate, !vbr, false, 
                                 true, true, emux );


	}



}



/******************************************************************
    Program shutdown packets.  Generate any irregular packets
    needed at the end of the stream...
   
******************************************************************/

void Multiplexor::OutputSuffix()
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


	
void Multiplexor::Multiplex()

{
	segment_state seg_state;
	VAunit *next_vau;
	unsigned int audio_bytes;
	unsigned int video_bytes;

	int i;
	vector<bool> completed;
	vector<bool>::iterator pcomp;
	vector<ElementaryStream *>::iterator str;
	bool video_ended = false;
	bool audio_ended = false;

	unsigned int packets_left_in_pack = 0; /* Suppress warning */
	bool padding_packet;
	bool video_first;

	Init( );

    for(i = 0; i < estreams.size() ; ++i )
		completed.push_back(false);




    
    
	/*  Let's try to read in unit after unit and to write it out into
		the outputstream. The only difficulty herein lies into the
		buffer management, and into the fact the the actual access
		unit *has* to arrive in time, that means the whole unit
		(better yet, packet data), has to arrive before arrival of
		DTS. If both buffers are full we'll generate a padding packet
	  
		Of course, when we start we're starting a new segment with no
		bytes output...
	*/

	ByteposTimecode( sector_transport_size, ticks_per_sector );
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

			if( runout_incomplete )
				break;

			/* Otherwise we write the stream suffix and start a new
			   stream file */
			OutputSuffix();
			psstrm->NextFile();

			running_out = false;
			seg_state = start_segment;

			/* Starting a new segment.
			   We send the segment prefix, video and audio reciever
			   buffers are assumed to start empty.  We reset the segment
			   length count and hence the SCR.
			   
			*/

		case start_segment :
			mjpeg_info( "New sequence commences..." );
			SetPosAndSCR(0);
			MuxStatus( LOG_INFO );

			for( str = estreams.begin(); str < estreams.end(); ++str )
			{
				(*str)->AllDemuxed();
			}

			packets_left_in_pack = packets_per_pack;
            start_of_new_pack = true;
			include_sys_header = sys_header_in_pack1;
			buffers_in_video = always_buffers_in_video;
			video_first = seg_starts_with_video & (vstreams.size() > 0);
			OutputPrefix();

			/* Set the offset applied to the raw PTS/DTS of AU's to
               make the DTS of the first AU in the master (video) stream
               precisely the video delay plus whatever time we wasted in
               the sequence pre-amble.

               The DTS of the remaining streams are set so that
               (modulo the relevant delay offset) they maintain the
               same relative timing to the master stream.
               
			*/

            clockticks ZeroSCR;

            if( vstreams.size() != 0 )
                ZeroSCR = vstreams[0]->au->DTS;
            else
                ZeroSCR = estreams[0]->au->DTS;

			for( str = vstreams.begin(); str < vstreams.end(); ++str )
				(*str)->SetSyncOffset(video_delay + current_SCR - ZeroSCR );
			for( str = astreams.begin(); str < astreams.end(); ++str )
				(*str)->SetSyncOffset(audio_delay + current_SCR - ZeroSCR );
			pstrm.nsec = 0;
			for( str = estreams.begin(); str < estreams.end(); ++str )
				(*str)->nsec = 0;
			seg_state = mid_segment;
            //for( str = estreams.begin(); str < estreams.end(); ++str )
            //{
            //mjpeg_info("STREAM %02x: SCR=%lld mux=%d reqDTS=%lld", 
            //(*str)->stream_id,
            //current_SCR /300,
            //(*str)->MuxPossible(current_SCR),
            //(*str)->RequiredDTS()/300
            //);
            //}
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
				if( multifile_segment || master == 0 )
					psstrm->NextFile();
				else 
				{
					if( master->NextAUType() == IFRAME)
					{
						seg_state = runout_segment;
						runout_PTS = master->NextRequiredPTS();
						mjpeg_debug("Running out to (raw) PTS %lld SCR=%lld", 
								   runout_PTS/300, current_SCR/300 );
						running_out = true;
						seg_state = runout_segment;
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
						mjpeg_error_exit1( "Sequence split detected %d but no following sequence found...", master->NextAUType());
					}
						
					runout_PTS = master->NextRequiredPTS();
                    mjpeg_debug("Running out to %lld SCR=%lld", 
                                runout_PTS/300, 
                                current_SCR/300 );
                    MuxStatus( LOG_INFO );
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


		
		//
		// Find the ready-to-mux stream with the most urgent DTS
		//
		ElementaryStream *despatch = 0;
		clockticks earliest;
		for( str = estreams.begin(); str < estreams.end(); ++str )
		{
            /*
                mjpeg_info("STREAM %02x: SCR=%lld mux=%d reqDTS=%lld", 
                           (*str)->stream_id,
                           current_SCR /300,
                           (*str)->MuxPossible(current_SCR),
                           (*str)->RequiredDTS()/300
				);
            */
			if( (*str)->MuxPossible(current_SCR) && 
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
			despatch->BufferAndOutputSector();
			video_first = false;
			if( current_SCR >=  earliest && underrun_ignore == 0)
			{
				mjpeg_warn( "Stream %02x: data will arrive too late sent(SCR)=%lld required(DTS)=%d", 
							despatch->stream_id, 
							current_SCR/300, 
							earliest/300 );
				MuxStatus( LOG_WARN );
				// Give the stream a chance to recover
				underrun_ignore = 300;
				++underruns;
				if( underruns > 10  )
				{
					mjpeg_error_exit1("Too many frame drops -exiting" );
				}
			}
            if( despatch->nsec > 50 &&
                despatch->Lookahead( ) != 0 && ! running_out)
                despatch->UpdateBufferMinMax();
			padding_packet = false;

		}
		else
		{
            //
            // If we got here no stream could be muxed out.
            // We therefore generate padding packets if necessary
            // usually this is because reciever buffers are likely to be
            // full.  
            //
            if( vbr )
            {
                //
                // VBR: For efficiency we bump SCR up to five times or
                // until it looks like buffer status will change
                NextPosAndSCR();
                clockticks next_change = static_cast<clockticks>(0);
                for( str = estreams.begin(); str < estreams.end(); ++str )
                {
                    clockticks change_time = (*str)->bufmodel.NextChange();
                    if( next_change == 0 || change_time < next_change )
                        next_change = change_time;
                }
                unsigned int bumps = 5;
                while( bumps > 0 
                       && next_change > current_SCR + ticks_per_sector)
                {
                    NextPosAndSCR();
                    --bumps;
                }
            }
            else
            {
                // Just output a padding packet
                OutputPadding (	false);
            }
			padding_packet = true;
		}

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

		pcomp = completed.begin();
		str = estreams.begin();
		while( str < estreams.end() )
		{
			if( !(*pcomp) && (*str)->MuxCompleted() )
			{
				mjpeg_info( "STREAM %02x completed @ %d.", (*str)->stream_id, (*str)->au->dorder );
				MuxStatus( LOG_DEBUG );
				(*pcomp) = true;
			}
			++str;
			++pcomp;
		}
	}
	// Tidy up
	
	OutputSuffix( );
	psstrm->Close();
	mjpeg_info( "Multiplex completion at SCR=%lld.", current_SCR/300);
	MuxStatus( LOG_INFO );
	for( str = estreams.begin(); str < estreams.end(); ++str )
	{
		(*str)->Close();
        if( (*str)->nsec <= 50 )
            mjpeg_info( "BUFFERING stream too short for useful statistics");
        else
            mjpeg_info( "BUFFERING min %d Buf max %d",
                        (*str)->BufferMin(),
                        (*str)->BufferMax() 
                );
	}

    if( underruns> 0 )
	{
		mjpeg_error("");
		mjpeg_error_exit1( "MUX STATUS: Frame data under-runs detected!" );
	}
	else
	{
		mjpeg_info( "" );
		mjpeg_info( "MUX STATUS: no under-runs detected.");
	}
}

/**
   Calculate the packet payload of the output stream at a certain timestamp. 
@param strm the output stream
@param buffers the number of buffers
@param PTSstamp presentation time stamp
@param DTSstamp decoding time stamp
 */
unsigned int Multiplexor::PacketPayload( MuxStream &strm, bool buffers, 
										  bool PTSstamp, bool DTSstamp )
{
	return psstrm->PacketPayload( strm, sys_header_ptr, pack_header_ptr, 
								  buffers, 
								  PTSstamp, DTSstamp)
        - strm.StreamHeaderSize();
}

/***************************************************

  WritePacket - Write out a normal packet carrying data from one of
              the elementary stream being muxed.
@param max_packet_data_size the maximum packet data size allowed
@param strm output mux stream
@param buffers ?
@param PTSstamp presentation time stamp of the packet
@param DTSstamp decoding time stamp of the packet
@param timestamps ?
@param returns the written bytes/packets (?)
***************************************************/

unsigned int 
Multiplexor::WritePacket( unsigned int     max_packet_data_size,
                           MuxStream        &strm,
                           bool 	 buffers,
                           clockticks   	 PTS,
                           clockticks   	 DTS,
                           uint8_t 	 timestamps
	)
{
    unsigned int written =
        psstrm->CreateSector ( pack_header_ptr,
                               sys_header_ptr,
                               max_packet_data_size,
                               strm,
                               buffers,
                               false,
                               PTS,
                               DTS,
                               timestamps );
    NextPosAndSCR();
    return written;
}

/***************************************************
 *
 * WriteRawSector - Write out a packet carrying data for
 *                    a control packet with irregular content.
@param rawsector data for the raw sector
@param length length of the raw sector
 ***************************************************/

void
Multiplexor::WriteRawSector(  uint8_t *rawsector,
                               unsigned int     length
	)
{
    //
    // Writing raw sectors when packs stretch over multiple sectors
    // is a recipe for disaster!
    //
    mjpeg_info( "RAW !!! \n");
    assert( packets_per_pack == 1 );
	psstrm->RawWrite( rawsector, length );
	NextPosAndSCR();

}



/******************************************************************
	OutputPadding

	generates Pack/Sys Header/Packet information for a 
	padding stream and saves the sector

	We have to pass in a special flag to cope with appalling mess VCD
	makes of audio packets (the last 20 bytes being dropped thing) 0 =
	Fill the packet completetely.  This include "audio packets" that
    include no actual audio, only a system header and padding.
@param vcd_audio_pad flag for VCD audio padding
******************************************************************/


void Multiplexor::OutputPadding (bool vcd_audio_pad)

{
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
	NextPosAndSCR();

}

 /******************************************************************
 *	OutputGOPControlSector
 *  DVD System headers are carried in peculiar sectors carrying 2
 *  PrivateStream2 packets.   We're sticking 0's in the packets
 *  as we have no idea what's supposed to be in there.
 *
 * Thanks to Brent Byeler who worked out this work-around.
 *
 ******************************************************************/

void Multiplexor::OutputDVDPriv2 (	)
{
    uint8_t *packet_size_field;
    uint8_t *index;
    uint8_t sector_buf[sector_size];
    unsigned int tozero;
    
    assert( sector_size == 2048 );
    PS_Stream::BufferSectorHeader( sector_buf,
                                pack_header_ptr,
                                &sys_header,
                                index );
    PS_Stream::BufferPacketHeader( index,
                                   PRIVATE_STR_2,
                                   2,      // MPEG 2
                                   false,  // No buffers
                                   0,
                                   0,
                                   0,      // No timestamps
                                   0,
                                   TIMESTAMPBITS_NO,
                                   packet_size_field,
                                   index );
    tozero = sector_buf+1024-index;
    memset( index, 0, tozero);
    index += tozero;
    PS_Stream::BufferPacketSize( packet_size_field, index );    

    PS_Stream::BufferPacketHeader( index,
                                   PRIVATE_STR_2,
                                   2,      // MPEG 2
                                   false,  // No buffers
                                   0,
                                   0,
                                   0,      // No timestamps
                                   0,
                                   TIMESTAMPBITS_NO,
                                   packet_size_field,
                                   index );
    tozero = sector_buf+2048-index;
    memset( index, 0, tozero );
    index += tozero;
    PS_Stream::BufferPacketSize( packet_size_field, index );

    WriteRawSector( sector_buf, sector_size );
}


/* 
 * Local variables:
 *  c-file-style: "stroustrup"
 *  tab-width: 4
 *  indent-tabs-mode: nil
 * End:
 */
