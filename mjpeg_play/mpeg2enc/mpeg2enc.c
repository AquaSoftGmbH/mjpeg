/* mpeg2enc.c, main() and parameter file reading                            */

/* Copyright (C) 1996, MPEG Software Simulation Group. All Rights Reserved. */

/*
 * Disclaimer of Warranty
 *
 * These software programs are available to the user without any license fee or
 * royalty on an "as is" basis.  The MPEG Software Simulation Group disclaims
 * any and all warranties, whether express, implied, or statuary, including any
 * implied warranties or merchantability or of fitness for a particular
 * purpose.  In no event shall the copyright-holder be liable for any
 * incidental, punitive, or consequential damages of any kind whatsoever
 * arising from the use of these programs.
 *
 * This disclaimer of warranty extends to the user of these programs and user's
 * customers, employees, agents, transferees, successors, and assigns.
 *
 * The MPEG Software Simulation Group does not represent or warrant that the
 * programs furnished hereunder are free of infringement of any third-party
 * patents.
 *
 * Commercial implementations of MPEG-1 and MPEG-2 video, including shareware,
 * are subject to royalty fees to patent holders.  Many of these patents are
 * general enough such that they are unavoidable regardless of implementation
 * design.
 *
 */
/* Modifications and enhancements (C) 2000/2001 Andrew Stevens */

/* These modifications are free software; you can redistribute it
 *  and/or modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2 of
 *  the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#define GLOBAL /* used by global.h */

#include "global.h"

int verbose = 2;

/* private prototypes */
static void init (void);
static void init_encoding_parms(void);
static void init_quantmat (void);



/*
 * RJ: Introduced the possiblity to not search at all.
 * Saves time at the cost of quality
 */

int do_not_search = 0;

/* RJ: Parameters for creation of the MPEG file */

static int param_bitrate    = 0;
static int param_quant      = 0;
static int param_searchrad  = 16;
static int param_mpeg       = 1;
static int param_aspect_ratio = 0;
static int param_frame_rate  = 0;
static int param_fieldpic   = 0;  /* 0: progressive, 1: bottom first, 2: top first, 3 = progressive seq, interlace frames with field MC and DCT in picture */
static int param_norm       = 'p';  /* 'n': NTSC, 'p': PAL, 's': SECAM, else unspecified */
static int param_44_red	= 2;
static int param_22_red	= 3;	
static int param_hfnoise_quant = 0;
static int param_hires_quant = 0;
static double param_act_boost = 2.0;
static int param_video_buffer_size = 46;
static int param_seq_hdr_every_gop = 0;
static int param_seq_length_limit = 2000;
static int param_min_GOP_size = 12;
static int param_max_GOP_size = 12;
static int param_Bgrp_size = 3;
static int param_num_cpus = 1;
static int param_32_pulldown = 0;

/* reserved: for later use */
int param_422 = 0;



static double framerates[]=
    {0.0, 24000.0/1001.0,24.0,25.0,30000.0/1001.0,30.0,50.0,60000.0/1001.0,60.0};

#define num_frame_rates (sizeof(framerates)/sizeof(double))

static char *frame_rate_definitions[num_frame_rates] =
{
    "illegal", 
	"24000.0/1001.0 (NTSC 3:2 pulldown converted FILM)",
	"24.0 (NATIVE FILM)",
	"25.0 (PAL/SECAM VIDEO / converted FILM)",
	"30000.0/1001.0 (NTSC VIDEO)",
	"30.0",
	"50.0 (PAL FIELD RATE)",
	"60000.0/1001.0 (NTSC FIELD RATE)",
	"60.0"
};


static char * mpeg1_aspect_ratio_definitions[] =
{
	"1:1 (square pixels)",
	"1:0.6735",
	"1:0.7031 (16:9 Anisomorphic PAL/SECAM for 720x578/352x288 images)",
    "1:0.7615",
	"1:0.8055",
	"1:0.8437 (16:9 Anisomorphic NTSC for 720x480/352x240 images)",
	"1:0.8935",
	"1:0.9375 (4:3 PAL/SECAM for 720x578/352x288 images)",
	"1:0.9815",
	"1:1.0255",
	"1:1:0695",
	"1:1.125  (4:3 NTSC for 720x480/352x240 images)",
	"1:1575",
	"1:1.2015"
};

static char *mpeg2_aspect_ratio_definitions[] = 
{
	"1  - 1:1 display",
	"2  - 4:3 display",
	"3  - 16:9 display",
	"4  - 2.21:1 display"
};

static char **aspect_ratio_definitions[2] = 
{
	mpeg1_aspect_ratio_definitions,
	mpeg2_aspect_ratio_definitions
};

static const int num_aspect_ratios[2] = 
{
	sizeof(mpeg1_aspect_ratio_definitions)/sizeof(char *),
     sizeof(mpeg2_aspect_ratio_definitions)/sizeof(char *)
};

static void DisplayFrameRates()
{
 	int i;
	printf("Frame-rate codes:\n");
	for( i = 0; i < num_frame_rates; ++i )
	{
		printf( "%2d - %s\n", i, frame_rate_definitions[i]);
	}
	exit(0);
}

