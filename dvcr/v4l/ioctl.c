/*
    Copyright (C) 2000 Mike Bernson <mike@mlb.org>

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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#include <stdio.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <linux/videodev.h>


main()
{
	FILE	*output;
	output = fopen("V4L.py", "w");

	fprintf(output,"VIDIOCGCAP = 0x%08x\n", VIDIOCGCAP);
	fprintf(output,"VIDIOCGCHAN = 0x%08x\n", VIDIOCGCHAN);
	fprintf(output,"VIDIOCSCHAN = 0x%08x\n", VIDIOCSCHAN);
	fprintf(output,"VIDIOCGTUNER = 0x%08x\n", VIDIOCGTUNER);
	fprintf(output,"VIDIOCSTUNER = 0x%08x\n", VIDIOCGTUNER);
	fprintf(output,"VIDIOCGPICT = 0x%08x\n", VIDIOCGPICT);
	fprintf(output,"VIDIOCSPICT = 0x%08x\n", VIDIOCSPICT);
	fprintf(output,"VIDIOCCCAPTURE = 0x%08x\n", VIDIOCCAPTURE);
	fprintf(output,"VIDIOCGWIN = 0x%08x\n", VIDIOCGWIN);
	fprintf(output,"VIDIOCSWIN = 0x%08x\n", VIDIOCSWIN);
	fprintf(output,"VIDIOCFFBUF = 0x%08x\n", VIDIOCGFBUF);
	fprintf(output,"VIDIOCSFBUF = 0x%08x\n", VIDIOCSFBUF);
	fprintf(output,"VIDIOCKEY = 0x%08x\n", VIDIOCKEY);
	fprintf(output,"VIDIOCGFREQ = 0x%08x\n", VIDIOCGFREQ);
	fprintf(output,"VIDIOCSFREQ = 0x%08x\n", VIDIOCSFREQ);
	fprintf(output,"VIDIOCGAUDIO = 0x%08x\n", VIDIOCGAUDIO);
	fprintf(output,"VIDIOCSAUDIO = 0x%08x\n", VIDIOCSAUDIO);
	fprintf(output,"VIDIOCSYNC = 0x%08x\n", VIDIOCSYNC);
	fprintf(output,"VIDIOCMCAPTURE = 0x%08x\n", VIDIOCMCAPTURE);
	fprintf(output,"VIDIOCGMBUF = 0x%08x\n", VIDIOCGMBUF);
	fprintf(output,"VIDIOCGUNIT = 0x%08x\n", VIDIOCGUNIT);
	fprintf(output,"VIDIOCGCAPTURE = 0x%08x\n", VIDIOCGCAPTURE);
	fprintf(output,"VIDIOCSCAPTURE = 0x%08x\n", VIDIOCSCAPTURE);

	fprintf(output, "\n\n\n");
	fprintf(output, "VID_TYPE_CAPTURE = %d\n", VID_TYPE_CAPTURE);
	fprintf(output, "VID_TYPE_TUNER = %d\n", VID_TYPE_TUNER);
	fprintf(output, "VID_TYPE_TELETEXT = %d\n", VID_TYPE_TELETEXT);
	fprintf(output, "VID_TYPE_OVERLAY = %d\n", VID_TYPE_OVERLAY);
	fprintf(output, "VID_TYPE_CHROMAKEY = %d\n", VID_TYPE_CHROMAKEY);
	fprintf(output, "VID_TYPE_CLIPPING = %d\n", VID_TYPE_CLIPPING);
	fprintf(output, "VID_TYPE_FRAMERAM = %d\n", VID_TYPE_FRAMERAM);
	fprintf(output, "VID_TYPE_SCALES = %d\n", VID_TYPE_SCALES);
	fprintf(output, "VID_TYPE_MONOCHROME = %d\n", VID_TYPE_MONOCHROME);
	fprintf(output, "VID_TYPE_SUBCAPTURE = %d\n", VID_TYPE_SUBCAPTURE);

	fprintf(output, "\n\n\n");
	fprintf(output, "VIDEO_VC_TUNER = %d\n", VIDEO_VC_TUNER);
	fprintf(output, "VIDEO_VC_AUDIO = %d\n", VIDEO_VC_AUDIO);
	fprintf(output, "VIDEO_TYPE_TV = %d\n", VIDEO_TYPE_TV);
	fprintf(output, "VIDEO_TYPE_CAMERA = %d\n", VIDEO_TYPE_CAMERA);

	fprintf(output, "\n\n\n");
	fprintf(output, "VIDEO_TUNER_PAL = %d\n", VIDEO_TUNER_PAL);
	fprintf(output, "VIDEO_TUNER_NTSC = %d\n", VIDEO_TUNER_NTSC);
	fprintf(output, "VIDEO_TUNER_SECAM = %d\n", VIDEO_TUNER_SECAM);
	fprintf(output, "VIDEO_TUNER_LOW = %d\n", VIDEO_TUNER_LOW);
	fprintf(output, "VIDEO_TUNER_NORM = %d\n", VIDEO_TUNER_NORM);
	fprintf(output, "VIDEO_TUNER_STEREO_ON = %d\n", VIDEO_TUNER_STEREO_ON);
	fprintf(output, "VIDEO_TUNER_RDS_ON = %d\n", VIDEO_TUNER_RDS_ON);
	fprintf(output, "VIDEO_TUNER_MBS_ON = %d\n", VIDEO_TUNER_MBS_ON);

	fprintf(output, "\n\n\n");
	fprintf(output, "VIDEO_MODE_PAL = %d\n", VIDEO_MODE_PAL);
	fprintf(output, "VIDEO_MODE_NTSC = %d\n", VIDEO_MODE_NTSC);
	fprintf(output, "VIDEO_MODE_SECAM = %d\n", VIDEO_MODE_SECAM);
	fprintf(output, "VIDEO_MODE_AUTO = %d\n", VIDEO_MODE_AUTO);

	fprintf(output, "\n\n\n");
	fprintf(output, "VIDEO_PALETTE_GREY = %d\n", VIDEO_PALETTE_GREY);
	fprintf(output, "VIDEO_PALETTE_HI240 = %d\n", VIDEO_PALETTE_HI240);
	fprintf(output, "VIDEO_PALETTE_RGB565 = %d\n", VIDEO_PALETTE_RGB565);
	fprintf(output, "VIDEO_PALETTE_RGB24 = %d\n", VIDEO_PALETTE_RGB24);
	fprintf(output, "VIDEO_PALETTE_RGB32 = %d\n", VIDEO_PALETTE_RGB32);
	fprintf(output, "VIDEO_PALETTE_RGB555 = %d\n", VIDEO_PALETTE_RGB555);
	fprintf(output, "VIDEO_PALETTE_YUV422 = %d\n", VIDEO_PALETTE_YUV422);
	fprintf(output, "VIDEO_PALETTE_YUYV = %d\n", VIDEO_PALETTE_YUYV);
	fprintf(output, "VIDEO_PALETTE_UYVY = %d\n", VIDEO_PALETTE_UYVY);
	fprintf(output, "VIDEO_PALETTE_YUV420 = %d\n", VIDEO_PALETTE_YUV420);
	fprintf(output, "VIDEO_PALETTE_YUV411 = %d\n", VIDEO_PALETTE_YUV411);
	fprintf(output, "VIDEO_PALETTE_RAW = %d\n", VIDEO_PALETTE_RAW);
	fprintf(output, "VIDEO_PALETTE_YUV422P = %d\n", VIDEO_PALETTE_YUV422P);
	fprintf(output, "VIDEO_PALETTE_YUV411P = %d\n", VIDEO_PALETTE_YUV411P);
	fprintf(output, "VIDEO_PALETTE_YUV420P = %d\n", VIDEO_PALETTE_YUV420P);
	fprintf(output, "VIDEO_PALETTE_YUV410P = %d\n", VIDEO_PALETTE_YUV410P);
	fprintf(output, "VIDEO_PALETTE_PLANAR = %d\n", VIDEO_PALETTE_PLANAR);
	fprintf(output, "VIDEO_PALETTE_COMPONENT = %d\n", VIDEO_PALETTE_COMPONENT);

	fprintf(output, "\n\n\n");
	fprintf(output, "VIDEO_AUDIO_MUTE = %d\n", VIDEO_AUDIO_MUTE);
	fprintf(output, "VIDEO_AUDIO_MUTABLE = %d\n", VIDEO_AUDIO_MUTABLE);
	fprintf(output, "VIDEO_AUDIO_VOLUME = %d\n", VIDEO_AUDIO_VOLUME);
	fprintf(output, "VIDEO_AUDIO_BASS = %d\n", VIDEO_AUDIO_BASS);
	fprintf(output, "VIDEO_AUDIO_TREBLE = %d\n", VIDEO_AUDIO_TREBLE);

	fprintf(output, "\n\n\n");
	fprintf(output, "VIDEO_SOUND_MONO = %d\n", VIDEO_SOUND_MONO);
	fprintf(output, "VIDEO_SOUND_STEREO = %d\n", VIDEO_SOUND_STEREO);
	fprintf(output, "VIDEO_SOUND_LANG1 = %d\n", VIDEO_SOUND_LANG1);
	fprintf(output, "VIDEO_SOUND_LANG2 = %d\n", VIDEO_SOUND_LANG2);

	fprintf(output, "\n\n\n");
	fprintf(output, "VIDEO_WINDOW_INTERLACE = %d\n", VIDEO_WINDOW_INTERLACE);
	fprintf(output, "VIDEO_CLIP_BITMAP = %d\n", VIDEO_CLIP_BITMAP);
	fprintf(output, "VIDEO_CLIPMAP_SIZE = %d\n", VIDEO_CLIPMAP_SIZE);
	fprintf(output, "VIDEO_CAPTURE_ODD = %d\n", VIDEO_CAPTURE_ODD);
	fprintf(output, "VIDEO_CAPTURE_EVEN = %d\n", VIDEO_CAPTURE_EVEN);
	fprintf(output, "VIDEO_MAX_FRAME = %d\n", VIDEO_MAX_FRAME);
	fprintf(output, "VIDEO_NO_UNIT = %d\n", VIDEO_NO_UNIT);
	fprintf(output, "BASE_VIDIOCPRIVATE = %d\n", BASE_VIDIOCPRIVATE);

	exit(0);
}
