
/*************************************************************************
*  mplex - MPEG/SYSTEMS multiplexer
*  Copyright (C) 1994 1995 Christoph Moar
*  Siemens ZFE ST SN 11 / T SN 6
*  moar@informatik.tu-muenchen.de 
*       (Christoph Moar)
*
*  Copyright (C) 2000, 2001, Andrew Stevens <andrew.stevens@philips.com>
*
*  This program is free software; you can redistribute it and/or modify	 *
*  it under the terms of the GNU General Public License as published by	 *
*  the Free Software Foundation; either version 2 of the License, or	 *
*  (at your option) any later version.					 *
*									 *
*  This program is distributed in the hope that it will be useful,	 *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of	 *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the	 *
*  GNU General Public License for more details.				 *
*									 *
*  You should have received a copy of the GNU General Public License	 *
*  along with this program; if not, write to the Free Software		 *
*  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.		 *
*************************************************************************/

#include <config.h>
#include <stdio.h>
#include <inttypes.h>
#include "bits.hh"
#ifdef TIMER
#include <sys/time.h>
#endif


#include "vector.hh"
#include "aunit.hh"

#include "mjpeg_logging.h"


/*************************************************************************
    Definitionen
*************************************************************************/
 
#define MPLEX_VER    "1.5.1"
#define MPLEX_DATE   "$Date$"

/* Buffer size parameters */

#define MAX_SECTOR_SIZE         16384
#define MAX_PACK_HEADER_SIZE	255
#define MAX_SYS_HEADER_SIZE     255


#define SEQUENCE_HEADER 	0x000001b3
#define SEQUENCE_END		0x000001b7
#define PICTURE_START		0x00000100
#define EXT_START_CODE 0x000001b5
#define GROUP_START		0x000001b8
#define SYNCWORD_START		0x000001
#define OLDFRAME				0
#define IFRAME                  1
#define PFRAME                  2
#define BFRAME                  3
#define DFRAME                  4

#define CODING_EXT_ID       8
#define AUDIO_SYNCWORD		0x7ff


#define PACK_START		0x000001ba
#define SYS_HEADER_START	0x000001bb
#define ISO11172_END		0x000001b9
#define PACKET_START		0x000001

#define MAX_FFFFFFFF		4294967295.0 	/* = 0xffffffff in dec.	*/

#define CLOCKS			(300 *90000)		/* MPEG-2 System Clock Hertz - we divide down by 300.0 for MPEG-1*/

/* Range of sizes of the fields following the packet length field in packet header:
	used to calculate if recieve buffers will have enough space... */

#define MPEG2_BUFFERINFO_LENGTH 3
#define MPEG1_BUFFERINFO_LENGTH 2
#define DTS_PTS_TIMESTAMP_LENGTH 5
#define MPEG2_AFTER_PACKET_LENGTH_MIN    3
#define MPEG1_AFTER_PACKET_LENGTH_MIN    (0+1)

	/* Sector under-size below which header stuffing rather than padding packets
		or post-packet zero stuffing is used.  *Must* be less than 20 for VCD
		multiplexing to work correctly!
	 */
	 
#define MINIMUM_PADDING_PACKET_SIZE 10

	/* 
	   A "safety margin" for the audio buffer just in case the
	   player doesn't *quite* clear it as quickly as we guess.
	   We don't send a new audio packet until there ought to be at least this
	   much space.
	*/
#define AUDIO_BUFFER_FILL_MARGIN 300

/* The following values for sys_header_length & size are only valid for */
/* System streams consisting of two basic streams. When wrapping around */
/* the system layer on a single video or a single audio stream, those   */
/* values get decreased by 3.                                           */


#define PACKET_HEADER_SIZE	6

#define VIDEOCD_SECTOR_SIZE	2324	        /* VideoCD sector size */
#define SVCD_SECTOR_SIZE        2324            /* Super VideoCD sector size */
#define DVD_SECTOR_SIZE         2048            /* DVD sector size     */


#define STREAMS_VIDEO           1
#define STREAMS_AUDIO           2
#define STREAMS_BOTH            3