static void DisplayAspectRatios()
{
 	int i;
	printf("MPEG1 pixel aspect ratio codes:\n");
	for( i = 0; i < num_aspect_ratios[0]; ++i )
	{
		printf( "%2d - %s\n", i+1, aspect_ratio_definitions[0][i]);
	}
	printf("\nMPEG2 display aspect ratio codes:\n");
	for( i = 0; i < num_aspect_ratios[1]; ++i )
	{
		printf( "%2d - %s\n", i+1, aspect_ratio_definitions[1][i]);
	}
	exit(0);
}

static void Usage(char *str)
{
	printf("mjpegtools mpeg2enc version " VERSION "\n" );
	printf("Usage: %s [params]\n",str);
	printf("   where possible params are:\n");
	printf("   -m num     MPEG level (1 or 2) default: 1\n");
	printf("   -a num     Aspect ratio displayed image [1..14] (default: code for 4:3 in specified norm)\n" );
	printf("              0 - Display MPEG1 and MPEG2 aspect ratio code tables");
	printf("   -f num     Frame rate for encoded video (default: frame rate of input stream)\n");
	printf("   -b num     Bitrate in KBit/sec (default: 1152 KBit/s for VCD)\n");
	printf("   -q num     Quality factor [1..31] (1 is best, no default)\n");
	printf("              Bitrate and Quality are mutually exclusive!\n");
	printf("   -o name    Outputfile name (REQUIRED!!!)\n");
	printf("   -F num     only for MPEG 2 output:\n");
	printf("               0 = progressive output (no field pictures)\n");
	printf("               1 = field pictures, bottom field first\n");
	printf("               2 = field pictures, top field first\n");
	printf("               3 = progressive output, field MC and MDCT\n");
	printf("   -r num     Search radius for motion compensation [0..32] (default 16)\n");
	printf("   -4 num     (default: 2)\n");
	printf("   			  Population halving passes 4*4-pel subsampled motion compensation\n" );
	printf("   -2 num     (default: 3)\n");
	printf("   			  Population halving passes 2*2-pel subsampled motion compensation\n" );
	printf("   -g num     Minimum GOP size (default 12)\n" );
	printf("   -G num     Maximum GOP size (default 12)\n" );
	printf("   -M num     Optimise threading for num CPU's (default: 1)\n");
	printf("   -Q num     Amount quantisation of highly active blocks is reduced by [0.1 .. 10] (default: 2.5)\n");
	printf("   -v num     Target video buffer size in KB (default 46)\n");
	printf("   -n n|p|s   Force video norm (NTSC, PAL, SECAM) (default: PAL).\n");
	printf("   -S num     Start a new sequence in the output every num Mbytes\n");
	printf("   -s         Generate a sequence header for every GOP rather than just for the first GOP\n");
	printf("   -p         Generate header flags for 32 pull down of 24fps movie.");
	printf("   -t         Activate dynamic thresholding of motion compensation window size\n" );
	printf("   -N         Noise filter via quantisation adjustment (experimental)\n" );
	printf("   -h         Maximise high-frequency resolution (useful for high quality sources at high bit-rates)\n" );
	printf("   -?         Print this lot out!\n");
	exit(0);
}


