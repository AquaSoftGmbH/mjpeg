#ifndef _SEQENCODER_HH
#define _SEQENCODER_HH


/*  (C) 2000/2001/2005 Andrew Stevens */

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
#include <deque>
#include "mjpeg_types.h"
#include "picture.hh"
#include "streamstate.h"

class MPEG2Encoder;
class EncoderParams;
class MPEG2CodingBuf;
class PictureReader;
class Despatcher;


class SeqEncoder
{
public:
	SeqEncoder( EncoderParams &encparams,
                PictureReader &reader,
                Quantizer &quantizer,
                ElemStrmWriter &writer,
                RateCtl    &ratecontroller
        );
	~SeqEncoder();


    /**********************************
     *
     * Setup ready to start encoding once parameter objects have
     * (where relevant) been Init-ed.
     *
     *********************************/

    void Init();
    

     /**********************************
     *
     * Encode Stream setup during Init
     *
     *********************************/
    void EncodeStream();
        
private:

     /*********************************
     *
     * Manage Picture's - they're expensive-ish to allocate so
     * we maintained a stack already allocated but unused objects.
     *
     *********************************/
     
    Picture *GetPicture();
    void ReleasePicture( Picture *);
      
    /**********************************
     *
     * Pass1EncodeFrame - Pass1 encode a frame.
     * Between zero and many frames may be read (lots of internal
     * look-ahead and buffering).  Internal parallelism via
     * POSIXworker threads.
     * May re-encode frames 
     *
     *********************************/
    void Pass1EncodeFrame();
    
     /**********************************
     *
     * Pass1EncodeFrame - Pass2 encode a frame.
     *
     *
     *********************************/
     void Pass2EncodeFrame();
     
     
     /**********************************
     *
     * Perform Epilogue to encoding video stream
     * Ensure all encoding work still being worked on
     * is completed and flushed.  Collects statistics etc etc.
     *
     *********************************/
   
    void StreamEnd();

	void EncodePicture( Picture *picture );
    
    uint64_t BitsAfterMux() const;
    
    EncoderParams &encparams;
    PictureReader &reader;
    Quantizer &quantizer;
    ElemStrmWriter &writer;
    RateCtl    &ratecontroller;
    
    Despatcher &despatcher;
	
    // Picture's (in decode order) that have been pass1 coded
    // but are not yet ready to be committed for pass 2 coding...
    std::deque<Picture *> pass1coded;

    // Queue of Picture's (in decode order) committed for pass2 encoding
    std::deque<Picture*> pass2queue;
    
    // Picture's allocated but currently not used in encoding
    std::vector<Picture *> free_pictures;
    
    
	// Internal state of encoding...
	StreamState ss;

	Picture *cur_picture, *old_picture;
	Picture *new_ref_picture, *old_ref_picture;
};


/* 
 * Local variables:
 *  c-file-style: "stroustrup"
 *  tab-width: 4
 *  indent-tabs-mode: nil
 * End:
 */
#endif
