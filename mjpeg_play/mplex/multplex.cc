#include "main.hh"

#include <math.h>
#include <stdlib.h>
#include <unistd.h>

#include <format_codes.h>

unsigned int    which_streams;
int 			system_header_size;
int 			sector_size;
int 			zero_stuffing;	/* Pad short sectors with 0's not padding packets */

int 		dmux_rate;	/* Actual data mux-rate for calculations always a multiple of 50  */
int 		mux_rate;	/* TODO: remove MPEG mux rate (50 byte/sec units      */


				/* In some situations the system/PES packets are embedded with external
				   transport data which has to be taken into account for SCR calculations
				   to be correct.
				   E.g. VCD streams. Where each 2324 byte system packet is embedded in a
				   2352 byte CD sector and the actual MPEG data is preceded by 30 empty sectors.
				*/
int				sector_transport_size;
int             transport_prefix_sectors; 


/* 
		Stream syntax parameters.
*/
		
	

static unsigned int packets_per_pack;
static unsigned int audio_buffer_size = 0;
static unsigned int video_buffer_size = 0;

static int always_sys_header_in_pack;
static int dtspts_for_all_vau;
static int sys_header_in_pack1;
static int buffers_in_video;
static int always_buffers_in_video;	
static int buffers_in_audio;
static int always_buffers_in_audio;
static int trailing_pad_pack;		/* Stick a padding packet at the end			*/
static unsigned int audio_max_packet_data;
static unsigned int video_max_packet_data;
static unsigned int audio_min_packet_data;
static unsigned int video_min_packet_data;
static int audio_packet_data_limit; /* Needed for VCD which wastes 20 bytes */


	/* Stream packet component buffers */

Sys_header_struc 	sys_header;
Sector_struc 		cur_sector;


	/* Stream parameters */
static  unsigned long long bytes_output;

static  FILE *ostream;				/* Outputstream MPEG	*/


typedef enum { start_segment, mid_segment, 
	       last_vau_segment, last_aaus_segment }
segment_state;

static void outputstreamsuffix(clockticks *SCR, FILE *ostream, 
			unsigned long long  *bytes_output);
static void next_video_access_unit (Buffer_struc *buffer,
									VAunit *video_au, unsigned int bytes_muxed, 
									unsigned int *AU_starting_next_sec, 
									clockticks SCR_delay,
									VideoStream &vstrm);
static void next_audio_access_unit (Buffer_struc *buffer,
									AAunit *audio_au, 
									unsigned int bytes_muxed,
									unsigned char *audio_frame_start, 
									clockticks SCR_delay,
									AUStream<AAunit> &aaunit_info_vec);
void outputstreamprefix( clockticks *current_SCR, VideoStream &vstrm);


/******************************************************************

	Initialisation of stream syntax paramters based on selected
	user options.
******************************************************************/

