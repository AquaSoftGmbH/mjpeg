/*
 *  audiostrm_in.c:  Audio strem class members handling scanning and buffering
 *                   raw input stream.
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

#include <config.h>
#include <math.h>
#include <stdlib.h>

#include "audiostrm.hh"
#include "interact.hh"
#include "outputstream.hh"


char *audio_version[4] =
{
	"2.5",
	"2.0",
	"reserved",
	"1.0"
};

unsigned int bitrates_kbps [4][3][16] =
{
	{ /* MPEG audio V2.5 */
		{0,32,48,56,64,80,96,112,128,144,160,176,192,224,256,0},
		{0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,0},
		{0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,0}
	},
	{ /*RESERVED*/
		{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
		{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
		{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
	},
	{ /* MPEG audio V2 */
		{0,32,48,56,64,80,96,112,128,144,160,176,192,224,256,0},
		{0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,0},
		{0,8,16,24,32,40,48,56,64,80,96,112,128,144,160,0}
	},
	{ /* MPEG audio V1 */
		{0,32,64,96,128,160,192,224,256,288,320,352,384,416,448,0},
		{0,32,48,56,64,80,96,112,128,160,192,224,256,320,384,0},
		{0,32,40,48,56,64,80,96,112,128,160,192,224,256,320,0}
	}

};


static int freq_table [4][4] = 
{
	/* MPEG audio V2.5 */
	{11025,12000,8000,0},
	/* RESERVED */
	{ 0, 0, 0, 0 }, 
	/* MPEG audio V2 */
	{22050,24000, 16000,0},
	/* MPEG audio V1 */
	{44100, 48000, 32000, 0}
};
static char stereo_mode [4][15] =
{ "stereo", "joint stereo", "dual channel", "single channel" };
static char copyright_status [2][20] =
{ "no copyright","copyright protected" };
static char original_bit [2][10] =
{ "copy","original" };
static char emphasis_mode [4][20] =
{ "none", "50/15 microseconds", "reserved", "CCITT J.17" };
static unsigned int slots [4] = {12, 144, 144, 0};
static unsigned int samples [4] = {384, 1152, 1152, 0};





/*************************************************************************
	Get_Info_Audio
	holt Informationen zu den einzelnen Audio Access Units
	(Audio frames) and records it.
*************************************************************************/


void AudioStream::Init ( const int stream_num, 
						 const char *audio_file)

{
    unsigned int i;
	int padding_bit;
	mjpeg_debug( "SETTING zero stuff to %d\n", muxinto.vcd_zero_stuffing );
	mjpeg_debug( "SETTING audio buffer to %d\n", muxinto.audio_buffer_size );

	MuxStream::Init( AUDIO_STR_0 + stream_num, 
					 0,  // Buffer scale
					 muxinto.audio_buffer_size,
					 muxinto.vcd_zero_stuffing,
					 muxinto.buffers_in_audio,
					 muxinto.always_buffers_in_audio
		);
    mjpeg_info ("Scanning Audio stream for access units information. \n");

	InitAUbuffer();
	InputStream::Init( audio_file, 512*1024 );
	
	/* A.Stevens 2000 - update to be compatible up to  MPEG2.5
	 */
    if (bs.getbits( 11)==AUDIO_SYNCWORD)
    {
		num_syncword++;
		version_id = bs.getbits( 2);
		layer 		= bs.getbits( 2);
		protection 		= bs.get1bit();
		bit_rate_code	= bs.getbits( 4);
		frequency 		= bs.getbits( 2);
		padding_bit     = bs.get1bit();
		bs.get1bit();
		mode 		= bs.getbits( 2);
		mode_extension 	= bs.getbits( 2);
		copyright 		= bs.get1bit();
		original_copy 	= bs.get1bit ();
		emphasis		= bs.getbits( 2);

		framesize =
			bitrates_kbps[version_id][3-layer][bit_rate_code]  * 
			slots[3-layer] *1000 /
			freq_table[version_id][frequency];

        mjpeg_info( "rate %d slots %d freq %d size = %d\n",
                    bitrates_kbps[version_id][3-layer][bit_rate_code],
                    slots [3-layer] *1000,
                    freq_table[version_id][frequency],
                    framesize );
        
        int frame_length;
        if( layer == 1 )
            frame_length = 
               12 *1000 * bitrates_kbps[version_id][3-layer][bit_rate_code] / freq_table[version_id][frequency];
        else
            frame_length = 
            144 *1000 * bitrates_kbps[version_id][3-layer][bit_rate_code] / freq_table[version_id][frequency];

        /* DEBUGGING!!!! */
        if( frame_length != framesize )
            mjpeg_error_exit1( "INTERNAL: just found an inconsistent frame length %d(%d)\n", frame_length, framesize );
		size_frames[0] = framesize;
		size_frames[1] = framesize+( layer == 1 ? 4 : 1);
		num_frames[padding_bit]++;
	
		access_unit.length = size_frames[padding_bit];
	  
		samples_per_second = freq_table[version_id][frequency];

		/* Presentation time-stamping  */
		access_unit.PTS = static_cast<clockticks>(decoding_order) * 
			static_cast<clockticks>(samples [3-layer]) * 
			static_cast<clockticks>(CLOCKS)	/ samples_per_second;
		access_unit.DTS = access_unit.PTS;
		access_unit.dorder = decoding_order;
		++decoding_order;
		aunits.append( access_unit );

    } else
    {
		mjpeg_error ( "Invalid MPEG Audio stream header.\n");
		exit (1);
    }


	OutputHdrInfo();
}

unsigned int AudioStream::NominalBitRate()
{ 
	return bitrates_kbps[version_id][3-layer][bit_rate_code]*128;
}


unsigned int AudioStream::SizeFrame( int rate_code, int padding )
{
	return bitrates_kbps[version_id][3-layer][rate_code]  * 
		slots [3-layer] *1000 /
		freq_table[version_id][frequency] + padding;
}

void AudioStream::FillAUbuffer(unsigned int frames_to_buffer )
{
	unsigned int i;
	unsigned int padding_bit;
	last_buffered_AU += frames_to_buffer;

	mjpeg_debug( "Scanning %d MPEG audio frames to frame %d\n", 
				 frames_to_buffer, last_buffered_AU );

	while (!bs.eos() && 
		   decoding_order < last_buffered_AU && 
		   (!opt_max_PTS || access_unit.PTS < opt_max_PTS))
	{

		skip=access_unit.length-4;
		if (skip & 0x1) bs.getbits( 8);
		if (skip & 0x2) bs.getbits( 16);
		skip=skip>>2;

		for (i=0;i<skip;i++)
		{
			bs.getbits( 32);
		}
		prev_offset = AU_start;
		AU_start = bs.bitcount();

		/* Check we have reached the end of have  another catenated 
		   stream to process before finishing ... */
		if ( (syncword = bs.getbits( 11))!=AUDIO_SYNCWORD )
		{
			if( !bs.eobs   )
			{
				/* There appears to be another catenated stream... */
				int next;
				mjpeg_warn( "End of component bit-stream ... seeking next\n" );
				/* Catenated stream must start on byte boundary */
				syncword = (syncword<<(8-AU_start % 8));
				next = bs.getbits(8-(AU_start % 8) );
				AU_start = bs.bitcount()-11;
				syncword = syncword | next;
				if( syncword != AUDIO_SYNCWORD )
				{
					mjpeg_warn("Failed to find start of next stream at %lld prev %lld !\n", AU_start/8, prev_offset/8 );
					break;
				}
			}
			else
				/* No catenated stream... finished! */
				break;
		}

		// Skip version_id:2, layer:2, protection:1
		(void) bs.getbits( 5);
		int rate_code	= bs.getbits( 4);
		// Skip frequency
		(void) bs.getbits( 2);

		padding_bit=bs.get1bit();
		access_unit.start = AU_start;
		access_unit.length = SizeFrame( rate_code, padding_bit );
		access_unit.PTS = static_cast<clockticks>(decoding_order) * static_cast<clockticks>(samples[3-layer]) * static_cast<clockticks>(CLOCKS)
			/ samples_per_second;
		access_unit.DTS = access_unit.PTS;
		access_unit.dorder = decoding_order;
		decoding_order++;
		aunits.append( access_unit );
		num_frames[padding_bit]++;

		bs.getbits( 9);
		
		num_syncword++;

		if (num_syncword >= old_frames+1000 )
		{
			mjpeg_debug ("Got %d frame headers.\n", num_syncword);
			old_frames=num_syncword;
		
		}
	


    }
	last_buffered_AU = decoding_order;
	eoscan = bs.eos() || (opt_max_PTS && access_unit.PTS >= opt_max_PTS);

}



void AudioStream::Close()
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

void AudioStream::OutputHdrInfo ()
{
    unsigned int bitrate;
    bitrate = bitrates_kbps[version_id][3-layer][bit_rate_code];


	mjpeg_info("AUDIO STREAM:\n");
	mjpeg_info ("Audio version  : %s\n", audio_version[version_id]);
    mjpeg_info   ("Layer          : %8u\n",4-layer);

    if (protection == 0) mjpeg_info ("CRC checksums  :      yes\n");
    else  mjpeg_info ("CRC checksums  :       no\n");

    if (bit_rate_code == 0)
		mjpeg_info ("Bit rate       :     free\n");
    else if (bit_rate_code == 0xf)
		mjpeg_info ("Bit rate       : reserved\n");
    else
		mjpeg_info ("Bit rate       : %8u bytes/sec (%3u kbit/sec)\n",
				bitrate*128, bitrate);

    if (frequency == 3)
		mjpeg_info ("Frequency      : reserved\n");
    else
		mjpeg_info ("Frequency      :     %d Hz\n",
				freq_table[version_id][frequency]);

    mjpeg_info   ("Mode           : %8u %s\n",
			  mode,stereo_mode[mode]);
    mjpeg_info   ("Mode extension : %8u\n",mode_extension);
    mjpeg_info   ("Copyright bit  : %8u %s\n",
			  copyright,copyright_status[copyright]);
    mjpeg_info   ("Original/Copy  : %8u %s\n",
			  original_copy,original_bit[original_copy]);
    mjpeg_info   ("Emphasis       : %8u %s\n",
			  emphasis,emphasis_mode[emphasis]);
}



/* 
 * Local variables:
 *  c-file-style: "stroustrup"
 *  tab-width: 4
 *  indent-tabs-mode: nil
 * End:
 */
