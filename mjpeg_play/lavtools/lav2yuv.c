/* lav2yuv - stream any lav input file to stdout as YUV4MPEG data */

/* Copyright (C) 2000, Rainer Johanni, Andrew Stevens */
/* - added scene change detection code 2001, pHilipp Zabel */
/* - broke some common code out to lav_common.c and lav_common.h,
 *   July 2001, Shawn Sulma <lavtools@athos.cx>.  In doing this, I
 *   repackaged the numerous globals into three structs that get passed into
 *   the relevant functions.  See lav_common.h for a bit more information.
 *   Helpful feedback is welcome.
 */

/*
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
 
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.                
*/

#include "lav_common.h"
#include <stdio.h>

void error(char *text);
void Usage(char *str);
void streamout(void);

int verbose = 1;

EditList el;

void Usage(char *str)
{
   fprintf(stderr,
   "Usage: %s [params] inputfiles\n"
   "   where possible params are:\n"
   "   -m         Force mono-chrome\n"
   "   -S list.el Output a scene list with scene detection\n"
   "   -T num     Set scene detection threshold to num (default: 4)\n"
   "   -D num     Width decimation to use for scene detection (default: 2)\n"
   "   -A w:h     Set output sample aspect ratio\n"
   "              (default:  read from DV files, or guess for MJPEG)\n"
   "   -P w:h     Declare the intended display aspect ratio (used to guess\n"
   "              the sample aspect ratio).  Common values are 4:3 and 16:9.\n"
   "              (default:  read from DV files, or assume 4:3 for MJPEG)\n"
   "   -o num     Frame offset - skip num frames in the beginning\n"
   "              if num is negative, all but the last num frames are skipped\n"
   "   -f num     Only num frames are written to stdout (0 means all frames)\n"
   "   -I num     De-Interlacing mode for DV input (0,1,2,3)\n"
   "   -i num     De-Interlacing spatial threshold (default: 440)\n"
   "   -j num     De-Interlacing temporal threshold (default: 220)\n",
  str);
   exit(0);
}

static int lum_mean;
static int last_lum_mean;
static int delta_lum;
static int scene_num;
static int scene_start;

LavParam param;
uint8_t *frame_buf[3];

void streamout(void)
{

	int framenum, movie_num=0, index[MAX_EDIT_LIST_FILES];
	long int oldframe=N_EL_FRAME(el.frame_list[0])-1;
	FILE *fd=NULL;

	int fd_out = 1; // stdout.
   
	char temp[32];
	
	y4m_stream_info_t streaminfo;
	y4m_frame_info_t frameinfo;

	y4m_init_stream_info(&streaminfo);
	y4m_init_frame_info(&frameinfo);


	if (!param.scenefile)
		writeoutYUV4MPEGheader(fd_out, &param, el, &streaminfo);
	scene_num = scene_start = 0;
	if (param.scenefile)
	{
		int num_files;
		int i;
		
		param.output_width = 
			param.output_width / param.scene_detection_decimation;
		
		//  Output file

		unlink(param.scenefile);
		fd = fopen(param.scenefile,"w");
		if(fd==0)
		{
			mjpeg_error_exit1("Can not open %s - no edit list written!\n",param.scenefile);
		}
		fprintf(fd,"LAV Edit List\n");
		fprintf(fd,"%s\n",el.video_norm=='n'?"NTSC":"PAL");
		for(i=0;i<MAX_EDIT_LIST_FILES;i++) index[i] = -1;
		for(i=0;i<el.video_frames;i++) index[N_EL_FILE(el.frame_list[i])] = 1;
		num_files = 0;
		for(i=0;i<MAX_EDIT_LIST_FILES;i++) 
			if(index[i]==1) 
				index[i] = num_files++;
		fprintf(fd,"%d\n",num_files);
		for(i=0;i<MAX_EDIT_LIST_FILES;i++)
			if(index[i]>=0) 
				fprintf(fd,"%s\n",el.video_file_list[i]);
		sprintf(temp,"%d %ld",
				index[N_EL_FILE(el.frame_list[0])],
				N_EL_FRAME(el.frame_list[0]));
	}

	for (framenum = param.offset; framenum < (param.offset + param.frames); ++framenum) 
	{
	  readframe(framenum, frame_buf, &param, el);
		if (param.scenefile) 
		{
			lum_mean =
				luminance_mean(frame_buf, param.output_width, param.output_height);
			if (framenum == 0)
			{
				delta_lum = 0;
				movie_num = N_EL_FILE(el.frame_list[0]);
			}
			else
				delta_lum = abs(lum_mean - last_lum_mean);
			last_lum_mean = lum_mean;

			mjpeg_debug( "frame %d/%ld, lum_mean %d, delta_lum %d        \n", framenum,
						 el.video_frames, lum_mean, delta_lum);

			if (delta_lum > param.delta_lum_threshold || index[N_EL_FILE(el.frame_list[framenum])] != movie_num ||
				oldframe+1 != N_EL_FRAME(el.frame_list[framenum])) {
				if (delta_lum <= param.delta_lum_threshold)
					fprintf(fd,"%c",'+'); /* Same scene, new file */
				fprintf(fd,"%s %ld\n", temp, N_EL_FRAME(el.frame_list[framenum-1]));
				sprintf(temp,"%d %ld",index[N_EL_FILE(el.frame_list[framenum])], N_EL_FRAME(el.frame_list[framenum]));
				scene_start = framenum;
				scene_num++;
			}

			oldframe = N_EL_FRAME(el.frame_list[framenum]);
			movie_num = N_EL_FILE(el.frame_list[framenum]);

		} 
		else
		{
		  int i;
		  i = y4m_write_frame(fd_out,
				      &streaminfo, &frameinfo, frame_buf);
		  if (i != Y4M_OK)
		    mjpeg_error("Failed to write frame: %s\n", y4m_strerr(i));
		}
	}
	if (param.scenefile)
	{
		fprintf(fd, "%s %ld\n", temp, N_EL_FRAME(el.video_frames-1));
		fclose(fd);
		mjpeg_info( "Editlist written to %s\n", param.scenefile);
	}
   
y4m_fini_frame_info(&frameinfo);
}

