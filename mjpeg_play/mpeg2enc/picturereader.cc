/* picturereader.cc   */

/* (C) 2000/2001/2003 Andrew Stevens, Rainer Johanni */

/* This software is free software; you can redistribute it
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


#include "picturereader.hh"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "simd.h"
#include "mpeg2encoder.hh"

PictureReader::PictureReader( EncoderParams &_encparams ) :
    encparams( _encparams )
{
    pthread_cond_init( &new_chunk_req, NULL );
    pthread_cond_init( &new_chunk_ack, NULL );
	frames_read = 0;
	last_frame = -1;
    lum_mean = 0;
    istrm_nframes = INT_MAX;
}


/*********************
 *
 * Mark the border so that blocks in the frame are unlikely
 * to match blocks outside the frame.  This is useful for
 * motion estimation routines that, for speed, are a little
 * sloppy about some of the candidates they consider.
 *
 ********************/

static void border_mark( uint8_t *frame,
                         int w1, int h1, int w2, int h2)
{
  int i, j;
  uint8_t *fp;
  uint8_t mask = 0xff;
  /* horizontal pixel replication (right border) */
 
  for (j=0; j<h1; j++)
  {
    fp = frame + j*w2;
    for (i=w1; i<w2; i++)
    {
      fp[i] = mask;
      mask ^= 0xff;
    }
  }
 
  /* vertical pixel replication (bottom border) */

  for (j=h1; j<h2; j++)
  {
    fp = frame + j*w2;
    for (i=0; i<w2; i++)
    {
        fp[i] = mask;
        mask ^= 0xff;
    }
  }
}

void PictureReader::Init()
{
#ifdef PTHREAD_MUTEX_ERRORCHECK
    pthread_mutexattr_t mu_attr;
    pthread_mutexattr_t *p_attr = &mu_attr;
    pthread_mutexattr_settype( &mu_attr, PTHREAD_MUTEX_ERRORCHECK );
    
#else
    pthread_mutexattr_t *p_attr = NULL;		
#endif		
    pthread_mutex_init( &input_imgs_buf_lock, p_attr );
    /* Allocate the frame data buffers: if we're not going to scan ahead
       for GOP size we can save a *lot* of memory... */
    int active_frames = max( encparams.encoding_parallelism, 1);
    int buffering_min = 2*READ_CHUNK_SIZE;
    int B_group_min = READ_CHUNK_SIZE + 
        (active_frames/encparams.M+1)*encparams.M;
    int GOP_lookahead_min  =2*encparams.N_max+READ_CHUNK_SIZE;
    input_imgs_buf_size = max( buffering_min,
                               ( encparams.N_max == encparams.N_min )
                               ? B_group_min
                               : GOP_lookahead_min );

    mjpeg_info( "Buffering %d frames", input_imgs_buf_size );
    input_imgs_buf = new ImagePlanes[input_imgs_buf_size];

	int n,i;
    for(n=0;n<input_imgs_buf_size;n++)
    {
        input_imgs_buf[n] =  new ImagePlaneArray;
        for (i=0; i<3; i++)
        {
            input_imgs_buf[n][i] = 
                static_cast<uint8_t *>( bufalloc( (i==0) 
                                                  ? encparams.lum_buffer_size 
                                                  : encparams.chrom_buffer_size ) 
                    );
        }
        
        border_mark(input_imgs_buf[n][0],
                    encparams.enc_width,encparams.enc_height,
                    encparams.phy_width,encparams.phy_height);
        border_mark(input_imgs_buf[n][1],
                    encparams.enc_chrom_width, encparams.enc_chrom_height,
                    encparams.phy_chrom_width,encparams.phy_chrom_height);
        border_mark(input_imgs_buf[n][2],
                    encparams.enc_chrom_width, encparams.enc_chrom_height,
                    encparams.phy_chrom_width,encparams.phy_chrom_height);
    }
    
    lum_mean = new int[input_imgs_buf_size];

    /*
          Pre-fill the buffer 
        */
    if( encparams.parallel_read )
    {
        StartWorker();
        ReadChunkParallel( input_imgs_buf_size/2 );
    }
    else
    {
        ReadChunkSequential( input_imgs_buf_size/2);
    }

}

PictureReader::~PictureReader()
{
    delete [] lum_mean;
    int n,i;
    for(n=0;n<input_imgs_buf_size;n++)
    {
        for (i=0; i<3; i++)
        {
            free( input_imgs_buf[n][i] );
        }
    }
    delete [] input_imgs_buf;


}

