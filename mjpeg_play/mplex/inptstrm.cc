#include "main.hh"

#include <math.h>
#include <stdlib.h>
#include <sys/stat.h>


static double picture_rates [9] = { 0., 24000./1001., 24., 25., 
									30000./1001., 30., 50., 60000./1001., 60. };

char *audio_version[4] =
{
	"2.5",
	"2.0",
	"reserved",
	"1.0"
};

unsigned int bitrate_index [4][3][16] =
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

static double ratio [16] = { 0., 1., 0.6735, 0.7031, 0.7615, 0.8055,
							 0.8437, 0.8935, 0.9157, 0.9815, 1.0255, 1.0695, 1.0950, 1.1575,
							 1.2015, 0.};

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
static char mode [4][15] =
{ "stereo", "joint stereo", "dual channel", "single channel" };
static char copyright [2][20] =
{ "no copyright","copyright protected" };
static char original [2][10] =
{ "copy","original" };
static char emphasis [4][20] =
{ "none", "50/15 microseconds", "reserved", "CCITT J.17" };
static unsigned int slots [4] = {12, 144, 144, 0};
static unsigned int samples [4] = {384, 1152, 1152, 0};



static void output_info_audio (AudioStream *audio_info);


/*************************************************************************
    MPEG Streams Kontrolle

    Basic Checks on MPEG Streams
*************************************************************************/

static void marker_bit (IBitStream &bs, unsigned int what)
{
    if (what != bs.get1bit())
    {
        mjpeg_error ("Illegal MPEG stream at offset (bits) %lld: supposed marker bit not found.\n",bs.bitcount());
        exit (1);
    }
}

/************************************************************************
Pick out and check MPEG stills files.
TODO: Do more than just collect the files can be opened!

************************************************************************/

void check_stills( const int argc, char *argv[], vector<const char *>stills )
{
	int i;
	off_t file_length;
	while( i < argc )
	{
		if( open_file( argv[i] ) )
		{
			mjpeg_info( "Still image: %s\n", argv[i], file_length);
				stills.push_back(argv[i]);
		}
		else
		{
			mjpeg_error_exit1( "Could not open file: %s\n", argv[i]);
		}
	}
}


/*************************************************************************
    MPEG Verifikation der Inputfiles

    Check if files are valid MPEG streams
*************************************************************************/

void check_files (int argc,
				  char* argv[],
				  char* *audio_file,
				  char* *video_file
	)
{
    IBitStream bs1, bs2, undo;
	
	/* As yet no streams determined... */
    if (argc == 2) {
		if (open_file(argv[1]))
			exit (1); }
    else if (argc == 3) {
		if (open_file(argv[1]) || open_file(argv[2]))
			exit (1); }
	    
    bs1.open(argv[1]);
 
    if (argc == 3)
		bs2.open(argv[2]);

	bs1.prepareundo( undo);
	if (bs1.getbits( 12 )  == 0xfff)
    {
		*audio_file = argv[1];
		mjpeg_info ("File %s is a 11172-3 Audio stream.\n",argv[1]);
		if (argc == 3 ) {
			if (  bs2.getbits( 32) != 0x1b3)
			{
				mjpeg_info ("File %s is not a MPEG-1/2 Video stream.\n",argv[2]);
				bs1.close();
				bs2.close();
				exit (1);
			} 
			else
			{
				mjpeg_info ("File %s is a MPEG-1/2 Video stream.\n",argv[2]);
				*video_file = argv[2];
			}
		}

    }
    else
    { 
		bs1.undochanges( undo);
		if (  bs1.getbits( 32)  == 0x1b3)
		{
			*video_file = argv[1];
			mjpeg_info ("File %s is an MPEG-1/2 Video stream.\n",argv[1]);
			if (argc == 3 ) {
				if ( bs2.getbits( 12 ) != 0xfff)
				{
					mjpeg_info ("File %s is not a 11172-3 Audio stream.\n",argv[2]);
					bs1.close();
					bs2.close();
					exit (1);
				} 
				else
				{
					mjpeg_info ("File %s is a 11172-3 Audio stream.\n",argv[2]);
					*audio_file = argv[2];
				}
			}
		}
		else 
		{
			if (argc == 3) {
				mjpeg_error ("Files %s and %s are not valid MPEG streams.\n",
						argv[1],argv[2]);
				bs1.close();
				bs2.close();
				exit (1);
			}
			else {
				mjpeg_error ("File %s is not a valid MPEG stream.\n", argv[1]);
				bs1.close();
				exit (1);
			}
		}
	}

	bs1.close();
    if (argc == 3)
		bs2.close();

}

