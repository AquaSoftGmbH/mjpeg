
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

	/* Syntax control parameters */

	bool always_sys_header_in_pack;
	bool dtspts_for_all_vau;
	bool sys_header_in_pack1;
	bool buffers_in_video;
	bool always_buffers_in_video;	
	bool buffers_in_audio;
	bool always_buffers_in_audio;
	bool trailing_pad_pack;		/* Stick a padding packet at the end	*/

/* In some situations the system/PES packets are embedded with
   external transport data which has to be taken into account for SCR
   calculations to be correct.  E.g. VCD streams. Where each 2324 byte
   system packet is embedded in a 2352 byte CD sector and the actual
   MPEG data is preceded by 30 empty sectors.
*/

	unsigned int	sector_transport_size;
	unsigned int    transport_prefix_sectors; 
	unsigned int 	audio_packet_data_limit; /* Needed for VCD which wastes 20 bytes */
	
	unsigned int 	sector_size;
	bool 			zero_stuffing;	/* Pad short sectors with 0's not padding packets */
	
	int 		dmux_rate;	/* Actual data mux-rate for calculations always a multiple of 50  */
int 		mux_rate;	/* TODO: remove MPEG mux rate (50 byte/sec units      */
	unsigned int packets_per_pack;
	
	
    /* Stream packet component buffers */
	
	Sys_header_struc 	sys_header;
	Sector_struc 		cur_sector;
	
	/* Output stream... */
	unsigned long long bytes_output;
	PS_Stream *psstrm;
	

private:
	void Init( VideoStream 	&vstrm,
			   AudioStream 	&astrm,
			   char *multi_file );
	
	void ByteposTimecode( bitcount_t bytepos, clockticks &ts );
	void OutputStream::OutputPrefix( clockticks &current_SCR, 
									 VideoStream &vstrm,
									 AudioStream &astrm);
	void OutputSuffix(clockticks &SCR, 
					  unsigned long long  *bytes_output);

	void NextVideoAU( unsigned int bytes_muxed,
					  clockticks SCR_delay,
					  VideoStream  &vstrm );
	void OutputVideo ( clockticks SCR,
					   clockticks SCR_delay,
					   VideoStream &vstrm,
					   bool marker_pack,
					   bool include_sys_header
		);
	void NextAudioAU( unsigned int bytes_muxed,
					  clockticks SCR_delay,
					  AudioStream &astrm
		);
	
	void OutputAudio ( clockticks SCR,
					   clockticks SCR_delay,
					   AudioStream &astrm,
					   bool marker_pack,
					   bool include_sys_header,
					   bool end_of_segment);
	

	void OutputPadding ( clockticks SCR,
						 bool start_of_new_pack,
						 bool include_sys_header,
						 bool pseudo_VBR,
						 int packet_data_limit
		);
	
};

#endif //__OUTPUTSTREAM_H__
