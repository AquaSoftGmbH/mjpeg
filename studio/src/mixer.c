/*
 * mixer.c, mixer module
 *
 * Copyright (C) 1997 Rasca, Berlin
 * EMail: thron@gmx.de
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 * example which sets the volume of all devices to zero:
 *
 * int id, no_of_devs, i;
 *
 * id = mixer_init ("/dev/mixer");
 * if (id > 0) {
 *     no_of_devs = mixer_num_of_devs (id);
 *     for (i = 0; i < no_of_devs; i++) {
 *         mixer_set_vol_left (id, i, 0);
 *         if (mixer_is_stereo (id, i)) {
 *             mixer_set_vol_right (id, i, 0);
 *         }
 *     }
 *     mixer_fini (id);
 * }
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#ifdef ALSA	/* not supported until now */
#	include <sys/asound.h>
#endif
#ifdef OSS
#	include <sys/soundcard.h>
#endif
#include "mixer.h"

/* if you have more than two soundcards in your system, adjust the
 * following accordingly..
 */
#define MAX_MIXER	2
#define MIXER_VER	"V0.5"

typedef struct {
    int id;				/* 0 if not used */
    int no_of_devs;		/* number of devices */
	int stereo_devs;
	int rec_mask;
	int rec_source;
	int caps;
	int exclusive_input;
	int device[SOUND_MIXER_NRDEVICES];	/* valid devices */
	int levels[SOUND_MIXER_NRDEVICES];
    char *file;			/* file name of the device */
} MIXER;

static MIXER mixer[MAX_MIXER];
static int must_init = 1;

/*
 * mixer_init()
 * returns an id for the following functions. on the first call
 * it is '1', so you don't remember if you just use one device.
 * just call all other functions with '1' as the first argument.
 */
int
mixer_init (char *mixer_dev) {
	int i, id, md;
	int dev_mask = 0;

	if (!mixer_dev) {
		return (0);
	}
	if (must_init) {
		/* just called the first time */
		must_init = 0;
		for (i = 0; i < MAX_MIXER; i++) {
			mixer[i].id =0;
		}
	}
	/* find a free structure */
	for (i = 0; i < MAX_MIXER; i++) {
		if (mixer[i].id == 0) {
			mixer[i].id = i+1;
			id = i;
			goto CONT;
		}
	}
	return (0);
	CONT:

	mixer[id].file = (char *) malloc (strlen (mixer_dev)+1);
	strcpy (mixer[id].file, mixer_dev);

	md = open (mixer[id].file, O_RDWR, 0);
	if (md == -1) {
		perror (mixer[id].file);
		return (0);
	}
	if (ioctl (md, SOUND_MIXER_READ_DEVMASK, &dev_mask) == -1) {
		perror("SOUND_MIXER_READ_DEVMASK");
		close(md);
		return (0);
	}

	if (ioctl (md, SOUND_MIXER_READ_RECMASK, &mixer[id].rec_mask) == -1) {
		perror("SOUND_MIXER_READ_RECMASK");
		close(md);
		return (0);
	}

	if (ioctl (md, SOUND_MIXER_READ_RECSRC, &mixer[id].rec_source) == -1) {
		perror("SOUND_MIXER_READ_RECSRC");
		close(md);
		return (0);
	}

	if (ioctl (md, SOUND_MIXER_READ_STEREODEVS, &mixer[id].stereo_devs) == -1) {
		perror("SOUND_MIXER_READ_STEREODEVS");
		close(md);
		return (0);
	}
	if (ioctl (md, SOUND_MIXER_READ_CAPS, &mixer[id].caps) == -1) {
		perror("SOUND_MIXER_READ_CAPS");
		close(md);
		return (0);
	}
	mixer[id].exclusive_input = mixer[id].caps & SOUND_CAP_EXCL_INPUT;

	mixer[id].no_of_devs =0;
	for (i = 0; i < SOUND_MIXER_NRDEVICES; i++)  {
		if ((1 << i) & dev_mask) {
#		ifdef DEBUG2
			fprintf (stderr, "mixer_init(): %d\n", i);
#		endif
			mixer[id].device[mixer[id].no_of_devs] = i;
			mixer[id].no_of_devs++;
			ioctl (md, MIXER_READ(i), &mixer[id].levels[i]);
		}
	}
	close (md);
	return (id+1);
}

/*
 * mixer_fini()
 * call this if the mixer is not needed any longer
 */
int
mixer_fini (int mixer_id) {
	mixer[mixer_id-1].id = 0;	/* mark it as not used */
	free (mixer[mixer_id-1].file);
	return (1);
}


/*
 */
const char
*mixer_get_label (int mixer_id, int dev) {
	int id;
	char *labels[SOUND_MIXER_NRDEVICES] = SOUND_DEVICE_LABELS;

	if (dev <= SOUND_MIXER_NRDEVICES) {
		id = mixer[mixer_id-1].device[dev];
		return (labels[id]);
	}
	return (NULL);
}


/*
 */
const char
*mixer_get_name (int mixer_id, int dev) {
	int id;
	char *names[SOUND_MIXER_NRDEVICES] = SOUND_DEVICE_NAMES;

	if (dev <= SOUND_MIXER_NRDEVICES) {
		id = mixer[mixer_id-1].device[dev];
		return (names[id]);
	}
	return (NULL);
}


/*
 */
