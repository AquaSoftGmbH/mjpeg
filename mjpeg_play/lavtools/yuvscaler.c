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
#include "mjpeg_logging.h"
#include "inttypes.h"

int verbose=1;

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
int infile=0; // =0 for stdin (default) =1 for file

// taken from lav2yuv
EditList el;
#define MAX_EDIT_LIST_FILES 256
#define MAX_JPEG_LEN (512*1024)
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
	fprintf(stderr,"usage: %s [-n p|s|n] -I [input_keyword] -M [mode_keyword] -O [output_keyword] [-d] [-h] [lav_input_file]\n"
			"%s downscales arbitrary sized yuv frames to a specified format\n\n"
			"%s is keyword driven :\n"
			"\t -I for keyword concerning INPUT  frame characteristics\n"
			"\t -M for keyword concerning the downscaling MODE of yuvscaler\n"
			"\t -O for keyword concerning OUTPUT frame characteristics\n"
			"\t ((the former syntax with -k and -w is still supported but depreciated))\n"
			"\n"
			"-n  Specifies the OUTPUT norm for VCD/SVCD p=pal,s=secam,n=ntsc\n"
			"Possible input keyword is USE_WidthxHeight+WidthOffset+HeightOffset\n"
			"to select a useful area of the input frame\n"
			"Possible mode keyword is WIDE2STD to converts widescreen (16:9)\n"
			"input frames into standard output (4:3), generating necessary black lines\n"
			"Possible output keywords are:\n"
			"\t VCD to generate  VCD compliant frames on output\n"
			"           (taking care of PAL and NTSC standards)\n"
			"\tSVCD to generate SVCD compliant frames on output\n"
            "           (taking care of PAL and NTSC standards)\n"
			"\tSIZE_WidthxHeight to generate frames of size WidthxHeight on output\n"
			"-h : this usage help\n"
			,argv[0],argv[0],argv[0]);
	exit(1);
}
// *************************************************************************************

// *************************************************************************************

static char *legal_opt_flags = "k:I:d:n:v:M:O:wdh";

void handle_args_global (int argc, char *argv[])
{
	// This function takes care of the global variables 
	// initialisation that are independent of the input stream
	int c;

	// Default values 
	//   output_height_black_above=output_height_black_under=skip_lines=0;
	// End of default values

	while((c = getopt(argc,argv,legal_opt_flags)) != -1)
	{
		switch (c)
		{
		case 'v' :
			verbose = atoi(optarg);
			if( verbose < 0 || verbose > 2 )
			{
				mjpeg_error_exit1( "Verbose level must be [0..2]\n");
			}
			break;


		case 'n' : // TV norm for SVCD/VCD output
			switch( *optarg )
			{
			case 'p' :
			case 's' :
				norm = 1;
				break;
			case 'n' :
				norm = 0;
				break;
			default:
				mjpeg_error_exit1( "Illegal norm letter specified: %c\n", *optarg );
			}

		case 'h':
		case '?':
			print_usage(argv);
			break;

		default:
		}
	}
}

