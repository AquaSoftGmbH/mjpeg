/*
 * $Id$
 *
 * Simple program to print a crude histogram of the Y'CbCr values for YUV4MPEG2
 * stream.  Usually used with a small number (single) of frames but that's not
 * a requirement.  Reads stdin until end of stream and then prints the Y'CbCr
 * counts.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#else
#define HAVE_STDINT_H
#include <stdint.h>
#endif
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <yuv4mpeg.h>

unsigned long long y_stats[256], u_stats[256], v_stats[256];
unsigned long long fy_stats[256], fu_stats[256], fv_stats[256];
unsigned long long ly_stats[256], lu_stats[256], lv_stats[256];
/* the l?_stats means till last frame, and f?_stat means the actual frame */

/* For the graphical history output */
#ifdef HAVE_SDLgfx
#include <SDL/SDL.h>
#include <SDL/SDL_gfxPrimitives.h>

/* Defining some colors */
#define white 0xFFFFFFFF
#define black 0x000000FF
#define red  0xFF0000FF
#define gray   0xA5A5A5FF
/* Defining the size of the screen */
#define width 640
#define heigth 480

/* Defining where the histograms are */
#define sum1_x 30
#define sum1_y 30
#define frm1_x 320
#define frm1_y 30 
#define sum2_x 30
#define sum2_y 180
#define frm2_x 320
#define frm2_y 180 
#define sum3_x 30
#define sum3_y 340
#define frm3_x 320
#define frm3_y 340 

	/* needed for SDL */
	SDL_Surface *screen;
	int desired_bpp;
        Uint32 video_flags;
	long number_of_frames;

void HandleEvent()
	{
        SDL_Event event;

        /* Check for events */
        while	(SDL_PollEvent(&event))
		{
		switch	(event.type)
			{
                        case SDL_KEYDOWN:
                        case SDL_QUIT:
				exit(0);
                        }
        	}
	}

void ClearScreen(SDL_Surface *screen)
	{
        int i;
        /* Set the screen to black */
        if	(SDL_LockSurface(screen) == 0 )
		{
                Uint32 Black;
                Uint8 *pixels;
                Black = SDL_MapRGB(screen->format, 0, 0, 0);
                pixels = (Uint8 *)screen->pixels;
                for	( i=0; i<screen->h; ++i )
			{
                        memset(pixels, Black,
                                screen->w*screen->format->BytesPerPixel);
                        pixels += screen->pitch;
                	}
                SDL_UnlockSurface(screen);
        	}
	}

/* Here we draw the layout of the histogram */
void draw_histogram_layout(int x_off, int y_off)
	{
	int i, offset;

	hlineColor(screen, x_off, x_off+257, y_off+101 , white);
	vlineColor(screen, x_off-1, y_off, y_off+101, white);
	vlineColor(screen, x_off+257,y_off,y_off+101, white);

	for	(i = 0; i < 15; i++)
  		{
		offset = i*16+ 16;
		vlineColor(screen, x_off+offset, y_off+101, y_off+105, white);
		}
	vlineColor(screen, x_off+128, y_off+106, y_off+110, white);
	}

/* init the screen */
void y4m_init_area(SDL_Surface *screen)
	{
/* Drawing for the summ of frames */
	draw_histogram_layout(sum1_x, sum1_y);
	draw_histogram_layout(frm1_x, frm1_y);
	draw_histogram_layout(sum2_x, sum2_y);
	draw_histogram_layout(frm2_x, frm2_y);
	draw_histogram_layout(sum3_x, sum3_y);
	draw_histogram_layout(frm3_x, frm3_y);

	SDL_UpdateRect(screen,0,0,0,0);
	}

void make_text(int x_off, int y_off, char text[25])
	{

	boxColor(screen, x_off, y_off-20, x_off+256, y_off-5, black); 
	stringColor(screen, x_off+10, y_off-12, text, white);
	}

/* Here we draw the text description for the histograms */
void make_histogram_desc(long number_of_frames)
	{
	char framestat[25];

	sprintf(framestat, "Y Stat till frame %ld", number_of_frames);
	make_text(sum1_x, sum1_y, framestat);
	sprintf(framestat, "U Stat till frame %ld", number_of_frames);
	make_text(sum2_x, sum2_y, framestat);
	sprintf(framestat, "V Stat till frame %ld", number_of_frames);
	make_text(sum3_x, sum3_y, framestat);

	sprintf(framestat, "Y Stat curent frame");
	make_text(frm1_x, frm1_y, framestat);
	sprintf(framestat, "U Stat curent frame");
	make_text(frm2_x, frm2_y, framestat);
	sprintf(framestat, "V Stat curent frame");
	make_text(frm3_x, frm3_y, framestat);
	}