int
mixer_is_stereo (int mixer_id, int dev) {
	int id;
	if (dev <= SOUND_MIXER_NRDEVICES) {
		id = mixer[mixer_id-1].device[dev];
		return (!!((1 << id) & mixer[mixer_id-1].stereo_devs));
	}
	return (0);
}

/*
 */
int
mixer_get_vol_left (int mixer_id, int dev) {
	int id, md;
	if (dev <= SOUND_MIXER_NRDEVICES) {
		id = mixer[mixer_id-1].device[dev];
		md = open (mixer[mixer_id-1].file, O_RDWR, 0);
		if (md < 0) {
			return (-1);
		}
		ioctl (md, MIXER_READ(id), &mixer[mixer_id-1].levels[id]);
		close (md);
		return (mixer[mixer_id-1].levels[id] & 0x7F);
	}
	return (-1);
}


/*
 */
int
mixer_get_vol_right (int mixer_id, int dev) {
	int id;
	if (dev <= SOUND_MIXER_NRDEVICES) {
		id = mixer[mixer_id-1].device[dev];
		return ((mixer[mixer_id-1].levels[id] >> 8) & 0x7F);
	}
	return (-1);
}


/*
 * set the mixer volume of the left channel, returns the
 * new value or -1 on failure
 */
int
mixer_set_vol_left (int mixer_id, int dev, int val) {
	int id, fd, level;
	if (dev <= SOUND_MIXER_NRDEVICES) {
		id = mixer[mixer_id-1].device[dev];
		level = mixer[mixer_id-1].levels[id] =
			(mixer[mixer_id-1].levels[id] & 0x7F00) | val;
		fd = open (mixer[mixer_id-1].file, O_RDWR, 0);
			if (fd < 0)
				return (-1);
#ifdef DEBUG2
		fprintf (stderr, "mixer_set_vol_left() left=%d right=%d\n",
					level & 0xff, (level >> 8)& 0xff);
#endif
		ioctl (fd, MIXER_WRITE(id), &level);
		close (fd);
		return (level & 0x7F);
	}
	return (-1);
}


/*
 */
int mixer_set_vol_right (int mixer_id, int dev, int val) {
	int id, md, level;
	if (dev <= SOUND_MIXER_NRDEVICES) {
		id = mixer[mixer_id-1].device[dev];
		level = mixer[mixer_id-1].levels[id] = 
			(mixer[mixer_id-1].levels[id] & 0x007F) | (val << 8);
		md = open (mixer[mixer_id-1].file, O_RDWR, 0);
		if (md == -1)
			return (0);
		ioctl (md, MIXER_WRITE(id), &level);
		close (md);
		return ((level >>8) & 0x7F);
	}
	return (-1);
}


/*
 */
int
mixer_is_rec_dev (int mixer_id, int dev) {
	int id;
	if (dev <= SOUND_MIXER_NRDEVICES) {
		id = mixer[mixer_id-1].device[dev];
		return ((1 << id) & mixer[mixer_id-1].rec_mask);
	}
	return (0);
}


/*
 * check if recording source is active
 */
int
mixer_is_rec_on (int mixer_id, int dev) {
	int id;
	if (dev <= SOUND_MIXER_NRDEVICES) {
		id = mixer[mixer_id-1].device[dev];
		return (((1 << id) & mixer[mixer_id-1].rec_source)>>id);
	}
	return (0);
}


/*
 */
int
mixer_set_rec (int mixer_id, int dev, int boolval) {
	int id, md, recsrc;
	if (dev <= SOUND_MIXER_NRDEVICES) {
		id = mixer[mixer_id-1].device[dev];
		if (boolval) {
			recsrc = mixer[mixer_id-1].rec_source | (boolval << id);
		} else {
			recsrc = mixer[mixer_id-1].rec_source & ~(1 << id);
		}
		md = open (mixer[mixer_id-1].file, O_RDWR, 0);
		if (md == -1)
			return (0);
		ioctl (md, SOUND_MIXER_WRITE_RECSRC, &recsrc);
		close (md);
		mixer[mixer_id-1].rec_source = recsrc;
#ifdef	DEBUG2
		fprintf (stderr, "mixer_set_rec(%d, %d): %d\n", dev, boolval, recsrc);
#endif
		return (1);
	}
	return (0);
}


/*
 */
int
mixer_num_of_devs (int mixer_id) {
	if (mixer_id > MAX_MIXER)
		return (0);
	return (mixer[mixer_id-1].no_of_devs);
}

/*
 */
int
mixer_get_dev_by_name (int mixer_id, char *dev_name) {
	int i;
	const char *name;
	char *names[SOUND_MIXER_NRDEVICES] = SOUND_DEVICE_NAMES;

	if (mixer_id > MAX_MIXER)
		return (-1);
	for (i = 0; i < mixer[mixer_id-1].no_of_devs; i++) {
		name = names[mixer[mixer_id-1].device[i]];
		if (strcmp (name, dev_name) == 0) {
#ifdef DEBUG2
			fprintf (stderr, "%s %d\n", name, i);
#endif
			return (i);
		}
	}
	return (-1);
}

/*
 * only one channel as recording source?
 * return: 1  yes
 *         0  no
 *        -1  error
 */
int
mixer_exclusive_input (int mixer_id)
{
	if (mixer_id > MAX_MIXER)
		return (-1);
	return (mixer[mixer_id-1].exclusive_input);
}

