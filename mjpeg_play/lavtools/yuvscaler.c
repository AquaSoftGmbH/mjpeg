/*
 *  yuvscaler.c
 *  Copyright (C) 2001 Xavier Biquard <xbiquard@free.fr>
 * 
 *  
 *  Downscales arbitrary sized yuv frame to yuv frames suitable for VCD, or SVCD
 * 
 *  yuvscaler is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * 
 *  yuvscaler is distributed under the terms of the GNU General Public Licence
 *  as discribed in the COPYING file
*/
  
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "yuvscaler.h"
#include "lav_io.h"
#include "editlist.h"
#include "jpegutils.c"

int verbose=2;

#define PARAM_LINE_MAX 256
float framerates[] = { 0, 23.976, 24.0, 25.0, 29.970, 30.0, 50.0, 59.940, 60.0 };

// Downscaling ratios
unsigned int input_height_slice;
unsigned int output_height_slice;
unsigned int input_width_slice;
unsigned int output_width_slice;

// For output
unsigned int display_width;
unsigned int display_height;
unsigned int output_active_width;
unsigned int output_active_height;
unsigned int output_height_black_above;
unsigned int output_height_black_under;
unsigned int black=0; // =1 if black lines must be generated on output

// For input
unsigned int input_useful_width;
unsigned int input_useful_height;
unsigned int input_offset_width;
unsigned int input_offset_height;
unsigned int input_width;
unsigned int input_height;

// Global variables
//int skip=0;    // =0 when there is no line to skip on input
int norm=-1;   // =0 for PAL and =1 for NTSC
int format=-1; // =0 for VCD and =1 for SVCD
int wide=0;    // =1 for wide (16:9) input to standard (4:3) output conversion
int debug=0; // =0 for no debugging informations, =1 for debugging informations
int infile=0; // =0 for stdin (default) =1 for file

// taken from lav2yuv
EditList el;
#define MAX_EDIT_LIST_FILES 256
#define MAX_JPEG_LEN (512*1024)
static unsigned char jpeg_data[MAX_JPEG_LEN];
int chroma_format = CHROMA422;

// Keywords for argument passing 
const char VCD_KEYWORD[]  = "VCD";
const char SVCD_KEYWORD[] = "SVCD";
const char SIZE_KEYWORD[] = "SIZE_";
const char USE_KEYWORD[] = "USE_";
const char WIDE2STD_KEYWORD[] = "WIDE2STD";
const char INFILE_KEYWORD[] = "INFILE_";
//const char RATIO_KEYWORD[] = "RATIO";

// Divers
unsigned long int diviseur;
unsigned short int * divide,* u_i_p;



// *************************************************************************************
void print_usage(char *argv[])
{
   fprintf(stderr,"usage: %s -i [input_keyword] -m [mode_keyword] -o [output_keyword] [-d] [-h] [input_file] > [output_file]\n"
	   "%s downscales arbitrary sized yuv frames to a specified format\n\n"
	   "%s is keyword driven :\n"
	   "\t -i for keyword concerning INPUT  frame characteristics\n"
	   "\t -m for keyword concerning the downscaling MODE of yuvscaler\n"
	   "\t -o for keyword concerning OUTPUT frame characteristics\n"
	   "\t ((the former syntax with -k and -w is still supported but depreciated))\n"
	   "\n"
	   "Possible input keyword is USE_WidthxHeight+WidthOffset+HeightOffset to select a useful area of the input frame\n"
	   "Possible mode keyword is WIDE2STD to converts widescreen (16:9) input frames into standard output (4:3), generating necessary black lines\n"
	   "Possible output keywords are:\n"
	   "\t VCD to generate  VCD compliant frames on output (taking care of PAL and NTSC standards)\n"
	   "\tSVCD to generate SVCD compliant frames on output (taking care of PAL and NTSC standards)\n"
	   "\tSIZE_WidthxHeight to generate frames of size WidthxHeight on output\n"
	   "-d : print debugging informations\n"
	   "-h : this usage help\n"
	   ,argv[0],argv[0],argv[0]);
   exit(1);
}
// *************************************************************************************

