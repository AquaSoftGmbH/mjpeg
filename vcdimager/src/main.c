/*
    $Id$

    Copyright (C) 2000 Herbert Valerio Riedel <hvr@gnu.org>

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

#include <glib.h>

#include <errno.h>
#include <fcntl.h>
#include <popt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/types.h>
#include <unistd.h>

#include "bytesex.h"
#include "cd_sector.h"
#include "directory.h"
#include "iso9660.h"
#include "logging.h"
#include "mpeg.h"
#include "salloc.h"
#include "transfer.h"
#include "vcdfiles.h"

/* some parameters */
#define PRE_TRACK_GAP 2*75
#define PRE_DATA_GAP  30
#define POST_DATA_GAP 45

/* defaults */
#define DEFAULT_CUE_FILE      "videocd.cue"
#define DEFAULT_BIN_FILE      "videocd.bin"
#define DEFAULT_VOLUME_LABEL  "VideoCD"
#define DEFAULT_CDI_DIR       CDIDIR

/* global stuff kept as a singleton makes for less typing effort :-) 
 */
static struct {
  guint32 image_iso_size; /* in sectors */
  guint32 image_size; /* complete image */
  FILE *image_fd;
  gchar *image_fname;

  FILE *cue_fd;
  gchar *cue_fname;

  gchar *cdi_path;
  gboolean disable_cdi_flag;

  gchar *add_files_path;

  GArray *track_infos;

  gchar *volume_label;

  gboolean verbose_flag;
} gl; /* global */

/* short cut's */
#define TRACK(n) (g_array_index(gl.track_infos, track_info_t, n))
#define NTRACKS  (gl.track_infos->len)

/****************************************************************************/

static void
global_init(void)
{
  gl.track_infos = g_array_new(FALSE, TRUE, sizeof(track_info_t));
  
  gl.image_iso_size = 0;
  gl.image_fname = DEFAULT_BIN_FILE;
  gl.cue_fname = DEFAULT_CUE_FILE;

  gl.volume_label = DEFAULT_VOLUME_LABEL;
  gl.cdi_path = DEFAULT_CDI_DIR;
  gl.add_files_path = NULL;

  logging_init();

  salloc_init();

  directory_init();
}

static void 
global_done(void)
{
  gint n;

  directory_done();

  salloc_done();

  logging_done();
  
  for(n = 0;n < NTRACKS;n++)
    g_free(TRACK(n).fname);

  g_array_free(gl.track_infos, TRUE);
}

/****************************************************************************/

