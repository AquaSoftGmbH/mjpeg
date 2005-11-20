/* (C) 2000, 2001, 2005 Andrew Stevens 
 *  This file is free software; you can redistribute it
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
/*
 * Disclaimer of Warranty
 *
 * These software programs are available to the user without any license fee or
 * royalty on an "as is" basis.  The MPEG Software Simulation Group disclaims
 * any and all warranties, whether express, implied, or statuary, including any
 * implied warranties or merchantability or of fitness for a particular
 * purpose.  In no event shall the copyright-holder be liable for any
 * incidental, punitive, or consequential damages of any kind whatsoever
 * arising from the use of these programs.
 *
 * This disclaimer of warranty extends to the user of these programs and user's
 * customers, employees, agents, transferees, successors, and assigns.
 *
 * The MPEG Software Simulation Group does not represent or warrant that the
 * programs furnished hereunder are free of infringement of any third-party
 * patents.
 *
 * Commercial implementations of MPEG-1 and MPEG-2 video, including shareware,
 * are subject to royalty fees to patent holders.  Many of these patents are
 * general enough such that they are unavoidable regardless of implementation
 * design.
 *
 */

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <cassert>
#include "mjpeg_types.h"
#include "mjpeg_logging.h"
#include "mpeg2syntaxcodes.h"
#include "mpeg2encoder.hh"
#include "elemstrmwriter.hh"
#include "picturereader.hh"
#include "seqencoder.hh"
#include "ratectl.hh"
#include "tables.h"
#include "channel.hh"


// --------------------------------------------------------------------------------
//  Striped Encoding Job parallel despatch classes


struct EncoderJob
{
    EncoderJob() : shutdown( false ) {}
    void (MacroBlock::*encodingFunc)(); 
    Picture *picture;
    unsigned int stripe;
    bool shutdown;
};

class ShutdownJob : public EncoderJob
{
public:
    ShutdownJob()
        {
            shutdown = true;
        }
};



class Despatcher
{
public:
    Despatcher();
    ~Despatcher();
    void Init( unsigned int mb_width,
               unsigned int mb_height,
               unsigned int parallelism );
    void Despatch( Picture *picture, void (MacroBlock::*encodingFunc)() );
    void ParallelWorker();
    void WaitForCompletion();
private:
    static void *ParallelPerformWrapper(void *despatcher);

    unsigned int parallelism;
    unsigned int mb_width;
    unsigned int mb_height;
    vector<unsigned int> stripe_start;
    vector<unsigned int> stripe_length;
    Channel<EncoderJob> jobs;	
    pthread_t *worker_threads;
};

Despatcher::Despatcher() :
    worker_threads(0)
{}

void Despatcher::Init( unsigned int _mb_width,
                       unsigned int _mb_height,
                       unsigned int _parallelism )
{
    parallelism = _parallelism;
    mb_width = _mb_width;
    mb_height = _mb_height;

    if( !parallelism )
        return;

    unsigned int mb_in_stripe = 0;
    int i = 0;
    unsigned int pitch = mb_width / parallelism;
    for( int stripe = 0; stripe < parallelism; ++stripe )
    {
        stripe_start.push_back(mb_in_stripe);
        mb_in_stripe += pitch;
        stripe_length.push_back(pitch);
    }
    stripe_length.back() = mb_width - stripe_start.back();
	pthread_attr_t *pattr = NULL;

	/* For some Unixen we get a ridiculously small default stack size.
	   Hence we need to beef this up if we can.
	*/
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
    worker_threads = new pthread_t[parallelism];
	for(i = 0; i < parallelism; ++i )
	{
        mjpeg_info("Creating worker thread" );
		if( pthread_create( &worker_threads[i], pattr, 
                            &Despatcher::ParallelPerformWrapper,
                            this ) != 0 )
		{
			mjpeg_error_exit1( "worker thread creation failed: %s", strerror(errno) );
		}
	}
}