// *************************************************************************************
void handle_args_global (int argc, char *argv[])
{
   // This function takes care of the global variables initialisation as a function of command line options
   int c;
   unsigned int ui1,ui2,ui3,ui4;
   int i1,i2,output,input,mode;
   
   // Default values 
//   output_height_black_above=output_height_black_under=skip_lines=0;
   // End of default values
   
   
   while((c = getopt(argc,argv,"k:o:i:m:wdh")) != -1)
     {
	switch (c)
	  {
	   case 'k':  // Compatibility reasons
	   case 'o':  // OUTPUT
	     output=0;
	     if (strcmp(optarg,VCD_KEYWORD)==0) {
		output=1;
		format=0;
		output_active_width =display_width =352;
		if (norm==0) {
		   fprintf(stderr,"VCD output format requested in PAL norm\n");
		   output_active_height=display_height=288;
		}
		if (norm==1) {
		   fprintf(stderr,"VCD output format requested in NTSC norm\n");
		   output_active_height=display_height=240;
		}
	     }
	     if (strcmp(optarg,SVCD_KEYWORD)==0) {
		output=1;
		format=1;
		output_active_width =display_width =480;
		if (norm==0) {
		   fprintf(stderr,"SVCD output format requested in PAL norm\n");
		   output_active_height=display_height=576;
		}
		if (norm==1) {
		   fprintf(stderr,"SVCD output format requested in NTSC norm\n");
		   output_active_height=display_height=480;
		}
	     }
	     if (strncmp(optarg,SIZE_KEYWORD,5)==0) {
		output=1;
		format=2;
		if (sscanf(optarg,"SIZE_%ux%u",&ui1,&ui2)==2) {
		   output_active_width=display_width=ui1;
		   output_active_height=display_height=ui2;
		} else 
		  fprintf(stderr,"Uncorrect argument %s\nSkipping argument\n",optarg);
	     }
	     if (output==0)
	       fprintf(stderr,"Uncorrect output argument %s\nSkipping argument\n",optarg);
	     break;
	     
	   case 'i': // INPUT
	     input=0;
	     if (strncmp(optarg,USE_KEYWORD,4)==0) {
		input=1;
		format=3;
		if (sscanf(optarg,"USE_%ux%u+%u+%u",&ui1,&ui2,&ui3,&ui4)==4) {
		   // Coherence check : offsets must be multiple of 2 since U and V have half Y resolution
		   // and the required zone must be inside the input size
		   if ((ui1%2==0) && (ui2%2==0) && (ui3%2==0) && (ui4%2==0) && (ui1+ui3<=input_width) && (ui2+ui4<=input_height)) {
		      input_useful_width=ui1;
		      input_useful_height=ui2;
		      input_offset_width=ui3;
		      input_offset_height=ui4;
		   } else 
		     fprintf(stderr,"Uncoherence in argument %s\nSkipping argument\n",optarg);
		} else
		  fprintf(stderr,"Uncorrect argument %s\nSkipping argument\n",optarg);
	     }
	     if (input==0)
	       fprintf(stderr,"Uncorrect input argument %s\nSkipping argument\n",optarg);
	     break;
	     
	   case 'd':
	     debug=1;
	     break;
	     
	   case 'w': // Compatibility reasons
	     wide=1;
	     break;
	   
	   case 'm':  // MODE
	     mode=0;
	     if (strcmp(optarg,WIDE2STD_KEYWORD)==0) {
		mode=1;
		wide=1;
	     }
	     if (mode==0)
	       fprintf(stderr,"Uncorrect mode argument %s\nSkipping argument\n",optarg);
	     break;
	     
	   case 'h':
	     print_usage(argv);
	     break;
	     
	   default:
	  }
     }
   if (format==-1) {
      fprintf(stderr,"No correct keyword given on commande line! STOP!!\n");
      exit(1);
   }
   if (wide==1) {
      output_active_height=(display_height*3)/4;
      // Common pitfall! it is 3/4 not 9/16!
      // Indeed, Standard ratio is 4:3, so 16:9 has an height that is 3/4 smaller than the display_height
      output_height_black_above=output_height_black_under=(display_height-output_active_height)/2;
      // For generalisation: check the division is right
      black=1;
   }
}
// *************************************************************************************

