#include "main.h"

#include <math.h>


static double picture_rates [9] = { 0., 24000./1001., 24., 25., 
									30000./1001., 30., 50., 60000./1001., 60. };

unsigned int bitrate_index [3][16] =
{{0,32,64,96,128,160,192,224,256,288,320,352,384,416,448,0},
 {0,32,48,56,64,80,96,112,128,160,192,224,256,320,384,0},
 {0,32,40,48,56,64,80,96,112,128,160,192,224,256,320,0}};

static double ratio [16] = { 0., 1., 0.6735, 0.7031, 0.7615, 0.8055,
							 0.8437, 0.8935, 0.9157, 0.9815, 1.0255, 1.0695, 1.0950, 1.1575,
							 1.2015, 0.};

static double frequency [4] = {44.1, 48, 32, 0};
static char mode [4][15] =
{ "stereo", "joint stereo", "dual channel", "single channel" };
static char copyright [2][20] =
{ "no copyright","copyright protected" };
static char original [2][10] =
{ "copy","original" };
static char emphasis [4][20] =
{ "none", "50/15 microseconds", "reserved", "CCITT J.17" };
static unsigned int slots [4] = {12, 144, 0, 0};
static unsigned int samples [4] = {384, 1152, 0, 0};


/*************************************************************************
    MPEG Streams Kontrolle

    Basic Checks on MPEG Streams
*************************************************************************/

