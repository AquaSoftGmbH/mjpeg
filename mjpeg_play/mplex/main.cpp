
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
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#include <string>
#include <memory>
#include <sys/stat.h>
#ifndef _WIN32
#include <sys/param.h>
#endif
#include <ctype.h>
#include <math.h>
#include "mjpeg_types.h"
#include "mjpeg_logging.h"
#include "mpegconsts.h"

#include "interact.hpp"
#include "bits.hpp"
#include "outputstrm.hpp"
#include "multiplexor.hpp"


using std::auto_ptr;


/*************************************************************************
 Command line wrapper.  Basically, all the command line and file (actually
 pipe and FIFO is what would be more normal) I/O specific sub-classes

 Plus the top-level entry point.  That's all!!

*************************************************************************/





class FileOutputStream : public OutputStream
{
public:
    FileOutputStream( const char *filename_pat );
    virtual int  Open( );
    virtual void Close();
    virtual off_t SegmentSize( );
    virtual void NextSegment();
    virtual void Write(uint8_t *data, unsigned int len);

private:
    FILE *strm;
    char filename_pat[MAXPATHLEN];
    char cur_filename[MAXPATHLEN];

};



FileOutputStream::FileOutputStream( const char *name_pat ) 
{
	strncpy( filename_pat, name_pat, MAXPATHLEN );
	snprintf( cur_filename, MAXPATHLEN, filename_pat, segment_num );
}
      
int FileOutputStream::Open()
{
	strm = fopen( cur_filename, "wb" );
	if( strm == NULL )
	{
		mjpeg_error_exit1( "Could not open for writing: %s", cur_filename );
	}

	return 0;
}

void FileOutputStream::Close()
{ 
    fclose(strm);
}


off_t
FileOutputStream::SegmentSize()
{
	struct stat stb;
    fstat(fileno(strm), &stb);
	off_t written = stb.st_size;
    return written;
}

void 
FileOutputStream::NextSegment( )
{
    auto_ptr<char> prev_filename_buf( new char[strlen(cur_filename)+1] );
    char *prev_filename = prev_filename_buf.get();
	fclose(strm);
	++segment_num;
    strcpy( prev_filename, cur_filename );
	snprintf( cur_filename, MAXPATHLEN, filename_pat, segment_num );
	if( strcmp( prev_filename, cur_filename ) == 0 )
	{
		mjpeg_error_exit1( 
			"Need to split output but there appears to be no %%d in the filename pattern %s", filename_pat );
	}
	strm = fopen( cur_filename, "wb" );
	if( strm == NULL )
	{
		mjpeg_error_exit1( "Could not open for writing: %s", cur_filename );
	}
}

void
FileOutputStream::Write( uint8_t *buf, unsigned int len )
{
    if( fwrite( buf, 1, len, strm ) != len )
    {
        mjpeg_error_exit1( "Failed write: %s", cur_filename );
    }
}



/********************************
 *
 * IFileBitStream - Input bit stream class for bit streams sourced
 * from standard file I/O (this of course *includes* network sockets,
 * fifo's, et al).
 *
 * OLAF: To hook into your PES reader/reconstructor you need to define
 * a class like this one, where 'ReadStreamBytes' calls you code to
 * generate the required number of bytes of ES data and transfer it 
 * to the specified buffer.  The logical way to do this would be to
 * inherit IBitStream as a base class of the top-level classes for the ES
 * reconstructors.
 *
 ********************************/

class IFileBitStream : public IBitStream
{
public:
 	IFileBitStream( const char *bs_filename, 
					unsigned int buf_size = BUFFER_SIZE);
	~IFileBitStream();

private:
	FILE *fileh;
	char *filename;
	virtual size_t ReadStreamBytes( uint8_t *buf, size_t number ) 
		{
			return fread(buf,sizeof(uint8_t), number, fileh ); 
		}
	virtual bool EndOfStream() { return feof(fileh) != 0; }
	
};



