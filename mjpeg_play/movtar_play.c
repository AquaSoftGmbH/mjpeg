#include <unistd.h>
#include <math.h>
#include <stdio.h>
#include "jpeg-6b-mmx/jpeglib.h"
#include "movtar.h"
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>

#include <SDL/SDL.h>
#include <SDL/SDL_timer.h>

//#include <Hermes/Hermes.h>

#define readbuffsize 200000

#define dprintf(x) if (debug) fprintf(stderr, x);

/* the usual code continuing here */
char *img=NULL;
int height;
int width;
FILE *movtarfile;
FILE *tempfile;
struct tarinfotype frameinfo;
char *readbufferljud;
char *readbuffer;
char **imglines;
struct movtarinfotype movtarinfo;
SDL_Surface *screen;
SDL_Surface *jpeg;
SDL_Rect jpegdims;

int debug = 1;

/* definition of srcmanager from memory to I/O with libjpeg */
typedef struct {
  struct jpeg_source_mgr pub;
  JOCTET * buffer;
  boolean start_of_file;
} mem_src_mgr;
typedef mem_src_mgr * mem_src_ptr;

METHODDEF(void)
init_source (j_decompress_ptr cinfo)
{
  mem_src_ptr src= (mem_src_ptr) cinfo->src;
  src->start_of_file = TRUE;
}

METHODDEF(boolean)
fill_input_buffer (j_decompress_ptr cinfo)
{
  printf("fel i bufferten\n");
  return TRUE;
}

METHODDEF(void)
skip_input_data (j_decompress_ptr cinfo, long num_bytes)
{
  mem_src_ptr src = (mem_src_ptr) cinfo->src;
  src->pub.next_input_byte += (size_t) num_bytes;
  src->pub.bytes_in_buffer -= (size_t) num_bytes; 
}

METHODDEF(void)
term_source (j_decompress_ptr cinfo)
{
  /* no work necessary here */
}

GLOBAL(void)
jpeg_mem_src (j_decompress_ptr cinfo,void * buff, int size)
{
  mem_src_ptr src;

   cinfo->src = (struct jpeg_source_mgr *)
     (*cinfo->mem->alloc_small) ((j_common_ptr) cinfo, JPOOL_PERMANENT,
				 sizeof(mem_src_mgr));

  src = (mem_src_ptr) cinfo->src;

  src->buffer = (JOCTET *) buff;
  
  src = (mem_src_ptr) cinfo->src;
  src->pub.init_source = init_source;
  src->pub.fill_input_buffer = fill_input_buffer;
  src->pub.skip_input_data = skip_input_data;
  src->pub.resync_to_restart = jpeg_resync_to_restart;
  src->pub.term_source = term_source;
  src->pub.bytes_in_buffer = size;
  src->pub.next_input_byte = src->buffer;
}

/* end of data source manager */

void ComplainAndExit(void)
{
  fprintf(stderr, "Problem: %s\n", SDL_GetError());
  exit(1);
}

void callback_AbortProg(int num)
{
  SDL_Quit();
}

/* forward reference */
void readpicfrommem(void * inbuffer,int size);

void initmovtar(char *filename)
{
  //char *readbuffer;
  int datasize;
  int datatype;

  /* allocate memory for readbuffer */
  readbuffer=(char *)malloc(readbuffsize);
  readbufferljud=(char *)malloc(readbuffsize);
  
  /* open file */
  movtarfile = (FILE *)movtar_open(filename);
  if(movtarfile == NULL)
    {
      fprintf(stderr,"The movtarfile %s couldn't be opened.\n", filename);
      exit(0);
    }

  /* read data from INFO file */
  datasize=movtar_read_tarblock(movtarfile,&frameinfo);
  movtar_read_data(movtarfile,readbuffer,datasize);
  movtar_parse_info(readbuffer,&movtarinfo);
  /* adjust parameters */
  height=movtarinfo.mov_height;
  width=movtarinfo.mov_width;
  /* start sound */
  //audio_init(0,movtarinfo.sound_stereo,movtarinfo.sound_size,movtarinfo.sound_rate);
  //audio_start();

}

