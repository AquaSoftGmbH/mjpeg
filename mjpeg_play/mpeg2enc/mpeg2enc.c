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

#define GLOBAL /* used by global.h */
#include "config.h"
#include "global.h"

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

/* RJ: additional variables */

#include "lav_io.h"
#include "editlist.h"

EditList el;

/* RJ: Parameters for creation of the MPEG file */

static int param_bitrate    = 0;
static int param_quant      = 0;
static int param_searchrad  = 0;
static int param_mpeg       = 1;
static int param_special    = 0;
static int param_drop_lsb   = 0;
static int param_noise_filt = 0;
static int param_fastmc     = 10;
static int param_threshold  = 0;

/* reserved: for later use */
int param_422 = 0;

static int output_width, output_height;

void Usage(char *str)
{
  printf("Usage: %s [params] inputfiles\n",str);
  printf("   where possible params are:\n");
  printf("   -m num     MPEG level (1 or 2) default: 1\n");
  printf("   -b num     Bitrate in KBit/sec (default: 1152 KBit/s)\n");
  printf("   -q num     Quality factor [1..31] (1 is best, no default)\n");
  printf("              Bitrate and Quality are mutually exclusive!\n");
  printf("   -o name    Outputfile name (REQUIRED!!!)\n");
  printf("   -r num     Search radius [0..32] (default 0: don\'t search at all)\n");
  printf("   -s num     Special output format option:\n");
  printf("                 0 output like input, nothing special\n");
  printf("                 1 create half height/width output from interlaced input\n");
  printf("                 2 create 480 wide output from 720 wide input (for SVCD)\n");
  printf("   -d num     Drop lsbs of samples [0..3] (default: 0)\n");
  printf("   -n num     Noise filter (low-pass) [0..2] (default: 0)\n");
  printf("   -f num     Fraction of fast motion estimates to consider in detail (1/num) [2..20] (default: 10)\n" );
  printf("   -t         Activate dynamic thresholding of motion compensation window size\n" );
  exit(0);
}