// *************************************************************************************
int main(int argc,char *argv[])
{
   char param_line[PARAM_LINE_MAX];
   const char help[]="-h";
   int n,nerr=0,len,res;
   unsigned long int i;
   int frame_rate_code;
   float frame_rate;
   int input_fd=0;
   long int frame_num=0;
   unsigned char magic[]="123456"; // : the last character of magic is the string termination character 
   const unsigned char key[]="FRAME\n";   
   unsigned int * height_coeff,* width_coeff;
   unsigned char * input,* output,* input_u,* input_v;
   unsigned char * black_y = NULL;
   unsigned char * black_uv = NULL;
   unsigned char * u_c_p; //u_c_p = unsigned char pointer
   unsigned long int black_pixels;
   unsigned int divider;
   FILE * in_file;
   FILE * out_file;
   char **filename;
   in_file=stdin;
   out_file=stdout;
   
   fprintf(stderr,"yuvscaler is a general downscaling utility for yuv frames\n"
	   "(C) 2001 Xavier Biquard <xbiquard@free.fr>\n"
	   "Launch %s -h for help\n",argv[0]);
   
   if ((argc==2)&&(strcmp(argv[1],help)==0)) print_usage(argv);
   
   for (i=1;i<argc;i++) {
      if (strncmp(argv[i],INFILE_KEYWORD,7)==0) {
	 // Data will come from file, not stdin
   	 infile=1;
	 filename[0]=argv[i]+7;
	 fprintf(stderr,"yuvcaler: reading input from file %s\n",filename[0]);
	 read_video_files(filename,1,&el);
	 chroma_format=el.MJPG_chroma;
	 if (chroma_format!=CHROMA422) {
	    fprintf(stderr,"Editlist not in chroma 422 format, exiting...\n");
	    exit(1);
	 }
	 input_width =el.video_width;
	 input_height=el.video_height;
	 frame_rate=el.video_fps;
	 if (frame_rate==25.0) {
	    frame_rate_code=3;
	    norm=0;
	 }
	 if (frame_rate==29.97) {
	    frame_rate_code=4;
	    norm=1;
	 }
	 if ((frame_rate_code!=3) && (frame_rate_code!=4)) {
	    fprintf(stderr,"Frames are neither PAL nor NTSC : size=%dx%d and frame_rate=%.3f fps!!\n",input_width,input_height,frame_rate);
	 exit(1);
	 }
	 i=argc; // end of the for loop
      }
   }
	 
   if (infile==0) {
      // Check for correct file header : taken from mpeg2enc
      for(n=0;n<PARAM_LINE_MAX;n++) {
	 if(!read(input_fd,param_line+n,1))	{
	    fprintf(stderr,"yuvscaler Error reading header from stdin\n");
	    exit(1);
	 }
	 if(param_line[n]=='\n') break;
      }
      if(n==PARAM_LINE_MAX)     {
	 fprintf(stderr,"yuvscaler Didn't get linefeed in first %d characters of data\n",
		 PARAM_LINE_MAX);
	 exit(1);
      }
      
      //   param_line[n] = 0; /* Replace linefeed by end of string */
      // 
      if(strncmp(param_line,"YUV4MPEG",8)) {
	 fprintf(stderr,"yuvscaler Input starts not with \"YUV4MPEG\"\n");
	 fprintf(stderr,"yuvscaler This is not a valid input for me\n");
	 exit(1);
      }
      
      
      sscanf(param_line+8,"%d %d %d",&input_width,&input_height,&frame_rate_code);
      nerr = 0;
      if(input_width<=0) {
	 fprintf(stderr,"yuvscaler Horizontal size illegal\n");
	 nerr++;
      }
      if(input_height<=0) {
	 fprintf(stderr,"yuvscaler Vertical size illegal\n");
	 nerr++;
      }
      if(frame_rate_code<1 || frame_rate_code>8) {
	 fprintf(stderr,"yuvscaler frame rate code illegal !!!!\n");
	 nerr++;
      }
      if(nerr) exit(1);
      // End of check for header
      // 
      // Are frame PAL or NTSC?
      if (frame_rate_code==3) {
	 norm=0;
      } else {
	 if (frame_rate_code==4) {
	    norm=1;
	 } else { 
	    fprintf(stderr,"Frames are neither PAL nor NTSC : size=%dx%d and frame_rate=%.3f fps!!\n",input_width,input_height,framerates[frame_rate_code]);
	    exit(1);
	 }
      }
   }
      
   // INITIALISATIONS
   input_useful_width =input_width;
   input_useful_height=input_height;
   input_offset_width=0;
   input_offset_height=0;
   handle_args_global(argc,argv);
   fprintf(stderr,"yuvscaler: from %ux%u, take %ux%u+%u+%u\ndownscaled to %ux%u, %ux%u being displayed\n",
	   input_width,input_height,input_useful_width,input_useful_height,input_offset_width,input_offset_height,
	   output_active_width,output_active_height,display_width,display_height);
   fprintf(stderr,"yuvscaler Frame rate code: %d\n",frame_rate_code);

   // Coherence check
   if ((input_useful_width<output_active_width) || (input_useful_height<output_active_height)) { 
      fprintf(stderr,"yuvscaler can only downscale, not upscale!!!!! STOP\n");
      exit(1);
   }

   if ((input_width==input_useful_width) && (input_height==input_useful_height) &&
       (input_width==output_active_width) && (input_height==output_active_height)) { 
      fprintf(stderr,"Frame size are OK! Nothing to do!!!!! STOP\n");
      exit(0);
   }

   // Output file header
   if (fprintf(stdout,"YUV4MPEG %3d %3d %1d\n",display_width,display_height,frame_rate_code)!=19) {
      fprintf(stderr,"Error writing output header. Stop\n");
      exit(1);
   }

   divider=pgcd(input_useful_width,output_active_width);
   input_width_slice =input_useful_width /divider;
   output_width_slice=output_active_width/divider;
   if (debug==1) fprintf(stderr,"divider,i_w_s,o_w_s = %d,%d,%d\n",divider,input_width_slice,output_width_slice);
   
   divider=pgcd(input_useful_height,output_active_height);
   input_height_slice =input_useful_height /divider;
   output_height_slice=output_active_height/divider;
   if (debug==1) fprintf(stderr,"divider,i_w_s,o_w_s = %d,%d,%d\n",divider,input_height_slice,output_height_slice);
   
   diviseur=input_height_slice*input_width_slice;
//   if (debug==1)   fprintf(stderr,"Diviseur=%ld\n",diviseur);
   fprintf(stderr,"Diviseur=%ld\n",diviseur);
   // To speed up downscaling, we tabulate the integral part of the division by "diviseur" which is inside [0;255] integral => unsigned short int storage class
   // but we do not tabulate the rest of this division since it may range up to diviseur-1 => unsigned long int storage class => too big in memory
   if (!(divide=(unsigned short int *)malloc((unsigned long int)256*diviseur*sizeof(unsigned short int)))) { 
      fprintf(stderr,"Error divide allocating memory. Stop\n");
      exit(1);
   }
   u_i_p=divide;
   for (i=0;i<256*diviseur;i++) {
      *(u_i_p++)=i/diviseur;
//      fprintf(stderr,"%ld: %d\r",i,(unsigned short int)(i/diviseur));
   }
   
   // Calculate averaging coefficient
   // For the height
   if (!(height_coeff=malloc((input_height_slice+1)*output_height_slice*sizeof(unsigned int)))){
      fprintf(stderr,"Error Height allocating memory. Stop\n");
      exit(1);
   }
   average_coeff(input_height_slice,output_height_slice,height_coeff);
   
   // For the width
   if (!(width_coeff=malloc((input_width_slice+1)*output_width_slice*sizeof(unsigned int)))){
      fprintf(stderr,"Error Width allocating memory. Stop\n");
      exit(1);
   }
   average_coeff(input_width_slice,output_width_slice,width_coeff);


   // Pointers allocations
   input=malloc((input_width*input_height*3)/2);
   output=malloc((output_active_width*output_active_height*3)/2);
   if (!input || !output) {
      fprintf(stderr,"Error input-output allocating memory. Stop\n");
      exit(1);
   }
   
   if (black==1) {
      black_pixels=(output_height_black_above>=output_height_black_under ? output_height_black_above : output_height_black_under)*display_width; 
      black_y=malloc(black_pixels);
      black_uv=malloc(black_pixels/4);
      if (!black_y || !black_uv ) {
	 fprintf(stderr,"Error output-black allocating memory. Stop\n");
	 exit(1);
      }
      // BLACK pixel in YUV = (16,128,128)
      u_c_p=black_y;
      for (i=0;i<black_pixels;i++) {
	 *u_c_p=16; u_c_p++; 
      }
      u_c_p=black_uv;
      for (i=0;i<black_pixels/4;i++) {
	 *u_c_p=128; u_c_p++; 
      }
   }
   
   if (debug==1) fprintf(stderr,"End of Initialisation\n");
   // END OF INITIALISATION
   
   
   // Frama by frame read and downscale
   if (infile==1) {
      input_u=input+input_width*input_height;
      input_v=input_u+input_width/2*input_height/2;
      for (frame_num=0;frame_num<el.video_frames;frame_num++) {
	 // taken from lav2yuv
	 len=el_get_video_frame(jpeg_data,frame_num,&el);
	 res=decode_jpeg_raw(jpeg_data, len, el.video_inter,chroma_format,input_width,input_height,input,input_u,input_v);
	 // Output key word
	 if(fwrite(key,6,1,out_file)!=1){fprintf(stderr,"key write failed\n");exit(1);}
	 // DOWNSCALING
	 average_Y (input,output,height_coeff,width_coeff);
	 average_UV(input+input_height*input_width,output+output_active_height*output_active_width,height_coeff,width_coeff);
	 average_UV(input+(input_height*input_width*5)/4,output+(output_active_height*output_active_width)*5/4,height_coeff,width_coeff);
	 // OUTPUT FRAME
	 if (black==0) {
	    // Here, display=output_active 
	    if (fwrite(output,(output_active_width*output_active_height*3)/2,1,out_file)!=1){fprintf(stderr,"Frame %ld output failed\n",frame_num);exit(1);}
	 } else {
	    // There are black lines to be displayed on top on each component
	    // Y Component
	    if (fwrite(black_y,output_height_black_above*output_active_width,1,out_file)!=1){fprintf(stderr,"black Y above write failed\n");exit(1);}
	    if (fwrite(output,output_active_width*output_active_height,1,out_file)!=1){fprintf(stderr,"Frame %ld Y compo write failed\n",frame_num);exit(1);}
	    if (fwrite(black_y,output_height_black_under*output_active_width,1,out_file)!=1){fprintf(stderr,"black Y under write failed\n");exit(1);}
	    // U Component
	    u_c_p=output+output_active_width*output_active_height;
	    if (fwrite(black_uv,output_height_black_above/2*output_active_width/2,1,out_file)!=1){fprintf(stderr,"black U above write failed\n");exit(1);}
	    if (fwrite(u_c_p,output_active_width/2*output_active_height/2,1,out_file)!=1){fprintf(stderr,"Frame %ld U compo write failed\n",frame_num);exit(1);}
	    if (fwrite(black_uv,output_height_black_under/2*output_active_width/2,1,out_file)!=1){fprintf(stderr,"black U under write failed\n");exit(1);}
	    // V Component
	    u_c_p+=output_active_width/2*output_active_height/2;
	    if (fwrite(black_uv,output_height_black_above/2*output_active_width/2,1,out_file)!=1){fprintf(stderr,"black V above write failed\n");exit(1);}
	    if (fwrite(u_c_p,output_active_width/2*output_active_height/2,1,out_file)!=1){fprintf(stderr,"Frame %ld V compo write failed\n",frame_num);exit(1);}
	    if (fwrite(black_uv,output_height_black_under/2*output_active_width/2,1,out_file)!=1){fprintf(stderr,"black V under write failed\n");exit(1);}
	 }
	 if (debug==1) fprintf(stderr,"yuvscaler Frame number %ld\r",frame_num);
      }
   } else {
      // input comes from stdin
      // Master loop : continue until there is no next frame in stdin
      while((fread(magic,6,1,in_file)==1)&&(strcmp(magic,key)==0)) {
	 // Output key word
	 if(fwrite(key,6,1,out_file)!=1){fprintf(stderr,"key write failed\n");exit(1);}
	 
	 // Frame by Frame read and down-scale
	 frame_num++;
	 if (fread(input,(input_height*input_width*3)/2,1,in_file)!=1){fprintf(stderr,"Frame %ld read failed\n",frame_num);exit(1);}
	 average_Y (input,output,height_coeff,width_coeff);
	 average_UV(input+input_height*input_width,output+output_active_height*output_active_width,height_coeff,width_coeff);
	 average_UV(input+(input_height*input_width*5)/4,output+(output_active_height*output_active_width)*5/4,height_coeff,width_coeff);
	 
	 if (black==0) {
	    // Here, display=output_active 
	    if (fwrite(output,(output_active_width*output_active_height*3)/2,1,out_file)!=1){fprintf(stderr,"Frame %ld output failed\n",frame_num);exit(1);}
	 } else {
	    // There are black lines to be displayed on top on each component
	    // Y Component
	    if (fwrite(black_y,output_height_black_above*output_active_width,1,out_file)!=1){fprintf(stderr,"black Y above write failed\n");exit(1);}
	    if (fwrite(output,output_active_width*output_active_height,1,out_file)!=1){fprintf(stderr,"Frame %ld Y compo write failed\n",frame_num);exit(1);}
	    if (fwrite(black_y,output_height_black_under*output_active_width,1,out_file)!=1){fprintf(stderr,"black Y under write failed\n");exit(1);}
	    // U Component
	    u_c_p=output+output_active_width*output_active_height;
	    if (fwrite(black_uv,output_height_black_above/2*output_active_width/2,1,out_file)!=1){fprintf(stderr,"black U above write failed\n");exit(1);}
	    if (fwrite(u_c_p,output_active_width/2*output_active_height/2,1,out_file)!=1){fprintf(stderr,"Frame %ld U compo write failed\n",frame_num);exit(1);}
	    if (fwrite(black_uv,output_height_black_under/2*output_active_width/2,1,out_file)!=1){fprintf(stderr,"black U under write failed\n");exit(1);}
	    // V Component
	    u_c_p+=output_active_width/2*output_active_height/2;
	    if (fwrite(black_uv,output_height_black_above/2*output_active_width/2,1,out_file)!=1){fprintf(stderr,"black V above write failed\n");exit(1);}
	    if (fwrite(u_c_p,output_active_width/2*output_active_height/2,1,out_file)!=1){fprintf(stderr,"Frame %ld V compo write failed\n",frame_num);exit(1);}
	    if (fwrite(black_uv,output_height_black_under/2*output_active_width/2,1,out_file)!=1){fprintf(stderr,"black V under write failed\n");exit(1);}
	 }
	 if (debug==1) fprintf(stderr,"yuvscaler Frame number %ld\r",frame_num);
      }
      // End of master loop
   }
   return 0;
}
   
