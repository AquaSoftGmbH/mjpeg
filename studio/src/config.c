/* Linux Video Studio - a program to capture video using MJPEG-codec boards
 * Copyright (C) 2000-2001 Ronald Bultje
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/extensions/Xv.h>
#include <X11/extensions/Xvlib.h>
#include <glib.h>

#include "parseconfig.h"
#include "studio.h"
#include "pipes.h"
#include "gtkfunctions.h"

/* Variables */
GtkWidget *textfield_timer, *textfield_lapse, *textfield_buffers, *textfield_buffer_size, *textfield_videodev, *textfield_audiodev, *textfield_mixerdev;
GtkWidget *tvplug_port_textfield, *tvplug_width_process_textfield, *tvplug_height_process_textfield;
GtkWidget *tvplug_width_edit_textfield, *tvplug_height_edit_textfield;
GtkWidget *tvplug_width_capture_textfield, *tvplug_height_capture_textfield;
GtkWidget *geom_x_text, *geom_y_text, *geom_w_text, *geom_h_text;
GtkWidget *soft_r_w_text, *soft_r_h_text;
GtkWidget *fileflush_text, *maxfilesize_text;
GtkWidget *soft_r_label, *soft_r_hbox, *decimation_label, *decimation_hbox;
GtkWidget *hbox_lapse, *hbox_timer, *textfield_sd_decimation, *textfield_sd_treshold;
int have_config;
char connector, video_input, *videodev, *audiodev, *mixerdev;

/* Global option objects, *yuck*, but I can't think of a
 * better option. Besides, they are only pointers anyway..
 */
GtkWidget *timelapsemode_checkbutton, *timermode_checkbutton, *screenperscreen_checkbutton, *audioaudio_mute_checkbutton, *audiostereo_checkbutton;
GtkObject *adjustment; //, *audio_adjustment;
char t_vinput, t_output, t_ainput, t_connector;
gint t_sync, t_res, t_bitsize, t_bitrate, t_encoding, t_useread;

/* Forward declarations */
void load_config(void);
void save_config(void);
void change_method(GtkWidget *widget, gpointer data);
void change_format(GtkWidget *widget, gpointer data);
void change_fileformat(GtkWidget *widget, gpointer data);
void change_resolution(GtkWidget *widget, gpointer data);
void change_audio_bitsize(GtkWidget *widget, gpointer data);
void change_audio_bitrate(GtkWidget *widget, gpointer data);
void change_audio_source(GtkWidget *widget, gpointer data);
void change_sync_correction(GtkWidget *widget, gpointer data);
void change_preconfigured_time(GtkWidget *widget, gpointer data);
void change_timelapsemode(GtkWidget *widget, gpointer data);
void accept_options(GtkWidget *widget, gpointer data);
void create_capture_layout(GtkWidget *table);
void create_sound_layout(GtkWidget *table);
void create_advanced_layout(GtkWidget *table);
void create_tvplug_options_layout(GtkWidget *table);
void open_options_window(GtkWidget *widget, gpointer data);

/* ================================================================= */

/* Found this one in bluefish (http://bluefish.openoffice.org/) */
#define DIR_MODE (S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)	/* same as 0755 */
int chk_dir(char *name)
{
	struct stat filestat;

	if ((stat(name, &filestat) == -1) && (errno == ENOENT)) {
		if (verbose) printf("chk_dir, %s does not exist, creating\n", name);
		if (mkdir(name, DIR_MODE) != 0) {
			gtk_show_text_window(STUDIO_ERROR,
				"Unable to create directory config directory \'%s\': %s",
				name, sys_errlist[errno]);
			return 0;
		};
	};
	return 1;
}