/*************************************************************************
	Get_Info_Video
	holt Informationen zu den einzelnen Access-Units (Frames) des
	Videofiles ein und speichert sie in einer temporaeren Datei
	ab. Wird spaeter zum Aufbau der gemultiplexten Datei gebraucht.

	Gets informations on the single access units (frames) of the
	video stream and saves them in a tmp file for further
	processing. We need it for building the multiplex file.
*************************************************************************/


void VideoStream::Init (const char *video_file, int stream_num )
{
	prev_offset=0;
    decoding_order=0;
	fields_presented=0;
    group_order=0;
    group_start_pic=0;
	group_start_field=0;
    temporal_reference=0;
	pulldown_32 = 0;
	last_buffered_AU=0;

	max_bits_persec = 0;
	AU_hdr = SEQUENCE_HEADER;  /* GOP or SEQ Header starting AU? */
	
    mjpeg_info ("Scanning Video stream %d for access units information.\n",
				stream_num);

	InputStream::Init( video_file, 
										   VIDEO_STR_0 + stream_num ); 
    if (bs.getbits( 32)==SEQUENCE_HEADER)
    {
		num_sequence++;
		horizontal_size	= bs.getbits( 12);
		vertical_size	= bs.getbits( 12);
		aspect_ratio	= bs.getbits(  4);
		pict_rate 		= bs.getbits(  4);
		picture_rate	= pict_rate;
		bit_rate		= bs.getbits( 18);
		marker_bit( bs, 1);
		vbv_buffer_size	= bs.getbits( 10);
		CSPF		= bs.get1bit();

    } else
    {
		mjpeg_error ("Invalid MPEG Video stream header.\n");
		exit (1);
    }

#ifdef REDUNDANT
    empty_vaunit_struc (&access_unit);
#endif

    if (pict_rate >0 && pict_rate<9)
    {
		frame_rate = picture_rates[pict_rate];
		film_rate = 1;
	}
    else
    {
		frame_rate = 25.0;
		film_rate = 0;
	}

	/* Skip to the end of the 1st AU (*2nd* Picture start!)
	*/
	AU_hdr = SEQUENCE_HEADER;
	AU_pict_data = 0;
	AU_start = 0LL;

	fillAUbuffer(FRAME_CHUNK);
    output_seqhdr_info();

}