// *************************************************************************************
int average_coeff(unsigned int input_length,unsigned int output_length,unsigned int *coeff) {
   // This function calculates multiplicative coeeficients to average an input (vector of) length
   // input_length into an output (vector of) length output_length;
   // We sequentially store the number-of-non-zero-coefficients, followed by the coefficients
   // themselvesc, and that, output_length time
   int last_coeff=0,remaining_coeff,still_to_go=0,in,out,non_zero=0,nb;
   int i,j;
   unsigned int *non_zero_p = NULL;
   unsigned int *pointer;
   
   if ((output_length>input_length) || (input_length<=0) || (output_length<=0)
       || (coeff==0)) {
      fprintf(stderr,"Function average_coeff : arguments are wrong\n");
      fprintf(stderr,"input length = %d, output length = %d, input = %p\n",
	      input_length,output_length,coeff);
      exit(1);
   }

   if (debug==1) fprintf(stderr,"Function average_coeff : input length = %d, output length = %d, input = %p\n",
		      input_length,output_length,coeff);
   
   pointer=coeff;
   
   if (output_length==1) {
      *pointer=input_length;
      pointer++;
      for (in=0;in<input_length;in++) {
	 *pointer=1;
	 pointer++;
      }
   } else {
      for (in=0;in<output_length;in++) {
	 non_zero=0;
	 non_zero_p=pointer;
	 pointer++;
	 still_to_go=input_length;
	 if (last_coeff>0) {
	    remaining_coeff=output_length-last_coeff;
	    *pointer=remaining_coeff;
	    pointer++;
	    non_zero++;
	    still_to_go-=remaining_coeff;
	 }
	 nb=(still_to_go/output_length);
	 if (debug==1) fprintf(stderr,"in=%d,nb=%d,still_to_go=%d\n",in,nb,still_to_go);
	 for (out=0;out<nb;out++) {
	    *pointer=output_length;
	    pointer++;
	 }
	 still_to_go-=nb*output_length;
	 non_zero+=nb;
	 
	 if ((last_coeff=still_to_go)!=0) {
	    *pointer=last_coeff;
	    if (debug==1) fprintf(stderr,"non_zero=%d,last_coeff=%d\n",non_zero,last_coeff);
	    pointer++; // now pointer points onto the next number-of-non_zero-coefficients
	    non_zero++;
	    *non_zero_p=non_zero;
	 } else {
	    if (in!=output_length-1) {
	       fprintf(stderr,"There is a common divider between %d and %d\n This should not be the case\n",input_length,output_length);
	       exit(1);
	    }
	 }
	 
      }
      *non_zero_p=non_zero;
      
      if (still_to_go!=0) {
	 fprintf(stderr,"Function average_coeff : calculus doesn't stop right : %d\n",still_to_go);
      }
   }
   if (debug==1) {
      for (i=0;i<output_length;i++) {
	 fprintf(stderr,"line=%d\n",i);
	 non_zero=*coeff; coeff++;
	 for (j=0;j<non_zero;j++) {
	    fprintf(stderr,"%d : %d ",j,*coeff);
	    coeff++;
	 }
	 fprintf(stderr,"\n");
      }
   }
   return(0);
}
// *************************************************************************************