Despatcher::~Despatcher()
{
    if( worker_threads != 0 )
    {
        WaitForCompletion();
        unsigned int i;
        ShutdownJob shutdownjob;

        for( i = 0; i < parallelism; ++i )
        {
            jobs.Put( shutdownjob );
        }
        for( i = 0; i < parallelism; ++i )
        {
            pthread_join( worker_threads[i], NULL );
        }
        delete [] worker_threads;
    }
}

void *Despatcher::ParallelPerformWrapper(void *despatcher)
{
    static_cast<Despatcher *>(despatcher)->ParallelWorker();
    return 0;
}

void Despatcher::ParallelWorker()
{
	EncoderJob job;
	mjpeg_debug( "Worker thread started" );
    pthread_setcanceltype( PTHREAD_CANCEL_ASYNCHRONOUS, NULL );

	for(;;)
	{
        // Get Job to do and do it!!
        jobs.Get( job );
        if( job.shutdown )
        {
            mjpeg_info ("SHUTDOWN worker" );
            pthread_exit( 0 );
        }
        vector<MacroBlock>::iterator stripe_begin;
        vector<MacroBlock>::iterator stripe_end;
        vector<MacroBlock>::iterator mbi;

        stripe_begin = job.picture->mbinfo.begin() + stripe_start[job.stripe];
        for( int row = 0; row < mb_height; ++row )
        {
            stripe_end = stripe_begin + stripe_length[job.stripe];
            for( mbi = stripe_begin; mbi < stripe_end; ++mbi )
            {
                (*mbi.*job.encodingFunc)();
            }
            stripe_begin += mb_width;
        }

    }
}

void Despatcher::Despatch(  Picture *picture,
                            void (MacroBlock::*encodingFunc)() )
{
    EncoderJob job;
    job.encodingFunc = encodingFunc;
    job.picture = picture;
    for( job.stripe = 0; job.stripe < parallelism; ++job.stripe )
    {
        jobs.Put( job );
    }
}

void Despatcher::WaitForCompletion()
{
    //
    // We know all despatched jobs have completed if the entire
    // pool of worker threads is waiting on the job despatch
    // channel
    //
    jobs.WaitUntilConsumersWaitingAtLeast( parallelism );
}






// --------------------------------------------------------------------------------
//  Sequence Encoder top-level class.
//



SeqEncoder::SeqEncoder( EncoderParams &_encparams,
                        PictureReader &_reader,
                        Quantizer &_quantizer,
                        ElemStrmWriter &_writer,
                        RateCtl    &_ratecontroller ) :
    encparams( _encparams ),
    reader( _reader ),
    quantizer( _quantizer ),
    writer( _writer ),
    ratecontroller( _ratecontroller ),
    despatcher( *new Despatcher ),
    ss( _encparams, _reader ),
    pass1_rcstate( ratecontroller.NewState() )
{
}

SeqEncoder::~SeqEncoder()
{
    delete &despatcher;
}


/*********************
 *
 * Init - Setup encoder once parameters have been set
 *
 ********************/
 
void SeqEncoder::Init()
{

    //
    // Setup the parallel job despatcher...
    //
    despatcher.Init( encparams.mb_width, 
                     encparams.mb_height2, 
                     encparams.encoding_parallelism );

    ratecontroller.InitSeq(false);
    ss.Init(  );
    old_ref_picture = 0;

    // Lots of routines assume (for speed) that
    // a dummy ref picture is provided even if it is not needed
    // because the picture being encoded is  INTRA
    new_ref_picture = GetPicture();
    free_pictures.push_back( new_ref_picture );
}
/*
	Encode a picture from scratch: motion estimate relative to the reference
	frames (if any), encode the resulting macro  block image data
	using the best motion estimate available and buffer the result.
*/