void VideoStream::fillAUbuffer(unsigned int frames_to_buffer)
{
	last_buffered_AU += frames_to_buffer;
	mjpeg_info( "Scanning %d video frames to frame %d\n", 
				 frames_to_buffer, last_buffered_AU );

	while(!bs.eos() && 
		  bs.seek_sync( SYNCWORD_START, 24, 100000) &&
	      decoding_order < last_buffered_AU  &&
		  (!opt_max_PTS || access_unit.PTS < opt_max_PTS ) 
		)
	{
		syncword = (SYNCWORD_START<<8) + bs.getbits( 8);
		if( AU_pict_data )
		{
			
			/* Handle the header *ending* an AU...
			   If we have the AU picture data an AU and have now
			   reached a header marking the end of an AU fill in the
			   the AU length and append it to the list of AU's and
			   start a new AU.  I.e. sequence and gop headers count as
			   part of the AU of the corresponding picture
			*/
			stream_length = bs.bitcount()-32LL;
			switch (syncword) 
			{
			case SEQUENCE_HEADER :
			case GROUP_START :
			case PICTURE_START :
				access_unit.length = (int) (stream_length - AU_start)>>3;
				access_unit.end_seq = 0;
				avg_frames[access_unit.type-1]+=access_unit.length;
				aunits.append( access_unit );					
				AU_hdr = syncword;
				AU_start = stream_length;
				AU_pict_data = 0;
				break;
			case SEQUENCE_END:
				access_unit.length = ((stream_length - AU_start)>>3)+4;
				access_unit.end_seq = 1;
				aunits.append( access_unit );
				avg_frames[access_unit.type-1]+=access_unit.length;

				/* Do we have a sequence split in the video stream? */
				if( !bs.eos() && 
					bs.getbits( 32) ==SEQUENCE_HEADER )
				{
					stream_length = bs.bitcount()-32LL;
					AU_start = stream_length;
					AU_hdr = SEQUENCE_HEADER;
					AU_pict_data = 0;
					if( opt_multifile_segment )
						mjpeg_warn("Sequence end marker found in video stream but single-segment splitting specified!\n" );
				}
				else
				{
					if( !bs.eos() && ! opt_multifile_segment )
						mjpeg_warn("No seq. header starting new sequence after seq. end!\n");
				}
					
				num_seq_end++;
				break;
			}
		}

		/* Handle the headers starting an AU... */
		switch (syncword) 
		{
		case SEQUENCE_HEADER:
			num_sequence++;
			break;
			
		case GROUP_START:
			num_groups++;
			group_order=0;
			break;
			
		case PICTURE_START:
			/* We have reached AU's picture data... */
			AU_pict_data = 1;
			
			temporal_reference = bs.getbits( 10);
			access_unit.type   = bs.getbits( 3);

			/* Now scan forward a little for an MPEG-2 picture coding extension
			   so we can get pulldown info (if present) */

			if( film_rate &&
				bs.seek_sync(EXT_START_CODE, 32, 10) &&
				bs.getbits(4) == CODING_EXT_ID )
			{
				/* Skip: 4 F-codes (4)... */
				(void)bs.getbits(16); 
                /* Skip: DC Precision(2), pict struct (2) topfirst (1)
				   frame pred dct (1), q_scale_type (1), intravlc (1)*/
				(void)bs.getbits(8);	
				/* Skip: altscan (1) */
				(void)bs.getbits(1);	
				repeat_first_field = bs.getbits(1);
				pulldown_32 |= repeat_first_field;
			}
			else
			{
				repeat_first_field = 0;
			}
			
			if( access_unit.type == IFRAME )
			{
				unsigned int bits_persec = 
					(unsigned int) ( ((double)(stream_length - prev_offset)) *
									 2*frame_rate / ((double)(2+fields_presented - group_start_field)));
				
				if( bits_persec > max_bits_persec )
				{
					max_bits_persec = bits_persec;
				}
				prev_offset = stream_length;
				group_start_pic = decoding_order;
				group_start_field = fields_presented;
			}

			if( pulldown_32 )
			{
				int frames2field;
				int frames3field;
				access_unit.DTS =  (clockticks) (fields_presented * (double)(CLOCKS/2) / frame_rate);
				access_unit.dorder = decoding_order;
				if( repeat_first_field )
				{
					frames2field = (temporal_reference+1) / 2;
					frames3field = temporal_reference / 2;
					fields_presented += 3;
				}
				else
				{
					frames2field = (temporal_reference) / 2;
					frames3field = (temporal_reference+1) / 2;
					fields_presented += 2;
				}
				access_unit.PTS =  (clockticks) 
					((frames2field*2 + frames3field*3 + group_start_field) * (double)(CLOCKS/2) / frame_rate);
				access_unit.porder = temporal_reference + group_start_pic;
			}
			else
			{
				access_unit.DTS =  (clockticks) (decoding_order * (double)CLOCKS / frame_rate);
				access_unit.PTS =  (clockticks) ((temporal_reference + group_start_pic) * (double)CLOCKS / frame_rate);
				fields_presented += 2;
			}

			access_unit.dorder = decoding_order;
			access_unit.porder = temporal_reference + group_start_pic;
			access_unit.seq_header = ( AU_hdr == SEQUENCE_HEADER);

			decoding_order++;
			group_order++;

			if ((access_unit.type>0) && (access_unit.type<5))
			{
				num_frames[access_unit.type-1]++;
			}

			prozent =static_cast<int>(bs.bitcount()/8*100/file_length);
			if ( prozent > old_prozent && verbose > 0 )
			{
				mjpeg_debug("Got %d picture headers. %2d%%\n",
						decoding_order, prozent);
				old_prozent = prozent;
			}
			
			break;		    

  
				
		}
	}
	last_buffered_AU = decoding_order;
	num_pictures = decoding_order;	
	eoscan = bs.eos() || (opt_max_PTS && access_unit.PTS >= opt_max_PTS);

}