IFileBitStream::IFileBitStream( const char *bs_filename,
                                unsigned int buf_size) :
    IBitStream()
{
	if ((fileh = fopen(bs_filename, "rb")) == NULL)
	{
		mjpeg_error_exit1( "Unable to open file %s for reading.", bs_filename);
	}
	filename = strcpy( new char[strlen(bs_filename)+1], bs_filename );
    streamname = filename;

    SetBufSize(buf_size);
	eobs = false;
    byteidx = 0;
	if (!ReadIntoBuffer())
	{
		if (buffered==0)
		{
			mjpeg_error_exit1( "Unable to read from %s.", bs_filename);
		}
	}
}


/**
   Destructor: close the device containing the bit stream after a read
   process
*/
IFileBitStream::~IFileBitStream()
{
	if (fileh)
	{
		fclose(fileh);
		delete filename;
	}
	fileh = 0;
    Release();
}


/*******************************
 *
 * Command line job class - sets up a Multiplex Job based on command
 * line and File I/O...
 *
 ******************************/

class CmdLineMultiplexJob : public MultiplexJob
{
public:
	CmdLineMultiplexJob( unsigned int argc, char *argv[]);

private:
	void InputStreamsFromCmdLine (unsigned int argc, char* argv[] );
	void Usage(char *program_name);
	bool ParseVideoOpt( const char *optarg );
	bool ParseLpcmOpt( const char *optarg );
	bool ParseWorkaroundOpt( const char *optarg );
	bool  ParseTimeOffset( const char *optarg );
	

	static const char short_options[];

#if defined(HAVE_GETOPT_LONG)
	static struct option long_options[];
#endif

};

const char CmdLineMultiplexJob::short_options[] =
    "o:b:r:O:v:f:l:s:S:p:W:L:VMh";
#if defined(HAVE_GETOPT_LONG)
struct option CmdLineMultiplexJob::long_options[] = 
{
	{ "verbose",           1, 0, 'v' },
	{ "format",            1, 0, 'f' },
	{ "mux-bitrate",       1, 0, 'r' },
	{ "video-buffer",      1, 0, 'b' },
	{ "lpcm-params",       1, 0, 'L' },
	{ "output",            1, 0, 'o' },
	{ "sync-offset",    	1, 0, 'O' },
	{ "vbr",      	        1, 0, 'V' },
	{ "system-headers",    1, 0, 'h' },
	{ "split-segment",     0, 0, 'M' },
	{ "max-segment-size",  1, 0, 'S' },
	{ "mux-limit",   	    1, 0, 'l' },
	{ "packets-per-pack",  1, 0, 'p' },
	{ "sector-size",       1, 0, 's' },
	{ "workarounds", 1, 0, 'W' },
	{ "help",              0, 0, '?' },
	{ 0,                   0, 0, 0   }
};
#endif


CmdLineMultiplexJob::CmdLineMultiplexJob(unsigned int argc, char *argv[]) :
	MultiplexJob()
{
    int n;
    outfile_pattern = NULL;

#if defined(HAVE_GETOPT_LONG)
	while( (n=getopt_long(argc,argv,short_options,long_options, NULL)) != -1 )
#else
    while( (n=getopt(argc,argv,short_options)) != -1 )
#endif
	{
		switch(n)
		{
        case 0 :
            break;
		case 'o' :
			outfile_pattern = optarg;
			break;
		case 'v' :
			verbose = atoi(optarg);
			if( verbose < 0 || verbose > 2 )
				Usage(argv[0]);
			break;

		case 'V' :
			VBR = true;
			break;
	  
		case 'h' :
			always_system_headers = true;
			break;

		case 'b' :
            if( ! ParseVideoOpt( optarg ) )
            {
                mjpeg_error( "Illegal video decoder buffer size(s): %s", 
                             optarg );
                Usage(argv[0]);
            }
            break;
        case 'L':
            if( ! ParseLpcmOpt( optarg ) )
            {
                mjpeg_error( "Illegal LPCM option(s): %s", optarg );
                Usage(argv[0]);
            }
            break;

		case 'r':
			data_rate = atoi(optarg);
			if( data_rate < 0 )
				Usage(argv[0]);
			/* Convert from kbit/sec (user spec) to 50B/sec units... */
			data_rate = (( data_rate * 1000 / 8 + 49) / 50 ) * 50;
			break;

		case 'O':
			if( ! ParseTimeOffset(optarg) )
			{
				mjpeg_error( "Time offset units if specified must: ms|s|pts" );
				Usage(argv[0]);
			}
			break;
          
		case 'l' : 
 			max_PTS = atoi(optarg);
			if( max_PTS < 1  )
				Usage(argv[0]);
			break;
		
		case 'p' : 
			packets_per_pack = atoi(optarg);
			if( packets_per_pack < 1 || packets_per_pack > 100  )
				Usage(argv[0]);
			break;
		
	  
		case 'f' :
			mux_format = atoi(optarg);
			if( mux_format < MPEG_FORMAT_MPEG1 || mux_format > MPEG_FORMAT_LAST
                )
				Usage(argv[0]);
			break;
		case 's' :
			sector_size = atoi(optarg);
			if( sector_size < 256 || sector_size > 16384 )
				Usage(argv[0]);
			break;
		case 'S' :
			max_segment_size = atoi(optarg);
			if( max_segment_size < 0  )
				Usage(argv[0]);
			break;
		case 'M' :
			multifile_segment = true;
			break;
        case 'W' :
            if( ! ParseWorkaroundOpt( optarg ) )
            {
                Usage(argv[0]);
            }
            break;
		case '?' :
		default :
			Usage(argv[0]);
			break;
		}
	}
	if (argc - optind < 1 || outfile_pattern == NULL)
    {	
		Usage(argv[0]);
    }
	(void)mjpeg_default_handler_verbosity(verbose);
	mjpeg_info( "mplex version %s (%s %s)",VERSION,MPLEX_VER,MPLEX_DATE );

    InputStreamsFromCmdLine( argc-(optind-1), argv+optind-1);
}