int main(argc,argv)
int argc;
char *argv[];
{
  char *outfilename=0;
  int n, nerr = 0;

  while( (n=getopt(argc,argv,"m:b:q:o:r:s:f:d:n:t")) != EOF)
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

      case 'r':
        param_searchrad = atoi(optarg);
        if(param_searchrad<0 || param_searchrad>32)
        {
          fprintf(stderr,"-r option requires arg 0 .. 32\n");
          nerr++;
        }
        break;

      case 's':
        param_special = atoi(optarg);
        if(param_special<0 || param_special>2)
        {
          fprintf(stderr,"-s option requires arg 0, 1 or 2\n");
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

  if(optind>=argc) nerr++;

  if(nerr) Usage(argv[0]);

  fix_mquant = param_quant;
  if(param_bitrate==0 && param_quant==0) param_bitrate = 1152;

  if(param_searchrad==0) do_not_search = 1;

  /* Open editlist */

  read_video_files(argv + optind, argc - optind, &el);

  output_width  = el.video_width;
  output_height = el.video_height;

  if(param_special==1)
  {
    if(el.video_inter)
    {
      output_width  = (el.video_width/2) &0xfffffff0;
      output_height = el.video_height/2;
    }
    else
    {
      fprintf(stderr,"-s 1 may only be set for interlaced video sources\n");
      Usage(argv[0]);
    }
  }

  if(param_special==2)
  {
    if(el.video_width==720)
    {
      output_width  = 480;
    }
    else
    {
      fprintf(stderr,"-s 2 may only be set for 720 pixel wide video sources\n");
      Usage(argv[0]);
    }
  }

  printf("\nEncoding MPEG-%d video to %s\n",param_mpeg,outfilename);
  if(param_bitrate) printf("Bitrate: %d KBit/s\n",param_bitrate);
  if(param_quant) printf("Quality factor: %d (1=best, 31=worst)\n",param_quant);
  printf("Search radius: %d\n",param_searchrad);
  printf("Output dimensions: %d x %d\n\n",output_width,output_height);

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
  putseq();

  fclose(outfile);
  fclose(statfile);

  return 0;
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

  /* clip table */
  if (!(clp = (unsigned char *)malloc(1024)))
    error("malloc failed\n");
  clp+= 384;
  for (i=-384; i<640; i++)
    clp[i] = (i<0) ? 0 : ((i>255) ? 255 : i);

  for (i=0; i<3; i++)
  {
	/* A.Stevens 2000 for liminance data we allow space for
	   fast motion estimation data.  This is actually 2*2 pixel
	   group sum data, hence 1/4 of actual luminance data.
	   However for speed data is stored in short's hence
	   sizeof(short/4).
	*/
	if (i==0)
	  size = (4*width*height * (1+sizeof(mcompuint)))/4;
	else
	  size = chrom_width*chrom_height;

    if (!(newrefframe[i] = (unsigned char *)malloc(size)))
      error("malloc failed\n");
    if (!(oldrefframe[i] = (unsigned char *)malloc(size)))
      error("malloc failed\n");
    if (!(auxframe[i] = (unsigned char *)malloc(size)))
      error("malloc failed\n");
    if (!(neworgframe[i] = (unsigned char *)malloc(size)))
      error("malloc failed\n");
    if (!(oldorgframe[i] = (unsigned char *)malloc(size)))
      error("malloc failed\n");
    if (!(auxorgframe[i] = (unsigned char *)malloc(size)))
      error("malloc failed\n");
    if (!(predframe[i] = (unsigned char *)malloc(size)))
      error("malloc failed\n");
  }

  if( !(filter_buf =  (unsigned char *)malloc( width*height )) )
	error("malloc failed\n");

  mbinfo = (struct mbinfo *)malloc(mb_width*mb_height2*sizeof(struct mbinfo));

  if (!mbinfo)
    error("malloc failed\n");

  blocks =
    (short (*)[64])malloc(mb_width*mb_height2*block_count*sizeof(short [64]));

  if (!blocks)
    error("malloc failed\n");

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
  strcpy(statname,"-");
  inputtype = 0;  /* doesnt matter */
  nframes = el.video_frames; /* leave 2 away */
  frame0  = 0;
  h = m = s = f = 0;
  N = 12;      /* I frame distance */
  M = 3;       /* I or P frame distance */
  mpeg1           = param_mpeg == 1;
  fieldpic        = (el.video_inter!=0) && (param_special!=1);
  horizontal_size = output_width;
  vertical_size   = output_height;
  /* RJ: aspectratio is differently coded for MPEG1 and MPEG2.
   *     For MPEG2 aspect ratio is for total image: 2 means 4:3
   *     For MPEG1 aspect ratio is for the pixels:  1 means square Pixels */
  aspectratio     = mpeg1 ? 1 : 2;
  frame_rate_code = el.video_norm == 'n' ? 4 : 3;
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
  prog_seq        = (el.video_inter==0)||(param_special==1);
  chroma_format   = param_422 ? CHROMA422 : CHROMA420;
  video_format    = el.video_norm == 'n' ? 2 : 1;
  color_primaries = 5;
  transfer_characteristics = 5;
  matrix_coefficients      = el.video_norm == 'n' ? 4 : 5;
  display_horizontal_size  = horizontal_size;
  display_vertical_size    = vertical_size;
  dc_prec         = 0;  /* 8 bits */
  topfirst        = (el.video_inter==2)&&(param_special!=1); /* 2: LAV_INTER_EVEN_FIRST */
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
  altscan_tab[0]  = mpeg1 ? 0 : (el.video_inter!=0);
  altscan_tab[1]  = mpeg1 ? 0 : (el.video_inter!=0);
  altscan_tab[2]  = mpeg1 ? 0 : (el.video_inter!=0);
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

  if(param_searchrad*M>127)
  {
     param_searchrad = 127/M;
     fprintf(stderr,"Search radius reduced to %d\n",param_searchrad);
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

static void readquantmat()
{
  int i,v;
  FILE *fd;

  if (iqname[0]=='-')
  {
    /* use default intra matrix */
    load_iquant = 0;
    for (i=0; i<64; i++)
      intra_q[i] = default_intra_quantizer_matrix[i];
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
      if (v<1 || v>255)
        error("invalid value in quant matrix");
      intra_q[i] = v;
    }

    fclose(fd);
  }

  if (niqname[0]=='-')
  {
    /* use default non-intra matrix */
    load_niquant = 0;
    for (i=0; i<64; i++)
      inter_q[i] = default_nonintra_quantizer_matrix[i];;
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
      if (v<1 || v>255)
        error("invalid value in quant matrix");
      inter_q[i] = v;
    }

    fclose(fd);
  }
}