#define AUDIO_STREAMS		0xb8		/* Marker Audio Streams	*/
#define VIDEO_STREAMS		0xb9		/* Marker Video Streams	*/
#define AUDIO_STR_0		0xc0		/* Marker Audio Stream0	*/
#define VIDEO_STR_0		0xe0		/* Marker Video Stream0	*/
#define PADDING_STR		0xbe		/* Marker Padding Stream*/

#define ZERO_STUFFING_BYTE	0
#define STUFFING_BYTE		0xff
#define RESERVED_BYTE		0xff
#define TIMESTAMPBITS_NO		0		/* Flag NO timestamps	*/
#define TIMESTAMPBITS_PTS		2		/* Flag PTS timestamp	*/
#define TIMESTAMPBITS_DTS		1		/* Flag PTS timestamp	*/
#define TIMESTAMPBITS_PTS_DTS	(TIMESTAMPBITS_DTS|TIMESTAMPBITS_PTS)		/* Flag BOTH timestamps	*/

#define MARKER_MPEG1_SCR		2		/* Marker SCR		*/
#define MARKER_MPEG2_SCR        1		/* These don't need to be distinct! */
#define MARKER_JUST_PTS			2		/* Marker only PTS	*/
#define MARKER_PTS				3		/* Marker PTS		*/
#define MARKER_DTS				1		/* Marker DTS		*/
#define MARKER_NO_TIMESTAMPS	0x0f		/* Marker NO timestamps	*/

#define STATUS_AUDIO_TIME_OUT	2		/* Statusmessage A out	*/
#define STATUS_VIDEO_TIME_OUT	3		/* Statusmessage V out	*/

/*************************************************************************
    Typ- und Strukturdefinitionen
*************************************************************************/



class VideoStream
{
public:
	VideoStream() :
		eoscan(false),
		stream_length(0),
		num_sequence(0),
		num_seq_end(0),
		num_pictures(0),
		num_groups(0)
		{
			for( int i =0; i<4; ++i )
				num_frames[i] = avg_frames[i] = 0;
		}


	void Init(char *video_file,	
			  clockticks *ret_first_frame_PTS,
			  off_t length);
	
	void close();
	VAunit *next();
	VAunit *lookahead( unsigned int lookahead );


private:
	static const unsigned int FRAME_CHUNK = 256;
	void fillAUbuffer(int frames_to_buffer);
	void output_seqhdr_info();
	bool eoscan;

public:	
	off_t      file_length;
    bitcount_t stream_length  ;
    unsigned int num_sequence 	;
    unsigned int num_seq_end	;
    unsigned int num_pictures 	;
    unsigned int num_groups 	;
    unsigned int num_frames[4] 	;
    unsigned int avg_frames[4]  ;
    
    unsigned int horizontal_size;
    unsigned int vertical_size 	;
    unsigned int aspect_ratio	;
    unsigned int picture_rate	;
    unsigned int bit_rate 	;
    unsigned int comp_bit_rate	;
    unsigned int peak_bit_rate  ;
    unsigned int vbv_buffer_size;
    unsigned int CSPF 		;
    double secs_per_frame;

	FILE *rawstrm;				// Elementary input stream
	                            // Eventually to be encapsulated
	
private:
	AUStream<VAunit> aunits;
	
	unsigned int last_buffered_AU;		// decode seq num of last buffered frame + 1

	/* State variables for scanning source bit-stream */
    IBitStream bs;
   	bitcount_t AU_start;
    bitcount_t prev_stream_length;
    VAunit access_unit;
    unsigned int syncword;
    unsigned int decoding_order;
	unsigned int fields_presented;
    unsigned int group_order;
    unsigned int group_start_pic;
	unsigned int group_start_field;
    unsigned long temporal_reference;
    unsigned short pict_rate;
	int pulldown_32;
	int repeat_first_field;
	int film_rate;
    unsigned int prozent;
    unsigned int old_prozent;
    double frame_rate;
	unsigned int max_bits_persec;
	int AU_pict_data;
	int AU_hdr;

}; 		

struct Audio_struc	/* Informationen ueber Audio Stream	*/
{   
	Audio_struc() : 
		stream_length(0),
		num_syncword(0)
		{
			for( int i = 0; i <2 ; ++i )
				num_frames[i] = size_frames[i] = 0;
		}
		
