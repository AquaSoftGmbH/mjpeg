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

#define PARAM_LINE_MAX 256
float framerates[] = { 0, 23.976, 24.0, 25.0, 29.970, 30.0, 50.0, 59.940, 60.0 };

// Global variables
unsigned int output_height_black_above;
unsigned int output_height_black_under;
unsigned int black=0; // =1 if black lines must be generated on output
unsigned int input_height_slice;
unsigned int output_height_slice;
unsigned int input_width_slice;
unsigned int output_width_slice;
unsigned int display_width;
unsigned int display_height;
unsigned int output_active_width;
unsigned int output_active_height;
//unsigned int input_active_width;
//unsigned int input_active_height;
unsigned int input_width;
unsigned int input_height;
//unsigned int skip_lines;
int norm=-1; // =0 for PAL and =1 for NTSC
int format=-1; // =0 for VCD and =1 for SVCD
int wide=0;  // =1 for wide (16:9) input to standard (4:3) output conversion
unsigned int debug=0; // =0 for no debugging informations, =1 for debugging informations
const char VCD_KEYWORD[] = "VCD";
const char SVCD_KEYWORD[] = "SVCD";

// *************************************************************************************
void print_usage(char *argv[])
{
   fprintf(stderr,"usage: cat [input_file] | %s -k [keyword] [-d] [-h] [-w] > [output_file]\n"
	   "%s downscales arbitrary sized yuv frames to VCD or SVCD yuv frames\n"
	   "respecting PAL and NTSC standard\n"
	   "Keyword is either 'VCD' or 'SVCD'\n"
	   "-w : converts widescreen (16:9) input into standard output (4:3)\n"
	   "-d : print debugging informations\n"
	   "-h : usage help\n"
	   ,argv[0],argv[0]);
   exit(1);
}
// *************************************************************************************

