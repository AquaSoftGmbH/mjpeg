#include "main.h"
#include <stdlib.h>
#include <unistd.h>


/*************************************************************************
    Startbildschirm und Anzahl der Argumente

    Intro Screen and argument check
*************************************************************************/

static void Usage(char *str)
{
	fprintf( stderr, "mjpegtools mplex version " VERSION "\n" );
	fprintf( stderr, "Usage: %s [params] -o <output file> [<input file1> [<input file2>] \n\n", str);
	fprintf( stderr, "  where possible params are:\n" );
	fprintf( stderr, " -v num  Level of verbosity. 0 = quiet, 1 = normal 2 = verbose/debug\n");
	fprintf( stderr, " -m      Mpeg version (default: 1) [1..2]\n");
	fprintf( stderr, " -b num  Specify decoder buffers size in kB. (default: 46) [ 20...1000]\n" );
    fprintf( stderr, " -r num  Specify data rate of output stream in kbit/sec\n"
			         "(default 0=Compute from source streams)\n" );
	fprintf( stderr, " -l num  Multiplex only num frames (default 0=multiplex all)\n");
	fprintf( stderr, " -O num  Specify offset of timestamps (video-audio) in mSec\n");
	fprintf( stderr, " -s num  Specify sector size in bytes (default: 2324) [256..16384]\n");
	fprintf( stderr, " -V      Multiplex variable bit-rate video\n");
	fprintf( stderr, " -p num  Number of packets per pack (default: 20) [1..100]\n"  );
	fprintf( stderr, " -h      System header in every pack rather than just in first\n" );
	fprintf( stderr, " -f fmt  Set pre-defined mux format.\n");
	fprintf( stderr, "         [0 = Auto MPEG1, 1 = VCD, 2 = user-rate VCD,\n");
	fprintf( stderr, "          3 = Auto MPEG2, 4 = SVCD, 5 = user-rate SVCD, 6 = DVD]\n");
	fprintf( stderr, "         (N.b only 0 .. 5 currently implemented!*)\n" ); 
	fprintf( stderr, " -S size Maximum size of output file in M bytes (default: 2000) (0 = no limit)\n" );
	fprintf( stderr, " -M      Generate a *single* multi-file program per\n"
			         "sequence rather a program per file\n");
	fprintf( stderr, "         %%d in the output file name is replaced by a segment counter\n");
	fprintf( stderr, " -e      Vcdmplex style start-up (debugging tool)\n");
	fprintf( stderr, " -?      Print this lot out!\n");
	fprintf( stderr, "SIZE_T %d\n", sizeof(off_t) );
			
	exit (1);
}

int verbose = 1;
int opt_buffer_size = 46;
int opt_data_rate = 0;  /* 3486 = 174300B/sec would be right for VCD */
int opt_video_offset = 0;
int opt_audio_offset = 0;
int opt_sector_size = 2324;
int opt_VBR = 0;
int opt_mpeg = 1;
int opt_mux_format = 0;			/* Generic MPEG-1 stream as default */
int opt_multifile_segment = 0;
int opt_always_system_headers = 0;
int opt_packets_per_pack = 20;
int opt_max_timeouts = 10;
clockticks opt_max_PTS = 0LL;
int opt_emul_vcdmplex = 0;

/* Should fit nicely on an ordinary CD ... */
intmax_t max_system_segment_size =  2000*1024*1024;