VAunit *VideoStream::next()
{
	if( !eoscan && aunits.curpos()+FRAME_CHUNK > last_buffered_AU  )
	{
		if( aunits.curpos() > FRAME_CHUNK )
		{
			aunits.flush(FRAME_CHUNK);
		}
		fillAUbuffer(FRAME_CHUNK);
	}

	return aunits.next();
}

VAunit *VideoStream::lookahead( unsigned int i )
{
	assert( i < FRAME_CHUNK-1 );
	return aunits.lookahead(i);
}

void VideoStream::close()
{

	bs.close();
	fclose( rawstrm );
    stream_length = (unsigned int)(AU_start / 8);
    for (int i=0; i<4; i++)
	{
		avg_frames[i] /= num_frames[i] == 0 ? 1 : num_frames[i];
	}

    comp_bit_rate = (unsigned int)
		(
			(((double)stream_length) / ((double) num_pictures)) 
			* ((double)frame_rate)  + 25.0
			) / 50;
	
	/* Peak bit rate in 50B/sec units... */
	peak_bit_rate = ((max_bits_persec / 8) / 50);

    mjpeg_info ("Video Stream length: %11llu\n",stream_length);
    mjpeg_info ("Sequence headers: %8u\n",num_sequence);
    mjpeg_info ("Sequence ends   : %8u\n",num_seq_end);
    mjpeg_info ("No. Pictures    : %8u\n",num_pictures);
    mjpeg_info ("No. Groups      : %8u\n",num_groups);
    mjpeg_info ("No. I Frames    : %8u avg. size%6u bytes\n",
			  num_frames[0],avg_frames[0]);
    mjpeg_info ("No. P Frames    : %8u avg. size%6u bytes\n",
			  num_frames[1],avg_frames[1]);
    mjpeg_info ("No. B Frames    : %8u avg. size%6u bytes\n",
			  num_frames[2],avg_frames[2]);
    mjpeg_info ("No. D Frames    : %8u avg. size%6u bytes\n",
			  num_frames[3],avg_frames[3]);
    mjpeg_info("Average bit-rate : %8u bits/sec\n",comp_bit_rate*400);
    mjpeg_info("Peak bit-rate    : %8u  bits/sec\n",peak_bit_rate*400);
	
}
	



/*************************************************************************
	Output_Info_Video
	gibt Ubersicht ueber gesammelte Informationen aus

	Prints information on video access units
*************************************************************************/