int main(argc,argv)
	int argc;
	char *argv[];
{
	char *outfilename=0;
	int nerr = 0;
	int n;

	while( (n=getopt(argc,argv,"m:a:f:n:b:q:o:S:F:r:M:4:2:Q:g:G:v:stpNhO?")) != EOF)
	{
		switch(n) {

		case 'm':
			if(strlen(optarg)!=1 || (optarg[0]!='1' && optarg[0]!='2') )
			{
				fprintf(stderr,"-m option requires arg 1 or 2\n");
				nerr++;
			}
			param_mpeg = optarg[0]-'0';
			break;

		case 'b':
			param_bitrate = atoi(optarg);
			break;

		case 'q':
			param_quant = atoi(optarg);
			if(param_quant<1 || param_quant>16)
			{
				fprintf(stderr,"-q option requires arg 1 .. 16\n");
				nerr++;
			}
			break;

        case 'a' :
			param_aspect_ratio = atoi(optarg);
            if( param_aspect_ratio == 0 )
				DisplayAspectRatios();
			/* Checking has to come later once MPEG 1/2 has been selected...*/
			if( param_aspect_ratio < 0 )
			{
				fprintf( stderr, "-a option must be positive\n");
				nerr++;
			}
			break;

       case 'f' :
			param_frame_rate = atoi(optarg);
            if( param_frame_rate == 0 )
				DisplayFrameRates();
			if( param_frame_rate < 0 || param_frame_rate > num_frame_rates-1)
			{
				fprintf( stderr, "-f option must be [0..%d]\n", 
						 num_frame_rates-1);
				nerr++;
			}
			break;

		case 'o':
			outfilename = optarg;
			break;

		case 'F':
			param_fieldpic = atoi(optarg);
			if(param_fieldpic<0 || param_fieldpic>3)
			{
				fprintf(stderr,"-F option requires 0 ... 3\n");
				nerr++;
			}
			break;

		case 'r':
			param_searchrad = atoi(optarg);
			if(param_searchrad<0 || param_searchrad>32)
			{
				fprintf(stderr,"-r option requires arg 0 .. 32\n");
				nerr++;
			}
			break;

		case 'M':
			param_num_cpus = atoi(optarg);
			if(param_num_cpus<1 || param_num_cpus>32)
			{
				fprintf(stderr,"-M option requires arg 1..32\n");
				nerr++;
			}
			break;

		case '4':
			param_44_red = atoi(optarg);
			if(param_44_red<0 || param_44_red>4)
			{
				fprintf(stderr,"-4 option requires arg 0..4\n");
				nerr++;
			}
			break;
			
		case '2':
			param_22_red = atoi(optarg);
			if(param_22_red<0 || param_22_red>4)
			{
				fprintf(stderr,"-2 option requires arg 0..4\n");
				nerr++;
			}
			break;

		case 'v':
			param_video_buffer_size = atoi(optarg);
			if(param_video_buffer_size<20 || param_video_buffer_size>4000)
			{
				fprintf(stderr,"-v option requires arg 20..4000\n");
				nerr++;
			}
			break;

		case 'S' :
			param_seq_length_limit = atoi(optarg);
			if(param_seq_length_limit<1 )
			{
				fprintf(stderr,"-S option requires arg > 1\n");
				nerr++;
			}
			break;
		case 'p' :
			param_32_pulldown = 1;
			break;
		case 's' :
			param_seq_hdr_every_gop = 1;
			break;

		case 'n' :
			switch( optarg[0] )
			{
			case 'p' :
			case 'n' :
			case 's' :
				param_norm = optarg[0];
				break;
			default :
				fprintf(stderr,"-n option requires arg n or p, or s.\n");
				++nerr;
			}

		case 'g' :
			param_min_GOP_size = atoi(optarg);
			break;
		case 'G' :
			param_max_GOP_size = atoi(optarg);
			break;
		case 'N':
			param_hfnoise_quant = 1;
			break;
		case 'h':
			param_hires_quant = 1;
			break;
		case 'Q' :
			param_act_boost = atof(optarg);
			if( param_act_boost <0.1 || param_act_boost > 10.0)
			{
				fprintf( stderr, "-q option requires arg 0.1 .. 10.0\n");
				nerr++;
			}
			break;
		case '?':
		default:
			nerr++;
		}
	}

    if( nerr )
		Usage(argv[0]);

	/* Select input stream */
	if(optind!=argc)
	{
		if( optind == argc-1 )
		{
			input_fd = open( argv[optind], O_RDONLY );
			if( input_fd < 0 )
			{
				fprintf( stderr, "Unable to open: %s: ",argv[optind] );
				perror("");
				nerr++;
			}
		}
		else
			nerr++;
	}
	else
		input_fd = 0; /* stdin */

	/* Read parameters inferred from input stream */
	read_stream_params( &horizontal_size, &vertical_size, &frame_rate_code );

	if(horizontal_size<=0)
	{
		fprintf(stderr,"Horizontal size from input stream illegal\n");
		nerr++;
	}
	if(vertical_size<=0)
	{
		fprintf(stderr,"Vertical size from input stream illegal\n");
		nerr++;
	}

	
	/* Infer norm, aspect ratios and frame_rate if not specified */
	if( param_norm == 0 && (frame_rate_code==3 || frame_rate_code == 2) )
	{
		fprintf(stderr,"Assuming norm PAL\n");
		param_norm = 'p';
	}
	if( param_norm == 0 && (frame_rate_code==4 || frame_rate_code == 1) )
	{
		fprintf(stderr,"Assuming norm NTSC\n");
		param_norm = 'n';
	}

	if( param_frame_rate != 0 )
	{
		if( frame_rate_code != param_frame_rate && 
			frame_rate_code > 0 && frame_rate_code < num_frame_rates )
		{
			fprintf(stderr, "+++ WARNING: Specified output frame-rate %3.2f will over-ride\n", framerates[param_frame_rate]);
			fprintf(stderr, "+++ WARNING: (different!) frame-rate %3.2f of the input stream\n", framerates[frame_rate_code]);
		}
		frame_rate_code = param_frame_rate;
	}

	if( param_32_pulldown )
	{
		if( frame_rate_code != 1 && frame_rate_code != 2  )
		{
			fprintf( stderr, "3:2 pulldown only sensible for 2400/1001 or 24fps output stream\n" );
			exit(1);
		}
		if( param_fieldpic != 0 )
		{
			fprintf( stderr, "3:2 pulldown only sensible for Frame pictures\n");
			exit(1);
		}
	}
	
	if( param_aspect_ratio == 0 )
	{
		if( param_norm == 'p' || param_norm == 's' )
		{
			param_aspect_ratio = param_mpeg == 1 ? 8 : 1;
		}
	    else if( param_norm == 'n' )
		{
			param_aspect_ratio = param_mpeg == 1 ? 12 : 1;
		}
		else
		{
			fprintf( stderr, "No default aspect ratio if norm unspecified!\n");
			nerr++;
		}
	}

	/* Check parameters that cannot be checked when parsed/read */

	if(!outfilename)
	{
		fprintf(stderr,"Output file name (-o option) is required!\n");
		nerr++;
	}

	if(frame_rate_code<1 || frame_rate_code>8)
	{
		fprintf(stderr,"Frame rate code from input stream cannot be interpreted !!!!\n");
		nerr++;
	}



	if(  param_aspect_ratio > num_aspect_ratios[param_mpeg-1] ) 
	{
		fprintf( stderr,"For MPEG-%d aspect ratio codes > %d illegal\n", 
				 param_mpeg, num_aspect_ratios[param_mpeg-1]);
		nerr++;
	}
		


	if( param_min_GOP_size > param_max_GOP_size )
	{
		fprintf(stderr, "Min GOP size must be <= Max GOP size\n" );
		++nerr;
	}

	if(	param_min_GOP_size < 2*param_Bgrp_size ||
		param_max_GOP_size+param_max_GOP_size+param_Bgrp_size+1 > 
		FRAME_BUFFER_SIZE-READ_CHUNK_SIZE  )
	{
		fprintf(stderr, 
				"Min and max GOP sizes must be in range [%d..%d]\n",
				2*param_Bgrp_size,
				(FRAME_BUFFER_SIZE-READ_CHUNK_SIZE-1-param_Bgrp_size)/2);
		++nerr;
	}



	if(nerr) Usage(argv[0]);

	if( param_quant )
	{
		quant_floor = (double)param_quant;
	}
	else
	{
		quant_floor = 0.0;		/* Larger than max quantisation */
	}
	act_boost = (param_act_boost+1.0);
  

	if(nerr) exit(1);


	printf("\nEncoding MPEG-%d video to %s\n",param_mpeg,outfilename);
	fprintf(stderr,"Horizontal size: %d pel\n",horizontal_size);
	fprintf(stderr,"Vertical size: %d pel\n",vertical_size);
	fprintf(stderr,"Aspect ratio code: %d = %s\n", 
			param_aspect_ratio,
			aspect_ratio_definitions[param_mpeg-1][param_aspect_ratio-1]);
	fprintf(stderr,"Frame rate code:   %d = %s\n",
			frame_rate_code,
			frame_rate_definitions[frame_rate_code]);

	if(param_bitrate) 
		printf("Bitrate: %d KBit/s\n",param_bitrate);
	else
		printf( "Bitrate: VCD\n");
	if(param_quant) 
		printf("Quality factor: %d (1=best, 31=worst)\n",param_quant);
	
	if(param_seq_hdr_every_gop ) 
		printf("Sequence header for every GOP\n");
	else 
		printf( "Sequence header only for first GOP\n");
		
	if( param_seq_length_limit )
		printf( "New Sequence every %d Mbytes\n", param_seq_length_limit );
	else
		printf( "Sequence unlimited length\n" );

	printf("Search radius: %d\n",param_searchrad);

	/* set params */
	init_encoding_parms();

	/* read quantization matrices */
	init_quantmat();

	/* open output file */
	if (!(outfile=fopen(outfilename,"wb")))
	{
		sprintf(errortext,"Couldn't create output file %s",outfilename);
		error(errortext);
	}

	init();
	init_quantizer();
	init_motion();
	init_transform();
	init_predict();
	putseq();

	fclose(outfile);
	fclose(statfile);

	return 0;
}

