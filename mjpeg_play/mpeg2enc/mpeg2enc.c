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

/*
 *
 * Modifications to speed up motion compensation (c) 2000 Andrew Stevens
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define GLOBAL /* used by global.h */
#include "config.h"
#include "global.h"

int verbose = 2;

/* private prototypes */
static void init _ANSI_ARGS_((void));
static void readparmfile();
static void readquantmat _ANSI_ARGS_((void));

/*
 * RJ: Introduced fix_mquant which is a quantization quality
 * parameter to circumvent the use of fixed bitrates only.
 */

int fix_mquant = 0; /* use fixed quant, range 1 ... 31 */

/*
 * RJ: Introduced the possiblity to not search at all.
 * Saves time at the cost of quality
 */

int do_not_search = 0;

/* RJ: Parameters for creation of the MPEG file */

static int param_bitrate    = 0;
static int param_quant      = 0;
static int param_searchrad  = 0;
static int param_mpeg       = 1;
static int param_fieldpic   = 0;  /* 0: progressive, 1: bottom first, 2: top first */
static int param_norm       = 0;  /* 'n': NTSC, 'p': PAL, 's': SECAM, else unspecified */
static int param_drop_lsb   = 0;
static int param_noise_filt = 0;
static int param_fastmc     = 10;
static int param_threshold  = 0;
static int param_hfnoise_quant = 0;
static int param_hires_quant = 0;
static double param_act_boost = 1.0;

static float framerates[] = { 0, 23.976, 24.0, 25.0, 29.970, 30.0, 50.0, 59.940, 60.0 };

/* reserved: for later use */
int param_422 = 0;

void Usage(char *str)
{
  printf("Usage: %s [params]\n",str);
  printf("   where possible params are:\n");
  printf("   -m num     MPEG level (1 or 2) default: 1\n");
  printf("   -b num     Bitrate in KBit/sec (default: 1152 KBit/s)\n");
  printf("   -q num     Quality factor [1..31] (1 is best, no default)\n");
  printf("              Bitrate and Quality are mutually exclusive!\n");
  printf("   -o name    Outputfile name (REQUIRED!!!)\n");
  printf("   -F num     only for MPEG 2 output:\n");
  printf("               0 = progressive output (no field pictures)\n");
  printf("               1 = field pictures, bottom field first\n");
  printf("               2 = field pictures, top field first\n");
  printf("   -r num     Search radius [0..32] (default 0: don\'t search at all)\n");
  printf("   -d num     Drop lsbs of samples [0..3] (default: 0)\n");
  printf("   -n num     Noise filter (low-pass) [0..2] (default: 0)\n");
  printf("   -f num     Fraction of fast motion estimates to consider in detail (1/num) [2..20] (default: 10)\n" );
  printf("   -Q num     Amount quantisation of highly active blocks is reduced by [0.1 .. 10] (default: 2.5)");
  printf("   -t         Activate dynamic thresholding of motion compensation window size\n" );
  printf("   -N         Noise filter via quantisation adjustment (experimental)\n" );
  printf("   -h         Maximise high-frequency resolution (useful for high quality sources)\n" );
  exit(0);
}

