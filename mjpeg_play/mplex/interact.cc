#include <config.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/stat.h>

#include <mjpeg_logging.h>
#include <format_codes.h>

#include "interact.hh"
#include "mpegconsts.hh"


int opt_verbosity = 1;
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
clockticks opt_max_PTS = 0;
int opt_emul_vcdmplex = 0;
bool opt_ignore_underrun = false;
bool opt_split_at_seq_end = true;
off_t opt_max_segment_size = 0;

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
	fprintf( stderr, " -l num  Multiplex only num seconds of material (default 0=multiplex all)\n");
	fprintf( stderr, " -O num  Specify offset of timestamps (video-audio) in mSec\n");
	fprintf( stderr, " -s num  Specify sector size in bytes (default: 2324) [256..16384]\n");
	fprintf( stderr, " -V      Multiplex variable bit-rate video\n");
	fprintf( stderr, " -p num  Number of packets per pack (default: 20) [1..100]\n"  );
	fprintf( stderr, " -h      System header in every pack rather than just in first\n" );
	fprintf( stderr, " -f fmt  Set pre-defined mux format.\n");
	fprintf( stderr, "         [0 = Auto MPEG1, 1 = VCD, 2 = user-rate VCD,\n");
	fprintf( stderr, "          3 = Auto MPEG2, 4 = SVCD, 5 = user-rate SVCD\n");
	fprintf( stderr, "          6 = VCD Stills, 7 = SVCD Stills, 8 = DVD\n");

	fprintf( stderr, "         (N.b only 0 .. 7 currently implemented!*)\n" ); 
	fprintf( stderr, " -S size Maximum size of output file in M bytes (default: 2000) (0 = no limit)\n" );
	fprintf( stderr, " -M      Generate a *single* multi-file program per\n"
			         "sequence rather a program per file\n");
	fprintf( stderr, "         %%d in the output file name is replaced by a segment counter\n");
	fprintf( stderr, " -e      Vcdmplex style start-up (debugging tool)\n");
	fprintf( stderr, " -?      Print this lot out!\n");
			
	exit (1);
}

int intro_and_options(int argc, char *argv[], char **multplex_outfile)
{
    int n;
	char *outfile = NULL;
	while( (n=getopt(argc,argv,"o:b:r:O:v:m:f:l:s:S:q:p:VXMeh")) != EOF)
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
			opt_verbosity = atoi(optarg);
			if( opt_verbosity < 0 || opt_verbosity > 2 )
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
 			opt_max_PTS = static_cast<clockticks>(atoi(optarg)) * CLOCKS;
			if( opt_max_PTS < 1LL  )
				Usage(argv[0]);
			break;
		
		case 'p' : 
			opt_packets_per_pack = atoi(optarg);
			if( opt_packets_per_pack < 1 || opt_packets_per_pack > 100  )
				Usage(argv[0]);
			break;
		
	  
		case 'f' :
			opt_mux_format = atoi(optarg);
			if( opt_mux_format < MPEG_FORMAT_MPEG1 || opt_mux_format > MPEG_FORMAT_LAST )
				Usage(argv[0]);
			break;
		case 's' :
			opt_sector_size = atoi(optarg);
			if( opt_sector_size < 256 || opt_sector_size > 16384 )
				Usage(argv[0]);
			break;
		case 'S' :
			opt_max_segment_size = atoi(optarg);
			if( opt_max_segment_size < 0  )
				Usage(argv[0]);
			opt_max_segment_size *= 1024*1024; 
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
	(void)mjpeg_default_handler_verbosity(opt_verbosity);
	mjpeg_info( "mplex version %s (%s)\n",MPLEX_VER,MPLEX_DATE );
	*multplex_outfile = outfile;
	return optind-1;
}



/*************************************************************************
    MPEG Streams Kontrolle

    Basic Checks on MPEG Streams
*************************************************************************/




/*************************************************************************
    Check if files are valid MPEG streams
*************************************************************************/

void check_files (int argc,
				  char* argv[],
				  char* *audio_file,
				  char* *video_file
	)
{
    IBitStream bs1, bs2;
    BitStreamUndo undo;
	
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
    File vorhanden?

    File found?
*************************************************************************/

bool open_file(const char *name)			
{
    FILE* datei;
	struct stat stb;

    datei=fopen (name, "rw");

    if (datei==NULL)
    {	
		mjpeg_error("File %s not found.\n", name);
		return (true);
    }
    fclose(datei);
    return (false);
}


/* 
 * Local variables:
 *  c-file-style: "stroustrup"
 *  tab-width: 4
 *  indent-tabs-mode: nil
 * End:
 */
