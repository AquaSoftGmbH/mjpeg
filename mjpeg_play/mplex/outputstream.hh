
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
	OutputStream()
		{
			underrun_ignore = 0;
			underruns = 0;
		}
	void OutputMultiplex ( VideoStream *vstrm,
						   AudioStream *astrm,
						   char *multi_file);


	void InitSyntaxParameters();
	void ByteposTimecode( bitcount_t bytepos, clockticks &ts );

	unsigned int PacketPayload(	MuxStream &strm,
								bool buffers, bool PTSstamp, bool DTSstamp );
	unsigned int WritePacket( unsigned int     max_packet_data_size,
							  MuxStream        &strm,
							  bool 	 buffers,
							  clockticks   	 PTS,
							  clockticks   	 DTS,
							  uint8_t 	 timestamps
		);

	/* Syntax control parameters, public becaus they're partly referenced
	   by the input stream objects.
	 */

	bool always_sys_header_in_pack;
	bool dtspts_for_all_vau;
	bool sys_header_in_pack1;
	bool buffers_in_video;
	bool always_buffers_in_video;	
	bool buffers_in_audio;
	bool always_buffers_in_audio;
	bool sector_align_iframeAUs;
	bool split_at_seq_end;
	bool seg_starts_with_video;
	unsigned int audio_buffer_size;
	unsigned int video_buffer_size;


	/* Sequence run-out control */
	bool running_out;
	clockticks runout_PTS;


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
	int 		mux_rate;	/* MPEG mux rate (50 byte/sec units      */
	unsigned int packets_per_pack;
	
private:	
	
    /* Stream packet component buffers */
	
	Sys_header_struc 	sys_header;
	Pack_struc          pack_header;
	Pack_struc *pack_header_ptr;
	Sys_header_struc *sys_header_ptr;

	/* Under-run error messages */
	unsigned int underruns;
	unsigned int underrun_ignore;

	/* Output data stream... */
	PS_Stream *psstrm;
	bitcount_t bytes_output;
	clockticks current_SCR;
	clockticks audio_delay;
	clockticks video_delay;
	bool vbr;
	/* Source data streams */
	/* Note: 1st video stream is regarded as the "master" stream for
	   the purpose of splitting sequences etc...
	*/
	vector<ElementaryStream *> estreams; // Complete set
	vector<ElementaryStream *> vstreams; // Video streams in estreams
	vector<ElementaryStream *> astreams; // Audio streams in estreams
	
	PaddingStream pstrm;
	VCDAPadStream vcdapstrm;

private:
	void Init( char *multi_file );
	

	void NextPosAndSCR();
	void SetPosAndSCR( bitcount_t bytepos );

	void OutputPrefix( );

	void OutputSuffix();
	void OutputPadding ( clockticks SCR,
						 bool start_of_new_pack,
						 bool include_sys_header,
						 bool pseudo_VBR,
						 bool vcd_audio_pad
		);

	void MuxStatus( log_level_t level );
	
};


#endif //__OUTPUTSTREAM_H__