void handle_args_dependent (int argc, char *argv[])
{
	// This function takes care of the global variables 
	// initialisation that may depend on the input stream
	int c;
	unsigned int ui1,ui2,ui3,ui4;
	int output,input,mode;

	// Default values 
	//   output_height_black_above=output_height_black_under=skip_lines=0;
	// End of default values


	optind = 1;
	while((c = getopt(argc,argv,legal_opt_flags)) != -1)
	{
		switch (c)
		{
		case 'k':  // Compatibility reasons
		case 'O':  // OUTPUT
			output=0;
			if (strcmp(optarg,VCD_KEYWORD)==0)
			{
				output=1;
				format=0;
				output_active_width =display_width =352;
				if (norm==0)
				{
					mjpeg_info("VCD output format requested in PAL norm\n");
					output_active_height=display_height=288;
				}
				else if (norm==1)
				{
					mjpeg_info("VCD output format requested in NTSC norm\n");
					output_active_height=display_height=240;
				}
				else
					mjpeg_error_exit1( "No norm specified cannot set VCD output!\n");
					
			}
			if (strcmp(optarg,SVCD_KEYWORD)==0)
			{
				output=1;
				format=1;
				output_active_width =display_width =480;
				if (norm==0)
				{
					mjpeg_info("SVCD output format requested in PAL norm\n");
					output_active_height=display_height=576;
				}
				else if (norm==1)
				{
					mjpeg_info("SVCD output format requested in NTSC norm\n");
					output_active_height=display_height=480;
				}
				else
					mjpeg_error_exit1( "No norm specified cannot set VCD output!\n");
			}
			if (strncmp(optarg,SIZE_KEYWORD,5)==0)
			{
				output=1;
				format=2;
				if (sscanf(optarg,"SIZE_%ux%u",&ui1,&ui2)==2)
				{
					output_active_width=display_width=ui1;
					output_active_height=display_height=ui2;
				} 
				else 
					mjpeg_error_exit1("Uncorrect SIZE keyword: %s\n",optarg);
			}
			if (output==0)
				mjpeg_error_exit1("Uncorrect output keyword: %s\n",optarg);
			break;

		case 'w': // Compatibility reasons
			wide=1;
			break;

		case 'M':  // MODE
			mode=0;
			if (strcmp(optarg,WIDE2STD_KEYWORD)==0)
			{
				mode=1;
				wide=1;
			}
			if (mode==0)
				mjpeg_error("Uncorrect mode argument %s\nSkipping argument\n",optarg);
			break;
			

		case 'I': // INPUT
			input=0;
			if (strncmp(optarg,USE_KEYWORD,4)==0)
			{
				input=1;
				format=3;
				if (sscanf(optarg,"USE_%ux%u+%u+%u",&ui1,&ui2,&ui3,&ui4)==4)
				{
					// Coherence check : offsets must be multiple of 2 since U and V have half Y resolution
					// and the required zone must be inside the input size
					if ((ui1%2==0) && (ui2%2==0) && (ui3%2==0) && (ui4%2==0) && 
						(ui1+ui3<=input_width) && (ui2+ui4<=input_height))
					{
						input_useful_width=ui1;
						input_useful_height=ui2;
						input_offset_width=ui3;
						input_offset_height=ui4;
					} else 
						mjpeg_error_exit1("Unconsistent USE keywaord: %s\n",optarg);
				} else
					mjpeg_error_exit1("Uncorrect input flag argument: %s\n",optarg);
			}
			if (input==0)
				mjpeg_error_exit1("Uncorrect input flag argument: %s\n",optarg);
			break;

		default:
		}
	}
	if (format==-1)
	{
		mjpeg_error_exit1("No correct keyword given on command line!\n");
	}
	if (wide==1)
	{
		output_active_height=(display_height*3)/4;
		// Common pitfall! it is 3/4 not 9/16!
		// Indeed, Standard ratio is 4:3, so 16:9 has an height that is 3/4 smaller than the display_height
		output_height_black_above=output_height_black_under=(display_height-output_active_height)/2;
		// For generalisation: check the division is right
		black=1;
	}

}

static __inline__ int fwrite_complete( const uint8_t *buf, const int buflen, FILE *file )
{
	return fwrite( buf, 1, buflen, file) == buflen;
}


// *************************************************************************************