void VideoStream::output_seqhdr_info ()
{
	char *str;
	mjpeg_info("VIDEO STREAM:\n");

    mjpeg_info ("Frame width    : %8u\n",horizontal_size);
    mjpeg_info ("Frame height   : %8u\n",vertical_size);

    switch (aspect_ratio)
    {
	case  0: str = "forbidden\n"; break;
	case  1: str = "VGA etc\n"; break;
	case  3: str = "16:9, 625 line\n"; break;
	case  6: str = "16:9, 525 line\n"; break;
	case  8: str = "CCIR601, 625 line\n"; break;
	case 12: str = "CCIR601, 525 line\n"; break;
	case 15: str = "reserved\n"; break;
	default: str = "\n";
    }
    mjpeg_info   ("Aspect ratio    :   %1.4f %s",ratio[aspect_ratio], str);

    if (picture_rate == 0)
		mjpeg_info( "Picture rate    : forbidden\n");
    else if (picture_rate <9)
		mjpeg_info( "Picture rate    :   %2.3f frames/sec\n",
					picture_rates[picture_rate]);
    else
		mjpeg_info( "Picture rate    : %x reserved\n",picture_rate);

    if (bit_rate == 0x3ffff)
		{
			bit_rate = 0;
			mjpeg_info( "Bit rate        : variable\n"); 
		}
    else if (bit_rate == 0)
		mjpeg_info( "Bit rate       : forbidden\n");
    else
		mjpeg_info( "Bit rate        : %7u bits/sec\n",
					bit_rate*400);

    mjpeg_info("Vbv buffer size : %8u bytes\n",vbv_buffer_size*2048);
    mjpeg_info("CSPF            : %8u\n",CSPF);
}

/*************************************************************************
	Get_Info_Audio
	holt Informationen zu den einzelnen Audio Access Units
	(Audio frames) and records it.
*************************************************************************/


void AudioStream::Init (
	char *audio_file,
	int stream_num
	)

{
    unsigned int i;
   
    mjpeg_info ("Scanning Audio stream for access units information. \n");
	InputStream::Init( audio_file, 
					   AUDIO_STR_0 + stream_num );
	
	/* A.Stevens 2000 - update to be compatible up to  MPEG2.5
	 */
    if (bs.getbits( 11)==AUDIO_SYNCWORD)
    {
		num_syncword++;
		version_id = bs.getbits( 2);
		layer 		= bs.getbits( 2);
		protection 		= bs.get1bit();
		bit_rate 		= bs.getbits( 4);
		frequency 		= bs.getbits( 2);
		padding_bit                 = bs.get1bit();
		bs.get1bit();
		mode 		= bs.getbits( 2);
		mode_extension 	= bs.getbits( 2);
		copyright 		= bs.get1bit();
		original_copy 	= bs.get1bit ();
		emphasis		= bs.getbits( 2);

		/* TODO: I'll be the slots counts have changed in the newer versions too... */
		framesize =
			bitrate_index[version_id][3-layer][bit_rate]  * 
			slots [3-layer] *1000 /
			freq_table[version_id][frequency];

		size_frames[0] = framesize;
		size_frames[1] = framesize+1;
		num_frames[padding_bit]++;
	
		access_unit.length = size_frames[padding_bit];
	  
		samples_per_second = freq_table[version_id][frequency];

		/* Presentation time-stamping  */
		access_unit.PTS = static_cast<clockticks>(decoding_order) * 
			static_cast<clockticks>(samples [3-layer]) * 
			static_cast<clockticks>(CLOCKS)	/ samples_per_second;
		access_unit.dorder = decoding_order;
		++decoding_order;
		aunits.append( access_unit );

    } else
    {
		mjpeg_error ( "Invalid MPEG Audio stream header.\n");
		exit (1);
    }



}

