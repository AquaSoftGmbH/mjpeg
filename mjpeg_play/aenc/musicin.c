/**********************************************************************
Copyright (c) 1991 MPEG/audio software simulation group, All Rights Reserved
musicin.c
**********************************************************************/
/**********************************************************************
 * MPEG/audio coding/decoding software, work in progress              *
 *   NOT for public distribution until verified and approved by the   *
 *   MPEG/audio committee.  For further information, please contact   *
 *   Davis Pan, 508-493-2241, e-mail: pan@3d.enet.dec.com             *
 *                                                                    *
 * VERSION 4.0                                                        *
 *   changes made since last update:                                  *
 *   date   programmers         comment                               *
 * 3/01/91  Douglas Wong,       start of version 1.1 records          *
 *          Davis Pan                                                 *
 * 3/06/91  Douglas Wong,       rename: setup.h to endef.h            *
 *                              removed extraneous variables          *
 * 3/21/91  J.Georges Fritsch   introduction of the bit-stream        *
 *                              package. This package allows you      *
 *                              to generate the bit-stream in a       *
 *                              binary or ascii format                *
 * 3/31/91  Bill Aspromonte     replaced the read of the SB matrix    *
 *                              by an "code generated" one            *
 * 5/10/91  W. Joseph Carter    Ported to Macintosh and Unix.         *
 *                              Incorporated Jean-Georges Fritsch's   *
 *                              "bitstream.c" package.                *
 *                              Modified to strictly adhere to        *
 *                              encoded bitstream specs, including    *
 *                              "Berlin changes".                     *
 *                              Modified user interface dialog & code *
 *                              to accept any input & output          *
 *                              filenames desired.  Also added        *
 *                              de-emphasis prompt and final bail-out *
 *                              opportunity before encoding.          *
 *                              Added AIFF PCM sound file reading     *
 *                              capability.                           *
 *                              Modified PCM sound file handling to   *
 *                              process all incoming samples and fill *
 *                              out last encoded frame with zeros     *
 *                              (silence) if needed.                  *
 *                              Located and fixed numerous software   *
 *                              bugs and table data errors.           *
 * 27jun91  dpwe (Aware Inc)    Used new frame_params struct.         *
 *                              Clear all automatic arrays.           *
 *                              Changed some variable names,          *
 *                              simplified some code.                 *
 *                              Track number of bits actually sent.   *
 *                              Fixed padding slot, stereo bitrate    *
 *                              Added joint-stereo : scales L+R.      *
 * 6/12/91  Earle Jennings      added fix for MS_DOS in obtain_param  *
 * 6/13/91  Earle Jennings      added stack length adjustment before  *
 *                              main for MS_DOS                       *
 * 7/10/91  Earle Jennings      conversion of all float to FLOAT      *
 *                              port to MsDos from MacIntosh completed*
 * 8/ 8/91  Jens Spille         Change for MS-C6.00                   *
 * 8/22/91  Jens Spille         new obtain_parameters()               *
 *10/ 1/91  S.I. Sudharsanan,   Ported to IBM AIX platform.           *
 *          Don H. Lee,                                               *
 *          Peter W. Farrett                                          *
 *10/ 3/91  Don H. Lee          implemented CRC-16 error protection   *
 *                              newly introduced functions are        *
 *                              I_CRC_calc, II_CRC_calc and encode_CRC*
 *                              Additions and revisions are marked    *
 *                              with "dhl" for clarity                *
 *11/11/91 Katherine Wang       Documentation of code.                *
 *                                (variables in documentation are     *
 *                                surround by the # symbol, and an '*'*
 *                                denotes layer I or II versions)     *
 * 2/11/92  W. Joseph Carter    Ported new code to Macintosh.  Most   *
 *                              important fixes involved changing     *
 *                              16-bit ints to long or unsigned in    *
 *                              bit alloc routines for quant of 65535 *
 *                              and passing proper function args.     *
 *                              Removed "Other Joint Stereo" option   *
 *                              and made bitrate be total channel     *
 *                              bitrate, irrespective of the mode.    *
 *                              Fixed many small bugs & reorganized.  *
 * 2/25/92  Masahiro Iwadare    made code cleaner and more consistent *
 * 8/07/92  Mike Coleman        make exit() codes return error status *
 *                              made slight changes for portability   *
 *19 aug 92 Soren H. Nielsen    Changed MS-DOS file name extensions.  *
 * 8/25/92  Shaun Astarabadi    Replaced rint() function with explicit*
 *                              rounding for portability with MSDOS.  *
 * 9/22/92  jddevine@aware.com  Fixed _scale_factor_calc() calls.     *
 *10/19/92  Masahiro Iwadare    added info->mode and info->mode_ext   *
 *                              updates for AIFF format files         *
 * 3/10/93  Kevin Peterson      In parse_args, only set non default   *
 *                              bit rate if specified in arg list.    *
 *                              Use return value from aiff_read_hdrs  *
 *                              to fseek to start of sound data       *
 * 7/26/93  Davis Pan           fixed bug in printing info->mode_ext  *
 *                              value for joint stereo condition      *
 * 8/27/93 Seymour Shlien,      Fixes in Unix and MSDOS ports,        *
 *         Daniel Lauzon, and                                         *
 *         Bill Truerniet                                             *
 **********************************************************************/