// *************************************************************************************
int main(int argc,char *argv[])
{
	char param_line[PARAM_LINE_MAX];
	int n,nerr=0;
	unsigned long int i;
	int frame_rate_code;
	int input_fd=0;
	long int frame_num=0;
	uint8_t magic[]="123456"; // : the last character of magic is the string termination character 
	const uint8_t key[]="FRAME\n";   
	unsigned int * height_coeff,* width_coeff;
	uint8_t *input, *output;
	uint8_t * black_y = NULL;
	uint8_t * black_uv = NULL;
	uint8_t * u_c_p; //u_c_p = uint8_t pointer
	unsigned long int black_pixels;
	unsigned int divider;
	FILE * in_file = stdin;
	FILE * out_file = stdout;

	mjpeg_info("yuvscaler is a general downscaling utility for yuv frames\n");
	mjpeg_info("(C) 2001 Xavier Biquard <xbiquard@free.fr>\n");
	mjpeg_info("%s -h for help\n",argv[0]);

	handle_args_global(argc,argv);
	mjpeg_default_handler_verbosity(verbose);

	// Check for correct file header : taken from mpeg2enc
	for(n=0;n<PARAM_LINE_MAX;n++)
	{
		if(!read(input_fd,param_line+n,1))
		{
			mjpeg_error("yuvscaler Unable to read header from stdin\n");

			exit(1);
		}
		if(param_line[n]=='\n') break;
	}

	if(n==PARAM_LINE_MAX)    
	{
		mjpeg_error("yuvscaler Didn't get linefeed in first %d characters of data\n",
				PARAM_LINE_MAX);
		exit(1);
	}

	//   param_line[n] = 0; /* Replace linefeed by end of string */
	// 
	if(strncmp(param_line,"YUV4MPEG",8))
	{
		mjpeg_error("yuvscaler Input starts not with \"YUV4MPEG\"\n");
		mjpeg_error("yuvscaler This is not a valid input for me\n");
		exit(1);
	}


	sscanf(param_line+8,"%d %d %d",&input_width,&input_height,&frame_rate_code);
	nerr = 0;
	if(input_width<=0)
	{
		mjpeg_error("yuvscaler Horizontal size illegal\n");
		nerr++;
	}
	if(input_height<=0)
	{
		mjpeg_error("yuvscaler Vertical size illegal\n");
		nerr++;
	}
	if(frame_rate_code<1 || frame_rate_code>8)
	{
		mjpeg_error("yuvscaler frame rate code illegal !!!!\n");
		nerr++;
	}

	if(nerr) exit(1);

	// End of check for header
	// 
	// Are frame PAL or NTSC?
	if ( norm < 0 && frame_rate_code==3)
	{
		norm=0;
	} 
	else if (norm < 0 && frame_rate_code==4)
	{
		norm=1;
	} 
	
	if( norm < 0 )
	{ 
		mjpeg_warn("Cannot infer TV norm from frame rate : frame size=%dx%d and rate=%.3f fps!!\n",
				   input_width,input_height,framerates[frame_rate_code]);
	}



	// INITIALISATIONS
	input_useful_width =input_width;
	input_useful_height=input_height;
	input_offset_width=0;
	input_offset_height=0;

	// Deal with args that depend on input stream
	handle_args_dependent(argc,argv);

	mjpeg_info("yuvscaler: from %ux%u, take %ux%u+%u+%u\n", 
			   input_width,input_height,
			   input_useful_width,input_useful_height,input_offset_width,input_offset_height );
	mjpeg_info("downscale to %ux%u, %ux%u being displayed\n",
			   output_active_width,output_active_height,
			   display_width,display_height);
	mjpeg_debug("yuvscaler Frame rate code: %d\n",frame_rate_code);

	// Coherence check
	if ((input_useful_width<output_active_width) || (input_useful_height<output_active_height))
	{ 
		mjpeg_error("yuvscaler can only downscale, not upscale!!!!! STOP\n");
		exit(1);
	}

	if ((input_width==input_useful_width) && (input_height==input_useful_height) &&
		(input_width==output_active_width) && (input_height==output_active_height))
	{ 
		mjpeg_error("Frame size are OK! Nothing to do!!!!! STOP\n");
		exit(0);
	}

	// Output file header
	if (fprintf(stdout,"YUV4MPEG %3d %3d %1d\n",display_width,display_height,frame_rate_code)!=19)
	{
		mjpeg_error("Error writing output header. Stop\n");
		exit(1);
	}

	divider=pgcd(input_useful_width,output_active_width);
	input_width_slice =input_useful_width /divider;
	output_width_slice=output_active_width/divider;
#ifdef DEBUG
	mjpeg_debug("divider,i_w_s,o_w_s = %d,%d,%d\n",
				divider,input_width_slice,output_width_slice);
#endif

	divider=pgcd(input_useful_height,output_active_height);
	input_height_slice =input_useful_height /divider;
	output_height_slice=output_active_height/divider;
#ifdef DEBUG
	mjpeg_debug("divider,i_w_s,o_w_s = %d,%d,%d\n",
				divider,input_height_slice,output_height_slice);
#endif
	
	diviseur=input_height_slice*input_width_slice;
#ifdef DEBUG
	mjpeg_debug("Diviseur=%ld\n",diviseur);
#endif

	// To speed up downscaling, we tabulate the integral part of the division by "diviseur" which is inside [0;255] integral => unsigned short int storage class
	// but we do not tabulate the rest of this division since it may range up to diviseur-1 => unsigned long int storage class => too big in memory
	divide=(unsigned short int *)alloca(256*diviseur*sizeof(unsigned short int));

	u_i_p=divide;
	for (i=0;i<256*diviseur;i++)
	{
		*(u_i_p++)=i/diviseur;
		//      mjpeg_error("%ld: %d\r",i,(unsigned short int)(i/diviseur));
	}

	// Calculate averaging coefficient
	// For the height
	height_coeff=alloca((input_height_slice+1)*output_height_slice*sizeof(unsigned int));
	average_coeff(input_height_slice,output_height_slice,height_coeff);

	// For the width
	width_coeff=alloca((input_width_slice+1)*output_width_slice*sizeof(unsigned int));
	average_coeff(input_width_slice,output_width_slice,width_coeff);


	// Pointers allocations
	input=alloca((input_width*input_height*3)/2);
	output=alloca((output_active_width*output_active_height*3)/2);

	if (black==1)
	{
		black_pixels=(output_height_black_above>=output_height_black_under ? output_height_black_above : output_height_black_under)*display_width; 
		black_y=alloca(black_pixels);
		black_uv=alloca(black_pixels/4);

		// BLACK pixel in YUV = (16,128,128)
		u_c_p=black_y;
		for (i=0;i<black_pixels;i++)
		{
			*u_c_p=16; u_c_p++; 
		}
		u_c_p=black_uv;
		for (i=0;i<black_pixels/4;i++)
		{
			*u_c_p=128; u_c_p++; 
		}
	}

	
	mjpeg_debug("End of Initialisation\n");
	// END OF INITIALISATION


	// input comes from stdin
	// Master loop : continue until there is no next frame in stdin
	while((fread(magic,6,1,in_file)==1)&&(strcmp(magic,key)==0))
	{
		// Output key word
		if(!fwrite_complete(key,6,out_file))
			goto out_error;
		
		// Frame by Frame read and down-scale
		frame_num++;
		if (fread(input,(input_height*input_width*3)/2,1,in_file)!=1)
		{
			mjpeg_error_exit1("Frame %ld read failed\n",frame_num);
		}
		average_Y (input,output,height_coeff,width_coeff);
		average_UV(input+input_height*input_width,
				   output+output_active_height*output_active_width,
				   height_coeff,width_coeff);
		average_UV(input+(input_height*input_width*5)/4,
				   output+(output_active_height*output_active_width)*5/4,
				   height_coeff,width_coeff);

		if (black==0)
		{
			// Here, display=output_active 
			if (!fwrite_complete(output,(output_active_width*output_active_height*3)/2,out_file))
				goto out_error;
		} 
		else 
		{
			// There are black lines to be displayed on top on each component
			// Y Component
			if (!fwrite_complete(black_y,output_height_black_above*output_active_width,out_file))
				goto out_error;
			
			if (!fwrite_complete(output,output_active_width*output_active_height,out_file))
				goto out_error;
			if (!fwrite_complete(black_y,output_height_black_under*output_active_width,out_file))
				goto out_error;

			// U Component
			u_c_p=output+output_active_width*output_active_height;
			if (!fwrite_complete(black_uv,output_height_black_above/2*output_active_width/2,out_file))
				goto out_error;
			if (!fwrite_complete(u_c_p,output_active_width/2*output_active_height/2,out_file))
				goto out_error;
			if (!fwrite_complete(black_uv,output_height_black_under/2*output_active_width/2,out_file))
				goto out_error;

			// V Component
			u_c_p+=output_active_width/2*output_active_height/2;
			if (!fwrite_complete(black_uv,output_height_black_above/2*output_active_width/2,out_file))
				goto out_error;
			if (!fwrite_complete(u_c_p,output_active_width/2*output_active_height/2,out_file))
				goto out_error;
			if (!fwrite_complete(black_uv,output_height_black_under/2*output_active_width/2,out_file))
				goto out_error;
		}
		mjpeg_debug("yuvscaler Frame number %ld\r",frame_num);
	}
	// End of master loop

	return 0;

out_error:
	mjpeg_error_exit1( "Unable to write to output - aborting!\n" );
	return 1;
}

