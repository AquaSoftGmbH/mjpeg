#ifndef _MPEG2CODER_HH
#define _MPEG2CODER_HH


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
#include "mjpeg_types.h"
#include "synchrolib.h"

class MPEG2Encoder;
class EncoderParams;

class MPEG2Coder
{
public:
	MPEG2Coder( MPEG2Encoder &encoder );

	void PutUserData( const uint8_t *userdata, int len);
	void PutGopHdr(int frame, int closed_gop );
	void PutSeqEnd();
	void PutSeqHdr();
private:
	void PutSeqExt();
	void PutSeqDispExt();

	int FrameToTimeCode( int gop_timecode0_frame );
private:
	MPEG2Encoder &encoder;
	EncoderParams &encparams;
};

/* 
 * Local variables:
 *  c-file-style: "stroustrup"
 *  tab-width: 4
 *  indent-tabs-mode: nil
 * End:
 */
#endif
