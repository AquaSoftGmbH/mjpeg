
/*
 *  inptstrm.hh:  Input stream classes for MPEG multiplexing
 *  TODO: Split into the base classes and the different types of
 *  actual input stream.
 *
 *  Copyright (C) 2001 Andrew Stevens <andrew.stevens@philips.com>
 *
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU General Public License
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef __INPUTSTRM_H__
#define __INPUTSTRM_H__

#include <config.h>
#include <stdio.h>
#include <vector>
#include <sys/stat.h>

#include "mjpeg_types.h"
#include "mpegconsts.hh"
#include "mjpeg_logging.h"

#include "bits.hh"
#include "aunit.hh"
#include "vector.hh"
#include "buffer.hh"
 //
 // TODO: Put into own source file...
 //


class InputStream
{
public:
	InputStream() :
		eoscan(false),
		stream_length(0),
		last_buffered_AU(0),
		decoding_order(0),
		old_frames(0)
		{}

	void Init( const char *file_name )
		{
			bs.open( const_cast<char *>(file_name) );
		}

    bitcount_t stream_length;
protected:
	off_t      file_length;
    IBitStream bs;
	bool eoscan;
	
	unsigned int last_buffered_AU;		// decode seq num of last buffered frame + 1
   	bitcount_t AU_start;
    uint32_t  syncword;
    bitcount_t prev_offset;
    unsigned int decoding_order;
    unsigned int old_frames;

};

//
// Abstract forward reference...
//

class OutputStream;

class MuxStream
{
public:
	MuxStream();

    void Init( const int strm_id, 
			   const unsigned int _buf_scale,
			   const unsigned int buf_size,
			   const unsigned int _zero_stuffing,
			   const bool bufs_in_first, 
			   const bool always_bufs
		  );

	unsigned int BufferSizeCode();
	inline unsigned int BufferSize() { return buffer_size; }
	inline unsigned int BufferScale() { return buffer_scale; }
	
	inline void SetMaxPacketData( unsigned int max )
		{
			max_packet_data = max;
		}
	inline void SetMinPacketData( unsigned int min )
		{
			min_packet_data = min;
		}
	inline unsigned int MaxPacketData() { return max_packet_data; }
	inline unsigned int MinPacketData() { return min_packet_data; }

	virtual unsigned int ReadStrm(uint8_t *dst, unsigned int to_read) = 0;

public:  // TODO should go protected once encapsulation complete


	BufferModel bufmodel;
	int        stream_id;
	unsigned int 	max_packet_data;
	unsigned int	min_packet_data;
	unsigned int    zero_stuffing;
	unsigned int    nsec;
	unsigned int    buffer_scale;
	unsigned int 	buffer_size;
	bool buffers_in_header;
	bool always_buffers_in_header;
	bool new_au_next_sec;
	bool init;
};


class ElementaryStream : public InputStream,
						 public MuxStream
{
protected:
	virtual void FillAUbuffer(unsigned int frames_to_buffer) = 0;
	virtual void InitAUbuffer() = 0;
	AUStream aunits;
    static const int FRAME_CHUNK = 4;
public:
	enum stream_kind { audio, video };

	ElementaryStream( OutputStream &into, 
					  stream_kind kind
					  );
	virtual void Close() = 0;

	bool NextAU();
	Aunit *Lookahead();
	unsigned int BytesToMuxAUEnd(unsigned int sector_transport_size);
	bool MuxCompleted();
	virtual bool MuxPossible();
	void Muxed( unsigned int bytes_muxed );
	void DemuxedTo( clockticks SCR );
	clockticks RequiredDTS();
	void SetTSOffset( clockticks baseTS );
	void AllDemuxed();
	inline stream_kind Kind() { return kind; }
    inline int BufferMin() { return buffer_min; }
    inline int BufferMax() { return buffer_max; }
    void UpdateBufferMinMax();

	void SetSyncOffset( clockticks timestamp_delay );

 
	inline bool BuffersInHeader() { return buffers_in_header; }
	virtual unsigned int NominalBitRate() = 0;
	virtual bool RunOutComplete() = 0;
	virtual void OutputSector() = 0;
	//
	//  Read the (parsed and spliced) stream data from the stream
	//  buffer.
	//
	unsigned int ReadStrm(uint8_t *dst, unsigned int to_read);


public:  // TODO should go protected once encapsulation complete
	     // N.b. currently length=0 is used to indicate an ended
	     // stream.
	     // au itself should simply disappear
	Aunit *au;
	clockticks timestamp_delay;
protected:
	unsigned int au_unsent;
	Aunit *next();
	OutputStream &muxinto;
	stream_kind kind;
    int buffer_min;
    int buffer_max;
									
};


class VideoStream : public ElementaryStream
{
public:
	VideoStream(OutputStream &into);
	void Init( const int stream_num,
			   const char *input_file);
	void Close();

	inline int AUType()
		{
			return au->type;
		}

	inline bool EndSeq()
		{
			return au->end_seq;
		}

	inline int NextAUType()
		{
			VAunit *p_au = Lookahead();
			if( p_au != NULL )
				return p_au->type;
			else
				return NOFRAME;
		}

	inline bool SeqHdrNext()
		{
			VAunit *p_au = Lookahead();
			return  p_au != NULL && p_au->seq_header;
		}

	virtual unsigned int NominalBitRate() 
		{ 
			return bit_rate * 50;
		}
	bool RunOutComplete();

	void OutputSector();
protected:
	void OutputSeqhdrInfo();
	virtual void FillAUbuffer(unsigned int frames_to_buffer);
	virtual void InitAUbuffer();
	virtual void NextDTSPTS( clockticks &DTS, clockticks &PTS );
	void ScanFirstSeqHeader();

public:	
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

	
	bool dtspts_for_all_au;

protected:

	/* State variables for scanning source bit-stream */
    VAunit access_unit;
	unsigned int fields_presented;
    unsigned int group_order;
    unsigned int group_start_pic;
	unsigned int group_start_field;
    unsigned long temporal_reference;
    unsigned short pict_rate;
	int pulldown_32;
	int repeat_first_field;
	int film_rate;
    double frame_rate;
	unsigned int max_bits_persec;
	int AU_pict_data;
	int AU_hdr;
	clockticks max_PTS;
}; 	