void AudioStream::fillAUbuffer(unsigned int frames_to_buffer )
{
	unsigned int i;
	last_buffered_AU += frames_to_buffer;

	mjpeg_info( "Scanning %d MPEG audio frames to frame %d\n", 
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
			bitcount_t bits_to_end = file_length*8 - AU_start;
			if( bits_to_end > 1024*8  )
			{
				/* There appears to be another catenated stream... */
				int next;
				mjpeg_warn( "End of component bit-stream ... seeking next\n" );
				/* Catenated stream must start on byte boundary */
				syncword = (syncword<<(8-AU_start % 8));
				next = bs.getbits(8-(AU_start % 8) );
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

		bs.getbits( 11); /* Skip version, layer, protection, bitrate,sampling */
		prozent =(int) bs.bitcount()*100/8/file_length;
		num_syncword++;

		if ((prozent > old_prozent && verbose > 0))
		{

			mjpeg_debug ("Got %d frame headers. %2d%%\n",
						 num_syncword,prozent);
			old_prozent=prozent;
		
		}
	
		padding_bit=bs.get1bit();
		access_unit.length = size_frames[padding_bit];
	
		access_unit.PTS = static_cast<clockticks>(decoding_order) * static_cast<clockticks>(samples[3-layer]) * static_cast<clockticks>(CLOCKS)
			/ samples_per_second;

		decoding_order++;
		aunits.append( access_unit );
		num_frames[padding_bit]++;

		bs.getbits( 9);

    }
	last_buffered_AU = decoding_order;
	eoscan = bs.eos() || (opt_max_PTS && access_unit.PTS >= opt_max_PTS);

	if( eoscan )
	{
		close();
	}
}

AAunit *AudioStream::next()
{
	if( !eoscan && aunits.curpos()+FRAME_CHUNK > last_buffered_AU  )
	{
		if( aunits.curpos() > FRAME_CHUNK )
		{
			aunits.flush(FRAME_CHUNK);
		}
		fillAUbuffer(FRAME_CHUNK);
	}

	return aunits.next();
}

AAunit *AudioStream::lookahead( unsigned int i )
{
	assert( i < FRAME_CHUNK-1 );
	return aunits.lookahead(i);
}

void AudioStream::close()
{

    mjpeg_info ("Audio stream length %lld.\n",AU_start);
	
    stream_length = AU_start >> 3;
    bs.close();
    output_info_audio (this);

}

/*************************************************************************
	Output_Info_Audio
	gibt gesammelte Informationen zu den Audio Access Units aus.

	Prints information on audio access units
*************************************************************************/

static void output_info_audio (AudioStream *audio_info)
{
    unsigned int layer;
    unsigned int bitrate;

    layer=3-audio_info->layer;
    bitrate = bitrate_index[audio_info->version_id][layer][audio_info->bit_rate];


	mjpeg_info("AUDIO STREAM:\n");
	mjpeg_info ("Audio version  : %s\n", audio_version[audio_info->version_id]);
    mjpeg_info ("Stream length  : %11llu\n",audio_info->stream_length);
    mjpeg_info   ("Syncwords      : %8u\n",audio_info->num_syncword);
    mjpeg_info   ("Frames         : %8u size %6u bytes\n",
			  audio_info->num_frames[0],audio_info->size_frames[0]);
    mjpeg_info   ("Frames         : %8u size %6u bytes\n",
			  audio_info->num_frames[1],audio_info->size_frames[1]);
    mjpeg_info   ("Layer          : %8u\n",1+layer);

    if (audio_info->protection == 0) mjpeg_info ("CRC checksums  :      yes\n");
    else  mjpeg_info ("CRC checksums  :       no\n");

    if (audio_info->bit_rate == 0)
		mjpeg_info ("Bit rate       :     free\n");
    else if (audio_info->bit_rate == 0xf)
		mjpeg_info ("Bit rate       : reserved\n");
    else
		mjpeg_info ("Bit rate       : %8u bytes/sec (%3u kbit/sec)\n",
				bitrate*128, bitrate);

    if (audio_info->frequency == 3)
		mjpeg_info ("Frequency      : reserved\n");
    else
		mjpeg_info ("Frequency      :     %d Hz\n",
				freq_table[audio_info->version_id][audio_info->frequency]);

    mjpeg_info   ("Mode           : %8u %s\n",
			  audio_info->mode,mode[audio_info->mode]);
    mjpeg_info   ("Mode extension : %8u\n",audio_info->mode_extension);
    mjpeg_info   ("Copyright bit  : %8u %s\n",
			  audio_info->copyright,copyright[audio_info->copyright]);
    mjpeg_info   ("Original/Copy  : %8u %s\n",
			  audio_info->original_copy,original[audio_info->original_copy]);
    mjpeg_info   ("Emphasis       : %8u %s\n",
			  audio_info->emphasis,emphasis[audio_info->emphasis]);
}


