#ifndef DVPRIVATE_H
#define DVPRIVATE_H

#include "dv.h"
#include <glib.h>
#include <pthread.h>
#include <sys/time.h>

#ifdef HAVE_FIREWIRE
#include <libraw1394/raw1394.h>

typedef struct
{
	raw1394handle_t handle;
	int done;
	unsigned char **frame_buffer;
	long bytes_read;
	long frame_size;
	int output_frame, input_frame;
	int frames;
	int port;
	int channel;

	int crash;
	int still_alive;
	int interrupted;
	int capturing;
	struct timeval delay;
	pthread_t keepalive_tid;

	pthread_t tid;
	pthread_mutex_t *input_lock, *output_lock;
} dv_grabber_t;
#endif

typedef struct
{
	dv_videosegment_t videoseg __attribute__ ((aligned (64)));
	dv_audiosegment_t audioseg;
} dv_t;





#endif