class AudioStream : public ElementaryStream
{
public:   
  AudioStream(OutputStream &into );
  void Init(const int stream_num, const char *audio_file);
  void OutputSector();
  
  void Close();

  virtual unsigned int NominalBitRate();
  bool RunOutComplete();

  unsigned int num_syncword	;
  unsigned int num_frames [2]	;
  unsigned int size_frames[2] ;
	unsigned int version_id ;
    unsigned int layer		;
    unsigned int protection	;
    unsigned int bit_rate_code;
    unsigned int frequency	;
    unsigned int mode		;
    unsigned int mode_extension ;
    unsigned int copyright      ;
    unsigned int original_copy  ;
    unsigned int emphasis	;

private:
	void OutputHdrInfo();
	unsigned int SizeFrame( int bit_rate, int padding_bit );
	virtual void FillAUbuffer(unsigned int frames_to_buffer);
	virtual void InitAUbuffer();

	/* State variables for scanning source bit-stream */
    unsigned int framesize;
    unsigned int skip;
    unsigned int samples_per_second;
    AAunit access_unit;
}; 	

class PaddingStream : public MuxStream
{
public:
	PaddingStream()
		{
			MuxStream::Init( PADDING_STR, 0, 0,  0, false,false );
		}

	unsigned int ReadStrm(uint8_t *dst, unsigned int to_read);
};

class VCDAPadStream : public MuxStream
{
public:
	VCDAPadStream()
		{
			Init( PADDING_STR, 0, 0, 20, false, false );

		}

	unsigned int ReadStrm(uint8_t *dst, unsigned int to_read);
};



#endif // __INPUTSTRM_H__


/* 
 * Local variables:
 *  c-file-style: "stroustrup"
 *  tab-width: 4
 *  indent-tabs-mode: nil
 * End:
 */