/*************************************************************************
 Usage banner for the command line wrapper.
*************************************************************************/

    
void CmdLineMultiplexJob::Usage(char *str)
{
    fprintf( stderr,
	"mjpegtools mplex-2 version " VERSION " (" MPLEX_VER ")\n"
	"Usage: %s [params] -o <output filename pattern> <input file>... \n"
	"         %%d in the output file name is by segment count\n"
	"  where possible params are:\n"
	"--verbose|-v num\n"
    "  Level of verbosity. 0 = quiet, 1 = normal 2 = verbose/debug\n"
	"--format|-f fmt\n"
    "  Set defaults for particular MPEG profiles\n"
	"  [0 = Generic MPEG1, 1 = VCD, 2 = user-rate VCD, 3 = Generic MPEG2,\n"
    "   4 = SVCD, 5 = user-rate SVCD\n"
	"   6 = VCD Stills, 7 = SVCD Stills, 8 = DVD with NAV sectors, 9 = DVD]\n"
    "--mux-bitrate|-r num\n"
    "  Specify data rate of output stream in kbit/sec\n"
	"    (default 0=Compute from source streams)\n"
	"--video-buffer|-b num [, num...] \n"
    "  Specifies decoder buffers size in kB.  [ 20...2000]\n"
    "--lpcm-params | -L samppersec:chan:bits [, samppersec:chan:bits]\n"
	"--mux-limit|-l num\n"
    "  Multiplex only num seconds of material (default 0=multiplex all)\n"
	"--sync-offset|-O num\n"
    "  Specify offset of timestamps (video-audio) in mSec\n"
	"--sector-size|-s num\n"
    "  Specify sector size in bytes for generic formats [256..16384]\n"
    "--vbr|-V\n"
    "  Multiplex variable bit-rate video\n"
	"--packets-per-pack|-p num\n"
    "  Number of packets per pack generic formats [1..100]\n"
	"--system-headers|-h\n"
    "  Create System header in every pack in generic formats\n"
	"--max-segment-size|-S size\n"
    "  Maximum size of output file(s) in Mbyte (default: 0) (no limit)\n"
	"--split-segment|-M\n"
    "  Simply split a sequence across files rather than building run-out/run-in\n"
    "--workaround|-W workaround [, workaround ]\n"
	"--help|-?\n"
    "  Print this lot out!\n", str);
	exit (1);
}


