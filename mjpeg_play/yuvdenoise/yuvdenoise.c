/*
 * Yuv-Denoiser to be used with Andrew Stevens mpeg2enc
 *
 * (C)2001 S.Fendt 
 *
 * Licensed and protected by the GNU-General-Public-License version 2 
 * (or if you prefer any later version of that license). See the file 
 * LICENSE for detailed information.
 *
 */

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>

//#define DEBUG 
 
#define MAGIC "YUV4MPEG"

/* we need several buffers. defineing them here is not
   very nice coding as it wastes memory but it's fast
   and doesn't produce any leaks (and modern systems
   should really have enough memory anyways :-} ...)

   HINT: Thist of course will go away if the filter is
   more stable ... bet for it ! But at present there are
   other problems to solve than an elegant memory 
   management...
*/
 
uint8_t YUV1[(int)(768*576*1.5)];
uint8_t YUV2[(int)(768*576*1.5)];
uint8_t SUBO[(int)(768*576*1.5)];
uint8_t SUBA[(int)(768*576*1.5)];
uint8_t SUBO4[(int)(768*576*1.5)];
uint8_t SUBA4[(int)(768*576*1.5)];
uint8_t DEL0[(int)(768*576*1.5)];
uint8_t DEL1[(int)(768*576*1.5)];
uint8_t DEL2[(int)(768*576*1.5)];
uint8_t DEL3[(int)(768*576*1.5)];
uint8_t DEL4[(int)(768*576*1.5)];
uint8_t DEL5[(int)(768*576*1.5)];
uint8_t DEL6[(int)(768*576*1.5)];
uint8_t DEL7[(int)(768*576*1.5)];
uint8_t DEL8[(int)(768*576*1.5)];
uint8_t AVRG[(int)(768*576*1.5)];
uint32_t framenr=0;

int width;             /* width of the images */
int height;            /* height of the images */
int u_offset;          /* offset of the U-component from beginning of the image */
int v_offset;          /* offset of the V-component from beginning of the image */
int uv_width;          /* width of the UV-components */
int uv_height;         /* height of the UV-components */
int best_match_x;      /* best block-match's X-coordinate in half-pixels */
int best_match_y;      /* best block-match's Y-coordinate in half-pixels */
uint32_t SQD;          /* best block-match's sum of absolute differences */
uint32_t YSQD;          /* best block-match's sum of absolute differences */
uint32_t USQD;          /* best block-match's sum of absolute differences */
uint32_t VSQD;          /* best block-match's sum of absolute differences */
uint32_t CQD;          /* center block-match's sum of absolute differences */
uint32_t init_SQD=0;
int lum_delta;
int cru_delta;
int crv_delta;
int search_radius=32;  /* search radius in half-pixels */
int border=-1;

uint8_t version[]="0.0.15";

void denoise_image();
void detect_black_borders();
void mb_search_88(int x, int y);
void mb_search(int x, int y);
void subsample_reference_image2();
void subsample_averaged_image2();
void subsample_reference_image4();
void subsample_averaged_image4();
void delay_images();
void time_average_images();
void display_greeting();
void display_help();

int main(int argc, char *argv[])
{
    int dot_x=0,dot_y=0;

    uint8_t magic[256];
    uint8_t norm;
    char c;

    int x,y;
    display_greeting();
    
    while((c = getopt(argc,argv,"b:m:h")) != -1)
    {
        switch (c)
	{
	    case 'b':
		border=atoi(optarg);
		fprintf(stderr,"---  Border-Automation deactivated. Border set to %i.\n",border);
		break;
	    case 'm':
		init_SQD=atoi(optarg);
		fprintf(stderr,"---  Initial mean-SQD set to %i.\n",init_SQD);
		break;
	    case 'h':
		display_help();
		break;
	}
    }

    /* read the YUV4MPEG header */

    if(0==(fscanf(stdin,"%s %i %i %i",&magic,&width,&height,&norm) ))
    {
	fprintf(stderr,"### yuvdenoise ### : Error, no header found\n");
	exit(-1);
    }
    else
    {
	if(0!=strcmp(magic,MAGIC))
	{
	    fprintf(stderr,"### yuv3Dfilter ### : Error, magic number is not 'YUV4MPEG'.\n");
	    exit(-1);
	}	    
    }
    getc(stdin); /* read newline character */

    /* write new header */

    fprintf(stdout,"YUV4MPEG %3i %3i %1i \n",width,height,norm);

    /* setting global variables */
    u_offset=width*height;
    v_offset=width*height*1.25;
    uv_width=width>>1;
    uv_height=height>>1;

    /* read frames until they're all done... */

    while(fread(magic,6,1,stdin) && strcmp(magic,"FRAME"))
    {
	fprintf(stderr,"---  [FRAME]  --------------------------------\n");

	/* read one frame */
	if(0==fread(YUV1,(width*height*1.5),1,stdin))
	    {
		fprintf(stderr,"### yuv3Dfilter ### : Error, unexpected end of input!\n");
		exit(-1);
	    }

	/* Do whatever ... */
	denoise_image();
	detect_black_borders();
	framenr++;

	/* write one frame */
	fprintf(stdout,"FRAME\n");
	fwrite(YUV2,(width*height*1.5),1,stdout);
    }
    return(0);
}