void init_stream_syntax_parameters(	VideoStream 	&vstrm,
									Audio_struc 	*audio_info	)
{

	unsigned int video_rate=0;
	unsigned int audio_rate=0;
	Pack_struc 			dummy_pack;
	Sys_header_struc 	dummy_sys_header;	
	

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
	  	trailing_pad_pack = 1;
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
		
	case  MPEG_FORMAT_MPEG2 : 
		mjpeg_info( "Selecting generic MPEG2 output profile\n");
		opt_mpeg = 2;
	 	packets_per_pack = 1;
	  	sys_header_in_pack1 = 1;
	  	always_sys_header_in_pack = 0;
	  	trailing_pad_pack = 0;
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
	  	trailing_pad_pack = 1;
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

		break;

	case MPEG_FORMAT_VCD_STILL :
		opt_data_rate = 75*2352;  			 /* 75 raw CD sectors/sec */ 
	  	video_buffer_size = 46*1024;
	  	opt_VBR = 0;
		opt_mpeg = 1;
	 	packets_per_pack = 1;
	  	sys_header_in_pack1 = 0;
	  	always_sys_header_in_pack = 0;
	  	trailing_pad_pack = 1;
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
		trailing_pad_pack = 0;
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
		break;
	}
	

  audio_buffer_size = 4 * 1024;
  mjpeg_info("SYSTEMS/PROGRAM stream:\n");
    
 

	
  /* These are used to make (conservative) decisions
	 about whether a packet should fit into the recieve buffers... 
	 Audio packets always have PTS fields, video packets needn't.	
  */ 

  audio_max_packet_data = packet_payload( NULL, NULL, false, true, false );
  video_max_packet_data = packet_payload( NULL, NULL, false, false, false );
  create_pack (&dummy_pack, 0, mux_rate);
  if( always_sys_header_in_pack )
  {
	  create_sys_header (&dummy_sys_header, mux_rate, 1, !opt_VBR, 1, 1, 1, 1,
						 AUDIO_STR_0, 0, audio_buffer_size/128,
						 vstrm.stream_id, 1, video_buffer_size/1024, 
						 which_streams  & STREAMS_AUDIO );

  	video_min_packet_data = 
		packet_payload( &dummy_sys_header, &dummy_pack, 
						always_buffers_in_video, true, true );
	audio_min_packet_data = 
		packet_payload( &dummy_sys_header, &dummy_pack, 
						true, true, false );

  }
  else
  {
    video_min_packet_data = 
		packet_payload( NULL, &dummy_pack, 
						always_buffers_in_video, true, true );
	audio_min_packet_data = 
		packet_payload( NULL, &dummy_pack, 
						true, true, false );

  }
      
     
  /* Set the mux rate depending on flags and the paramters of the specified streams */
     
  	if (which_streams & STREAMS_VIDEO) 
    {
		video_rate = vstrm.bit_rate * 50;
    }

	if (which_streams & STREAMS_AUDIO)
		audio_rate = bitrate_index[audio_info->version_id][3-audio_info->layer][audio_info->bit_rate]*128;

    
	/* Attempt to guess a sensible mux rate for the given video and audio streams 	*/
	/* TODO: This is a pretty inexact guess and may need tweaking for different stream formats	 */
	 
	dmux_rate = static_cast<int>(
		1.01 * (video_rate + audio_rate) * 
		( (1.0  *   sector_size)/video_min_packet_data +
		  (packets_per_pack-1) * sector_size/video_max_packet_data
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
	
 
}


/******************************************************************
    Program start-up packets.  Generate any irregular packets						needed at the start of the stream...
	Note: *must* leave a sensible in-stream system header in
	sys_header.
	TODO: get rid of this grotty sys_header global.
    TODO: VIDEO_STR_0 should depend on video stream!
******************************************************************/

void outputstreamprefix( clockticks *current_SCR, VideoStream &vstrm)
{
	int vcd_2nd_syshdr_data_limit;
	Pack_struc 			dummy_pack;

	/* Deal with transport padding */
	bytes_output += transport_prefix_sectors*sector_transport_size;
	bytepos_timecode ( bytes_output, current_SCR);
	
	/* VCD: Two padding packets with video and audio system headers */

	switch (opt_mux_format)
		{
		case MPEG_FORMAT_VCD :
		case MPEG_FORMAT_VCD_NSR :
		/* First packet carries video-info-only sys_header */
		create_sys_header (&sys_header, mux_rate,0, 0, 1, 1, 1, 1,
					   AUDIO_STR_0, 0, audio_buffer_size/128,
					   vstrm.stream_id, 1, video_buffer_size/1024, 
					   which_streams  & STREAMS_VIDEO);
	  	output_padding( *current_SCR, ostream,
					  	true,
						true,
						false,
						0);					 
		bytes_output += sector_transport_size;			 
		bytepos_timecode ( bytes_output, current_SCR);
		
				/* Second packet carries audio-info-only sys_header */
		create_sys_header (&sys_header, mux_rate, 1, 0, 1, 1, 1, 0,
					   AUDIO_STR_0, 0, audio_buffer_size/128,
					   vstrm.stream_id, 1, video_buffer_size/1024, 
					   which_streams  & STREAMS_AUDIO );
		create_pack (&dummy_pack, 0, mux_rate);

		vcd_2nd_syshdr_data_limit = 
			packet_payload( &sys_header, &dummy_pack,false, 0, 0)-20;
										  
	  	output_padding( *current_SCR, ostream,
					  	true,
						true,
						false,
						vcd_2nd_syshdr_data_limit
			          );
		bytes_output += sector_transport_size;
		bytepos_timecode ( bytes_output, current_SCR);
		/* Calculate the payload of an audio packet... currently 2279 */
		audio_packet_data_limit = 
			packet_payload( NULL, &dummy_pack,true,TIMESTAMPBITS_PTS,0 )-20;
		
        /* Ugh... what *were* they thinking of? */

		break;
		
		case MPEG_FORMAT_SVCD :
		case MPEG_FORMAT_SVCD_NSR :
		/* First packet carries sys_header */
		create_sys_header (&sys_header, mux_rate,1, !opt_VBR, 1, 1, 1, 1,
					   AUDIO_STR_0, 0, audio_buffer_size/128,
					   vstrm.stream_id, 1, video_buffer_size/1024, 
					   which_streams  );
	  	output_padding( *current_SCR, ostream,
					  	true,
						true,
						false,
						0);					 
		bytes_output += sector_transport_size;			 
		bytepos_timecode ( bytes_output, current_SCR);
		break;

		case MPEG_FORMAT_VCD_STILL :

			/* First packet carries small-still sys_header */
			create_sys_header (&sys_header, mux_rate,0, 0, 1, 1, 1, 1,
							   AUDIO_STR_0, 0, audio_buffer_size/128,
							   vstrm.stream_id, 1, 46, 
							   which_streams  & STREAMS_VIDEO);
			output_padding( *current_SCR, ostream,
							true,
							true,
							false,
							0);					
			bytes_output += sector_transport_size;			 
			bytepos_timecode ( bytes_output, current_SCR);
			/* Second packet carries large-still sys_header */
			create_sys_header (&sys_header, mux_rate,0, 0, 1, 1, 1, 1,
							   AUDIO_STR_0, 0, audio_buffer_size/128,
							   vstrm.stream_id+1, 1, 117, 
							   which_streams  & STREAMS_VIDEO);
			output_padding( *current_SCR, ostream,
							true,
							true,
							false,
							0);					 
			bytes_output += sector_transport_size;			 
			bytepos_timecode ( bytes_output, current_SCR);
			break;
			
		case MPEG_FORMAT_SVCD_STILL :
			mjpeg_error_exit1("SVCD STILLS NOT YET IMPLEMENTED!\n");
			break;
	}

   /* Create the in-stream header if needed */
	create_sys_header (&sys_header, mux_rate,1, !opt_VBR, 1, 1, 1, 1,
					   AUDIO_STR_0, 0, audio_buffer_size/128,
					   vstrm.stream_id, 1, video_buffer_size/1024, 
					   which_streams);


}


/******************************************************************* 
	Find the timecode corresponding to given position in the system stream
   (assuming the SCR starts at 0 at the beginning of the stream 
   
****************************************************************** */

void bytepos_timecode(long long bytepos, clockticks *ts)
{
	 *ts = (bytepos*CLOCKS)/((long long)dmux_rate);
}

/******************************************************************
    Program shutdown packets.  Generate any irregular packets
    needed at the end of the stream...
   
******************************************************************/

void outputstreamsuffix(clockticks *SCR,
						FILE *ostream,
						unsigned long long  *bytes_output)
{
  unsigned char *index;
  Pack_struc 	pack;

  if (trailing_pad_pack )
	{
		int end_code_padding_payload;

		create_pack (&pack, *SCR, mux_rate);
		end_code_padding_payload = packet_payload( NULL, &pack, false, 0, 0)-4;
		create_sector (&cur_sector, &pack, NULL,
					   end_code_padding_payload,
					   NULL, PADDING_STR, 0, 0,
					   false, 0, 0,
					   TIMESTAMPBITS_NO );
		index = cur_sector.buf  + sector_size-4;
	}
  else
	  index = cur_sector.buf;
  /* write out ISO 11172 END CODE				*/


  *(index++) = (unsigned char)((ISO11172_END)>>24);
  *(index++) = (unsigned char)((ISO11172_END & 0x00ff0000)>>16);
  *(index++) = (unsigned char)((ISO11172_END & 0x0000ff00)>>8);
  *(index++) = (unsigned char)(ISO11172_END & 0x000000ff);

  fwrite (cur_sector.buf, sizeof (unsigned char), sector_size, ostream);
  *bytes_output += sector_transport_size;
  bytepos_timecode ( *bytes_output, SCR);
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


int frame_cnt=0;
	
void outputstream ( VideoStream &vstrm,
					char 		*audio_file,
					char 		*multi_file,
					AUStream<AAunit>   &aaunit_info_vec
					)

{
	segment_state seg_state;
	VAunit video_au;		/* Video Access Unit	*/
	AAunit audio_au;		/* Audio Access Unit	*/
	VAunit *next_vau;
	static clockticks delay,audio_delay,video_delay;

	static unsigned int sectors_delay;
	unsigned int AU_starting_next_sec; 
	unsigned char audio_frame_start;
	unsigned int audio_bytes;
	unsigned int video_bytes;

	unsigned int nsec_a=0;
	unsigned int nsec_v=0;
	unsigned int nsec_p=0;

	clockticks SCR_audio_delay = 0;
	clockticks SCR_video_delay = 0;

	clockticks current_SCR;
	clockticks audio_next_SCR;
	clockticks video_next_SCR;

	static Buffer_struc video_buffer;
	static Buffer_struc audio_buffer;

	int video_ended = 0;
	int audio_ended = 0;

	unsigned int packets_left_in_pack = 0; /* Suppress warning */
	unsigned char padding_packet;
	unsigned char start_of_new_pack;
	unsigned char include_sys_header = 0; /* Suppress warning */

	static  FILE *istream_a =NULL;			/* Inputstream Audio	*/

	/* Oeffne alle Ein- und Ausgabefiles			*/
	/* Open in- and outputstream				*/
		
	if (which_streams & STREAMS_AUDIO) istream_a = fopen (audio_file, "rb");
	
	ostream	= system_open_init(multi_file);
		
	/* Einlesen erster Access Unit Informationen		*/
	audio_frame_start = false;
	AU_starting_next_sec = OLDFRAME;
	
	if (which_streams & STREAMS_AUDIO) {
			audio_au = *aaunit_info_vec.next();
			audio_frame_start = true;
	}
	if (which_streams & STREAMS_VIDEO) {
		video_au = *vstrm.next();;
		AU_starting_next_sec = video_au.type;
	}


	/* Bufferstrukturen Initialisieren				*/
	/* initialize buffer structure				*/

	init_buffer_struc (&video_buffer,video_buffer_size);
	init_buffer_struc (&audio_buffer,audio_buffer_size);
	

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
			 (video_au.length+video_min_packet_data-1)/video_min_packet_data  +
			 (audio_au.length+audio_min_packet_data-1)/video_min_packet_data) *
			(clockticks)sector_transport_size*(clockticks)CLOCKS/dmux_rate;

	/* Debugging aid - allows easy comparison with streams generated by vcdmplex */
	if( opt_mux_format == 1 && opt_emul_vcdmplex)
		delay = (36000-2400) * 300;
	video_delay = (clockticks)opt_video_offset*(clockticks)CLOCKS/1000;
	audio_delay = (clockticks)opt_audio_offset*(clockticks)CLOCKS/1000;
	audio_delay += delay;
	video_delay += delay;
	


  /*  Let's try to read in unit after unit and to write it out into
	  the outputstream. The only difficulty herein lies into the 
	  buffer management, and into the fact the the actual access
	  unit *has* to arrive in time, that means the whole unit
	  (better yet, packet data), has to arrive before arrival of
	  DTS. If both buffers are full we'll generate a padding packet
	  
	  Of course, when we start we're starting a new segment with no bytes output...
	    */

	
	seg_state = start_segment;

	while ((video_au.length + audio_au.length) > 0)
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
			if( audio_au.length>0 && audio_au.PTS >= video_au.PTS )
				{
					/* Current segment has been written out... 
					*/
					outputstreamsuffix( &current_SCR, ostream, &bytes_output);
					ostream = system_next_file( ostream, multi_file );
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
				status_info (nsec_a, nsec_v, nsec_p, bytes_output,
							 buffer_space(&video_buffer),
							 buffer_space(&audio_buffer),
							 LOG_INFO);

				bytes_output = 0;
				bytepos_timecode ( bytes_output, &current_SCR);	
				
				buffer_flush (&video_buffer);
				buffer_flush (&audio_buffer);
				outputstreamprefix( &current_SCR, vstrm );

				/* The starting PTS/DTS of AU's may of course be
				   non-zero since this might not be the first segment
				   we've built. Hence we adjust the "delay" to
				   compensate for this as well as for any
				   synchronisation / start-up delay needed.  
				*/
				
				SCR_audio_delay = audio_delay + current_SCR-video_au.DTS;
				SCR_video_delay = video_delay + current_SCR-audio_au.PTS;
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
				if( system_file_lim_reached( ostream ) )
				{
					if( opt_multifile_segment )
						ostream = system_next_file( ostream, multi_file );
					else
					{
						next_vau = vstrm.lookahead( 1);
						if( next_vau->type != IFRAME)
							seg_state = last_vau_segment;
					}
				}
				else if( video_au.end_seq )
				{
					next_vau = vstrm.lookahead( 1);
					if( next_vau  )
					{
						if( ! next_vau->seq_header || next_vau->type != IFRAME)
						{
							mjpeg_error_exit1( "Sequence split detected but no following sequence found...\n");
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
				if( video_au.type == IFRAME )
					seg_state = last_aaus_segment;
				break;
		}

		padding_packet = false;
		start_of_new_pack = (packets_left_in_pack == packets_per_pack); 

		/* Calculate amount of data to be moved for the next AU's.
		   Slightly pessimistic - assumes worst-case packet data capacity
		   and the need to start a new packet.
		*/
		audio_bytes = (audio_au.length/audio_min_packet_data)*sector_transport_size +
			(audio_au.length%audio_min_packet_data)+(sector_transport_size-audio_min_packet_data);
		video_bytes = (video_au.length/video_min_packet_data)*sector_transport_size +
			(video_au.length%video_min_packet_data)+(sector_transport_size-video_min_packet_data);


	
		/* Calculate when the the next AU's will finish arriving under the assumption
		   that the next sector carries a *different* payload.  Using
		   this time we can see if a stream will definately under-run
		   if no sector is sent immediately. 		   
		*/
		
		
		bytepos_timecode ( bytes_output, &current_SCR);
		bytepos_timecode (bytes_output+(sector_transport_size+audio_bytes), &audio_next_SCR);
		bytepos_timecode (bytes_output+(sector_transport_size+video_bytes), &video_next_SCR);

		if (which_streams & STREAMS_AUDIO) 
			buffer_clean (&audio_buffer, current_SCR);
		if (which_streams & STREAMS_VIDEO) 
			buffer_clean (&video_buffer, current_SCR);


		/* CASE: Audio Buffer OK, Audio Data ready
		   SEND An audio packet
		*/

		/* Heuristic... if we can we prefer to send audio rather than video. 
		   Even a few uSec under-run are audible and in any case the data-rate
		   is trivial compared wth video. The only exception is if not
		   sending video would cause it to under-run but there's no danger of
		   and audio under-run
		   	   
		*/

		if ( (buffer_space (&audio_buffer)-AUDIO_BUFFER_FILL_MARGIN
			  > audio_max_packet_data)
			 && (audio_au.length>0)
			 && nsec_v != 0
			 &&  ! (  video_au.length !=0 &&
					  seg_state != last_aaus_segment &&
					  video_next_SCR >= video_au.DTS+SCR_video_delay &&
					  audio_next_SCR < audio_au.PTS+SCR_audio_delay
				 )
			)
		{
			/* Calculate actual time current AU is likely to arrive. */
			bytepos_timecode (bytes_output+audio_bytes, &audio_next_SCR);
			if( audio_next_SCR >= audio_au.PTS+SCR_audio_delay )
				timeout_error (STATUS_AUDIO_TIME_OUT,audio_au.dorder);
			output_audio (current_SCR, SCR_audio_delay, 
						  istream_a, ostream, 
						  &audio_buffer, &audio_au, aaunit_info_vec,
						  &audio_frame_start,
						  start_of_new_pack,
						  include_sys_header,
						  seg_state == last_aaus_segment);
			++nsec_a;
		}

		/* CASE: Video Buffer OK, Video Data ready  (implicitly -  no audio packet to send 
		   SEND a video packet.
		*/

		else if( buffer_space (&video_buffer) >= video_max_packet_data
				 && video_au.length>0 && seg_state != last_aaus_segment
			)
		{

			/* Calculate actual time current AU is likely to arrive. */
			bytepos_timecode (bytes_output+video_bytes, &video_next_SCR);
			if( video_next_SCR >= video_au.DTS+SCR_video_delay )
				timeout_error (STATUS_VIDEO_TIME_OUT,video_au.dorder);
			output_video ( current_SCR, SCR_video_delay, 
						  vstrm, ostream, 
						  &video_buffer, &video_au, 
						  &AU_starting_next_sec,
						  start_of_new_pack,
						  include_sys_header);

			++nsec_v;

		}

		/* CASE: Audio Buffer and Video Buffers NOT OK (too full to send)
		   SEND padding packet */
		else
		{

			output_padding (current_SCR, ostream, 
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
		bytes_output += sector_transport_size;


		status_info (nsec_a, nsec_v, nsec_p, bytes_output,
					 buffer_space(&video_buffer),
					 buffer_space(&audio_buffer),
					 LOG_DEBUG);

		/* Unless sys headers are always required we turn them off after the first
		   packet has been generated */
		include_sys_header = always_sys_header_in_pack;

		if( !video_ended && video_au.length == 0 )
		{
			mjpeg_info( "Video stream ended.\n" );
			status_info (nsec_a, nsec_v, nsec_p, bytes_output,
						 buffer_space(&video_buffer),
						 buffer_space(&audio_buffer),
						 LOG_INFO);
			video_ended = 1;
		}

		if( !audio_ended && audio_au.length == 0 )
		{
			mjpeg_info( "Audio stream ended.\n" );
			status_info (nsec_a, nsec_v, nsec_p, bytes_output,
						 buffer_space(&video_buffer),
						 buffer_space(&audio_buffer),
						 LOG_INFO);
			audio_ended = 1;
		}
	}

	// Tidy up
	
	outputstreamsuffix( &current_SCR, ostream, &bytes_output);
	fclose (ostream);
	if (which_streams & STREAMS_AUDIO) fclose (istream_a);
    
}


/******************************************************************
	Next_Video_Access_Unit
	holt aus dem TMP File, der die Info's ueber die Access
	Units enthaelt, die jetzt gueltige Info her. Nach
	dem Erstellen des letzten Packs sind naemlich eine
	bestimmte Anzahl Bytes und damit AU's eingelesen worden.

	gets information for the next access unit from the tmp
	file
******************************************************************/

void next_video_access_unit (Buffer_struc *buffer,
							 VAunit *video_au,
							 unsigned int bytes_muxed,
							 unsigned int *AU_starting_next_sec,
							 clockticks SCR_delay,
							 VideoStream  &vstrm
							 )
{
  clockticks   decode_time;
  VAunit *vau;
  
  if (bytes_muxed == 0)
	return;


  /* What's left of the current AU and any prefix remaining of the next AU.
	NOTE: It *is* possible for this loop to iterate. MPEG specifically says the
	DTS/PTS field for a packet carrying multiple AU's refers to the first to start
	in the packet.  Whether Joe-Blow's hardware VCD player handles this properly is
	another matter of course!
	*/
  decode_time = video_au->DTS + SCR_delay;
  while (video_au->length < bytes_muxed)
	{	  

	  queue_buffer (buffer, video_au->length, decode_time);
	  bytes_muxed -= video_au->length;
	  vau = vstrm.next();
	  if( vau != NULL )
		*video_au = *vau;
	  else
	    {
		  video_au->markempty();
		  return;
	    }
	  *AU_starting_next_sec = video_au->type;
	  
	  decode_time = video_au->DTS + SCR_delay;
	};

	
  if (video_au->length > bytes_muxed)
	{

	  queue_buffer (buffer, bytes_muxed, decode_time);
	  video_au->length -= bytes_muxed;
	  *AU_starting_next_sec = OLDFRAME;
	} 
  else
	  if (video_au->length == bytes_muxed)
		{
		  queue_buffer (buffer, bytes_muxed, decode_time);

		  vau = vstrm.next();
		  if( vau != NULL )
			*video_au = *vau;
		  else
			{
			  video_au->markempty();
			  return;
			}
		  *AU_starting_next_sec = video_au->type;
		};	   

}


/******************************************************************
	Output_Video
	generiert Pack/Sys_Header/Packet Informationen aus dem
	Video Stream und speichert den so erhaltenen Sektor ab.

	generates Pack/Sys_Header/Packet information from the
	video stream and writes out the new sector
******************************************************************/

void output_video ( clockticks SCR,
					clockticks SCR_delay,
					VideoStream &vstrm,
					FILE *ostream,
					Buffer_struc *buffer,
					VAunit *video_au,
					unsigned int *AU_starting_next_sec,
					unsigned char start_of_new_pack,
					unsigned char include_sys_header
					)

{

  unsigned int max_packet_payload; 	 
  unsigned int prev_au_tail;
  Pack_struc *pack_ptr = NULL;
  Sys_header_struc *sys_header_ptr = NULL;
  unsigned char timestamps;
  VAunit *vau;
  Pack_struc pack;
  unsigned int old_au_then_new_payload;
  clockticks  DTS,PTS;
  
  max_packet_payload = 0;	/* 0 = Fill sector */
  	/* 	We're now in the last AU of a segment. 
		So we don't want to go beyond it's end when filling
		sectors. Hence we limit packet payload size to (remaining) AU length.
	*/
  if( video_au->end_seq )
  {
	max_packet_payload = video_au->length;
  }

  if (start_of_new_pack)
    {
	  /* Wir generieren den Pack Header				*/
	  /* let's generate pack header					*/
	  create_pack (&pack, SCR, mux_rate);
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

  old_au_then_new_payload = packet_payload( sys_header_ptr, pack_ptr, 
  									        buffers_in_video, true, true);

  PTS = video_au->PTS + SCR_delay;
  DTS = video_au->DTS + SCR_delay;

   /* CASE: Packet starts with new access unit			*/
  if (*AU_starting_next_sec != OLDFRAME)
	{
	  if(  dtspts_for_all_vau && max_packet_payload == 0 )
	  	max_packet_payload = video_au->length;

  if( video_au->dorder == 1499 )
		
	  if (video_au->type == BFRAME)
		timestamps=TIMESTAMPBITS_PTS;
	  else
		timestamps=TIMESTAMPBITS_PTS_DTS;

	  create_sector ( &cur_sector, pack_ptr, sys_header_ptr,
					  max_packet_payload,
					  vstrm.rawstrm, vstrm.stream_id,
					  1, video_buffer_size/1024,
					  buffers_in_video, PTS, DTS,
					  timestamps );
	  next_video_access_unit (buffer, video_au, cur_sector.length_of_packet_data, 
							  AU_starting_next_sec, SCR_delay, vstrm);

	}

  /* FALL: Packet beginnt mit alter Access Unit, es kommt	*/
  /*       keine neue im selben Packet vor					*/
  /* CASE: Packet begins with old access unit, no new one	*/
  /*	     begins in the very same packet					*/
  else if ((*AU_starting_next_sec == OLDFRAME) &&
		   (video_au->length >= old_au_then_new_payload))
	{
	  create_sector( &cur_sector, pack_ptr, sys_header_ptr,
					 video_au->length,
					 vstrm.rawstrm, vstrm.stream_id, 1, video_buffer_size/1024,
					 buffers_in_video, 0, 0,
					 TIMESTAMPBITS_NO );
	  next_video_access_unit (buffer, video_au, cur_sector.length_of_packet_data, 
							  AU_starting_next_sec, SCR_delay, vstrm);

	}

  /* FALL: Packet beginnt mit alter Access Unit, es kommt	*/
  /*       eine neue im selben Packet vor			*/
  /* CASE: Packet begins with old access unit, a new one	*/
  /*	     begins in the very same packet			*/
  else if ((*AU_starting_next_sec == OLDFRAME) && 
		   (video_au->length < old_au_then_new_payload))
	{
	  prev_au_tail = video_au->length;
	  queue_buffer (buffer, video_au->length, DTS);

	  /* is there a new access unit anyway? */

 
	  vau = vstrm.next();
	  if( vau != NULL )
		{
		  *video_au = *vau;

		  if(  dtspts_for_all_vau  && max_packet_payload == 0 )
	  		max_packet_payload = video_au->length+prev_au_tail;

		  if (video_au->type == BFRAME)
			timestamps=TIMESTAMPBITS_PTS;
		  else
			timestamps=TIMESTAMPBITS_PTS_DTS;
	  	 *AU_starting_next_sec = video_au->type;
 		 PTS = video_au->PTS + SCR_delay;
  		 DTS = video_au->DTS + SCR_delay;
	
		  create_sector (&cur_sector, pack_ptr, sys_header_ptr,
						 max_packet_payload,
						 vstrm.rawstrm, vstrm.stream_id, 1, video_buffer_size/1024,
						 buffers_in_video, PTS, DTS,
						 timestamps );
		  next_video_access_unit (buffer, video_au, 
		  						  cur_sector.length_of_packet_data - prev_au_tail, 
								  AU_starting_next_sec, SCR_delay, vstrm);
		} 
	  else
		{
		  video_au->markempty();
		  create_sector ( &cur_sector, pack_ptr, sys_header_ptr,
						  0,
						  vstrm.rawstrm, vstrm.stream_id, 1, video_buffer_size/1024,
						  buffers_in_video, 0, 0,
						  TIMESTAMPBITS_NO);
		};


	}


  /* Sector auf Platte schreiben				*/
  fwrite (cur_sector.buf, sector_size, 1, ostream);
  
  buffers_in_video = always_buffers_in_video;
	
}


/******************************************************************
	Next_Audio_Access_Unit
	holt aus dem TMP File, der die Info's ueber die Access
	Units enthaelt, die jetzt gueltige Info her. Nach
	dem Erstellen des letzten Packs sind naemlich eine
	bestimmte Anzahl Bytes und damit AU's eingelesen worden.

	gets information on access unit from the tmp file
******************************************************************/

void next_audio_access_unit (Buffer_struc *buffer,
							 AAunit *audio_au,
							 unsigned int bytes_muxed,
							 unsigned char *audio_frame_start,
							 clockticks SCR_delay,
							 AUStream<AAunit> &aaunit_info_vec							 )

{
  AAunit *aau;
  clockticks   decode_time;
  
  if (bytes_muxed == 0)
	return;

  decode_time = audio_au->PTS + SCR_delay;
  while (audio_au->length < bytes_muxed)
	{
	  queue_buffer (buffer, audio_au->length, decode_time);
	  bytes_muxed -= audio_au->length;
	  aau = aaunit_info_vec.next();
	  if( aau != NULL )
		*audio_au = *aau;
	  else
	    {
		  audio_au->markempty();
		  return;
	    }
	  *audio_frame_start = true;
	  decode_time = audio_au->PTS + SCR_delay;
	};

  if (audio_au->length > bytes_muxed)
	{
	  queue_buffer (buffer, bytes_muxed, decode_time);
	  audio_au->length -= bytes_muxed;
	  *audio_frame_start = false;
	} else
	  if (audio_au->length == bytes_muxed)
		{
		  queue_buffer (buffer, bytes_muxed, decode_time);
		  aau = aaunit_info_vec.next();
		  if( aau != NULL )
			*audio_au = *aau;
		  else
			{
			  audio_au->markempty();
			  return;
			}
		  *audio_frame_start = true;
		};

}

/******************************************************************
	Output_Audio
	erstellt Pack/Sys_Header/Packet Informationen aus dem
	Audio Stream und speichert den so erhaltenen Sector ab.

	generates Pack/Sys Header/Packet information from the
	audio stream and saves them into the sector
******************************************************************/

void output_audio ( clockticks SCR,
					clockticks SCR_delay,
					FILE *istream_a,
					FILE *ostream,
					Buffer_struc *buffer,
					AAunit *audio_au,
					AUStream<AAunit> &aaunit_info_vec,
					unsigned char *audio_frame_start,
					unsigned char start_of_new_pack,
					unsigned char include_sys_header,
					unsigned char last_au_segment
					)

{
  clockticks   PTS;
  unsigned int max_packet_data; 	 
  unsigned int bytes_sent;
  Pack_struc *pack_ptr = NULL;
  Sys_header_struc *sys_header_ptr = NULL;
  AAunit *aau;
  Pack_struc pack;
  unsigned int old_au_then_new_payload;
  
  PTS = audio_au->PTS + SCR_delay;
  old_au_then_new_payload = packet_payload( sys_header_ptr, pack_ptr,
											buffers_in_audio, false, false );

  max_packet_data= audio_packet_data_limit;
  if( last_au_segment )
  {
  	/* We're now in the last AU of a segment.  So we don't want to go beyond it's end when willing
	sectors. Hence we limit packet payload size to (remaining) AU length.
	*/
	max_packet_data = audio_au->length;
  }
  
  if( audio_packet_data_limit && old_au_then_new_payload > max_packet_data )
  	old_au_then_new_payload = max_packet_data;
 
  if (start_of_new_pack)
    {
	  /* Wir generieren den Pack Header				*/
	  /* let's generate pack header					*/
	  create_pack (&pack, SCR, mux_rate);
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
	
  if (*audio_frame_start)
    {
	  create_sector (&cur_sector, pack_ptr, sys_header_ptr,
					 audio_packet_data_limit,
					 istream_a, AUDIO_STR_0, 0, audio_buffer_size/128,
					 buffers_in_audio, PTS, 0,
					 TIMESTAMPBITS_PTS);

	  next_audio_access_unit (buffer, audio_au, cur_sector.length_of_packet_data, 
							  audio_frame_start, SCR_delay, aaunit_info_vec);
    }

  /* FALL: Packet beginnt mit alter Access Unit, es kommt	*/
  /*       keine neue im selben Packet vor			*/
  /* CASE: packet starts with old access unit, no new one	*/
  /*       starts in this very same packet			*/
  else if (!(*audio_frame_start) && 
		   (audio_au->length >= old_au_then_new_payload))
    {
	  create_sector (&cur_sector, pack_ptr, sys_header_ptr,
					 audio_packet_data_limit,
					 istream_a, AUDIO_STR_0, 0, audio_buffer_size/128,
					 buffers_in_audio, 0, 0,
					 TIMESTAMPBITS_NO );

	  next_audio_access_unit (buffer, audio_au, cur_sector.length_of_packet_data,
							  audio_frame_start, SCR_delay, aaunit_info_vec);
    }

  /* FALL: Packet beginnt mit alter Access Unit, es kommt	*/
  /*       eine neue im selben Packet vor			*/
  /* CASE: packet starts with old access unit, a new one	*/
  /*       starts in this very same packet			*/
  else /* !(*audio_frame_start) &&  (audio_au->length < old_au_then_new_payload)) */
    {
	  bytes_sent = audio_au->length;
	  queue_buffer (buffer, audio_au->length, PTS);

	  /* gibt es ueberhaupt noch eine Access Unit ? */
	  /* is there another access unit anyway ? */
	  aau = aaunit_info_vec.next();
	  if( aau != NULL )
		{
		  *audio_au = *aau;
		  *audio_frame_start = true;
		  PTS = audio_au->PTS + SCR_delay;
		  create_sector (&cur_sector, pack_ptr, sys_header_ptr,
						 audio_packet_data_limit,
						 istream_a, AUDIO_STR_0, 0, audio_buffer_size/128,
						 buffers_in_audio, PTS, 0,
						 TIMESTAMPBITS_PTS );

		  next_audio_access_unit (buffer, audio_au,
		  						  cur_sector.length_of_packet_data - bytes_sent, 
								  audio_frame_start, SCR_delay, aaunit_info_vec );
		} else
		  {
			audio_au->markempty();
			create_sector (&cur_sector, pack_ptr, sys_header_ptr,
						   0,
						   istream_a, AUDIO_STR_0, 0, audio_buffer_size/128,
						   buffers_in_audio, 0, 0,
						   TIMESTAMPBITS_NO );
		  };

    }

  /* Sector auf Platte schreiben				*/
  /* write out sector onto disk				*/
  fwrite (cur_sector.buf, sector_size, 1, ostream);

  buffers_in_audio = always_buffers_in_audio;
	
}

/******************************************************************
	Output_Padding
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

void output_padding (
					 clockticks SCR,
					 FILE *ostream,
					 unsigned char start_of_new_pack,
					 unsigned char include_sys_header,
					 unsigned char VBR_pseudo,
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
		  create_pack (&pack, SCR, mux_rate);
	      pack_ptr = &pack;
	      if( include_sys_header )
	  	    sys_header_ptr = &sys_header;
		}


	  /* Wir generieren das Packet				*/
	  /* let's generate the packet				*/
	  create_sector (&cur_sector, pack_ptr, sys_header_ptr,
					 padding_limit,
					 NULL, PADDING_STR, 0, 0,
					 false, 0, 0,
					 TIMESTAMPBITS_NO );

	  fwrite (cur_sector.buf, sector_size, 1, ostream);
	}


	
}

