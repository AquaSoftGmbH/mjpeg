/* lavinfo - give info */
#include "lav_common.h"

int verbose = -1;

EditList el;

LavBounds bounds;
LavParam param = { 0, 0, 0, 0, 0, 0, NULL, 0, 0, 440, 220, -1, 4, 2, 0, 0 };
LavBuffers buffer;

int main(argc, argv)
	int argc;
	char *argv[];
{
	memset(&bounds, 0, sizeof(LavBounds));

   if(argc <=1) {
      printf("Usage: %s file1 [ file2 file3 ... ]\n",argv[0]);
      printf("\ttakes a list of editlist or lav files, and prints out info about them\n");
      printf("\tmust all be of same dimensions\n");
      exit(1);
   }
	(void)mjpeg_default_handler_verbosity(verbose);
	read_video_files(argv+1, argc-1, &el);

   // Video 
   printf("video_frames=%li\n",el.video_frames);
   printf("video_width=%li\n",el.video_width);
   printf("video_height=%li\n",el.video_height);
   printf("video_inter=%li\n",el.video_inter);
	printf("video_norm=%s\n",el.video_norm=='n'?"NTSC":"PAL");
   printf("video_fps=%f\n",el.video_fps);
   printf("video_sar_width=%i\n",el.video_sar_width);
   printf("video_sar_height=%i\n",el.video_sar_height);
   printf("max_frame_size=%li\n",el.max_frame_size);
   printf("MJPG_chroma=%i\n",el.MJPG_chroma);
   // Audio
   printf("has_audio=%i\n",el.has_audio);
   printf("audio_bps=%i\n",el.audio_bps);
   printf("audio_chans=%i\n",el.audio_chans);
   printf("audio_bits=%i\n",el.audio_bits);
   printf("audio_rate=%ld\n",el.audio_rate);
   // Misc
   printf("num_video_files=%li\n",el.num_video_files);
   exit(0);
}