void display_greeting()
{
    fprintf(stderr,"\n\n\n-----------------------------------------------------------------\n");
    fprintf(stderr,"---      YUV4MPEG  Motion-Compensating-Movie-Denoiser         ---\n");
    fprintf(stderr,"---      Version %6s     (this is a developer's version)       ---\n",version);
    fprintf(stderr,"-----------------------------------------------------------------\n");
    fprintf(stderr,"\n");
    fprintf(stderr," 2001 by Stefan Fendt <stefan@lionfish.ping.de>\n");
    fprintf(stderr," Use this at your own risk. Your milage may vary. You have been\n");
    fprintf(stderr," warned. If it totally corrupts your movies... That's life.\n");
    fprintf(stderr,"\n");
}

void display_help()
{
    fprintf(stderr,"\n\nOptions:\n");
    fprintf(stderr,"--------\n");
    fprintf(stderr,"\n");
    fprintf(stderr,"-h         This message. :)\n\n");
    fprintf(stderr,"-b [num]   Set frame-border to [num] lines.\n");
    fprintf(stderr,"           These lines from top (respectively from bottom) of the\n");
    fprintf(stderr,"           processed frames turn into pure black to be encoded better.\n\n");
    fprintf(stderr,"-m [num]   Set initial mean-SQD \n");
    fprintf(stderr,"           This is most useful if you have to start directly in a new\n");
    fprintf(stderr,"           scene with quite heavy motion.\n\n");
    exit(0);
}

void time_average_images()
{
    int x,y;

    for(y=0;y<height;y++)
	for(x=0;x<width;x++)
	{
	    AVRG[x+y*width]=
		(uint16_t)(
		    DEL1[x+y*width]+
		    DEL2[x+y*width]+
		    DEL3[x+y*width]+
		    DEL4[x+y*width]+
		    DEL5[x+y*width]+
		    DEL6[x+y*width]+
		    DEL7[x+y*width]+
		    DEL8[x+y*width]
		    )>>3;
	}
    for(y=0;y<uv_height;y++)
	for(x=0;x<uv_width;x++)
	{
	    AVRG[x+y*uv_width+u_offset]=
		(uint16_t)(
		    DEL1[x+y*uv_width+u_offset]+
		    DEL2[x+y*uv_width+u_offset]+
		    DEL3[x+y*uv_width+u_offset]+
		    DEL4[x+y*uv_width+u_offset]+
		    DEL5[x+y*uv_width+u_offset]+
		    DEL6[x+y*uv_width+u_offset]+
		    DEL7[x+y*uv_width+u_offset]+
		    DEL8[x+y*uv_width+u_offset]
		    )>>3;
	    AVRG[x+y*uv_width+v_offset]=
		(uint16_t)(
		    DEL1[x+y*uv_width+v_offset]+
		    DEL2[x+y*uv_width+v_offset]+
		    DEL3[x+y*uv_width+v_offset]+
		    DEL4[x+y*uv_width+v_offset]+
		    DEL5[x+y*uv_width+v_offset]+
		    DEL6[x+y*uv_width+v_offset]+
		    DEL7[x+y*uv_width+v_offset]+
		    DEL8[x+y*uv_width+v_offset]
		    )>>3;
	}
}

void delay_images()
{
    static int init=0;
    int x,y;

    if(init==0)
    {
	for(y=0;y<height;y++)
	    for(x=0;x<width;x++)
	    {
		/* Y */
		DEL0[x+y*width]=
		DEL1[x+y*width]=
		DEL2[x+y*width]=
		DEL3[x+y*width]=
		DEL4[x+y*width]=
		DEL5[x+y*width]=
		DEL6[x+y*width]=
		DEL7[x+y*width]=
		DEL8[x+y*width]=
		    YUV1[x+y*width];    
	    }
	for(y=0;y<uv_height;y++)
	    for(x=0;x<uv_width;x++)
	    {
		/* U + V */
		DEL0[x+y*uv_width+u_offset]=
		DEL1[x+y*uv_width+u_offset]=
		DEL2[x+y*uv_width+u_offset]=
		DEL3[x+y*uv_width+u_offset]=
		DEL4[x+y*uv_width+u_offset]=
		DEL5[x+y*uv_width+u_offset]=
		DEL6[x+y*uv_width+u_offset]=
		DEL7[x+y*uv_width+u_offset]=
		DEL8[x+y*uv_width+u_offset]=
		    YUV1[x+y*uv_width+u_offset];    
		DEL0[x+y*uv_width+v_offset]=
		DEL1[x+y*uv_width+v_offset]=
		DEL2[x+y*uv_width+v_offset]=
		DEL3[x+y*uv_width+v_offset]=
		DEL4[x+y*uv_width+v_offset]=
		DEL5[x+y*uv_width+v_offset]=
		DEL6[x+y*uv_width+v_offset]=
		DEL7[x+y*uv_width+v_offset]=
		DEL8[x+y*uv_width+v_offset]=
		    YUV1[x+y*uv_width+v_offset];    
	    }
	init=1;
    }
    else
    {
	for(y=0;y<height;y++)
	    for(x=0;x<width;x++)
	    {
		DEL8[x+y*width]=DEL7[x+y*width];
		DEL7[x+y*width]=DEL6[x+y*width];
		DEL6[x+y*width]=DEL5[x+y*width];
		DEL5[x+y*width]=DEL4[x+y*width];
		DEL4[x+y*width]=DEL3[x+y*width];
		DEL3[x+y*width]=DEL2[x+y*width];
		DEL2[x+y*width]=DEL1[x+y*width];
		DEL1[x+y*width]=DEL0[x+y*width];
		DEL0[x+y*width]=YUV1[x+y*width];
	    }	
	for(y=0;y<uv_height;y++)
	    for(x=0;x<uv_width;x++)
	    {
		DEL8[x+y*uv_width+u_offset]=DEL7[x+y*uv_width+u_offset];
		DEL7[x+y*uv_width+u_offset]=DEL6[x+y*uv_width+u_offset];
		DEL6[x+y*uv_width+u_offset]=DEL5[x+y*uv_width+u_offset];
		DEL5[x+y*uv_width+u_offset]=DEL4[x+y*uv_width+u_offset];
		DEL4[x+y*uv_width+u_offset]=DEL3[x+y*uv_width+u_offset];
		DEL3[x+y*uv_width+u_offset]=DEL2[x+y*uv_width+u_offset];
		DEL2[x+y*uv_width+u_offset]=DEL1[x+y*uv_width+u_offset];
		DEL1[x+y*uv_width+u_offset]=DEL0[x+y*uv_width+u_offset];
		DEL0[x+y*uv_width+u_offset]=YUV1[x+y*uv_width+u_offset];

		DEL8[x+y*uv_width+v_offset]=DEL7[x+y*uv_width+v_offset];
		DEL7[x+y*uv_width+v_offset]=DEL6[x+y*uv_width+v_offset];
		DEL6[x+y*uv_width+v_offset]=DEL5[x+y*uv_width+v_offset];
		DEL5[x+y*uv_width+v_offset]=DEL4[x+y*uv_width+v_offset];
		DEL4[x+y*uv_width+v_offset]=DEL3[x+y*uv_width+v_offset];
		DEL3[x+y*uv_width+v_offset]=DEL2[x+y*uv_width+v_offset];
		DEL2[x+y*uv_width+v_offset]=DEL1[x+y*uv_width+v_offset];
		DEL1[x+y*uv_width+v_offset]=DEL0[x+y*uv_width+v_offset];
		DEL0[x+y*uv_width+v_offset]=YUV1[x+y*uv_width+v_offset];
	    }
    }
}