void load_config()
{
	char filename[100];
	char *val;
	int  i;

	sprintf(filename,"%s/%s/%s",getenv("HOME"),".studio/", studioconfigfile);
	if (0 == cfg_parse_file(filename))
		have_config = 1;

	if (NULL != (val = cfg_get_str("Studio","default_input_source")))
	{
		input_source = val[0];
		if (val[0] != 'p' && val[0] != 'P' && val[0] != 'N' &&
			val[0] != 'n' && val[0] != 'S' && val[0] != 's' &&
			val[0] != 't' && val[0] != 'T' && val[0] != 'a')
		{
			input_source = 'a';
			printf("Config file error: default_input_source == \'%c\' not allowed\n",
				val[0]);
		}
		else
		if (verbose) printf("default_input_source \'%c\' loaded\n",val[0]);
	}
	else
	{
		input_source = 'a';
		if (verbose) printf("default_input_source - default \'a\' loaded\n");
	}

	if (NULL != (val = cfg_get_str("Studio","default_input_connector")))
	{
		connector = val[0];
		if (val[0] != 's' && val[0] != 'c' && val[0] != 't' && val[0] != 'a')
		{
			connector = 'a';
			printf("Config file error: default_input_connector == \'%c\' not allowed\n",
				val[0]);
		}
		else
			if (verbose) printf("default_input_connector \'%c\' loaded\n",val[0]);
	}
	else
	{
		connector = 'a';
		if (verbose) printf("default_input_connector - default \'a\' loaded\n");
	}

	if (NULL != (val = cfg_get_str("Studio","default_input_format")))
	{
		video_input = val[0];
		if (val[0] != 's' && val[0] != 'p' && val[0] != 'n')
		{
			video_input = 'p';
			printf("Config file error: default_input_format == \'%c\' not allowed\n",
				val[0]);
		}
		else
			if (verbose) printf("default_input_format \'%c\' loaded\n",val[0]);
	}
	else
	{
		video_input = 'p';
		if (verbose) printf("default_input_format - default \'p\' loaded\n");
	}

	if (NULL != (val = cfg_get_str("Studio","default_video_format")))
	{
		video_format = val[0];
		if (val[0] != 'a' && val[0] != 'A' && val[0] != 'q' && val[0] != 'm')
		{
			connector = 'a';
			printf("Config file error: default_video_format == \'%c\' not allowed\n",
				val[0]);
		}
		else
			if (verbose) printf("default_video_format \'%c\' loaded\n", val[0]);
	}
	else
	{
		video_format = 'a';
		if (verbose) printf("default_video_format - default \'a\' loaded\n");
	}

	if (NULL != (val = cfg_get_str("Studio","default_capture_resolution")))
	{
		if (2 != sscanf(val,"%d x %d",&hordcm,&verdcm))
		{
			hordcm = 2;
			verdcm = 2;
			printf("Config file Error: default_capture_resolution == \'%s\' not allowed\n",
				val);
		}
		else
			if (verbose) printf("default_capture_resolution \'%s\' loaded\n", val);
	}
	else
	{
		hordcm = 2;
		verdcm = 2;
		if (verbose) printf("default_capture_resolution - default \'2 x 2\' loaded\n");
	}

	if (-1 != (i = cfg_get_int("Studio","default_capture_quality")))
	{
		if (i >= 0 && i <= 100)
		{
			if (verbose) printf("default_capture_quality \'%d\' loaded\n", i);
			quality = i;
		}
		else
		{
			printf("Config file Error: default_capture_quality == %d not allowed\n",
				i);
			quality = 50;
		}
	}
	else
	{
		quality = 50;
		if (verbose) printf("default_capture_quality - default 50 loaded\n");
	}

	if (-1 != (i = cfg_get_int("Studio","default_audio_bitsize")))
	{
		if (i == 0 || i == 8 || i == 16)
		{
			audio_size = i;
			if (verbose) printf("default_audio_bitsize \'%d\' loaded\n", i);
		}
		else
		{
			printf("Config file Error: default_audio_bitsize == %d not allowed\n",
				i);
			audio_size = 16;
		}
	}
	else
	{
		audio_size = 16;
		if (verbose) printf("default_audio_bitsize - default 16 loaded\n");
	}

	if (-1 != (i = cfg_get_int("Studio","default_audio_bitrate")))
	{
		if (i == 11025 || i == 22050 || i == 44100)
		{
			audio_rate = i;
			if (verbose) printf("default_audio_bitrate \'%d\' loaded\n", i);
		}
		else
		{
			printf("Config file Error: default_audio_bitrate == %d not allowed\n",
				i);
			audio_rate = 22050;
		}
	}
	else
	{
		audio_rate = 22050;
		if (verbose) printf("default_audio_bitrate - default 22050 loaded\n");
	}

	if (NULL != (val = cfg_get_str("Studio","default_audio_source")))
	{
		audio_recsrc = val[0];
		if (val[0] != 'l' && val[0] != 'm' && val[0] != 'c')
		{
			printf("Config file Error: default_audio_source == \'%c\' not allowed\n",
				val[0]);
			audio_recsrc = 'l';
		}
		else
			if (verbose) printf("default_audio_source \'%c\' loaded\n", val[0]);
	}
	else
	{
		audio_recsrc = 'l';
		if (verbose) printf("default_audio_source - default \'l\' loaded\n");
	}

	if (-1 != (i = cfg_get_int("Studio","default_sync_correction")))
	{
		if (i == 0 || i == 1 || i == 2)
		{
			sync_corr = i;
			if (verbose) printf("default_sync_correction \'%d\' loaded\n", i);
		}
		else
		{
			printf("Config file Error: default_sync_correction == %d not allowed\n",
				i);
			sync_corr = 2;
		}
	}
	else
	{
		sync_corr = 2;
		if (verbose) printf("default_sync_correction - default 2 loaded\n");
	}

	if (-1 != (i = cfg_get_int("Studio","default_file_flush")))
	{
		file_flush = i;
		if (verbose) printf("default_file_flush \'%d\' loaded\n", i);
	}
	else
	{
		file_flush = 0;
		if (verbose) printf("default_file_flush - default 0 loaded\n");
	}

	if (-1 != (i = cfg_get_int("Studio","default_max_file_size")))
	{
		max_file_size = i;
		if (verbose) printf("default_max_file_size \'%d\' loaded\n", i);
	}
	else
	{
		max_file_size = 0;
		if (verbose) printf("default_max_file_size - default 0 loaded\n");
	}

	if (-1 != (i = cfg_get_int("Studio","default_software_encoding")))
	{
		if (i==0 || i==1)
		{
			software_encoding = i;
			if (verbose) printf("default_software_encoding \'%d\' loaded\n", i);
		}
		else
		{
			printf("Config file Error: default_software_encoding == \'%d\' not allowed\n",
				i);
			software_encoding = 0;
		}
	}
	else
	{
		software_encoding = 0;
		if (verbose) printf("default_software_encoding - default 0 loaded\n");
	}

	if (-1 != (i = cfg_get_int("Studio","default_use_read")))
	{
		if (i==0 || i==1)
		{
			use_read = i;
			if (verbose) printf("default_use_read \'%d\' loaded\n", i);
		}
		else
		{
			printf("Config file Error: default_use_read == \'%d\' not allowed\n",
				i);
			use_read = 0;
		}
	}
	else
	{
		use_read = 0;
		if (verbose) printf("default_use_read - default 0 loaded\n");
	}

/*	if (-1 != (i = cfg_get_int("Studio","default_audio_volume")))
	{
		if (i >= 0 && i <= 100)
		{
			audio_lev = i;
			if (verbose) printf("default audio volume \'%d\' loaded\n", i);
		}
		else
		{
			if (verbose) printf("Config file Error: default_audio_volume\n");
			audio_lev = 50;
		}
	}
	else
	{
		audio_lev = 50;
		if (verbose) printf("default audio volume - none loaded\n");
	}*/

	if (-1 != (i = cfg_get_int("Studio","default_stereo")))
	{
		if (i == 0 || i == 1)
		{
			if (verbose) printf("default_stereo \'%d\' loaded\n", i);
			stereo = i;
		}
		else
		{
			printf("Config file Error: default_stereo == %d not allowerd\n", i);
			stereo = 0;
		}
	}
	else
	{
		stereo = 0;
		if (verbose) printf("default_stereo - default 0 loaded\n");
	}

	if (-1 != (i = cfg_get_int("Studio","default_audio_mute")))
	{
		if (i == 0 || i == 1)
		{
			audio_mute = i;
			if (verbose) printf("default_audio_mute \'%d\' loaded\n", i);
		}
		else
		{
			printf("Config file Error: default_audio_mute == %d not allowerd\n", i);
			audio_mute = 0;
		}
	}
	else
	{
		audio_mute = 0;
		if (verbose) printf("default_audio_mute - default 0 loaded\n");
	}

	if (-1 != (i = cfg_get_int("Studio","default_port")))
	{
		port = i;
		if (verbose) printf("default Xvideo port \'%d\' loaded\n", i);
	}
	else
	{
		port = 0;
		if (verbose) printf("default Xvideo port - default 0 loaded\n");
	}

	if (NULL != (val = cfg_get_str("Studio","default_tv_c_size")))
	{
		if (2 != sscanf(val,"%d x %d",&tv_width_capture,&tv_height_capture))
		{
			printf("Config file Error: default_tv_c_size == \'%s\' not allowerd\n", val);
			tv_width_capture = 288;
			tv_height_capture = 214;
		}
		else
			if (verbose) printf("default_tv_c_size \'%s\' loaded\n", val);
	}
	else
	{
		tv_width_capture = 288;
		tv_height_capture = 214;
	}

	if (NULL != (val = cfg_get_str("Studio","default_tv_p_size")))
	{
		if (2 != sscanf(val,"%d x %d",&tv_width_process,&tv_height_process))
		{
			printf("Config file Error: default_tv_p_size == \'%s\' not allowerd\n", val);
			tv_width_process = 288;
			tv_height_process = 214;
		}
		else
			if (verbose) printf("default_tv_p_size \'%s\' loaded\n", val);
	}
	else
	{
		tv_width_process = 288;
		tv_height_process = 214;
	}

	if (NULL != (val = cfg_get_str("Studio","default_tv_e_size")))
	{
		if (2 != sscanf(val,"%d x %d",&tv_width_edit,&tv_height_edit))
		{
			printf("Config file Error: default_tv_e_size == \'%s\' not allowerd\n", val);
			tv_width_edit = 288;
			tv_height_edit = 214;
		}
		else
			if (verbose) printf("default_tv_e_size \'%s\' loaded\n", val);
	}
	else
	{
		tv_width_edit = 288;
		tv_height_edit = 214;
	}

	if (NULL != (val = cfg_get_str("Studio","default_recording_size")))
	{
		if (4 != sscanf(val,"%d x %d + %d + %d",&geom_width, &geom_height, &geom_x, &geom_y))
		{
			printf("Config file Error: default_recording_size == \'%s\' not allowed\n",
				val);
			geom_width = 0;
			geom_height = 0;
			geom_x = 0;
			geom_y = 0;
		}
		else
			if (verbose) printf("default_recording_size \'%s\' loaded\n", val);
	}
	else
	{
		geom_width = 0;
		geom_height = 0;
		geom_x = 0;
		geom_y = 0;
	}

	if (-1 != (i = cfg_get_int("Studio","default_screenperscreenmode")))
	{
		if (i == 0 || i == 1)
		{
			single_frame = i;
			if (i == 1)
				wait_for_start = 0;
			else
				wait_for_start = 1;
			if (verbose) printf("default_screenperscreenmode \'%d\' loaded\n", i);
		}
		else
		{
			printf("Config file Error: default_screenperscreenmode == %d not allowed\n",
				i);
			single_frame = 0;
			wait_for_start = 1;
		}
	}
	else
	{
		single_frame = 0;
		wait_for_start = 1;
		if (verbose) printf("default_screenperscreenmode - default 0 loaded\n");
	}

	if (-1 != (i = cfg_get_int("Studio","default_time")))
		record_time = i;
	else
		record_time = 100000;

	if (-1 != (i = cfg_get_int("Studio","default_lapse")))
		time_lapse = i;
	else
		time_lapse = 1;

	if (-1 != (i = cfg_get_int("Studio","default_mjpeg_buffers")))
		MJPG_nbufs = i;
	else
		MJPG_nbufs = 32;

	if (-1 != (i = cfg_get_int("Studio","default_mjpeg_buffer_size")))
		MJPG_bufsize = i;
	else
		MJPG_bufsize = 256;

	if (-1 != (i = cfg_get_int("Studio","default_scene_detection_treshold")))
		scene_detection_treshold = i;
	else
		scene_detection_treshold = 10;

	if (NULL != (val = cfg_get_str("Studio","default_software_recording_size")))
	{
		if (sscanf(val, "%d x %d", &software_recwidth, &software_recheight) == 2)
		{
			if (verbose) printf("default_software_recording_size - \'%s\' loaded\n",
				val);
		}
		else
		{
			software_recwidth = 160;
			software_recheight = 120;
			printf("Config file error: default_software_recording_size \'%s\' not allowed\n",
				val);
		}
	}
	else
	{
		software_recwidth = 160;
		software_recheight = 120;
		if (verbose) printf("default_software_recording_size - default %d x %d loaded\n",
			software_recwidth, software_recheight);
	}

	if (-1 != (i = cfg_get_int("Studio","default_scene_detection_decimation")))
		scene_detection_width_decimation = i;
	else
		scene_detection_width_decimation = 2;

	if (NULL != (val = cfg_get_str("Studio","default_video_dev")))
	{
		if (val[0] == '/')
			videodev = val;
		else
		{
			printf("Config file Error: default_video_dev == \'%s\' not allowed\n", val);
			videodev = DEFAULT_VIDEO;
		}
	}
	else
		videodev = DEFAULT_VIDEO;
	setenv("LAV_VIDEO_DEV", videodev, 1);

	if (NULL != (val = cfg_get_str("Studio","default_audio_dev")))
	{
		if (val[0] == '/')
			audiodev = val;
		else
		{
			printf("Config file Error: default_audio_dev == \'%s\' not allowed\n", val);
			audiodev = DEFAULT_AUDIO;
		}
	}
	else
		audiodev = DEFAULT_AUDIO;
	setenv("LAV_AUDIO_DEV", audiodev, 1);

	if (NULL != (val = cfg_get_str("Studio","default_mixer_dev")))
	{
		if (val[0] == '/')
			mixerdev = val;
		else
		{
			printf("Config file Error: default_mixer_dev == \'%s\' not allowed\n", val);
			mixerdev = DEFAULT_MIXER;
		}
	}
	else
		mixerdev = DEFAULT_MIXER;
	setenv("LAV_MIXER_DEV", mixerdev, 1);

	if (NULL != (val = cfg_get_str("Studio","default_record_dir")))
	{
		record_dir = val;
		if (verbose) printf("default recording dir \'%s\' loaded\n", record_dir);
	}
	else
	{
		char temp[256];
		sprintf(temp, "%s/%s", getenv("HOME"), "video%03d.avi");
		record_dir = temp;
	}

	load_app_locations();
}

