/*
 *  audiostrm_in.c:  Audio strem class members handling scanning and buffering
 *                   raw input stream.
 *
 *  Copyright (C) 2000,2001 Brent Byeler
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

#include <config.h>
#include <math.h>
#include <stdlib.h>

#include "audiostrm.hh"
#include "interact.hh"
#include "outputstream.hh"



#define AC3_SYNCWORD            0x0b77
#define AC3_PACKET_SAMPLES      1536

static const unsigned int ac3_bitrate_index[32] =
{ 32,40,48,56,64,80,96,112,128,160,192,
  224,256,320,384,448,512,576,640,
  0,0,0,0,0,0,0,0,0,0,0,0,0
};

static const unsigned int ac3_frame_size[3][32] =
{
    { 64,80,96,112,128,160,192,224,256,320,384,
      448,512,640,768,896,1024, 1152,1280,
      0,0,0,0,0,0,0,0,0,0,0,0,0
    },
    { 69,87,104,121,139,174,208,243,278,348,417,
      487,557,696,835,975,1114, 1253,1393,
          0,0,0,0,0,0,0,0,0,0,0,0,0
    },
    { 96,120,144,168,192,240,288,336,384,480,576,
      672,768,960,1152,1344, 1536,1728,1920,
      0,0,0,0,0,0,0,0,0,0,0,0,0
    }
}; 

static const unsigned int ac3_frequency[4] = 
{ 48000, 44100, 32000, 0};


AC3Stream::AC3Stream(OutputStream &into) : 
	AudioStream( into )
{
}

bool AC3Stream::Probe(IBitStream &bs )
{
    return bs.getbits(16) == AC3_SYNCWORD;
}


/*************************************************************************
 *
 * Reads initial stream parameters and displays feedback banner to users
 *
 *************************************************************************/


void AC3Stream::Init ( const int stream_num, 
					   const char *audio_file)

{
    unsigned int i;
    unsigned int framesize_code;

	mjpeg_debug( "SETTING zero stuff to %d\n", muxinto.vcd_zero_stuffing );
	mjpeg_debug( "SETTING audio buffer to %d\n", muxinto.audio_buffer_size );

	MuxStream::Init( PRIVATE_STR_1, // TODO Currently hard-wired....
					 1,  // Buffer scale
					 muxinto.audio_buffer_size,
					 muxinto.vcd_zero_stuffing,
					 muxinto.buffers_in_audio,
					 muxinto.always_buffers_in_audio
		);
    mjpeg_info ("Scanning Audio stream for access units information. \n");

	InitAUbuffer();
	InputStream::Init( audio_file, 512*1024 );
	
    if (bs.getbits(16)==AC3_SYNCWORD)
    {
		num_syncword++;
        bs.getbits(16);         // CRC field
        frequency = bs.getbits(2);  // Sample rate code
        framesize_code = bs.getbits(6); // Frame size code
        framesize = ac3_frame_size[frequency][framesize_code>>1];
        framesize = 
            (framesize_code&1) && frequency == 1 ?
            (framesize + 1) << 1:
            (framesize <<1);
            

		size_frames[0] = framesize;
		size_frames[1] = framesize;
		num_frames[0]++;
		access_unit.length = framesize;
        bit_rate = ac3_bitrate_index[framesize_code>>1];
		samples_per_second = ac3_frequency[frequency];

		/* Presentation time-stamping  */
		access_unit.PTS = static_cast<clockticks>(decoding_order) * 
			static_cast<clockticks>(AC3_PACKET_SAMPLES) * 
			static_cast<clockticks>(CLOCKS)	/ samples_per_second;
		access_unit.DTS = access_unit.PTS;
		access_unit.dorder = decoding_order;
		++decoding_order;
		aunits.append( access_unit );

    } else
    {
		mjpeg_error ( "Invalid AC3 Audio stream header.\n");
		exit (1);
    }


	OutputHdrInfo();
}

