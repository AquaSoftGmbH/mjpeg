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
#include "bitstrm.h"
#ifdef TIMER
#include <sys/time.h>
#endif


/*************************************************************************
    Definitionen
*************************************************************************/
 
#define MPLEX_VER    " 1.1  "
#define MPLEX_DATE   "06.06.95"

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

#define CLOCKS			90000.0		/* System Clock Hertz	*/

#define AFTER_PACKET_LENGTH	15		/* No of non-data-bytes	*/
						/* following the packet	*/
						/* length field		*/
#define LAST_SCR_BYTE_IN_PACK	9		/* No of bytes in pack	*/
						/* preceding, and 	*/
						/* including, the SCR	*/

/* The following values for sys_header_length & size are only valid for */
/* System streams consisting of two basic streams. When wrapping around */
/* the system layer on a single video or a single audio stream, those   */
/* values get decreased by 3.                                           */

#define SYS_HEADER_LENGTH	12		/* length of Sys Header	*/
						/* after start code and	*/
						/* length field		*/

#define SYS_HEADER_SIZE		18		/* incl. start code and	*/
						/* length field		*/
#define PACK_HEADER_SIZE	12

#define PACKET_HEADER_SIZE	6

#define MAX_SECTOR_SIZE		4096		/* Max Sektor Groesse	*/

/* #define SECTOR_SIZE		2324	*/	/* CDROM Sektor Groesse	*/

/* #define MIN_PACKET_DATA	2273	*/	/* SECTOR_SIZE -	*/
						/* PACK_HEADER_SIZE -	*/
						/* SYS_HEADER_SIZE -	*/
						/* PACKET_HEADER_SIZE -	*/
						/* AFTER_PACKET_LENGTH	*/

/* #define MAX_PACKET_DATA	2303	*/	/* SECTOR_SIZE -	*/
						/* PACKET_HEADER_SIZE -	*/
						/* AFTER_PACKET_LENGTH	*/

/* #define PACKETS_PER_PACK	1	*/

/* #define AUDIO_BUFFER		4*1024	*/	/* Groesse Audio Buffer	*/
/* #define VIDEO_BUFFER		40*1024	*/	/* Groesse Video Buffer	*/

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
#define TIMESTAMPS_NO		0		/* Flag NO timestamps	*/
#define TIMESTAMPS_PTS		1		/* Flag PTS timestamp	*/
#define TIMESTAMPS_PTS_DTS	2		/* Flag BOTH timestamps	*/

#define MARKER_SCR		2		/* Marker SCR		*/
#define MARKER_JUST_PTS		2		/* Marker only PTS	*/
#define MARKER_PTS		3		/* Marker PTS		*/
#define MARKER_DTS		1		/* Marker DTS		*/
#define MARKER_NO_TIMESTAMPS	0x0f		/* Marker NO timestamps	*/

#define STATUS_AUDIO_END	0		/* Statusmessage A end	*/
#define STATUS_VIDEO_END	1		/* Statusmessage V end	*/
#define STATUS_AUDIO_TIME_OUT	2		/* Statusmessage A out	*/
#define STATUS_VIDEO_TIME_OUT	3		/* Statusmessage V out	*/

/*************************************************************************
    Typ- und Strukturdefinitionen
*************************************************************************/

typedef struct timecode_struc	/* Time_code Struktur laut MPEG		*/
{   unsigned long msb;		/* fuer SCR, DTS, PTS			*/
    unsigned long lsb;
} Timecode_struc;	

typedef struct vaunit_struc	/* Informationen ueber Video AU's 	*/
{   unsigned int length		;
    unsigned int type		;
    Timecode_struc DTS		;
    Timecode_struc PTS		;
} Vaunit_struc;

typedef struct aaunit_struc	/* Informationen ueber Audio AU's 	*/
{   unsigned long length	;
    Timecode_struc PTS		;
} Aaunit_struc;

typedef struct video_struc	/* Informationen ueber Video Stream	*/
{   unsigned int stream_length  ;
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
} Video_struc; 		