static void
write_iso_track(void)
{
  gint n;
  FILE *fd = gl.image_fd; /* just a shortcut */
  const gchar zero[ISO_BLOCKSIZE] = { 0, };
  gchar buf[ISO_BLOCKSIZE] = { 0, };

  if(sector_alloc(0, 16) == SECTOR_NIL) /* pre-alloc 16 blocks of
					   ISO9660 required silence */
    g_assert_not_reached();

  for(n = 0;n < 16; n++)
    write_mode2_sector(fd, zero, n, 0, 0, SM_DATA, 0);

  if(sector_alloc(75, 75) == SECTOR_NIL) /* keep karaoke sectors blank
                                            -- well... guess I'm too
                                            paranoid :) */
    g_assert_not_reached();

  for(n = 75;n < 150; n++)
    write_mode2_sector(fd, zero, n, 0, 0, SM_DATA, 0);

  if(sector_alloc(152,75-2) == SECTOR_NIL) /* keep rest of vcd sector
                                              blank -- paranoia */
    g_assert_not_reached();

  for(n = 152;n < 225; n++)
    write_mode2_sector(fd, zero, n, 0, 0, SM_DATA, 0);

  {
    guint32
      pvd_sec = sector_alloc(16, 2),      /* pre-alloc descriptors, PVD */  /* EOR */
      evd_sec = pvd_sec+1,                /* EVD */                         /* EOR+EOF */
      dir_secs = sector_alloc(18, 75-18),
      info_sec = sector_alloc(150, 1),    /* INFO.VCD */                    /* EOF */
      entries_sec = sector_alloc(151, 1), /* ENTRIES.VCD */                 /* EOF */

      ptl_sec = SECTOR_NIL, /* EOR+EOF */
      ptm_sec = SECTOR_NIL; /* EOR+EOF */
    
    g_assert(dir_secs != SECTOR_NIL);
    g_assert(pvd_sec != SECTOR_NIL);
    g_assert(info_sec != SECTOR_NIL);
    g_assert(entries_sec != SECTOR_NIL);

    /* directory_mkdir("CDDA"); */
    directory_mkdir("CDI");
    directory_mkdir("EXT");
    /* directory_mkdir("KARAOKE"); */
    directory_mkdir("MPEGAV");
    directory_mkdir("SEGMENT"); 
    directory_mkdir("VCD");

    directory_mkfile("VCD/ENTRIES.VCD;1", entries_sec, ISO_BLOCKSIZE, FALSE, 0);
    directory_mkfile("VCD/INFO.VCD;1", info_sec, ISO_BLOCKSIZE, FALSE, 0);

    /* allocate and write cdi files if needed */

    if(!gl.disable_cdi_flag)
    {
      gchar *fname = NULL;

      vcd_verbose("CD-i support enabled");

      fname = g_strconcat(gl.cdi_path, "/", "cdi_imag.rtf", NULL);
      insert_file_raw_mode2(gl.image_fd, fname, "CDI/CDI_IMAG.RTF;1");
      g_free(fname);

      fname = g_strconcat(gl.cdi_path, "/", "cdi_text.fnt", NULL);
      insert_file_mode2_form1(gl.image_fd, fname, "CDI/CDI_TEXT.FNT;1");
      g_free(fname);

      fname = g_strconcat(gl.cdi_path, "/", "cdi_vcd.app", NULL);
      insert_file_mode2_form1(gl.image_fd, fname, "CDI/CDI_VCD.APP;1");
      g_free(fname);
    } else
      vcd_info("CD-i support disabled");

    /* add custom user files */

    if(gl.add_files_path)
      insert_tree_mode2_form1(gl.image_fd, gl.add_files_path);
    
    /* calculate iso size -- after this point no sector shall be
       allocated anymore */

    gl.image_iso_size = MAX(MIN_ISO_SIZE, salloc_get_highest());

    vcd_verbose("iso9660: highest alloced sector is %d (using %d as isosize)", 
		salloc_get_highest(), gl.image_iso_size);

    /* fix up start extents, now that we have the iso size */

    for(n = 0;n < NTRACKS;n++)
      TRACK(n).extent += gl.image_iso_size;

    gl.image_size += gl.image_iso_size;

    /* after this point the ISO9660's size is frozen */

    for(n = 0; n < NTRACKS; n++) {
      gchar *avseq_pathname =
	g_strdup_printf("MPEGAV/AVSEQ%2.2d.DAT;1", n+1);

      directory_mkfile(avseq_pathname, TRACK(n).extent+PRE_DATA_GAP,
		       TRACK(n).length*ISO_BLOCKSIZE, TRUE, n+1);

      g_free(avseq_pathname);
    }

    /* some information/debugging stuff */

    vcd_verbose("track %d, ISO9660, blocks %d," 
		" track starts at sector %d",
		1, gl.image_iso_size, 0);

    for(n = 0;n < NTRACKS;n++) {
      mpeg_info_t* info = &(TRACK(n).mpeg_info);
      gchar *norm_str = NULL;

      switch(info->norm) {
      case MPEG_NORM_PAL:
	norm_str = g_strdup("PAL (352x288/25fps)");
	break;
      case MPEG_NORM_NTSC:
	norm_str = g_strdup("NTSC (352x240/30fps)");
	break;
      case MPEG_NORM_FILM:
	norm_str = g_strdup("FILM (352x240/24fps)");
	break;
      case MPEG_NORM_OTHER:
	norm_str = g_strdup_printf("UNKNOWN (%dx%d/%2.2ffps)",
				   info->hsize, info->vsize, info->frate);
	break;
      }

      if(info->norm == MPEG_NORM_OTHER)
	vcd_warning("track %d: mpeg encoding `%s' may not be VideoCD compliant", n+2, norm_str);

      if(info->bitrate > 1152000)
	vcd_warning("track %d: bitrate (%d bits/sec) too high for VideoCD", n+2, info->bitrate);

      vcd_verbose("track %d: file `%s', %s, sectors %d," 
		  " track starts at sector %d",
		  n+2, TRACK(n).fname, norm_str, 
		  TRACK(n).length, TRACK(n).extent);
      
      g_free(norm_str);

    }
      
    vcd_verbose("track %d ends at sector %d", n+1, gl.image_size);

    /* write directory entries, pathtables and volume descriptors */

    vcd_verbose("writing track 1 (ISO9660)...");

    {
      guint dirs_size = directory_get_size();
      gchar dir_buf[ISO_BLOCKSIZE*dirs_size];
      gchar ptm_buf[ISO_BLOCKSIZE], ptl_buf[ISO_BLOCKSIZE], 
	    pvd_buf[ISO_BLOCKSIZE], evd_buf[ISO_BLOCKSIZE];

      ptl_sec = dir_secs + dirs_size;
      ptm_sec = ptl_sec+1;

      if(ptm_sec >= 75)
	vcd_error("directory section to big\n");

      directory_dump_entries(dir_buf, 18);
      directory_dump_pathtables(ptl_buf, ptm_buf);

      /* write directory tree */

      for(n = 0;n < dirs_size;n++)
	write_mode2_sector(fd, dir_buf+(ISO_BLOCKSIZE*n), 18+n, 0, 0,
			   n+1 != dirs_size ? SM_DATA : SM_DATA|SM_EOR|SM_EOF,
			   0);
      
      write_mode2_sector(fd, ptl_buf, ptl_sec, 0, 0, SM_DATA|SM_EOR|SM_EOF, 0);
      write_mode2_sector(fd, ptm_buf, ptm_sec, 0, 0, SM_DATA|SM_EOR|SM_EOF, 0);

      /* blank remaining sectors */

      for(n = ptm_sec+1;n < 75;n++)
	write_mode2_sector(fd, zero, n, 0, 0, SM_DATA, 0);
      
      /* set PVD at last... */
      set_iso_pvd(pvd_buf, gl.volume_label, gl.image_iso_size, dir_buf,
		  ptl_sec, ptm_sec, pathtable_get_size(ptm_buf));
    
      set_iso_evd(evd_buf);

      write_mode2_sector(fd, pvd_buf, pvd_sec, 0, 0, SM_DATA|SM_EOR, 0);
      write_mode2_sector(fd, evd_buf, evd_sec, 0, 0, SM_DATA|SM_EOR|SM_EOF, 0);
    }

    /* fill VCD relevant files with data */
    set_info_vcd(buf, NTRACKS, (track_info_t*) gl.track_infos->data); 
    write_mode2_sector(fd, buf, info_sec, 0, 0, SM_DATA|SM_EOF, 0);
      
    set_entries_vcd(buf, NTRACKS, (track_info_t*) gl.track_infos->data); 
    write_mode2_sector(fd, buf, entries_sec, 0, 0, SM_DATA|SM_EOF, 0);

    /* blank unalloced tracks */
      
    while((n = sector_alloc(SECTOR_NIL, 1)) < gl.image_iso_size)
      write_mode2_sector(fd, zero, n, 0, 0, SM_DATA, 0);
  }
}