	bitcount_t stream_length ;
    unsigned int num_syncword	;
    unsigned int num_frames [2]	;
    unsigned int size_frames[2] ;
	unsigned int version_id ;
    unsigned int layer		;
    unsigned int protection	;
    unsigned int bit_rate	;
    unsigned int frequency	;
    unsigned int mode		;
    unsigned int mode_extension ;
    unsigned int copyright      ;
    unsigned int original_copy  ;
    unsigned int emphasis	;
}; 	

typedef struct sector_struc	/* Ein Sektor, kann Pack, Sys Header	*/
				/* und Packet enthalten.		*/
{   unsigned char  buf [MAX_SECTOR_SIZE] ;
    unsigned int   length_of_packet_data ;
    clockticks TS                ;
} Sector_struc;

typedef struct pack_struc	/* Pack Info				*/
{   unsigned char  buf [MAX_PACK_HEADER_SIZE];
	int length;
    clockticks SCR;
} Pack_struc;

typedef struct sys_header_struc	/* System Header Info			*/
{   unsigned char  buf [MAX_SYS_HEADER_SIZE];
	int length;
} Sys_header_struc;

typedef struct buffer_queue	/* FIFO-Queue fuer STD Buffer		*/
{   unsigned int size	;	/* als verkettete Liste implementiert	*/
    clockticks DTS	;
    struct buffer_queue *next	;
} Buffer_queue;
    

typedef struct buffer_struc	/* Simuliert STD Decoder Buffer		*/
{   unsigned int max_size;	/* enthaelt Anker auf verkettete Liste	*/
    Buffer_queue *first;
} Buffer_struc;
    
    
/*************************************************************************
    Funktionsprototypen, keine Argumente, K&R Style
*************************************************************************/

int intro_and_options( int, char **, char**);
/* Anzeigen des Introbildschirmes und	*/
/* Ueberpruefen der Argumente			*/

void init_stream_syntax_parameters(VideoStream 	*video_info,
							    	Audio_struc 	*audio_info );	
										/* Initialisation of syntax paramters 	*/
										/* based on (checked) options 			*/
										
void check_files (int argc,
				  char* argv[],
				  char**audio_file,
				  char**video_file,
				  off_t &audio_bytes,
				  off_t &video_bytes
	);
bool open_file(const char *name, off_t &size);
void get_info_audio (char *audio_file,
					  Audio_struc *audio_info,
					  clockticks first_frame_PTS,
					  unsigned int length,
					  AUStream<AAunit> &audio_info_vec

					  );

// TODO Get rid of the ugly use of access unit structs with 0 length
// fields a means of signalling "Null AU/beyond end of AU's".

#ifdef REDDUNDANT
void empty_vaunit_struc   (VAunit *);	
void empty_aaunit_struc   (AAunit *);	
void empty_video_struc    ();
void empty_audio_struc    ();
void empty_sector_struc   ();	/* Initialisiert Struktur fuer SUN cc	*/
void empty_clockticks ();	/* Initialisiert Struktur fuer SUN cc	*/
#endif
void init_buffer_struc (Buffer_struc *pointer, unsigned int size);

void offset_timecode      (clockticks *time1,clockticks *time2,clockticks *offset);	/* Rechnet Offset zwischen zwei TimeC.	*/
void copy_timecode        (clockticks *,clockticks *);	/* setzt 2tes TimeC. dem 1ten gleich	*/
void bytepos_timecode(long long bytepos, clockticks *ts);
void make_timecode        (double, clockticks *);	/* rechnet aus double einen TimeC.	*/
				/* und schreibt ihn in clockticks   */
void add_to_timecode      (clockticks *,clockticks *);	/* addiert 1tes TimeC. zum 2ten		*/ 
void buffer_dtspts_mpeg1scr_timecode (clockticks timecode,
									 unsigned char  marker,
									 unsigned char **buffer);
void buffer_mpeg2scr_timecode( clockticks timecode,
								unsigned char **buffer
							 );

int  comp_timecode        (clockticks *,clockticks *);	/* 1tes TimeC. <= 2tes TimeC. ?		*/
unsigned int packet_payload(  Sys_header_struc *sys_header,  
							  Pack_struc *pack_header, 
							  int buffers, int PTSstamp, int DTSstamp );
	/* Compute available packet payload in a sector... */