void marker_bit (bs, what)
	Bit_stream_struc *bs;
	unsigned int what;
{
    if (what != get1bit(bs))
    {
        printf ("\nError in MPEG stream at offset (bits) %lld: supposed marker bit not found.\n",bitcount(bs));
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
				  char** multi_file,
				  unsigned int *audio_bytes,
				  unsigned int *video_bytes
	)
{
    Bit_stream_struc bs1, bs2, undo;
    unsigned int bytes_1, bytes_2;
	
	/* As yet no streams determined... */
	which_streams = 0;
    if (argc == 3) {
		if (open_file(argv[1],&bytes_1))
			exit (1); }
    else if (argc == 4) {
		if (open_file(argv[1],&bytes_1) || open_file(argv[2],&bytes_2))
			exit (1); }
	    
    init_getbits (&bs1, argv[1]);
 
    if (argc == 4)
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
		printf ("File %s is a 11172-3 Audio stream.\n",argv[1]);
		which_streams |= STREAMS_AUDIO;
		if (argc == 4 ) {
			if (  getbits(&bs2, 32) != 0x1b3)
			{
				printf ("File %s is not a MPEG-1/2 Video stream.\n",argv[2]);
				finish_getbits (&bs1);
				finish_getbits (&bs2);
				exit (1);
			} 
			else
			{
				printf ("File %s is a MPEG-1/2 Video stream.\n",argv[2]);
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
			printf ("File %s is a 11172-2 Video stream.\n",argv[1]);
			which_streams |= STREAMS_VIDEO;
			if (argc == 4 ) {
				if ( getbits( &bs2, 12 ) != 0xfff)
				{
					printf ("File %s is not a 11172-3 Audio stream.\n",argv[2]);
					finish_getbits (&bs1);
					finish_getbits (&bs2);
					exit (1);
				} 
				else
				{
					printf ("File %s is a 11172-3 Audio stream.\n",argv[2]);
					which_streams |= STREAMS_AUDIO;
					*audio_file = argv[2];
					*audio_bytes= bytes_2;
				}
			}
		}
		else 
		{
			if (argc == 4) {
				printf ("Files %s and %s are not valid MPEG streams.\n",
						argv[1],argv[2]);
				finish_getbits (&bs1);
				finish_getbits (&bs2);
				exit (1);
			}
			else {
				printf ("File %s is not a valid MPEG stream.\n", argv[1]);
				finish_getbits (&bs1);
				exit (1);
			}
		}
	}

	finish_getbits (&bs1);
    if (argc == 4)
		finish_getbits (&bs2);
    if (argc == 4 )
		*multi_file = argv[3];
    else
		*multi_file = argv[2];
		

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
					 double *startup_delay,
					 unsigned int length,
					 Vector *vid_info_vec)
{
    FILE* info_file;
    Bit_stream_struc video_bs;
   	bitcount_t offset_bits=0LL;
    bitcount_t stream_length=0LL; 
    bitcount_t prev_stream_length=0LL;
    Vaunit_struc access_unit;
    unsigned long syncword;
    unsigned long decoding_order=0;
    unsigned long group_order=0;
    unsigned long temporal_reference=0;
    double secs_per_frame=0;
    unsigned short pict_rate;
    double DTS;
    double PTS;
    int i;
    unsigned int prozent;
    unsigned int old_prozent=0;
    int frame_rate;
	unsigned int max_bits_persec = 0;
	Vector vaunits = NewVector( sizeof(Vaunit_struc));

	
  
    printf ("\nScanning Video stream for access units information.\n");
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
		printf ("Invalid MPEG Video stream header.\n");
		exit (1);
    }

    empty_vaunit_struc (&access_unit);
    *startup_delay = 2*MAX_FFFFFFFF;

    if (pict_rate >0 && pict_rate<9)
    {
		secs_per_frame = 1. / picture_rates[pict_rate];
		frame_rate = picture_rates[pict_rate];
	}
    else
    {
		secs_per_frame = 1. / 25.;	/* invalid pict_rate info */
		frame_rate = 25;
	}
    do {
		if (seek_sync (&video_bs, SYNCWORD_START, 24))
		{
			syncword = (SYNCWORD_START<<8) + getbits (&video_bs, 8);
			switch (syncword) {

			case SEQUENCE_HEADER:
				video_info->num_sequence++;
				break;

			case GROUP_START:
				video_info->num_groups++;
				group_order=0;
				break;

			case PICTURE_START:
			
				stream_length = bitcount (&video_bs)-32LL;
			
				temporal_reference = getbits (&video_bs, 10);
				access_unit.type   = getbits (&video_bs, 3);
				/* skip access unit number 0 */
				if (access_unit.type != 0)
				{

					access_unit.length = (int) (stream_length - offset_bits)>>3;
					offset_bits = stream_length;
					VectorAppend( vaunits, &access_unit );
					video_info->avg_frames[access_unit.type-1]+=access_unit.length;
	
				}

				if( video_info->num_pictures % frame_rate == 0 )
				{
					unsigned int bits_persec = (unsigned int) (stream_length - prev_stream_length);
					if( bits_persec > max_bits_persec )
					{
						printf( "SL=%lld  PSL=%lld\n", stream_length, prev_stream_length );
						printf( "Inc mbps to %d frame %d\n", bits_persec, video_info->num_pictures );
						max_bits_persec = bits_persec;
					}
					prev_stream_length = stream_length;

				}


				DTS = decoding_order * secs_per_frame*CLOCKS;
				PTS = (temporal_reference - group_order + 1 + 
					   decoding_order) * secs_per_frame*CLOCKS;

				*startup_delay=(PTS<*startup_delay ? PTS : *startup_delay);

				make_timecode (DTS,&access_unit.DTS);
				make_timecode (PTS,&access_unit.PTS);
				decoding_order++;
				group_order++;

				if ((access_unit.type>0) && (access_unit.type<5))
					video_info->num_frames[access_unit.type-1]++;

				prozent =(int) (((float)bitcount(&video_bs)/8/(float)length)*100);
				video_info->num_pictures++;	
		    


				if (prozent > old_prozent && verbose > 0 )
				{
					printf ("Got %d picture headers. %2d%%%c",
							video_info->num_pictures, prozent, verbose > 1 ? '\n' : '\r');
					fflush (stdout);
					old_prozent = prozent;
				}
			
				break;		    

			case SEQUENCE_END:
				stream_length = bitcount (&video_bs);
				access_unit.length = (stream_length - offset_bits)>>3;
				VectorAppend( vaunits, &access_unit );
				video_info->avg_frames[access_unit.type-1]+=access_unit.length;
				offset_bits = stream_length;
				video_info->num_seq_end++;
				break;		    

			}
		} else break;
    } while (!end_bs(&video_bs));

    printf ("\nDone, stream bit offset %lld.\n",offset_bits);

    video_info->stream_length = (unsigned int)(offset_bits / 8);
    for (i=0; i<4; i++)
		if (video_info->num_frames[i]!=0)
			video_info->avg_frames[i] /= video_info->num_frames[i];

    if (secs_per_frame >0.)
        video_info->comp_bit_rate = (unsigned int)
			(
				(((double)video_info->stream_length) / ((double) video_info->num_pictures)) 
				* ((double)frame_rate)  + 25.0
				) / 50;
    else
		video_info->comp_bit_rate = 0;
		
	printf( "Original calc would give video rate of %f\n",
			ceil ((double)(video_info->stream_length)/
				  (double)(video_info->num_pictures)/secs_per_frame/1250.)*25);
	/* Peak bit rate in 50B/sec units... */
	video_info->peak_bit_rate = ((max_bits_persec / 8) / 50);
    finish_getbits (&video_bs);
    output_info_video (video_info);

	*vid_info_vec = vaunits;
    ask_continue ();
 	
}

/*************************************************************************
	Output_Info_Video
	gibt Ubersicht ueber gesammelte Informationen aus

	Prints information on video access units
*************************************************************************/

