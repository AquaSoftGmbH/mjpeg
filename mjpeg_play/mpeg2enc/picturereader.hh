#ifndef _PICTUREREADER_HH
#define _PICTUREREADER_HH

/* readpic.h Picture reader base class and basic file I/O based reader */
/*  (C) 2000/2001 Andrew Stevens */

/*  This Software is free software; you can redistribute it
 *  and/or modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2 of
 *  the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 */

#include <config.h>
#include <pthread.h>
#include "mjpeg_types.h"
#include "picture.hh"

class EncoderParams;
struct MPEG2EncInVidParams;

class PictureReader
{
public:
	PictureReader(EncoderParams &encoder );
    virtual ~PictureReader();
    void Init();
    void ReadPictureData( int num_frame, uint8_t **frame);
    int FrameLumMean( int num_frame );
    virtual void StreamPictureParams( MPEG2EncInVidParams &strm ) = 0;
    void ReadFrame( int num_frame, uint8_t *frame[] );
    inline int NumberOfFrames() { return istrm_nframes; }
protected:
    int LumMean(uint8_t *frame );
    void ReadChunk();
    static void *ReadChunksWrapper(void *picread);
    void ReadChunksWorker();
    void StartWorker();
    void ReadChunkSequential( int num_frame );
    void ReadChunkParallel( int num_frame );
    void FillBufferUpto( int num_frame ); // Load frame
    virtual bool LoadFrame( ) = 0;
    
protected:
    EncoderParams &encparams;
	pthread_mutex_t input_imgs_buf_lock;

	pthread_cond_t new_chunk_req;
	pthread_cond_t new_chunk_ack;
	pthread_t      worker_thread;
 
   /* NOTE: access to frames_read *must* be read-only in other threads
	  once the chunk-reading worker thread has been started.
   */

    
    int *lum_mean;
	volatile int frames_read;
	int last_frame;
    ImagePlanes *input_imgs_buf;
    int input_imgs_buf_size;
    int istrm_nframes;
};


/* 
 * Local variables:
 *  c-file-style: "stroustrup"
 *  tab-width: 4
 *  indent-tabs-mode: nil
 * End:
 */
#endif