int intro_and_options(int argc, char *argv[], char **multplex_outfile)
{
    int n;
	char *outfile = NULL;
	while( (n=getopt(argc,argv,"o:b:r:O:v:m:f:l:s:S:qiVnMeh")) != EOF)
	{
		switch(n)
		{
		case 'o' :
			outfile = optarg;
			break;
		case 'm' :
			opt_mpeg = atoi(optarg);
			if( opt_mpeg < 1 || opt_mpeg > 2 )
				Usage(argv[0]);
  	
			break;
		case 'v' :
			verbose = atoi(optarg);
			if( verbose < 0 || verbose > 2 )
				Usage(argv[0]);
			break;

		case 'V' :
			opt_VBR = 1;
			break;
	  
		case 'h' :
			opt_always_system_headers = 1;
			break;

		case 'b':
			opt_buffer_size = atoi(optarg);
			if( opt_buffer_size < 0 || opt_buffer_size > 1000 )
				Usage(argv[0]);
			break;

		case 'r':
			opt_data_rate = atoi(optarg);
			if( opt_data_rate < 0 )
				Usage(argv[0]);
			/* Convert from kbit/sec (user spec) to 50B/sec units... */
			opt_data_rate = (( opt_data_rate * 1000 / 8 + 49) / 50 ) * 50;
			break;

		case 'O':
			opt_video_offset = atoi(optarg);
			if( opt_video_offset < 0 )
			{
				opt_audio_offset = - opt_video_offset;
				opt_video_offset = 0;
			}
			break;
          
		case 'l' : 
			opt_max_PTS = (clockticks)atoi(optarg) * (clockticks)CLOCKS;
			if( opt_max_PTS < 1  )
				Usage(argv[0]);
			break;
		
		case 'p' : 
			opt_packets_per_pack = atoi(optarg);
			if( opt_packets_per_pack < 1 || opt_packets_per_pack > 100  )
				Usage(argv[0]);
			break;
		
	  
		case 'f' :
			opt_mux_format = atoi(optarg);
			if( opt_mux_format < MPEG_MPEG1 || opt_mux_format > MPEG_SVCD_NSR )
				Usage(argv[0]);
			break;
		case 's' :
			opt_sector_size = atoi(optarg);
			if( opt_sector_size < 256 || opt_sector_size > 16384 )
				Usage(argv[0]);
			break;
		case 'S' :
			max_system_segment_size = atoi(optarg);
			if( max_system_segment_size < 0  )
				Usage(argv[0]);
			max_system_segment_size *= 1024*1024; 
			break;
		case 'M' :
			opt_multifile_segment = 1;
			break;
		case 'e' :
			opt_emul_vcdmplex = 1;
			break;
		case '?' :
		default :
			Usage(argv[0]);
			break;
		}
	}
	if (argc - optind < 1 || outfile == NULL)
    {	
		Usage(argv[0]);
    }
	(void)mjpeg_default_handler_verbosity(verbose);
	mjpeg_info( "mplex version %s (%s)\n",MPLEX_VER,MPLEX_DATE );
	*multplex_outfile = outfile;
	return optind-1;
}


/*************************************************************************
    File vorhanden?

    File found?
*************************************************************************/

int open_file(char *name, unsigned int *bytes)			
{
    FILE* datei;

    datei=fopen (name, "rw");
    if (datei==NULL)
    {	
		mjpeg_error("File %s not found.\n", name);
		return (TRUE);
    }
    fseek (datei, 0, 2);
    *bytes = ftell(datei);
    fclose(datei);
    return (FALSE);
}


/******************************************************************
	Status_Info
	druckt eine Statuszeile waehrend des Multiplexens aus.

	prints a status line during multiplexing
******************************************************************/

void status_info (	unsigned int nsectors_a,
					unsigned int nsectors_v,
					unsigned int nsectors_p,
					unsigned long long nbytes,
					unsigned int buf_v,
					unsigned int buf_a,
					log_level_t level
				 )
{
	mjpeg_log( level, "sectors muxed: audio=%07d video=%08d padding=%07d\n" ,nsectors_a,nsectors_v,nsectors_p);
	mjpeg_log( LOG_DEBUG,"l=%11lld abuf=%6d vbuf=%6d\n",nbytes,buf_a,buf_v);
}



void timeout_error(int what, int decode_number)
{
	static int timeouts = 0;
	switch (what)
		{
		case STATUS_AUDIO_TIME_OUT:
			mjpeg_error("Likely buffer under-run at  audio sector %d\n",decode_number);
			break;
		case STATUS_VIDEO_TIME_OUT:
			mjpeg_error("Likely buffer under-run  at video sector %d\n",decode_number);
			break;
		}
	if( ++timeouts > opt_max_timeouts )
		exit(1);
  
}

