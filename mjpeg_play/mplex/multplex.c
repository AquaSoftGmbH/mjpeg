#include "main.h"

#include <math.h>
#include <unistd.h>


#ifdef TIMER
extern long total_sec;
extern long total_usec;
extern long global_sec;
extern long global_usec;
extern struct timeval  tp_start;
extern struct timeval  tp_end;
extern struct timeval  tp_global_start;
extern struct timeval  tp_global_end;
#endif

unsigned int    which_streams;
int pack_header_size;


/* 
		Stream syntax parameters.
	*/
		
	
static unsigned int min_packet_data;
static unsigned int max_packet_data;
static unsigned int packets_per_pack;
static unsigned int audio_buffer_size;
static unsigned int video_buffer_size;
static int last_scr_byte_in_pack;
static int rate_restriction_flag;
static int always_sys_header_in_pack;
static int trailing_pad_pack;

static int audio_max_packet_data;
static int video_max_packet_data;

int sector_size;



	/* Stream packet component buffers */

Sys_header_struc 	sys_header;
Sector_struc 		cur_sector;


	/* Stream parameters */
static  unsigned long long bytes_output;
static  FILE *istream_v =NULL;			/* Inputstream Video	*/
static  FILE *istream_a =NULL;			/* Inputstream Audio	*/
static  FILE *ostream;					/* Outputstream MPEG	*/



/******************************************************************

	Initialisation of stream syntax paramters based on selected
	user options.
******************************************************************/

void init_stream_syntax_parameters(	)
{

  if ( opt_mux_format == MPEG_VCD )
    {

	  packets_per_pack = 1;
	  always_sys_header_in_pack = 0;
	  trailing_pad_pack = 1;
	  opt_data_rate = 174300;  
	  opt_sector_size = 2324;
	  opt_mpeg = 1;
	  opt_VBR = 0;
	  opt_buffer_size = 46;
	}
  else /* MPEG_MPEG1 - auto format MPEG1 */
	{
	  packets_per_pack = 20;
	  always_sys_header_in_pack = 1;
	  trailing_pad_pack = 0;
	}
	
  sector_size = opt_sector_size;
  video_buffer_size = opt_buffer_size * 1024;
  audio_buffer_size = 4 * 1024;
  printf("\n+------------------ MPEG/SYSTEMS INFORMATION -----------------+\n");
    
  /* VCD: sector_size=VIDEOCD_SECTOR_SIZE, 
	 fixed bitrate=1150000.0, packets_per_pack=1,
	 sys_headers only in first sector(s?)... all timestamps...
	 program end code needs to be appended.
  */
  if( opt_interactive_mode ) 
	{
	  do 
		{
		  printf ("\nsector size (CD-ROM 2324 bytes)          : ");
		  scanf ("%d", &sector_size);
		} while (sector_size>MAX_SECTOR_SIZE);

	  printf   ("packs to packets ratio                 1 : ");
	  scanf ("%d", &packets_per_pack);
	  printf ("\nSTD video buffer in kB (CSPS: max 46 kB) : ");
	  scanf ("%d", &video_buffer_size);
	  printf   ("STD audio buffer in kB (CSPS: max  4 kB) : ");
	  scanf ("%d", &audio_buffer_size);

	}

	
  /* These are used to make (conservative) decisions
	 about whether a packet should fit into the recieve buffers... 
	 Audio packets always have PTS fields, video packets needn't.	
  */ 
  audio_max_packet_data = packet_payload( FALSE, FALSE, FALSE, TRUE, FALSE );
  video_max_packet_data = packet_payload( FALSE, FALSE, FALSE, FALSE, FALSE );

  if( opt_mpeg == 2 )
	{	
	  min_packet_data = sector_size - MPEG2_PACK_HEADER_SIZE - 
		PACKET_HEADER_SIZE - MPEG2_AFTER_PACKET_LENGTH_MAX;
	  max_packet_data = sector_size - PACKET_HEADER_SIZE - MPEG2_AFTER_PACKET_LENGTH_MAX;
	  last_scr_byte_in_pack = MPEG2_LAST_SCR_BYTE_IN_PACK;
	  pack_header_size = MPEG2_PACK_HEADER_SIZE;
    }
  else
    {
	  min_packet_data = sector_size - MPEG1_PACK_HEADER_SIZE -
		PACKET_HEADER_SIZE - MPEG1_AFTER_PACKET_LENGTH_MAX;
	  max_packet_data = sector_size - PACKET_HEADER_SIZE - MPEG1_AFTER_PACKET_LENGTH_MAX;
	  last_scr_byte_in_pack = MPEG1_LAST_SCR_BYTE_IN_PACK;
	  pack_header_size = MPEG1_PACK_HEADER_SIZE;

 	}

  if( always_sys_header_in_pack )
	min_packet_data -= SYS_HEADER_SIZE;

  /* if we have only one stream, we have 3 more bytes in the sys header free */

  if (which_streams != STREAMS_BOTH) { 
	min_packet_data += 3; 
  } 	
     
  /* Only meaningful for MPEG-2 un-used at present, always 0 */
  rate_restriction_flag = 0;
     
 
}