#ifdef MS_DOS
#include <dos.h>
#endif
#include <stdlib.h>
#include "common.h"
#include "encoder.h"

/* Global variable definitions for "musicin.c" */

FILE               *musicin;
Bit_stream_struc   bs;
int		   verbose = 2;
char               *programName;

/* Global variables */

int freq_in=0;
int freq_out=44100;
int chans_in=0;
int chans_out=0;
int audio_bits=0;
unsigned long audio_bytes=0;

void Usage(char *str)
{
  printf("Usage: %s [params] < input.wav\n",str);
  printf("   where possible params are:\n");
  printf("   -b num     Bitrate in KBit/sec (default: 224 KBit/s)\n");
  printf("   -o name    Outputfile name (REQUIRED)\n");
  printf("   -r num     Force output rate to be num Hz (default: 44100)\n");
  printf("              num must be one of 32000, 44100, 48000\n");
  printf("   -s         Force stereo output (default)\n");
  printf("   -m         Force mono output\n");
  printf("   -v         Force VCD compatible output (same as: -b 224 -r 44100 -s)\n");
  exit(0);
}


void
get_params(argc, argv,fr_ps,psy,num_samples,encoded_file_name)
int             argc;
char**          argv;
frame_params    *fr_ps;
int             *psy;
unsigned long   *num_samples;
char            encoded_file_name[MAX_NAME_SIZE];
{
    layer *info = fr_ps->header;
    int brt=224;
    char *outfilename = 0;
    int stereo = 1;
    int mono = 0;
    int video_cd = 0;
    int j, n;
    int audio_format;

    /* RJ: We set most params to fixed defaults: */

    info->lay = 2;
    info->emphasis = 0;
    info->extension = 0;
    info->error_protection = FALSE;
    info->copyright = 0;
    info->original = 0;

    *psy = 2;
 
    *num_samples = MAX_U_32_NUM; /* Unlimited */

    brt = 0;

    while( (n=getopt(argc,argv,"b:o:r:smvV:")) != EOF)
    {
        switch(n) {

            case 'b':
               brt = atoi(optarg);
               break;

            case 'o':
               outfilename = optarg;
               break;

            case 'r':
               freq_out = atoi(optarg);
               break;

            case 's':
               stereo = 1;
               break;

            case 'm':
               mono = 1;
               break;

            case 'v':
               video_cd = 1;
               break;

	    case 'V':
		verbose = atoi(optarg);
		break;

            case '?':
               Usage(argv[0]);
        }
    }

    if(argc!=optind) Usage(argv[0]);

    if(outfilename==0)
    {
        fprintf(stderr,"Output file name (-o option) is required!\n");
        Usage(argv[0]);
    }
    strncpy(encoded_file_name,outfilename,MAX_NAME_SIZE);

    /* Sanity checks */

    if(freq_out!=0 && freq_out!=32000 && freq_out!=44100 && freq_out!=48000)
    {
       fprintf(stderr,"Options -r requires one of 32000 44100 48000!\n");
       Usage(argv[0]);
    }

    if(stereo && mono)
    {
       fprintf(stderr,"Options -s and -m are mutally exclusive!\n");
       Usage(argv[0]);
    }
    if(mono)   chans_out = 1;
    if(stereo) chans_out = 2;

    if(video_cd && mono)
    {
       fprintf(stderr,"Options -v and -m are mutally exclusive!\n");
       Usage(argv[0]);
    }

    if(video_cd && freq_out!=0 && freq_out!=44100)
    {
       fprintf(stderr,"Option -v requires sample rate 44100!\n");
       Usage(argv[0]);
    }

    if(video_cd && brt!=0 && brt!=224)
    {
       fprintf(stderr,"Option -v requires bit rate 224 KBit/s!\n");
       Usage(argv[0]);
    }

    if(video_cd)
    {
       freq_out=44100;
       brt = 224;
       chans_out = 2;
    }

    /* Read the WAV file header, make sanity checks */

    if(wav_read_header(stdin,&freq_in,&chans_in,&audio_bits,
                       &audio_format,&audio_bytes))
    {
       fprintf(stderr,"*** Error reading WAV file ***\n");
       exit(0);
    }

    printf("Opened WAV file, freq = %d Hz, channels = %d, bits = %d\n",
           freq_in, chans_in, audio_bits);
    printf("   format = 0x%x, audio length = %lu bytes\n",audio_format,audio_bytes);

    if(audio_format!=1)
    {
       fprintf(stderr,"Error: WAV file is not in PCM format\n");
       exit(1);
    }
    if(audio_bits!=8 && audio_bits!=16)
    {
       fprintf(stderr,"Error: audio samples must have 8 or 16 bits\n");
       exit(1);
    }

    if(chans_in!=1 && chans_in!=2)
    {
       fprintf(stderr,"Error: can only handle files with 1 or 2 channels\n");
       exit(1);
    }

    if(chans_out==0) chans_out = chans_in;

    if(chans_out==1)
    {
       info->mode = MPG_MD_MONO;
       info->mode_ext = 0;
    }
    else
    {
       info->mode = MPG_MD_STEREO;
       info->mode_ext = 0;
    }

    if(freq_out==0) freq_out = freq_in;
    switch (freq_out) {
       case 48000 :
           info->sampling_frequency = 1;
           break;
       case 44100 :
           info->sampling_frequency = 0;
           break;
       case 32000 :
           info->sampling_frequency = 2;
           break;
       default:
           fprintf(stderr,"Error: Frequency must be one of 32000 44100 48000"
                          " unless -r is used!\n");
           exit(1);
    }

    *num_samples = audio_bytes/(audio_bits/8);

    if (brt==0) brt = (info->mode == MPG_MD_MONO) ? 112 : 224;

    for(j=0;j<15;j++) if (bitrate[info->lay-1][j] == brt) break;

    if (j==15) {
        fprintf(stderr,"Bitrate of %d KBit/s not allowed!\n",brt);
        exit(1);
    }

    info->bitrate_index = j;

    if(info->lay==2 && brt>192 && info->mode==MPG_MD_MONO) {
        fprintf(stderr,"Bitrate of %d KBit/s not allowed for MONO\n",brt);
        exit(1);
    }
 
    open_bit_stream_w(&bs, encoded_file_name, BUFFER_SIZE);
}