int PictureReader::LumMean( uint8_t *frame )
{
	uint8_t *p;
    int stride = encparams.phy_width;
    int width = encparams.enc_width;
    int height = encparams.enc_height;
	int sum = 0;
    int line;
    uint8_t *line_base = frame;
    for( line = 0; line < height; ++line )
    {
        for( p = line_base; p < line_base+width ; p += 8 )
        {
            sum += (static_cast<int>(p[0]) + static_cast<int>(p[1])) 
                + (static_cast<int>(p[2]) + static_cast<int>(p[3]))
                + (static_cast<int>(p[4]) + static_cast<int>(p[5]))
                + (static_cast<int>(p[6]) + static_cast<int>(p[7]));
            
        }
        line_base += stride;
    }
	return sum / (width * height);
}


void PictureReader::ReadChunk()
{
    int j, e;
   for(j=0;j<READ_CHUNK_SIZE;++j)
   {
	   if( encparams.parallel_read )
	   {
		   // Unlock during the actual I/O filling buffers to allow
		   // the main thread to run if there are still the frames
		   // it needs.  The main thread atomically signals new_chunk_req 
		   // before waiting on new_chunk_ack.
		   // This thread atomically signals new_chunk_ack before
		   // waiting on new_chunk_req.
		   // Thus neither can suspend without first
		   // starting the other.
		   //mjpeg_info( "PRO:  releasing frame buf lock @ %d ", frames_read);

		   e = pthread_mutex_unlock(&input_imgs_buf_lock);
		   if (e != 0)
		      {
		      fprintf(stderr, "*1 pthread_mutex_unlock=%d\n", e);
		      abort();
		      }
	   }
      if( LoadFrame() )
      {
          mjpeg_debug( "End of input stream detected" );
          if  (encparams.parallel_read)
              {
              e = pthread_mutex_lock(&input_imgs_buf_lock);
              if (e != 0) 
		 {
		 fprintf(stderr, "*1 pthread_mutex_lock=%d\n", e);
		 abort();
		 }
              }
          last_frame = frames_read-1;
          istrm_nframes = frames_read;
          mjpeg_info( "Signaling last frame = %d", last_frame );
          if( encparams.parallel_read )
          {
              //mjpeg_info( "PRO: Signaling new_chunk_ack @ %d", frames_read );
              pthread_cond_broadcast( &new_chunk_ack );
          }
          return;
      }

	  if( encparams.parallel_read )
	  {
		  //
		  // Lock to atomically signal the availability of additional
		  // material from a chunk - waking the main thread
		  // if it suspended because a required frame was
		  // unavailable
		  //
		  //mjpeg_info( "PRO:  waiting for frame buf lock @ %d ", frames_read);
              e = pthread_mutex_lock(&input_imgs_buf_lock);
              if (e != 0) 
		 {
		 fprintf(stderr, "*2 pthread_mutex_lock=%d\n", e);
		 abort();
		 }
	  }
	  ++frames_read;

	  if( encparams.parallel_read )
	  {
		  //mjpeg_info( "PRO: Signaling new_chunk_ack @ %d", frames_read );
		  pthread_cond_broadcast( &new_chunk_ack );
	  }

   }

   //
   // When we exit we're holding the lock again so we can
   // ensure we're waiting on new_chunk_req if the main thread is
   // currently running.
   //

}


void *PictureReader::ReadChunksWrapper( void *picread )
{
    static_cast<PictureReader *>(picread)->ReadChunksWorker();
    return NULL;
}

void PictureReader::ReadChunksWorker( )
{
    int e;

	//mjpeg_info( "PRO: requesting frame buf lock" );
    //mjpeg_info( "PRO: has frame buf lock @ %d ", frames_read );
    //mjpeg_info( "PRO: Initial fill of frame buf" );
    e = pthread_mutex_lock( &input_imgs_buf_lock);
    if (e != 0)
        {
	fprintf(stderr, "*3 pthread_mutex_lock=%d\n", e);
	abort();
	}
    ReadChunk();
	for(;;)
	{
		//mjpeg_info( "PRO: has frame buf lock @ %d ", frames_read );
		//mjpeg_info( "PRO: Waiting for new_chunk_req " );
		pthread_cond_wait( &new_chunk_req, &input_imgs_buf_lock );
		//mjpeg_info( "PRO: new_chunk_req regained frame buf lock @  %d ", frames_read ); 
		if( frames_read < istrm_nframes ) 
		{
			ReadChunk();
		}
	}
}