/******************************************************************
    Program start-up packets.  Generate any irregular packets						needed at the start of the stream...
   
******************************************************************/

void outputstreamprefix()
{
}

/******************************************************************
    Program shutdown packets.  Generate any irregular packets						needed at the end of the stream...
   
******************************************************************/

void outputstreamsuffix(Timecode_struc *SCR,
						FILE *ostream,
						unsigned long long  *bytes_output,
						unsigned int mux_rate)
{
  unsigned char *index;
  if (trailing_pad_pack )
	{
	  output_padding( SCR, ostream, bytes_output,
					  mux_rate,
					  0,
					  TRUE,
					  FALSE,
					  FALSE);
	}
  index = cur_sector.buf;
    
  /* TODO: MPEG-2 variant...??? */
  /* ISO 11172 END CODE schreiben				*/
  /* write out ISO 11172 END CODE				*/
  /* TODO: VCD/SVCD/DVD support should generate a nice end packet here */
  index = cur_sector.buf;

  *(index++) = (unsigned char)((ISO11172_END)>>24);
  *(index++) = (unsigned char)((ISO11172_END & 0x00ff0000)>>16);
  *(index++) = (unsigned char)((ISO11172_END & 0x0000ff00)>>8);
  *(index++) = (unsigned char)(ISO11172_END & 0x000000ff);

  while( index < cur_sector.buf + sector_size )
  	*(index++) = 0;
  fwrite (cur_sector.buf, sizeof (unsigned char), sector_size, ostream);
  *bytes_output += sector_size;
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

void outputstream ( char 		*video_file,
					Video_struc 	*video_info,
					char 		*audio_file,
					Audio_struc 	*audio_info,
					char 		*multi_file,
					Vector	   vaunit_info_vec,
					Vector     aaunit_info_vec
					)

{

	int in_time;
	Vaunit_struc video_au;		/* Video Access Unit	*/
	Aaunit_struc audio_au;		/* Audio Access Unit	*/

	unsigned int data_rate=0;		/* AudioVideo Byterate	*/
	unsigned int video_rate=0;
	unsigned int audio_rate=0;
	double delay,audio_delay,video_delay;
	double clock_cycles;
	double audio_next_clock_cycles;
	double video_next_clock_cycles;
	double dmux_rate;
	unsigned int sectors_delay,video_delay_ms,audio_delay_ms;
	unsigned int mux_rate;
	unsigned char picture_start;
	unsigned char audio_frame_start;
	unsigned int audio_bytes;
	unsigned int video_bytes;

	unsigned int nsec_a=0;
	unsigned int nsec_v=0;
	unsigned int nsec_p=0;


	Timecode_struc SCR_audio_delay;
	Timecode_struc SCR_video_delay;
	Timecode_struc current_SCR;
	Timecode_struc audio_next_SCR;
	Timecode_struc video_next_SCR;

	Buffer_struc video_buffer;
	Buffer_struc audio_buffer;


	unsigned int packets_left_in_pack;
	unsigned char padding_packet;
	unsigned char start_of_new_pack;
	unsigned char include_sys_header;
	unsigned int packet_data_size;


  /* Oeffne alle Ein- und Ausgabefiles			*/
  /* Open in- and outputstream				*/

	if (which_streams & STREAMS_VIDEO) istream_v = fopen (video_file, "rb");
	if (which_streams & STREAMS_AUDIO) istream_a = fopen (audio_file, "rb");

	ostream	= fopen (multi_file, "wb");

	/* Einlesen erster Access Unit Informationen		*/
	/* read in first access unit information			*/

	empty_vaunit_struc (&video_au);
	empty_aaunit_struc (&audio_au);

	if (which_streams & STREAMS_AUDIO) {
		audio_au = *(Aaunit_struc*)VectorNext( aaunit_info_vec );
		audio_frame_start = TRUE;
	}
	if (which_streams & STREAMS_VIDEO) {
		video_au = *(Vaunit_struc*)VectorNext( vaunit_info_vec );
		picture_start = TRUE;
	}

	printf("\nMerging elementary streams to MPEG/SYSTEMS multiplexed stream.\n");

	packets_left_in_pack = packets_per_pack;
	picture_start     = FALSE;
	audio_frame_start = FALSE;
	include_sys_header = TRUE;
 

  /*	DTS ist in den ersten Units i.d.R. gleich null. Damit
		kein Bufferunterlauf passiert, muss berechnet werden, 
		wie lange es dauert, bis alle Daten sowohl des ersten
		Video- als auch des ersten Audio-Access units ankommen.
		Diesen Wert dann als Art Startup-Delay zu den Time-
		stamps dazurechnen. Um etwas Spielraum zu haben, wird
		als Wert einfach die Anzahl der zu uebertragenden
		Sektoren aufgerundet.					*/
  /*  DTS of the first units is supposed to be zero. To avoid
	  Buffer underflow, we have to compute how long it takes for
	  all first Video and Audio access units to arrive at the 
	  system standard decoder buffer. This delay is added as a 
	  kind of startup delay to all of the TimeStamps. We compute
	  a ceiling based on the number of sectors we will have
	  to transport for the first access units */

	if (which_streams & STREAMS_VIDEO) 
    {
		if( opt_VBR )
		{
			video_rate = video_info->peak_bit_rate *50;
			video_buffer_size = video_rate * 4 / 25 ;  
			/* > 3 frames out of 25 or 30 */
			printf( "VBR set - pseudo bit rate = %d vbuffer = %d\n",
					video_rate, video_buffer_size );
		}
		else
		{
			if (video_info->bit_rate > video_info->comp_bit_rate)
				video_rate = video_info->bit_rate * 50;
			else
				video_rate = video_info->comp_bit_rate * 50;
		}
    }
	if (which_streams & STREAMS_AUDIO)
		audio_rate = bitrate_index[3-audio_info->layer][audio_info->bit_rate]*128;

	data_rate = video_rate + audio_rate;
    
	/* Bufferstrukturen Initialisieren				*/
	/* initialize buffer structure				*/

	init_buffer_struc (&video_buffer,video_buffer_size);
	init_buffer_struc (&audio_buffer,audio_buffer_size);

	/* TODO: This is a pretty inexact guess and may need tweaking once we
	 allow more dynamic allocation of space in sectors */
	dmux_rate =  ceil((double)(data_rate) *
					  ((double)(sector_size)/(double)(min_packet_data) +
					   ((double)(sector_size)/(double)(max_packet_data) *
						(double)(packets_per_pack-1.))) / (double)(packets_per_pack) );
	data_rate = ceil(dmux_rate/50.)*50;

	if( opt_interactive_mode )
	{
		printf ("target data rate (e.g. %6u)           : ",data_rate);
		scanf  ("%lf", &dmux_rate);
		printf ("\nstartup sectors_delay                    : ");
		scanf  ("%u", &sectors_delay);
		printf ("video stream startup offset (ms)         : ");
		scanf  ("%u", &video_delay_ms);
		printf ("audio stream startup offset (ms)         : ");
		scanf  ("%u", &audio_delay_ms);
		
	}
	else
	{
		printf ("\ncomputed multiplexed stream data rate    : %07d\n",data_rate * 8);
		if( opt_data_rate != 0 )
			printf ("\ntarget data-rate specified               : %7d\n", opt_data_rate*8 );
		if( opt_data_rate == 0 )
		{
			printf( "Setting best-guess data rate:%7d\n", data_rate*8 );
			dmux_rate = (double)data_rate;
		}
		else if ( opt_data_rate >= data_rate)
		{
			printf( "Setting specified specified data rate: %7d\n", opt_data_rate*8 );
			dmux_rate = (double)opt_data_rate;
		}
		else if ( opt_data_rate < data_rate )
		{
			printf( "Warning: Target data rate lower than computed requirement!\n");
			printf( "N.b. a *small* discrepancy is	common and harmless.\n"); 
			dmux_rate = (double)opt_data_rate;
		}


		if( opt_VBR )
			sectors_delay = video_buffer_size / ( 4 * sector_size );
		else
			sectors_delay = video_buffer_size / ( 2* sector_size);
		video_delay_ms = opt_video_offset;
		audio_delay_ms = opt_audio_offset;
		  
	}

	video_delay = (double)video_delay_ms*(double)(CLOCKS/1000);
	audio_delay = (double)audio_delay_ms*(double)(CLOCKS/1000);

	if( opt_interactive_mode )
	{
		verbose=ask_verbose();
		printf ("\n");
	}

#ifdef TIMER
	gettimeofday (&tp_global_start,NULL);
#endif

	mux_rate = ceil(dmux_rate/50.);
	dmux_rate= mux_rate * 50.;

	/* Wir generieren den System Header				*/
	/* let's generate the system header				*/
	create_sys_header (&sys_header, mux_rate, 1, 1, 1, 1, 1, 1,
					   AUDIO_STR_0, 0, audio_buffer_size/128,
					   VIDEO_STR_0, 1, video_buffer_size/1024, which_streams );
 
	/* Calculate start delay in SCR units */

	delay = ((double)sectors_delay +
			 ceil((double)video_au.length/(double)min_packet_data)  +
			 ceil((double)audio_au.length/(double)min_packet_data )) *
		(double)sector_size/dmux_rate*(double)CLOCKS;

	audio_delay += delay;
	video_delay += delay;

	make_timecode (audio_delay, &SCR_audio_delay);
	make_timecode (video_delay, &SCR_video_delay);

	add_to_timecode	(&SCR_video_delay, &video_au.DTS);
	add_to_timecode	(&SCR_video_delay, &video_au.PTS);
	add_to_timecode	(&SCR_audio_delay, &audio_au.PTS);

	bytes_output = 0;

  /* 	Jetzt probieren wir mal, Unit fuer Unit auszulesen und 
		ins Outputstream auszuschreiben. Die Schwierigkeit liegt
		darin, dass die Buffer konstant ueberprueft werden muessen
		und dass die jeweilige Access Unit auch innerhalb des DTS
		eintreten muss. Es kann passieren, dass z.B. im Video-
		Buffer noch ein altes Picture liegt, das noch dekodiert
		werden muss, wir jetzt schon das naechste schicken, aber
		nach einigen Packets der Buffer voll ist. Da darf nichts
		mehr geschickt werden, bis die DTS des alten Bildes
		eingetreten ist und damit der Buffer kleiner geworden ist.
		In der Zwischenzeit kann ein Audio-Packet geschickt werden
		und/oder ein Padding-Packet generiert werden.		*/
  /*  Let's try to read in unit after unit and to write it out into
	  the outputstream. The only difficulty herein lies into the 
	  buffer management, and into the fact the the actual access
	  unit *has* to arrive in time, that means the whole unit
	  (better yet, packet data), has to arrive before arrival of
	  DTS. If both buffers are full we'll generate a padding packet */


	status_header ();


	while ((video_au.length + audio_au.length) > 0)
	{
		padding_packet = FALSE;
		if (packets_left_in_pack == packets_per_pack) 
		{
			start_of_new_pack = TRUE;
			packet_data_size = min_packet_data;
		} else 
		{
			start_of_new_pack = FALSE;
			packet_data_size = max_packet_data;
		}




		/* Calculate amount of data to be moved for the next AU's.
		*/
		audio_bytes = (audio_au.length/min_packet_data)*sector_size +
			(audio_au.length%min_packet_data)+(sector_size-min_packet_data);
		video_bytes = (video_au.length/min_packet_data)*sector_size +
			(video_au.length%min_packet_data)+(sector_size-min_packet_data);


		clock_cycles = ((double)(bytes_output+last_scr_byte_in_pack))*CLOCKS/dmux_rate;
	
		/* Calculate when the the next AU's will under the assumption
		   that the next sector carries a *different* payload.  Using
		   this time we can see if a stream will definately under-run
		   if no sector is sent immediately.  Since a sector is relatively
		   small compared to buffer sizes we use the exact same time measure
		   to detect "actual" under-runs.  
		   
		*/
		audio_next_clock_cycles = (((double)bytes_output)+((double)sector_size)+
								   ((double)audio_bytes))*CLOCKS/dmux_rate;
		video_next_clock_cycles = (((double)bytes_output)+((double)sector_size)+
								   ((double)video_bytes))*CLOCKS/dmux_rate;

		make_timecode (clock_cycles, &current_SCR);
		make_timecode (audio_next_clock_cycles, &audio_next_SCR);
		make_timecode (video_next_clock_cycles, &video_next_SCR);


		if (which_streams & STREAMS_AUDIO) 
			buffer_clean (&audio_buffer, &current_SCR);
		if (which_streams & STREAMS_VIDEO) 
			buffer_clean (&video_buffer, &current_SCR);

		/*
		  printf( "BO=%lld VNCF=%f VNSCR=%lld VDTS=%lld\n",
		  bytes_output, video_next_clock_cycles, video_next_SCR.thetime, 
		  video_au.DTS.thetime );
		  printf( "VAUL = %d RCS=%lld FCS=%g ANSCR=%lld\n",
		  video_au.length, 
		  (bytes_output+MPEG1_LAST_SCR_BYTE_IN_PACK)*((unsigned long long)CLOCKS)/((unsigned long long)dmux_rate),
		  ((double)(bytes_output+MPEG1_LAST_SCR_BYTE_IN_PACK))*CLOCKS/dmux_rate,
		  audio_next_SCR.thetime);
		*/

		/* CASE: Audio Buffer OK, Audio Data ready
		   SEND An audio packet
		*/
	
		/* Heuristic... if we can we prefer to send audio rather than video. 
		   Even a few uSec under-run are audible and in any case the data-rate
		   is trivial compared with video. The only exception is if not
		   sending video would cause it to under-run but there's no danger of
		   and audio under-run
		   	   
		*/
		if ( (buffer_space (&audio_buffer) >= audio_max_packet_data)
			 && (audio_au.length>0)
			 && ! (  video_au.length !=0 &&
					 ! comp_timecode (&video_next_SCR, &video_au.DTS) &&
					 comp_timecode (&audio_next_SCR, &audio_au.PTS ) 
				 )
			)
		{
			
			in_time = comp_timecode (&audio_next_SCR, &audio_au.PTS);
			output_audio (&current_SCR, &SCR_audio_delay, 
						  istream_a, ostream, 
						  &audio_buffer, &audio_au, aaunit_info_vec,
						  &audio_frame_start,
						  &bytes_output, mux_rate,
						  start_of_new_pack,
						  include_sys_header);

#ifdef TIMER
			gettimeofday (&tp_start,NULL);
#endif 
			status_info (++nsec_a, nsec_v, nsec_p, bytes_output, 
						 buffer_space(&video_buffer),
						 buffer_space(&audio_buffer),verbose);
			if( ! in_time )
				status_message (STATUS_AUDIO_TIME_OUT);


#ifdef TIMER
			gettimeofday (&tp_end,NULL);
			total_sec  += (tp_end.tv_sec - tp_start.tv_sec);
			total_usec += (tp_end.tv_usec - tp_start.tv_usec);
#endif
		}

		/* CASE: Video Buffer OK, Video Data ready  (implicitly -
		   no audio packet to send 
		   SEND a video packet.
		*/

		else if( buffer_space (&video_buffer) >= video_max_packet_data
				 && video_au.length>0
			)
		{
			/* video packet schicken */
			/* write out video packet */
			in_time = comp_timecode (&video_next_SCR, &video_au.DTS);
			output_video (&current_SCR, &SCR_video_delay, 
						  istream_v, ostream, 
						  &video_buffer, &video_au, 
						  vaunit_info_vec, &picture_start,
						  &bytes_output, mux_rate, 
						  start_of_new_pack,
						  include_sys_header);

			/* status info */
#ifdef TIMER
			gettimeofday (&tp_start,NULL);
#endif 
			status_info (nsec_a, ++nsec_v, nsec_p, bytes_output,
						 buffer_space(&video_buffer),
						 buffer_space(&audio_buffer),verbose);
			if( ! in_time )
				status_message (STATUS_VIDEO_TIME_OUT);

#ifdef TIMER
			gettimeofday (&tp_end,NULL);
			total_sec  += (tp_end.tv_sec - tp_start.tv_sec);
			total_usec += (tp_end.tv_usec - tp_start.tv_usec);
#endif
		}

		/* CASE: Audio Buffer and Video Buffers NOT OK (too full to send)
		   SEND padding packet */
		else
		{
			/* padding packet schicken */
			/* write out padding packet */
			output_padding (&current_SCR, ostream, 
							&bytes_output, mux_rate,  
							packet_data_size, start_of_new_pack, include_sys_header, opt_VBR);
			padding_packet =TRUE;

#ifdef TIMER
			gettimeofday (&tp_start,NULL);
#endif 
			/* In case of VBR "padding" packets are stripped */
			if( ! opt_VBR )
				status_info (nsec_a, nsec_v, ++nsec_p, bytes_output, 
							 buffer_space(&video_buffer),
							 buffer_space(&audio_buffer),verbose);
		
#ifdef TIMER
			gettimeofday (&tp_end,NULL);
			total_sec  += (tp_end.tv_sec - tp_start.tv_sec);
			total_usec += (tp_end.tv_usec - tp_start.tv_usec);
#endif
		}
	
		/* Update the counter for pack packets.  VBR is a tricky case as here padding
		   packets are "virtual" */
		if( ! opt_VBR || !padding_packet )
		{
			--packets_left_in_pack;
			if (packets_left_in_pack == 0) 
				packets_left_in_pack = packets_per_pack;
		}
		/* Unless sys headers are always required we turn them off after the first
		   packet has been generated */
		include_sys_header = always_sys_header_in_pack;
	}



#ifdef TIMER
	gettimeofday (&tp_start,NULL);
#endif 
	/* status info*/
	status_info (nsec_a, nsec_v, nsec_p, bytes_output, 
				 buffer_space(&video_buffer),
				 buffer_space(&audio_buffer),2); 
	status_footer ();
#ifdef TIMER
	gettimeofday (&tp_end,NULL);
	total_sec  += (tp_end.tv_sec - tp_start.tv_sec);
	total_usec += (tp_end.tv_usec - tp_start.tv_usec);
#endif

	outputstreamsuffix( &current_SCR, ostream, &bytes_output, mux_rate);

  /* Schliesse alle Ein- und Ausgabefiles			*/
  /* close all In- and Outputfiles				*/

	fclose (ostream);
	if (which_streams & STREAMS_AUDIO) fclose (istream_a);
	if (which_streams & STREAMS_VIDEO) fclose (istream_v);


#ifdef TIMER
	gettimeofday (&tp_global_end, NULL);
	global_sec = 10*(tp_global_end.tv_sec - tp_global_start.tv_sec);
	global_usec= 10*(tp_global_end.tv_usec - tp_global_start.tv_usec);
	global_sec += (global_usec / 100000);
	total_sec *= 10;
	total_sec  += (total_usec  / 100000);

	printf ("Timing global: %10.1f secs\n",(float)global_sec/10.);
	printf ("Timing IO    : %10.1f secs\n",(float)total_sec/10.);
#endif
    
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
							 Vaunit_struc *video_au,
							 unsigned int *bytes_left,
							 unsigned char *picture_start,
							 Timecode_struc *SCR_delay,
							 Vector vaunit_info_vec
							 )
{

  Vaunit_struc *vau;

  if (*bytes_left == 0)
	return;

  while (video_au->length < *bytes_left)
	{
	  queue_buffer (buffer, video_au->length, &video_au->DTS);
	  *bytes_left -= video_au->length;
	  vau = (Vaunit_struc*)VectorNext( vaunit_info_vec );
	  if( vau != NULL )
		*video_au = *vau;
	  else
	    {
		  empty_vaunit_struc (video_au);
		  status_message(STATUS_VIDEO_END);
		  return;
	    }
	  *picture_start = TRUE;
	  add_to_timecode (SCR_delay, &video_au->DTS);
	  add_to_timecode (SCR_delay, &video_au->PTS);
	};

  if (video_au->length > *bytes_left)
	{
	  queue_buffer (buffer, *bytes_left, &video_au->DTS);
	  video_au->length -= *bytes_left;
	  *picture_start = FALSE;
	} else
	  if (video_au->length == *bytes_left)
		{
		  queue_buffer (buffer, *bytes_left, &video_au->DTS);
		  vau = (Vaunit_struc*)VectorNext( vaunit_info_vec );
		  if( vau != NULL )
			*video_au = *vau;
		  else
			{
			  empty_vaunit_struc (video_au);
			  status_message(STATUS_VIDEO_END);
			  return;
			}
		  *picture_start = TRUE;
		  add_to_timecode (SCR_delay, &video_au->DTS);
		  add_to_timecode (SCR_delay, &video_au->PTS);
		};

}


/******************************************************************
	Output_Video
	generiert Pack/Sys_Header/Packet Informationen aus dem
	Video Stream und speichert den so erhaltenen Sektor ab.

	generates Pack/Sys_Header/Packet information from the
	video stream and writes out the new sector
******************************************************************/

void output_video ( Timecode_struc *SCR,
					Timecode_struc *SCR_delay,
					FILE *istream_v,
					FILE *ostream,
					Buffer_struc *buffer,
					Vaunit_struc *video_au,
					Vector vaunit_info_vec,
					unsigned char *picture_start,
					unsigned long long *bytes_output,
					unsigned int mux_rate,
					unsigned char start_of_new_pack,
					unsigned char include_sys_header)

{

  unsigned int bytes_left;
  unsigned int temp;
  Pack_struc *pack_ptr = NULL;
  Sys_header_struc *sys_header_ptr = NULL;
  unsigned char timestamps;
  Vaunit_struc *vau, *next_vau;
  Pack_struc pack;
  unsigned int old_au_then_new_payload;

  if (start_of_new_pack)
    {
	  /* Wir generieren den Pack Header				*/
	  /* let's generate pack header					*/
	  create_pack (&pack, SCR, mux_rate);
	  pack_ptr = &pack;
	  if( include_sys_header )
		sys_header_ptr = &sys_header;
    }
        
  /* Figure out the threshold payload size below which we can fit more than one AU into a packet
	 N.b. because fitting more than one in imposses an overhead of additional header
	 fields so there is a dead spot where we *have* to stuff the packet rather than start
	 fitting in an extra AU.
  */

  next_vau = VectorLookAhead( vaunit_info_vec, 1 );
  if( next_vau != NULL )
	{
	  old_au_then_new_payload = 
		packet_payload( sys_header_ptr!=0, pack_ptr!=0, TRUE, TRUE, next_vau->type != BFRAME );
	}
  else
	old_au_then_new_payload = packet_payload( sys_header_ptr!=0, pack_ptr!=0, TRUE, FALSE, FALSE );
  /* Wir generieren das Packet				*/
  /* let's generate packet					*/

  /* faengt im Packet ein Bild an?				*/
  /* does a frame start in this packet?			*/
    
  /* FALL: Packet beginnt mit neuer Access Unit		*/
  /* CASE: Packet starts with new access unit			*/
  if (*picture_start)
	{
	  if (video_au->type == BFRAME)
		timestamps=TIMESTAMPBITS_PTS;
	  else
		timestamps=TIMESTAMPBITS_PTS_DTS;

	  create_sector ( &cur_sector, pack_ptr, sys_header_ptr,
					  0,
					  istream_v, VIDEO_STR_0, 1, video_buffer_size/1024,
					  TRUE, &video_au->PTS, &video_au->DTS,
					  timestamps );

	  bytes_left = cur_sector.length_of_packet_data;
	  next_video_access_unit (buffer, video_au, &bytes_left, 
							  picture_start, SCR_delay, vaunit_info_vec);

	}

  /* FALL: Packet beginnt mit alter Access Unit, es kommt	*/
  /*       keine neue im selben Packet vor			*/
  /* CASE: Packet begins with old access unit, no new one	*/
  /*	     begins in the very same packet			*/
  else if (!(*picture_start) &&
		   (video_au->length >= old_au_then_new_payload))
	{
	  create_sector( &cur_sector, pack_ptr, sys_header_ptr,
					 video_au->length,
					 istream_v, VIDEO_STR_0, 1, video_buffer_size/1024,
					 TRUE, NULL, NULL,
					 TIMESTAMPBITS_NO );

	  bytes_left = cur_sector.length_of_packet_data;	
	  next_video_access_unit (buffer, video_au, &bytes_left, 
							  picture_start, SCR_delay, vaunit_info_vec);

	}

  /* FALL: Packet beginnt mit alter Access Unit, es kommt	*/
  /*       eine neue im selben Packet vor			*/
  /* CASE: Packet begins with old access unit, a new one	*/
  /*	     begins in the very same packet			*/
  else if (!(*picture_start) && 
		   (video_au->length < old_au_then_new_payload))
	{
	  temp = video_au->length;
	  queue_buffer (buffer, video_au->length, &video_au->DTS);

	  /* gibt es ueberhaupt noch eine Access Unit ? */
	  /* is there a new access unit anyway? */

 
	  vau = (Vaunit_struc*)VectorNext( vaunit_info_vec );
	  if( vau != NULL )
		{
		  *video_au = *vau;
		  if (video_au->type == BFRAME)
			timestamps=TIMESTAMPBITS_PTS;
		  else
			timestamps=TIMESTAMPBITS_PTS_DTS;

		  *picture_start = TRUE;
		  add_to_timecode (SCR_delay, &video_au->DTS);
		  add_to_timecode (SCR_delay, &video_au->PTS);
		  create_sector (&cur_sector, pack_ptr, sys_header_ptr,
						 0,
						 istream_v, VIDEO_STR_0, 1, video_buffer_size/1024,
						 TRUE, &video_au->PTS, &video_au->DTS,
						 timestamps );
		  bytes_left = cur_sector.length_of_packet_data - temp;
		  next_video_access_unit (buffer, video_au, &bytes_left, 
								  picture_start, SCR_delay, vaunit_info_vec);
		} 
	  else
		{
		  status_message(STATUS_VIDEO_END);
		  empty_vaunit_struc (video_au);
		  create_sector ( &cur_sector, pack_ptr, sys_header_ptr,
						  0,
						  istream_v, VIDEO_STR_0, 1, video_buffer_size/1024,
						  TRUE, NULL, NULL,
						  TIMESTAMPBITS_NO );
		};
#ifdef TIMER
	  gettimeofday (&tp_end,NULL);
	  total_sec  += (tp_end.tv_sec - tp_start.tv_sec);
	  total_usec += (tp_end.tv_usec - tp_start.tv_usec);
#endif

	}


  /* Sector auf Platte schreiben				*/
  /* write out sector						*/
#ifdef TIMER
  gettimeofday (&tp_start,NULL);
#endif 
  fwrite (cur_sector.buf, cur_sector.length_of_sector, 1, ostream);
#ifdef TIMER
  gettimeofday (&tp_end,NULL);
  total_sec  += (tp_end.tv_sec - tp_start.tv_sec);
  total_usec += (tp_end.tv_usec - tp_start.tv_usec);
#endif
  *bytes_output += cur_sector.length_of_sector;
	
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
							 Aaunit_struc *audio_au,
							 unsigned int *bytes_left,
							 unsigned char *audio_frame_start,
							 Timecode_struc *SCR_delay,
							 Vector aaunit_info_vec
							 )

{
  Aaunit_struc *aau;

  if (*bytes_left == 0)
	return;

  while (audio_au->length < *bytes_left)
	{
	  queue_buffer (buffer, audio_au->length, &audio_au->PTS);
	  *bytes_left -= audio_au->length;
	  aau = (Aaunit_struc*)VectorNext( aaunit_info_vec );
	  if( aau != NULL )
		*audio_au = *aau;
	  else
	    {
		  empty_aaunit_struc (audio_au);
		  status_message(STATUS_AUDIO_END);
		  return;
	    }
	  *audio_frame_start = TRUE;
	  add_to_timecode (SCR_delay, &audio_au->PTS);
	};

  if (audio_au->length > *bytes_left)
	{
	  queue_buffer (buffer, *bytes_left, &audio_au->PTS);
	  audio_au->length -= *bytes_left;
	  *audio_frame_start = FALSE;
	} else
	  if (audio_au->length == *bytes_left)
		{
		  queue_buffer (buffer, *bytes_left, &audio_au->PTS);
		  aau = (Aaunit_struc*)VectorNext( aaunit_info_vec );
		  if( aau != NULL )
			*audio_au = *aau;
		  else
			{
			  empty_aaunit_struc (audio_au);
			  status_message(STATUS_AUDIO_END);
			  return;
			}
		  *audio_frame_start = TRUE;
		  add_to_timecode (SCR_delay, &audio_au->PTS);
		};

}

/******************************************************************
	Output_Audio
	erstellt Pack/Sys_Header/Packet Informationen aus dem
	Audio Stream und speichert den so erhaltenen Sector ab.

	generates Pack/Sys Header/Packet information from the
	audio stream and saves them into the sector
******************************************************************/

void output_audio ( Timecode_struc *SCR,
					Timecode_struc *SCR_delay,
					FILE *istream_a,
					FILE *ostream,
					Buffer_struc *buffer,
					Aaunit_struc *audio_au,
					Vector aaunit_info_vec,
					unsigned char *audio_frame_start,
					unsigned long long  *bytes_output,
					unsigned int mux_rate,
					unsigned char start_of_new_pack,
					unsigned char include_sys_header
					)

{

  unsigned int bytes_left;
  unsigned int temp;
  Pack_struc *pack_ptr = NULL;
  Sys_header_struc *sys_header_ptr = NULL;
  Aaunit_struc *aau;
  Pack_struc pack;
	
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
					 0,
					 istream_a, AUDIO_STR_0, 0, audio_buffer_size/128,
					 TRUE, &audio_au->PTS, NULL,
					 TIMESTAMPBITS_PTS);

	  bytes_left = cur_sector.length_of_packet_data;

	  next_audio_access_unit (buffer, audio_au, &bytes_left, 
							  audio_frame_start, SCR_delay, aaunit_info_vec);
    }

  /* FALL: Packet beginnt mit alter Access Unit, es kommt	*/
  /*       keine neue im selben Packet vor			*/
  /* CASE: packet starts with old access unit, no new one	*/
  /*       starts in this very same packet			*/
  else if (!(*audio_frame_start) && 
		   (audio_au->length >= packet_payload( sys_header_ptr!=0, pack_ptr!=0, TRUE, FALSE, FALSE )))
    {
	  create_sector (&cur_sector, pack_ptr, sys_header_ptr,
					 0,
					 istream_a, AUDIO_STR_0, 0, audio_buffer_size/128,
					 TRUE, NULL, NULL,
					 TIMESTAMPBITS_NO );

	  bytes_left = cur_sector.length_of_packet_data;

	  next_audio_access_unit (buffer, audio_au, &bytes_left,
							  audio_frame_start, SCR_delay, aaunit_info_vec);
    }

  /* FALL: Packet beginnt mit alter Access Unit, es kommt	*/
  /*       eine neue im selben Packet vor			*/
  /* CASE: packet starts with old access unit, a new one	*/
  /*       starts in this very same packet			*/
  else if (!(*audio_frame_start) && 
		   (audio_au->length < packet_payload( sys_header_ptr!=0, pack_ptr!=0, TRUE, TRUE, FALSE )))
    {
	  temp = audio_au->length;
	  queue_buffer (buffer, audio_au->length, &audio_au->PTS);

	  /* gibt es ueberhaupt noch eine Access Unit ? */
	  /* is there another access unit anyway ? */
	  aau = (Aaunit_struc*)VectorNext( aaunit_info_vec );
	  if( aau != NULL )
		{
		  *audio_au = *aau;
		  *audio_frame_start = TRUE;
		  add_to_timecode (SCR_delay, &audio_au->PTS);
		  create_sector (&cur_sector, pack_ptr, sys_header_ptr,
						 0,
						 istream_a, AUDIO_STR_0, 0, audio_buffer_size/128,
						 TRUE, &audio_au->PTS, NULL,
						 TIMESTAMPBITS_PTS );

		  bytes_left = cur_sector.length_of_packet_data - temp;

		  next_audio_access_unit (buffer, audio_au, &bytes_left, 
								  audio_frame_start, SCR_delay, aaunit_info_vec );
		} else
		  {
			status_message(STATUS_AUDIO_END);
			empty_aaunit_struc (audio_au);
			create_sector (&cur_sector, pack_ptr, sys_header_ptr,
						   0,
						   istream_a, AUDIO_STR_0, 0, audio_buffer_size/128,
						   TRUE, NULL, NULL,
						   TIMESTAMPBITS_NO );
		  };
#ifdef TIMER
	  gettimeofday (&tp_end,NULL);
	  total_sec  += (tp_end.tv_sec - tp_start.tv_sec);
	  total_usec += (tp_end.tv_usec - tp_start.tv_usec);
#endif


    }

  /* Sector auf Platte schreiben				*/
  /* write out sector onto disk				*/