/*
	Wrapper for malloc that allocates pbuffers aligned to the 
	specified byte boundary and checks for failure.
	N.b.  don't try to free the resulting pointers, eh...
	BUG: 	Of course this won't work if a char * won't fit in an int....
*/
uint8_t *bufalloc( size_t size )
{
	char *buf = malloc( size + BUFFER_ALIGN );
	int adjust;

	if( buf == NULL )
	{
		error("malloc failed\n");
	}
	adjust = BUFFER_ALIGN-((int)buf)%BUFFER_ALIGN;
	if( adjust == BUFFER_ALIGN )
		adjust = 0;
	return (uint8_t*)(buf+adjust);
}

static void init()
{
	int i, n;
	static int block_count_tab[3] = {6,8,12};

	initbits(); 
	init_fdct();
	init_idct();

	/* Tune threading for specified number of CPU's 
	   Currently pretty crude...
	 */
	
	switch( param_num_cpus )
	{
	case 1 :
		max_encoding_frames = 1;
		break;
	case 2:
		max_encoding_frames = 2;
		break;
	default :
		/* Note: if this is ever made variable it must be kept less
		   on MAX_WORKER_THREADS */
		max_encoding_frames = 3;
		break;
	}


	/* round picture dimensions to nearest multiple of 16 or 32 */
	mb_width = (horizontal_size+15)/16;
	mb_height = prog_seq ? (vertical_size+15)/16 : 2*((vertical_size+31)/32);
	mb_height2 = fieldpic ? mb_height>>1 : mb_height; /* for field pictures */
	width = 16*mb_width;
	height = 16*mb_height;

	/* Calculate the sizes and offsets in to luminance and chrominance
	   buffers.  A.Stevens 2000 for luminance data we allow space for
	   fast motion estimation data.  This is actually 2*2 pixel
	   sub-sampled uint8_t followed by 4*4 sub-sampled.  We add an
	   extra row to act as a margin to allow us to neglect / postpone
	   edge condition checking in time-critical loops...  */

	chrom_width = (chroma_format==CHROMA444) ? width : width>>1;
	chrom_height = (chroma_format!=CHROMA420) ? height : height>>1;



	height2 = fieldpic ? height>>1 : height;
	width2 = fieldpic ? width<<1 : width;
	chrom_width2 = fieldpic ? chrom_width<<1 : chrom_width;
 
	block_count = block_count_tab[chroma_format-1];
	lum_buffer_size = (width*height) + 
					 sizeof(uint8_t) *(width/2)*(height/2) +
					 sizeof(uint8_t) *(width/4)*(height/4+1);
	chrom_buffer_size = chrom_width*chrom_height;


	fsubsample_offset = (width)*(height) * sizeof(uint8_t);
	qsubsample_offset =  fsubsample_offset + (width/2)*(height/2)*sizeof(uint8_t);

	mb_per_pict = mb_width*mb_height2;


	/* clip table */
	if (!(clp = (uint8_t *)malloc(1024)))
		error("malloc failed\n");
	clp+= 384;
	for (i=-384; i<640; i++)
		clp[i] = (i<0) ? 0 : ((i>255) ? 255 : i);
	
	/* Allocate the frame data buffers */

	frame_buffers = (uint8_t ***) 
		bufalloc(FRAME_BUFFER_SIZE*sizeof(uint8_t**));
	
	for(n=0;n<FRAME_BUFFER_SIZE;n++)
	{
         frame_buffers[n] = (uint8_t **) bufalloc(3*sizeof(uint8_t*));
		 for (i=0; i<3; i++)
		 {
			 frame_buffers[n][i] = 
				 bufalloc( (i==0) ? lum_buffer_size : chrom_buffer_size );
		 }
	}

	/* TODO: The ref and aux frame buffers are not redundant! */
	for( i = 0 ; i<3; i++)
	{
		int size =  (i==0) ? lum_buffer_size : chrom_buffer_size;
		newrefframe[i] = bufalloc(size);
		oldrefframe[i] = bufalloc(size);
		auxframe[i]    = bufalloc(size);
		predframe[i]   = bufalloc(size);
	}

  
	/* open statistics output file */
	if (statname[0]=='-')
		statfile = stdout;
	else if (!(statfile = fopen(statname,"w")))
	{
		sprintf(errortext,"Couldn't create statistics output file %s",statname);
		error(errortext);
	}


}