void PictureReader::StartWorker()
{
	pthread_attr_t *pattr = NULL;

#ifdef HAVE_PTHREADSTACKSIZE
#define MINSTACKSIZE 200000
	pthread_attr_t attr;
	size_t stacksize;
	
	pthread_attr_init(&attr);
	pthread_attr_getstacksize(&attr, &stacksize);
	
	if (stacksize < MINSTACKSIZE) {
		pthread_attr_setstacksize(&attr, MINSTACKSIZE);
	}
	
	pattr = &attr;
#endif


	if( pthread_create( &worker_thread, pattr, PictureReader::ReadChunksWrapper, this ) != 0 )
	{
		mjpeg_error_exit1( "worker thread creation failed: %s", strerror(errno) );

	}

}

 /*****************************************************
 *
 *  Read another chunk of frames into the frame buffer if the
 *  specified frame is less than a chunk away from the end of the
 *  buffer.  This version is for when frame input reading is not
 *  multi-threaded and just goes ahead and does it.
 *
 * N.b. if encparams.parallel_read is active then ReadChunk signals/locks
 * which could cause problems hence the assert!
 *
 *****************************************************/
   
void PictureReader::ReadChunkSequential( int num_frame )
{
    while(frames_read - num_frame < READ_CHUNK_SIZE && 
          frames_read < istrm_nframes ) 
    {
        ReadChunk();
    }
}

 /*****************************************************
 *
 * Request read worker thread to read a chunk of frame into the frame
 * buffer if less than a chunk of frames is left after the specified
 * frame.  Wait for acknowledgement of the reading of a chunk (implying
 * at least one extra frame added to the buffer) if the specified frame
 * is not yet in the buffer.
 *
 * N.b. *must* be called with encparams.parallel_read active as otherwise it
 * will thoroughly deadlocked.
 *
 *****************************************************/
   


void PictureReader::ReadChunkParallel( int num_frame)
{
    int e;

	//mjpeg_info( "CON: requesting frame buf lock");
	e = pthread_mutex_lock( &input_imgs_buf_lock);
	if (e != 0)
	   {
	   fprintf(stderr, "*4 pthread_mutex_lock=%d\n", e);
	   abort();
	   }
	for(;;)
	{
		//mjpeg_info( "CON: has frame buf lock @ %d (%d recorded read)", frames_read,  num_frame );
		// Activate reader process "on the fly"
		if( frames_read - num_frame < READ_CHUNK_SIZE && 
			frames_read < istrm_nframes )
		{
			//mjpeg_info( "CON: Running low on frames: signaling new_chunk_req" );

			pthread_cond_broadcast( &new_chunk_req );
		}
		if( frames_read > num_frame  || 
			frames_read >= istrm_nframes )
		{
			//mjpeg_info( "CON:  releasing frame buf lock - enough frames to go on with...");
			e = pthread_mutex_unlock(&input_imgs_buf_lock);
			if (e != 0)
			   {
			   fprintf(stderr, "*4 pthread_mutex_unlock=%d\n", e);
			   abort();
			   }
			return;
		}
		//mjpeg_info( "CON: waiting for new_chunk_ack - too few frames" );
		pthread_cond_wait( &new_chunk_ack, &input_imgs_buf_lock );
		//mjpeg_info( "CON: regained frame buf lock @ %d (%d processed)", frames_read,  num_frame );

	}
	
}

void PictureReader::FillBufferUpto( int num_frame )
{
	if(last_frame>=0 && num_frame>last_frame &&num_frame<istrm_nframes)
	{
		mjpeg_error("Internal:readframe: internal error reading beyond end of frames");
		abort();
	}
	
   /* Read a chunk of frames if we've got less than one chunk buffered
	*/

   if( encparams.parallel_read )
	   ReadChunkParallel( num_frame );
   else
	   ReadChunkSequential( num_frame );

   /* We aren't allowed to go too far behind the last read
	  either... */

   if( num_frame+input_imgs_buf_size < frames_read )
   {
	   mjpeg_error("Internal: buffer flushed too soon req %d buffer %d..%d", num_frame, frames_read-input_imgs_buf_size, frames_read );
	   abort();
   }

	
}

void PictureReader::ReadFrame( int num_frame, uint8_t *frame[] )
{
   int n;
   
   FillBufferUpto( num_frame ); 
   n = num_frame % input_imgs_buf_size;
   frame[0] = input_imgs_buf[n][0];
   frame[1] = input_imgs_buf[n][1];
   frame[2] = input_imgs_buf[n][2];
}


int PictureReader::FrameLumMean( int num_frame )
{
	int n = num_frame;
    //
    // We use this function to probe for the existence of frames
    // so we clip at the end if the end is already found...
    //
	if(  last_frame > 0 && num_frame > last_frame )
	{
		n = last_frame;
	}
	FillBufferUpto( n );
    //
    // We may now know where the last frame is...
    //
    if( last_frame > 0 && n > last_frame )
        n = last_frame;
	return lum_mean[n% input_imgs_buf_size];
}



/* 
 * Local variables:
 *  c-file-style: "stroustrup"
 *  tab-width: 4
 *  indent-tabs-mode: nil
 * End:
 */