static void
write_data_tracks()
{
  FILE *fd = gl.image_fd;
  guint32 lastsect = gl.image_iso_size;
  gint t, n;

  for(t = 0; t < NTRACKS; t++) {
    gchar buf[M2F2_SIZE] = { 0, };
    for(n = 0; n < PRE_TRACK_GAP;n++)
      write_mode2_sector(fd, buf, lastsect++, 0, 0, SM_FORM2, 0);
    
    g_assert(lastsect == TRACK(t).extent);
      
    for(n = 0; n < PRE_DATA_GAP;n++)
      write_mode2_sector(fd, buf, lastsect++, t+1, 0, SM_FORM2|SM_REALT, 0);

    {
      FILE *mfd = fopen(TRACK(t).fname, "rb");
      gchar buf2[M2F2_SIZE] = { 0, };

      if(!mfd) 
	vcd_error("fopen(`%s'): %s", TRACK(t).fname, g_strerror(errno));

      vcd_verbose("writing track %d...", t+2);

      for(n = 0; n < TRACK(t).length;n++) {
	guint8 ci = 0, sm = 0;

	fread(buf2, sizeof(buf2), 1, mfd);

	if(ferror(mfd))
	  vcd_error("fread(): %s", g_strerror(errno));
	else if(feof(mfd)) 
	  vcd_error("fread(): unexpected EOF encountered...");

	switch(mpeg_type(buf2)) {
	case MPEG_VIDEO:
	  sm = SM_FORM2|SM_REALT|SM_VIDEO;
	  ci = CI_NTSC;
	  break;
	case MPEG_AUDIO:
	  sm = SM_FORM2|SM_REALT|SM_AUDIO;
	  ci = CI_AUD;
	  break;
	case MPEG_UNKNOWN:
	  sm = SM_FORM2|SM_REALT;
	  ci = 0;
	  break;
	case MPEG_INVALID:
	  vcd_error("invalid mpeg packet found in `%s' at packet %d"
		    " -- please fix this mpeg file!", 
		    TRACK(t).fname ,n);
	  break;
	default:
	  g_assert_not_reached();
	}

	if(n == TRACK(t).length-1)
	  sm |= SM_EOR;

	write_mode2_sector(fd, buf2, lastsect++, t+1, 1, sm, ci);
      }

      g_assert(n == TRACK(t).length);

      fclose(mfd);
    }

    for(n = 0; n < POST_DATA_GAP;n++)
      write_mode2_sector(fd, buf, lastsect++, t+1, 0, SM_FORM2|SM_REALT, 0);
  }
}

