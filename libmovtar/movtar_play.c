#include <unistd.h>
#include <stdio.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>

#include <SDL/SDL.h>
#include <SDL/SDL_timer.h>

#ifndef IRIX
#define JPEG_INTERNALS
#include <jinclude.h>
#endif
#include <jpeglib.h>

#include <glib.h>
#include <movtar.h>

#define readbuffsize 200000

#define dprintf(x) if (debug) fprintf(stderr, x);

/* the usual code continuing here */
char *img=NULL;
int height;
int width;
movtar_t *movtar;
FILE *tempfile;
struct tarinfotype frameinfo;
char readbuffer[readbuffsize];
char **imglines;
SDL_Surface *screen;
SDL_Rect jpegdims;
struct jpeg_decompress_struct cinfo; 

int debug = 1;


/***********************************************************
 * JPEG HACKING (PLUGIN extensions)                        * 
 ***********************************************************/

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
  printf("Error in the JPEG buffer !\n");
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

GLOBAL(void)
jpeg_mem_src_reset (j_decompress_ptr cinfo, int size)
{
  mem_src_ptr src;

  src = (mem_src_ptr) cinfo->src;
  src->pub.bytes_in_buffer = size;
  src->pub.next_input_byte = src->buffer;
}

/* end of data source manager */

#ifndef IRIX 
/* Colorspace conversion */
/* RGB, 32 bits, 8bits each: (Junk), R, G, B */ 
#if defined(__GNUC__)
#define int64 unsigned long long
#endif
static const int64 te0 = 0x0080008000800080; // -128 << 2
static const int64 te1 = 0xe9fa7168e9fa7168; // for cb 
static const int64 te2 = 0x59bad24d59bad24d; // for cr

