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
	fprintf( stderr, " -i      Interactive mode for user intervention\n" );
	fprintf( stderr, " -n      Noisy (verbose) mode for debugging streams\n" );
	fprintf( stderr, " -b num  Specify decoder buffers size in kB. (default: 46) [ 20...1000]\n" );
    fprintf( stderr, " -r num  Specify data rate of output stream in kbit/sec (default 0=Compute from source streams)\n" );
	fprintf( stderr, " -v num  Specify a video timestamp offset in mSec\n");
	fprintf( stderr, " -V      Multiplex variable bit-rate (experimental)\n");
	fprintf( stderr, " -a num  Specify an audio timestamp offset in mSec \n" );
	fprintf( stderr, " -s num  Specify sector size in bytes (default: 2324) [256..16384]");
	exit (1);
}

int verbose = 1;
int opt_interactive_mode = 0;
int opt_buffer_size = 46;
int opt_data_rate = 1740;
int opt_video_offset = 0;
int opt_audio_offset = 0;
int opt_sector_size = 2324;
int opt_VBR = 0;

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


  while( (n=getopt(argc,argv,"b:r:v:a:qiVn")) != EOF)
  {
    switch(n)
	  {
	  
	  case 'q' :
		verbose = 0;
		break;
	
	  case 'n' :
		verbose = 2;
		break;
	
	  case 'V' :
	    opt_VBR = 1;
	    break;
	  
	  case 'i' :
		opt_interactive_mode = 1;
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


/*************************************************************************
	ask_continue
	Nach Anzeige der Streaminformationen Abfrage, ob weiter
	gearbeitet werden soll.

	After displaying Stream informations there is a check, wether
	we should continue computing or not.
*************************************************************************/

void ask_continue ()
{
    char input[20];
	
	if( ! opt_interactive_mode )
	  return;

    printf ("\nContinue processing (y/n) : ");
    do fgets (input,20,stdin);
    while (input[0]!='N'&&input[0]!='n'&&input[0]!='y'&&input[0]!='Y');

    if (input[0]=='N' || input[0]=='n')
    {
	printf ("\nStop processing.\n\n");
	exit (0);

    }

}

/*************************************************************************
	ask_verbose
	Soll die letzte, MPEG/SYSTEM Tabelle vollstaendig ausgegeben
	werden?

	Should we print the MPEG/SYSTEM table very verbose or not?
*************************************************************************/

int ask_verbose ()
{
    char input[20];

	if( ! opt_interactive_mode )
	  return FALSE;

    printf ("\nVery verbose mode (y/n) : ");
    do fgets (input,20,stdin);
    while (input[0]!='N'&&input[0]!='n'&&input[0]!='y'&&input[0]!='Y');

    if (input[0]=='N' || input[0]=='n') return (FALSE); else return (TRUE);
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