void subsample_reference_image2()
{
    int x,y;

    /* subsample Y */
    for(y=0;y<height;y+=2)
	for(x=0;x<width;x+=2)
	{
	    SUBO[(x  ) + (y  )*width]=
	    SUBO[(x+1) + (y  )*width]=
	    SUBO[(x  ) + (y+1)*width]=
	    SUBO[(x+1) + (y+1)*width]=
		(YUV1[(x  ) + (y  )*width]+
		 YUV1[(x+1) + (y  )*width]+
		 YUV1[(x  ) + (y+1)*width]+
		 YUV1[(x+1) + (y+1)*width])>>2;
 	}

    /* subsample U and V */
/*
    for(y=0;y<uv_height;y+=2)
	for(x=0;x<uv_width;x+=2)
	{
	    * U *
	    SUBO[(x  ) + (y  )*uv_width+u_offset]=
	    SUBO[(x+1) + (y  )*uv_width+u_offset]=
	    SUBO[(x  ) + (y+1)*uv_width+u_offset]=
	    SUBO[(x+1) + (y+1)*uv_width+u_offset]=
		(YUV1[(x  ) + (y  )*uv_width+u_offset]+
		 YUV1[(x+1) + (y  )*uv_width+u_offset]+
		 YUV1[(x  ) + (y+1)*uv_width+u_offset]+
		 YUV1[(x+1) + (y+1)*uv_width+u_offset])>>2;
	    * V *
	    SUBO[(x  ) + (y  )*uv_width+v_offset]=
	    SUBO[(x+1) + (y  )*uv_width+v_offset]=
	    SUBO[(x  ) + (y+1)*uv_width+v_offset]=
	    SUBO[(x+1) + (y+1)*uv_width+v_offset]=
		(YUV1[(x  ) + (y  )*uv_width+v_offset]+
		 YUV1[(x+1) + (y  )*uv_width+v_offset]+
		 YUV1[(x  ) + (y+1)*uv_width+v_offset]+
		 YUV1[(x+1) + (y+1)*uv_width+v_offset])>>2;
	}
*/
    for(y=0;y<uv_height;y++)
	for(x=0;x<uv_width;x++)
	{
	    /* U */
	    SUBO[(x  ) + (y  )*uv_width+u_offset]=
		YUV1[(x  ) + (y  )*uv_width+u_offset];

	    /* V */
	    SUBO[(x  ) + (y  )*uv_width+v_offset]=
		YUV1[(x  ) + (y  )*uv_width+v_offset];
	}
}