/************************************************************************
 *
 * print_config
 *
 * PURPOSE:  Prints the encoding parameters used
 *
 ************************************************************************/
 
void
print_config(fr_ps, psy, num_samples, outPath)
frame_params *fr_ps;
int     *psy;
unsigned long *num_samples;
char    outPath[MAX_NAME_SIZE];
{
 layer *info = fr_ps->header;

   if (verbose < 2)
	return;
 
   printf("Encoding configuration:\n");
   if(info->mode != MPG_MD_JOINT_STEREO)
      printf("Layer=%s   mode=%s   extn=%d   psy model=%d\n",
             layer_names[info->lay-1], mode_names[info->mode],
             info->mode_ext, *psy);
   else printf("Layer=%s   mode=%s   extn=data dependant   psy model=%d\n",
               layer_names[info->lay-1], mode_names[info->mode], *psy);
   printf("samp frq=%.1f kHz   total bitrate=%d kbps\n",
          s_freq[info->sampling_frequency],
          bitrate[info->lay-1][info->bitrate_index]);
   printf("de-emph=%d   c/right=%d   orig=%d   errprot=%d\n",
          info->emphasis, info->copyright, info->original,
          info->error_protection);
   printf("output file: '%s'\n", outPath);
}
 
/************************************************************************
 *
 * main
 *
 * PURPOSE:  MPEG I Encoder supporting layers 1 and 2, and
 * psychoacoustic models 1 (MUSICAM) and 2 (AT&T)
 *
 * SEMANTICS:  One overlapping frame of audio of up to 2 channels are
 * processed at a time in the following order:
 * (associated routines are in parentheses)
 *
 * 1.  Filter sliding window of data to get 32 subband
 * samples per channel.
 * (window_subband,filter_subband)
 *
 * 2.  If joint stereo mode, combine left and right channels
 * for subbands above #jsbound#.
 * (*_combine_LR)
 *
 * 3.  Calculate scalefactors for the frame, and if layer 2,
 * also calculate scalefactor select information.
 * (*_scale_factor_calc)
 *
 * 4.  Calculate psychoacoustic masking levels using selected
 * psychoacoustic model.
 * (*_Psycho_One, psycho_anal)
 *
 * 5.  Perform iterative bit allocation for subbands with low
 * mask_to_noise ratios using masking levels from step 4.
 * (*_main_bit_allocation)
 *
 * 6.  If error protection flag is active, add redundancy for
 * error protection.
 * (*_CRC_calc)
 *
 * 7.  Pack bit allocation, scalefactors, and scalefactor select
 * information (layer 2) onto bitstream.
 * (*_encode_bit_alloc,*_encode_scale,II_transmission_pattern)
 *
 * 8.  Quantize subbands and pack them into bitstream
 * (*_subband_quantization, *_sample_encoding)
 *
 ************************************************************************/