void save_config()
{
	char filename[100], directory[100];
	FILE *fp;

	sprintf(filename,"%s/%s/%s",getenv("HOME"),".studio", studioconfigfile);
	sprintf(directory,"%s/%s",getenv("HOME"),".studio");
	if (chk_dir(directory) == 0)
	{
		fprintf(stderr,"can't open config file %s\n",filename);
		return;
	}
	unlink(filename);

	/* write new one... */
	fp = fopen(filename,"w");
	if (NULL == fp)
	{
		gtk_show_text_window(STUDIO_ERROR,
			"Can't open config file \'%s\': %s",
			filename, sys_errlist[errno]);
		return;
	}

	fprintf(fp,"[Studio]\n");
	fprintf(fp,"default_input_source = %c\n", input_source);
	fprintf(fp,"default_input_connector = %c\n", connector);
	fprintf(fp,"default_input_format = %c\n", video_input);
	fprintf(fp,"default_video_format = %c\n", video_format);
	fprintf(fp,"default_capture_resolution = %d x %d\n", hordcm, verdcm);
	fprintf(fp,"default_capture_quality = %d\n", quality);
	fprintf(fp,"default_audio_bitsize = %d\n", audio_size);
	fprintf(fp,"default_audio_bitrate = %d\n", audio_rate);
	fprintf(fp,"default_audio_source = %c\n", (char)audio_recsrc);
	//fprintf(fp,"default_audio_volume = %d\n", audio_lev);
	fprintf(fp,"default_sync_correction = %d\n", sync_corr);
	fprintf(fp,"default_stereo = %d\n", stereo);
	fprintf(fp,"default_audio_mute = %d\n", audio_mute);
	fprintf(fp,"default_port = %d\n", port);
	fprintf(fp,"default_tv_c_size = %d x %d\n", tv_width_capture, tv_height_capture);
	fprintf(fp,"default_tv_p_size = %d x %d\n", tv_width_process, tv_height_process);
	fprintf(fp,"default_tv_e_size = %d x %d\n", tv_width_edit, tv_height_edit);
	fprintf(fp,"default_recording_size = %d x %d + %d + %d\n", geom_width, geom_height, geom_x, geom_y);
	fprintf(fp,"default_software_recording_size = %d x %d\n", software_recwidth, software_recheight);
	fprintf(fp,"default_screenperscreenmode = %d\n", single_frame);
	fprintf(fp,"default_time = %d\n", record_time);
	fprintf(fp,"default_lapse = %d\n", time_lapse);
	fprintf(fp,"default_file_flush = %d\n", file_flush);
	fprintf(fp,"default_max_file_size = %d\n", max_file_size);
	fprintf(fp,"default_software_encoding = %d\n", software_encoding);
	fprintf(fp,"default_use_read = %d\n", use_read);
	fprintf(fp,"default_mjpeg_buffers = %d\n", MJPG_nbufs);
	fprintf(fp,"default_mjpeg_buffer_size = %d\n", MJPG_bufsize);
	fprintf(fp,"default_video_dev = %s\n", videodev);
	fprintf(fp,"default_audio_dev = %s\n", audiodev);
	fprintf(fp,"default_mixer_dev = %s\n", mixerdev);
	fprintf(fp,"default_record_dir = %s\n", record_dir);
	fprintf(fp,"default_scene_detection_treshold = %d\n", scene_detection_treshold);
	fprintf(fp,"default_scene_detection_decimation = %d\n", scene_detection_width_decimation);

	save_app_locations(fp);

	if (verbose) printf("Configuration saved\n");

	fclose(fp);

        /* Added by Bernhard, to get the encoding options saved when exiting */
        if (saveonexit != 0)
          save_config_encode();
}

void change_method(GtkWidget *widget, gpointer data)
{
	if ((char *) data == "Super-VHS")
		t_connector = 's';
	else if ((char *) data == "Composite")
		t_connector = 'c';
	else if ((char *) data == "TV")
		t_connector = 't';
	else if ((char *) data == "Auto")
		t_connector = 'a';
}

void change_format(GtkWidget *widget, gpointer data)
{
	if ((char *) data == "NTSC")
		t_vinput = 'n';
	else if ((char *) data == "PAL")
		t_vinput = 'p';
	else if ((char *) data == "SECAM")
		t_vinput = 's';
} 

void change_fileformat(GtkWidget *widget, gpointer data)
{
	if ((char *) data == "AVI")
		t_output = 'a';
	else if ((char *) data == "AVI (f.e.)")
		t_output = 'A';
	else if ((char *) data == "Movtar")
		t_output = 'm';
	else if ((char *) data == "Quicktime")
		t_output = 'q';
}

void change_resolution(GtkWidget *widget, gpointer data)
{
	t_res = (int)data;
}

static void change_encodingtype(GtkWidget *widget, gpointer data)
{
	t_encoding = (int)data;
	if (t_encoding)
	{
		gtk_widget_hide(decimation_label);
		gtk_widget_hide(decimation_hbox);

		gtk_widget_show(soft_r_hbox);
		gtk_widget_show(soft_r_label);
	}
	else
	{
		gtk_widget_hide(soft_r_label);
		gtk_widget_hide(soft_r_hbox);

		gtk_widget_show(decimation_hbox);
		gtk_widget_show(decimation_label);
	}
}

static void change_audioreadmethod(GtkWidget *widget, gpointer data)
{
	t_useread = (int)data;
}

void change_audio_bitsize(GtkWidget *widget, gpointer data)
{
	t_bitsize = (int)data;
}

void change_audio_bitrate(GtkWidget *widget, gpointer data)
{
	t_bitrate = (int)data;
}

void change_audio_source(GtkWidget *widget, gpointer data)
{
	if ((char *) data == "Line-in")
		t_ainput = 'l';
	else if ((char *) data == "Microphone")
		t_ainput = 'm';
	else if ((char *) data == "CD-ROM")
		t_ainput = 'c';
}

void change_sync_correction(GtkWidget *widget, gpointer data)
{
	t_sync = (int)data;
}

void change_preconfigured_time(GtkWidget *widget, gpointer data)
{
	if (GTK_TOGGLE_BUTTON (widget)->active) 
	{
		gtk_widget_show(hbox_timer);
	}
	else
	{
		gtk_widget_hide(hbox_timer);
	}
}

void change_timelapsemode(GtkWidget *widget, gpointer data)
{
	if (GTK_TOGGLE_BUTTON (widget)->active) 
	{
		gtk_widget_show(hbox_lapse);
	}
	else
	{
		gtk_widget_hide(hbox_lapse);
	}
}