// *************************************************************************************
int average_coeff(unsigned int input_length,unsigned int output_length,unsigned int *coeff)
{
	// This function calculates multiplicative coeeficients to average an input (vector of) length
	// input_length into an output (vector of) length output_length;
	// We sequentially store the number-of-non-zero-coefficients, followed by the coefficients
	// themselvesc, and that, output_length time
	int last_coeff=0,remaining_coeff,still_to_go=0,in,out,non_zero=0,nb;
	unsigned int *non_zero_p = NULL;
	unsigned int *pointer;

	if ((output_length>input_length) || (input_length<=0) || (output_length<=0)
		|| (coeff==0))
	{
		mjpeg_error("Function average_coeff : arguments are wrong\n");
		mjpeg_error("input length = %d, output length = %d, input = %p\n",
				input_length,output_length,coeff);
		exit(1);
	}
#ifdef DEBUG
	mjpeg_debug("Function average_coeff : input length = %d, output length = %d, input = %p\n",
				input_length,output_length,coeff);
#endif

	pointer=coeff;

	if (output_length==1)
	{
		*pointer=input_length;
		pointer++;
		for (in=0;in<input_length;in++)
		{
			*pointer=1;
			pointer++;
		}
	} 
	else
	{
		for (in=0;in<output_length;in++)
		{
			non_zero=0;
			non_zero_p=pointer;
			pointer++;
			still_to_go=input_length;
			if (last_coeff>0)
			{
				remaining_coeff=output_length-last_coeff;
				*pointer=remaining_coeff;
				pointer++;
				non_zero++;
				still_to_go-=remaining_coeff;
			}
			nb=(still_to_go/output_length);
#ifdef DEBUG
			mjpeg_debug("in=%d,nb=%d,stgo=%d ol=%d\n",in,nb,still_to_go, output_length);
#endif
			for (out=0;out<nb;out++)
			{
				*pointer=output_length;
				pointer++;
			}
			still_to_go-=nb*output_length;
			non_zero+=nb;

			if ((last_coeff=still_to_go)!=0)
			{
				*pointer=last_coeff;
#ifdef DEBUG
				 mjpeg_debug("non_zero=%d,last_coeff=%d\n",non_zero,last_coeff);
#endif
				pointer++; // now pointer points onto the next number-of-non_zero-coefficients
				non_zero++;
				*non_zero_p=non_zero;
			} else {
				if (in!=output_length-1)
				{
					mjpeg_error("There is a common divider between %d and %d\n This should not be the case\n",input_length,output_length);
					exit(1);
				}
			}

		}
		*non_zero_p=non_zero;

		if (still_to_go!=0)
		{
			mjpeg_error("Function average_coeff : calculus doesn't stop right : %d\n",still_to_go);
		}
	}
#ifdef DEBUG
	if( verbose == 2 )
	{		
		int i,j;
		for (i=0;i<output_length;i++)
		{
			mjpeg_debug("line=%d\n",i);
			non_zero=*coeff; coeff++;
			mjpeg_debug(" ");
			for (j=0;j<non_zero;j++)
			{
				fprintf(stderr,"%d : %d ",j,*coeff);
				coeff++;
			}
			fprintf(stderr,"\n");
		}
	}
#endif
	return(0);
}
// *************************************************************************************