void SeqEncoder::Pass1EncodePicture(Picture *picture)
{
    double prev_sum_avg_act = ratecontroller.SumAvgActivity();
	mjpeg_debug("Start %d %c %d %d",
			   picture->decode, 
			   pict_type_char[picture->pict_type],
			   picture->temp_ref,
			   picture->present);


    pass1_rcstate->Set( ratecontroller.GetState() );

    picture->MotionSubSampledLum();

    EncodePicture(picture);
#ifdef TRACE_ACTIVITY
	mjpeg_info("Frame %5d %5d %c q=%3.2f Sact=(%8.5f->%8.5f) %s [%.0f%%]",
               picture->decode, 
               picture->input,
			   pict_type_char[picture->pict_type],
               picture->AQ,
               prev_sum_avg_act,
               picture->sum_avg_act,
               picture->pad ? "PAD" : "   ",
               picture->IntraCodedBlocks() * 100.0
        );
#else
    mjpeg_info("Frame %5d %5d %c q=%3.2f %s [%.0f%% Intra]",
               picture->decode, 
               picture->input,
               pict_type_char[picture->pict_type],
               picture->AQ,
               picture->pad ? "PAD" : "   ",
               picture->IntraCodedBlocks() * 100.0
        );
#endif
			
}

/*
	Re-Encode a picture after type or parameters have been revised

    N.b. currently motion estimation is *RE-DONE* as it is only
    used for picture type changes (where new ME is needed).


*/

void SeqEncoder::Pass1ReEncodePicture(Picture *picture)
{

	// Flush any previous encoding
	picture->DiscardCoding();
    ratecontroller.SetState( pass1_rcstate->Get() );
    double prev_sum_avg_act = ratecontroller.SumAvgActivity();

	mjpeg_debug("Reencode %d %c %d %d @%8.5f",
			   picture->decode, 
			   pict_type_char[picture->pict_type],
			   picture->temp_ref,
			   picture->present);


    EncodePicture(picture);

#ifdef TRACE_ACTIVITY
    mjpeg_info("Reenc %5d %5d %c q=%3.2f Sact=(%8.5f->%8.5f) %s", 
               picture->decode, 
               picture->input,
               pict_type_char[picture->pict_type],
               picture->AQ,
               prev_sum_avg_act,
               picture->sum_avg_act,
               picture->pad ? "PAD" : "   ");
#else
    mjpeg_info("Reenc %5d %5d %c q=%3.2f %s", 
               picture->decode, 
               picture->input,
               pict_type_char[picture->pict_type],
               picture->AQ,
               picture->pad ? "PAD" : "   ");
#endif

			
}


/*
    Encode a picture based on set picture parameters.

    N.b. this motion estimates as well
    as purely encoding macroblocks

*/

void SeqEncoder::EncodePicture(Picture *picture)
{
    if( picture->pict_struct != FRAME_PICTURE )
        mjpeg_debug("Field %s (%d)",
                   (picture->pict_struct == TOP_FIELD) ? "top" : "bot",
                   picture->pict_struct
            );

    if( encparams.encoding_parallelism > 1 )
    {
        despatcher.Despatch( picture, &MacroBlock::Encode );
        despatcher.WaitForCompletion();
    }
    else
    {
        picture->Encode();
    }

    picture->QuantiseAndEncode(ratecontroller);
    picture->Reconstruct();

    /* Handle second field of a frame that is being field encoded */
    if( encparams.fieldpic )
    {
        picture->Adjust2ndField();
        mjpeg_debug("Field %s (%d)",
                   (picture->pict_struct == TOP_FIELD) ? "top" : "bot",
                   picture->pict_struct
            );

        if( encparams.encoding_parallelism > 1 )
        {
            despatcher.Despatch( picture, &MacroBlock::Encode );
            despatcher.WaitForCompletion();
        }
        else
        {
            picture->Encode();
        }
        picture->QuantiseAndEncode(ratecontroller);
        picture->Reconstruct();
    }
}



