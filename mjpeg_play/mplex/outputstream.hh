
#ifndef __OUTPUTSTREAM_H__
#define __OUTPUTSTREAM_H__

#include <config.h>
#include <stdio.h>
#include <inttypes.h>

#include "inputstrm.hh"
#include "systems.hh"


class OutputStream
{
public:
	void OutputMultiplex ( VideoStream &vstrm,
						   AudioStream &astrm,
						   char *multi_file);


	void InitSyntaxParameters();

	unsigned int PacketPayload(	MuxStream &strm,
								bool buffers, bool PTSstamp, bool DTSstamp );
	unsigned int WritePacket( unsigned int     max_packet_data_size,
							  MuxStream        &strm,
							  bool 	 buffers,
							  clockticks   	 PTS,
							  clockticks   	 DTS,
							  uint8_t 	 timestamps
		);

	/* Syntax control parameters */

	bool always_sys_header_in_pack;
	bool dtspts_for_all_vau;
	bool sys_header_in_pack1;
	bool buffers_in_video;
	bool always_buffers_in_video;	
	bool buffers_in_audio;
	bool always_buffers_in_audio;
	bool sector_align_iframeAUs;
	bool segment_runout;
/* In some situations the system/PES packets are embedded with
   external transport data which has to be taken into account for SCR
   calculations to be correct.  E.g. VCD streams. Where each 2324 byte
   system packet is embedded in a 2352 byte CD sector and the actual
   MPEG data is preceded by 30 empty sectors.
*/

	unsigned int	sector_transport_size;
	unsigned int    transport_prefix_sectors; 
	unsigned int 	sector_size;
	unsigned int	vcd_zero_stuffing;	/* VCD audio sectors have 20 0 bytes :-( */
	
	int 		dmux_rate;	/* Actual data mux-rate for calculations always a multiple of 50  */
	int 		mux_rate;	/* TODO: remove MPEG mux rate (50 byte/sec units      */
	unsigned int packets_per_pack;
	
	
    /* Stream packet component buffers */
	
	Sys_header_struc 	sys_header;
	Pack_struc          pack_header;
	Pack_struc *pack_header_ptr;
	Sys_header_struc *sys_header_ptr;

	/* Output stream... */
	bitcount_t bytes_output;
	clockticks current_SCR;
	clockticks audio_delay;
	clockticks video_delay;
	PS_Stream *psstrm;


private:
	void Init( VideoStream 	&vstrm,
			   AudioStream 	&astrm,
			   char *multi_file );
	
	void ByteposTimecode( bitcount_t bytepos, clockticks &ts );

	void NextPosAndSCR();
	void SetPosAndSCR( bitcount_t bytepos );

	void OutputPrefix( VideoStream &vstrm,
						AudioStream &astrm);

	void OutputSuffix();


	void OutputVideo ( VideoStream &vstrm,
					   bool marker_pack,
					   bool include_sys_header
		);
	void NextAudioAU( unsigned int bytes_muxed,
					  AudioStream &astrm
		);
	
	void OutputAudio ( AudioStream &astrm,
					   bool marker_pack,
					   bool include_sys_header,
					   bool end_of_segment);
	

	void OutputPadding ( clockticks SCR,
						 bool start_of_new_pack,
						 bool include_sys_header,
						 bool pseudo_VBR,
						 bool vcd_audio_pad
		);

	
};

	void NextVideoAU( unsigned int bytes_muxed,
					  VideoStream  &vstrm );

#endif //__OUTPUTSTREAM_H__
