#include "main.h"

#include <math.h>
#include <stdlib.h>


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

static int frequency [4][4] = 
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



static void output_info_video (Video_struc *video_info);
static void output_info_audio (Audio_struc *audio_info);


/*************************************************************************
    MPEG Streams Kontrolle

    Basic Checks on MPEG Streams
*************************************************************************/

static void marker_bit (Bit_stream_struc *bs, unsigned int what)
{
    if (what != get1bit(bs))
    {
        mjpeg_error ("Illegal MPEG stream at offset (bits) %lld: supposed marker bit not found.\n",bitcount(bs));
        exit (1);
    }
}



/*************************************************************************
    MPEG Verifikation der Inputfiles

    Check if files are valid MPEG streams
*************************************************************************/

void check_files (int argc,
				  char* argv[],
				  char** audio_file,
				  char** video_file,
				  unsigned int *audio_bytes,
				  unsigned int *video_bytes
	)
{
    Bit_stream_struc bs1, bs2, undo;
    unsigned int bytes_1, bytes_2;
	
	/* As yet no streams determined... */
	which_streams = 0;
    if (argc == 2) {
		if (open_file(argv[1],&bytes_1))
			exit (1); }
    else if (argc == 3) {
		if (open_file(argv[1],&bytes_1) || open_file(argv[2],&bytes_2))
			exit (1); }
	    
    init_getbits (&bs1, argv[1]);
 
    if (argc == 3)
		init_getbits (&bs2, argv[2]);

	/* Das Bitstreampaket kuemmert sich bei einem look_ahead nicht
       darum, den Buffer vorzubereiten, weil es davon ausgeht, dass
       vorher mindestens einmal ein getbits () gemacht wurde. Da das
       hier nicht zutrifft, muss manuell das erledigt werden, was 
       sonst getbits () macht, d.h. ein Buffer eingelesen werden und
       bestimmte Werte in der Struktur gesetzt werden. */

	prepareundo(&bs1, &undo);
	if (getbits( &bs1, 12 )  == 0xfff)
    {
		*audio_file = argv[1];
		*audio_bytes= bytes_1;
		mjpeg_info ("File %s is a 11172-3 Audio stream.\n",argv[1]);
		which_streams |= STREAMS_AUDIO;
		if (argc == 3 ) {
			if (  getbits(&bs2, 32) != 0x1b3)
			{
				mjpeg_info ("File %s is not a MPEG-1/2 Video stream.\n",argv[2]);
				finish_getbits (&bs1);
				finish_getbits (&bs2);
				exit (1);
			} 
			else
			{
				mjpeg_info ("File %s is a MPEG-1/2 Video stream.\n",argv[2]);
				which_streams |= STREAMS_VIDEO;
				*video_file = argv[2];
				*video_bytes= bytes_2;
			}
		}

    }
    else
    { 
		undochanges( &bs1, &undo);
		if (  getbits( &bs1, 32)  == 0x1b3)
		{
			*video_file = argv[1];
			*video_bytes= bytes_1;
			mjpeg_info ("File %s is a 11172-2 Video stream.\n",argv[1]);
			which_streams |= STREAMS_VIDEO;
			if (argc == 3 ) {
				if ( getbits( &bs2, 12 ) != 0xfff)
				{
					mjpeg_info ("File %s is not a 11172-3 Audio stream.\n",argv[2]);
					finish_getbits (&bs1);
					finish_getbits (&bs2);
					exit (1);
				} 
				else
				{
					mjpeg_info ("File %s is a 11172-3 Audio stream.\n",argv[2]);
					which_streams |= STREAMS_AUDIO;
					*audio_file = argv[2];
					*audio_bytes= bytes_2;
				}
			}
		}
		else 
		{
			if (argc == 3) {
				mjpeg_error ("Files %s and %s are not valid MPEG streams.\n",
						argv[1],argv[2]);
				finish_getbits (&bs1);
				finish_getbits (&bs2);
				exit (1);
			}
			else {
				mjpeg_error ("File %s is not a valid MPEG stream.\n", argv[1]);
				finish_getbits (&bs1);
				exit (1);
			}
		}
	}

	finish_getbits (&bs1);
    if (argc == 3)
		finish_getbits (&bs2);

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


void get_info_video (char *video_file,	
					 Video_struc *video_info,
					 double *ret_first_frame_PTS,
					 unsigned int length,
					 Vector *vid_info_vec)
{
    Bit_stream_struc video_bs;
   	bitcount_t AU_start;
    bitcount_t stream_length=0LL; 
    bitcount_t prev_stream_length=0LL;
    Vaunit_struc access_unit;
    unsigned int syncword;
    unsigned int decoding_order=0;
	unsigned int fields_presented=0;
    unsigned int group_order=0;
    unsigned int group_start_pic=0;
	unsigned int group_start_field=0;
    unsigned long temporal_reference=0;
    unsigned short pict_rate;
	int pulldown_32 = 0;
	int repeat_first_field;
	int film_rate;
    int i;
    unsigned int prozent = 0;
    unsigned int old_prozent=0;
    double frame_rate;
	unsigned int max_bits_persec = 0;
	Vector vaunits = NewVector( sizeof(Vaunit_struc));
	int AU_pict_data;
	int AU_hdr = SEQUENCE_HEADER;  /* GOP or SEQ Header starting AU? */
	
  
    mjpeg_info ("Scanning Video stream for access units information.\n");
    init_getbits (&video_bs, video_file);

    if (getbits (&video_bs, 32)==SEQUENCE_HEADER)
    {
		video_info->num_sequence++;
		video_info->horizontal_size	= getbits (&video_bs, 12);
		video_info->vertical_size	= getbits (&video_bs, 12);
		video_info->aspect_ratio	= getbits (&video_bs,  4);
		pict_rate 			= getbits (&video_bs,  4);
		video_info->picture_rate	= pict_rate;
		video_info->bit_rate		= getbits (&video_bs, 18);
		marker_bit (&video_bs, 1);
		video_info->vbv_buffer_size	= getbits (&video_bs, 10);
		video_info->CSPF		= get1bit (&video_bs);

    } else
    {
		mjpeg_error ("Invalid MPEG Video stream header.\n");
		exit (1);
    }

    empty_vaunit_struc (&access_unit);

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
	*ret_first_frame_PTS = 0.0;
	AU_hdr = SEQUENCE_HEADER;
	AU_pict_data = 0;
	AU_start = 0LL;
	while(!end_bs(&video_bs) && 
		  seek_sync (&video_bs, SYNCWORD_START, 24, 100000) &&
	      ( !opt_max_PTS || access_unit.PTS < opt_max_PTS   ) )
	{
		syncword = (SYNCWORD_START<<8) + getbits (&video_bs, 8);
		
		if( AU_pict_data )
		{
			
			/* Handle the header *ending* an AU...
			   If we have the AU picture data an AU and have now
			   reached a header marking the end of an AU fill in the
			   the AU length and append it to the list of AU's and
			   start a new AU.  I.e. sequence and gop headers count as
			   part of the AU of the corresponding picture
			*/
			stream_length = bitcount (&video_bs)-32LL;
			switch (syncword) 
			{
			case SEQUENCE_HEADER :
			case GROUP_START :
			case PICTURE_START :
				access_unit.length = (int) (stream_length - AU_start)>>3;
				access_unit.end_seq = 0;
				video_info->avg_frames[access_unit.type-1]+=access_unit.length;
				VectorAppend( vaunits, &access_unit );					
				AU_hdr = syncword;
				AU_start = stream_length;
				AU_pict_data = 0;
				break;
			case SEQUENCE_END:
				access_unit.length = ((stream_length - AU_start)>>3)+4;
				access_unit.end_seq = 1;
				VectorAppend( vaunits, &access_unit );
				video_info->avg_frames[access_unit.type-1]+=access_unit.length;

				/* Do we have a sequence split in the video stream? */
				if( !end_bs(&video_bs) && 
					getbits (&video_bs, 32) ==SEQUENCE_HEADER )
				{
					stream_length = bitcount (&video_bs)-32LL;
					AU_start = stream_length;
					AU_hdr = SEQUENCE_HEADER;
					AU_pict_data = 0;
					if( opt_multifile_segment )
						mjpeg_warn("Sequence end marker found in video stream but single-segment splitting specified!\n" );
				}
				else
				{
					if( !end_bs(&video_bs) && ! opt_multifile_segment )
						mjpeg_warn("No seq. header starting new sequence after seq. end!\n");
				}
					
				video_info->num_seq_end++;
				break;
			}
		}

		/* Handle the headers starting an AU... */
		switch (syncword) 
		{
		case SEQUENCE_HEADER:
			video_info->num_sequence++;
			break;
			
		case GROUP_START:
			video_info->num_groups++;
			group_order=0;
			break;
			
		case PICTURE_START:
			/* We have reached AU's picture data... */
			AU_pict_data = 1;
			
			temporal_reference = getbits (&video_bs, 10);
			access_unit.type   = getbits (&video_bs, 3);

			/* Now scan forward a little for an MPEG-2 picture coding extension
			   so we can get pulldown info (if present) */

			if( film_rate &&
				seek_sync (&video_bs,EXT_START_CODE, 32, 10) &&
				getbits(&video_bs,4) == CODING_EXT_ID )
			{
				/* Skip: 4 F-codes (4)... */
				(void)getbits(&video_bs,16); 
                /* Skip: DC Precision(2), pict struct (2) topfirst (1)
				   frame pred dct (1), q_scale_type (1), intravlc (1)*/
				(void)getbits(&video_bs,8);	
				/* Skip: altscan (1) */
				(void)getbits(&video_bs,1);	
				repeat_first_field = getbits(&video_bs,1);
				pulldown_32 |= repeat_first_field;
			}
			else
			{
				repeat_first_field = 0;
			}
			
			if( access_unit.type == IFRAME )
			{
				unsigned int bits_persec = 
					(unsigned int) ( ((double)(stream_length - prev_stream_length)) *
									 2*frame_rate / ((double)(2+fields_presented - group_start_field)));
				
				if( bits_persec > max_bits_persec )
				{
					max_bits_persec = bits_persec;
				}
				prev_stream_length = stream_length;
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
				video_info->num_frames[access_unit.type-1]++;
			}

			prozent =(int) (((float)bitcount(&video_bs)/8/(float)length)*100);
			if ( prozent > old_prozent && verbose > 0 )
			{
				mjpeg_debug("Got %d picture headers. %2d%%\n",
						decoding_order, prozent);
				old_prozent = prozent;
			}
			
			break;		    

  
				
		}
	}

	video_info->num_pictures = decoding_order;	

    video_info->stream_length = (unsigned int)(AU_start / 8);
    for (i=0; i<4; i++)
		if (video_info->num_frames[i]!=0)
			video_info->avg_frames[i] /= video_info->num_frames[i];

    video_info->comp_bit_rate = (unsigned int)
		(
			(((double)video_info->stream_length) / ((double) video_info->num_pictures)) 
			* ((double)frame_rate)  + 25.0
			) / 50;
	
	/* Peak bit rate in 50B/sec units... */
	video_info->peak_bit_rate = ((max_bits_persec / 8) / 50);
    finish_getbits (&video_bs);
    output_info_video (video_info);

	*vid_info_vec = vaunits;

}

/*************************************************************************
	Output_Info_Video
	gibt Ubersicht ueber gesammelte Informationen aus

	Prints information on video access units
*************************************************************************/

static void output_info_video (Video_struc *video_info)
{
	char *str;
	mjpeg_info("VIDEO STREAM:\n");

    mjpeg_info ("Stream length  : %11llu\n",video_info->stream_length);
    mjpeg_info ("Sequence start : %8u\n",video_info->num_sequence);
    mjpeg_info ("Sequence end   : %8u\n",video_info->num_seq_end);
    mjpeg_info ("No. Pictures   : %8u\n",video_info->num_pictures);
    mjpeg_info ("No. Groups     : %8u\n",video_info->num_groups);
    mjpeg_info ("No. I Frames   : %8u avg. size%6u bytes\n",
			  video_info->num_frames[0],video_info->avg_frames[0]);
    mjpeg_info ("No. P Frames   : %8u avg. size%6u bytes\n",
			  video_info->num_frames[1],video_info->avg_frames[1]);
    mjpeg_info ("No. B Frames   : %8u avg. size%6u bytes\n",
			  video_info->num_frames[2],video_info->avg_frames[2]);
    mjpeg_info ("No. D Frames   : %8u avg. size%6u bytes\n",
			  video_info->num_frames[3],video_info->avg_frames[3]);

    mjpeg_info ("Horizontal size: %8u\n",video_info->horizontal_size);
    mjpeg_info ("Vertical size  : %8u\n",video_info->vertical_size);

    switch (video_info->aspect_ratio)
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
    mjpeg_info   ("Aspect ratio   :   %1.4f %s",ratio[video_info->aspect_ratio], str);

    if (video_info->picture_rate == 0)
		mjpeg_info( "Picture rate   : forbidden\n");
    else if (video_info->picture_rate <9)
		mjpeg_info( "Picture rate   :   %2.3f frames/sec\n",
					picture_rates[video_info->picture_rate]);
    else
		mjpeg_info( "Picture rate   : %x reserved\n",video_info->picture_rate);

    if (video_info->bit_rate == 0x3ffff)
		{
			video_info->bit_rate = 0;
			mjpeg_info( "Bit rate       : variable\n"); 
		}
    else if (video_info->bit_rate == 0)
		mjpeg_info( "Bit rate      : forbidden\n");
    else
		mjpeg_info( "Bit rate       : %7u bits/sec\n",
					video_info->bit_rate*400);

    mjpeg_info("Computed rate  : %8u bits/sec\n",video_info->comp_bit_rate*400);
    mjpeg_info("Peak     rate  : %8u  bits/sec\n",video_info->peak_bit_rate*400);
    mjpeg_info("Vbv buffer size: %8u bytes\n",video_info->vbv_buffer_size*2048);
    mjpeg_info("CSPF           : %8u\n",video_info->CSPF);
}

/*************************************************************************
	Output_Info_Audio
	gibt gesammelte Informationen zu den Audio Access Units aus.

	Prints information on audio access units
*************************************************************************/

static void output_info_audio (Audio_struc *audio_info)
{
    unsigned int layer;
    unsigned int bitrate;

    layer=3-audio_info->layer;
    bitrate = bitrate_index[audio_info->version_id][layer][audio_info->bit_rate];


	mjpeg_info("AUDIO STREAM:\n");
	mjpeg_info ("Audio version  : %s\n", audio_version[audio_info->version_id]);
    mjpeg_info (" Stream length  : %11llu\n",audio_info->stream_length);
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
				frequency[audio_info->version_id][audio_info->frequency]);

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

/*************************************************************************
	Get_Info_Audio
	holt Informationen zu den einzelnen Audio Access Units
	(Audio frames) and records it.
*************************************************************************/


void get_info_audio (
	char *audio_file,
	Audio_struc *audio_info,
	double first_frame_PTS,
	unsigned int length,
	Vector *audio_info_vec
	)

{
    Bit_stream_struc audio_bs;
    bitcount_t AU_start=0;
	bitcount_t prev_offset;
    unsigned int framesize;
	unsigned int padding_bit;
    unsigned int skip;
    unsigned int decoding_order=0;
    unsigned int samples_per_second;
    Aaunit_struc access_unit;
    unsigned long syncword;
    int i;
    unsigned int prozent;
    unsigned int old_prozent=0;
    Vector aaunits = NewVector(sizeof(Aaunit_struc));
   
    mjpeg_info ("Scanning Audio stream for access units information. \n");
    init_getbits (&audio_bs, audio_file);
    empty_aaunit_struc (&access_unit);

	/* A.Stevens 2000 - update to be compatible up to  MPEG2.5
	 */
    if (getbits (&audio_bs, 11)==AUDIO_SYNCWORD)
    {
		audio_info->num_syncword++;
		audio_info->version_id = getbits (&audio_bs, 2);
		audio_info->layer 		= getbits (&audio_bs, 2);
		audio_info->protection 		= get1bit (&audio_bs);
		audio_info->bit_rate 		= getbits (&audio_bs, 4);
		audio_info->frequency 		= getbits (&audio_bs, 2);
		padding_bit=get1bit(&audio_bs);
		get1bit (&audio_bs);
		audio_info->mode 		= getbits (&audio_bs, 2);
		audio_info->mode_extension 	= getbits (&audio_bs, 2);
		audio_info->copyright 		= get1bit (&audio_bs);
		audio_info->original_copy 	= get1bit (&audio_bs);
		audio_info->emphasis		= getbits (&audio_bs, 2);

		/* TODO: I'll be the slots counts have changed in the newer versions too... */
		framesize =
			bitrate_index[audio_info->version_id][3-audio_info->layer][audio_info->bit_rate]  * 
			slots [3-audio_info->layer] *1000 /
			frequency[audio_info->version_id][audio_info->frequency];

		audio_info->size_frames[0] = framesize;
		audio_info->size_frames[1] = framesize+1;
		audio_info->num_frames[padding_bit]++;
	
		access_unit.length = audio_info->size_frames[padding_bit];
	  
		samples_per_second = frequency[audio_info->version_id][audio_info->frequency];

		/* Presentation time-stamping  */
		access_unit.PTS = (clockticks)
			decoding_order * samples [3-audio_info->layer] * (clockticks)(CLOCKS) /
			samples_per_second + first_frame_PTS;
		access_unit.dorder = decoding_order;
		++decoding_order;
		VectorAppend( aaunits, &access_unit );

    } else
    {
		mjpeg_error ( "Invalid MPEG Audio stream header.\n");
		exit (1);
    }


    do {
    

		skip=access_unit.length-4;
		if (skip & 0x1) getbits (&audio_bs, 8);
		if (skip & 0x2) getbits (&audio_bs, 16);
		skip=skip>>2;

		for (i=0;i<skip;i++)
		{
			getbits (&audio_bs, 32);
		}
		prev_offset = AU_start;
		AU_start = bitcount(&audio_bs);

		/* Check we have reached the end of have  another catenated 
		   stream to process before finishing ... */

	
		if ( (syncword = getbits (&audio_bs, 11))!=AUDIO_SYNCWORD )
		{
			int bits_to_end = length*8 - AU_start;
			if( bits_to_end > 1024*8  )
			{
				/* There appears to be another catenated stream... */
				int next;
				mjpeg_warn( "End of component bit-stream ... seeking next\n" );
				/* Catenated stream must start on byte boundary */
				syncword = (syncword<<(8-AU_start % 8));
				next = getbits( &audio_bs,8-(AU_start % 8) );
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

		getbits( &audio_bs, 11); /* Skip version, layer, protection, bitrate,sampling */
		prozent =(int) (((float) bitcount(&audio_bs)/8/(float)length)*100);
		audio_info->num_syncword++;

		if ((prozent > old_prozent && verbose > 0))
		{

			mjpeg_debug ("Got %d frame headers. %2d%%\n",
						 audio_info->num_syncword,prozent);
			old_prozent=prozent;
		
		}
	
		padding_bit=get1bit(&audio_bs);
		access_unit.length = audio_info->size_frames[padding_bit];
	
		access_unit.PTS = (clockticks)(decoding_order) * (clockticks)(samples [3-audio_info->layer])* 
						  (clockticks)(CLOCKS) / samples_per_second +first_frame_PTS;
	
		decoding_order++;
		VectorAppend( aaunits, &access_unit );
		audio_info->num_frames[padding_bit]++;

		getbits (&audio_bs, 9);

    } while (!end_bs(&audio_bs) && 
    		(!opt_max_PTS || access_unit.PTS < opt_max_PTS));

    mjpeg_info ("Done, stream bit offset %lld.\n",AU_start);

    audio_info->stream_length = AU_start >> 3;
    finish_getbits (&audio_bs);
    output_info_audio (audio_info);
    
    *audio_info_vec = aaunits;

}