bool CmdLineMultiplexJob::ParseLpcmOpt( const char *optarg )
{
    char *endptr, *startptr;
    unsigned int samples_sec;
    unsigned int channels;
    unsigned int bits_sample;
    endptr = const_cast<char *>(optarg);
    do 
    {
        startptr = endptr;
        samples_sec = static_cast<unsigned int>(strtol(startptr, &endptr, 10));
        if( startptr == endptr || *endptr != ':' )
            return false;


        startptr = endptr+1;
        channels = static_cast<unsigned int>(strtol(startptr, &endptr, 10));
        if(startptr == endptr || *endptr != ':' )
            return false;

        startptr = endptr+1;
        bits_sample = static_cast<unsigned int>(strtol(startptr, &endptr, 10));
        if( startptr == endptr )
            return false;

        LpcmParams *params = LpcmParams::Checked( samples_sec,
                                                  channels,
                                                  bits_sample );
        if( params == 0 )
            return false;
        lpcm_param.push_back(params);
        if( *endptr == ',' )
            ++endptr;
    } while( *endptr != '\0' );
    return true;
}


bool CmdLineMultiplexJob::ParseWorkaroundOpt( const char *optarg )
{
    char *endptr, *startptr;
    endptr = const_cast<char *>(optarg);
    struct { const char *longname; char shortname; bool *flag; } flag_table[] =
        {
            { 0, '\0', 0 }
        };
    for(;;)
    {
        // Find start of next flag...
        while( isspace(*endptr) || *endptr == ',' )
            ++endptr;
        if( *endptr == '\0' )
            break;
        startptr = endptr;
        // Find end of current flag...
        while( *endptr != ' ' && *endptr != ',' && *endptr != '\0' )
            ++endptr;
            
        size_t len = endptr - startptr;

        int flag = 0;
        while( flag_table[flag].longname != 0 )
        {
            if( (len == 1 && *startptr == flag_table[flag].shortname ) ||
                strncmp( startptr, flag_table[flag].longname, len ) == 0 )
            {
                *flag_table[flag].flag = true;
                break;
            }
            ++flag;
        }

        if( flag_table[flag].longname == 0 )
        {
            std::string message( "Illegal work-around option: not one of " );
            flag = 0;
            char sep[] = ",";
            do
            {
                message += flag_table[flag].longname;
                message += sep;
                message += flag_table[flag].shortname;
                ++flag;
                if( flag_table[flag].longname != 0 )
                    message += sep;
            }
            while( flag_table[flag].longname != 0 );
            mjpeg_error( message.c_str() );
            return false;
        }

    } 
    return true;
}

bool CmdLineMultiplexJob::ParseVideoOpt( const char *optarg )
{
    char *endptr, *startptr;
    unsigned int buffer_size;
    endptr = const_cast<char *>(optarg);
    do 
    {
        startptr = endptr;
        buffer_size = static_cast<unsigned int>(strtol(startptr, &endptr, 10));
        if( startptr == endptr )
            return false;

        VideoParams *params = VideoParams::Checked( buffer_size );
        if( params == 0 )
            return false;
        video_param.push_back(params);
        if( *endptr == ',' )
            ++endptr;
    } 
    while( *endptr != '\0' );
    return true;
}

bool CmdLineMultiplexJob::ParseTimeOffset(const char *optarg)
{
    double f;
    double persecond=1000.0;
    char *e;

    f=strtod(optarg,&e);
    if( e ) {
        while(isspace(*e)) e++;
        if(!strcmp(e,"ms")) persecond=1000.0;
        else if(!strcmp(e,"s")) persecond=1.0;
        else if(!strcmp(e,"pts")) persecond=90000.0;
		else
			return false;
    }
    video_offset = lround(f*CLOCKS/(persecond));
	if( video_offset < 0 )
	{
		audio_offset = - video_offset;
		video_offset = 0;
	}
	return true;
}

void CmdLineMultiplexJob::InputStreamsFromCmdLine(unsigned int argc, char* argv[] )
{
	vector<IBitStream *> inputs;
    unsigned int i;
	for( i = 1; i < argc; ++i )
    {
		inputs.push_back( new IFileBitStream( argv[i] ) );
	}
	SetupInputStreams( inputs );
}


int main (int argc, char* argv[])
{
	CmdLineMultiplexJob job(argc,argv);
	FileOutputStream output( job.outfile_pattern );
	Multiplexor mux(job, output);
	mux.Multiplex();

    return (0);	
}

			