void output_info_video (video_info)

	Video_struc *video_info;
{
	printf("\n+------------------ VIDEO STREAM INFORMATION -----------------+\n");

    printf ("\nStream length  : %11llu\n",video_info->stream_length);
    printf   ("Sequence start : %8u\n",video_info->num_sequence);
    printf   ("Sequence end   : %8u\n",video_info->num_seq_end);
    printf   ("No. Pictures   : %8u\n",video_info->num_pictures);
    printf   ("No. Groups     : %8u\n",video_info->num_groups);
    printf   ("No. I Frames   : %8u avg. size%6u bytes\n",
			  video_info->num_frames[0],video_info->avg_frames[0]);
    printf   ("No. P Frames   : %8u avg. size%6u bytes\n",
			  video_info->num_frames[1],video_info->avg_frames[1]);
    printf   ("No. B Frames   : %8u avg. size%6u bytes\n",
			  video_info->num_frames[2],video_info->avg_frames[2]);
    printf   ("No. D Frames   : %8u avg. size%6u bytes\n",
			  video_info->num_frames[3],video_info->avg_frames[3]);

    printf   ("Horizontal size: %8u\n",video_info->horizontal_size);
    printf   ("Vertical size  : %8u\n",video_info->vertical_size);
    printf   ("Aspect ratio   :   %1.4f ",ratio[video_info->aspect_ratio]);

    switch (video_info->aspect_ratio)
    {
	case  0: printf ("forbidden\n"); break;
	case  1: printf ("VGA etc\n"); break;
	case  3: printf ("16:9, 625 line\n"); break;
	case  6: printf ("16:9, 525 line\n"); break;
	case  8: printf ("CCIR601, 625 line\n"); break;
	case 12: printf ("CCIR601, 525 line\n"); break;
	case 15: printf ("reserved\n"); break;
	default: printf ("\n");
    }

    if (video_info->picture_rate == 0)
		printf("Picture rate   : forbidden\n");
    else if (video_info->picture_rate <9)
		printf("Picture rate   :   %2.3f frames/sec\n",
			   picture_rates[video_info->picture_rate]);
    else
		printf("Picture rate   : %x reserved\n",video_info->picture_rate);

    if (video_info->bit_rate == 0x3ffff) {
		video_info->bit_rate = 0;
		printf("Bit rate       : variable\n"); }
    else if (video_info->bit_rate == 0)
		printf("Bit rate      : forbidden\n");
    else
		printf("Bit rate       : %7u bits/sec\n",
			   video_info->bit_rate*400);

    printf   ("Computed rate  : %8u bits/sec\n",video_info->comp_bit_rate*400);
    printf   ("Peak     rate  : %8u  bits/sec\n",video_info->peak_bit_rate*400);
    printf   ("Vbv buffer size: %8u bytes\n",video_info->vbv_buffer_size*2048);
    printf   ("CSPF           : %8u\n",video_info->CSPF);
}

/*************************************************************************
	Output_Info_Audio
	gibt gesammelte Informationen zu den Audio Access Units aus.

	Prints information on audio access units
*************************************************************************/

void output_info_audio (audio_info)

	Audio_struc *audio_info;
{
    unsigned int layer;
    unsigned int bitrate;

    layer=3-audio_info->layer;
    bitrate = bitrate_index[layer][audio_info->bit_rate];


	printf("\n+------------------ AUDIO STREAM INFORMATION -----------------+\n");

    printf ("\nStream length  : %11llu\n",audio_info->stream_length);
    printf   ("Syncwords      : %8u\n",audio_info->num_syncword);
    printf   ("Frames         : %8u size %6u bytes\n",
			  audio_info->num_frames[0],audio_info->size_frames[0]);
    printf   ("Frames         : %8u size %6u bytes\n",
			  audio_info->num_frames[1],audio_info->size_frames[1]);
    printf   ("Layer          : %8u\n",1+layer);

    if (audio_info->protection == 0) printf ("CRC checksums  :      yes\n");
    else  printf ("CRC checksums  :       no\n");

    if (audio_info->bit_rate == 0)
		printf ("Bit rate       :     free\n");
    else if (audio_info->bit_rate == 0xf)
		printf ("Bit rate       : reserved\n");
    else
		printf ("Bit rate       : %8u bytes/sec (%3u kbit/sec)\n",
				bitrate*128, bitrate);

    if (audio_info->frequency == 3)
		printf ("Frequency      : reserved\n");
    else
		printf ("Frequency      :     %2.1f kHz\n",
				frequency[audio_info->frequency]);

    printf   ("Mode           : %8u %s\n",
			  audio_info->mode,mode[audio_info->mode]);
    printf   ("Mode extension : %8u\n",audio_info->mode_extension);
    printf   ("Copyright bit  : %8u %s\n",
			  audio_info->copyright,copyright[audio_info->copyright]);
    printf   ("Original/Copy  : %8u %s\n",
			  audio_info->original_copy,original[audio_info->original_copy]);
    printf   ("Emphasis       : %8u %s\n",
			  audio_info->emphasis,emphasis[audio_info->emphasis]);
}