#ifdef TIMER
  gettimeofday (&tp_start,NULL);
#endif 
  fwrite (cur_sector.buf, cur_sector.length_of_sector, 1, ostream);
#ifdef TIMER
  gettimeofday (&tp_end,NULL);
  total_sec  += (tp_end.tv_sec - tp_start.tv_sec);
  total_usec += (tp_end.tv_usec - tp_start.tv_usec);
#endif
  *bytes_output += cur_sector.length_of_sector;

	
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
******************************************************************/

void output_padding (
					 Timecode_struc *SCR,
					 FILE *ostream,
					 unsigned long long  *bytes_output,
					 unsigned int mux_rate,
					 unsigned long packet_data_size,
					 unsigned char start_of_new_pack,
					 unsigned char include_sys_header,
					 unsigned char VBR_pseudo
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
					 0,
					 NULL, PADDING_STR, 0, 0,
					 FALSE, NULL, NULL,
					 TIMESTAMPBITS_NO );

#ifdef TIMER
	  gettimeofday (&tp_start,NULL);
#endif 
	  fwrite (cur_sector.buf, cur_sector.length_of_sector*sizeof (unsigned char), 1,
			  ostream);
	}

#ifdef TIMER
  gettimeofday (&tp_end,NULL);
  total_sec  += (tp_end.tv_sec - tp_start.tv_sec);
  total_usec += (tp_end.tv_usec - tp_start.tv_usec);
#endif
  *bytes_output += cur_sector.length_of_sector;
	
}