void subsample_averaged_image2()
{
    int x,y;

    /* subsample Y */
    for(y=0;y<height;y+=2)
	for(x=0;x<width;x+=2)
	{
	    SUBA[(x  ) + (y  )*width]=
	    SUBA[(x+1) + (y  )*width]=
	    SUBA[(x  ) + (y+1)*width]=
	    SUBA[(x+1) + (y+1)*width]=
		(AVRG[(x  ) + (y  )*width]+
		 AVRG[(x+1) + (y  )*width]+
		 AVRG[(x  ) + (y+1)*width]+
		 AVRG[(x+1) + (y+1)*width])>>2;
 	}
/*
    * subsample U and V *
    for(y=0;y<uv_height;y+=2)
	for(x=0;x<uv_width;x+=2)
	{
	    * U *
	    SUBA[(x  ) + (y  )*uv_width+u_offset]=
	    SUBA[(x+1) + (y  )*uv_width+u_offset]=
	    SUBA[(x  ) + (y+1)*uv_width+u_offset]=
	    SUBA[(x+1) + (y+1)*uv_width+u_offset]=
		(AVRG[(x  ) + (y  )*uv_width+u_offset]+
		 AVRG[(x+1) + (y  )*uv_width+u_offset]+
		 AVRG[(x  ) + (y+1)*uv_width+u_offset]+
		 AVRG[(x+1) + (y+1)*uv_width+u_offset])>>2;
	    * V *
	    SUBA[(x  ) + (y  )*uv_width+v_offset]=
	    SUBA[(x+1) + (y  )*uv_width+v_offset]=
	    SUBA[(x  ) + (y+1)*uv_width+v_offset]=
	    SUBA[(x+1) + (y+1)*uv_width+v_offset]=
		(AVRG[(x  ) + (y  )*uv_width+v_offset]+
		 AVRG[(x+1) + (y  )*uv_width+v_offset]+
		 AVRG[(x  ) + (y+1)*uv_width+v_offset]+
		 AVRG[(x+1) + (y+1)*uv_width+v_offset])>>2;
	}
*/

    for(y=0;y<uv_height;y++)
	for(x=0;x<uv_width;x++)
	{
	    /* U */
	    SUBA[(x  ) + (y  )*uv_width+u_offset]=
		AVRG[(x  ) + (y  )*uv_width+u_offset];
	    /* V */
	    SUBA[(x  ) + (y  )*uv_width+v_offset]=
		AVRG[(x  ) + (y  )*uv_width+v_offset];
	}
}
void subsample_reference_image4()
{
    int x,y;

    /* subsample Y */
    for(y=0;y<height;y+=4)
	for(x=0;x<width;x+=4)
	{
	    SUBO4[(x  ) + (y  )*width]=
	    SUBO4[(x+1) + (y  )*width]=
	    SUBO4[(x+2) + (y  )*width]=
	    SUBO4[(x+3) + (y  )*width]=
	    SUBO4[(x  ) + (y+1)*width]=
	    SUBO4[(x+1) + (y+1)*width]=
	    SUBO4[(x+2) + (y+1)*width]=
	    SUBO4[(x+3) + (y+1)*width]=
	    SUBO4[(x  ) + (y+2)*width]=
	    SUBO4[(x+1) + (y+2)*width]=
	    SUBO4[(x+2) + (y+2)*width]=
	    SUBO4[(x+3) + (y+2)*width]=
	    SUBO4[(x  ) + (y+3)*width]=
	    SUBO4[(x+1) + (y+3)*width]=
	    SUBO4[(x+2) + (y+3)*width]=
	    SUBO4[(x+3) + (y+3)*width]=
		(YUV1[(x  ) + (y  )*width]+
		 YUV1[(x+1) + (y  )*width]+
		 YUV1[(x+2) + (y  )*width]+
		 YUV1[(x+3) + (y  )*width]+
		 YUV1[(x  ) + (y+1)*width]+
		 YUV1[(x+1) + (y+1)*width]+
		 YUV1[(x+2) + (y+1)*width]+
		 YUV1[(x+3) + (y+1)*width]+
		 YUV1[(x  ) + (y+2)*width]+
		 YUV1[(x+1) + (y+2)*width]+
		 YUV1[(x+2) + (y+2)*width]+
		 YUV1[(x+3) + (y+2)*width]+
		 YUV1[(x  ) + (y+3)*width]+
		 YUV1[(x+1) + (y+3)*width]+
		 YUV1[(x+2) + (y+3)*width]+
		 YUV1[(x+3) + (y+3)*width])>>4;
 	}

    /* subsample U and V */
    for(y=0;y<uv_height;y+=2)
	for(x=0;x<uv_width;x+=2)
	{
	    /* U */
	    SUBO4[(x  ) + (y  )*uv_width+u_offset]=
	    SUBO4[(x+1) + (y  )*uv_width+u_offset]=
	    SUBO4[(x  ) + (y+1)*uv_width+u_offset]=
	    SUBO4[(x+1) + (y+1)*uv_width+u_offset]=
		(YUV1[(x  ) + (y  )*uv_width+u_offset]+
		 YUV1[(x+1) + (y  )*uv_width+u_offset]+
		 YUV1[(x  ) + (y+1)*uv_width+u_offset]+
		 YUV1[(x+1) + (y+1)*uv_width+u_offset])>>2;

	    /* V */
	    SUBO4[(x  ) + (y  )*uv_width+v_offset]=
	    SUBO4[(x+1) + (y  )*uv_width+v_offset]=
	    SUBO4[(x  ) + (y+1)*uv_width+v_offset]=
	    SUBO4[(x+1) + (y+1)*uv_width+v_offset]=
		(YUV1[(x  ) + (y  )*uv_width+v_offset]+
		 YUV1[(x+1) + (y  )*uv_width+v_offset]+
		 YUV1[(x  ) + (y+1)*uv_width+v_offset]+
		 YUV1[(x+1) + (y+1)*uv_width+v_offset])>>2;
	}
}

