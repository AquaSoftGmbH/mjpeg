
/*
 *  interact.hpp:  Simple command-line front-end
 *
 *  Copyright (C) 2001 Andrew Stevens <andrew.stevens@philips.com>
 *
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU General Public License
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef __INTERACT_HH__
#define __INTERACT_HH__

#include <config.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <vector>
#include "mjpeg_types.h"
#include "stream_params.hpp"

class IBitStream;

using std::vector;

/*************************************************************************
 *
 * The Multiplexor job Parameters:
 * The various parametes of a multiplexing job: muxing options
 *
 *************************************************************************/

class MultiplexParams
{
public:
  unsigned int data_rate;
  unsigned int packets_per_pack;
  int video_offset;
  int audio_offset;
  unsigned int sector_size;
  bool VBR;
  int mpeg;
  int mux_format;
  bool multifile_segment;
  bool always_system_headers;
  unsigned int max_PTS;
  bool emul_vcdmplex;
  bool stills;
  int verbose;
  int max_timeouts;
  char *outfile_pattern;
  off_t max_segment_size;
};

/***********************************************************************
 *
 * Multiplexor job - paramters plus the streams to mux.
 *
 *
 **********************************************************************/

class MultiplexJob : public MultiplexParams
{
public:
  MultiplexJob();
  void SetFromCmdLine( unsigned int argc, char *argv[]);
private:
  void InputStreamsFromCmdLine (unsigned int argc, char* argv[] );
  static void Usage(char *program_name);
  bool ParseVideoOpt( const char *optarg );
  bool ParseLpcmOpt( const char *optarg );
public:  
  vector<IBitStream *> mpa_files;
  vector<IBitStream *> ac3_files;
  vector<IBitStream *> lpcm_files;
  vector<IBitStream *> video_files;
  vector<IBitStream *> z_alpha_files;
  vector<LpcmParams *> lpcm_param;
  vector<VideoParams *> video_param;
};



/*************************************************************************
    Program ID
*************************************************************************/
 
#define MPLEX_VER    "2.2.2"
#define MPLEX_DATE   "$Date$"


#endif // __INTERACT_H__


/* 
 * Local variables:
 *  c-file-style: "gnu"
 *  tab-width: 8
 *  indent-tabs-mode: nil
 * End:
 */
