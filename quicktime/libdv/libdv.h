#ifndef LIBDV_H
#define LIBDV_H

#ifdef __cplusplus
extern "C" {
#endif

// Color models
#define DV_RGB888 0
#define DV_BGR8880 1
#define DV_RGB8880 2

// Buffer sizes
#define DV_NTSC_SIZE 120000
#define DV_PAL_SIZE 140000

// Norms
#define DV_NTSC 0
#define DV_PAL 1

#include "dvprivate.h"

// ================================== The frame decoder
dv_t* dv_new();
int dv_delete(dv_t* dv);
// Decode a video frame from the data and return nonzero if failure
int dv_read_video(dv_t *dv, 
		unsigned char **rgb_rows, 
		unsigned char *data, 
		long bytes,
		int color_model);
// Decode audio from the data and return the number of samples decoded.
int dv_read_audio(dv_t *dv, 
		unsigned char *samples,
		unsigned char *data,
		long size);

#ifdef HAVE_FIREWIRE

// ================================== The Firewire grabber
dv_grabber_t *dv_grabber_new();
int dv_grabber_delete(dv_grabber_t *dv);
// Spawn a grabber in the background.  The grabber buffers frames number of frames.
int dv_start_grabbing(dv_grabber_t *grabber, int port, int channel, int buffers);
int dv_stop_grabbing(dv_grabber_t* grabber);
int dv_grab_frame(dv_grabber_t* grabber, unsigned char **frame, long *size);
int dv_unlock_frame(dv_grabber_t* grabber);
int dv_grabber_crashed(dv_grabber_t* grabber);
int dv_interrupt_grabber(dv_grabber_t* grabber);

#endif // HAVE_FIREWIRE

#ifdef __cplusplus
}
#endif

#endif