void subsample_averaged_image4()
{
    int x,y;

    /* subsample Y */
    for(y=0;y<height;y+=4)
	for(x=0;x<width;x+=4)
	{
	    SUBA4[(x  ) + (y  )*width]=
	    SUBA4[(x+1) + (y  )*width]=
	    SUBA4[(x+2) + (y  )*width]=
	    SUBA4[(x+3) + (y  )*width]=
	    SUBA4[(x  ) + (y+1)*width]=
	    SUBA4[(x+1) + (y+1)*width]=
	    SUBA4[(x+2) + (y+1)*width]=
	    SUBA4[(x+3) + (y+1)*width]=
	    SUBA4[(x  ) + (y+2)*width]=
	    SUBA4[(x+1) + (y+2)*width]=
	    SUBA4[(x+2) + (y+2)*width]=
	    SUBA4[(x+3) + (y+2)*width]=
	    SUBA4[(x  ) + (y+3)*width]=
	    SUBA4[(x+1) + (y+3)*width]=
	    SUBA4[(x+2) + (y+3)*width]=
	    SUBA4[(x+3) + (y+3)*width]=
		(AVRG[(x  ) + (y  )*width]+
		 AVRG[(x+1) + (y  )*width]+
		 AVRG[(x+2) + (y  )*width]+
		 AVRG[(x+3) + (y  )*width]+
		 AVRG[(x  ) + (y+1)*width]+
		 AVRG[(x+1) + (y+1)*width]+
		 AVRG[(x+2) + (y+1)*width]+
		 AVRG[(x+3) + (y+1)*width]+
		 AVRG[(x  ) + (y+2)*width]+
		 AVRG[(x+1) + (y+2)*width]+
		 AVRG[(x+2) + (y+2)*width]+
		 AVRG[(x+3) + (y+2)*width]+
		 AVRG[(x  ) + (y+3)*width]+
		 AVRG[(x+1) + (y+3)*width]+
		 AVRG[(x+2) + (y+3)*width]+
		 AVRG[(x+3) + (y+3)*width])>>4;
 	}

    /* subsample U and V */
    for(y=0;y<uv_height;y+=2)
	for(x=0;x<uv_width;x+=2)
	{
	    /* U */
	    SUBA4[(x  ) + (y  )*uv_width+u_offset]=
	    SUBA4[(x+1) + (y  )*uv_width+u_offset]=
	    SUBA4[(x  ) + (y+1)*uv_width+u_offset]=
	    SUBA4[(x+1) + (y+1)*uv_width+u_offset]=
		(AVRG[(x  ) + (y  )*uv_width+u_offset]+
		 AVRG[(x+1) + (y  )*uv_width+u_offset]+
		 AVRG[(x  ) + (y+1)*uv_width+u_offset]+
		 AVRG[(x+1) + (y+1)*uv_width+u_offset])/4;

	    /* V */
	    SUBA4[(x  ) + (y  )*uv_width+v_offset]=
	    SUBA4[(x+1) + (y  )*uv_width+v_offset]=
	    SUBA4[(x  ) + (y+1)*uv_width+v_offset]=
	    SUBA4[(x+1) + (y+1)*uv_width+v_offset]=
		(AVRG[(x  ) + (y  )*uv_width+v_offset]+
		 AVRG[(x+1) + (y  )*uv_width+v_offset]+
		 AVRG[(x  ) + (y+1)*uv_width+v_offset]+
		 AVRG[(x+1) + (y+1)*uv_width+v_offset])/4;

	}
}

void mb_search_44(int x, int y)
{
    int dy,dx,qy,qx,x1,x2,y1,y2;
    uint32_t d;
    int Y,U,V;

    SQD=10000000;
    for( qy =  -(search_radius>>1);
	 qy <= +(search_radius>>1);
	 qy += 4)
	for( qx =  -(search_radius>>1);
	     qx <= +(search_radius>>1);
	     qx += 4)
	{
	    d=0;
	    for(dy=0;dy<16;dy+=4)
		for(dx=0;dx<16;dx+=4)
		{
		    Y=
			SUBA4[(x+dx+qx) + (y+dy+qy)*width]-
			SUBO4[(x+dx   ) + (y+dy   )*width];

		    U=  
			SUBA4[(x+dx+qx)/2 + (y+dy+qy)/2*uv_width + u_offset]-
			SUBO4[(x+dx   )/2 + (y+dy   )/2*uv_width + u_offset];

		    V=
			SUBA4[(x+dx+qx)/2 + (y+dy+qy)/2*uv_width + v_offset]-
			SUBO4[(x+dx   )/2 + (y+dy   )/2*uv_width + v_offset];

		    d += (Y>0)? Y : -Y; /* SAD is faster and SQD not needed here */
		    d += (U>0)? U : -U;
		    d += (V>0)? V : -V;

		}
	    if(qx==0 && qy==0) CQD=d; /* store center's SAD -- we need it... */

	    if(d<SQD)
	    {
		best_match_x=qx*2;
		best_match_y=qy*2;
		SQD=d;
	    }
	}

    if(CQD==SQD) /* compensate for center */
	best_match_x=best_match_y=0;
    
}

