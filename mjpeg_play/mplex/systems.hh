#ifndef __SYSTEMS_HH__
#define __SYSTEMS_HH__

#include <sys/param.h>
#include "inputstrm.hh"

/* Buffer size parameters */

#define MAX_SECTOR_SIZE         16384
#define MAX_PACK_HEADER_SIZE	255
#define MAX_SYS_HEADER_SIZE     255


typedef struct sector_struc	/* Ein Sektor, kann Pack, Sys Header	*/
				/* und Packet enthalten.		*/
{   unsigned char  buf [MAX_SECTOR_SIZE] ;
    unsigned int   length_of_packet_data ;
    //clockticks TS                ;
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


class PS_Stream
{
public:
	PS_Stream( unsigned _mpeg,
			   unsigned int _sector_size
		)
		: mpeg_version( _mpeg),
		  sector_size( _sector_size ),
		  segment_num( 1 ),
		  max_segment_size( 2040 * 1024 * 1024 )
		{
			sector_buf = new uint8_t[sector_size];
		}

	void Init( const char *filename_pat,
			   off_t max_segment_size // 0 = No Limit
		);
	bool FileLimReached( );
	void NextFile();
	unsigned int PacketPayload( MuxStream &strm,
								Sys_header_struc *sys_header, 
								Pack_struc *pack_header, 
								int buffers, int PTSstamp, int DTSstamp );

	unsigned int CreateSector (Pack_struc	 	 *pack,
							   Sys_header_struc *sys_header,
							   unsigned int     max_packet_data_size,
							   MuxStream        &strm,
							   bool 	 buffers,
							   bool      end_marker,
							   clockticks   	 PTS,
							   clockticks   	 DTS,
							   uint8_t 	 timestamps
		);

	void CreatePack ( Pack_struc	 *pack,
					  clockticks   SCR,
					  unsigned int 	 mux_rate
		);
	void CreateSysHeader (	Sys_header_struc *sys_header,
							unsigned int	 rate_bound,
							bool	 fixed,
							int	     CSPS,
							bool	 audio_lock,
							bool	 video_lock,
							vector<ElementaryStream *> &streams
		);

	void Close() { fclose(strm); }

private:
	void RawWrite(uint8_t *data, unsigned int len);

	void BufferDtsPtsMpeg1ScrTimecode (clockticks    timecode,
								  uint8_t  marker,
								  uint8_t **buffer);
	void BufferMpeg2ScrTimecode( clockticks    timecode,

								 uint8_t **buffer);
	
private:
	unsigned int mpeg_version;
	unsigned int sector_size;
	int segment_num;
	off_t max_segment_size;
	FILE *strm;
	char filename_pat[MAXPATHLEN];
	char cur_filename[MAXPATHLEN];
	uint8_t *sector_buf;
};
#endif // __SYSTEMS_HH__