int main(argc, argv)
	int argc;
	char *argv[];
{
	int n, nerr = 0;

	param.offset = 0;
	param.frames = 0;
	param.mono = 0;
	param.scenefile = NULL;
	param.DV_deinterlace = 0;
	param.spatial_tolerance = 440;
	param.temporal_tolerance = 220;
	param.default_temporal_tolerance = -1;
	param.delta_lum_threshold = 4;
	param.scene_detection_decimation = 2;
	param.output_width = 0;
	param.output_height = 0;
	param.interlace = -1;
	param.sar = y4m_sar_UNKNOWN;
	param.dar = y4m_dar_4_3;

	while ((n = getopt(argc, argv, "mYv:S:T:D:o:f:I:i:j:P:A:")) != EOF) {
		switch (n) {

		case 'v':
			verbose = atoi(optarg);
			if (verbose < 0 ||verbose > 2) {
				mjpeg_error( "-v option requires arg 0, 1, or 2\n");
				nerr++;
			}
			break;

		case 'I':
			param.DV_deinterlace = atoi(optarg);
			if (param.DV_deinterlace < 0 || param.DV_deinterlace > 3) {
				mjpeg_error( "-I option requires arg 0..3\n");
				nerr++;
			}
			break;

		case 'i':
			param.spatial_tolerance = atoi(optarg);
			if (param.spatial_tolerance < 0 || param.spatial_tolerance > 65025)
			{
				mjpeg_error( "-i option requires a spatial tolerance between 0 and 65025\n");
				nerr++;
			}
			break;

		case 'j':
			param.temporal_tolerance = atoi(optarg);
			if (param.temporal_tolerance < 0 || param.temporal_tolerance > 65025) {
				mjpeg_error( "-i option requires a temporal tolerance between 0 and 65025\n");
				nerr++;
			}
			break;

		case 'm':
			param.mono = 1;
			break;
		case 'S':
			param.scenefile = optarg;
			break;
		case 'T':
			param.delta_lum_threshold = atoi(optarg);
			break;
		case 'D':
			param.scene_detection_decimation = atoi(optarg);
			break;
		case 'o':
			param.offset = atoi(optarg);
			break;
		case 'f':
			param.frames = atoi(optarg);
			break;

		case 'A':
		  if (y4m_parse_ratio(&(param.sar), optarg)) {
		    mjpeg_error("Couldn't parse ratio '%s'\n", optarg);
		    nerr++;
		  }
		  break;
		case 'P':
		  if (y4m_parse_ratio(&(param.dar), optarg)) {
		    mjpeg_error("Couldn't parse ratio '%s'\n", optarg);
		    nerr++;
		  }
			break;
		default:
			nerr++;
		}
	}

	if (optind >= argc)
		nerr++;

	if (nerr)
		Usage(argv[0]);

	(void)mjpeg_default_handler_verbosity(verbose);


	/* Open editlist */

	read_video_files(argv + optind, argc - optind, &el,0);

	param.output_width = el.video_width;
	param.output_height = el.video_height;

	if (param.offset < 0) {
		param.offset = el.video_frames + param.offset;
	}
	if (param.offset >= el.video_frames) {
		mjpeg_error_exit1("offset greater than # of frames in input\n");
	}
	if ((param.offset + param.frames) > el.video_frames) {
		mjpeg_warn("input too short for -f %d\n", param.frames);
		param.frames = el.video_frames - param.offset;
	}
	if (param.frames == 0) {
		param.frames = el.video_frames - param.offset;
	}

	param.interlace = el.video_inter;


	init(&param, frame_buf /*&buffer*/);

#ifdef SUPPORT_READ_DV2
	lav_init_dv_decoder();
#endif
	if (param.delta_lum_threshold != -1) 
	{
		streamout();
	}
	else 
	{
		write_edit_list(param.scenefile, 0, el.video_frames, &el);
	}

	return 0;
}