static void
analyze_mpeg_files()
{
  guint lastsect = 0; /* fix up later when iso size is known */
  gint n, j;

  g_return_if_fail(NTRACKS > 0);

  for(n = 0; n < NTRACKS; n++) {
    FILE *fd = fopen(TRACK(n).fname, "rb");
    struct stat statbuf;
    glong size;

    if(!fd) 
      vcd_error("fopen(`%s'): %s", TRACK(n).fname, g_strerror(errno));

    if(fstat(fileno(fd), &statbuf))
      vcd_error("fstat(`%s'): %s", TRACK(n).fname, g_strerror(errno));

    size = statbuf.st_size;
    
    lastsect += PRE_TRACK_GAP;

    if(size % M2F2_SIZE)
      vcd_error("file `%s' is not padded to %d byte " 
		"-- use some mpeg tool to fix it", 
		 TRACK(n).fname, M2F2_SIZE);
	
    size /= M2F2_SIZE;

    if(size+PRE_TRACK_GAP+PRE_DATA_GAP+POST_DATA_GAP < MIN_TRACK_SIZE)
      vcd_warning("file `%s' is shorter that the minimum allowed track length",
		  TRACK(n).fname);

    TRACK(n).length = size;
    TRACK(n).extent = lastsect;
    
    lastsect += PRE_DATA_GAP;

    lastsect += size;

    lastsect += POST_DATA_GAP;

    for(j = 0;;j++) {
      gchar buf[M2F2_SIZE] = { 0, };

      fread(buf, sizeof(buf), 1, fd);

      if(ferror(fd))
	vcd_error("fread(): %s", g_strerror(errno));
      else if(feof(fd)) 
	vcd_error("fread(): unexpected EOF encountered...");

      if(mpeg_analyze_start_seq(buf, &(TRACK(n).mpeg_info)))
	break;

      if(j > 16) {
	vcd_warning("could not determine mpeg format of `%s', maybe stream not ok?", TRACK(n).fname);
	break;
      }
	
    }

    fclose(fd);
  }

  gl.image_size = lastsect;

}

static void
cue_start(const gchar fname[])
{
  if(fprintf(gl.cue_fd, "FILE \"%s\" BINARY\r\n", fname) == EOF) 
    vcd_error("fprintf(): %s", g_strerror(errno));
}

