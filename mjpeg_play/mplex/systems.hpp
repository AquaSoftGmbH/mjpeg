#ifndef __SYSTEMS_HH__
#define __SYSTEMS_HH__

#ifndef _WIN32
#include <sys/param.h>
#endif
#include "inputstrm.hpp"

#include <vector>

using std::vector;

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

struct Pack_struc	/* Pack Info				*/
{ 
    uint8_t buf[MAX_PACK_HEADER_SIZE];
    int length;
    clockticks SCR;
};

struct Sys_header_struc	/* System Header Info			*/
{   
    uint8_t buf[MAX_SYS_HEADER_SIZE];
    int length;
};


class PS_Stream
{
public:
    PS_Stream( unsigned _mpeg,
               unsigned int _sector_size,
               const char *filename_pat,
               off_t max_segment_size // 0 = No Limit
        );

    virtual bool FileLimReached( );
    virtual void NextFile();
    virtual unsigned int PacketPayload( MuxStream &strm,
                                Sys_header_struc *sys_header, 
                                Pack_struc *pack_header, 
                                int buffers, int PTSstamp, int DTSstamp );

    virtual unsigned int CreateSector (Pack_struc	 	 *pack,
                               Sys_header_struc *sys_header,
                               unsigned int     max_packet_data_size,
                               MuxStream        &strm,
                               bool 	 buffers,
                               bool      end_marker,
                               clockticks   	 PTS,
                               clockticks   	 DTS,
                               uint8_t 	 timestamps
        );
    virtual void RawWrite(uint8_t *data, unsigned int len);
    static void BufferSectorHeader( uint8_t *buf,
                             Pack_struc	 	 *pack,
                             Sys_header_struc *sys_header,
                             uint8_t *&header_end );
    static void BufferPacketHeader( uint8_t *buf,
                                    uint8_t type,
                                    unsigned int mpeg_version,
                                    bool buffers,
                                    unsigned int buffer_size,
                                    uint8_t buffer_scale,
                                    clockticks   	 PTS,
                                    clockticks   	 DTS,
                                    uint8_t 	 timestamps,
                                    unsigned int min_pes_hdr_len,
                                    uint8_t     *&size_field,
                                    uint8_t     *&header_end );
    
    static inline void 
    BufferPacketSize( uint8_t *size_field, uint8_t *packet_end )
        {
            unsigned int packet_size = packet_end-size_field-2;
            size_field[0] = static_cast<uint8_t>(packet_size>>8);
            size_field[1] = static_cast<uint8_t>(packet_size&0xff);

        }

    virtual void CreatePack ( Pack_struc	 *pack,
                      clockticks   SCR,
                      unsigned int 	 mux_rate
        );
    virtual void CreateSysHeader ( Sys_header_struc *sys_header,
                           unsigned int	 rate_bound,
                           bool	 fixed,
                           int	     CSPS,
                           bool	 audio_lock,
                           bool	 video_lock,
                           vector<MuxStream *> &streams
        );

    virtual void Close() { fclose(strm); }
    virtual int  Open( );

private:

    static void 
    BufferDtsPtsMpeg1ScrTimecode (clockticks    timecode,
                                  uint8_t  marker,
                                  uint8_t *&buffer);
    static void BufferMpeg2ScrTimecode( clockticks    timecode,
                                        uint8_t *&buffer);
    void BufferPaddingPacket( int padding,
                              uint8_t *&buffer );
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


/* 
 * Local variables:
 *  c-file-style: "stroustrup"
 *  tab-width: 4
 *  indent-tabs-mode: nil
 * End:
 */