// *************************************************************************************
int average_Y(uint8_t * input,uint8_t * output, unsigned int * height_coeff, unsigned int * width_coeff)
{
	// This function average an input matrix of name input that contains the Y component (size input_width*input_height, useful size input_useful_width*input_useful_height)
	// into an output matrix of name output of size output_active_width*output_active_height
	uint8_t *input_line_p[input_height_slice];
	uint8_t *output_line_p[output_height_slice];
	unsigned int *H_var, *W_var, *H, *W;
	uint8_t *u_c_p;
	int j,nb_H,nb_W,in_line,out_line;
	int out_col_slice, out_nb_col_slice, out_col;
	int out_line_slice, out_nb_line_slice;
	int current_line,last_line;
	unsigned long int round_off_error=0,value=0;
	unsigned short int divi;

	//Init
	mjpeg_debug("Start of average_Y\n");
	out_nb_col_slice=output_active_width/output_width_slice;
	out_nb_line_slice=output_active_height/output_height_slice;
	input+=input_offset_height*input_width+input_offset_width;
	//End of INIT


	if ((output_height_slice!=1)||(input_height_slice!=1))
	{
		for (out_line_slice=0;out_line_slice<out_nb_line_slice;out_line_slice++)
		{
			u_c_p=input+out_line_slice*input_height_slice*input_width;
			for (in_line=0;in_line<input_height_slice;in_line++)
			{
				input_line_p[in_line]=u_c_p;
				u_c_p+=input_width;
			}
			u_c_p=output+out_line_slice*output_height_slice*output_active_width;
			for (out_line=0;out_line<output_height_slice;out_line++)
			{
				output_line_p[out_line]=u_c_p;
				u_c_p+=output_active_width;
			}

			for (out_col_slice=0;out_col_slice<out_nb_col_slice;out_col_slice++)
			{
				H=height_coeff;
				in_line=0;
				for (out_line=0;out_line<output_height_slice;out_line++)
				{
					nb_H=*H;
					W=width_coeff;
					for (out_col=0;out_col<output_width_slice;out_col++)
					{
						H_var=H+1;
						nb_W=*W;
						value=round_off_error;
						last_line=in_line+nb_H;
						for (current_line=in_line;current_line<last_line;current_line++)
						{ 
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
						divi=divide[value];
						*(output_line_p[out_line]++)=divi;
						round_off_error=value-divi*diviseur;
						W+=nb_W+1;
					}
					H+=nb_H+1;
					in_line+=nb_H-1;
					input_line_p[in_line]-=input_width_slice-1; 
					// If last line of input is to be reused in next loop, 
					// make the pointer points at the correct place
				}
				if (in_line!=input_height_slice-1)
				{
					mjpeg_error_exit1("INTERNAL: Error line conversion\n");
				}
				else input_line_p[in_line]+=input_width_slice-1;
				for (in_line=0;in_line<input_height_slice;in_line++) 
					input_line_p[in_line]++;
			}
		}
	}
	else 
	{
		// We just take the average along the width, not the height, line per line
		for (out_line_slice=0;out_line_slice<output_active_height;out_line_slice++)
		{
			input_line_p[0] =input+ out_line_slice*input_width;
			output_line_p[0]=output+out_line_slice*output_active_width;
			for (out_col_slice=0;out_col_slice<out_nb_col_slice;out_col_slice++)
			{
				W=width_coeff;
				for (out_col=0;out_col<output_width_slice;out_col++)
				{
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
	mjpeg_debug("End of average_Y\n");
	return(0);
}
// *************************************************************************************

// *************************************************************************************
int average_UV(uint8_t * input,uint8_t * output, 
			   unsigned int * height_coeff, unsigned int * width_coeff)
{
	// This function average an input matrix of name input that contains
	// either the U or the V component (size input_height/2*input_width/2)
	// into an output matrix of name output of size output_active_height/2*output_active_width/2
	uint8_t *input_line_p[input_height_slice];
	uint8_t *output_line_p[output_height_slice];
	unsigned int *H_var, *W_var, *H, *W;
	uint8_t *u_c_p;
	int j,nb_H,nb_W,in_line,out_line,out_col_slice;
	int out_nb_col_slice,out_col,out_line_slice;
	int out_nb_line_slice,current_line,last_line;
	unsigned long int round_off_error=0,value;
	unsigned short int divi;

	//Init
	mjpeg_debug("Start of average_UV\n");
	out_nb_line_slice=output_active_height/2/output_height_slice;
	out_nb_col_slice=output_active_width/2/output_width_slice;
	input+=input_offset_height/2*input_width/2+input_offset_width/2;
	//End of INIT

	if ((output_height_slice!=1)||(input_height_slice!=1))
	{
		for (out_line_slice=0;out_line_slice<out_nb_line_slice;out_line_slice++)
		{
			u_c_p=input+out_line_slice*input_height_slice*input_width/2;
			for (in_line=0;in_line<input_height_slice;in_line++)
			{
				input_line_p[in_line]=u_c_p;
				u_c_p+=input_width/2;
			}
			u_c_p=output+out_line_slice*output_height_slice*output_active_width/2;
			for (out_line=0;out_line<output_height_slice;out_line++)
			{
				output_line_p[out_line]=u_c_p;
				u_c_p+=output_active_width/2;
			}

			for (out_col_slice=0;out_col_slice<out_nb_col_slice;out_col_slice++)
			{
				H=height_coeff;
				in_line=0;
				for (out_line=0;out_line<output_height_slice;out_line++)
				{
					nb_H=*H;
					W=width_coeff;
					for (out_col=0;out_col<output_width_slice;out_col++)
					{
						H_var=H+1;
						nb_W=*W;
						value=round_off_error;
						last_line=in_line+nb_H;
						for (current_line=in_line;current_line<last_line;current_line++)
						{ 
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
				if (in_line!=input_height_slice-1)
				{
					mjpeg_error_exit1("Error line conversion\n");
				}
				else 
					input_line_p[in_line]+=input_width_slice-1;
				for (in_line=0;in_line<input_height_slice;in_line++) 
					input_line_p[in_line]++;
			}
		}
	} else {
		// We just take the average along the width, not the height, line per line
		for (out_line_slice=0;out_line_slice<output_active_height/2;out_line_slice++)
		{
			input_line_p[0] =input+out_line_slice*input_width/2;
			output_line_p[0]=output+out_line_slice*output_active_width/2;
			for (out_col_slice=0;out_col_slice<out_nb_col_slice;out_col_slice++)
			{
				W=width_coeff;
				for (out_col=0;out_col<output_width_slice;out_col++)
				{
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

	mjpeg_debug("End of average_UV\n");
	return(0);
}

// *************************************************************************************

// *************************************************************************************
unsigned int pgcd(unsigned int num1,unsigned int num2)
{
	// Calculates the biggest common divider between num1 and num2, with num2<=num1
	unsigned int i;
	for (i=num2;i>=2;i--)
	{
		if ( ((num2%i)==0) && ((num1%i)==0) ) 
			return(i);
	}
	return(1);
}
// *************************************************************************************