int main(argc,argv)
int argc;
char *argv[];
{
  char *outfilename=0;
  int n, nerr = 0;
#define PARAM_LINE_MAX 256
  char param_line[PARAM_LINE_MAX];

  while( (n=getopt(argc,argv,"m:b:q:o:F:r:f:d:n:Q:tNh")) != EOF)
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
        if(param_quant<1 || param_quant>31)
        {
          fprintf(stderr,"-q option requires arg 1 .. 31\n");
          nerr++;
        }
        break;

      case 'o':
        outfilename = optarg;
        break;

      case 'F':
        param_fieldpic = atoi(optarg);
        if(param_fieldpic<0 || param_fieldpic>2)
        {
          fprintf(stderr,"-F option requires 0, 1 or 2\n");
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

      case 'd':
        param_drop_lsb = atoi(optarg);
        if(param_drop_lsb<0 || param_drop_lsb>3)
        {
          fprintf(stderr,"-d option requires arg 0..3\n");
          nerr++;
        }
		break;
		
      case 'n':
        param_noise_filt = atoi(optarg);
        if(param_noise_filt<0 || param_noise_filt>2)
        {
          fprintf(stderr,"-n option requires arg 0..2\n");
          nerr++;
        }
		break;


      case 'f':
        param_fastmc = atoi(optarg);
        if(param_fastmc<2 || param_fastmc>20)
        {
          fprintf(stderr,"-f option requires arg 2..20\n");
          nerr++;
        }
        break;

	case 't':
	  param_threshold = 1;
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
	default:
	  nerr++;
    }
  }

  if(!outfilename)
  {
    fprintf(stderr,"Output file name (-o option) is required!\n");
    nerr++;
  }

  if(param_bitrate!=0 && param_quant!=0)
  {
    fprintf(stderr,"-b and -q options are mutually exclusive\n");
    nerr++;
  }

  if(optind!=argc) nerr++;

  if(nerr) Usage(argv[0]);

  fix_mquant = param_quant;
  act_boost = param_act_boost;
  
  if(param_bitrate==0 && param_quant==0) param_bitrate = 1152; /* Or 1150? */
  if(param_searchrad==0) do_not_search = 1;

  /* Read stdin until linefeed is seen */

  for(n=0;n<PARAM_LINE_MAX;n++)
  {
    if(!read(0,param_line+n,1))
    {
      fprintf(stderr,"Error reading header from stdin\n");
      exit(1);
    }
    if(param_line[n]=='\n') break;
  }
  if(n==PARAM_LINE_MAX)
  {
    fprintf(stderr,"Didn't get linefeed in first %d characters of data\n",
            PARAM_LINE_MAX);
    exit(1);
  }
  param_line[n] = 0; /* Replace linefeed by end of string */

  if(strncmp(param_line,"YUV4MPEG",8))
  {
    fprintf(stderr,"Input starts not with \"YUV4MPEG\"\n");
    fprintf(stderr,"This is not a valid input for me\n");
    exit(1);
  }


  sscanf(param_line+8,"%d %d %d",&horizontal_size,&vertical_size,&frame_rate_code);

  nerr = 0;
  fprintf(stderr,"Horizontal size: %d\n",horizontal_size);
  if(horizontal_size<=0)
  {
    fprintf(stderr,"Horizontal size illegal\n");
    nerr++;
  }
  fprintf(stderr,"Vertical size: %d\n",vertical_size);
  if(vertical_size<=0)
  {
    fprintf(stderr,"Vertical size size illegal\n");
    nerr++;
  }
  fprintf(stderr,"Frame rate code: %d",frame_rate_code);
  if(frame_rate_code<1 || frame_rate_code>8)
  {
    fprintf(stderr," illegal !!!!\n");
    nerr++;
  }
  else
    fprintf(stderr," = %.3f fps\n",framerates[frame_rate_code]);

  if(nerr) exit(1);

  if(frame_rate_code==3)
  {
    fprintf(stderr,"Assuming norm PAL\n");
    param_norm = 'p';
  }
  if(frame_rate_code==4)
  {
    fprintf(stderr,"Assuming norm NTSC\n");
    param_norm = 'n';
  }

  printf("\nEncoding MPEG-%d video to %s\n",param_mpeg,outfilename);
  if(param_bitrate) printf("Bitrate: %d KBit/s\n",param_bitrate);
  if(param_quant) printf("Quality factor: %d (1=best, 31=worst)\n",param_quant);
  printf("Search radius: %d\n",param_searchrad);

  /* set params */
  readparmfile();

  /* read quantization matrices */
  readquantmat();

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
static unsigned char *bufalloc( size_t size )
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
	return (unsigned char*)(buf+adjust);
}

