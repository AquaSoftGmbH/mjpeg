/*************************************************************************
*  MPEG SYSTEMS MULTIPLEXER                                              *
*  Erzeugen einer MPEG/SYSTEMS                           		 *
*  MULTIPLEXED VIDEO/AUDIO DATEI					 *
*  aus zwei MPEG Basis Streams						 *
*  Christoph Moar							 *
*  SIEMENS ZFE ST SN 11 / T SN 6					 *
*  (C) 1994 1995    							 *
**************************************************************************
*  Generating a MPEG/SYSTEMS						 *
*  MULTIPLEXED VIDEO/AUDIO STREAM					 *
*  from two MPEG source streams						 *
*  Christoph Moar							 *
*  SIEMENS CORPORATE RESEARCH AND DEVELOPMENT ST SN 11 / T SN 6		 *
*  (C) 1994 1995							 *
**************************************************************************
*  Einschraenkungen vorhanden. Unterstuetzt nicht gesamten MPEG/SYSTEMS  *
*  Standard. Haelt sich i.d.R. an den CSPF-Werten, zusaetzlich (noch)    *
*  nur fuer ein Audio- und/oder ein Video- Stream. Evtl. erweiterbar.    *
**************************************************************************
*  Restrictions apply. Will not support the whole MPEG/SYSTEM Standard.  *
*  Basically, will generate Constrained System Parameter Files.		 *
*  Mixes only one audio and/or one video stream. Might be expanded.	 *
*************************************************************************/

/*************************************************************************
*  mplex - MPEG/SYSTEMS multiplexer					 *
*  Copyright (C) 1994 1995 Christoph Moar				 *
*  Siemens ZFE ST SN 11 / T SN 6					 *
*									 *
*  moar@informatik.tu-muenchen.de 					 *
*       (Christoph Moar)			 			 *
*									 *
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

#include <stdio.h>
#include <inttypes.h>
#include "bits.h"
#ifdef TIMER
#include <sys/time.h>
#endif


#include "vector.h"

typedef uint64_t clockticks;

/*************************************************************************
    Definitionen
*************************************************************************/
 
#define MPLEX_VER    " 1.3"
#define MPLEX_DATE   "24.09.2000"

#define SEQUENCE_HEADER 	0x000001b3
#define SEQUENCE_END		0x000001b7
#define PICTURE_START		0x00000100
#define GROUP_START		0x000001b8
#define SYNCWORD_START		0x000001
#define IFRAME                  1
#define PFRAME                  2
#define BFRAME                  3
#define DFRAME                  4

#define AUDIO_SYNCWORD		0xfff


#define PACK_START		0x000001ba
#define SYS_HEADER_START	0x000001bb
#define ISO11172_END		0x000001b9
#define PACKET_START		0x000001

#define MAX_FFFFFFFF		4294967295.0 	/* = 0xffffffff in dec.	*/

#define CLOCKS			(300 *90000)		/* MPEG-2 System Clock Hertz - we divide down by 300.0 for MPEG-1*/

/* Range of sizes of the fields following the packet length field in packet header:
	used to calculate if recieve buffers will have enough space... */


#define MPEG2_AFTER_PACKET_LENGTH_MIN    3
#define MPEG1_AFTER_PACKET_LENGTH_MIN    (0+1)

#define MPEG2_LAST_SCR_BYTE_IN_PACK  10			/* No of bytes in pack	*/
#define MPEG1_LAST_SCR_BYTE_IN_PACK   9         /* preceding, and 	*/
												/* including, the SCR	*/
#define DTS_PTS_TIMESTAMP_LENGTH 5
#define BUFFERINFO_LENGTH 2
#define MPEG2_AFTER_PACKET_LENGTH_MAX    \
	   (MPEG2_AFTER_PACKET_LENGTH_MIN+DTS_PTS_TIMESTAMP_LENGTH*2+BUFFERINFO_LENGTH)
#define MPEG1_AFTER_PACKET_LENGTH_MAX    \
		(MPEG1_AFTER_PACKET_LENGTH_MIN+DTS_PTS_TIMESTAMP_LENGTH*2+BUFFERINFO_LENGTH-1)

	/* Sector under-size below which header stuffing rather than padding packets
		or post-packet zero stuffing is used.  *Must* be less than 20 for VCD
		multiplexing to work correctly!
	 */
#define MINIMUM_PADDING_PACKET_SIZE 10

/* The following values for sys_header_length & size are only valid for */
/* System streams consisting of two basic streams. When wrapping around */
/* the system layer on a single video or a single audio stream, those   */
/* values get decreased by 3.                                           */


#define SYS_HEADER_SIZE		18		/* incl. start code and	*/
                                    /* length field, 2 streams		*/
#define MPEG2_PACK_HEADER_SIZE	14
#define MPEG1_PACK_HEADER_SIZE  12