// *************************************************************************************
int average_Y(unsigned char * input,unsigned char * output, unsigned int * height_coeff, unsigned int * width_coeff) {
   // This function average an input matrix of name input that contains the Y component (size input_width*input_height, useful size input_useful_width*input_useful_height)
   // into an output matrix of name output of size output_active_width*output_active_height
   unsigned char *input_line_p[input_height_slice];
   unsigned char *output_line_p[output_height_slice];
   unsigned int *H_var, *W_var, *H, *W;
   unsigned char *u_c_p;
   int j,nb_H,nb_W,in_line,out_line,out_col_slice,out_nb_col_slice,out_col,out_line_slice,out_nb_line_slice,current_line,last_line;
   unsigned long int round_off_error=0,value=0;
   unsigned short int divi;

   //Init
   if (debug==1) fprintf(stderr,"Start of average_Y\n");
   out_nb_col_slice=output_active_width/output_width_slice;
   out_nb_line_slice=output_active_height/output_height_slice;
   input+=input_offset_height*input_width+input_offset_width;
   //End of INIT
   

   if ((output_height_slice!=1)||(input_height_slice!=1)) {
      for (out_line_slice=0;out_line_slice<out_nb_line_slice;out_line_slice++) {
	 u_c_p=input+out_line_slice*input_height_slice*input_width;
	 for (in_line=0;in_line<input_height_slice;in_line++) {
	    input_line_p[in_line]=u_c_p;
	    u_c_p+=input_width;
	 }
	 u_c_p=output+out_line_slice*output_height_slice*output_active_width;
	 for (out_line=0;out_line<output_height_slice;out_line++) {
	    output_line_p[out_line]=u_c_p;
	    u_c_p+=output_active_width;
	 }
	 
	 for (out_col_slice=0;out_col_slice<out_nb_col_slice;out_col_slice++) {
	    H=height_coeff;
	    in_line=0;
	    for (out_line=0;out_line<output_height_slice;out_line++) {
	       nb_H=*H;
	       W=width_coeff;
	       for (out_col=0;out_col<output_width_slice;out_col++) {
		  H_var=H+1;
		  nb_W=*W;
		  value=round_off_error;
		  last_line=in_line+nb_H;
		  for (current_line=in_line;current_line<last_line;current_line++) { 
		     W_var=W+1;
		     // we average nb_W columns of input : we increment input_line_p[current_line] and W_var each time, except for the last value where 
		     // input_line_p[current_line] and W_var do not need to be incremented, but H_var does
		     for (j=0;j<nb_W-1;j++) 
		       value+=(*H_var)*(*W_var++)*(*input_line_p[current_line]++);
		     value+=(*H_var++)*(*W_var)*(*input_line_p[current_line]);
		  }
		  //		  Straiforward implementation is 
		  //	          *(output_line_p[out_line]++)=value/diviseur;
		  //		  round_off_error=value%diviseur;
		  //		  Here, we speed up things but using the pretabulated integral parts
		  divi=*(divide+value);
		  *(output_line_p[out_line]++)=divi;
		  round_off_error=value-divi*diviseur;
		  W+=nb_W+1;
	       }
	       H+=nb_H+1;
	       in_line+=nb_H-1;
	       input_line_p[in_line]-=input_width_slice-1; // If last line of input is to be reused in next loop, 
	       // make the pointer points at the correct place
	    }
	    if (in_line!=input_height_slice-1) {fprintf(stderr,"Error line conversion\n"); exit(1);}
	    else input_line_p[in_line]+=input_width_slice-1;
	    for (in_line=0;in_line<input_height_slice;in_line++) 
	      input_line_p[in_line]++;
	 }
      }
   } else {
      // We just take the average along the width, not the height, line per line
      for (out_line_slice=0;out_line_slice<output_active_height;out_line_slice++) {
	 input_line_p[0] =input+ out_line_slice*input_width;
	 output_line_p[0]=output+out_line_slice*output_active_width;
	 for (out_col_slice=0;out_col_slice<out_nb_col_slice;out_col_slice++) {
	    W=width_coeff;
	    for (out_col=0;out_col<output_width_slice;out_col++) {
	       nb_W=*W;
	       value=round_off_error;
	       W_var=W+1;
	       for (j=0;j<nb_W-1;j++) 
		 value+=(*W_var++)*(*input_line_p[0]++);
	       value+=(*W_var)*(*input_line_p[0]);
	       divi=*(divide+value);
	       *(output_line_p[0]++)=divi;
	       round_off_error=value-divi*diviseur;
	       W+=nb_W+1;
	    }
	    input_line_p[0]++;
	 }
      }
   }
   if (debug==1) fprintf(stderr,"End of average_Y\n");
   return(0);
}
// *************************************************************************************

