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

void error(char *text);
void Usage(char *str);
void streamout(void);

int verbose = 1;

EditList el;

void Usage(char *str)
{
   printf("Usage: %s [params] inputfiles\n", str);
   printf("   where possible params are:\n");
   printf("   -a widthxhight+x+y\n");
   printf("   -s num     Special output format option:\n");
   printf("                 0 output like input, nothing special\n");
   printf
       ("                 1 create half height/width output from interlaced input\n");
   printf
       ("                 2 create 480 wide output from 720 wide input (for SVCD)\n");
   printf
       ("                 3 create 352 wide output from 720 wide input (for vcd)\n");
   printf
       ("                 4 create 480 x 480 output from 720 x 240 input (for svcd)\n");
   printf("   -d num     Drop lsbs of samples [0..3] (default: 0)\n");
   printf("   -n num     Noise filter (low-pass) [0..2] (default: 0)\n");
   printf("   -m         Force mono-chrome\n");
   printf("   -x         Exchange fields for interlaced frames\n");
   printf("   -F         Shift fields (corrects capturing frames wrong field first)\n");
   printf("   -S list.el Output a scene list with scene detection\n");
   printf("   -T num     Set scene detection threshold to num (default: 4)\n");
   printf("   -D num     Width decimation to use for scene detection (default: 2)\n");
   printf("   -o num     Frame offset - skip num frames in the beginning\n");
   printf("              if num is negative, all but the last num frames are skipped\n");
   printf("   -f num     Only num frames are written to stdout (0 means all frames)\n");
   printf("   -I num     De-Interlacing mode for DV input (0,1,2,3)\n");
   printf("   -i num     De-Interlacing spatial threshold (default: 440)\n");
   printf("   -j num     De-Interlacing temporal threshold (default: 220)\n");
   printf("   -Y         switch field order in DV input\n");
   exit(0);
}

static int lum_mean;
static int last_lum_mean;
static int delta_lum;
static int scene_num;
static int scene_start;
static int param_exchange_fields=0;
static int param_shift_fields=0;
static int source_inter;

LavBounds bounds;
LavParam param = { 0, 0, 0, 0, 0, 0, NULL, 0, 0, 440, 220, -1, 4, 2, 0, 0 };
LavBuffers buffer;