#define PACKET_HEADER_SIZE	6

#define VIDEOCD_SECTOR_SIZE	2324	        /* VideoCD sector size */
#define SVCD_SECTOR_SIZE        2324            /* Super VideoCD sector size */
#define DVD_SECTOR_SIZE         2048            /* DVD sector size     */
#define MAX_SECTOR_SIZE         4096


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
#define MARKER_MPEG2_SCR        2		/* These don't need to be distinct! */
#define MARKER_JUST_PTS			2		/* Marker only PTS	*/
#define MARKER_PTS				3		/* Marker PTS		*/
#define MARKER_DTS				1		/* Marker DTS		*/
#define MARKER_NO_TIMESTAMPS	0x0f		/* Marker NO timestamps	*/

#define STATUS_AUDIO_END	0		/* Statusmessage A end	*/
#define STATUS_VIDEO_END	1		/* Statusmessage V end	*/
#define STATUS_AUDIO_TIME_OUT	2		/* Statusmessage A out	*/
#define STATUS_VIDEO_TIME_OUT	3		/* Statusmessage V out	*/

/*************************************************************************
    Typ- und Strukturdefinitionen
*************************************************************************/

/* TODO: Eventually this should be dealt with as a union... */
typedef struct timecode_struc	/* Time_code Struktur laut MPEG		*/
{  
  clockticks thetime;  /* The actual time... (for comparisons)*/
} Timecode_struc;	

typedef struct vaunit_struc	/* Informationen ueber Video AU's 	*/
{   unsigned int length		;
    unsigned int type		;
    Timecode_struc DTS		;
    Timecode_struc PTS		;
    int            dorder;
} Vaunit_struc;

typedef struct aaunit_struc	/* Informationen ueber Audio AU's 	*/
{   unsigned long length	;
    Timecode_struc PTS		;
} Aaunit_struc;

typedef struct video_struc	/* Informationen ueber Video Stream	*/
{   bitcount_t stream_length  ;
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
} Video_struc; 		

typedef struct audio_struc	/* Informationen ueber Audio Stream	*/
{   bitcount_t stream_length ;
    unsigned int num_syncword	;
    unsigned int num_frames [2]	;
    unsigned int size_frames[2] ;
    unsigned int layer		;
    unsigned int protection	;
    unsigned int bit_rate	;
    unsigned int frequency	;
    unsigned int mode		;
    unsigned int mode_extension ;
    unsigned int copyright      ;
    unsigned int original_copy  ;
    unsigned int emphasis	;
} Audio_struc; 	

typedef struct sector_struc	/* Ein Sektor, kann Pack, Sys Header	*/
				/* und Packet enthalten.		*/
{   unsigned char  buf [MAX_SECTOR_SIZE] ;
    unsigned int   length_of_sector  ;
    unsigned int   length_of_packet_data ;
    Timecode_struc TS                ;
} Sector_struc;

typedef struct pack_struc	/* Pack Info				*/
{   unsigned char  buf [MPEG2_PACK_HEADER_SIZE];
	int length;
    Timecode_struc SCR;
} Pack_struc;

typedef struct sys_header_struc	/* System Header Info			*/
{   unsigned char  buf [SYS_HEADER_SIZE];
	int length;
} Sys_header_struc;

typedef struct buffer_queue	/* FIFO-Queue fuer STD Buffer		*/
{   unsigned int size	;	/* als verkettete Liste implementiert	*/
    Timecode_struc DTS	;
    struct buffer_queue *next	;
} Buffer_queue;
    

typedef struct buffer_struc	/* Simuliert STD Decoder Buffer		*/
{   unsigned int max_size;	/* enthaelt Anker auf verkettete Liste	*/
    Buffer_queue *first;
} Buffer_struc;
    
    
/*************************************************************************
    Funktionsprototypen, keine Argumente, K&R Style
*************************************************************************/

int intro_and_options( int, char **);	/* Anzeigen des Introbildschirmes und	*/
										/* Ueberpruefen der Argumente			*/
void init_stream_syntax_parameters(Video_struc 	*video_info,
							    	Audio_struc 	*audio_info );	
										/* Initialisation of syntax paramters 	*/
										/* based on (checked) options 			*/
										
void check_files          ();	/* Kontrolliert ob Files vorhanden und	*/
				/* weist sie Audio/Video Pointern zu	*/
int  open_file            ();	/* File vorhanden?			*/
void get_info_video (char *video_file,	
					Video_struc *video_info,
					double *first_frame_PTS,
					unsigned int length,
					Vector *vid_info_vec);
void get_info_audio (char *audio_file,
					  Audio_struc *audio_info,
					  double first_frame_PTS,
					  unsigned int length,
					  Vector *audio_info_vec
					  );