METHODDEF(void)
ycc_rgb32_convert_mmx (j_decompress_ptr cinfo,
		     JSAMPIMAGE input_buf, JDIMENSION input_row,
		     JSAMPARRAY output_buf, int num_rows)
{
  JSAMPROW outptr;
  JSAMPROW inptr0, inptr1, inptr2;
  JDIMENSION col;
  JDIMENSION num_cols = cinfo->output_width;

  while (--num_rows >= 0) {
    inptr0 = input_buf[0][input_row];
    inptr1 = input_buf[1][input_row];
    inptr2 = input_buf[2][input_row];
    input_row++;
    outptr = *output_buf++;
    num_cols/=4;    for (col = 0; col < num_cols; col++) {
#if defined(HAVE_MMX_INTEL_MNEMONICS)
#error "RGB32 MMX routines haven't been converted to INTEL assembler yet - contact JPEGlib/MMX"
#endif
#if defined(HAVE_MMX_ATT_MNEMONICS)
      asm("movd %1,%%mm0\n"   // mm0: 0 0 0 0 y3 y2 y1 y0 - 8 bit
	  "movd %2,%%mm1\n"   // mm1: 0 0 0 0 cb3 cb2 cb1 cb0
	  "movd %3,%%mm2\n"   // mm2: 0 0 0 0 cr3 cr2 cr1 cr0
	  "pxor %%mm7,%%mm7\n"     // mm7 = 0
	  "punpcklbw %%mm7,%%mm0\n"// mm0: y3 y2 y1 y0 - expand to 16 bit
	  "punpcklbw %%mm7,%%mm1\n"// mm1: cb3 cb2 cb1 cb0
	  "punpcklbw %%mm7,%%mm2\n"// mm2: cr3 cr2 cr1 cr0
	  "psubw te0,%%mm1\n"  //minus 128 for cb and cr
	  "psubw te0,%%mm2\n"
	  "psllw $2,%%mm1\n"       // shift left 2 bits for Cr and Cb to fit the mult constants
	  "psllw $2,%%mm2\n"

	  // prepare for RGB 1 & 0
	  "movq %%mm1,%%mm3\n"     // mm3_16: cb3 cb2 cb1 cb0
	  "movq %%mm2,%%mm4\n"     // mm4_16: cr3 cr2 cr1 cr0
	  "punpcklwd %%mm3,%%mm3\n"// expand to 32 bit: mm3: cb1 cb1 cb0 cb0
	  "punpcklwd %%mm4,%%mm4\n"// mm4: cr1 cr1 cr0 cr0
	  
	  // Y    Y     Y    Y 
	  // 0    CB*g  CB*b 0
	  // CR*r CR*g  0    0
	  //------------------
	  // R    G     B  

	  "pmulhw te1,%%mm3\n"// multiplicate in the constants: mm3: cb1/green cb1/blue cb0/green cb0/blue
	  "pmulhw te2,%%mm4\n"// mm4: cr1/red cb1/green cr0/red cr0/green

	  "movq %%mm0,%%mm5\n"      // mm5: y3 y2 y1 y0
	  "punpcklwd %%mm5,%%mm5\n" // expand to 32 bit: y1 y1 y0 y0
	  "movq %%mm5,%%mm6\n"      // mm6: y1 y1 y0 y0
	  "punpcklwd %%mm5,%%mm5\n" // mm5: y0 y0 y0 y0
	  "punpckhwd %%mm6,%%mm6\n" // mm6: y1 y1 y1 y1

	  // RGB 0
	  "movq %%mm3,%%mm7\n"      // mm7: cb1 cb1 cb0 cb0
	  "psllq $32,%%mm7\n"       // shift left 32 bits: mm7: cb0 cb0 0 0
	  "psrlq $16,%%mm7\n"       // mm7 = 0 cb0 cb0 0
	  "paddw %%mm7,%%mm5\n"     // add: mm7: y+cb
	  "movq %%mm4,%%mm7\n"      // mm7 = cr1 cr1 cr0 cr0
	  "psllq $32,%%mm7\n"       // shift left 32 bits: mm7: cr0 cr0 0 0
	  "paddw %%mm7,%%mm5\n"     // y+cb+cr r g b ?
	  
	  // RGB 1
	  "psrlq $32,%%mm4\n"       // mm4: 0 0 cr1 cr1 
	  "psllq $16,%%mm4\n"       // mm4: 0 cr1 cr1 0 
	  "paddw %%mm4,%%mm6\n"     //y+cr
	  "psrlq $32,%%mm3\n"       // mm3: 0 0 cb1 cb1
	  "paddw %%mm3,%%mm6\n"     //y+cr+cb: mm6 = r g b

	  "psrlq $16,%%mm5\n"        // mm5: 0 r0 g0 b0
	  "packuswb %%mm6,%%mm5\n"   //mm5 = ? r1 g1 b1 0 r0 g0 b0
	  "movq %%mm5,%0\n"         // store mm5

	  // prepare for RGB 2 & 3
	  "punpckhwd %%mm0,%%mm0\n" //mm0 = y3 y3 y2 y2
	  "punpckhwd %%mm1,%%mm1\n" //mm1 = cb3 cb3 cb2 cb2
	  "punpckhwd %%mm2,%%mm2\n" //mm2 = cr3 cr3 cr2 cr2
	  "pmulhw te1,%%mm1\n"      //mm1 = cb * ?
	  "pmulhw te2,%%mm2\n"      //mm2 = cr * ?
	  "movq %%mm0,%%mm3\n"      //mm3 = y3 y3 y2 y2
	  "punpcklwd %%mm3,%%mm3\n" //mm3 = y2 y2 y2 y2
	  "punpckhwd %%mm0,%%mm0\n" //mm0 = y3 y3 y3 y3

	  // RGB 2
	  "movq %%mm1,%%mm4\n"      //mm4 = cb3 cb3 cb2 cb2
	  "movq %%mm2,%%mm6\n"      //mm5 = cr3 cr3 cr2 cr2
	  "psllq $32,%%mm4\n"       //mm4 = cb2 cb2 0 0
 	  "psllq $32,%%mm6\n"       //mm5 = cr2 cr2 0 0
	  "psrlq $16,%%mm4\n"       //mm4 = 0 cb2 cb2 0
	  "paddw %%mm4,%%mm3\n"     // y+cb
	  "paddw %%mm6,%%mm3\n"     //mm3 = y+cb+cr

	  // RGB 3
	  "psrlq $32,%%mm2\n"       //mm2 = 0 0 cr3 cr3
	  "psrlq $32,%%mm1\n"       //mm1 = 0 0 cb3 cb3
	  "psllq $16,%%mm2\n"       //mm1 = 0 cr3 cr3 0
	  "paddw %%mm2,%%mm0\n"     //y+cr
	  "paddw %%mm1,%%mm0\n"     //y+cb+cr

	  "psrlq $16,%%mm3\n"        // shift to the right corner
	  "packuswb %%mm0,%%mm3\n"  // pack in a quadword
	  "movq %%mm3,8%0\n"       //  save two more RGB pixels

	  :"=m"(outptr[0])
	  :"m"(inptr0[0]),"m"(inptr1[0]),"m"(inptr2[0]) //y cb cr
	  : "st");
#endif
      outptr+=16;
      inptr0+=4;
      inptr1+=4;
      inptr2+=4;
    }
  }

  asm ("emms");
}

