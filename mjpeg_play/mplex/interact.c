#include "main.h"
#include <stdlib.h>
#include <unistd.h>


/*************************************************************************
    Startbildschirm und Anzahl der Argumente

    Intro Screen and argument check
*************************************************************************/

static void Usage(char *str)
{
	fprintf( stderr, "Usage: %s [params] [<input stream1> [<input stream2>] <output system stream>\n\n", str);
	fprintf( stderr, "  where possible params are:\n" );
	fprintf( stderr, " -q      Quiet mode for unattended batch usage\n" );
	fprintf( stderr, " -n      Noisy (verbose) mode for debugging streams\n" );
	fprintf( stderr, " -m      Mpeg version (default: 1) [1..2]\n");
	fprintf( stderr, " -b num  Specify decoder buffers size in kB. (default: 46) [ 20...1000]\n" );
    fprintf( stderr, " -r num  Specify data rate of output stream in kbit/sec (default 0=Compute from source streams)\n" );
	fprintf( stderr, " -v num  Specify a video timestamp offset in mSec\n");
	fprintf( stderr, " -V      Multiplex variable bit-rate (experimental)\n");
	fprintf( stderr, " -a num  Specify an audio timestamp offset in mSec \n" );
	fprintf( stderr, " -s num  Specify sector size in bytes (default: 2324) [256..16384]\n");
	fprintf( stderr, " -f fmt  Set pre-defined mux format.\n");
	fprintf( stderr, "         [0 = Auto MPEG1, 1 = VCD, 2 = Auto MPEG2, 3 = SVCD, 4 = DVD]\n");
	fprintf( stderr, "         (N.b only 1 and 2 currently implemented!*)\n" ); 
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
int opt_mux_format = 0;
int opt_multi_segment = 1;
intmax_t max_system_segment_size = 700*1024*1024;

int intro_and_options(int argc, char *argv[])
{
    int n;
    printf("\n***************************************************************\n");
    printf(  "*               MPEG1/SYSTEMS      Multiplexer                *\n");
    printf(  "*               (C)  Christoph Moar, 1994/1995                *\n");
    printf(  "*               moar@informatik.tu-muenchen.de                *\n");
    printf(  "*               Technical University of Munich                *\n");
    printf(  "*               SIEMENS ZFE  ST SN 11 / T SN 6                *\n");
    printf(  "*                                                             *\n");
    printf(  "*  This program is free software. See the GNU General Public  *\n");
    printf(  "*  License in the file COPYING for more details.              *\n");
    printf(  "*  Release %s (%s)                                  *\n",MPLEX_VER,MPLEX_DATE);
    printf(  "***************************************************************\n\n");


  while( (n=getopt(argc,argv,"b:r:v:a:m:f:qiVn")) != EOF)
  {
    switch(n)
	  {
	  
	  case 'm' :
		opt_mpeg = atoi(optarg);
		if( opt_mpeg < 1 || opt_mpeg > 2 )
		  Usage(argv[0]);
  	
	  	break;
	  case 'q' :
		verbose = 0;
		break;
	
	  case 'n' :
		verbose = 2;
		break;
	
	  case 'V' :
	    opt_VBR = 1;
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

	  case 'v':
		opt_video_offset = atoi(optarg);
		if( opt_video_offset < -10000000 || opt_video_offset > 1000000 )
		  Usage(argv[0]);
		break;

	  case 'a' :
		opt_audio_offset = atoi(optarg);
		if( opt_video_offset < -10000000 || opt_video_offset > 1000000 )
		  Usage(argv[0]);
		break;
	  
	  case 'f' :
	    opt_mux_format = atoi(optarg);
	    if( opt_mux_format < MPEG_MPEG1 || opt_mux_format > 4 )
	    	Usage(argv[0]);
		break;
	  case 's' :
		opt_sector_size = atoi(optarg);
		if( opt_sector_size < 256 || opt_sector_size > 16384 )
		  Usage(argv[0]);
		break;

	  default :
		Usage(argv[0]);
		break;
	  }
  }
  if (argc - optind < 2)
    {	
	  Usage(argv[0]);
    }
  return optind-1;
}


/*************************************************************************
    File vorhanden?

    File found?
*************************************************************************/

int open_file(name, bytes)			
char *name;
unsigned int *bytes;				
{
    FILE* datei;

    datei=fopen (name, "rw");
    if (datei==NULL)
    {	
	printf("File %s not found.\n", name);
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

void status_info (nsectors_a, nsectors_v, nsectors_p, nbytes, 
		  buf_v, buf_a,verbose)
unsigned int nsectors_a;
unsigned int nsectors_v;
unsigned int nsectors_p;
unsigned long long nbytes;
unsigned int buf_v;
unsigned int buf_a;
int verbose;
{
	if( verbose > 0 )
	{
	  printf ("| %7d | %7d |",nsectors_a,nsectors_v);
	  printf (" %7d | %11lld |",nsectors_p,nbytes);
	  printf (" %6d | %6d |",buf_a,buf_v);
	  printf ((verbose > 1?"\n":"\r"));
	  fflush (stdout);
	}
}

void status_header ()
{
    status_footer();
    printf("|  Audio  |  Video  | Padding | Bytes  MPEG | Audio  | Video  |\n");
    printf("| Sectors | Sectors | Sectors | System File | Buffer | Buffer |\n");
    status_footer();
}


void status_message (what)
unsigned char what;
{
  if( verbose == 1 )
	printf( "\n" );
  switch (what)
  {
  case STATUS_AUDIO_END:
  printf("|file  end|         |         |             |        |        |\n");
  break;
  case STATUS_AUDIO_TIME_OUT:
  printf("|time  out|         |         |             |        |        |\n");
  break;
  case STATUS_VIDEO_END:
  printf("|         |file  end|         |             |        |        |\n");
  break;
  case STATUS_VIDEO_TIME_OUT:
  printf("|         |time  out|         |             |        |        |\n");
  }
}

void status_footer ()
{
  printf("+---------+---------+---------+-------------+--------+--------+\n");
}
