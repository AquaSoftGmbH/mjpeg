#ifndef PRIVATE_H
#define PRIVATE_H

/* ================================= structures */

#define HEADER_LENGTH 8
#define MAXTRACKS 1024

#include "codecs.h"
#include "sizes.h"

typedef struct
{
	float values[9];
} quicktime_matrix_t;


typedef struct
{
	int version;
	long flags;
	unsigned long creation_time;
	unsigned long modification_time;
	int track_id;
	long reserved1;
	long duration;
	char reserved2[8];
	int layer;
	int alternate_group;
	float volume;
	long reserved3;
	quicktime_matrix_t matrix;
	float track_width;
	float track_height;
} quicktime_tkhd_t;


typedef struct
{
	long seed;
	long flags;
	long size;
	short int *alpha;
	short int *red;
	short int *green;
	short int *blue;
} quicktime_ctab_t;



/* ===================== sample table ======================== // */



/* sample description */

typedef struct
{
	int motion_jpeg_quantization_table;
} quicktime_mjqt_t;


typedef struct
{
	int motion_jpeg_huffman_table;
} quicktime_mjht_t;


typedef struct
{
	char format[4];
	char reserved[6];
	int data_reference;

/* common to audio and video */
	int version;
	int revision;
	char vendor[4];

/* video description */
	long temporal_quality;
	long spatial_quality;
	int width;
	int height;
	float dpi_horizontal;
	float dpi_vertical;
	long data_size;
	int frames_per_sample;
	char compressor_name[32];
	int depth;
	int ctab_id;
	quicktime_ctab_t ctab;
	float gamma;
	int fields;    /* 0, 1, or 2 */
	int field_dominance;   /* 0 - unknown     1 - top first     2 - bottom first */
	quicktime_mjqt_t mjqt;
	quicktime_mjht_t mjht;

/* audio description */
	int channels;
	int sample_size;
	int compression_id;
	int packet_size;
	float sample_rate;
} quicktime_stsd_table_t;


typedef struct
{
	int version;
	long flags;
	long total_entries;
	quicktime_stsd_table_t *table;
} quicktime_stsd_t;


/* time to sample */
typedef struct
{
	long sample_count;
	long sample_duration;
} quicktime_stts_table_t;

typedef struct
{
	int version;
	long flags;
	long total_entries;
	quicktime_stts_table_t *table;
} quicktime_stts_t;


/* sync sample */
typedef struct
{
	long sample;
} quicktime_stss_table_t;

typedef struct
{
	int version;
	long flags;
	long total_entries;
	quicktime_stss_table_t *table;
} quicktime_stss_t;


/* sample to chunk */
typedef struct
{
	long chunk;
	long samples;
	long id;
} quicktime_stsc_table_t;

typedef struct
{
	int version;
	long flags;
	long total_entries;
	
	long entries_allocated;
	quicktime_stsc_table_t *table;
} quicktime_stsc_t;


/* sample size */
typedef struct
{
	long size;
} quicktime_stsz_table_t;

typedef struct
{
	int version;
	long flags;
	long sample_size;
	long total_entries;

	long entries_allocated;    /* used by the library for allocating a table */
	quicktime_stsz_table_t *table;
} quicktime_stsz_t;


/* chunk offset */
typedef struct
{
	long offset;
} quicktime_stco_table_t;

typedef struct
{
	int version;
	long flags;
	long total_entries;
	
	long entries_allocated;    /* used by the library for allocating a table */
	quicktime_stco_table_t *table;
} quicktime_stco_t;


/* sample table */
typedef struct
{
	int version;
	long flags;
	quicktime_stsd_t stsd;
	quicktime_stts_t stts;
	quicktime_stss_t stss;
	quicktime_stsc_t stsc;
	quicktime_stsz_t stsz;
	quicktime_stco_t stco;
} quicktime_stbl_t;

/* data reference */

typedef struct
{
	long size;
	char type[4];
	int version;
	long flags;
	char *data_reference;
} quicktime_dref_table_t;

typedef struct
{
	int version;
	long flags;
	long total_entries;
	quicktime_dref_table_t *table;
} quicktime_dref_t;

/* data information */

typedef struct
{
	quicktime_dref_t dref;
} quicktime_dinf_t;

/* video media header */

typedef struct
{
	int version;
	long flags;
	int graphics_mode;
	int opcolor[3];
} quicktime_vmhd_t;


/* sound media header */

typedef struct
{
	int version;
	long flags;
	int balance;
	int reserved;
} quicktime_smhd_t;

/* handler reference */

typedef struct
{
	int version;
	long flags;
	char component_type[4];
	char component_subtype[4];
	long component_manufacturer;
	long component_flags;
	long component_flag_mask;
	char component_name[256];
} quicktime_hdlr_t;

/* media information */