/* Here we delete the old histograms */
void clear_histogram_area()
	{
	boxColor(screen, sum1_x, sum1_y-1, sum1_x+256, sum1_y+100, black); 
	boxColor(screen, sum2_x, sum2_y-1, sum2_x+256, sum2_y+100, black); 
	boxColor(screen, sum3_x, sum3_y-1, sum3_x+256, sum3_y+100, black); 

	boxColor(screen, frm1_x, frm1_y-1, frm1_x+256, frm1_y+100, black); 
	boxColor(screen, frm2_x, frm2_y-1, frm2_x+256, frm2_y+100, black); 
	boxColor(screen, frm3_x, frm3_y-1, frm3_x+256, frm3_y+100, black); 
	} 

/* Show the frame stat */
void make_stat()
	{
	int percent_y, percent_u, percent_v, percent_fy, percent_fu, percent_fv;
	long long num_pixels_y, num_pixels_u, num_pixels_v;
	long long f_pixels_y, f_pixels_u, f_pixels_v;
	int i;

	num_pixels_y = y_stats[0];
	num_pixels_u = u_stats[0];
	num_pixels_v = v_stats[0];

	f_pixels_y = fy_stats[0];
	f_pixels_u = fu_stats[0];
	f_pixels_v = fv_stats[0];

/* geting the maimal number for all frames */
	for	(i = 0; i < 255; i++)
  		{  /* getting the maximal numbers for Y, U, V for all frames */
		if	(num_pixels_y < y_stats[i]) 
			num_pixels_y = y_stats[i];
		if	(num_pixels_u < u_stats[i]) 
			num_pixels_u = u_stats[i];
		if	(num_pixels_v < v_stats[i]) 
			num_pixels_v = v_stats[i];

/* getting the maximal numbers for Y, U, V for the current frame */
		fy_stats[i]= y_stats[i] - ly_stats[i];
		ly_stats[i] = y_stats[i];
		if	(f_pixels_y < fy_stats[i])
			f_pixels_y = fy_stats[i];

		fu_stats[i]= u_stats[i] - lu_stats[i];
		lu_stats[i] = u_stats[i];
		if	(f_pixels_u < fu_stats[i])
			f_pixels_u = fu_stats[i];

		fv_stats[i]= v_stats[i] - lv_stats[i];
		lv_stats[i] = v_stats[i];
		if	(f_pixels_v < fv_stats[i])
			f_pixels_v = fv_stats[i];
  		} 

	num_pixels_y = (num_pixels_y /100);
	num_pixels_u = (num_pixels_u /100);
	num_pixels_v = (num_pixels_v /100);

	f_pixels_y = (f_pixels_y /100);
	f_pixels_u = (f_pixels_u /100);
	f_pixels_v = (f_pixels_v /100);

/* The description for the histogram */
	make_histogram_desc(number_of_frames);
	number_of_frames++;

	clear_histogram_area(); /* Here we delete the old histograms */

	for	(i = 0; i < 255; i++)
		{
		percent_y = (y_stats[i] / num_pixels_y);
		percent_u = (u_stats[i] / num_pixels_u);
		percent_v = (v_stats[i] / num_pixels_v);

		percent_fy = (fy_stats[i] / f_pixels_y);
		percent_fu = (fu_stats[i] / f_pixels_u);
		percent_fv = (fv_stats[i] / f_pixels_v);

		if	((i < 16) || (i > 235)) /* Y luma */
			{   /* Red means out of the allowed range */
			vlineColor(screen,(sum1_x+i),sum1_y+100,
					((sum1_y+100)-percent_y),red);
			vlineColor(screen,(frm1_x+i),frm1_y+100,
					((frm1_y+100)-percent_fy),red);
			}
		else    
			{
			vlineColor(screen,(sum1_x+i),sum1_y+100,
					((sum1_y+100)-percent_y),gray);
			vlineColor(screen,(frm1_x+i),frm1_y+100,
					((frm1_y+100)-percent_fy),gray);
			}

		if	((i < 16) || (i > 240)) /* U V chroma */
			{   /* Red means out of the allowed range */
			vlineColor(screen,(sum2_x+i),sum2_y+100,
					((sum2_y+100)-percent_u),red);
			vlineColor(screen,(sum3_x+i),sum3_y+100,
					((sum3_y+100)-percent_v),red);
			vlineColor(screen,(frm2_x+i),frm2_y+100,
					((frm2_y+100)-percent_fu),red);
			vlineColor(screen,(frm3_x+i),frm3_y+100,
					((frm3_y+100)-percent_fv),red);
			}
		else    
			{
			vlineColor(screen,(sum2_x+i),sum2_y+100,
					((sum2_y+100)-percent_u),gray);
			vlineColor(screen,(sum3_x+i),sum3_y+100,
					((sum3_y+100)-percent_v),gray);
			vlineColor(screen,(frm2_x+i),frm2_y+100,
					((frm2_y+100)-percent_fu),gray);
			vlineColor(screen,(frm3_x+i),frm3_y+100,
					((frm3_y+100)-percent_fv),gray);
			}
		}
	SDL_UpdateRect(screen,0,0,0,0);
	}
