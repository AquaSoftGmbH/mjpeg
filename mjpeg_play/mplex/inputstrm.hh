
#ifndef __INPUTSTRM_H__
#define __INPUTSTRM_H__

#include <config.h>
#include <stdio.h>
#include <inttypes.h>
#include <vector>
#include <sys/stat.h>
#include <vector>

#include "bits.hh"
#include "aunit.hh"
#include "vector.hh"
#include "mpegconsts.hh"

#include "mjpeg_logging.h"

class BufferQueue
{
public:
	unsigned int size	;	/* als verkettete Liste implementiert	*/
    clockticks DTS	;
    BufferQueue *next	;
};
    

class BufferModel
{
public:
void init( unsigned int size);

void cleaned(  clockticks timenow);
void flushed( );
unsigned int space();
void queued( unsigned int bytes,
			 clockticks removaltime);

private:
	unsigned int max_size;
    BufferQueue *first;
};




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
// TODO: This is really very unnecessary templating.  If we 
// encapsulate the AU buffering code so it doesn't generate
// lots of memory requests even though it only
// works with points this could be handled *far* more elegantly
// through inheritance.  
//

class MuxStream
{
public:
	MuxStream(const int strm_id, const int buf_scale) : 
		stream_id(strm_id), 
		new_au_next_sec(true),
		buffer_scale( buf_scale ),
		init(false)
		{

		}

	void SetMuxParams( unsigned int buf_size );
	unsigned int BufferSizeCode();

	virtual unsigned int ReadStrm(uint8_t *dst, unsigned int to_read) = 0;

public:  // TODO should go protected once encapsulation complete


	BufferModel bufmodel;
	int        stream_id;
	bool new_au_next_sec;
	int        buffer_scale;
	unsigned int 	buffer_size;
	unsigned int 	max_packet_data;
	unsigned int	min_packet_data;
	bool       init;
};


template <class T, const int frame_chunk>
class ElementaryStream : public InputStream,
						 public MuxStream
{
protected:
	virtual void FillAUbuffer(unsigned int frames_to_buffer) = 0;

	AUStream<T> aunits;
    static const int FRAME_CHUNK = frame_chunk;
public:
	ElementaryStream( const int stream_id,
					  const int buf_scale ) : 
		MuxStream( stream_id, buf_scale )
		{}

	bool NextAU()
		{
			T *p_au = next();
			if( p_au != NULL )
			{
				au = *p_au;
				au_unsent = p_au->length;
				return true;
			}
			else
			{
				au_unsent = 0;
				return false;
			}
		}


	int NextAUType()
		{
			T *p_au = Lookahead(1);
			if( p_au != NULL )
				return p_au->type;
			else
				return NOFRAME;
		}

	bool SeqHdrNext()
		{
			T *p_au = Lookahead(1);
			return p_au != NULL && p_au->seq_header;
		}

	T *Lookahead( unsigned int i )
		{
			assert( i < FRAME_CHUNK-1 );
			return aunits.lookahead(i);
		}

	unsigned int BytesToMuxAUEnd(unsigned int sector_transport_size)
		{
			return (au_unsent/min_packet_data)*sector_transport_size +
				(au_unsent%min_packet_data)+(sector_transport_size-min_packet_data);
		}

	//
	//  Read the (parsed and spliced) stream data from the stream
	//  buffer.
	//
	unsigned int ReadStrm(uint8_t *dst, unsigned int to_read)
		{
			return bs.read_buffered_bytes( dst, to_read );
		}


public:  // TODO should go protected once encapsulation complete
	     // N.b. currently length=0 is used to indicate an ended
	     // stream.
	     // au itself should simply disappear
	T au;
	unsigned int au_unsent;
private:
	T *next()
		{
			if( !eoscan && aunits.curpos()+FRAME_CHUNK > last_buffered_AU  )
			{
				if( aunits.curpos() > FRAME_CHUNK )
				{
					T *p_au = aunits.lookahead(0);
					aunits.flush(FRAME_CHUNK);
				}
				FillAUbuffer(FRAME_CHUNK);
			}
			
			return aunits.next();
		}
	
};


class VideoStream : public ElementaryStream<VAunit, 4>
{
public:
	VideoStream(const int stream_num) :
		ElementaryStream<VAunit, 4>(VIDEO_STR_0+stream_num,1),
		num_sequence(0),
		num_seq_end(0),
		num_pictures(0),
		num_groups(0)
		{
			for( int i =0; i<4; ++i )
				num_frames[i] = avg_frames[i] = 0;
		}

	void Init(const char *input_file);
	void Close();

	unsigned int NominalBitRate() { return bit_rate * 50; }

private:
	void OutputSeqhdrInfo();
	virtual void FillAUbuffer(unsigned int frames_to_buffer);

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

	
private:

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

	// State variables for multiplexed sub-stream...
public:							// TODO make private once encapsulation comple
	int next_sec_AU_type;
}; 		

class AudioStream : public ElementaryStream<AAunit, 4>
{
public:   
	AudioStream(const int stream_num) : 
		ElementaryStream<AAunit, 4>( AUDIO_STR_0 + stream_num, 0),
		num_syncword(0)
		{
			for( int i = 0; i <2 ; ++i )
				num_frames[i] = size_frames[i] = 0;
		}

	void Init(char *audio_file);

	void Close();

	unsigned int NominalBitRate();

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

	/* State variables for scanning source bit-stream */
    unsigned int framesize;
    unsigned int skip;
    unsigned int samples_per_second;
    AAunit access_unit;

}; 	

class PaddingStream : public MuxStream
{
public:
	PaddingStream() :
		MuxStream( PADDING_STR, 0 )
		{
			init = true;
		}

	unsigned int ReadStrm(uint8_t *dst, unsigned int to_read);
};

class EndMarkerStream : public MuxStream
{
public:
	EndMarkerStream() :
		MuxStream( PADDING_STR, 0 )
		{
			init = true;
		}

	unsigned int ReadStrm(uint8_t *dst, unsigned int to_read);
};


#endif // __INPUTSTRM_H__