static void
cue_track(guint8 num, guint32 extent)
{
  guint8 f = extent % 75;
  guint8 s = (extent / 75) % 60;
  guint8 m = (extent / 75) / 60;
  
  fprintf(gl.cue_fd, "  TRACK %2.2d MODE2/2352\r\n", num);
  fprintf(gl.cue_fd, "    INDEX %2.2d %2.2d:%2.2d:%2.2d\r\n", 1, m, s, f);

  if(ferror(gl.cue_fd)) 
    vcd_error("fprintf(): %s", g_strerror(errno));
}

	
int
main(int argc, const char *argv[])
{
  gint n = 0;
  
  g_set_prgname(argv[0]);

  global_init();

  {
    const gchar **args = NULL;
    gint opt = 0;

    struct poptOption optionsTable[] = {

      { "volume-label", 'l', POPT_ARG_STRING, &gl.volume_label, 0,
	"specify volume label for video cd (default: '" DEFAULT_VOLUME_LABEL "')", "LABEL" },

      { "cue-file", 'c', POPT_ARG_STRING, &gl.cue_fname, 0, 
	"specify cue file for output (default: '" DEFAULT_CUE_FILE "')", "FILE" },
      { "bin-file", 'b', POPT_ARG_STRING, &gl.image_fname, 0, 
	"specify bin file for output (default: '" DEFAULT_BIN_FILE "')", "FILE" },
      
      { "cdi-dir", 0, POPT_ARG_STRING, &gl.cdi_path, 0, 
	"find CD-i support files in DIR (default: `" DEFAULT_CDI_DIR "')", "DIR" },

      { "disable-cdi", 'e', POPT_ARG_NONE, &gl.disable_cdi_flag, 0, "disable CD-i support (enabled by default)" },

      { "add-files", 0, POPT_ARG_STRING, &gl.add_files_path, 0,
	"add files from given DIR directory to ISO9660 fs track", "DIR" },

      { "verbose", 'v', POPT_ARG_NONE, &gl.verbose_flag, 0, "be verbose" },

      { "version", 'V', POPT_ARG_NONE, NULL, 1,
	"display version and copyright information and exit" },
      
      POPT_AUTOHELP
      { NULL, 0, 0, NULL, 0 }
    };

    poptContext optCon = poptGetContext(NULL, argc, argv, optionsTable, 0);
      
    while((opt = poptGetNextOpt(optCon)) != -1) 
      switch(opt) {
      case 1:
	g_print("GNU VCDImager " VERSION "\n\n"
		"Copyright (c) 2000 Herbert Valerio Riedel <hvr@gnu.org>\n\n"
		"VCDImager may be distributed under the terms of the GNU General Public Licence;\n"
		"For details, see the file `COPYING', which is included in the VCDImager\n"
		"distribution. There is no warranty, to the extent permitted by law.\n");
	exit(EXIT_SUCCESS);
	break;
      default:
	vcd_error("error while parsing command line - try --help");
	break;
      }
    
    if((args = poptGetArgs(optCon)) == NULL)
      vcd_error("error: need at least one data track as argument -- try --help");

    for(n = 0;args[n];n++);

    if(n > CD_MAX_TRACKS-1)
      vcd_error("error: maximal number of supported mpeg tracks (%d) reached",
		CD_MAX_TRACKS-1);

    g_array_set_size(gl.track_infos, n);

    for(n=0;args[n];n++) 
      TRACK(n).fname = g_strdup(args[n]);

    poptFreeContext(optCon);
  }

  logging_set_verbose(gl.verbose_flag);

  /* open output files */

  gl.image_fd = fopen(gl.image_fname, "wb");
  if(!gl.image_fd) 
    vcd_error("fopen(`%s'): %s", gl.image_fname, g_strerror(errno));

  gl.cue_fd = fopen(gl.cue_fname, "w");
  if(!gl.cue_fd) 
    vcd_error("fopen(`%s'): %s", gl.cue_fname, g_strerror(errno));

  analyze_mpeg_files();

  write_iso_track();

  write_data_tracks();

  /* create toc */
  cue_start(gl.image_fname);
  cue_track(1, 0);
  for(n=0;n < NTRACKS;n++)
    cue_track(n+2, TRACK(n).extent);

  /* cleanup up */

  fclose(gl.image_fd);
  fclose(gl.cue_fd);

  global_done();

  g_print("finished ok, image created with %d sectors (%d bytes)\n",
	  gl.image_size, gl.image_size*CDDA_SIZE);

  return EXIT_SUCCESS;
}