void accept_options(GtkWidget *widget, gpointer data)
{
	int tv_changed = 0, audio_changed = 0;

	/* time lapse mode config */
	quality = GTK_ADJUSTMENT (adjustment)->value;
/*	audio_lev = GTK_ADJUSTMENT (audio_adjustment)->value;*/
	stereo = GTK_TOGGLE_BUTTON (audiostereo_checkbutton)->active;
	audio_mute = GTK_TOGGLE_BUTTON (audioaudio_mute_checkbutton)->active;
	single_frame = GTK_TOGGLE_BUTTON (screenperscreen_checkbutton)->active;
	if (GTK_TOGGLE_BUTTON (screenperscreen_checkbutton)->active)
		wait_for_start = 0;
	else
		wait_for_start = 1;
	if(GTK_TOGGLE_BUTTON (timelapsemode_checkbutton)->active)
		time_lapse = atoi(gtk_entry_get_text(GTK_ENTRY(textfield_lapse)));
	else
		time_lapse = 1;
	if(GTK_TOGGLE_BUTTON (timermode_checkbutton)->active)
		record_time = atoi(gtk_entry_get_text(GTK_ENTRY(textfield_timer)));
	else
		record_time = 100000;

	software_recwidth = atoi(gtk_entry_get_text(GTK_ENTRY(soft_r_w_text)));
	software_recheight = atoi(gtk_entry_get_text(GTK_ENTRY(soft_r_h_text)));

	connector = t_connector; /* auto/tc/composite/svideo */
	video_input = t_vinput; /* pal/secam/ntsc */
	sync_corr = t_sync;
	if (t_connector == 'a')
		input_source = 'a';
	else if (t_connector == 't')
	{
		if (t_vinput == 'p' || t_vinput == 's')
			input_source = 't';
		else
			input_source = 'T';
	}
	else if (t_connector == 'c')
	{
		if (t_vinput == 'p')
			input_source = 'p';
		else if (t_vinput == 'n')
			input_source = 'n';
		else if (t_vinput == 's')
			input_source = 's';
	}
	else
	{
		if (t_vinput == 'p')
			input_source = 'P';
		else if (t_vinput == 'n')
			input_source = 'N';
		else if (t_vinput == 's')
			input_source = 'S';
	}
	if (audio_recsrc != t_ainput)
		audio_changed = 1;
	audio_recsrc = t_ainput;
	video_format = t_output;
	audio_size = t_bitsize;
	audio_rate = t_bitrate;
	hordcm = t_res;
	verdcm = t_res;
	MJPG_nbufs = atoi(gtk_entry_get_text(GTK_ENTRY(textfield_buffers)));
	MJPG_bufsize = atoi(gtk_entry_get_text(GTK_ENTRY(textfield_buffer_size)));
	software_encoding = t_encoding;
	use_read = t_useread;
	videodev = gtk_entry_get_text(GTK_ENTRY(textfield_videodev));
	audiodev = gtk_entry_get_text(GTK_ENTRY(textfield_audiodev));
	mixerdev = gtk_entry_get_text(GTK_ENTRY(textfield_mixerdev));
	setenv("LAV_VIDEO_DEV", videodev, 1);
	setenv("LAV_AUDIO_DEV", audiodev, 1);
	setenv("LAV_MIXER_DEV", mixerdev, 1);
	if (atoi(gtk_entry_get_text(GTK_ENTRY(tvplug_port_textfield))) != port)
		tv_changed = 1;
	if (atoi(gtk_entry_get_text(GTK_ENTRY(tvplug_height_capture_textfield))) != tv_height_capture)
		tv_changed = 1;
	if (atoi(gtk_entry_get_text(GTK_ENTRY(tvplug_width_capture_textfield))) != tv_width_capture)
		tv_changed = 1;
	if (atoi(gtk_entry_get_text(GTK_ENTRY(tvplug_height_process_textfield))) != tv_height_process)
		tv_changed = 1;
	if (atoi(gtk_entry_get_text(GTK_ENTRY(tvplug_width_process_textfield))) != tv_width_process)
		tv_changed = 1;
	port = atoi(gtk_entry_get_text(GTK_ENTRY(tvplug_port_textfield)));
	tv_width_capture = atoi(gtk_entry_get_text(GTK_ENTRY(tvplug_width_capture_textfield)));
	tv_height_capture = atoi(gtk_entry_get_text(GTK_ENTRY(tvplug_height_capture_textfield)));
	tv_width_process = atoi(gtk_entry_get_text(GTK_ENTRY(tvplug_width_process_textfield)));
	tv_height_process = atoi(gtk_entry_get_text(GTK_ENTRY(tvplug_height_process_textfield)));
	tv_width_edit = atoi(gtk_entry_get_text(GTK_ENTRY(tvplug_width_edit_textfield)));
	tv_height_edit = atoi(gtk_entry_get_text(GTK_ENTRY(tvplug_height_edit_textfield)));
	scene_detection_treshold = atoi(gtk_entry_get_text(GTK_ENTRY(textfield_sd_treshold)));
	scene_detection_width_decimation = atoi(gtk_entry_get_text(GTK_ENTRY(textfield_sd_decimation)));

	geom_height = atoi(gtk_entry_get_text(GTK_ENTRY(geom_h_text)));
	geom_width = atoi(gtk_entry_get_text(GTK_ENTRY(geom_w_text)));
	geom_y = atoi(gtk_entry_get_text(GTK_ENTRY(geom_y_text)));
	geom_x = atoi(gtk_entry_get_text(GTK_ENTRY(geom_x_text)));

	max_file_size = atoi(gtk_entry_get_text(GTK_ENTRY(maxfilesize_text)));
	file_flush = atoi(gtk_entry_get_text(GTK_ENTRY(fileflush_text)));

	if (tv_changed)
	{
		reset_lavrec_tv();
		reset_lavplay_tv();
		reset_lavedit_tv();
	}
	if (audio_changed)
	{
		reset_audio();
	}

	save_config();

	table_set_text(0,0,0,0,0,0,0,0);
}