static int64 rb16mask = 0x00f800f800f800f8; // just red and blue remain
static int64 rb16mult = 0x2000000820000008; // mult/Add factor (see intel appnote 553)
static int64 g16mask = 0x0000f8000000f800; // just green remains
static int64 rgb16offset = 6; // shift right after the whole stuff
static const int64 shiftmask = 0xffff; // shift right after the whole stuff

void calc_rgb16_params(struct SDL_PixelFormat *format)
{
  rb16mask = ((0xff >> format->Rloss) << (16 + format->Rloss)) 
            | (0xff >> format->Bloss) << format->Bloss;
  rb16mask = rb16mask | (rb16mask << 32); // two pixels at once (see default long above)
  
  g16mask = (0xff >> format->Gloss) << (8 + format->Gloss);
  g16mask = g16mask | (g16mask << 32);
  
  rgb16offset = 8 + format->Gloss - format->Gshift; // shift right after the whole stuff
  rb16mult = (1 << (rgb16offset + format->Bshift - format->Bloss)) | 
             (1 << (16 + rgb16offset + format->Rshift - format->Rloss));
  rb16mult = rb16mult | (rb16mult << 32);

  //printf("rb16mask = 0x%llx\n", rb16mask);
  //printf("rb16offset = 0x%llx\n", rgb16offset);
  //printf("g16mask = 0x%llx\n", g16mask);
  //printf("rb16mult = 0x%llx\n", rb16mult);
}

