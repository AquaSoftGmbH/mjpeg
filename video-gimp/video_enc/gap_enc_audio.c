/* gap_enc_audio.c
 *
 *  GAP common encoder audio tool procedures
 *  with no dependencies to external libraries.
 *  Some tools to read/write audiofiles
 *
 */

 
/* SYTEM (UNIX) includes */ 
#include <stdio.h>
#include <stdlib.h> 

/* GIMP includes */
#include "gtk/gtk.h"
#include <stdplugins-intl.h>
#include <libgimp/gimp.h> 

/* GAP includes */
#include "gap_enc_audio.h"

extern      int gap_debug; /* ==0  ... dont print debug infos */
 
void
p_stereo_split16to16(unsigned char *l_left_ptr, unsigned char *l_right_ptr, unsigned char *l_aud_ptr, long l_data_len)
{
  long l_idx;

  /* split stereo 16 bit data from wave data (sequence is  2 bytes left channel, 2 right, 2 left, 2 right ...)
   * into 2 seperate datablocks for left and right channel of 16 bit per sample
   */
  l_idx = 0; 
  while(l_idx < l_data_len)
  {
    *(l_left_ptr++)  = l_aud_ptr[l_idx++];
    *(l_left_ptr++)  = l_aud_ptr[l_idx++];
    *(l_right_ptr++) = l_aud_ptr[l_idx++];
    *(l_right_ptr++) = l_aud_ptr[l_idx++];
  }
}

void
p_dbl_sample_8_to_16(unsigned char insample8,
                     unsigned char *lsb_out,
                     unsigned char *msb_out)
{
  /* 16 bit audiodata is signed, 
   *  8 bit audiodata is unsigned
   */   

   /* this conversion uses only positive 16bit values
    */
   *msb_out = (insample8 >> 1);
   *lsb_out = ((insample8 << 7) &  0x80);
   return;


   /* this conversion makes use of negative 16bit values
    * and the full sample range 
    * (somehow it sounds not as good as the other one)
    */
   *msb_out = (insample8 -64);
   *lsb_out = 0;
}

void
p_stereo_split8to16(unsigned char *l_left_ptr, unsigned char *l_right_ptr, unsigned char *l_aud_ptr, long l_data_len)
{
  long l_idx;
  unsigned char l_lsb, l_msb;

  /* split stereo 8 bit data from wave data (sequence is  2 bytes left channel, 2 right, 2 left, 2 right ...)
   * into 2 seperate datablocks for left and right channel of 16 bit per sample
   */
  l_idx = 0; 
  while(l_idx < l_data_len)
  {
    p_dbl_sample_8_to_16(l_aud_ptr[l_idx], &l_lsb, &l_msb);
    l_idx++;
    *(l_left_ptr++)  = l_lsb;
    *(l_left_ptr++)  = l_msb;
    
    p_dbl_sample_8_to_16(l_aud_ptr[l_idx], &l_lsb, &l_msb);
    l_idx++;
    *(l_left_ptr++)  = l_lsb;
    *(l_left_ptr++)  = l_msb;
  }
}




/* p_get_wavparams
 * Opens a WAV file and determines its parameters.
 * in: inwavname: Filename of the WAV file to open.
 * out:wavsize: The size of the file content.
 *     sound_size: The bit size of the samples (usually 8 or 16).
 *     sound_rate: The sample rate (e.g. 22050 Hz).
 *     sound_stereo: TRUE: This video contains stereo sound. 
 * returns: FILE *: A file pointer to the WAV file, already pointing to the
 *                   desired audio content. (NULL on error).
 */

FILE *p_get_wavparams(const char *inwavname, gint32 *wavsize,
		      gint32 *sound_size, gint32 *sound_rate, gint32 *sound_stereo)
{ 
  WaveHeader wp;
  FILE *inwav;

  *sound_size = -1;
  /* open input wavfile */
  inwav = fopen(inwavname, "rb+");
  if (inwav == NULL) { perror("gap_encode: Opening the input WAVE-file failed.\n"); return NULL; }

  if (fread(&wp, 1, sizeof(WaveHeader), inwav) != sizeof(WaveHeader))
  { printf("gap_encode: Error reading the WAVE-file header.\n"); return NULL; }

  if (wp.main_chunk == WAV_RIFF && wp.chunk_type == WAV_WAVE &&
      wp.sub_chunk == WAV_FMT && wp.data_chunk == WAV_DATA) 
    {
      if (wp.format != WAV_PCM_CODE) 
	{ fprintf(stderr, "gap_encode: Can't process other than PCM-coded WAVE-files\n"); return NULL; }

      if (wp.modus < 1 || wp.modus > 2) 
	{ fprintf(stderr, "gap_encode: Can't process WAVE-files with %d tracks\n", wp.modus); return NULL; }

      *sound_stereo = (wp.modus == 2) ? TRUE : FALSE;
      *sound_size = wp.bit_p_spl;
      *sound_rate = wp.sample_fq;
      *wavsize = wp.data_length;
    }
  else
    { printf("gap_encode: %s is no valid WAVE-file.\n", 
	     inwavname); return NULL;
    }

  if (gap_debug) 
  { printf("The WAVE-file %s has the following parameters\n"
	   "Audio rate: %d Hz, Sample size: %d bits, Stereo: %s\n",
	   inwavname, *sound_rate, *sound_size, 
	   (*sound_stereo == TRUE) ? "yes" : "no");
  }
  return inwav;
}