// *************************************************************************************
int average_UV(unsigned char * input,unsigned char * output, unsigned int * height_coeff, unsigned int * width_coeff) {
   // This function average an input matrix of name input that containseither the U or the V component (size input_height/2*input_width/2)
   // into an output matrix of name output of size output_active_height/2*output_active_width/2
   unsigned char *input_line_p[input_height_slice];
   unsigned char *output_line_p[output_height_slice];
   unsigned int *H_var, *W_var, *H, *W;
   unsigned char *u_c_p;
   int j,nb_H,nb_W,in_line,out_line,out_col_slice,out_nb_col_slice,out_col,out_line_slice,out_nb_line_slice,current_line,last_line;
   unsigned long int round_off_error=0,value;
   unsigned short int divi;

   //Init
   if (debug==1) fprintf(stderr,"Start of average_UV\n");
   out_nb_line_slice=output_active_height/2/output_height_slice;
   out_nb_col_slice=output_active_width/2/output_width_slice;
   input+=input_offset_height/2*input_width/2+input_offset_width/2;
   //End of INIT
   
   if ((output_height_slice!=1)||(input_height_slice!=1)) {
      for (out_line_slice=0;out_line_slice<out_nb_line_slice;out_line_slice++) {
	 u_c_p=input+out_line_slice*input_height_slice*input_width/2;
	 for (in_line=0;in_line<input_height_slice;in_line++) {
	    input_line_p[in_line]=u_c_p;
	    u_c_p+=input_width/2;
	 }
	 u_c_p=output+out_line_slice*output_height_slice*output_active_width/2;
	 for (out_line=0;out_line<output_height_slice;out_line++) {
	    output_line_p[out_line]=u_c_p;
	    u_c_p+=output_active_width/2;
	 }
	 
	 for (out_col_slice=0;out_col_slice<out_nb_col_slice;out_col_slice++) {
	    H=height_coeff;
	    in_line=0;
	    for (out_line=0;out_line<output_height_slice;out_line++) {
	       nb_H=*H;
	       W=width_coeff;
	       for (out_col=0;out_col<output_width_slice;out_col++) {
		  H_var=H+1;
		  nb_W=*W;
		  value=round_off_error;
		  last_line=in_line+nb_H;
		  for (current_line=in_line;current_line<last_line;current_line++) { 
		     W_var=W+1;
		     for (j=0;j<nb_W-1;j++) 
		       value+=(*H_var)*(*W_var++)*(*input_line_p[current_line]++);
		     value+=(*H_var++)*(*W_var)*(*input_line_p[current_line]);
		  }
		  u_i_p=divide+2*value;
		  divi=*(divide+value);
		  *(output_line_p[out_line]++)=divi;
		  round_off_error=value-divi*diviseur;
		  W+=nb_W+1;
	       }
	       H+=nb_H+1;
	       in_line+=nb_H-1;
	       input_line_p[in_line]-=input_width_slice-1; // If last line of input is to be reused in next loop, 
	       // make the pointer points at the correct place
	    }
	    if (in_line!=input_height_slice-1) {fprintf(stderr,"Error line conversion\n"); exit(1);}
	    else input_line_p[in_line]+=input_width_slice-1;
	    for (in_line=0;in_line<input_height_slice;in_line++) 
	      input_line_p[in_line]++;
	 }
      }
   } else {
      // We just take the average along the width, not the height, line per line
      for (out_line_slice=0;out_line_slice<output_active_height/2;out_line_slice++) {
	 input_line_p[0] =input+out_line_slice*input_width/2;
	 output_line_p[0]=output+out_line_slice*output_active_width/2;
	 for (out_col_slice=0;out_col_slice<out_nb_col_slice;out_col_slice++) {
	    W=width_coeff;
	    for (out_col=0;out_col<output_width_slice;out_col++) {
	       nb_W=*W;
	       value=round_off_error;
	       W_var=W+1;
	       for (j=0;j<nb_W-1;j++)
		 value+=(*W_var++)*(*input_line_p[0]++);
	       value+=(*W_var)*(*input_line_p[0]);
	       divi=*(divide+value);
	       *(output_line_p[0]++)=divi;
	       round_off_error=value-divi*diviseur;
	       W+=nb_W+1;
	    }
	    input_line_p[0]++;
	 }
      }
   }
      
   if (debug==1) fprintf(stderr,"End of average_UV\n");
   return(0);
}
// *************************************************************************************

// *************************************************************************************
unsigned int pgcd(unsigned int num1,unsigned int num2) {
   // Calculates the biggest common divider between num1 and num2, with num2<=num1
   unsigned int i;
   for (i=num2;i>=2;i--) {
      if ( ((num2%i)==0) && ((num1%i)==0) ) 
	return(i);
   }
   return(1);
}
// *************************************************************************************
   
  
  
  