static void init()
{
  int i, size;
  static int block_count_tab[3] = {6,8,12};

  initbits();
  init_fdct();
  init_idct();

  /* round picture dimensions to nearest multiple of 16 or 32 */
  mb_width = (horizontal_size+15)/16;
  mb_height = prog_seq ? (vertical_size+15)/16 : 2*((vertical_size+31)/32);
  mb_height2 = fieldpic ? mb_height>>1 : mb_height; /* for field pictures */
  width = 16*mb_width;
  height = 16*mb_height;

  chrom_width = (chroma_format==CHROMA444) ? width : width>>1;
  chrom_height = (chroma_format!=CHROMA420) ? height : height>>1;

  height2 = fieldpic ? height>>1 : height;
  width2 = fieldpic ? width<<1 : width;
  chrom_width2 = fieldpic ? chrom_width<<1 : chrom_width;
  
  block_count = block_count_tab[chroma_format-1];

  fsubsample_offset = (width)*(height) * sizeof(unsigned char);
  qsubsample_offset =  fsubsample_offset + (width/2)*(height/2)*sizeof(mcompuint);
#ifdef TEST_RCSEARCH
  rowsums_offset = qsubsample_offset +  (width/4)*(height/4)*sizeof(mcompuint);
  colsums_offset = rowsums_offset + (width+1)*(height+1);
#endif

  /* clip table */
  if (!(clp = (unsigned char *)malloc(1024)))
    error("malloc failed\n");
  clp+= 384;
  for (i=-384; i<640; i++)
    clp[i] = (i<0) ? 0 : ((i>255) ? 255 : i);

  for (i=0; i<3; i++)
  {
	/* A.Stevens 2000 for liminance data we allow space for
	   fast motion estimation data.  This is actually 2*2 pixel sub-sampled
	   mcompuint followed by 4*4 sub-sampled.
	   We add an extra row to act as a margin to allow us to neglect exact
	   postpone condition checking in critical loops...
	*/
	if (i==0)
	  size = (width*height) + 
		sizeof(mcompuint) *(width/2)*(height/2) +
		sizeof(mcompuint) *(width/4)*(height/4+1);
#ifdef TEST_RCSEARCH
		+ sizeof(unsigned short) * (width+1)*(height+1) * 2
#endif		
		
	else
	  size = chrom_width*chrom_height;

    newrefframe[i] = bufalloc(size);
    oldrefframe[i] = bufalloc(size);
 	auxframe[i]    = bufalloc(size);
	neworgframe[i] = bufalloc(size);
 	oldorgframe[i] = bufalloc(size);
 	auxorgframe[i] = bufalloc(size);
	predframe[i]   = bufalloc(size);
  }

  filter_buf =  bufalloc( width*height );

  mbinfo = (struct mbinfo *)bufalloc(mb_width*mb_height2*sizeof(struct mbinfo));

  blocks =
    (short (*)[64])bufalloc(mb_width*mb_height2*block_count*sizeof(short [64]));
  
  qblocks =
    (short (*)[64])bufalloc(mb_width*mb_height2*block_count*sizeof(short [64]));

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

static void readparmfile()
{
  int i;
  int h,m,s,f;
  int c;
  static double ratetab[8]=
    {24000.0/1001.0,24.0,25.0,30000.0/1001.0,30.0,50.0,60000.0/1001.0,60.0};
  extern int r,Xi,Xb,Xp,d0i,d0p,d0b; /* rate control */
  extern double avg_act; /* rate control */

  sprintf(id_string,"Converted by aq2mpeg v0.1");
  strcpy(tplorg,"%8d"); /* Name of input files */
  strcpy(tplref,"-");
  strcpy(iqname,"-");
  strcpy(niqname,"-");
#ifdef OUTPUT_STAT
  strcpy(statname,"stats.log");
#else
  strcpy(statname,"-");
#endif
  inputtype = 0;  /* doesnt matter */
  nframes = 999999999; /* determined by EOF of stdin */
  frame0  = 0;
  h = m = s = f = 0;
  N = 12;      /* I frame distance */
  M = 3;       /* I or P frame distance */
  mpeg1           = param_mpeg == 1;
  fieldpic        = (param_fieldpic!=0);
  /* RJ: horizontal_size, vertical_size, frame_rate_code
         are read from stdin! */
  /* RJ: aspectratio is differently coded for MPEG1 and MPEG2.
   *     For MPEG2 aspect ratio is for total image: 2 means 4:3
   *     For MPEG1 aspect ratio is for the pixels:  1 means square Pixels */
  aspectratio     = mpeg1 ? 1 : 2;
  bit_rate        = MAX(1000,param_bitrate*1000);
  drop_lsb        = param_drop_lsb;
  noise_filt	  = param_noise_filt;
  fast_mc_frac    = param_fastmc;
  fast_mc_threshold = param_threshold;
  vbv_buffer_size = mpeg1 ? 20 : 112;
  low_delay       = 0;
  constrparms     = mpeg1;             /* Will be reset, if not coompliant */
  profile         = param_422 ? 1 : 4; /* High or Main profile resp. */
  level           = 8;                 /* Main Level      CCIR 601 rates */
  prog_seq        = (fieldpic==0);
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
  dc_prec         = 0;  /* 8 bits */
  topfirst        = (param_fieldpic==2);
  frame_pred_dct_tab[0] = mpeg1 ? 1 : 0;
  frame_pred_dct_tab[1] = mpeg1 ? 1 : 0;
  frame_pred_dct_tab[2] = mpeg1 ? 1 : 0;
  conceal_tab[0]  = 0; /* not implemented */
  conceal_tab[1]  = 0;
  conceal_tab[2]  = 0;
  qscale_tab[0]   = mpeg1 ? 0 : 1;
  qscale_tab[1]   = mpeg1 ? 0 : 1;
  qscale_tab[2]   = mpeg1 ? 0 : 1;
  intravlc_tab[0] = mpeg1 ? 0 : 1;
  intravlc_tab[1] = mpeg1 ? 0 : 1;
  intravlc_tab[2] = mpeg1 ? 0 : 1;
  altscan_tab[0]  = mpeg1 ? 0 : (fieldpic!=0);
  altscan_tab[1]  = mpeg1 ? 0 : (fieldpic!=0);
  altscan_tab[2]  = mpeg1 ? 0 : (fieldpic!=0);
  repeatfirst     = 0;
  prog_frame      = prog_seq;
  P       = 0;
  r       = 0;
  avg_act = 0;
  Xi      = 0;
  Xp      = 0;
  Xb      = 0;
  d0i     = 0;
  d0p     = 0;
  d0b     = 0;
  
  if (N<1)
    error("N must be positive");
  if (M<1)
    error("M must be positive");
  if (N%M != 0)
    error("N must be an integer multiple of M");

  motion_data = (struct motion_data *)malloc(M*sizeof(struct motion_data));
  if (!motion_data)
    error("malloc failed\n");

  /*  A.Stevens 2000: The search radius has to be a multiple of 4 for the new fast motion
      compensation search to work correctly.  We simply round it up if needs be.
      */  

  if(param_searchrad*M>127)
  {
     param_searchrad = 127/M;
     fprintf(stderr,"Search radius reduced to %d\n",param_searchrad);
  }
  if( param_searchrad % 4 != 0 )
  {
     param_searchrad = (param_searchrad / 4) * 4;
     fprintf( stderr, "Adjusting search radius to a multiple of 4: %d\n", param_searchrad);
  }
  c = 5;
  if(param_searchrad*M<64) c = 4;
  if(param_searchrad*M<32) c = 3;
  if(param_searchrad*M<16) c = 2;
  if(param_searchrad*M< 8) c = 1;

  for (i=0; i<M; i++)
  {
    if(i==0)
    {
      motion_data[i].forw_hor_f_code  = c;
      motion_data[i].forw_vert_f_code = c;
      motion_data[i].sxf = MAX(1,param_searchrad*M);
      motion_data[i].syf = MAX(1,param_searchrad*M);
    }
    else
    {
      motion_data[i].forw_hor_f_code  = c;
      motion_data[i].forw_vert_f_code = c;
      motion_data[i].sxf = MAX(1,param_searchrad*i);
      motion_data[i].syf = MAX(1,param_searchrad*i);
      motion_data[i].back_hor_f_code  = c;
      motion_data[i].back_vert_f_code = c;
      motion_data[i].sxb = MAX(1,param_searchrad*(M-i));
      motion_data[i].syb = MAX(1,param_searchrad*(M-i));
    }
  }

  /* make flags boolean (x!=0 -> x=1) */
  mpeg1 = !!mpeg1;
  fieldpic = !!fieldpic;
  low_delay = !!low_delay;
  constrparms = !!constrparms;
  prog_seq = !!prog_seq;
  topfirst = !!topfirst;

  for (i=0; i<3; i++)
  {
    frame_pred_dct_tab[i] = !!frame_pred_dct_tab[i];
    conceal_tab[i] = !!conceal_tab[i];
    qscale_tab[i] = !!qscale_tab[i];
    intravlc_tab[i] = !!intravlc_tab[i];
    altscan_tab[i] = !!altscan_tab[i];
  }
  repeatfirst = !!repeatfirst;
  prog_frame = !!prog_frame;

  /* make sure MPEG specific parameters are valid */
  range_checks();

  frame_rate = ratetab[frame_rate_code-1];

  /* timecode -> frame number */
  tc0 = h;
  tc0 = 60*tc0 + m;
  tc0 = 60*tc0 + s;
  tc0 = (int)(frame_rate+0.5)*tc0 + f;

  if (!mpeg1)
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
          fprintf(stderr,"Warning: setting constrained_parameters_flag = 0\n");
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
            fprintf(stderr,"Warning: setting constrained_parameters_flag = 0\n");
          constrparms = 0;
          break;
        }

        if (motion_data[i].forw_vert_f_code>4)
        {
          if (!quiet)
            fprintf(stderr,"Warning: setting constrained_parameters_flag = 0\n");
          constrparms = 0;
          break;
        }

        if (i!=0)
        {
          if (motion_data[i].back_hor_f_code>4)
          {
            if (!quiet)
              fprintf(stderr,"Warning: setting constrained_parameters_flag = 0\n");
            constrparms = 0;
            break;
          }

          if (motion_data[i].back_vert_f_code>4)
          {
            if (!quiet)
              fprintf(stderr,"Warning: setting constrained_parameters_flag = 0\n");
            constrparms = 0;
            break;
          }
        }
      }
    }
  }

  /* relational checks */

  if (mpeg1)
  {
    if (!prog_seq)
    {
      if (!quiet)
        fprintf(stderr,"Warning: setting progressive_sequence = 1\n");
      prog_seq = 1;
    }

    if (chroma_format!=CHROMA420)
    {
      if (!quiet)
        fprintf(stderr,"Warning: setting chroma_format = 1 (4:2:0)\n");
      chroma_format = CHROMA420;
    }

    if (dc_prec!=0)
    {
      if (!quiet)
        fprintf(stderr,"Warning: setting intra_dc_precision = 0\n");
      dc_prec = 0;
    }

    for (i=0; i<3; i++)
      if (qscale_tab[i])
      {
        if (!quiet)
          fprintf(stderr,"Warning: setting qscale_tab[%d] = 0\n",i);
        qscale_tab[i] = 0;
      }

    for (i=0; i<3; i++)
      if (intravlc_tab[i])
      {
        if (!quiet)
          fprintf(stderr,"Warning: setting intravlc_tab[%d] = 0\n",i);
        intravlc_tab[i] = 0;
      }

    for (i=0; i<3; i++)
      if (altscan_tab[i])
      {
        if (!quiet)
          fprintf(stderr,"Warning: setting altscan_tab[%d] = 0\n",i);
        altscan_tab[i] = 0;
      }
  }

  if (!mpeg1 && constrparms)
  {
    if (!quiet)
      fprintf(stderr,"Warning: setting constrained_parameters_flag = 0\n");
    constrparms = 0;
  }

  if (prog_seq && !prog_frame)
  {
    if (!quiet)
      fprintf(stderr,"Warning: setting progressive_frame = 1\n");
    prog_frame = 1;
  }

  if (prog_frame && fieldpic)
  {
    if (!quiet)
      fprintf(stderr,"Warning: setting field_pictures = 0\n");
    fieldpic = 0;
  }

  if (!prog_frame && repeatfirst)
  {
    if (!quiet)
      fprintf(stderr,"Warning: setting repeat_first_field = 0\n");
    repeatfirst = 0;
  }

  if (prog_frame)
  {
    for (i=0; i<3; i++)
      if (!frame_pred_dct_tab[i])
      {
        if (!quiet)
          fprintf(stderr,"Warning: setting frame_pred_frame_dct[%d] = 1\n",i);
        frame_pred_dct_tab[i] = 1;
      }
  }

  if (prog_seq && !repeatfirst && topfirst)
  {
    if (!quiet)
      fprintf(stderr,"Warning: setting top_field_first = 0\n");
    topfirst = 0;
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

static void readquantmat()
{
  int i,v;
  FILE *fd;

  if (iqname[0]=='-')
  {
  	if( param_hires_quant )
	{
	  load_iquant = 1;
	  intra_q[0] = 8;
	  for (i=1; i<64; i++)
	  {
	     intra_q[i] = 16;
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
	  load_niquant = 0;
	  for (i=0; i<64; i++)
	  {
	     intra_q[i] = 16;
	  }	
	}
	else
	{
        /* default non-intra matrix is all 16's. OFr our default we use something
    	more suitable for domestic analog sources... which is non-standard...*/
    	load_niquant = 1;
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
  
}