typedef struct audio_struc	/* Informationen ueber Audio Stream	*/
{   unsigned int stream_length  ;
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
{   unsigned char  buf [PACK_HEADER_SIZE];
    Timecode_struc SCR;
} Pack_struc;

typedef struct sys_header_struc	/* System Header Info			*/
{   unsigned char  buf [SYS_HEADER_SIZE];
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

void intro                ();	/* Anzeigen des Introbildschirmes und	*/
				/* Ueberpruefen der Argumente		*/
void check_files          ();	/* Kontrolliert ob Files vorhanden und	*/
				/* weist sie Audio/Video Pointern zu	*/
int  open_file            ();	/* File vorhanden?			*/
void get_info_video       ();	/* Info Access Units Video Stream	*/
void output_info_video    ();	/* Ausgabe Information Access Units	*/
void get_info_audio       ();	/* Info Access Units Audio Stream	*/
void output_info_audio    ();	/* Ausgabe Information Access Units	*/
void marker_bit           ();	/* Checks for marker bit		*/
void empty_video_struc    ();	/* Initialisiert Struktur fuer SUN cc	*/
void empty_audio_struc    ();	/* Initialisiert Struktur fuer SUN cc	*/
void empty_vaunit_struc   ();	/* Initialisiert Struktur fuer SUN cc	*/
void empty_aaunit_struc   ();	/* Initialisiert Struktur fuer SUN cc	*/
void empty_sector_struc   ();	/* Initialisiert Struktur fuer SUN cc	*/
void empty_timecode_struc ();	/* Initialisiert Struktur fuer SUN cc	*/
void init_buffer_struc    ();	/* Initialisiert Struktur fuer SUN cc	*/

void offset_timecode      ();	/* Rechnet Offset zwischen zwei TimeC.	*/
void copy_timecode        ();	/* setzt 2tes TimeC. dem 1ten gleich	*/
void make_timecode        ();	/* rechnet aus double einen TimeC.	*/
				/* und schreibt ihn in Timecode_struc   */
void add_to_timecode      ();	/* addiert 1tes TimeC. zum 2ten		*/ 
void buffer_timecode      ();	/* schreibt Timecode in Bitstreamformat */
int  comp_timecode        ();	/* 1tes TimeC. <= 2tes TimeC. ?		*/

void create_sector	  ();	/* erstellt einen Sector		*/
void create_sys_header	  ();	/* erstellt einen System Header		*/
void create_pack	  ();	/* erstellt einen Pack Header		*/

void output_video         ();	/* erstellt und schreibt Video pack aus	*/
void output_audio         ();   /* erstellt und schreibt Audio pack aus */
void output_padding       ();	/* erstellt und schreibt Padding pack	*/

void next_video_access_unit ();	/* holt naechste gueltige AU Info her	*/
void next_audio_access_unit ();	/* holt naechste gueltige AU Info her	*/

void buffer_clean	  ();	/* saeubert die Bufferschlange 		*/
int  buffer_space         ();	/* Anzahl freier Bytes in Buffer	*/
void queue_buffer         ();	/* An Bufferliste anhaengen		*/

void outputstream         ();	/* Hauptschleife Multiplexroutinen	*/

void status_info          ();	/* Statusmitteilung bei Erstellen	*/
				/* MPEG multiplex stream		*/
void status_header	  ();	/* Titelzeilen Statusblock		*/
void status_message	  ();	/* Event (end, time_out) mitteilen	*/
void status_footer	  ();	/* Endzeile				*/

void ask_continue	  ();	/* Soll weiter gearbeitet werden ?	*/
int ask_verbose ();	/* Soll verbose gearbeitet werden ?	*/

extern int verbose;

/*************************************************************************
    Statische Arrays
*************************************************************************/

static double picture_rates [9] = { 0., 24000./1001., 24., 25., 
	30000./1001., 30., 50., 60000./1001., 60. };

static double ratio [16] = { 0., 1., 0.6735, 0.7031, 0.7615, 0.8055,
	0.8437, 0.8935, 0.9157, 0.9815, 1.0255, 1.0695, 1.0950, 1.1575,
	1.2015, 0.};

static unsigned int bitrate_index [3][16] =
    {{0,32,64,96,128,160,192,224,256,288,320,352,384,416,448,0},
     {0,32,48,56,64,80,96,112,128,160,192,224,256,320,384,0},
     {0,32,40,48,56,64,80,96,112,128,160,192,224,256,320,0}};

static double frequency [4] = {44.1, 48, 32, 0};
static unsigned int slots [4] = {12, 144, 0, 0};
static unsigned int samples [4] = {384, 1152, 0, 0};

static char mode [4][15] =
    { "stereo", "joint stereo", "dual channel", "single channel" };
static char copyright [2][20] =
    { "no copyright","copyright protected" };
static char original [2][10] =
    { "copy","original" };
static char emphasis [4][20] =
    { "none", "50/15 microseconds", "reserved", "CCITT J.17" };

/*************************************************************************
    Command line options
*************************************************************************/

extern int opt_quiet_mode;
extern int opt_interactive_mode;
extern int opt_buffer_size;
extern int opt_data_rate;
extern int opt_video_offset;
extern int opt_audio_offset;
extern int opt_sector_size;
extern int opt_VBR;

