/*************************************************************************
*  MPEG SYSTEMS MULTIPLEXER                                              *
*  Erzeugen einer MPEG/SYSTEMS                           		 *
*  MULTIPLEXED VIDEO/AUDIO DATEI					 *
*  aus zwei MPEG Basis Streams						 *
*  Christoph Moar							 *
*  SIEMENS ZFE ST SN 11 / T SN 6					 *
*  (C) 1994 1995    							 *
**************************************************************************
*  Generating a MPEG/SYSTEMS						 *
*  MULTIPLEXED VIDEO/AUDIO STREAM					 *
*  from two MPEG source streams						 *
*  Christoph Moar							 *
*  SIEMENS CORPORATE RESEARCH AND DEVELOPMENT ST SN 11 / T SN 6		 *
*  (C) 1994 1995							 *
**************************************************************************
*  Einschraenkungen vorhanden. Unterstuetzt nicht gesamten MPEG/SYSTEMS  *
*  Standard. Haelt sich i.d.R. an den CSPF-Werten, zusaetzlich (noch)    *
*  nur fuer ein Audio- und/oder ein Video- Stream. Evtl. erweiterbar.    *
**************************************************************************
*  Restrictions apply. Will not support the whole MPEG/SYSTEM Standard.  *
*  Basically, will generate Constrained System Parameter Files.		 *
*  Mixes only one audio and/or one video stream. Might be expanded.	 *
*************************************************************************/

/*************************************************************************
*  mplex - MPEG/SYSTEMS multiplexer					 *
*  Copyright (C) 1994 1995 Christoph Moar				 *
*  Siemens ZFE ST SN 11 / T SN 6					 *
*									 *
*  moar@informatik.tu-muenchen.de 					 *
*       (Christoph Moar)			 			 *
*  klee@heaven.zfe.siemens.de						 *
*       (Christian Kleegrewe, Siemens only requests)			 *
*									 *
*  This program is free software; you can redistribute it and/or modify	 *
*  it under the terms of the GNU General Public License as published by	 *	
*  the Free Software Foundation; either version 2 of the License, or	 *
*  (at your option) any later version.					 *
*									 *
*  This program is distributed in the hope that it will be useful,	 *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of	 *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the	 *
*  GNU General Public License for more details.				 *
*									 *
*  You should have received a copy of the GNU General Public License	 *
*  along with this program; if not, write to the Free Software		 *
*  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.		 *
*************************************************************************/

/*************************************************************************
*  Notwendige Systemmittel:						 *
*  Festplattenspeicher fuer Quell- und Zielstreams, pro Minute 		 *
*  MPEG/SYSTEMS Stream noch zusaetzlich 50-100 kByte tmp Plattenspeicher *
**************************************************************************
*  Necessary resources:							 *
*  Hard Disk space for source and destination streams, per Minute	 *
*  MPEG/SYSTEM stream an additional 50-100 kByte tmp Diskspace		 *
*************************************************************************/

#include "main.h"





/*************************************************************************
    Main
*************************************************************************/

#ifdef TIMER
    long total_sec = 0;
    long total_usec = 0;
    long global_sec = 0;
    long global_usec = 0;
    struct timeval  tp_start;
    struct timeval  tp_end;
    struct timeval  tp_global_start;
    struct timeval  tp_global_end;
#endif

int verbose;
int main (argc, argv)

int argc;
char* argv[];
{
    char 	*audio_file = NULL;
    char        *video_file = NULL;
    char        *multi_file = NULL;	

    char	*video_units = NULL;
    char	*audio_units = NULL;
	int     optargs;
    Video_struc video_info;
    Audio_struc audio_info;
    unsigned int audio_bytes, video_bytes;
    unsigned int which_streams=0;
    double	startup_delay=0;
    Vector  vaunits_info, aaunits_info;


    optargs = intro_and_options (argc, argv);
    check_files (argc-optargs, argv+optargs, 
                 &audio_file, &video_file, &multi_file,
		 &audio_bytes, &video_bytes, &which_streams);

    empty_video_struc (&video_info);
    empty_audio_struc (&audio_info);

    if (which_streams & STREAMS_AUDIO) {
	  audio_units=tempnam ("./","tmp_a");
	  get_info_audio (audio_file, audio_units, &audio_info, &startup_delay,
			  audio_bytes, &aaunits_info);
    }

    if (which_streams & STREAMS_VIDEO) {
	  video_units=tempnam ("./","tmp_v");
	  get_info_video (video_file, video_units, &video_info, &startup_delay,
			  video_bytes, &vaunits_info);
    }


    outputstream (video_file, video_units, &video_info,
		  audio_file, audio_units, &audio_info, multi_file, which_streams, vaunits_info, aaunits_info );

    return (0);	
}

			