void mb_search_88(int x, int y)
{
    int dy,dx,qy,qx,x1,x2,y1,y2;
    uint32_t d;
    int Y,U,V;
    int bx=best_match_x/2;
    int by=best_match_y/2;

    SQD=10000000;
    for( qy =  (by-4);
	 qy <= (by+4);
	 qy += 2)
	for( qx =  (bx-4);
	     qx <= (bx+4);
	     qx += 2)
	{
	    d=0;
	    for(dy=0;dy<16;dy+=2)
		for(dx=0;dx<16;dx+=2)
		{
		    Y=
			SUBA[(x+dx+qx) + (y+dy+qy)*width]-
			SUBO[(x+dx   ) + (y+dy   )*width];

		    U=  
			SUBA[(x+dx+qx)/2 + (y+dy+qy)/2*uv_width + u_offset]-
			SUBO[(x+dx   )/2 + (y+dy   )/2*uv_width + u_offset];

		    V=
			SUBA[(x+dx+qx)/2 + (y+dy+qy)/2*uv_width + v_offset]-
			SUBO[(x+dx   )/2 + (y+dy   )/2*uv_width + v_offset];

		    d += (Y>0)? Y : -Y; /* SAD is faster and SQD not needed here */
		    d += (U>0)? U : -U;
		    d += (V>0)? V : -V;
		}

	    if(qx==bx && qy==by) CQD=d; /* store center's SAD -- we need it... */
	    
	    if(d<SQD)
	    {
		best_match_x=qx*2;
		best_match_y=qy*2;
		SQD=d;
	    }
	}

    if(CQD==SQD) /* compensate for center */
	best_match_x=bx*2, best_match_y=by*2;

}

void mb_search(int x, int y)
{
    int dy,dx,qy,qx,x1,x2,y1,y2;
    uint32_t d,du,dv;
    int Y,U,V;
    int L_min=255,L_max=0;
    int bx=best_match_x/2;
    int by=best_match_y/2;

    SQD=10000000;
    for( qy =  (by-1);
	 qy <= (by+1);
	 qy++ )
	for( qx =  (bx-1);
	     qx <= (bx+1);
	     qx++ )
	{
	    d=0;
	    dv=0;
	    du=0;
	    for(dy=0;dy<16;dy++)
		for(dx=0;dx<16;dx++)
		{
		    Y=
			AVRG[(x+dx+qx) + (y+dy+qy)*width]-
			YUV1[(x+dx   ) + (y+dy   )*width];

		    U=  
			AVRG[(x+dx+qx)/2 + (y+dy+qy)/2*uv_width + u_offset]-
			YUV1[(x+dx   )/2 + (y+dy   )/2*uv_width + u_offset];

		    V=
			AVRG[(x+dx+qx)/2 + (y+dy+qy)/2*uv_width + v_offset]-
			YUV1[(x+dx   )/2 + (y+dy   )/2*uv_width + v_offset];

		    Y  *= 3;
		    d  += Y*Y;
		    d  += U*U;
		    d  += V*V;
		    du += U*U;
		    dv += V*V;
		}

	    if(qx==bx && qy==by) CQD=d; /* store center's SQD -- we need it... */
	    
	    if(d<SQD)
	    {
		best_match_x=qx*2;
		best_match_y=qy*2;
		SQD=d;
		USQD=du;
		VSQD=dv;
	    }
	}

    if(CQD==SQD) /* compensate for center */
    {
	best_match_x=bx*2;
	best_match_y=by*2;
	SQD=CQD;
    }
}

void mb_search_half(int x, int y)
{
    int dy,dx,qy,qx,xx,yy,x0,x1,y0,y1;
    uint32_t d;
    int Y,U,V;
    int lum_min=255,lum_max=0;
    int bx=best_match_x;
    int by=best_match_y;
    float sx,sy;
    float a0,a1,a2,a3;
    uint32_t old_SQD;

    old_SQD=SQD;
    SQD=10000000;
    for( qy =  (by-1);
	 qy <= (by+1);
	 qy++ )
	for( qx =  (bx-1);
	     qx <= (bx+1);
	     qx++ )
	{
	    sx=(qx-(qx&~1))*0.5;
	    sy=(qy-(qy&~1))*0.5;

	    a0=(1-sx)*(1-sy);
	    a1=(  sx)*(1-sy);
	    a2=(1-sx)*(  sy);
	    a3=(  sx)*(  sy);

	    xx=x+qx/2; /* Keeps some calc. out of the MB-search-loop... */
	    yy=y+qy/2;

	    d=0;
	    for(dy=0;dy<16;dy++)
		for(dx=0;dx<16;dx++)
		{
		    x0=xx+dx;
		    x1=x0+1;
		    y0=yy+dy;
		    y1=y0+1;
		    y0*=width;
		    y1*=width;

		    /* we only need Y for half-pels... */
		    Y=  (
			AVRG[(x0) +(y0)]*a0+
			AVRG[(x1) +(y0)]*a1+
			AVRG[(x0) +(y1)]*a2+
			AVRG[(x1) +(y1)]*a3
			)-
			YUV1[(x+dx   )  +(y+dy   )*width];
		    
		    Y  *= 3;
  		    d  += Y*Y;
		}
	    if(qx==bx && qy==by) CQD=d; /* store center's SQD -- we need it... */
	    
	    if(d<SQD)
	    {
		best_match_x=qx;
		best_match_y=qy;
		SQD=d;
	    }
	}

    if(CQD==SQD) /* compensate for center */
    {
	/* No half-pel matches... OK, go back to the old values... */
	best_match_x=bx , best_match_y=by;
    }

    YSQD=SQD;
    SQD=old_SQD-CQD+SQD; /* This ugly trick is a major speed-up ;-) */ 

    for(dy=0;dy<16;dy++)
	for(dx=0;dx<16;dx++)
	{
	    Y = YUV1[x+dx+(y+dy)*width];
	    
	    if(Y>lum_max) lum_max=Y;
	    if(Y<lum_min) lum_min=Y;
	}
    lum_delta=lum_max-lum_min;
}