void streamout(void)
{

	int framenum, movie_num=0, index[MAX_EDIT_LIST_FILES];
	long int oldframe=N_EL_FRAME(el.frame_list[0])-1;
	FILE *fd=NULL;

	int fd_out = 1; // stdout.
   
	char temp[32];
	uint8_t *frame_buf[3];
	uint8_t *prev_frame_buf[3];
	y4m_frame_info_t frameinfo;

	y4m_init_frame_info(&frameinfo);

	if( param_shift_fields )
	{
		prev_frame_buf[0] = malloc( param.output_width*param.output_height );
		prev_frame_buf[1] = malloc( param.output_width/2*param.output_height/2 );
		prev_frame_buf[2] = malloc( param.output_width/2*param.output_height/2 );
	}

	if (!param.scenefile)
		writeoutYUV4MPEGheader(fd_out, &param, el);
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
		readframe(framenum, frame_buf, &bounds, &param, &buffer, el);
		if (param.scenefile) 
		{
			lum_mean =
				luminance_mean(frame_buf[0], param.output_width, param.output_height);
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
		else if( param_shift_fields )
		{
			int i,j;
			int Y_offset;
			int C_offset;
			int64_t tmp;
			uint8_t **buffer_frame;
			int64_t *cur,*prev,*buf;

			if( framenum == param.offset )
			{
				buffer_frame = frame_buf;
			}
			else
			{
				buffer_frame = prev_frame_buf;
			}

			/* We need to use the field captured 2nd of the previous
			   frame as the corresponding field of our current frame.
			   Since we're going to use the captured 2nd field of the
			   current frame for the next frame we simply swap the captured
			   2nd field with a buffer...
			*/

			if( source_inter == LAV_INTER_BOTTOM_FIRST )
			{
				Y_offset = 0;
				C_offset = 0;
			}
			else				/* LAV_INTER_TOP_FIRST */
			{
				Y_offset = param.output_width;
				C_offset = param.output_width>>1;
			}

			for( j = 0; j < param.output_height; j += 4 )
			{
				/* Swap first row of luma data 4:2:2 video!*/
				cur = (int64_t *)(frame_buf[0]+Y_offset);
				prev = (int64_t *)(prev_frame_buf[0]+Y_offset);
				buf = (int64_t *)(buffer_frame[0]+Y_offset);
				for( i = 0; i < param.output_width/8; i += 2 )
				{
					tmp = cur[i];
					cur[i] = buf[i];
					prev[i] = tmp;
					tmp = cur[i+1];
					cur[i+1] = buf[i+1];
					prev[i+1] = tmp;
				}
				Y_offset += (param.output_width*2);
				
				/* Swap second row of luma data 4:2:2 video!*/
				cur = (int64_t *)(frame_buf[0]+Y_offset);
				prev = (int64_t *)(prev_frame_buf[0]+Y_offset);
				buf = (int64_t *)(buffer_frame[0]+Y_offset);
				for( i = 0; i < param.output_width/8; i +=2 )
				{
					tmp = cur[i];
					cur[i] = buf[i];
					prev[i] = tmp;
					tmp = cur[i+1];
					cur[i+1] = buf[i+1];
					prev[i+1] = tmp;
				}
				Y_offset += (param.output_width*2);

				
				/* Swap row of U data 4:2:2 video!*/
				cur = (int64_t *)(frame_buf[1]+C_offset);
				prev = (int64_t *)(prev_frame_buf[1]+C_offset);
				buf = (int64_t *)(buffer_frame[1]+C_offset);
				for( i = 0; i < param.output_width/(8*2); ++i )
				{
					tmp = cur[i];
					cur[i] = buf[i];
					prev[i] = tmp;
				}

				/* Swap row of V data 4:2:2 video!*/
				cur = (int64_t *)(frame_buf[2]+C_offset);
				prev = (int64_t *)(prev_frame_buf[2]+C_offset);
				buf = (int64_t *)(buffer_frame[2]+C_offset);
				for( i = 0; i < param.output_width/(8*2); ++i )
				{
					tmp = cur[i];
					cur[i] = buf[i];
					prev[i] = tmp;
				}
				
				C_offset += (param.output_width);
			}
			writeoutframeinYUV4MPEG(fd_out, frame_buf, &bounds, &param, &buffer, &frameinfo);


		}
		else
		{
			writeoutframeinYUV4MPEG(fd_out, frame_buf, &bounds, &param, &buffer, &frameinfo);
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
	char *geom;
	char *end;

	memset(&bounds, 0, sizeof(LavBounds));

	while ((n = getopt(argc, argv, "mYv:a:s:d:n:S:T:D:o:f:I:i:j:xF")) != EOF) {
		switch (n) {

		case 'a':
			geom = optarg;
			bounds.active_width = strtol(geom, &end, 10);
			if (*end != 'x' || bounds.active_width < 100) {
				mjpeg_error("Bad width parameter\n");
				nerr++;
				break;
			}

			geom = end + 1;
			bounds.active_height = strtol(geom, &end, 10);
			if ((*end != '+' && *end != '\0') || bounds.active_height < 100) {
				mjpeg_error( "Bad height parameter\n");
				nerr++;
				break;
			}

			if (*end == '\0')
				break;

			geom = end + 1;
			bounds.active_x = strtol(geom, &end, 10);
			if ((*end != '+' && *end != '\0') || bounds.active_x > 720) {
				mjpeg_error( "Bad x parameter\n");
				nerr++;
				break;
			}


			geom = end + 1;
			bounds.active_y = strtol(geom, &end, 10);
			if (*end != '\0' || bounds.active_y > 240) {
				mjpeg_error( "Bad y parameter\n");
				nerr++;
				break;
			}
			break;


		case 's':
			param.special = atoi(optarg);
			if (param.special < 0 || param.special > 4) {
				mjpeg_error( "-s option requires arg 0, 1, 2 or 3\n");
				nerr++;
			}
			break;

		case 'v':
			verbose = atoi(optarg);
			if (verbose < 0 ||verbose > 2) {
				mjpeg_error( "-v option requires arg 0, 1, or 2\n");
				nerr++;
			}
			break;

		case 'd':
			param.drop_lsb = atoi(optarg);
			if (param.drop_lsb < 0 || param.drop_lsb > 3) {
				mjpeg_error( "-d option requires arg 0..3\n");
				nerr++;
			}
			break;

		case 'n':
			param.noise_filt = atoi(optarg);
			if (param.noise_filt < 0 || param.noise_filt > 2) {
				mjpeg_error( "-n option requires arg 0..2\n");
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
			if (param.spatial_tolerance < 0 || param.spatial_tolerance > 65025) {
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

		case 'Y':
			param.YUV_swap_lines = 1;
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
		case 'x':
			param_exchange_fields = 1;
			break;

		case 'F':
			param_shift_fields = 1;
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

	read_video_files(argv + optind, argc - optind, &el);

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

	switch (param.special) {

	case 1:
		if (el.video_inter) {
			param.output_width = (el.video_width / 2) & 0xfffffff0;
			param.output_height = el.video_height / 2;
		} else {
			mjpeg_error(
				"-s 1 may only be set for interlaced video sources\n");
			Usage(argv[0]);
		}
		break;

	case 2:
		if (el.video_width > 700) {
			param.output_width = 480;
		} else {
			mjpeg_error(
				"-s 2 may only be set for 720 pixel wide video sources\n");
			Usage(argv[0]);
		}
		break;

	case 3:
		if (el.video_width > 700) {
			param.output_width = 352;
		} else {
			mjpeg_error(
				"-s 3 may only be set for 720 pixel wide video sources\n");
			Usage(argv[0]);
		}
		break;

	case 4:
		if (el.video_width > 700) {
			param.output_width = 480;
			param.output_height = 480;
		} else {
			mjpeg_error(
				"-s 4 may only be set for 720 pixel wide video sources\n");
			Usage(argv[0]);
		}
	}

	/* Round sizes to multiples of 16 ... */

	//since when does JPEG need to be *16 size? That's only zoran limitation...
	//param.output_width = ((param.output_width + 15) / 16) * 16;
	//param.output_height = ((param.output_height + 15) / 16) * 16;

	if (bounds.active_width == 0) {
		bounds.active_width = param.output_width;
	}

	if (bounds.active_height == 0) {
		bounds.active_height = param.output_height;
	}


	if (bounds.active_width + bounds.active_x > param.output_width) {
		mjpeg_error( "active offset + active width > image size\n");
		Usage(argv[0]);
	}

	if (bounds.active_height + bounds.active_y > param.output_height) {
		mjpeg_error( "active offset + active height > image size\n");
		Usage(argv[0]);
	}

	param.interlace = el.video_inter;
	/* exchange fields if wanted */
	if ( param_exchange_fields || param_shift_fields )
	{
		source_inter = el.video_inter;
		if( param_shift_fields &&
			el.video_inter != LAV_INTER_TOP_FIRST &&
			el.video_inter != LAV_INTER_BOTTOM_FIRST )
		{
            mjpeg_warn("Input not interlaced - cannot shift field order\n");
		}
			
		if( param_exchange_fields )
		{
			el.video_inter ^= (LAV_INTER_BOTTOM_FIRST^LAV_INTER_TOP_FIRST);
		}
		
		if( param_exchange_fields )
		{
			param.interlace ^= (LAV_INTER_BOTTOM_FIRST^LAV_INTER_TOP_FIRST);
		}
		
	}

	init(&bounds, &param, &buffer);

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