/* RGB, 15/16 bits, 5-6bits each: (Junk), R, G, B */ 
METHODDEF(void)
ycc_rgb16_convert_mmx (j_decompress_ptr cinfo,
		     JSAMPIMAGE input_buf, JDIMENSION input_row,
		     JSAMPARRAY output_buf, int num_rows)
{
  JSAMPROW outptr;
  JSAMPROW inptr0, inptr1, inptr2;
  JDIMENSION col;
  JDIMENSION num_cols = cinfo->output_width;

  while (--num_rows >= 0) {
    inptr0 = input_buf[0][input_row];
    inptr1 = input_buf[1][input_row];
    inptr2 = input_buf[2][input_row];
    input_row++;
    outptr = *output_buf++;
    num_cols/=4;    for (col = 0; col < num_cols; col++) {
#if defined(HAVE_MMX_INTEL_MNEMONICS)
#error "RGB16 MMX routines haven't been converted to INTEL assembler yet - contact JPEGlib/MMX"
#endif
#if defined(HAVE_MMX_ATT_MNEMONICS)
      asm("movd %1,%%mm0\n"   // mm0: 0 0 0 0 y3 y2 y1 y0 - 8 bit
	  "movd %2,%%mm1\n"   // mm1: 0 0 0 0 cb3 cb2 cb1 cb0
	  "movd %3,%%mm2\n"   // mm2: 0 0 0 0 cr3 cr2 cr1 cr0
	  "pxor %%mm7,%%mm7\n"     // mm7 = 0
	  "punpcklbw %%mm7,%%mm0\n"// mm0: y3 y2 y1 y0 - expand to 16 bit
	  "punpcklbw %%mm7,%%mm1\n"// mm1: cb3 cb2 cb1 cb0
	  "punpcklbw %%mm7,%%mm2\n"// mm2: cr3 cr2 cr1 cr0
	  "psubw te0,%%mm1\n"  //minus 128 for cb and cr
	  "psubw te0,%%mm2\n"
	  "psllw $2,%%mm1\n"       // shift left 2 bits for Cr and Cb to fit the mult constants
	  "psllw $2,%%mm2\n"

	  // prepare for RGB 1 & 0
	  "movq %%mm1,%%mm3\n"     // mm3_16: cb3 cb2 cb1 cb0
	  "movq %%mm2,%%mm4\n"     // mm4_16: cr3 cr2 cr1 cr0
	  "punpcklwd %%mm3,%%mm3\n"// expand to 32 bit: mm3: cb1 cb1 cb0 cb0
	  "punpcklwd %%mm4,%%mm4\n"// mm4: cr1 cr1 cr0 cr0
	  
	  // Y    Y     Y    Y 
	  // 0    CB*g  CB*b 0
	  // CR*r CR*g  0    0
	  //------------------
	  // R    G     B  

	  "pmulhw te1,%%mm3\n"// multiplicate in the constants: mm3: cb1/green cb1/blue cb0/green cb0/blue
	  "pmulhw te2,%%mm4\n"// mm4: cr1/red cb1/green cr0/red cr0/green

	  "movq %%mm0,%%mm5\n"      // mm5: y3 y2 y1 y0
	  "punpcklwd %%mm5,%%mm5\n" // expand to 32 bit: y1 y1 y0 y0
	  "movq %%mm5,%%mm6\n"      // mm6: y1 y1 y0 y0
	  "punpcklwd %%mm5,%%mm5\n" // mm5: y0 y0 y0 y0
	  "punpckhwd %%mm6,%%mm6\n" // mm6: y1 y1 y1 y1

	  // RGB 0
	  "movq %%mm3,%%mm7\n"      // mm7: cb1 cb1 cb0 cb0
	  "psllq $32,%%mm7\n"       // shift left 32 bits: mm7: cb0 cb0 0 0
	  "psrlq $16,%%mm7\n"       // mm7 = 0 cb0 cb0 0
	  "paddw %%mm7,%%mm5\n"     // add: mm7: y+cb
	  "movq %%mm4,%%mm7\n"      // mm7 = cr1 cr1 cr0 cr0
	  "psllq $32,%%mm7\n"       // shift left 32 bits: mm7: cr0 cr0 0 0
	  "paddw %%mm7,%%mm5\n"     // y+cb+cr r g b ?
	  "psrlq $16,%%mm5\n"        // mm5: 0 r0 g0 b0

	  // RGB 1
	  "psrlq $32,%%mm4\n"       // mm4: 0 0 cr1 cr1 
	  "psllq $16,%%mm4\n"       // mm4: 0 cr1 cr1 0 
	  "paddw %%mm4,%%mm6\n"     //y+cr
	  "psrlq $32,%%mm3\n"       // mm3: 0 0 cb1 cb1
	  "paddw %%mm3,%%mm6\n"     //y+cr+cb: mm6 = r g b

	  "packuswb %%mm6,%%mm5\n"   //mm5 = ? r1 g1 b1 0 r0 g0 b0 

	  // prepare for RGB 2 & 3
	  "punpckhwd %%mm0,%%mm0\n" //mm0 = y3 y3 y2 y2
	  "punpckhwd %%mm1,%%mm1\n" //mm1 = cb3 cb3 cb2 cb2
	  "punpckhwd %%mm2,%%mm2\n" //mm2 = cr3 cr3 cr2 cr2
	  "pmulhw te1,%%mm1\n"      //mm1 = cb * ?
	  "pmulhw te2,%%mm2\n"      //mm2 = cr * ?
	  "movq %%mm0,%%mm3\n"      //mm3 = y3 y3 y2 y2
	  "punpcklwd %%mm3,%%mm3\n" //mm3 = y2 y2 y2 y2
	  "punpckhwd %%mm0,%%mm0\n" //mm0 = y3 y3 y3 y3

	  // RGB 2
	  "movq %%mm1,%%mm4\n"      //mm4 = cb3 cb3 cb2 cb2
	  "movq %%mm2,%%mm6\n"      //mm5 = cr3 cr3 cr2 cr2
	  "psllq $32,%%mm4\n"       //mm4 = cb2 cb2 0 0
 	  "psllq $32,%%mm6\n"       //mm5 = cr2 cr2 0 0
	  "psrlq $16,%%mm4\n"       //mm4 = 0 cb2 cb2 0
	  "paddw %%mm4,%%mm3\n"     // y+cb
	  "paddw %%mm6,%%mm3\n"     //mm3 = y+cb+cr

	  // RGB 3
	  "psrlq $32,%%mm2\n"       //mm2 = 0 0 cr3 cr3
	  "psrlq $32,%%mm1\n"       //mm1 = 0 0 cb3 cb3
	  "psllq $16,%%mm2\n"       //mm1 = 0 cr3 cr3 0
	  "paddw %%mm2,%%mm0\n"     //y+cr
	  "paddw %%mm1,%%mm0\n"     //y+cb+cr

	  "psrlq $16,%%mm3\n"        // shift to the right corner
	  "packuswb %%mm0,%%mm3\n"  // pack in a quadword

	  // mm5 for pixels 0 and 1, mm3 contains pixel 2 and 3, 16 bit conversion:
	  // Reusing mm0, mm6 (this code could be optimized by interleaving the commands, I don't dare)
	  // see http://developer.intel.com/drg/mmx/appnotes/ap553.htm:
	  //      rgb24to16 is an optimized MMX routine to convert RGB data from 
	  //      24 bit true color to 16 bit high color.  The inner loop processes 8 
	  //      pixels at a time and packs the 8 pixels represented as 8 DWORDs 
	  //      into 8 WORDs. The algorithm used for each 2 pixels is as follows:
	  //      Step 1:         read in 2 pixels as a quad word => mm5
	  //      Step 2:         make a copy of the two pixels
	  "movq %%mm3, %%mm0\n" // mm3 = ? r1 g1 b1 0 r0 g0 b0
	  "movq %%mm5, %%mm6\n" // mm7 = ? r1 g1 b1 0 r0 g0 b0

	  //      Step 3:         AND the 2 pixels with 0x00f800f800f800f8 to obtain a 
	  //                              quad word of:
	  //      00000000RRRRR00000000000BBBBB000 00000000rrrrr00000000000bbbbb000
	  "pand rb16mask, %%mm3\n"
	  "pand rb16mask, %%mm5\n"

	  //      Step 4:         PMADDWD this quad word by 0x2000000820000008 to obtain
	  //                              a quad word of:
	  //      00000000000RRRRR00000BBBBB000000 00000000000rrrrr00000bbbbb000000
	  "pmaddwd rb16mult, %%mm3\n"
	  "pmaddwd rb16mult, %%mm5\n"

	  //      Step 5:         AND the copy of the original pixels with 
	  //                              0x0000f8000000f800 to obtain
	  //      0000000000000000GGGGG00000000000 0000000000000000ggggg00000000000

	  "pand g16mask, %%mm0\n"
	  "pand g16mask, %%mm6\n"
	  //      Step 6:         OR the results of step 4 and step 5 to obtain
	  //      00000000000RRRRRGGGGGBBBBB000000 00000000000rrrrrgggggbbbbb000000
	  "por %%mm6, %%mm5\n"
	  "por %%mm0, %%mm3\n"

	  //      Step 7:         SHIFT RIGHT by 6 bits to obtain
	  //      00000000000000000RRRRRGGGGGBBBBB 00000000000000000rrrrrgggggbbbbb
	  "psrlq rgb16offset, %%mm5\n"
	  "psrlq rgb16offset, %%mm3\n"

	  //      Step 8:         When two pairs of pixels are converted, pack the 
	  //                              results into one register and then store them into
	  //                              the q Matrix.
          //"packssdw %%mm3, %%mm5\n" // this is the magic command for ONLY 15bit color !!
	  // and would replace all the workaround code below !!!! Reason: There is no packusdw !! 
          "movq %%mm5, %%mm6\n" // copy mm5
	  "psrlq $16, %%mm6\n" // shift out pixel 1, keep pixel 0
	  "pand shiftmask, %%mm5\n" // and out pixel 0
	  "por %%mm6, %%mm5\n" // or pix 0 and pix 1 together
	  "movd %%mm5, %0\n" // write pix 0 and 1 out

          "movq %%mm3, %%mm0\n" // copy mm3
	  "psrlq $16, %%mm0\n" // shift out pixel 3, keep pixel 2
	  "pand shiftmask, %%mm3\n" // and out pixel 2
	  "por %%mm0, %%mm3\n" // or pix 3 and pix 2 together
	  "movd %%mm3, 4%0\n" // write pix 2 and 3

	  :"=m"(outptr[0])
	  :"m"(inptr0[0]),"m"(inptr1[0]),"m"(inptr2[0]) //y cb cr
	  : "st");
#endif
      outptr+=8;
      inptr0+=4;
      inptr1+=4;
      inptr2+=4;
    }
  }

  asm ("emms");
}
#endif // ifndef IRIX