/*********************
 *
 * EncodeStream - Where it all happens.  This is the top-level loop
 * that despatches all the encoding work.  Encoding is always performed
 * two-pass. 
 * Pass 1: a first encoding that determines the GOP
 * structure, but may only do rough-and-ready bit allocatino that is 
 * visually sub-optimal and/or may violate the specified maximum bit-rate.
 * 
 * Pass 2: Pictures from Pass1 are, if necessary, re-quantised and the results
 * coded to accurate achieve good bit-allocation and satisfy bit-rate limits.
 * 
 * In 'single-pass mode' pass 2 re-encodes only if it 'has to'.
 * In 'look-ahead mode' pass 2 always re-encodes.
 * In 'Pass 1 of two-pass mode' Pass-2 simply dumps frame complexity and motion
 * estimation data from Pass-1.
 * In 'Pass 2 of two-pass mode' Pass-1 rebuilds frames based on ME and complexity
 * data from  a 'Pass 1 of two-pass mode' run and Pass-2 does some final optimisation.
 *
 * N.b. almost all the interesting stuff occurs in the Pass1 encoding. If selected:
 * - A GOP may be low-passed and re-encoded if it looks like excessive quantisation 
 * is needed. 
 * - GOP length is determined (P frames with mostly intra-coded blocks are turned
 * into I-frames.
 *
 * NOTE: Eventually there will be support for Pass2 to occur in seperate threads...
 * 
 ********************/
 
void SeqEncoder::EncodeStream()
{
    //
    // Repeated calls to TransformFrame build up the queue of
    // Encoded with quantisation controlled by the
    // pass1 rate controller.
    do 
    {
        // If we have Pass2 work to do
        if( pass2queue.size() != 0 )
        {
            Pass2EncodeFrame();
        }
        else if( ss.FrameInStream() < reader.NumberOfFrames() )
        {
            Pass1EncodeFrame();
            ss.Next( BitsAfterMux() );
        }
        else // Run-out at end of stream
        {
            pass2queue.push_back( pass1coded.front() );
            pass1coded.pop_front(); 
        }
    } while( pass2queue.size() > 0 || pass1coded.size() > 0 );

    StreamEnd();
}


Picture *SeqEncoder::GetPicture()
{
    if( free_pictures.size() == 0 )
        return new Picture(encparams,  writer , quantizer);
    else
    {
        Picture *free = free_pictures.back();
        free_pictures.pop_back();
        return free;
    }
}

void SeqEncoder::ReleasePicture( Picture *picture )
{
    reader.ReleaseFrame( picture->input );
    free_pictures.push_back( picture );
}

/*********************
 *
 * Pass1EncodeFrame - Do a unit of work in building up a queue of
 * Pass-1 encoded frame's.
 *
 * A Picture is encoded based on a normal (maximum) length GOP with quantisation
 * determined by Pass1 rate controller.
 * 
 * If the Picture is a P-frame and is almost entirely intra-coded the picture is
 * converted to an I-frame and the current GOP ended early.
 *
 * Once a GOP is succesfully completed its Picture's are transferred to the
 * pass2queue for Pass-2 encoding.
 *
 *********************/
 
  