void error(text)
	char *text;
{
	fprintf(stderr,text);
	putc('\n',stderr);
	exit(1);
}

#define MAX(a,b) ( (a)>(b) ? (a) : (b) )

static void init_encoding_parms(void)
{
	int i;
	int c;

	sprintf(id_string,"Converted by mjpegtools mpeg2enc 1.3");
	strcpy(tplorg,"%8d"); /* Name of input files */
	strcpy(tplref,"-");
	strcpy(iqname,"-");
	strcpy(niqname,"-");

	inputtype = 0;  /* doesnt matter */
	nframes = 999999999; /* determined by EOF of stdin */

	N_min = param_min_GOP_size;      /* I frame distance */
	N_max = param_max_GOP_size;
	M = param_Bgrp_size;             /* I or P frame distance */
	mpeg1           = (param_mpeg == 1);
	fieldpic        = (param_fieldpic!=0 && param_fieldpic != 3);
	pulldown_32     = param_32_pulldown;

	aspectratio     = param_aspect_ratio;
	dctsatlim		= mpeg1 ? 255 : 2047;

	/* If we're using a non standard (VCD?) profile bit-rate adjust	the vbv
		buffer accordingly... */
#ifdef OUTPUT_STAT
	strcpy(statname, mpeg1 ? "mp1stats.log" : "mp2stats.log" );
#else
	strcpy(statname,"-");
#endif

	if( mpeg1 )
	{
		/* Handle the default VCD case... or scale accordingly */
		if( param_bitrate == 0 && param_quant == 0 )
		{
			bit_rate = 1151929;	/* Weird and wonderful max rate specified  */
								/* No, it's *not* 1152Kbps ... VCD bleah!! */
			vbv_buffer_code = 20;	/* = 40KB */
		}
		else
		{
			bit_rate = MAX(10000, param_bitrate*1000);
			vbv_buffer_code = (20 * param_bitrate  / 1152);
		}
	}
	else
	{
		if( param_bitrate == 0 )
		{
			fprintf(stderr, "MPEG-2 specified - must specify bit-rate!\n" );
			exit(1);
		}
		bit_rate = MAX(10000, param_bitrate*1000);
		vbv_buffer_code = 112;
	}
	vbv_buffer_size = vbv_buffer_code*16384;
	
	mc_44_red		= param_44_red;
	mc_22_red		= param_22_red;
	video_buffer_size = param_video_buffer_size * 1024 * 8;
	
	seq_header_every_gop = param_seq_hdr_every_gop;
	seq_length_limit = param_seq_length_limit;
	low_delay       = 0;
	constrparms     = mpeg1;       /* Will be reset, if not coompliant */
	constrparms = 1;
	profile         = param_422 ? 1 : 4; /* High or Main profile resp. */
	level           = 8;                 /* Main Level      CCIR 601 rates */
	prog_seq        = (param_fieldpic==0);
	chroma_format   = param_422 ? CHROMA422 : CHROMA420;
	switch(param_norm)
	{
	case 'p': video_format = 1; break;
	case 'n': video_format = 2; break;
	case 's': video_format = 3; break;
	default:  video_format = 5; break; /* unspec. */
	}
	color_primaries = 5;
	transfer_characteristics = 5;
	switch(param_norm)
	{
	case 'p': matrix_coefficients = 5; break;
	case 'n': matrix_coefficients = 4; break;
	case 's': matrix_coefficients = 5; break; /* ???? */
	default:  matrix_coefficients = 2; break; /* unspec. */
	}
	display_horizontal_size  = horizontal_size;
	display_vertical_size    = vertical_size;

	opt_dc_prec         = 0;  /* 8 bits */
	opt_topfirst        = (param_fieldpic==2) || (param_fieldpic == 3);
	opt_repeatfirst     = 0;
	opt_prog_frame      = (param_fieldpic==0) || (param_fieldpic == 3);

	frame_pred_dct_tab[0] = mpeg1 ? 1 : (param_fieldpic == 0);
	frame_pred_dct_tab[1] = mpeg1 ? 1 : (param_fieldpic == 0);
	frame_pred_dct_tab[2] = mpeg1 ? 1 : (param_fieldpic == 0);

	conceal_tab[0]  = 0; /* not implemented */
	conceal_tab[1]  = 0;
	conceal_tab[2]  = 0;

	qscale_tab[0]   = mpeg1 ? 0 : 1;
	qscale_tab[1]   = mpeg1 ? 0 : 1;
	qscale_tab[2]   = mpeg1 ? 0 : 1;

	intravlc_tab[0] = mpeg1 ? 0 : 1;
	intravlc_tab[1] = mpeg1 ? 0 : 1;
	intravlc_tab[2] = mpeg1 ? 0 : 1;

	intravlc_tab[0] = 0;
	intravlc_tab[1] = 0;
	intravlc_tab[2] = 0;

	altscan_tab[0]  = mpeg1 ? 0 : (fieldpic!=0);
	altscan_tab[1]  = mpeg1 ? 0 : (fieldpic!=0);
	altscan_tab[2]  = mpeg1 ? 0 : (fieldpic!=0);

	altscan_tab[0]  = 0;
	altscan_tab[1]  = 0;
	altscan_tab[2]  = 0;



	/*  A.Stevens 2000: The search radius *has* to be a multiple of 8
		for the new fast motion compensation search to work correctly.
		We simply round it up if needs be.  */

	if(param_searchrad*M>127)
	{
		param_searchrad = 127/M;
		fprintf(stderr,"Search radius reduced to %d\n",param_searchrad);
	}
	
	{ 
		int radius_x = ((param_searchrad+4)/8)*8;
		int radius_y = ((param_searchrad*vertical_size/horizontal_size+4)/8)*8;

		/* TODO: These f-codes should really be adjusted for each
		   picture type... */
		c=5;
		if( radius_x*M < 64) c = 4;
		if( radius_x*M < 32) c = 3;
		if( radius_x*M < 16) c = 2;
		if( radius_x*M < 8) c = 1;

		motion_data = (struct motion_data *)malloc(M*sizeof(struct motion_data));
		if (!motion_data)
			error("malloc failed\n");

		for (i=0; i<M; i++)
		{
			if(i==0)
			{
				motion_data[i].forw_hor_f_code  = c;
				motion_data[i].forw_vert_f_code = c;
				motion_data[i].sxf = MAX(1,radius_x*M);
				motion_data[i].syf = MAX(1,radius_y*M);
			}
			else
			{
				motion_data[i].forw_hor_f_code  = c;
				motion_data[i].forw_vert_f_code = c;
				motion_data[i].sxf = MAX(1,radius_x*i);
				motion_data[i].syf = MAX(1,radius_y*i);
				motion_data[i].back_hor_f_code  = c;
				motion_data[i].back_vert_f_code = c;
				motion_data[i].sxb = MAX(1,radius_x*(M-i));
				motion_data[i].syb = MAX(1,radius_y*(M-i));
			}
		}
		
	}
	/* make flags boolean (x!=0 -> x=1) */
	mpeg1 = !!mpeg1;
	fieldpic = !!fieldpic;
	low_delay = !!low_delay;
	constrparms = !!constrparms;
	prog_seq = !!prog_seq;
	opt_topfirst = !!opt_topfirst;

	for (i=0; i<3; i++)
	{
		frame_pred_dct_tab[i] = !!frame_pred_dct_tab[i];
		conceal_tab[i] = !!conceal_tab[i];
		qscale_tab[i] = !!qscale_tab[i];
		intravlc_tab[i] = !!intravlc_tab[i];
		altscan_tab[i] = !!altscan_tab[i];
	}

	/* make sure MPEG specific parameters are valid */
	range_checks();

	frame_rate = framerates[frame_rate_code];


	if ( !mpeg1)
	{
		profile_and_level_checks();
	}
	else
	{
		/* MPEG-1 */
		if (constrparms)
		{
			if (horizontal_size>768
				|| vertical_size>576
				|| ((horizontal_size+15)/16)*((vertical_size+15)/16)>396
				|| ((horizontal_size+15)/16)*((vertical_size+15)/16)*frame_rate>396*25.0
				|| frame_rate>30.0)
			{
				if (!quiet)
					fprintf(stderr,"Warning: size - setting constrained_parameters_flag = 0\n");
				constrparms = 0;
			}
		}

		if (constrparms)
		{
			for (i=0; i<M; i++)
			{
				if (motion_data[i].forw_hor_f_code>4)
				{
					if (!quiet)
						fprintf(stderr,"Warning: hor motion search setting constrained_parameters_flag = 0\n");
					constrparms = 0;
					break;
				}

				if (motion_data[i].forw_vert_f_code>4)
				{
					if (!quiet)
						fprintf(stderr,"Warning: ver motion search setting constrained_parameters_flag = 0\n");
					constrparms = 0;
					break;
				}

				if (i!=0)
				{
					if (motion_data[i].back_hor_f_code>4)
					{
						if (!quiet)
							fprintf(stderr,"Warning: hor motion search setting constrained_parameters_flag = 0\n");
						constrparms = 0;
						break;
					}

					if (motion_data[i].back_vert_f_code>4)
					{
						if (!quiet)
							fprintf(stderr,"Warning: ver motion search setting constrained_parameters_flag = 0\n");
						constrparms = 0;
						break;
					}
				}
			}
		}
	}

	/* relational checks */
	if ( mpeg1 )
	{
		if (!prog_seq)
		{
			if (!quiet)
				fprintf(stderr,"Warning: mpeg1 - setting progressive_sequence = 1\n");
			prog_seq = 1;
		}

		if (chroma_format!=CHROMA420)
		{
			if (!quiet)
				fprintf(stderr,"Warning: mpeg1 - forcing chroma_format = 1 (4:2:0) - others not supported\n");
			chroma_format = CHROMA420;
		}

		if (opt_dc_prec!=0)
		{
			if (!quiet)
				fprintf(stderr,"Warning: mpeg1 - setting intra_dc_precision = 0\n");
			opt_dc_prec = 0;
		}

		for (i=0; i<3; i++)
			if (qscale_tab[i])
			{
				if (!quiet)
					fprintf(stderr,"Warning: mpeg1 - setting qscale_tab[%d] = 0\n",i);
				qscale_tab[i] = 0;
			}

		for (i=0; i<3; i++)
			if (intravlc_tab[i])
			{
				if (!quiet)
					fprintf(stderr,"Warning: mpeg1 - setting intravlc_tab[%d] = 0\n",i);
				intravlc_tab[i] = 0;
			}

		for (i=0; i<3; i++)
			if (altscan_tab[i])
			{
				if (!quiet)
					fprintf(stderr,"Warning: mpeg1 - setting altscan_tab[%d] = 0\n",i);
				altscan_tab[i] = 0;
			}
	}

	if ( !mpeg1 && constrparms)
	{
		if (!quiet)
			fprintf(stderr,"Warning: not mpeg1 - setting constrained_parameters_flag = 0\n");
		constrparms = 0;
	}

	if (prog_seq && !opt_prog_frame)
	{
		if (!quiet)
			fprintf(stderr,"Warning: prog sequence - setting progressive_frame = 1\n");
		opt_prog_frame = 1;
	}

	if (opt_prog_frame && fieldpic)
	{
		if (!quiet)
			fprintf(stderr,"Warning: prog frame - setting field_pictures = 0\n");
		fieldpic = 0;
	}

	if (!opt_prog_frame && opt_repeatfirst)
	{
		if (!quiet)
			fprintf(stderr,"Warning: not prog frame setting repeat_first_field = 0\n");
		opt_repeatfirst = 0;
	}

	if (prog_seq && !opt_repeatfirst && opt_topfirst)
	{
		if (!quiet)
			fprintf(stderr,"Warning: prog sequence setting top_field_first = 0\n");
		opt_topfirst = 0;
	}

	/* search windows */
	for (i=0; i<M; i++)
	{
		if (motion_data[i].sxf > (4<<motion_data[i].forw_hor_f_code)-1)
		{
			if (!quiet)
				fprintf(stderr,
						"Warning: reducing forward horizontal search width to %d\n",
						(4<<motion_data[i].forw_hor_f_code)-1);
			motion_data[i].sxf = (4<<motion_data[i].forw_hor_f_code)-1;
		}

		if (motion_data[i].syf > (4<<motion_data[i].forw_vert_f_code)-1)
		{
			if (!quiet)
				fprintf(stderr,
						"Warning: reducing forward vertical search width to %d\n",
						(4<<motion_data[i].forw_vert_f_code)-1);
			motion_data[i].syf = (4<<motion_data[i].forw_vert_f_code)-1;
		}

		if (i!=0)
		{
			if (motion_data[i].sxb > (4<<motion_data[i].back_hor_f_code)-1)
			{
				if (!quiet)
					fprintf(stderr,
							"Warning: reducing backward horizontal search width to %d\n",
							(4<<motion_data[i].back_hor_f_code)-1);
				motion_data[i].sxb = (4<<motion_data[i].back_hor_f_code)-1;
			}

			if (motion_data[i].syb > (4<<motion_data[i].back_vert_f_code)-1)
			{
				if (!quiet)
					fprintf(stderr,
							"Warning: reducing backward vertical search width to %d\n",
							(4<<motion_data[i].back_vert_f_code)-1);
				motion_data[i].syb = (4<<motion_data[i].back_vert_f_code)-1;
			}
		}
	}

}