/* end of custom color deconverter */

/********************************************************
 *          MAIN PROGRAM                                *
 ********************************************************/

void ComplainAndExit(void)
{
  fprintf(stderr, "Problem: %s\n", SDL_GetError());
  exit(1);
}

void callback_AbortProg(int num)
{
  SDL_Quit();
}

void initmovtar(char *filename)
{
  /* open file */
  movtar = movtar_open(filename, 1, 0, 0x0);
  if(movtar == NULL)
    {
      fprintf(stderr,"The movtar %s couldn't be opened.\n", filename);
      exit(0);
    }

  /* adjust parameters */
  height = movtar_video_height(movtar);
  width = movtar_video_width(movtar);
}  

void inline readpicfrommem(void *inbuffer, long size)
{
  static struct jpeg_color_deconverter *cconvert;
  int i;

  jpeg_mem_src_reset(&cinfo, size);
  jpeg_read_header(&cinfo, TRUE);

  cinfo.dct_method = JDCT_IFAST;
  jpeg_start_decompress(&cinfo);

  switch (screen->format->BytesPerPixel)
    {
    case 4:
#ifndef IRIX
      cconvert = cinfo.cconvert;
      cconvert->color_convert = ycc_rgb32_convert_mmx;
#else
      fprintf(stderr, "32 bits per pixel can't be decoded by libjpeg on IRIX !\n");
#endif
      break;
    case 2:
#ifndef IRIX
      cconvert = cinfo.cconvert;
      cconvert->color_convert = ycc_rgb16_convert_mmx;
#else
      fprintf(stderr, "15/16 bits per pixel can't be decoded by libjpeg on IRIX!");
#endif
      break;
    default: break;
    }

  /* lock the screen for current decompression */
  if ( SDL_MUSTLOCK(screen) ) 
    {
      if ( SDL_LockSurface(screen) < 0 )
	ComplainAndExit();
    }

  if(img == NULL)
    {
      img = screen->pixels;

      if((imglines = (char **)calloc(cinfo.output_height, sizeof(char *)))==NULL)
	{
	  fprintf(stderr, "couldn't allocate memory for imglines\n");
	  exit(0);
	}
      for(i=0;i < cinfo.output_height;i++)
	imglines[i]= screen->pixels + i * screen->format->BytesPerPixel * screen->w;

      jpegdims.x = 0; // This is not going to work with interlaced pics !!
      jpegdims.y = 0;
      jpegdims.w = cinfo.output_width;
      jpegdims.h = cinfo.output_height;
    }
  
  while (cinfo.output_scanline < cinfo.output_height) 
    {       
      /* try to save the picture directly */
      jpeg_read_scanlines(&cinfo, (JSAMPARRAY) &imglines[cinfo.output_scanline], 100);
    }

  /* unlock it again */
  if ( SDL_MUSTLOCK(screen) ) 
    {
      SDL_UnlockSurface(screen);
    }

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

void debug_writejpeg(unsigned char *filebuffer, long size)
{
  FILE *temp;
  
  temp = fopen("/root/temp.jpeg", "w");
  fwrite(filebuffer, 1, size, temp);
  fclose(temp);
}

void debug_drawcolors(SDL_Surface *screen)
{
  int i;
  unsigned char *buffer;

  /* Draw bands of color on the raw surface, as run indicator for debugging */
    buffer=(unsigned char *)screen->pixels;
  for ( i=0; i < screen->h; ++i ) 
    {
      memset(buffer,(i*255)/screen->h,
	     screen->w*screen->format->BytesPerPixel);
      buffer += screen->pitch;
    }
}

int main(int argc,char** argv)
{ 
  char wintitle[255];
  int frame =0;
  SDL_Event event;
  struct jpeg_error_mgr jerr;
  long framesize;

  movtar_init(FALSE, FALSE);

  if (movtar_check_sig(argv[1]) == 0)
    {  fprintf(stderr, "%s does not seem to be a movtar file.\n", argv[1]); exit(1); }

  /* Initialize SDL library */
  if ( SDL_Init(SDL_INIT_VIDEO) < 0 ) 
    ComplainAndExit();
  
  signal(SIGINT, callback_AbortProg);
  atexit(SDL_Quit);
  
  /* get the movtar parameters */
  initmovtar(argv[1]);

  printf("wxh: %dx%d@%f fr/s\n", width, height, movtar_frame_rate(movtar));

  /* Set the video mode (at least the movtar resolution, with native bitdepth) */
#ifndef IRIX /* let the hardware choose its mode */
  screen = SDL_SetVideoMode(width, height, 0, SDL_HWSURFACE /* | SDL_HWSURFACE *//*| SDL_FULLSCREEN */);
#else /* must force it to a mode */
  screen = SDL_SetVideoMode(width, height, 24, SDL_HWSURFACE /* | SDL_HWSURFACE *//*| SDL_FULLSCREEN */);
#endif
  SDL_EventState(SDL_KEYDOWN, SDL_ENABLE);
  SDL_EventState(SDL_MOUSEMOTION, SDL_IGNORE);

  dump_pixel_format(screen->format);

#ifdef IRIX
  fprintf(stderr, "Screen parameters haven't been determined yet !\n");
#else
  calc_rgb16_params(screen->format);
#endif

  if ( screen == NULL )  
    ComplainAndExit(); 
  cinfo.err = jpeg_std_error(&jerr);	
  jpeg_create_decompress(&cinfo);
  cinfo.out_color_space = JCS_RGB;
  cinfo.dct_method = JDCT_IFAST;
  jpeg_mem_src(&cinfo, readbuffer, 200000);

  sprintf(wintitle, "movtar_play %s", argv[1]);
  SDL_WM_SetCaption(wintitle, "0000000");  

  do
    {
      framesize = movtar_read_frame(movtar, readbuffer);
      printf("Now showing frame %d (size %ld)\n", frame, framesize);

      if (framesize != 0)
	{
	  if (framesize == 1) fprintf(stderr, "fake frame at %d !\n", frame);
	  readpicfrommem(readbuffer, framesize);
	  frame++;
	}
    }
  while((frame < 250) && (framesize != 0) && !SDL_PollEvent(&event));

  return 0;
}









