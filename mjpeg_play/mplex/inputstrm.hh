
#ifndef __INPUTSTRM_H__
#define __INPUTSTRM_H__

#include <config.h>
#include <inttypes.h>
#include <vector>
#include <sys/stat.h>
#include <vector>

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
		old_prozent(0),
		prozent(0)
		{}

	void Init( const char *file_name )
		{
			struct stat stb;
			/* We actually maintain *two* file-handles one is used for
			   scanning ahead to pick-up access unit information the
			   other for the reading done to shuffle data into the
			   multiplexed output file */
			
			rawstrm = fopen( file_name, "rb" );
			if( rawstrm == NULL )
				mjpeg_error_exit1( "Cannot open for scan and read: %s\n", file_name );
			fstat(fileno(rawstrm), &stb);
			file_length = stb.st_size;
			
			bs.open( const_cast<char *>(file_name) );
		}


	//void close();
	//T *next() = 0;
	//T *lookahead( unsigned int lookahead ) = 0;

	FILE *rawstrm;				// Elementary input stream
	                            // Eventually to be encapsulated
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
    unsigned int prozent;
    unsigned int old_prozent;

};



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
	void SetMuxParams( unsigned int buf_size )
		{
			bufmodel.init( buf_size );
			buffer_size = buf_size;
			init = true;
		}
	unsigned int buffer_size_code()
		{
			assert(init);
			if( buffer_scale == 1 )
				return buffer_size / 1024;
			else if( buffer_scale == 0 )
				return buffer_size / 128;
			else
				assert(false);
		}
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
class BufInputStream : public InputStream
{
protected:
	virtual void fillAUbuffer(unsigned int frames_to_buffer) = 0;

	AUStream<T> aunits;
    static const int FRAME_CHUNK = frame_chunk;

public:  // TODO should go protected once encapsulation complete
	T au;

};


class VideoStream : public BufInputStream<VAunit, 256>,
					public MuxStream
{
public:
	VideoStream(const int stream_num) :
		MuxStream(VIDEO_STR_0+stream_num,1),
		num_sequence(0),
		num_seq_end(0),
		num_pictures(0),
		num_groups(0)
		{
			for( int i =0; i<4; ++i )
				num_frames[i] = avg_frames[i] = 0;
		}
	void Init(const char *input_file);
	
	void close();
	VAunit *next();
	VAunit *lookahead( unsigned int lookahead );


private:
	void output_seqhdr_info();
	virtual void fillAUbuffer(unsigned int frames_to_buffer);

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

class AudioStream : public BufInputStream<AAunit, 128>,
					public MuxStream
{
public:   
	AudioStream(const int stream_num) : 
		MuxStream( AUDIO_STR_0 + stream_num, 0),
		num_syncword(0)
		{
			for( int i = 0; i <2 ; ++i )
				num_frames[i] = size_frames[i] = 0;
		}

	void Init(char *audio_file);

	void close();
	AAunit *next();
	AAunit *lookahead( unsigned int lookahead );

	
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

private:
	void output_audio_info();
	virtual void fillAUbuffer(unsigned int frames_to_buffer);

	/* State variables for scanning source bit-stream */
    unsigned int framesize;
	unsigned int padding_bit;
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
};


#endif // __INPUTSTRM_H__