void SeqEncoder::Pass1EncodeFrame()
{
    old_picture = cur_picture;
    
    if ( ss.b_idx == 0 ) // I or P Frame (First frame in B-group)
    {
        old_ref_picture = new_ref_picture;
        new_ref_picture = cur_picture = GetPicture();
        cur_picture->fwd_org = old_ref_picture->org_img;
        cur_picture->fwd_rec = old_ref_picture->rec_img;
        cur_picture->fwd_ref_frame = old_ref_picture;
        cur_picture->bwd_ref_frame = 0;
    }
    else
    {
        cur_picture = GetPicture();
        cur_picture->fwd_org = old_ref_picture->org_img;
        cur_picture->fwd_rec = old_ref_picture->rec_img;
        cur_picture->bwd_org = new_ref_picture->org_img;
        cur_picture->bwd_rec = new_ref_picture->rec_img;
        cur_picture->fwd_ref_frame = old_ref_picture;
        cur_picture->bwd_ref_frame = new_ref_picture;
    }

    cur_picture->SetEncodingParams(ss, reader.NumberOfFrames() );
    cur_picture->org_img = reader.ReadFrame( cur_picture->input );

    Pass1EncodePicture( cur_picture );

    if( cur_picture->end_seq )
        mjpeg_info( "Sequence end inserted");
 
    pass1coded.push_back( cur_picture );
    
    // Figure out how many pictures can be queued on to pass 2 encoding
    int to_queue = 0;
    int i;

    if( cur_picture->end_seq )
    {
        // If end of sequence we flush everything as next GOP won't refer to this frame
        to_queue = pass1coded.size();
    }
    else if( ss.b_idx == 0  )    // I or P Frame (First frame in B-group)
    {
    
        // Decide if a P frame really should have been an I-frame, and re-encoded
        // as such if necessary
        if( cur_picture->IntraCodedBlocks() > 0.6 && ss.g_idx >= encparams.N_min )
        {
            mjpeg_info( "DEVEL: GOP split point found here... %.0f%% intra coded", 
                        cur_picture->IntraCodedBlocks() * 100.0 );
            ss.ForceIFrame();
            cur_picture->SetEncodingParams(ss, reader.NumberOfFrames());
            Pass1ReEncodePicture( cur_picture );
        }
        // We have a new fwd reference picture: anything decoded before
        // will no longer be referenced and can be passed on.
        for( i = 0; i < pass1coded.size();  ++i )
        {
            if( pass1coded[i] == old_ref_picture) 
                break;
        }
        to_queue = i == pass1coded.size() ? 0 : i;
    }
 

    for( i = 0; i < to_queue; ++i )
    {
        pass2queue.push_back( pass1coded.front() );
        pass1coded.pop_front();
    }
    

}

/*********************
*
*   BitsAfterMux    -   Estimate the size of the multiplexed stream based
*                   on video stream size and estimate overheads for other
*                   components
*
*********************/

uint64_t    SeqEncoder::BitsAfterMux() const
{
    double frame_periods;
    uint64_t bits_after_mux;
    if( encparams.pulldown_32 )
        frame_periods = (double)ss.FrameInStream()*(5.0/4.0);
    else
        frame_periods = (double)ss.FrameInStream();
    //
    //    For VBR we estimate total bits based on actual stream size and
    //    an estimate for the other streams based on time.
    //    For CBR we do *both* based on time to account for padding during
    //    muxing.
    
    if( encparams.quant_floor > 0.0 )       // VBR
        bits_after_mux = 
            writer.BitCount() +  (uint64_t)((frame_periods / encparams.frame_rate) * encparams.nonvid_bit_rate);
    else                                    // CBR
        bits_after_mux = 
            (uint64_t)((frame_periods / encparams.frame_rate) * (encparams.nonvid_bit_rate + encparams.bit_rate));
    return bits_after_mux;
}   

/*********************
 *
 * Pass2EncodeFrame - Take a frame from pass2queue if necessary
 * requantize and re-encode then commit the result.
 *
 *********************/
 
  
void SeqEncoder::Pass2EncodeFrame()
{
    Picture *cur_pass2_picture = pass2queue.front();
    pass2queue.pop_front();
    // Simple single-pass encoding (look-ahead coming soon)
    cur_pass2_picture->CommitCoding();
    ReleasePicture( cur_pass2_picture );
}

void SeqEncoder::StreamEnd()
{
    uint64_t bits_after_mux = BitsAfterMux();
    mjpeg_info( "Guesstimated final muxed size = %lld\n", bits_after_mux/8 );
    
    int i;
    for( i = 0; i < free_pictures.size(); ++i )
    {
        delete free_pictures[i];
    }
}


/* 
 * Local variables:
 *  c-file-style: "stroustrup"
 *  tab-width: 4
 *  indent-tabs-mode: nil
 * End:
 */
