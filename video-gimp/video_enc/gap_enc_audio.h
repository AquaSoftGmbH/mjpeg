/* gap_enc_audio.h
 *
 *  GAP common encoder audio tool procedures
 *  with no dependencies to external libraries.
 *  Some tools to read/write audiofiles
 *
 */

#ifndef GAP_ENC_AUDIO_H
#define GAP_ENC_AUDIO_H


/* GIMP includes */
#include "gtk/gtk.h"
#include "libgimp/gimp.h"



/* Definitions for Microsoft WAVE format -
   I am not using libsndfile yet */
#define WAV_RIFF		0x46464952
#define WAV_WAVE		0x45564157
#define WAV_FMT			0x20746D66
#define WAV_DATA		0x61746164
#define WAV_PCM_CODE		1

/* it's in chunks like .voc and AMIGA iff, but my source says they
   exist only in this combination, so I combined them in one header;
   it works with all WAVE-files I have.
 */
typedef struct wav_header {
	gint main_chunk;	/* 'RIFF' */
	gint length;		/* filelen */
	gint chunk_type;	/* 'WAVE' */

	gint sub_chunk;	/* 'fmt ' */
	gint sc_len;		/* length of sub_chunk, =16 */
	gshort format;		/* should be 1 for PCM-code */
	gshort modus;		/* 1 Mono, 2 Stereo */
	gint sample_fq;	/* frequence of sample */
	gint byte_p_sec;
	gshort byte_p_spl;	/* samplesize; 1 or 2 bytes */
	gshort bit_p_spl;	/* 8, 12 or 16 bit */

	gint data_chunk;	/* 'data' */
	gint data_length;	/* samplecount */
} WaveHeader;



 
void     p_stereo_split16to16(unsigned char *l_left_ptr, 
                  unsigned char *l_right_ptr, unsigned char *l_aud_ptr,
		  long l_data_len);

void     p_dbl_sample_8_to_16(unsigned char insample8,
                     unsigned char *lsb_out,
                     unsigned char *msb_out);

void     p_stereo_split8to16(unsigned char *l_left_ptr,
                             unsigned char *l_right_ptr,
			     unsigned char *l_aud_ptr,
			     long l_data_len);



FILE *p_get_wavparams(const char *inwavname, gint32 *wavsize,
		      gint32 *sound_size, 
		      gint32 *sound_rate, 
		      gint32 *sound_stereo);

#endif		/* end  GAP_ENC_AUDIO_H */