void readnext()
{
static int played=0;
static int viewed=0; 

  int datasize; 
  //char *readbuffer;
  int datatype;

/*  played++; */
 
/*   if(viewed>200) */
/*     { */
/*       printf("visat %i av %i\n",viewed,played); */
/*       audio_shutdown(); */
/*       exit(0); */
/*     } */

  do
    {
      //reads until jpegfile found
      datasize=movtar_read_tarblock(movtarfile,&frameinfo);
      datatype=movtar_datatype(&frameinfo);

      /* check if it read video or audio data */
      /* play sound */
      if(datatype & MOVTAR_DATA_AUDIO)
	{
	  movtar_read_data(movtarfile,readbufferljud,datasize);
	  // write audio over SDL 
	  //audio_write(readbufferljud,datasize,0);
	}

      /* test to increase the speed just play some of the frames */
      else if((datatype & MOVTAR_DATA_VIDEO))
      //else if((datatype & MOVTAR_DATA_VIDEO)&&( (shmemptr->buffers) >1))
	{
	  viewed++;
	  movtar_read_data(movtarfile,readbuffer,datasize);
	  readpicfrommem(readbuffer,datasize);
	}
    }
  while(!(datatype & MOVTAR_DATA_VIDEO));
}

void inline triple_swap(unsigned char *a, unsigned char *b, unsigned char *c)
{
  unsigned char tmp_a = *a;
  //unsigned char tmp_b = *b;
  unsigned char tmp_c = *c;
  *a = tmp_c;
  //*b = tmp_b;
  *c = tmp_a;
}

void inline swap(unsigned char *a, unsigned char *b)
{
  unsigned char tmp = *a;
  *a = *b;
  *b = tmp;
}

void readpicfrommem(void *inbuffer,int size)
{
  struct jpeg_error_mgr jerr;
  struct jpeg_decompress_struct cinfo; 
  JSAMPARRAY buffer;
  

  int i, x, y;
  unsigned long pixelval;
  unsigned long mask;

  cinfo.err = jpeg_std_error(&jerr);
	
  jpeg_create_decompress(&cinfo);

  jpeg_mem_src(&cinfo,inbuffer, size);
  jpeg_read_header(&cinfo, TRUE);

  cinfo.out_color_space = JCS_RGB;
  cinfo.dct_method = JDCT_IFAST;

  jpeg_start_decompress(&cinfo);
  

#if 0
  /* lock the screen for current decompression */
  if ( SDL_MUSTLOCK(screen) ) 
    {
      if ( SDL_LockSurface(screen) < 0 )
	ComplainAndExit();
    }
#endif

  if(img == NULL)
    {
      img = jpeg->pixels;
      /* and WHERE are they deallocated ?? */
      if((imglines = (char **)calloc(cinfo.output_height, sizeof(char *)))==NULL)
	{
	  fprintf(stderr, "couldn't allocate memory for imglines\n");
	  exit(0);
	}
      for(i=0;i < cinfo.output_height;i++)
	imglines[i]= img + i * 3 * screen->w;

      jpegdims.x = 0;
      jpegdims.y = 0;
      jpegdims.w = cinfo.output_width;
      jpegdims.h = cinfo.output_height;
    }
  
  while (cinfo.output_scanline < cinfo.output_height) 
    {       
      /* try to save the picture directly */
      jpeg_read_scanlines(&cinfo, (JSAMPARRAY) &imglines[cinfo.output_scanline], 10);
    }


#if 0
  /* need to convert all pixels - ugh ! */
  if (screen->format->BytesPerPixel != 3)
    { 
      // dprintf("Have to convert pixels - BAD for performance !");
      switch(screen->format->BytesPerPixel)
	{
	case 4:
	  mask = 0xffffffff; break;
	case 3:

	  for (y = 0; y < cinfo.output_height; y++)
	    for (x = 0; x < cinfo.output_width; x++)
	      {
		swap(img + (y * screen->w + x) * screen->format->BytesPerPixel, 
		     img + (y * screen->w + x) * screen->format->BytesPerPixel + 2);
	      };
	  break;


	case 2:
	  /*	  pixelval = SDL_MapRGB(screen->format, img[y * screen->w * 3 + x * 3 + 2], 
				img[y * screen->w * 3 + x * 3 + 1], 
				img[y * screen->w * 3 + x * 3 + 0]); 
	  */

	case 1:
	  mask = 0xff; break;
	}

    }
#endif

#if 0 
  /* unlock it again */
  if ( SDL_MUSTLOCK(screen) ) 
    {
      SDL_UnlockSurface(screen);
    }

  /* Update the screen */
  SDL_UpdateRect(screen, 0, 0, screen->w, screen->h);
#endif

  /* Only blit and update the neccessary parts */
  SDL_BlitSurface(jpeg, &jpegdims, screen, &jpegdims);
  SDL_UpdateRect(screen, 0, 0, jpegdims.w, jpegdims.h);
                          
  jpeg_finish_decompress(&cinfo);
}