/*
  If the use has selected suppression of hf noise via
  quantisation then we boost quantisation of hf components
  EXPERIMENTAL: currently a linear ramp from 0 at 4pel to 
  50% increased quantisation...
*/

static int quant_hfnoise_filt(int orgquant, int qmat_pos )
{
	int x = qmat_pos % 8;
	int y = qmat_pos / 8;
	int qboost = 1024;

	if( ! param_hfnoise_quant )
	{
		return orgquant;
	}

	/* Maximum 50% quantisation boost for HF components... */
	if( x > 4 )
		qboost += (256*(x-4)/3);
	if( y > 4 )
		qboost += (256*(y-4)/3);

	return (orgquant * qboost + 512)/ 1024;
}

static void init_quantmat()
{
	int i,v, q;
	FILE *fd;
	load_iquant = 0;
	load_niquant = 0;
	if (iqname[0]=='-')
	{
		if( param_hires_quant )
		{
			load_iquant |= 1;
			printf( "Setting hi-res intra Quantisation matrix\n" );
			for (i=0; i<64; i++)
			{
				intra_q[i] = hires_intra_quantizer_matrix[i];
			}	
		}
		else
		{
			/* use default intra matrix */
			load_iquant = param_hfnoise_quant;
			for (i=0; i<64; i++)
			{
				v = quant_hfnoise_filt( default_intra_quantizer_matrix[i], i);
				if (v<1 || v>255)
					error("value in intra quant matrix invalid (after noise filt adjust)");
				intra_q[i] = v;

			} 
		}
	}
	else
	{
		/* read customized intra matrix */
		load_iquant = 1;
		if (!(fd = fopen(iqname,"r")))
		{
			sprintf(errortext,"Couldn't open quant matrix file %s",iqname);
			error(errortext);
		}

		for (i=0; i<64; i++)
		{
			fscanf(fd,"%d",&v);
			v = quant_hfnoise_filt(v,i);
			if (v<1 || v>255)
				error("value in intra quant matrix invalid (after noise filt adjust)");

			intra_q[i] = v;
		}

		fclose(fd);
	}

	/* TODO: Inv Quant matrix initialisation should check if the fraction fits in 16 bits! */
	if (niqname[0]=='-')
	{

		if( param_hires_quant )
		{
			printf( "Setting hi-res non-intra quantiser matrix\n" );
			for (i=0; i<64; i++)
			{
				inter_q[i] = hires_nonintra_quantizer_matrix[i];
			}	
		}
		else
		{
			/* default non-intra matrix is all 16's. For *our* default we use something
			   more suitable for domestic analog sources... which is non-standard...*/
			load_niquant |= 1;
			for (i=0; i<64; i++)
			{
				v = quant_hfnoise_filt(default_nonintra_quantizer_matrix[i],i);
				if (v<1 || v>255)
					error("value in non-intra quant matrix invalid (after noise filt adjust)");
				inter_q[i] = v;
			}
		}
	}
	else
	{
		/* read customized non-intra matrix */
		load_niquant = 1;
		if (!(fd = fopen(niqname,"r")))
		{
			sprintf(errortext,"Couldn't open quant matrix file %s",niqname);
			error(errortext);
		}

		for (i=0; i<64; i++)
		{
			fscanf(fd,"%d",&v);
			v = quant_hfnoise_filt(v,i);
			inter_q[i] = v;
			if (v<1 || v>255)
				error("value in non-intra quant matrix invalid (after noise filt adjust)");
			i_inter_q[i] = (int)(((double)IQUANT_SCALE) / ((double)v)); 
		}
		fclose(fd);
	}
  
	for (i=0; i<64; i++)
	{
		i_intra_q[i] = (int)(((double)IQUANT_SCALE) / ((double)intra_q[i]));
		i_inter_q[i] = (int)(((double)IQUANT_SCALE) / ((double)inter_q[i]));
	}
	
	for( q = 1; q <= 112; ++q )
	{
		for (i=0; i<64; i++)
		{
			intra_q_tbl[q][i] = intra_q[i] * q;
			inter_q_tbl[q][i] = inter_q[i] * q;
			intra_q_tblf[q][i] = (float)intra_q_tbl[q][i];
			inter_q_tblf[q][i] = (float)inter_q_tbl[q][i];
			i_intra_q_tblf[q][i] = 1.0f/ ( intra_q_tblf[q][i] * 0.98);
			i_intra_q_tbl[q][i] = (IQUANT_SCALE/intra_q_tbl[q][i]);
			i_inter_q_tblf[q][i] =  1.0f/ (inter_q_tblf[q][i] * 0.98);
			i_inter_q_tbl[q][i] = (IQUANT_SCALE/inter_q_tbl[q][i] );
		}
	}
  
}