void output_info_video    ();	/* Ausgabe Information Access Units	*/
void output_info_audio    ();	/* Ausgabe Information Access Units	*/
void marker_bit           ();	/* Checks for marker bit		*/
void empty_video_struc    ();	/* Initialisiert Struktur fuer SUN cc	*/
void empty_audio_struc    ();	/* Initialisiert Struktur fuer SUN cc	*/
void empty_vaunit_struc   ();	/* Initialisiert Struktur fuer SUN cc	*/
void empty_aaunit_struc   ();	/* Initialisiert Struktur fuer SUN cc	*/
void empty_sector_struc   ();	/* Initialisiert Struktur fuer SUN cc	*/
void empty_timecode_struc ();	/* Initialisiert Struktur fuer SUN cc	*/
void init_buffer_struc    ();	/* Initialisiert Struktur fuer SUN cc	*/

void offset_timecode      (Timecode_struc *time1,Timecode_struc *time2,Timecode_struc *offset);	/* Rechnet Offset zwischen zwei TimeC.	*/
void copy_timecode        (Timecode_struc *,Timecode_struc *);	/* setzt 2tes TimeC. dem 1ten gleich	*/
void bytepos_timecode(long long bytepos, Timecode_struc *ts);
void make_timecode        (double, Timecode_struc *);	/* rechnet aus double einen TimeC.	*/
				/* und schreibt ihn in Timecode_struc   */
void add_to_timecode      (Timecode_struc *,Timecode_struc *);	/* addiert 1tes TimeC. zum 2ten		*/ 
void buffer_dtspts_mpeg1scr_timecode (Timecode_struc *pointer,
									 unsigned char  marker,
									 unsigned char **buffer);
void buffer_mpeg2scr_timecode( Timecode_struc *pointer,
								unsigned char **buffer
							 );

int  comp_timecode        (Timecode_struc *,Timecode_struc *);	/* 1tes TimeC. <= 2tes TimeC. ?		*/
int packet_payload(  Sys_header_struc *sys_header,  Pack_struc *pack_header, int buffers, int PTSstamp, int DTSstamp );
	/* Compute available packet payload in a sector... */
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
				  );
				  
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
						);						/* erstellt einen System Header		*/
void create_pack (
				  Pack_struc	 *pack,
				  Timecode_struc   *SCR,
				  unsigned int 	 mux_rate
				);	/* erstellt einen Pack Header		*/

void output_video ( Timecode_struc *SCR,
					Timecode_struc *SCR_delay,
					FILE *istream_v,
					FILE *ostream,
					Buffer_struc *buffer,
					Vaunit_struc *video_au,
					Vector vaunit_info_vec,
					unsigned char *picture_start,
					unsigned char marker_pack,
					unsigned char include_sys_header
					);
void output_audio ( Timecode_struc *SCR,
					Timecode_struc *SCR_delay,
					FILE *istream_a,
					FILE *ostream,
					Buffer_struc *buffer,
					Aaunit_struc *audio_au,
					Vector aaunit_info_vec,
					unsigned char *audio_frame_start,
					unsigned char marker_pack,
					unsigned char include_sys_header);

void output_padding       (
					Timecode_struc *SCR,
					FILE *ostream,
					unsigned char start_of_new_pack,
					unsigned char include_sys_header,
					unsigned char pseudo_VBR
						);	/* erstellt und schreibt Padding packet	*/

void buffer_clean	  ();	/* saeubert die Bufferschlange 		*/
int  buffer_space         ();	/* Anzahl freier Bytes in Buffer	*/
void queue_buffer         ();	/* An Bufferliste anhaengen		*/

void outputstream ( char 		*video_file,
					char 		*audio_file,
					char 		*multi_file,
					Vector	   vaunit_info_vec,
					Vector     aaunit_info_vec
				 );
FILE *system_open_init( const char *filename_pat );
FILE *system_next( FILE *cur_system_strm, const char *filename_pat );

void status_info          ();	/* Statusmitteilung bei Erstellen	*/
				/* MPEG multiplex stream		*/
void status_header	  ();	/* Titelzeilen Statusblock		*/
void status_message	  ();	/* Event (end, time_out) mitteilen	*/
void status_footer	  ();	/* Endzeile				*/





/*************************************************************************
    Statische Arrays
*************************************************************************/

extern unsigned int bitrate_index [3][16];

/*************************************************************************
    Command line options and derived parameters
*************************************************************************/

#define MPEG_MPEG1  0
#define MPEG_VCD    1
#define MPEG_MPEG2  2
#define MPEG_SVCD   3
#define MPEG_DVD    4


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
extern int opt_multi_segment;

extern int verbose;
extern unsigned int which_streams;

extern int packet_overhead;
extern int rate_restriction_flag;
extern int pack_header_size;
extern int sector_size;
extern int mux_rate;
extern int dmux_rate;
extern int zero_stuffing;

extern intmax_t max_system_segment_size;