void create_sector (Sector_struc 	 *sector,
					Pack_struc	     *pack,
					Sys_header_struc *sys_header,
					unsigned int     max_packet_data_size,
					FILE		 *inputstream,

				  unsigned char 	 type,
				  unsigned char 	 buffer_scale,
				  unsigned int 	 	buffer_size,
				  unsigned char 	 buffers,
				  clockticks   		PTS,
				  clockticks   		DTS,
				  unsigned char 	 timestamps
				  );
				  
void create_sys_header (
						Sys_header_struc *sys_header,
						unsigned int	 rate_bound,
						int	 audio_bound,
						int	 fixed,
						int	 CSPS,
						int	 audio_lock,
						int	 video_lock,
						int	 video_bound,

						unsigned int 	 stream1,
						unsigned int 	 buffer1_scale,
						unsigned int 	 buffer1_size,
						unsigned int 	 stream2,
						unsigned int 	 buffer2_scale,
						unsigned int 	 buffer2_size,
						unsigned int     which_streams
						);						/* erstellt einen System Header		*/
void create_pack (
				  Pack_struc	 *pack,
				  clockticks     SCR,
				  unsigned int 	 mux_rate
				);	/* erstellt einen Pack Header		*/

void output_video ( clockticks SCR,
					clockticks SCR_delay,
					VideoStream &vstrm,
					FILE *ostream,
					Buffer_struc *buffer,
					VAunit *video_au,
					unsigned int *new_picture_type,
					unsigned char marker_pack,
					unsigned char include_sys_header
					);
void output_audio ( clockticks SCR,
					clockticks SCR_delay,
					FILE *istream_a,
					FILE *ostream,
					Buffer_struc *buffer,
					AAunit *audio_au,
					AUStream<AAunit> &aaunit_info_vec,
					unsigned char *audio_frame_start,
					unsigned char marker_pack,
					unsigned char include_sys_header,
					unsigned char end_of_segment);

void output_padding       (
					clockticks SCR,
					FILE *ostream,
					unsigned char start_of_new_pack,
					unsigned char include_sys_header,
					unsigned char pseudo_VBR,
					int packet_data_limit
						);	/* erstellt und schreibt Padding packet	*/

void buffer_clean	  (Buffer_struc *buffer, clockticks timenow);
void buffer_flush     (Buffer_struc *buffer);
unsigned int  buffer_space     (Buffer_struc *buffer);	/* Anzahl freier Bytes in Buffer	*/
void queue_buffer     (Buffer_struc *buffer,
						unsigned int bytes,
						clockticks removaltime);	/* An Bufferliste anhaengen		*/

void outputstream ( VideoStream &vstrm,
					char 		*audio_file,
					char 		*multi_file,
					AUStream<AAunit>   &aaunit_info_vec
				 );
FILE *system_open_init( const char *filename_pat );
int system_file_lim_reached(  FILE *cur_system_strm );
FILE *system_next_file( FILE *cur_system_strm, const char *filename_pat );

void status_info (	unsigned int nsectors_a,
					unsigned int nsectors_v,
					unsigned int nsectors_p,
					unsigned long long nbytes,
					unsigned int buf_v,
					unsigned int buf_a,
					log_level_t level
				 );	/* Status line update	*/

void status_header (log_level_t level);
void status_footer (log_level_t level);
void timeout_error	  (int what, int decode_number);





/*************************************************************************
    Statische Arrays
*************************************************************************/

extern unsigned int bitrate_index [4][3][16];

/*************************************************************************
    Command line options and derived parameters
*************************************************************************/



extern int opt_quiet_mode;
extern int opt_interactive_mode;
extern int opt_buffer_size;
extern int opt_data_rate;
extern int opt_video_offset;
extern int opt_audio_offset;
extern int opt_sector_size;
extern int opt_VBR;
extern int opt_mpeg;
extern int opt_mux_format;
extern int opt_multifile_segment;
extern int opt_always_system_headers;
extern int opt_packets_per_pack;
extern bitcount_t opt_max_PTS;
extern int opt_emul_vcdmplex;

extern int verbose;
extern unsigned int which_streams;

extern int packet_overhead;
/* extern int pack_header_size; */
extern int sector_size;
extern int mux_rate;
extern int dmux_rate;
extern int zero_stuffing;

extern intmax_t max_system_segment_size;