typedef struct
{
	int is_video;
	int is_audio;
	quicktime_vmhd_t vmhd;
	quicktime_smhd_t smhd;
	quicktime_stbl_t stbl;
	quicktime_hdlr_t hdlr;
	quicktime_dinf_t dinf;
} quicktime_minf_t;


/* media header */

typedef struct
{
	int version;
	long flags;
	unsigned long creation_time;
	unsigned long modification_time;
	long time_scale;
	long duration;
	int language;
	int quality;
} quicktime_mdhd_t;


/* media */

typedef struct
{
	quicktime_mdhd_t mdhd;
	quicktime_minf_t minf;
	quicktime_hdlr_t hdlr;
} quicktime_mdia_t;

/* edit list */
typedef struct
{
	long duration;
	long time;
	float rate;
} quicktime_elst_table_t;

typedef struct
{
	int version;
	long flags;
	long total_entries;

	quicktime_elst_table_t *table;
} quicktime_elst_t;

typedef struct
{
	quicktime_elst_t elst;
} quicktime_edts_t;


typedef struct
{
	quicktime_tkhd_t tkhd;
	quicktime_mdia_t mdia;
	quicktime_edts_t edts;
} quicktime_trak_t;


typedef struct
{
	int version;
	long flags;
	unsigned long creation_time;
	unsigned long modification_time;
	long time_scale;
	long duration;
	float preferred_rate;
	float preferred_volume;
	char reserved[10];
	quicktime_matrix_t matrix;
	long preview_time;
	long preview_duration;
	long poster_time;
	long selection_time;
	long selection_duration;
	long current_time;
	long next_track_id;
} quicktime_mvhd_t;

typedef struct
{
	char *copyright;
	int copyright_len;
	char *name;
	int name_len;
	char *info;
	int info_len;
} quicktime_udta_t;


typedef struct
{
	int total_tracks;

	quicktime_mvhd_t mvhd;
	quicktime_trak_t *trak[MAXTRACKS];
	quicktime_udta_t udta;
	quicktime_ctab_t ctab;
} quicktime_moov_t;

typedef struct
{
	long start;
	long size;
} quicktime_mdat_t;


typedef struct
{
	long start;      /* byte start in file */
	long end;        /* byte endpoint in file */
	long size;       /* byte size for writing */
	char type[4];
} quicktime_atom_t;

/* table of pointers to every track */
typedef struct
{
	quicktime_trak_t *track; /* real quicktime track corresponding to this table */
	int channels;            /* number of audio channels in the track */
	long current_position;   /* current sample in output file */
	long current_chunk;      /* current chunk in output file */

	void *codec;
} quicktime_audio_map_t;

typedef struct
{
	quicktime_trak_t *track;
	long current_position;
	long current_chunk;

/* Array of pointers to frames of raw data when caching frames. */
	unsigned char **frame_cache;
	long frames_cached;

	void *codec;
} quicktime_video_map_t;

/* file descriptor passed to all routines */

typedef struct
{
	FILE *stream;
	long total_length;
	quicktime_mdat_t mdat;
	quicktime_moov_t moov;
	int rd;
	int wr;
	int use_avi;

/* mapping of audio channels to movie tracks */
/* one audio map entry exists for each channel */
	int total_atracks;
	quicktime_audio_map_t *atracks;

/* mapping of video tracks to movie tracks */
	int total_vtracks;
	quicktime_video_map_t *vtracks;

/* for begining and ending frame writes where the user wants to write the  */
/* file descriptor directly */
	long offset;

/* Number of processors at our disposal */
	int cpus;

/* I/O */
	long file_position;      /* Current position of file descriptor */

/* Read ahead buffer */
	long preload_size;      /* Enables preload when nonzero. */
	char *preload_buffer;
	long preload_start;     /* Start of preload_buffer in file */
	long preload_end;       /* End of preload buffer in file */
	long preload_ptr;       /* Offset of preload_start in preload_buffer */

/* Parameters for frame currently being decoded */
	int do_scaling;
	int in_x, in_y, in_w, in_h, out_w, out_h;
	int color_model;
} quicktime_t;

typedef struct
{
	int (*delete_vcodec)(quicktime_video_map_t *vtrack);
	int (*delete_acodec)(quicktime_audio_map_t *atrack);
	int (*decode_video)(quicktime_t *file, 
				unsigned char **row_pointers, 
				int track);
	int (*encode_video)(quicktime_t *file, 
				unsigned char **row_pointers, 
				int track);
	int (*decode_audio)(quicktime_t *file, 
				QUICKTIME_INT16 *output_i, 
				float *output_f, 
				long samples, 
				int track,
				int channel);
	int (*encode_audio)(quicktime_t *file, 
				QUICKTIME_INT16 **input_i, 
				float **input_f, 
				int track, 
				long samples);
	int (*test_colormodel)(quicktime_t *file, 
		int colormodel, 
		int track);

	void *priv;
} quicktime_codec_t;

#endif