void create_capture_layout(GtkWidget *table)
{
	GtkWidget *label, *button, *hbox, *scrollbar;
	char tmp[10];

	/*Input method (SHVS/Composite)*/
	label = gtk_label_new("Video Input Method: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 0, 1);
	gtk_widget_show (label);
	hbox = gtk_hbox_new (FALSE, 10);
	button = gtk_radio_button_new_with_label(NULL, "Super-VHS");
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(change_method), (gpointer) "Super-VHS");
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
	if (t_connector == 's') gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_widget_show (button);
	button = gtk_radio_button_new_with_label(gtk_radio_button_group (GTK_RADIO_BUTTON (button)), "Composite");
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(change_method), (gpointer) "Composite");
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
	if (t_connector == 'c') gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_widget_show (button);
	button = gtk_radio_button_new_with_label(gtk_radio_button_group (GTK_RADIO_BUTTON (button)), "TV Tuner");
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(change_method), (gpointer) "TV");
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
	if (t_connector == 't') gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_widget_show (button);
	button = gtk_radio_button_new_with_label(gtk_radio_button_group (GTK_RADIO_BUTTON (button)), "Auto");
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(change_method), (gpointer) "Auto");
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
	if (t_connector == 'a') gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_widget_show (button);
	gtk_table_attach_defaults (GTK_TABLE (table), hbox, 1, 2, 0, 1);
	gtk_widget_show(hbox);

	/*Input Format (NTSC/PAL/SECAM)*/
	label = gtk_label_new("Video Input Format: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 1, 2);
	gtk_widget_show (label);
	hbox = gtk_hbox_new (FALSE, 10);
	button = gtk_radio_button_new_with_label(NULL, "NTSC");
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(change_format), (gpointer) "NTSC");
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
	if (t_vinput == 'n') gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_widget_show (button);
	button = gtk_radio_button_new_with_label(gtk_radio_button_group (GTK_RADIO_BUTTON (button)), "PAL");
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(change_format), (gpointer) "PAL");
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
	if (t_vinput == 'p') gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_widget_show (button);
	button = gtk_radio_button_new_with_label(gtk_radio_button_group (GTK_RADIO_BUTTON (button)), "SECAM");
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(change_format), (gpointer) "SECAM");
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
	if (t_vinput == 's') gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_widget_show (button);
	gtk_table_attach_defaults (GTK_TABLE (table), hbox, 1, 2, 1, 2);
	gtk_widget_show(hbox);
										
	/*Input File Format (Avi/AVI w/ fields exchanged/quicktime/movtar)*/
	label = gtk_label_new("Capture File Format: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 2, 3);
	gtk_widget_show (label);
	hbox = gtk_hbox_new (FALSE, 10);
	button = gtk_radio_button_new_with_label(NULL, "AVI");
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(change_fileformat), (gpointer) "AVI");
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
	if (t_output == 'a') gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_widget_show (button);
	button = gtk_radio_button_new_with_label(gtk_radio_button_group (GTK_RADIO_BUTTON (button)), "AVI (f.e.)");
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(change_fileformat), (gpointer) "AVI (f.e.)");
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
	if (t_output == 'A') gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_widget_show (button);
	button = gtk_radio_button_new_with_label(gtk_radio_button_group (GTK_RADIO_BUTTON (button)), "Quicktime");
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(change_fileformat), (gpointer) "Quicktime");
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
	if (t_output == 'q') gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_widget_show (button);
	button = gtk_radio_button_new_with_label(gtk_radio_button_group (GTK_RADIO_BUTTON (button)), "Movtar");
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(change_fileformat), (gpointer) "Movtar");
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
	if (t_output == 'm') gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_widget_show (button);
	gtk_table_attach_defaults (GTK_TABLE (table), hbox, 1, 2, 2, 3);
	gtk_widget_show(hbox);

	/*Capture Resolution (1x, 1/2x, 1/4x)*/
	decimation_label = gtk_label_new("Capture Resolution: ");
	gtk_misc_set_alignment(GTK_MISC(decimation_label), 0.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), decimation_label, 0, 1, 3, 4);
	gtk_widget_show (decimation_label);
	if (software_encoding) gtk_widget_hide(decimation_label);
	decimation_hbox = gtk_hbox_new (FALSE, 10);
	button = gtk_radio_button_new_with_label(NULL, "Full");
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(change_resolution), (gpointer) 1);
	gtk_box_pack_start (GTK_BOX (decimation_hbox), button, FALSE, FALSE, 0);
	if (t_res == 1) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_widget_show (button);
	button = gtk_radio_button_new_with_label(gtk_radio_button_group (GTK_RADIO_BUTTON (button)), "1/2");
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(change_resolution), (gpointer) 2);
	gtk_box_pack_start (GTK_BOX (decimation_hbox), button, FALSE, FALSE, 0);
	if (t_res == 2) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_widget_show (button);
	button = gtk_radio_button_new_with_label(gtk_radio_button_group (GTK_RADIO_BUTTON (button)), "1/4");
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(change_resolution), (gpointer) 4);
	gtk_box_pack_start (GTK_BOX (decimation_hbox), button, FALSE, FALSE, 0);
	if (t_res == 4) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_widget_show (button);
	gtk_table_attach_defaults (GTK_TABLE (table), decimation_hbox, 1, 2, 3, 4);
	gtk_widget_show(decimation_hbox);
	if (software_encoding) gtk_widget_hide(decimation_hbox);

	/* software-encoded recording size */
	soft_r_label = gtk_label_new("Recording Size (<w>x<h>): ");
	gtk_misc_set_alignment(GTK_MISC(soft_r_label), 0.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), soft_r_label, 0, 1, 4, 5);
	gtk_widget_show (soft_r_label);
	if (!software_encoding) gtk_widget_hide(soft_r_label);
	soft_r_hbox = gtk_hbox_new(FALSE, 10);
	soft_r_w_text = gtk_entry_new();
	gtk_widget_set_usize(soft_r_w_text, 50, 23);
	sprintf(tmp, "%i", software_recwidth);
	gtk_entry_set_text(GTK_ENTRY(soft_r_w_text), tmp);
	gtk_box_pack_start (GTK_BOX (soft_r_hbox), soft_r_w_text, FALSE, FALSE, 0);
	gtk_widget_show(soft_r_w_text);
	label = gtk_label_new("x");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_box_pack_start (GTK_BOX (soft_r_hbox), label, FALSE, FALSE, 0);
	gtk_widget_show (label);
	soft_r_h_text = gtk_entry_new();
	gtk_widget_set_usize(soft_r_h_text, 50, 23);
	sprintf(tmp, "%i", software_recheight);
	gtk_entry_set_text(GTK_ENTRY(soft_r_h_text), tmp);
	gtk_box_pack_start (GTK_BOX (soft_r_hbox), soft_r_h_text, FALSE, FALSE, 0);
	gtk_widget_show(soft_r_h_text);
	gtk_table_attach_defaults (GTK_TABLE (table), soft_r_hbox, 1, 2, 4, 5);
	gtk_widget_show(soft_r_hbox);
	if (!software_encoding) gtk_widget_hide(soft_r_hbox);

	/* Capture Quality */
	label = gtk_label_new("Capture Quality: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 5, 6);
	gtk_widget_show (label);
	hbox = gtk_hbox_new (FALSE, 10);
	adjustment = gtk_adjustment_new(quality, 0, 100, 1, 10, 0);
	scrollbar = gtk_hscale_new(GTK_ADJUSTMENT (adjustment));
	gtk_scale_set_digits(GTK_SCALE (scrollbar), 0);
	gtk_scale_set_value_pos(GTK_SCALE (scrollbar), GTK_POS_RIGHT);
	gtk_box_pack_start(GTK_BOX (hbox), scrollbar, TRUE, TRUE, 0);
	gtk_widget_show(scrollbar);
	label = gtk_label_new("                ");
	gtk_box_pack_start(GTK_BOX (hbox), label, TRUE, TRUE, 0);
	gtk_widget_show (label);
	gtk_table_attach_defaults (GTK_TABLE (table), hbox, 1, 2, 5, 6);
	gtk_widget_show(hbox);

	/* software or hardware JPEG encoding ? */
	label = gtk_label_new("JPEG Encoding Type: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 6, 7);
	gtk_widget_show (label);
	hbox = gtk_hbox_new (FALSE, 10);
	button = gtk_radio_button_new_with_label(NULL, "Hardware JPEG");
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
		GTK_SIGNAL_FUNC(change_encodingtype), (gpointer)0);
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
	if (t_encoding==0) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_widget_show (button);
	button = gtk_radio_button_new_with_label(gtk_radio_button_group (GTK_RADIO_BUTTON (button)),
		"Software JPEG");
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
		GTK_SIGNAL_FUNC(change_encodingtype), (gpointer)1);
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
	if (t_encoding==1) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_widget_show (button);
	gtk_table_attach_defaults (GTK_TABLE (table), hbox, 1, 2, 6, 7);
	gtk_widget_show(hbox);	

	/* Video Device */
	label = gtk_label_new("Video Device: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 7, 8);
	gtk_widget_show (label);
	hbox = gtk_hbox_new(FALSE, 0);
	textfield_videodev = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(textfield_videodev), videodev);
	gtk_box_pack_start (GTK_BOX (hbox), textfield_videodev, FALSE, FALSE, 0);
	gtk_widget_show(textfield_videodev);
	gtk_table_attach_defaults (GTK_TABLE (table), hbox, 1, 2, 7, 8);
	gtk_widget_show(hbox);
}

void create_sound_layout(GtkWidget *table)
{
	GtkWidget *label, *button, *hbox; //, *scrollbar;

	/*Audio Input Source*/
	label = gtk_label_new("Audio Source: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 0, 1);
	gtk_widget_show (label);
	hbox = gtk_hbox_new (FALSE, 10);
	button = gtk_radio_button_new_with_label(NULL, "Line-in");
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(change_audio_source), (gpointer) "Line-in");
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
	if (t_ainput == 'l') gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_widget_show (button);
	button = gtk_radio_button_new_with_label(gtk_radio_button_group (GTK_RADIO_BUTTON (button)), "Microphone");
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(change_audio_source), (gpointer) "Microphone");
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
	if (t_ainput == 'm') gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_widget_show (button);
	button = gtk_radio_button_new_with_label(gtk_radio_button_group (GTK_RADIO_BUTTON (button)), "CD-ROM");
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(change_audio_source), (gpointer) "CD-ROM");
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
	if (t_ainput == 'c') gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_widget_show (button);
	gtk_table_attach_defaults (GTK_TABLE (table), hbox, 1, 2, 0, 1);
	gtk_widget_show(hbox);

	/*Audio Capturing Volume*/