int main(argc, argv)
int     argc;
char    **argv;
{
typedef double SBS[2][3][SCALE_BLOCK][SBLIMIT];
    SBS  FAR        *sb_sample;
typedef double JSBS[3][SCALE_BLOCK][SBLIMIT];
    JSBS FAR        *j_sample;
typedef double IN[2][HAN_SIZE];
    IN   FAR        *win_que;
typedef unsigned int SUB[2][3][SCALE_BLOCK][SBLIMIT];
    SUB  FAR        *subband;
 
    frame_params fr_ps;
    layer info;
    char encoded_file_name[MAX_NAME_SIZE];
    short FAR **win_buf;
static short FAR buffer[2][1152];
static unsigned int bit_alloc[2][SBLIMIT], scfsi[2][SBLIMIT];
static unsigned int scalar[2][3][SBLIMIT], j_scale[3][SBLIMIT];
static double FAR ltmin[2][SBLIMIT], lgmin[2][SBLIMIT], max_sc[2][SBLIMIT];
    FLOAT snr32[32];
    short sam[2][1056];
    int whole_SpF, extra_slot = 0;
    double avg_slots_per_frame, frac_SpF, slot_lag;
    int model, stereo, error_protection;
static unsigned int crc;
    int i, j, k, adb;
    unsigned long bitsPerSlot, samplesPerFrame, frameNum = 0;
    unsigned long frameBits, sentBits = 0;
    unsigned long num_samples;

#ifdef  MACINTOSH
    console_options.nrows = MAC_WINDOW_SIZE;
    argc = ccommand(&argv);
#endif

    /* Most large variables are declared dynamically to ensure
       compatibility with smaller machines */

    sb_sample = (SBS FAR *) mem_alloc(sizeof(SBS), "sb_sample");
    j_sample = (JSBS FAR *) mem_alloc(sizeof(JSBS), "j_sample");
    win_que = (IN FAR *) mem_alloc(sizeof(IN), "Win_que");
    subband = (SUB FAR *) mem_alloc(sizeof(SUB),"subband");
    win_buf = (short FAR **) mem_alloc(sizeof(short *)*2, "win_buf");
 
    /* clear buffers */
    memset((char *) buffer, 0, sizeof(buffer));
    memset((char *) bit_alloc, 0, sizeof(bit_alloc));
    memset((char *) scalar, 0, sizeof(scalar));
    memset((char *) j_scale, 0, sizeof(j_scale));
    memset((char *) scfsi, 0, sizeof(scfsi));
    memset((char *) ltmin, 0, sizeof(ltmin));
    memset((char *) lgmin, 0, sizeof(lgmin));
    memset((char *) max_sc, 0, sizeof(max_sc));
    memset((char *) snr32, 0, sizeof(snr32));
    memset((char *) sam, 0, sizeof(sam));
 
    fr_ps.header = &info;
    fr_ps.tab_num = -1;             /* no table loaded */
    fr_ps.alloc = NULL;
    info.version = MPEG_AUDIO_ID;

    programName = argv[0];
    get_params(argc, argv, &fr_ps, &model, &num_samples, encoded_file_name);
    print_config(&fr_ps, &model, &num_samples, encoded_file_name);

    hdr_to_frps(&fr_ps);
    stereo = fr_ps.stereo;
    error_protection = info.error_protection;
 
    if (info.lay == 1) { bitsPerSlot = 32; samplesPerFrame = 384;  }
    else               { bitsPerSlot = 8;  samplesPerFrame = 1152; }
    /* Figure average number of 'slots' per frame. */
    /* Bitrate means TOTAL for both channels, not per side. */
    avg_slots_per_frame = ((double)samplesPerFrame /
                           s_freq[info.sampling_frequency]) *
                          ((double)bitrate[info.lay-1][info.bitrate_index] /
                           (double)bitsPerSlot);
    whole_SpF = (int) avg_slots_per_frame;
    if (verbose > 1)
	printf("slots/frame = %d\n",whole_SpF);
    frac_SpF  = avg_slots_per_frame - (double)whole_SpF;
    slot_lag  = -frac_SpF;
    if (verbose > 1)
	printf("frac SpF=%.3f, tot bitrate=%d kbps, s freq=%.1f kHz\n",
           frac_SpF, bitrate[info.lay-1][info.bitrate_index],
           s_freq[info.sampling_frequency]);
 
    if (verbose > 2 &&frac_SpF != 0)
       printf("Fractional number of slots, padding required\n");
    else info.padding = 0;
 
    while (get_audio(musicin, buffer, num_samples, stereo, info.lay) > 0) {

       win_buf[0] = &buffer[0][0];
       win_buf[1] = &buffer[1][0];
       if (frac_SpF != 0) {
          if (slot_lag > (frac_SpF-1.0) ) {
             slot_lag -= frac_SpF;
             extra_slot = 0;
             info.padding = 0;
             /*  printf("No padding for this frame\n"); */
          }
          else {
             extra_slot = 1;
             info.padding = 1;
             slot_lag += (1-frac_SpF);
             /*  printf("Padding for this frame\n");    */
          }
       }
       adb = (whole_SpF+extra_slot) * bitsPerSlot;

       switch (info.lay) {
 
/***************************** Layer I **********************************/
 
          case 1 :
             for (j=0;j<SCALE_BLOCK;j++)
             for (k=0;k<stereo;k++) {
                window_subband(&win_buf[k], &(*win_que)[k][0], k);
                filter_subband(&(*win_que)[k][0], &(*sb_sample)[k][0][j][0]);
             }

             I_scale_factor_calc(*sb_sample, scalar, stereo);
             if(fr_ps.actual_mode == MPG_MD_JOINT_STEREO) {
                I_combine_LR(*sb_sample, *j_sample);
                I_scale_factor_calc(j_sample, &j_scale, 1);
             }
 
             put_scale(scalar, &fr_ps, max_sc);
 
             if (model == 1) I_Psycho_One(buffer, max_sc, ltmin, &fr_ps);
             else {
                for (k=0;k<stereo;k++) {
                   psycho_anal(&buffer[k][0],&sam[k][0], k, info.lay, snr32,
                               (FLOAT)s_freq[info.sampling_frequency]*1000);
                   for (i=0;i<SBLIMIT;i++) ltmin[k][i] = (double) snr32[i];
                }
             }
 
             I_main_bit_allocation(ltmin, bit_alloc, &adb, &fr_ps);
 
             if (error_protection) I_CRC_calc(&fr_ps, bit_alloc, &crc);
 
             encode_info(&fr_ps, &bs);
 
             if (error_protection) encode_CRC(crc, &bs);
 
             I_encode_bit_alloc(bit_alloc, &fr_ps, &bs);
             I_encode_scale(scalar, bit_alloc, &fr_ps, &bs);
             I_subband_quantization(scalar, *sb_sample, j_scale, *j_sample,
                                    bit_alloc, *subband, &fr_ps);
             I_sample_encoding(*subband, bit_alloc, &fr_ps, &bs);
             for (i=0;i<adb;i++) put1bit(&bs, 0);
          break;
 
/***************************** Layer 2 **********************************/
 
          case 2 :
             for (i=0;i<3;i++) for (j=0;j<SCALE_BLOCK;j++)
                for (k=0;k<stereo;k++) {
                   window_subband(&win_buf[k], &(*win_que)[k][0], k);
                   filter_subband(&(*win_que)[k][0], &(*sb_sample)[k][i][j][0]);
                }
 
                II_scale_factor_calc(*sb_sample, scalar, stereo, fr_ps.sblimit);
                pick_scale(scalar, &fr_ps, max_sc);
                if(fr_ps.actual_mode == MPG_MD_JOINT_STEREO) {
                   II_combine_LR(*sb_sample, *j_sample, fr_ps.sblimit);
                   II_scale_factor_calc(j_sample, &j_scale, 1, fr_ps.sblimit);
                }       /* this way we calculate more mono than we need */
                        /* but it is cheap */
 
                if (model == 1) II_Psycho_One(buffer, max_sc, ltmin, &fr_ps);
                else {
                   for (k=0;k<stereo;k++) {
                      psycho_anal(&buffer[k][0],&sam[k][0], k, 
                                 info.lay, snr32,
                                 (FLOAT)s_freq[info.sampling_frequency]*1000);
                      for (i=0;i<SBLIMIT;i++) ltmin[k][i] = (double) snr32[i];
                   }
                }
 
                II_transmission_pattern(scalar, scfsi, &fr_ps);
                II_main_bit_allocation(ltmin, scfsi, bit_alloc, &adb, &fr_ps);
 
                if (error_protection)
                   II_CRC_calc(&fr_ps, bit_alloc, scfsi, &crc);
 
                encode_info(&fr_ps, &bs);
 
                if (error_protection) encode_CRC(crc, &bs);
 
                II_encode_bit_alloc(bit_alloc, &fr_ps, &bs);
                II_encode_scale(bit_alloc, scfsi, scalar, &fr_ps, &bs);
                II_subband_quantization(scalar, *sb_sample, j_scale,
                                      *j_sample, bit_alloc, *subband, &fr_ps);
                II_sample_encoding(*subband, bit_alloc, &fr_ps, &bs);
                for (i=0;i<adb;i++) put1bit(&bs, 0);
          break;
 
/***************************** Layer 3 **********************************/

          case 3 : break;

       }
 
       frameBits = sstell(&bs) - sentBits;
       if(frameBits%bitsPerSlot)   /* a program failure */
          fprintf(stderr,"Sent %ld bits = %ld slots plus %ld\n",
                  frameBits, frameBits/bitsPerSlot,
                  frameBits%bitsPerSlot);
       sentBits += frameBits;

    }

    close_bit_stream_w(&bs);

    if (verbose > 1)
	printf("Avg slots/frame = %.3f; b/smp = %.2f; br = %.3f kbps\n",
           (FLOAT) sentBits / (frameNum * bitsPerSlot),
           (FLOAT) sentBits / (frameNum * samplesPerFrame),
           (FLOAT) sentBits / (frameNum * samplesPerFrame) *
           s_freq[info.sampling_frequency]);

#ifdef  MACINTOSH
    set_mac_file_attr(encoded_file_name, VOL_REF_NUM, CREATOR_ENCODE,
                      FILETYPE_ENCODE);
#endif

    if (verbose > 1) {
	printf("Encoding with psychoacoustic model %d is finished\n", model);
	printf("The MPEG encoded output file name is \"%s\"\n",
            encoded_file_name);
    }
    exit(0);
}