void copy_filtered_block(int x, int y)
{
    int dx,dy;
    int qx=best_match_x;
    int qy=best_match_y;
    int xx,yy,x2,y2;
    float sx,sy;
    float a0,a1,a2,a3;

    sx=(qx-(qx&~1))*0.5;
    sy=(qy-(qy&~1))*0.5;
    qx/=2;
    qy/=2;

    a0=(1-sx)*(1-sy);
    a1=(  sx)*(1-sy);
    a2=(1-sx)*(  sy);
    a3=(  sx)*(  sy);

    for(dy=0;dy<16;dy++)
	for(dx=0;dx<16;dx++)
	{
	    xx=x+dx;
	    yy=y+dy;
	    x2=xx>>1;
	    y2=yy/2*uv_width;

	    /* Y */
	    if(sx!=0 || sy!=0) 
	    YUV2[(xx) + (yy)*width]=
	    DEL1[(xx) + (yy)*width]=
	    DEL2[(xx) + (yy)*width]=
	    DEL3[(xx) + (yy)*width]=
	    DEL4[(xx) + (yy)*width]=
	    DEL5[(xx) + (yy)*width]=
	    DEL6[(xx) + (yy)*width]=
	    DEL7[(xx) + (yy)*width]=
	    DEL8[(xx) + (yy)*width]=
		(
		AVRG[(xx+qx  ) + (yy+qy  )*width]*a0+
		AVRG[(xx+qx+1) + (yy+qy  )*width]*a1+
		AVRG[(xx+qx  ) + (yy+qy+1)*width]*a2+
		AVRG[(xx+qx+1) + (yy+qy+1)*width]*a3
		)*.8+
		DEL0[(xx) + (yy)*width]*.2;

	    else
	    YUV2[(xx) + (yy)*width]=
	    DEL1[(xx) + (yy)*width]=
	    DEL2[(xx) + (yy)*width]=
	    DEL3[(xx) + (yy)*width]=
	    DEL4[(xx) + (yy)*width]=
	    DEL5[(xx) + (yy)*width]=
	    DEL6[(xx) + (yy)*width]=
	    DEL7[(xx) + (yy)*width]=
	    DEL8[(xx) + (yy)*width]=
		AVRG[(xx+qx  ) + (yy+qy  )*width];

	    /* U */
	    YUV2[(x+dx)/2 + (y+dy)/2*uv_width +u_offset]=
	    DEL1[(x2) + (y2) +u_offset]=
	    DEL2[(x2) + (y2) +u_offset]=
	    DEL3[(x2) + (y2) +u_offset]=
	    DEL4[(x2) + (y2) +u_offset]=
	    DEL5[(x2) + (y2) +u_offset]=
	    DEL6[(x2) + (y2) +u_offset]=
	    DEL7[(x2) + (y2) +u_offset]=
	    DEL8[(x2) + (y2) +u_offset]=
		AVRG[(x+dx+qx)/2 + (y+dy+qy)/2*uv_width +u_offset];

	    /* V */
	    YUV2[(x2) + (y2) +v_offset]=
	    DEL1[(x2) + (y2) +v_offset]=
	    DEL2[(x2) + (y2) +v_offset]=
	    DEL3[(x2) + (y2) +v_offset]=
	    DEL4[(x2) + (y2) +v_offset]=
	    DEL5[(x2) + (y2) +v_offset]=
	    DEL6[(x2) + (y2) +v_offset]=
	    DEL7[(x2) + (y2) +v_offset]=
	    DEL8[(x2) + (y2) +v_offset]=
		AVRG[(x+dx+qx)/2 + (y+dy+qy)/2 *uv_width+v_offset];
	}
}

void copy_reference_block(int x, int y)
{
    int dx,dy;
    int xx,yy,x2,y2;

    for(dy=0;dy<16;dy++)
	for(dx=0;dx<16;dx++)
	{
	    xx=x+dx;
	    yy=(y+dy)*width;

	    /* Y */
	    YUV2[(xx)+(yy)]=
	    DEL1[(xx)+(yy)]=
	    DEL2[(xx)+(yy)]=
	    DEL3[(xx)+(yy)]=
	    DEL4[(xx)+(yy)]=
	    DEL5[(xx)+(yy)]=
	    DEL6[(xx)+(yy)]=
	    DEL7[(xx)+(yy)]=
	    DEL8[(xx)+(yy)]=
	    DEL0[(xx)+(yy)];

	    /* U */
	    xx = (x+dx)/2+u_offset;
	    yy = (y+dy)/2*uv_width;

	    YUV2[(xx)+(yy)]=
	    DEL1[(xx)+(yy)]=
	    DEL2[(xx)+(yy)]=
	    DEL3[(xx)+(yy)]=
	    DEL4[(xx)+(yy)]=
	    DEL5[(xx)+(yy)]=
	    DEL6[(xx)+(yy)]=
	    DEL7[(xx)+(yy)]=
	    DEL8[(xx)+(yy)]=
		DEL0[(xx)+(yy)];

	    xx  += u_offset>>2;

	    YUV2[(xx)+(yy)]=
	    DEL1[(xx)+(yy)]=
	    DEL2[(xx)+(yy)]=
	    DEL3[(xx)+(yy)]=
	    DEL4[(xx)+(yy)]=
	    DEL5[(xx)+(yy)]=
	    DEL6[(xx)+(yy)]=
	    DEL7[(xx)+(yy)]=
	    DEL8[(xx)+(yy)]=
		DEL0[(xx)+(yy)];
	}
}