/*	label = gtk_label_new("Audio Volume: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 1, 2);
	gtk_widget_show (label);
	hbox = gtk_hbox_new (FALSE, 10);
	audio_adjustment = gtk_adjustment_new(audio_lev, 0, 100, 1, 10, 0);
	scrollbar = gtk_hscale_new(GTK_ADJUSTMENT (audio_adjustment));
	gtk_scale_set_digits(GTK_SCALE (scrollbar), 0);
	gtk_scale_set_value_pos(GTK_SCALE (scrollbar), GTK_POS_RIGHT);
	gtk_box_pack_start(GTK_BOX (hbox), scrollbar, TRUE, TRUE, 0);
	gtk_widget_show(scrollbar);
	label = gtk_label_new("                ");
	gtk_box_pack_start(GTK_BOX (hbox), label, TRUE, TRUE, 0);
	gtk_widget_show (label);
	gtk_table_attach_defaults (GTK_TABLE (table), hbox, 1, 2, 1, 2);
	gtk_widget_show(hbox);*/

	/*Audio Bit Size (0 (=no sound), 8, 16)*/
	label = gtk_label_new("Audio Bit Size: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 2, 3);
	gtk_widget_show (label);
	hbox = gtk_hbox_new (FALSE, 10);
	button = gtk_radio_button_new_with_label(NULL, "0 (no sound)");
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(change_audio_bitsize), (gpointer) 0);
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
	if (t_bitsize == 0) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_widget_show (button);
	button = gtk_radio_button_new_with_label(gtk_radio_button_group (GTK_RADIO_BUTTON (button)), "8");
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(change_audio_bitsize), (gpointer) 8);
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
	if (t_bitsize == 8) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_widget_show (button);
	button = gtk_radio_button_new_with_label(gtk_radio_button_group (GTK_RADIO_BUTTON (button)), "16");
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(change_audio_bitsize), (gpointer) 16);
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
	if (t_bitsize == 16) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_widget_show (button);
	gtk_table_attach_defaults (GTK_TABLE (table), hbox, 1, 2, 2, 3);
	gtk_widget_show(hbox);

	/*Audio Bit Rate (11025, 22050 (default), 44100)*/
	label = gtk_label_new("Audio Bit Rate: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 3, 4);
	gtk_widget_show (label);
	hbox = gtk_hbox_new (FALSE, 10);
	button = gtk_radio_button_new_with_label(NULL, "11025");
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(change_audio_bitrate), (gpointer) 11025);
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
	if (t_bitrate == 11025) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_widget_show (button);
	button = gtk_radio_button_new_with_label(gtk_radio_button_group (GTK_RADIO_BUTTON (button)), "22050");
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(change_audio_bitrate), (gpointer) 22050);
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
	if (t_bitrate == 22050) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_widget_show (button);
	button = gtk_radio_button_new_with_label(gtk_radio_button_group (GTK_RADIO_BUTTON (button)), "44100");
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(change_audio_bitrate), (gpointer) 44100);
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
	if (t_bitrate == 44100) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_widget_show (button);
	gtk_table_attach_defaults (GTK_TABLE (table), hbox, 1, 2, 3, 4);
	gtk_widget_show(hbox);

	/* read() or mmap() ? */
	label = gtk_label_new("Audio Read Method: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 4, 5);
	gtk_widget_show (label);
	hbox = gtk_hbox_new (FALSE, 10);
	button = gtk_radio_button_new_with_label(NULL, "mmap()");
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
		GTK_SIGNAL_FUNC(change_audioreadmethod), (gpointer) 0);
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
	if (t_useread==0) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_widget_show (button);
	button = gtk_radio_button_new_with_label(gtk_radio_button_group (GTK_RADIO_BUTTON (button)),
		"read()");
	gtk_signal_connect(GTK_OBJECT(button), "clicked",
		GTK_SIGNAL_FUNC(change_audioreadmethod), (gpointer) 1);
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
	if (t_useread==1) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_widget_show (button);
	gtk_table_attach_defaults (GTK_TABLE (table), hbox, 1, 2, 4, 5);
	gtk_widget_show(hbox);

	/*Stereo?*/
	label = gtk_label_new("Other options: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 5, 6);
	gtk_widget_show (label);
	audiostereo_checkbutton =  gtk_check_button_new_with_label ("Stereo");
	if (stereo) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (audiostereo_checkbutton), TRUE);
	gtk_widget_show (audiostereo_checkbutton);
	gtk_table_attach_defaults (GTK_TABLE (table), audiostereo_checkbutton, 1, 2, 5, 6);

	/*audio_mute audio during recording?*/
	audioaudio_mute_checkbutton =  gtk_check_button_new_with_label ("Mute during capturing");
	if (audio_mute) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (audioaudio_mute_checkbutton), TRUE);
	gtk_widget_show (audioaudio_mute_checkbutton);
	gtk_table_attach_defaults (GTK_TABLE (table), audioaudio_mute_checkbutton, 1, 2, 6, 7);

	/* Audio Device */
	label = gtk_label_new("Audio Device: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 7, 8);
	gtk_widget_show (label);
	hbox = gtk_hbox_new(FALSE, 0);
	textfield_audiodev = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(textfield_audiodev), audiodev);
	gtk_box_pack_start (GTK_BOX (hbox), textfield_audiodev, FALSE, FALSE, 0);
	gtk_widget_show(textfield_audiodev);
	gtk_table_attach_defaults (GTK_TABLE (table), hbox, 1, 2, 7, 8);
	gtk_widget_show(hbox);

	/* Mixer Device */
	label = gtk_label_new("Mixer Device: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 8, 9);
	gtk_widget_show (label);
	hbox = gtk_hbox_new(FALSE, 0);
	textfield_mixerdev = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(textfield_mixerdev), mixerdev);
	gtk_box_pack_start (GTK_BOX (hbox), textfield_mixerdev, FALSE, FALSE, 0);
	gtk_widget_show(textfield_mixerdev);
	gtk_table_attach_defaults (GTK_TABLE (table), hbox, 1, 2, 8, 9);
	gtk_widget_show(hbox);
}