void dump_pixel_format(struct SDL_PixelFormat *format)
{
  printf("Dumping format content\n");

  printf("BitsPerPixel: %d\n", format->BitsPerPixel);
  printf("BytesPerPixel: %d\n", format->BytesPerPixel);
  printf("Rloss: %d\n", format->Rloss);
  printf("Gloss: %d\n", format->Gloss);
  printf("Bloss: %d\n", format->Bloss);
  printf("Aloss: %d\n", format->Aloss);
  printf("Rshift: %d\n", format->Rshift);
  printf("Gshift: %d\n", format->Gshift);
  printf("Bshift: %d\n", format->Bshift);
  printf("Rmask: 0x%x\n", format->Rmask);
  printf("Gmask: 0x%x\n", format->Gmask);
  printf("Bmask: 0x%x\n", format->Bmask);
  printf("Amask: 0x%x\n", format->Amask);
}

int main(int argc,char** argv)
{ 
  int i;
  unsigned char *buffer;
  char wintitle[255];
  int frame =0;

  /* Initialize SDL library */
  if ( SDL_Init(SDL_INIT_VIDEO) < 0 ) 
    ComplainAndExit();
  
  signal(SIGINT, callback_AbortProg);
  atexit(SDL_Quit);
  
  /* Set the video mode (800x600 at native depth) */
  screen = SDL_SetVideoMode(800, 600, 0, SDL_HWSURFACE | SDL_FULLSCREEN);
  dump_pixel_format(screen->format);
  jpeg = SDL_CreateRGBSurface (SDL_SWSURFACE, 800, 600, 24, 0x0ff, 0x00ff00, 0xff0000, 0xff000000); 
  dump_pixel_format(jpeg->format);

  if ( screen == NULL )  
    ComplainAndExit(); 

  /* init the movtar library */
  initmovtar(argv[1]);

  /* init the Hermes pixel conversion library */
  //if (!Hermes_Init()) 
  //  ComplainAndExit();

  //YUV2MyRGB = Hermes_ConverterInstance(0);
  //Hermes_ConverterRequest(YUV2MyRGB, HermesFormat *source, HermesFormat *dest);


  sprintf(wintitle, "movtar_play %s", argv[1]);
  SDL_WM_SetCaption(wintitle, "0000000");  

  /* Draw bands of color on the raw surface */
  buffer=(unsigned char *)screen->pixels;
  for ( i=0; i < screen->h; ++i ) 
    {
      memset(buffer,(i*255)/screen->h,
	     screen->w*screen->format->BytesPerPixel);
      buffer += screen->pitch;
    }

  do
    {
      readnext();
      frame++;
    }
  while((frame < 200) && !movtar_eof(movtarfile));

  //SDL_Delay(1000);        /* Wait x seconds */

  /* Deinitialize Hermes functions */
  //Hermes_ConverterReturn(YUV2MyRGB);
  //Hermes_Done();
                         
  return 0;
}









