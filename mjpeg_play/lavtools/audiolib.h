#ifndef __AUDIOLIB_H__
#define __AUDIOLIB_H__

#include "mjpeg_types.h"

void audio_shutdown(void);

int audio_init(int a_read, int use_read, int a_stereo, int a_size, int a_rate);
long audio_get_buffer_size(void);
void audio_get_output_status(struct timeval *tmstmp, int *nb_out, int *nb_err);

int audio_read( uint8_t *buf, int size, int swap, 
			    struct timeval *tmstmp, int *status);
int audio_write( uint8_t *buf, int size, int swap);

void audio_start(void);

char *audio_strerror(void);


#endif /* __AUDIOLIB_H__ */