#endif /* HAVE_SDLgfx */

static void
usage(void)
	{
	mjpeg_error_exit1("usage: [-t]\n\temit text summary even if graphical mode enabled");
	}

int
main(int argc, char **argv)
	{
	int	i, fdin, ss_v, ss_h, chroma_ss, textout;
	int	plane0_l, plane1_l, plane2_l;
	u_char	*yuv[3], *cp;
	y4m_stream_info_t istream;
	y4m_frame_info_t iframe;


#ifdef	HAVE_SDLgfx
	textout = 0;
#else
	textout = 1;
#endif

	while	((i = getopt(argc, argv, "t")) != EOF)
		{
		switch	(i)
			{
			case	't':
				textout = 1;
				break;
			default:
				usage();
			}
		}

#ifdef HAVE_SDLgfx
	/* Initialize SDL */
	desired_bpp = 8; 
	video_flags = 0;
	video_flags |= SDL_DOUBLEBUF;
	number_of_frames = 1;

	memset(fy_stats, '\0', sizeof (fy_stats));
	memset(ly_stats, '\0', sizeof (ly_stats));

        if	( SDL_Init(SDL_INIT_VIDEO) < 0 ) 
                mjpeg_error_exit1("Couldn't initialize SDL:%s",SDL_GetError());
        atexit(SDL_Quit);                       /* Clean up on exit */
        /* Initialize the display */
        screen = SDL_SetVideoMode(width, heigth, desired_bpp, video_flags);
        if	(screen == NULL)
                mjpeg_error_exit1("Couldn't set %dx%dx%d video mode: %s",
                                width, heigth, desired_bpp, SDL_GetError());

	SDL_WM_SetCaption("y4mhistogram", "y4mhistogram");

	y4m_init_area(screen); /* Here we draw the basic layout */
#endif /* HAVE_SDLgfx */

	fdin = fileno(stdin);

	y4m_accept_extensions(1);

	y4m_init_stream_info(&istream);
	y4m_init_frame_info(&iframe);

	if	(y4m_read_stream_header(fdin, &istream) != Y4M_OK)
		mjpeg_error_exit1("stream header error");

        if      (y4m_si_get_plane_count(&istream) != 3)
                mjpeg_error_exit1("Only 3 plane formats supported");

	chroma_ss = y4m_si_get_chroma(&istream);
	ss_h = y4m_chroma_ss_x_ratio(chroma_ss).d;
	ss_v = y4m_chroma_ss_y_ratio(chroma_ss).d;

	plane0_l = y4m_si_get_plane_length(&istream, 0);
	plane1_l = y4m_si_get_plane_length(&istream, 1);
	plane2_l = y4m_si_get_plane_length(&istream, 2);

	yuv[0] = malloc(plane0_l);
	if	(yuv[0] == NULL)
		mjpeg_error_exit1("malloc(%d) plane 0", plane0_l);
	yuv[1] = malloc(plane1_l);
	if	(yuv[1] == NULL)
		mjpeg_error_exit1(" malloc(%d) plane 1", plane1_l);
	yuv[2] = malloc(plane2_l);
	if	(yuv[2] == NULL)
		mjpeg_error_exit1(" malloc(%d) plane 2\n", plane2_l);

	while	(y4m_read_frame(fdin,&istream,&iframe,yuv) == Y4M_OK)
		{
		for	(i = 0, cp = yuv[0]; i < plane0_l; i++, cp++)
			y_stats[*cp]++; /* Y' */
		for	(i = 0, cp = yuv[1]; i < plane1_l; i++, cp++)
			u_stats[*cp]++;	/* U */
		for	(i = 0, cp = yuv[2]; i < plane2_l; i++, cp++)
			v_stats[*cp]++;	/* V */
#ifdef HAVE_SDLgfx
		make_stat();

		/* Events for SDL */
		HandleEvent();
#endif
		}
	y4m_fini_frame_info(&iframe);
	y4m_fini_stream_info(&istream);

	if	(textout)
		{
		for	(i = 0; i < 255; i++)
			printf("Y %d %lld\n", i, y_stats[i]);
		for	(i = 0; i < 255; i++)
			printf("U %d %lld\n", i, u_stats[i]);
		for	(i = 0; i < 255; i++)
			printf("V %d %lld\n", i, v_stats[i]);
		}
	exit(0);
	}
