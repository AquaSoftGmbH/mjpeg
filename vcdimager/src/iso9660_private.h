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

#define ISO_VD_PRIMARY          1
#define ISO_VD_END              255

#define ISO_STANDARD_ID         "CD001"
#define ISO_VERSION             1

#define ISO_XA_MARKER_STRING    "CD-XA001"
#define ISO_XA_MARKER_OFFSET    1024

struct iso_volume_descriptor {
  guint8  type __attribute__((packed));                      /* 711 */
  gchar   id[5] __attribute__((packed));
  guint8  version __attribute__((packed));                   /* 711 */
  gchar   data[2041] __attribute__((packed));
};

struct iso_primary_descriptor {
  guint8  type __attribute__((packed));                      /* 711 */
  gchar   id[5] __attribute__((packed));
  guint8  version __attribute__((packed));                   /* 711 */
  gchar   unused1[1] __attribute__((packed));
  gchar   system_id[32] __attribute__((packed));             /* achars */
  gchar   volume_id[32] __attribute__((packed));             /* dchars */
  gchar   unused2[8] __attribute__((packed));
  guint64 volume_space_size __attribute__((packed));         /* 733 */
  gchar   escape_sequences[32] __attribute__((packed));
  guint32 volume_set_size __attribute__((packed));           /* 723 */
  guint32 volume_sequence_number __attribute__((packed));    /* 723 */
  guint32 logical_block_size __attribute__((packed));        /* 723 */
  guint64 path_table_size __attribute__((packed));           /* 733 */
  guint32 type_l_path_table __attribute__((packed));         /* 731 */
  guint32 opt_type_l_path_table __attribute__((packed));     /* 731 */
  guint32 type_m_path_table __attribute__((packed));         /* 732 */
  guint32 opt_type_m_path_table  __attribute__((packed));    /* 732 */
  gchar   root_directory_record[34] __attribute__((packed)); /* 9.1 */
  gchar   volume_set_id[128] __attribute__((packed));        /* dchars */
  gchar   publisher_id[128] __attribute__((packed));         /* achars */
  gchar   preparer_id[128] __attribute__((packed));          /* achars */
  gchar   application_id[128] __attribute__((packed));       /* achars */
  gchar   copyright_file_id[37] __attribute__((packed));     /* 7.5 dchars */
  gchar   abstract_file_id[37] __attribute__((packed));      /* 7.5 dchars */
  gchar   bibliographic_file_id[37] __attribute__((packed)); /* 7.5 dchars */
  gchar   creation_date[17] __attribute__((packed));         /* 8.4.26.1 */
  gchar   modification_date[17] __attribute__((packed));     /* 8.4.26.1 */
  gchar   expiration_date[17] __attribute__((packed));       /* 8.4.26.1 */
  gchar   effective_date[17] __attribute__((packed));        /* 8.4.26.1 */
  guint8  file_structure_version __attribute__((packed));    /* 711 */
  gchar   unused4[1] __attribute__((packed));
  gchar   application_data[512] __attribute__((packed));
  gchar   unused5[653] __attribute__((packed));
};

struct iso_path_table {
  guint8  name_len __attribute__((packed));     /* 711 */
  guint8  xa_len __attribute__((packed));       /* 711 */
  guint32 extent __attribute__((packed));       /* 731/732 */
  guint16 parent __attribute__((packed));       /* 721/722 */
  gchar   name[0] __attribute__((packed));
};

struct iso_directory_record {
  guint8  length __attribute__((packed));                 /* 711 */
  guint8  ext_attr_length __attribute__((packed));        /* 711 */
  guint64 extent __attribute__((packed));                 /* 733 */
  guint64 size __attribute__((packed));                   /* 733 */
  guint8  date[7] __attribute__((packed));                /* 7 by 711 */
  guint8  flags __attribute__((packed));
  guint8  file_unit_size __attribute__((packed));         /* 711 */
  guint8  interleave __attribute__((packed));             /* 711 */
  guint32 volume_sequence_number __attribute__((packed)); /* 723 */
  guint8  name_len __attribute__((packed));               /* 711 */
  gchar   name[0] __attribute__((packed));
};
