
/*************************************************************************
* @mainpage
*  mplex - General-purpose MPEG-1/2 multiplexer.
* (C) 2000, 2001 Andrew Stevens <andrew.stevens@philips.com>
* 
* Doxygen documentation and MPEG Z/Alpha multiplexing part by
* Gernot Ziegler <gz@lysator.liu.se>
*  Constructed using mplex - MPEG-1/SYSTEMS multiplexer as starting point
*  Copyright (C) 1994 1995 Christoph Moar
*  Siemens ZFE ST SN 11 / T SN 6
*
*  This program is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 2 of the License, or
*  (at your option) any later version.					 *
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.	
*
*  You should have received a copy of the GNU General Public License
*  along with this program; if not, write to the Free Software	
*  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*************************************************************************/

#include <config.h>
#include <stdio.h>

#include "mjpeg_types.h"
#include "mjpeg_logging.h"
#include "mpegconsts.h"

#include "interact.hh"
#include "bits.hh"
#include "multiplexor.hh"

/*************************************************************************
    Main
Very high-level. Parses the command line, tries out different modules
on the input files. If all input files were recognized by the modules, 
it initializes the chosen output module. Tells the output module about
the available input files, and makes the first video stream the master
substream in the output file. 

@param argc number of command line parameters
@param argv content of command line parameters
*************************************************************************/


int main (int argc, char* argv[])
{
	MultiplexJob job;
	int     i;
    off_t audio_bytes, video_bytes;
    clockticks first_frame_PTS = 0;

	job.SetFromCmdLine(argc, argv);

	mjpeg_info( "Found %d video streams %d MPEG audio streams %d LPCM audio streams and %d AC3 streams",
				job.video_files.size(),
				job.mpa_files.size(),
				job.lpcm_files.size(),
				job.ac3_files.size());

	Multiplexor mux(job);
	mux.Multiplex();

    return (0);	
}

			