/*************************************************************************
	Get_Info_Audio
	holt Informationen zu den einzelnen Audio Access Units
	(Audio frames) ein und speichert sie in einer temporaeren
	Datei ab.

	gets information on the single audio access units (audio frames)
	and saves them into a tmp file for further processing.
*************************************************************************/


void get_info_audio (
	char *audio_file,
	Audio_struc *audio_info,
	double *startup_delay,
	unsigned int length,
	Vector *audio_info_vec
	)

{
    FILE* info_file;
    Bit_stream_struc audio_bs;
    bitcount_t offset_bits=0;
	bitcount_t prev_offset;
    unsigned int framesize;
	unsigned int padding_bit;
    unsigned int skip;
    unsigned int decoding_order=0;
    double PTS;
    double samples_per_second;
    Aaunit_struc access_unit;
    unsigned long syncword;
    int i;
    unsigned int prozent;
    unsigned int old_prozent=0;
    Vector aaunits = NewVector(sizeof(Aaunit_struc));
   
    printf ("\nScanning Audio stream for access units information.\n");
    init_getbits (&audio_bs, audio_file);
    empty_aaunit_struc (&access_unit);

    if (getbits (&audio_bs, 12)==AUDIO_SYNCWORD)
    {
		marker_bit (&audio_bs, 1);
		audio_info->num_syncword++;
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

		framesize =
			bitrate_index[3-audio_info->layer][audio_info->bit_rate] /
			frequency[audio_info->frequency] * slots [3-audio_info->layer];
		audio_info->size_frames[0] = framesize;
		audio_info->size_frames[1] = framesize+1;
		audio_info->num_frames[padding_bit]++;
	
		access_unit.length = audio_info->size_frames[padding_bit];
	  
		samples_per_second = (double)frequency [audio_info->frequency];

		/* TODO This code here should be based on CLOCK */
		PTS = decoding_order * samples [3-audio_info->layer] /
			samples_per_second * (CLOCKS/1000.) + *startup_delay;

		make_timecode (PTS, &access_unit.PTS);
		decoding_order++;
		VectorAppend( aaunits, &access_unit );

    } else
    {
		printf ("Invalid MPEG Audio stream header.\n");
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
		prev_offset = offset_bits;
		offset_bits = bitcount(&audio_bs);

		/* Check we have reached the end of have  another catenated 
		   stream to process before finishing ... */

		if ( (syncword = getbits (&audio_bs, 12))!=AUDIO_SYNCWORD )
		{
			int bits_to_end = length*8 - offset_bits;
			if( bits_to_end > 1024*8  )
			{
				/* There appears to be another catenated stream... */
				int next;
				printf( "\nEnd of component bit-stream ... seeking next\n" );
				/* Catenated stream must start on byte boundary */
				syncword = (syncword<<(8-offset_bits % 8));
				next = getbits( &audio_bs,8-(offset_bits % 8) );
				syncword = syncword | next;
				/*
				  printf( "syncword offset = %d syncword = %03x, next = %08x\n",
				  offset_bits % 8, syncword, next ); */
				if( syncword != AUDIO_SYNCWORD )
				{
					printf( "Warning: Failed to find start of next stream at %lld prev %lld !\n", offset_bits/8, prev_offset/8 );
					break;
				}
			}
			else
				/* No catenated stream... finished! */
				break;
		}
	
		marker_bit (&audio_bs, 1);
		prozent =(int) (((float) bitcount(&audio_bs)/8/(float)length)*100);
		audio_info->num_syncword++;
		if (prozent > old_prozent && verbose > 0)
		{
			printf ("Got %d frame headers. %2d%%%c",
					audio_info->num_syncword,prozent, verbose > 1? '\n' : '\r');
			fflush (stdout);
			old_prozent=prozent;
		
		}
		getbits (&audio_bs, 9);
	
		padding_bit=get1bit(&audio_bs);
		access_unit.length = audio_info->size_frames[padding_bit];
	
		PTS = decoding_order * samples [3-audio_info->layer] /
			samples_per_second * (CLOCKS/1000.)+ *startup_delay;
		make_timecode (PTS, &access_unit.PTS);
	
		decoding_order++;
		VectorAppend( aaunits, &access_unit );
		audio_info->num_frames[padding_bit]++;

		getbits (&audio_bs, 9);
	
    } while (!end_bs(&audio_bs));

    printf ("\nDone, stream bit offset %lld.\n",offset_bits);

    audio_info->stream_length = offset_bits >> 3;
    finish_getbits (&audio_bs);
    output_info_audio (audio_info);
    ask_continue ();
    
    *audio_info_vec = aaunits;

}