// *************************************************************************************
void handle_args_global (int argc, char *argv[])
{
   // This function takes care of the global variables initialisation as a function of command line options
   int c;
   
   // Default values 
//   output_height_black_above=output_height_black_under=skip_lines=0;
   // End of default values
   
   
   while((c = getopt(argc,argv,"k:wdh")) != -1)
     {
	switch (c)
	  {
	   case 'k':
	     if (strcmp(optarg,VCD_KEYWORD)==0) {
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
	     break;
	     
	   case 'd':
	     debug=1;
	     break;
	     
	   case 'w':
	     wide=1;
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
   int i,n,nerr=0,diff_col=0,line,yuv,component;
   int frame_rate_code;
   int input_fd=0;
   long int frame_num=0;
   unsigned char magic[]="123456"; // : the last character of magic is the string termination character 
   const unsigned char key[]="FRAME\n";   
   unsigned int * height_coeff,* width_coeff;
   unsigned char * input,* output;
   unsigned char * black_y,* black_uv;
//   unsigned char * skip;
   unsigned char * u_c_p; //u_c_p = unsigned char pointer
   unsigned long int black_pixels;
   unsigned int divider;
   FILE * in_file;
   FILE * out_file;
   in_file=stdin;
   out_file=stdout;
   
   fprintf(stderr,"yuvscaler downscales arbitrary sized yuv frames to VCD or SVCD yuv frames\n"
	   "respecting PAL and NTSC standard, taking into account widescreen coding\n"
	   "(C) 2001 Xavier Biquard <xbiquard@free.fr>\n"
	   "Launch %s -h for help\n",argv[0]);
   
   if ((argc==2)&&(strcmp(argv[1],help)==0)) print_usage(argv);
   
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
   

   // INITIALISATIONS

   handle_args_global(argc,argv);
   fprintf(stderr,"yuvscaler Horizontal size: from %d to %d, %d displayed\n",input_width,output_active_width,display_width);
   fprintf(stderr,"yuvscaler   Vertical size: from %d to %d, %d displayed\n",input_height,output_active_height,display_height);
   fprintf(stderr,"yuvscaler Frame rate code: %d\n",frame_rate_code);

   // Coherence check
   if ((input_width<output_active_width) || (input_height<output_active_height)) { 
      fprintf(stderr,"yuvscaler can only downscale, not upscale!!!!! STOP\n");
      exit(1);
   }

   if ((input_width==output_active_width) && (input_height==output_active_height)) { 
      fprintf(stderr,"Frame size are OK! Nothing to do!!!!! STOP\n");
      exit(0);
   }

   // Output file header
   if (fprintf(stdout,"YUV4MPEG %3d %3d %1d\n",display_width,display_height,frame_rate_code)!=19) {
      fprintf(stderr,"Error writing output header. Stop\n");
      exit(1);
   }

   divider=pgcd(input_width,output_active_width);
   input_width_slice=input_width/divider;
   output_width_slice=output_active_width/divider;
   if (debug==1) fprintf(stderr,"divider,i_w_s,o_w_s = %d,%d,%d\n",divider,input_width_slice,output_width_slice);
   
   divider=pgcd(input_height,output_active_height);
   input_height_slice=input_height/divider;
   output_height_slice=output_active_height/divider;
   if (debug==1) fprintf(stderr,"divider,i_w_s,o_w_s = %d,%d,%d\n",divider,input_height_slice,output_height_slice);
   
   
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
   input=malloc(input_width*input_height);
   output=malloc(output_active_width*output_active_height);
   if (!input || !output) {
      fprintf(stderr,"Error input-output allocating memory. Stop\n");
      exit(1);
   }
   
/*   if((skip_lines!=0) && !(skip=malloc(skip_lines*input_width))) {
      fprintf(stderr,"Error skip allocating memory. Stop\n");
      exit(1);
   }
   
   // real input_height
   if (skip_lines!=0) 
     input_height-=2*skip_lines;   
*/
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
      
   // Number of columns that will not be displayed on screen
   diff_col=output_active_width-display_width;
   if (debug==1) fprintf(stderr,"Diff_col = %d\n",diff_col);
   
   // END OF INITIALISATION
   
   //   fprintf(stderr,"End of Initialisation\n");

   // Master loop : continue until there is no next frame in stdin
   while((fread(magic,6,1,in_file)==1)&&(strcmp(magic,key)==0)) {
      // Output key word
      if(fwrite(key,6,1,out_file)!=1){fprintf(stderr,"key write failed\n");exit(1);}
      
      // Data are read and treated frame by frame : we first treate all the Y component frame,
      // then the U & V components
      for (component=0;component<3;component++) {
	 if (component==0) {// Y component
	    yuv=1; 
	 } else {  // U or V component
	    yuv=2;
	 }
	 
/*	 
	 // skip necessary lines : use of the fseek function is not possible here
	 if (skip_lines!=0)
	   if (fread(skip,(unsigned int)input_width/yuv*skip_lines/yuv,1,in_file)!=1){fprintf(stderr,"skip read failed\n");exit(1);}
*/	 
	 // output necessary black lines
	 if (black==1) {
	    if (yuv==1) {
	       if (fwrite(black_y,output_height_black_above*display_width,1,out_file)!=1){fprintf(stderr,"black write failed\n");exit(1);}
	    } else { 
	       if (fwrite(black_uv,output_height_black_above/2*display_width/2,1,out_file)!=1){fprintf(stderr,"black write failed\n");exit(1);};
	    }
	 }

	 // Frame by Frame read and down-scale
	 if (fread(input,input_height/yuv*input_width/yuv,1,in_file)!=1){fprintf(stderr,"Frame %ld read failed\n",frame_num);exit(1);}
	 average(input,output,height_coeff,width_coeff,yuv);
	 
	 // Frame output
	 u_c_p=output+diff_col/2/yuv;
	 for (line=0;line<output_active_height/yuv;line++) {
	    if (fwrite(u_c_p,display_width/yuv,1,out_file)!=1){fprintf(stderr,"Reduced col width write failed\n");exit(1);}
	    u_c_p+=output_active_width/yuv;
	 }
	 
/*	 // skip necessary lines
	 if (skip_lines!=0)
	   if (fread(skip,(unsigned int)input_width/yuv*skip_lines/yuv,1,in_file)!=1){fprintf(stderr,"skip read failed\n");exit(1);}
*/	 
	 // output necessary black lines
	 if (black==1) {
	    if (yuv==1) {
	       if (fwrite(black_y,output_height_black_under*display_width,1,out_file)!=1){fprintf(stderr,"black write failed\n");exit(1);}
	    } else { 
	       if (fwrite(black_uv,output_height_black_under/2*display_width/2,1,out_file)!=1){fprintf(stderr,"black write failed\n");exit(1);};
	    }
	 }

      }
      // End of data YUV treatment
      if (debug==1) fprintf(stderr,"yuvscaler Frame number %ld\r",frame_num++);
   }
   // End of master loop
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
   unsigned int *non_zero_p;
   unsigned int *pointer;
   
   if ((output_length>input_length) || (input_length<=0) || (output_length<=0)
       || (coeff==0)) {
      fprintf(stderr,"Function average_coeff : arguments are wrong\n");
      fprintf(stderr,"input length = %d, output length = %d, input = %p\n",
	      input_length,output_length,coeff);
      exit(1);
   }

   if (debug==1) fprintf(stderr,"Function average : input length = %d, output length = %d, input = %p\n",
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
	 fprintf(stderr,"Function average : calculus doesn't stop right : %d\n",still_to_go);
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
int average(unsigned char * input,unsigned char * output, unsigned int * height_coeff, unsigned int * width_coeff,int yuv) {
   unsigned char *input_line_p[input_height_slice];
   unsigned char *output_line_p[output_height_slice];
   unsigned int *H_var, *W_var, *H, *W;
   unsigned char *u_c_p;
   int j,nb_H,nb_W,in_line,out_line,out_col_slice,out_nb_col_slice,out_col,out_line_slice,out_nb_line_slice,current_line,last_line;
   unsigned long int round_off_error=0,value,diviseur;

   //Init
   if (debug==1) fprintf(stderr,"Start of average\n");
   diviseur=input_height_slice*input_width_slice;
   //End of INIT
   
   out_nb_line_slice=output_active_height/yuv/output_height_slice;
   for (out_line_slice=0;out_line_slice<out_nb_line_slice;out_line_slice++) {
      u_c_p=input+out_line_slice*input_height_slice*input_width/yuv;
      for (in_line=0;in_line<input_height_slice;in_line++) {
	 input_line_p[in_line]=u_c_p;
	 u_c_p+=input_width/yuv;
      }
      u_c_p=output+out_line_slice*output_height_slice*output_active_width/yuv;
      for (out_line=0;out_line<output_height_slice;out_line++) {
	 output_line_p[out_line]=u_c_p;
	 u_c_p+=output_active_width/yuv;
      }

      out_nb_col_slice=output_active_width/yuv/output_width_slice;
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
		  for (j=0;j<nb_W;j++) // we average nb_W columns of input : we increment input_line_p[current_line] and W-var each time
		     value+=(*H_var)*(*W_var++)*(*input_line_p[current_line]++);
		  H_var++;
		  input_line_p[current_line]--; // Une fois de trop dans la boucle de j, mais on ne met pas de test dans cette
		  // boucle car c'est trop lent, on incrémente current_line
	       }
	       round_off_error=value%diviseur;
	       *(output_line_p[out_line]++)=value/diviseur;
	       W+=nb_W+1;
	    }
	    H+=nb_H+1;
	    in_line+=nb_H-1;
	    input_line_p[in_line]-=input_width_slice-1; // Si la dernière ligne en input est à réutiliser, on replace le pointeur où il faut
	 }
	 if (in_line!=input_height_slice-1) {fprintf(stderr,"Error line conversion\n"); exit(1);}
	 else input_line_p[in_line]+=input_width_slice-1;
	 for (in_line=0;in_line<input_height_slice;in_line++) 
	   input_line_p[in_line]++;
      }
      if (debug==1) fprintf(stderr,"End of average\n");
   }
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
   
  
  
  