unsigned int AC3Stream::NominalBitRate()
{ 
	return bit_rate;
}



void AC3Stream::FillAUbuffer(unsigned int frames_to_buffer )
{
	unsigned int framesize_code;
	last_buffered_AU += frames_to_buffer;

	mjpeg_debug( "Scanning %d MPEG audio frames to frame %d\n", 
				 frames_to_buffer, last_buffered_AU );

	while (!bs.eos() && 
		   decoding_order < last_buffered_AU && 
		   (!opt_max_PTS || access_unit.PTS < opt_max_PTS))
	{

		skip=access_unit.length-5; // 5 bytes of header read already...
		if (skip & 0x1) bs.getbits( 8);
		if (skip & 0x2) bs.getbits( 16);
		skip=skip>>2;

		for (int i=0;i<skip;i++)
		{
			bs.getbits( 32);
		}

		prev_offset = AU_start;
		AU_start = bs.bitcount();

		/* Check we have reached the end of have  another catenated 
		   stream to process before finishing ... */
		if ( (syncword = bs.getbits(16))!=AC3_SYNCWORD )
		{
			if( !bs.eobs   )
			{
				mjpeg_warn( "Can't find next AC3 frame - broken bit-stream?\n" );
            }
            break;
		}

        bs.getbits(16);         // CRC field
        bs.getbits(2);          // Sample rate code TOOD: check for change!
        framesize_code = bs.getbits(6);
        if( bs.eobs )
            break;
        framesize = ac3_frame_size[frequency][framesize_code>>1];
        framesize = 
            (framesize_code&1) && frequency == 1 ?
            (framesize + 1) << 1:
            (framesize <<1);
        
		access_unit.start = AU_start;
		access_unit.length = framesize;
		access_unit.PTS = static_cast<clockticks>(decoding_order) * 
			static_cast<clockticks>(AC3_PACKET_SAMPLES) * 
			static_cast<clockticks>(CLOCKS)	/ samples_per_second;;
		access_unit.DTS = access_unit.PTS;
		access_unit.dorder = decoding_order;
		decoding_order++;
		aunits.append( access_unit );
		num_frames[0]++;
		
		num_syncword++;

		if (num_syncword >= old_frames+10 )
		{
			mjpeg_debug ("Got %d frame headers.\n", num_syncword);
			old_frames=num_syncword;
		
		}


    }
	last_buffered_AU = decoding_order;
	eoscan = bs.eos() || (opt_max_PTS && access_unit.PTS >= opt_max_PTS);

}



void AC3Stream::Close()
{
    stream_length = AU_start >> 3;
	mjpeg_info ("AUDIO_STATISTICS: %02x\n", stream_id); 
    mjpeg_info ("Audio stream length %lld bytes.\n", stream_length);
    mjpeg_info   ("Syncwords      : %8u\n",num_syncword);
    mjpeg_info   ("Frames         : %8u padded\n",  num_frames[0]);
    mjpeg_info   ("Frames         : %8u unpadded\n", num_frames[1]);
	
    bs.close();
}

/*************************************************************************
	OutputAudioInfo
	gibt gesammelte Informationen zu den Audio Access Units aus.

	Prints information on audio access units
*************************************************************************/

void AC3Stream::OutputHdrInfo ()
{
	mjpeg_info("AC3 AUDIO STREAM:\n");

    mjpeg_info ("Bit rate       : %8u bytes/sec (%3u kbit/sec)\n",
				bit_rate*128, bit_rate);

    if (frequency == 3)
		mjpeg_info ("Frequency      : reserved\n");
    else
		mjpeg_info ("Frequency      :     %d Hz\n",
				ac3_frequency[frequency]);

}



/* 
 * Local variables:
 *  c-file-style: "stroustrup"
 *  tab-width: 4
 *  indent-tabs-mode: nil
 * End:
 */