void create_advanced_layout(GtkWidget *table)
{
	GtkWidget *label, *button, *hbox;
	char tmp[10];

	/*Sync Correction*/
	label = gtk_label_new("Sync correction: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 0, 1);
	gtk_widget_show (label);
	hbox = gtk_hbox_new (FALSE, 10);
	button = gtk_radio_button_new_with_label(NULL, "None");
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(change_sync_correction), (gpointer) 0);
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
	if (t_sync == 0) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_widget_show (button);
	button = gtk_radio_button_new_with_label(gtk_radio_button_group (GTK_RADIO_BUTTON (button)), "Replicate frames");
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(change_sync_correction), (gpointer) 1);
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
	if (t_sync == 1) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_widget_show (button);
	button = gtk_radio_button_new_with_label(gtk_radio_button_group (GTK_RADIO_BUTTON (button)), "Replicate frames + sync");
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(change_sync_correction), (gpointer) 2);
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
	if (t_sync == 2) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
	gtk_widget_show (button);
	gtk_table_attach_defaults (GTK_TABLE (table), hbox, 1, 2, 0, 1);
	gtk_widget_show(hbox);

	/* maxfilesize */
	label = gtk_label_new("Max file size: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 1, 2);
	gtk_widget_show (label);
	hbox = gtk_hbox_new(FALSE, 0);
	maxfilesize_text = gtk_entry_new();
	gtk_widget_set_usize(maxfilesize_text, 100, 23);
	sprintf(tmp, "%d", max_file_size);
	gtk_entry_set_text(GTK_ENTRY(maxfilesize_text), tmp);
	gtk_box_pack_start (GTK_BOX (hbox), maxfilesize_text, FALSE, FALSE, 0);
	gtk_widget_show(maxfilesize_text);
	label = gtk_label_new(" (MB)");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_widget_show (label);
	gtk_table_attach_defaults (GTK_TABLE (table), hbox, 1, 2, 1, 2);
	gtk_widget_show(hbox);

	/* file-flush */
	label = gtk_label_new("File flush rate: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 2, 3);
	gtk_widget_show (label);
	hbox = gtk_hbox_new(FALSE, 0);
	fileflush_text = gtk_entry_new();
	gtk_widget_set_usize(fileflush_text, 50, 23);
	sprintf(tmp, "%d", file_flush);
	gtk_entry_set_text(GTK_ENTRY(fileflush_text), tmp);
	gtk_box_pack_start (GTK_BOX (hbox), fileflush_text, FALSE, FALSE, 0);
	gtk_widget_show(fileflush_text);
	label = gtk_label_new(" (1 flush per N frames)");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);
	gtk_table_attach_defaults (GTK_TABLE (table), hbox, 1, 2, 2, 3);
	gtk_widget_show(hbox);

	/*screenshot mode*/
	label = gtk_label_new("Other options: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 3, 4);
	gtk_widget_show (label);
	screenperscreen_checkbutton =  gtk_check_button_new_with_label ("Single frame capture mode");
	gtk_widget_show (screenperscreen_checkbutton);
	gtk_table_attach_defaults (GTK_TABLE (table), screenperscreen_checkbutton, 1, 2, 3, 4);
	if (single_frame) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (screenperscreen_checkbutton), TRUE);

	/*timer mode*/
	hbox = gtk_hbox_new(FALSE, 10);
	hbox_timer = gtk_hbox_new (FALSE, 10);
	timermode_checkbutton =  gtk_check_button_new_with_label ("Record for a pre-specified time");
	gtk_signal_connect(GTK_OBJECT(timermode_checkbutton), "clicked", GTK_SIGNAL_FUNC(change_preconfigured_time), NULL);
	gtk_widget_show (timermode_checkbutton);
	gtk_box_pack_start (GTK_BOX (hbox), timermode_checkbutton, FALSE, FALSE, 0);
	if (record_time != 100000) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (timermode_checkbutton), TRUE);
	label = gtk_label_new("- time (sec): ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox_timer), label, FALSE, FALSE, 0);
	textfield_timer = gtk_entry_new();
	gtk_widget_set_usize(textfield_timer, 50, 23);
	sprintf(tmp, "%i", record_time);
	gtk_entry_set_text(GTK_ENTRY(textfield_timer), tmp);
	gtk_widget_show(textfield_timer);
	gtk_box_pack_start (GTK_BOX (hbox_timer), textfield_timer, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), hbox_timer, FALSE, FALSE, 0);
	gtk_widget_show(hbox);
	gtk_table_attach_defaults (GTK_TABLE (table), hbox, 1, 2, 4, 5);

	/*time lapse mode*/
	hbox = gtk_hbox_new(FALSE, 10);
	hbox_lapse = gtk_hbox_new (FALSE, 10);
	timelapsemode_checkbutton =  gtk_check_button_new_with_label ("Time lapse mode");
	gtk_signal_connect(GTK_OBJECT(timelapsemode_checkbutton), "clicked", GTK_SIGNAL_FUNC(change_timelapsemode), NULL);
	gtk_widget_show (timelapsemode_checkbutton);
	if (time_lapse != 1) gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (timelapsemode_checkbutton), TRUE);
	gtk_box_pack_start (GTK_BOX (hbox), timelapsemode_checkbutton, FALSE, FALSE, 0);
	label = gtk_label_new("- speed: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox_lapse), label, FALSE, FALSE, 0);
	textfield_lapse = gtk_entry_new();
	gtk_widget_set_usize(textfield_lapse, 50, 23);
	sprintf(tmp, "%i", time_lapse);
	gtk_entry_set_text(GTK_ENTRY(textfield_lapse), tmp);
	gtk_widget_show(textfield_lapse);
	gtk_box_pack_start (GTK_BOX (hbox_lapse), textfield_lapse, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), hbox_lapse, FALSE, FALSE, 0);
	gtk_widget_show(hbox);
	gtk_table_attach_defaults (GTK_TABLE (table), hbox, 1, 2, 5, 6);

	/* MJPEG buffer # */
	label = gtk_label_new("MJPEG buffer number: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 6, 7);
	gtk_widget_show (label);
	hbox = gtk_hbox_new(FALSE, 0);
	textfield_buffers = gtk_entry_new();
	gtk_widget_set_usize(textfield_buffers, 50, 23);
	sprintf(tmp, "%i", MJPG_nbufs);
	gtk_entry_set_text(GTK_ENTRY(textfield_buffers), tmp);
	gtk_box_pack_start (GTK_BOX (hbox), textfield_buffers, FALSE, FALSE, 0);
	gtk_widget_show(textfield_buffers);
	//gtk_table_attach_defaults (GTK_TABLE (table), hbox, 1, 2, 6, 7);
	//gtk_widget_show(hbox);
	
	/* MJPEG buffer size */
	label = gtk_label_new("     MJPEG buffer size: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	//gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 6, 7);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_widget_show (label);
	//hbox = gtk_hbox_new(FALSE, 0);
	textfield_buffer_size = gtk_entry_new();
	gtk_widget_set_usize(textfield_buffer_size, 50, 23);
	sprintf(tmp, "%i", MJPG_bufsize);
	gtk_entry_set_text(GTK_ENTRY(textfield_buffer_size), tmp);
	gtk_box_pack_start (GTK_BOX (hbox), textfield_buffer_size, FALSE, FALSE, 0);
	gtk_widget_show(textfield_buffer_size);
	label = gtk_label_new("(kilobyte)");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_widget_show (label);
	gtk_table_attach_defaults (GTK_TABLE (table), hbox, 1, 2, 6, 7);
	gtk_widget_show(hbox);

	/* treshold/decimation for scene detection */
	label = gtk_label_new("Scene detection decimation: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 7, 8);
	gtk_widget_show (label);
	hbox = gtk_hbox_new(FALSE, 0);
	textfield_sd_decimation = gtk_entry_new();
	gtk_widget_set_usize(textfield_sd_decimation, 50, 23);
	sprintf(tmp, "%i", scene_detection_width_decimation);
	gtk_entry_set_text(GTK_ENTRY(textfield_sd_decimation), tmp);
	gtk_box_pack_start (GTK_BOX (hbox), textfield_sd_decimation, FALSE, FALSE, 0);
	gtk_widget_show(textfield_sd_decimation);
	label = gtk_label_new("     Scene detection treshold: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_widget_show (label);
	textfield_sd_treshold = gtk_entry_new();
	gtk_widget_set_usize(textfield_sd_treshold, 50, 23);
	sprintf(tmp, "%i", scene_detection_treshold);
	gtk_entry_set_text(GTK_ENTRY(textfield_sd_treshold), tmp);
	gtk_box_pack_start (GTK_BOX (hbox), textfield_sd_treshold, FALSE, FALSE, 0);
	gtk_widget_show(textfield_sd_treshold);
	gtk_table_attach_defaults (GTK_TABLE (table), hbox, 1, 2, 7, 8);
	gtk_widget_show(hbox);
}

void create_tvplug_options_layout(GtkWidget *table)
{
	XvAdaptorInfo *ai;
	int ver, rel, req, ev, err;
	int adaptors;
	int i;
	GString *tv_ports;
	char tmp[10];
	GtkWidget *hbox, *label;

	/* TV size (capture) */
	label = gtk_label_new("TV Size (Capturing): ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 0, 1);
	gtk_widget_show (label);
	hbox = gtk_hbox_new(FALSE, 10);
	tvplug_width_capture_textfield = gtk_entry_new();
	gtk_widget_set_usize(tvplug_width_capture_textfield, 50, 23);
	sprintf(tmp, "%i", tv_width_capture);
	gtk_entry_set_text(GTK_ENTRY(tvplug_width_capture_textfield), tmp);
	gtk_box_pack_start (GTK_BOX (hbox), tvplug_width_capture_textfield, FALSE, FALSE, 0);
	gtk_widget_show(tvplug_width_capture_textfield);
	label = gtk_label_new("x");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_widget_show (label);
	tvplug_height_capture_textfield = gtk_entry_new();
	gtk_widget_set_usize(tvplug_height_capture_textfield, 50, 23);
	sprintf(tmp, "%i", tv_height_capture);
	gtk_entry_set_text(GTK_ENTRY(tvplug_height_capture_textfield), tmp);
	gtk_box_pack_start (GTK_BOX (hbox), tvplug_height_capture_textfield, FALSE, FALSE, 0);
	gtk_widget_show(tvplug_height_capture_textfield);
	label = gtk_label_new("(pixels)");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_widget_show (label);
	gtk_table_attach_defaults (GTK_TABLE (table), hbox, 1, 2, 0, 1);
	gtk_widget_show(hbox);

	/* TV size (edit) */
	label = gtk_label_new("TV Size (Editing): ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 1, 2);
	gtk_widget_show (label);
	hbox = gtk_hbox_new(FALSE, 10);
	tvplug_width_edit_textfield = gtk_entry_new();
	gtk_widget_set_usize(tvplug_width_edit_textfield, 50, 23);
	sprintf(tmp, "%i", tv_width_edit);
	gtk_entry_set_text(GTK_ENTRY(tvplug_width_edit_textfield), tmp);
	gtk_box_pack_start (GTK_BOX (hbox), tvplug_width_edit_textfield, FALSE, FALSE, 0);
	gtk_widget_show(tvplug_width_edit_textfield);
	label = gtk_label_new("x");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_widget_show (label);
	tvplug_height_edit_textfield = gtk_entry_new();
	gtk_widget_set_usize(tvplug_height_edit_textfield, 50, 23);
	sprintf(tmp, "%i", tv_height_edit);
	gtk_entry_set_text(GTK_ENTRY(tvplug_height_edit_textfield), tmp);
	gtk_box_pack_start (GTK_BOX (hbox), tvplug_height_edit_textfield, FALSE, FALSE, 0);
	gtk_widget_show(tvplug_height_edit_textfield);
	label = gtk_label_new("(pixels)");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_widget_show (label);
	gtk_table_attach_defaults (GTK_TABLE (table), hbox, 1, 2, 1, 2);
	gtk_widget_show(hbox);

	/* TV size (view) */
	label = gtk_label_new("TV Size (processing): ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 2, 3);
	gtk_widget_show (label);
	hbox = gtk_hbox_new(FALSE, 10);
	tvplug_width_process_textfield = gtk_entry_new();
	gtk_widget_set_usize(tvplug_width_process_textfield, 50, 23);
	sprintf(tmp, "%i", tv_width_process);
	gtk_entry_set_text(GTK_ENTRY(tvplug_width_process_textfield), tmp);
	gtk_box_pack_start (GTK_BOX (hbox), tvplug_width_process_textfield, FALSE, FALSE, 0);
	gtk_widget_show(tvplug_width_process_textfield);
	label = gtk_label_new("x");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_widget_show (label);
	tvplug_height_process_textfield = gtk_entry_new();
	gtk_widget_set_usize(tvplug_height_process_textfield, 50, 23);
	sprintf(tmp, "%i", tv_height_process);
	gtk_entry_set_text(GTK_ENTRY(tvplug_height_process_textfield), tmp);
	gtk_box_pack_start (GTK_BOX (hbox), tvplug_height_process_textfield, FALSE, FALSE, 0);
	gtk_widget_show(tvplug_height_process_textfield);
	label = gtk_label_new("(pixels)");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_widget_show (label);
	gtk_table_attach_defaults (GTK_TABLE (table), hbox, 1, 2, 2, 3);
	gtk_widget_show(hbox);

	/* Recording size parameters (-g) */
	label = gtk_label_new("Subframe size (<x>,<y>,<w>x<h>): ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 3, 4);
	gtk_widget_show (label);
	hbox = gtk_hbox_new(FALSE, 10);
	geom_x_text = gtk_entry_new();
	gtk_widget_set_usize(geom_x_text, 50, 23);
	sprintf(tmp, "%i", geom_x);
	gtk_entry_set_text(GTK_ENTRY(geom_x_text), tmp);
	gtk_box_pack_start (GTK_BOX (hbox), geom_x_text, FALSE, FALSE, 0);
	gtk_widget_show(geom_x_text);
	label = gtk_label_new(",");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_widget_show (label);
	geom_y_text = gtk_entry_new();
	gtk_widget_set_usize(geom_y_text, 50, 23);
	sprintf(tmp, "%i", geom_y);
	gtk_entry_set_text(GTK_ENTRY(geom_y_text), tmp);
	gtk_box_pack_start (GTK_BOX (hbox), geom_y_text, FALSE, FALSE, 0);
	gtk_widget_show(geom_y_text);
	label = gtk_label_new(",");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_widget_show (label);
	geom_w_text = gtk_entry_new();
	gtk_widget_set_usize(geom_w_text, 50, 23);
	sprintf(tmp, "%i", geom_width);
	gtk_entry_set_text(GTK_ENTRY(geom_w_text), tmp);
	gtk_box_pack_start (GTK_BOX (hbox), geom_w_text, FALSE, FALSE, 0);
	gtk_widget_show(geom_w_text);
	label = gtk_label_new("x");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_widget_show (label);
	geom_h_text = gtk_entry_new();
	gtk_widget_set_usize(geom_h_text, 50, 23);
	sprintf(tmp, "%i", geom_height);
	gtk_entry_set_text(GTK_ENTRY(geom_h_text), tmp);
	gtk_box_pack_start (GTK_BOX (hbox), geom_h_text, FALSE, FALSE, 0);
	gtk_widget_show(geom_h_text);
	gtk_widget_show (label);
	gtk_table_attach_defaults (GTK_TABLE (table), hbox, 1, 2, 3, 4);
	gtk_widget_show(hbox);

	/* TV port */
	label = gtk_label_new("TV V4L/Xvideo-port: ");
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1, 4, 5);
	gtk_widget_show (label);
	hbox = gtk_hbox_new(FALSE, 10);
	tvplug_port_textfield = gtk_entry_new();
	gtk_widget_set_usize(tvplug_port_textfield, 50, 23);
	sprintf(tmp, "%i", port);
	gtk_entry_set_text(GTK_ENTRY(tvplug_port_textfield), tmp);
	gtk_box_pack_start (GTK_BOX (hbox), tvplug_port_textfield, FALSE, FALSE, 0);
	gtk_widget_show(tvplug_port_textfield);
/*	gtk_table_attach_defaults (GTK_TABLE (table), hbox, 1, 2, 4, 5);
	gtk_widget_show(hbox);*/

	if (Success != XvQueryExtension(GDK_DISPLAY(),&ver,&rel,&req,&ev,&err))
	{
		tv_ports = g_string_new("Server doesn't support Xvideo");
	}
	else if (Success != XvQueryAdaptors(GDK_DISPLAY(),DefaultRootWindow(GDK_DISPLAY()),&adaptors,&ai))
	{
		tv_ports = g_string_new("Oops: XvQueryAdaptors failed");
	}
	else
	{
		// create string: "Available: "
		tv_ports = g_string_new("Available V4L ports: ");
		for (i = 0; i < adaptors; i++)
		{
			if (strcmp(ai[i].name, "video4linux") == 0)
			{
				if (i != 0)
					sprintf(tmp, ", %ld", ai[i].base_id);
				else
					sprintf(tmp, "%ld", ai[i].base_id);
				g_string_append(tv_ports, tmp);
			}
		}
	}

	label = gtk_label_new(tv_ports->str);
	gtk_misc_set_alignment(GTK_MISC(label), 0.0, GTK_MISC(label)->yalign);
	/*gtk_table_attach_defaults (GTK_TABLE (table), label, 1, 2, 5, 6);
	gtk_widget_show (label);*/

	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);
	gtk_table_attach_defaults (GTK_TABLE (table), hbox, 1, 2, 4, 5);
	gtk_widget_show(hbox);
}

void open_options_window(GtkWidget *widget, gpointer data)
{
	GtkWidget *options_window, *table, *button, *options_notebook;
	GtkWidget *vbox, *hbox;

	load_config();

	/* set temp variables */
	t_sync = sync_corr;
	t_bitsize = audio_size;
	t_bitrate = audio_rate;
	t_res = hordcm;
	t_connector = connector;
	t_vinput = video_input;
	t_ainput = audio_recsrc;
	t_output = video_format;
	t_encoding = software_encoding;
	t_useread = use_read;

	options_window = gtk_window_new(GTK_WINDOW_DIALOG);
	vbox = gtk_vbox_new (FALSE, 10);
	options_notebook = gtk_notebook_new ();

	gtk_window_set_title (GTK_WINDOW(options_window), "Linux Video Studio - Options");
	gtk_container_set_border_width (GTK_CONTAINER (options_window), 15);

	hbox = gtk_hbox_new(FALSE, 20);
	table = gtk_table_new (2, 8, FALSE);
	create_capture_layout(table);
	gtk_box_pack_start (GTK_BOX (hbox), table, TRUE, TRUE, 20);
	gtk_widget_show(table);
	gtk_notebook_append_page (GTK_NOTEBOOK (options_notebook), hbox, gtk_label_new("Video Options"));
	gtk_widget_show (hbox);

	hbox = gtk_hbox_new(TRUE, 20);
	table = gtk_table_new (2,9, FALSE);
	create_sound_layout(table);
	gtk_box_pack_start (GTK_BOX (hbox), table, TRUE, TRUE, 20);
	gtk_widget_show(table);
	gtk_notebook_append_page (GTK_NOTEBOOK (options_notebook), hbox, gtk_label_new("Audio Options"));
	gtk_widget_show (hbox);

	hbox = gtk_hbox_new(TRUE, 20);
	table = gtk_table_new (2,8, FALSE);
	create_advanced_layout(table);
	gtk_box_pack_start (GTK_BOX (hbox), table, TRUE, TRUE, 20);
	gtk_widget_show(table);
	gtk_notebook_append_page (GTK_NOTEBOOK (options_notebook), hbox, gtk_label_new("Advanced Options"));
	gtk_widget_show (hbox);

	hbox = gtk_hbox_new(TRUE, 20);
	table = gtk_table_new (2,5, FALSE);
	create_tvplug_options_layout(table);
	gtk_box_pack_start (GTK_BOX (hbox), table, TRUE, TRUE, 20);
	gtk_widget_show(table);
	gtk_notebook_append_page (GTK_NOTEBOOK (options_notebook), hbox, gtk_label_new("TV Screen Options"));
	gtk_widget_show (hbox);

        hbox = gtk_hbox_new(TRUE, 20);
        table = gtk_table_new (2,4, FALSE);
        create_encoding_layout(table);
        gtk_box_pack_start (GTK_BOX (hbox), table, TRUE, TRUE, 20);
        gtk_widget_show(table);
        gtk_notebook_append_page (GTK_NOTEBOOK (options_notebook), hbox, gtk_label_new("Encoding Options"));
        gtk_widget_show (hbox);

	gtk_notebook_set_tab_pos (GTK_NOTEBOOK (options_notebook), GTK_POS_TOP);
	gtk_box_pack_start (GTK_BOX (vbox), options_notebook, FALSE, FALSE, 0);
	gtk_widget_show(options_notebook);

	hbox = gtk_hbox_new(TRUE, 20);

	button = gtk_button_new_with_label("OK");
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC (accept_encoptions), NULL);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC (accept_options), NULL);
	gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
		gtk_widget_destroy, GTK_OBJECT(options_window));
	gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 20);
	gtk_widget_show(button);

	button = gtk_button_new_with_label("Cancel");
	gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
		gtk_widget_destroy, GTK_OBJECT(options_window));
	gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 20);
	gtk_widget_show(button);

	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show (hbox);

	gtk_container_add (GTK_CONTAINER (options_window), vbox);
	gtk_widget_show(vbox);

	gtk_widget_show(options_window);

}