void detect_black_borders()
{
    int x,y,z;
    int vy,vu,vv,vd;
    int top_border;
    int bot_border;
    static int tb=0,bb=0,bbb=0;

    {
	if(border!=-1) tb=border;
	if(border== 0) tb=0;

	if(tb!=0)
	    for(y=0;y<tb;y++)
		for(x=0;x<width;x++)
		{
		    YUV2[x+y*width]=16; 
		    YUV2[x/2+y/2*uv_width+u_offset]=128;
		    YUV2[x/2+y/2*uv_width+v_offset]=128;
		    
		    z=height-y;
		    YUV2[x+z*width]=16;
		    YUV2[x/2+z/2*uv_width+u_offset]=128;
		    YUV2[x/2+z/2*uv_width+v_offset]=128;
		}
    }
}

void antiflicker_reference()
{
    int x,y;
    int diff;
    int value;

    if(framenr>1)
    for(y=0;y<uv_height;y++)
	for(x=0;x<uv_width;x++)
	{
	    YUV1[x+y*uv_width+u_offset]=
		YUV1[x+y*uv_width+u_offset]*0.4+
		DEL0[x+y*uv_width+u_offset]*0.4+
		DEL1[x+y*uv_width+u_offset]*0.2;
	    YUV1[x+y*uv_width+v_offset]=
		YUV1[x+y*uv_width+v_offset]*0.4+
		DEL0[x+y*uv_width+v_offset]*0.4+
		DEL1[x+y*uv_width+v_offset]*0.2;
	}

    /* Do a primitive mean filter */
    for(y=0;y<(height-1);y++)
	for(x=0;x<(width-1);x++)
	{
	    YUV1[(x+0)+(y+0)*width]=
		(
		    YUV1[(x+0)+(y+0)*width]*4+
		    YUV1[(x+1)+(y+0)*width]+
		    YUV1[(x+0)+(y+1)*width]+
		    YUV1[(x+1)+(y+1)*width]
		    )/7;

	}
}

void denoise_image()
{
    int div;
    int x,y;
    uint32_t min_SQD;
    uint32_t min_Y_SQD;
    uint32_t min_U_SQD;
    uint32_t min_V_SQD;
    static uint32_t mean_Y_SQD=-1;
    static uint32_t mean_U_SQD=-1;
    static uint32_t mean_V_SQD=-1;
    float SDEV;
    float RATIO;
    int y_start,y_end;

    /* manual mean-override ? */
    if(mean_Y_SQD==-1)
    {
	mean_Y_SQD  =
	mean_U_SQD  =
	mean_V_SQD  =
	    init_SQD;
    }

    /* remove color-flicker and do a *very* light
       lowpass-filtering of the reference frame
    */
    antiflicker_reference();

    /* fill the delay loop */
    delay_images();

    /* calculate the time averaged image */
    time_average_images();

    /* subsample the reference and the averaged image */
    subsample_averaged_image2();
    subsample_reference_image2();
    subsample_averaged_image4();
    subsample_reference_image4();


    /* top and bottom border "blackener" ? */
    if(border!=-1)
    {
	/* YES ! -> it's not necessary any more to check the 
	   complete image 
	*/
	y_start= border&~15;
	y_end  = height-(border&~15-1);
	y_end  = (y_end>height)? height : y_end;
    }
    else
    {
	/* No, check everything */
	y_start=0;
	y_end  =height;
    }

    /* reset the minimum SQD-counter */
    min_Y_SQD=0;
    min_U_SQD=0;
    min_V_SQD=0;
    div=0;
    for(y=y_start;y<y_end;y+=16)
	for(x=0;x<width;x+=16)
	{
	    div++;
	    /* subsampled full-search, first 4x4 then 8x8 pixels */
	    mb_search_44(x,y);
	    mb_search_88(x,y);

	    /* full-pel search, 16x16 pixels, with range [+/-1 ; +/-1] */
	    mb_search(x,y);

	    /* half-pel search, 16x16 pixels, with range [+/-0.5 ; +/-0.5] */
	    mb_search_half(x,y); 

	    /* calculate the mean of the three SQDs (Y-U-V) */
	    min_Y_SQD+=YSQD;
	    min_U_SQD+=USQD;
	    min_V_SQD+=VSQD;

	    /* it doesn't seem to be a good idea to set the
	       factors to any higher value than '2' ... If
	       you do, you *will* have blocks 
	    */
	    if(	YSQD <= (mean_Y_SQD*2)  &&
		USQD <= (mean_U_SQD*2)  &&
		VSQD <= (mean_V_SQD*2)  )
	    { 
		copy_filtered_block(x,y);
	    }
	    else
	    {
		copy_reference_block(x,y);
	    }
	}

    fprintf(stderr,"---  mean_Y_SQD :%i\n",mean_Y_SQD);
    fprintf(stderr,"---  mean_U_SQD :%i\n",mean_U_SQD);
    fprintf(stderr,"---  mean_V_SQD :%i\n",mean_V_SQD);

    mean_Y_SQD = (mean_Y_SQD + min_Y_SQD/div)/2;
    mean_U_SQD = (mean_U_SQD + min_U_SQD/div)/2;
    mean_V_SQD = (mean_V_SQD + min_V_SQD/div)/2;
}





