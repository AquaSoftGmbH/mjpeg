#ifndef FUNCPROTOS_H
#define FUNCPROTOS_H

// atom handling routines
int quicktime_atom_reset(quicktime_atom_t *atom);
int quicktime_atom_read_header(quicktime_t *file, quicktime_atom_t *atom);
int quicktime_atom_is(quicktime_atom_t *atom, char *type);
long quicktime_atom_read_size(char *data);
int quicktime_atom_read_type(char *data, char *output);
int quicktime_atom_skip(quicktime_t *file, quicktime_atom_t *atom);
int quicktime_atom_write_header(quicktime_t *file, quicktime_atom_t *atom, char *text);
int quicktime_atom_write_footer(quicktime_t *file, quicktime_atom_t *atom);

// utilities for reading data types
int quicktime_read_data(quicktime_t *file, char *data, int size);
int quicktime_write_data(quicktime_t *file, char *data, int size);
int quicktime_read_pascal(quicktime_t *file, char *data);
int quicktime_write_pascal(quicktime_t *file, char *data);
float quicktime_read_fixed32(quicktime_t *file);
int quicktime_write_fixed32(quicktime_t *file, float number);
float quicktime_read_fixed16(quicktime_t *file);
int quicktime_write_fixed16(quicktime_t *file, float number);
long quicktime_read_int32(quicktime_t *file);
int quicktime_write_int32(quicktime_t *file, long number);
long quicktime_read_int24(quicktime_t *file);
int quicktime_write_int24(quicktime_t *file, long number);
int quicktime_read_int16(quicktime_t *file);
int quicktime_write_int16(quicktime_t *file, int number);
int quicktime_read_char(quicktime_t *file);
int quicktime_write_char(quicktime_t *file, char x);
int quicktime_read_char32(quicktime_t *file, char *string);
int quicktime_write_char32(quicktime_t *file, char *string);
int quicktime_copy_char32(char *output, char *input);
long quicktime_position(quicktime_t *file);
int quicktime_set_position(quicktime_t *file, long position);
int quicktime_print_chars(char *desc, char *input, int len);
int quicktime_match_32(char *input, char *output);
int quicktime_test_position(quicktime_t *file);

// initializers for every atom
int quicktime_matrix_init(quicktime_matrix_t *matrix);
int quicktime_edts_init_table(quicktime_edts_t *edts);
int quicktime_edts_init(quicktime_edts_t *edts);
int quicktime_elst_init(quicktime_elst_t *elst);
int quicktime_elst_init_all(quicktime_elst_t *elst);
int quicktime_elst_table_init(quicktime_elst_table_t *table); // initialize a table
int quicktime_tkhd_init(quicktime_tkhd_t *tkhd);
int quicktime_tkhd_init_video(quicktime_t *file, quicktime_tkhd_t *tkhd, int frame_w, int frame_h);
int quicktime_ctab_init(quicktime_ctab_t *ctab);
int quicktime_mjqt_init(quicktime_mjqt_t *mjqt);
int quicktime_mjht_init(quicktime_mjht_t *mjht);
int quicktime_stsd_table_init(quicktime_stsd_table_t *table);
int quicktime_stsd_init(quicktime_stsd_t *stsd);
int quicktime_stsd_init_table(quicktime_stsd_t *stsd);
int quicktime_stsd_init_video(quicktime_t *file, quicktime_stsd_t *stsd, int frame_w, int frame_h, float frame_rate, char *compression);
int quicktime_stsd_init_audio(quicktime_t *file, quicktime_stsd_t *stsd, int channels, int sample_rate, int bits, char *compressor);
int quicktime_stts_init(quicktime_stts_t *stts);
int quicktime_stts_init_table(quicktime_stts_t *stts);
int quicktime_stts_init_video(quicktime_t *file, quicktime_stts_t *stts, int time_scale, float frame_rate);
int quicktime_stts_init_audio(quicktime_t *file, quicktime_stts_t *stts, int sample_rate);
int quicktime_stss_init(quicktime_stss_t *stss);
int quicktime_stsc_init(quicktime_stsc_t *stsc);
int quicktime_stsc_init_video(quicktime_t *file, quicktime_stsc_t *stsc);
int quicktime_stsc_init_audio(quicktime_t *file, quicktime_stsc_t *stsc, int sample_rate);
int quicktime_stsz_init(quicktime_stsz_t *stsz);
int quicktime_stsz_init_video(quicktime_t *file, quicktime_stsz_t *stsz);
int quicktime_stsz_init_audio(quicktime_t *file, quicktime_stsz_t *stsz, int channels, int bits);
int quicktime_stco_init(quicktime_stco_t *stco);
int quicktime_stco_init_common(quicktime_t *file, quicktime_stco_t *stco);
int quicktime_stbl_init(quicktime_stbl_t *tkhd);
int quicktime_stbl_init_video(quicktime_t *file, quicktime_stbl_t *stbl, int frame_w, int frame_h, int time_scale, float frame_rate, char *compressor);
int quicktime_stbl_init_audio(quicktime_t *file, quicktime_stbl_t *stbl, int channels, int sample_rate, int bits, char *compressor);
int quicktime_vmhd_init(quicktime_vmhd_t *vmhd);
int quicktime_vmhd_init_video(quicktime_t *file, quicktime_vmhd_t *vmhd, int frame_w, int frame_h, float frame_rate);
int quicktime_smhd_init(quicktime_smhd_t *smhd);
int quicktime_dref_table_init(quicktime_dref_table_t *table);
int quicktime_dref_init_all(quicktime_dref_t *dref);
int quicktime_dref_init(quicktime_dref_t *dref);
int quicktime_dinf_init_all(quicktime_dinf_t *dinf);
int quicktime_dinf_init(quicktime_dinf_t *dinf);
int quicktime_minf_init(quicktime_minf_t *minf);
int quicktime_minf_init_video(quicktime_t *file, quicktime_minf_t *minf, int frame_w, int frame_h, int time_scale, float frame_rate, char *compressor);
int quicktime_minf_init_audio(quicktime_t *file, quicktime_minf_t *minf, int channels, int sample_rate, int bits, char *compressor);
int quicktime_mdhd_init(quicktime_mdhd_t *mdhd);
int quicktime_mdhd_init_video(quicktime_t *file, quicktime_mdhd_t *mdhd, int frame_w, int frame_h, float frame_rate);
int quicktime_mdhd_init_audio(quicktime_t *file, quicktime_mdhd_t *mdhd, int channels, int sample_rate, int bits, char *compressor);
int quicktime_mdia_init(quicktime_mdia_t *mdia);
int quicktime_mdia_init_video(quicktime_t *file, quicktime_mdia_t *mdia, int frame_w, int frame_h, float frame_rate, char *compressor);
int quicktime_mdia_init_audio(quicktime_t *file, quicktime_mdia_t *mdia, int channels, int sample_rate, int bits, char *compressor);
int quicktime_trak_init(quicktime_trak_t *trak);
int quicktime_trak_init_video(quicktime_t *file, quicktime_trak_t *trak, int frame_w, int frame_h, float frame_rate, char *compressor);
int quicktime_trak_init_audio(quicktime_t *file, quicktime_trak_t *trak, int channels, int sample_rate, int bits, char *compressor);
int quicktime_udta_init(quicktime_udta_t *udta);
int quicktime_mvhd_init(quicktime_mvhd_t *mvhd);
int quicktime_mhvd_init_video(quicktime_t *file, quicktime_mvhd_t *mvhd, float frame_rate);
int quicktime_moov_init(quicktime_moov_t *moov);
int quicktime_mdat_init(quicktime_mdat_t *mdat);
int quicktime_init(quicktime_t *file);
int quicktime_hdlr_init(quicktime_hdlr_t *hdlr);
int quicktime_hdlr_init_video(quicktime_hdlr_t *hdlr);
int quicktime_hdlr_init_audio(quicktime_hdlr_t *hdlr);
int quicktime_hdlr_init_data(quicktime_hdlr_t *hdlr);

// deleters for every atom
int quicktime_matrix_delete(quicktime_matrix_t *matrix);
int quicktime_edts_delete(quicktime_edts_t *edts);
int quicktime_elst_delete(quicktime_elst_t *elst);
int quicktime_elst_table_delete(quicktime_elst_table_t *table);
int quicktime_tkhd_delete(quicktime_tkhd_t *tkhd);
int quicktime_ctab_delete(quicktime_ctab_t *ctab);
int quicktime_mjqt_delete(quicktime_mjqt_t *mjqt);
int quicktime_mjht_delete(quicktime_mjht_t *mjht);
int quicktime_stsd_table_delete(quicktime_stsd_table_t *table);
int quicktime_stsd_delete(quicktime_stsd_t *stsd);
int quicktime_stts_delete(quicktime_stts_t *stts);
int quicktime_stss_delete(quicktime_stss_t *stss);
int quicktime_stsc_delete(quicktime_stsc_t *stsc);
int quicktime_stsz_delete(quicktime_stsz_t *stsz);
int quicktime_stco_delete(quicktime_stco_t *stco);
int quicktime_stbl_delete(quicktime_stbl_t *stbl);
int quicktime_vmhd_delete(quicktime_vmhd_t *vmhd);
int quicktime_smhd_delete(quicktime_smhd_t *smhd);
int quicktime_dref_table_delete(quicktime_dref_table_t *table);
int quicktime_dref_delete(quicktime_dref_t *dref);
int quicktime_dinf_delete(quicktime_dinf_t *dinf);
int quicktime_minf_delete(quicktime_minf_t *minf);
int quicktime_mdhd_delete(quicktime_mdhd_t *mdhd);
int quicktime_mdia_delete(quicktime_mdia_t *mdia);
int quicktime_trak_delete(quicktime_trak_t *trak);
quicktime_trak_t* quicktime_add_track(quicktime_moov_t *moov);
int quicktime_udta_delete(quicktime_udta_t *udta);
int quicktime_mvhd_delete(quicktime_mvhd_t *mvhd);
int quicktime_moov_delete(quicktime_moov_t *moov);
int quicktime_mdat_delete(quicktime_mdat_t *moov);
int quicktime_delete(quicktime_t *file);
int quicktime_hdlr_delete(quicktime_hdlr_t *hdlr);


// dumpers for every atom
int quicktime_matrix_dump(quicktime_matrix_t *matrix);
int quicktime_edts_dump(quicktime_edts_t *edts);
int quicktime_elst_dump(quicktime_elst_t *elst);
int quicktime_elst_table_dump(quicktime_elst_table_t *table);
int quicktime_tkhd_dump(quicktime_tkhd_t *tkhd);
int quicktime_ctab_dump(quicktime_ctab_t *ctab);
int quicktime_mjqt_dump(quicktime_mjqt_t *mjqt);
int quicktime_mjht_dump(quicktime_mjht_t *mjht);
int quicktime_stsd_table_dump(void *minf_ptr, quicktime_stsd_table_t *table);
int quicktime_stsd_audio_dump(quicktime_stsd_table_t *table);
int quicktime_stsd_video_dump(quicktime_stsd_table_t *table);
int quicktime_stsd_dump(void *minf_ptr, quicktime_stsd_t *stsd);
int quicktime_stts_dump(quicktime_stts_t *stts);
int quicktime_stss_dump(quicktime_stss_t *stss);
int quicktime_stsc_dump(quicktime_stsc_t *stsc);
int quicktime_stsz_dump(quicktime_stsz_t *stsz);
int quicktime_stco_dump(quicktime_stco_t *stco);
int quicktime_stbl_dump(void *minf_ptr, quicktime_stbl_t *stbl);
int quicktime_vmhd_dump(quicktime_vmhd_t *vmhd);
int quicktime_smhd_dump(quicktime_smhd_t *smhd);
int quicktime_dref_table_dump(quicktime_dref_table_t *table);
int quicktime_dref_dump(quicktime_dref_t *dref);
int quicktime_dinf_dump(quicktime_dinf_t *dinf);
int quicktime_minf_dump(quicktime_minf_t *minf);
int quicktime_mdhd_dump(quicktime_mdhd_t *mdhd);
int quicktime_mdia_dump(quicktime_mdia_t *mdia);
int quicktime_trak_dump(quicktime_trak_t *trak);
int quicktime_udta_dump(quicktime_udta_t *udta);
int quicktime_dump_mvhd(quicktime_mvhd_t *mvhd);
int quicktime_moov_dump(quicktime_moov_t *moov);
int quicktime_hdlr_dump(quicktime_hdlr_t *hdlr);

// storage for every type of atom
int quicktime_read_moov(quicktime_t *file, quicktime_moov_t *moov, quicktime_atom_t *parent_atom);
int quicktime_write_moov(quicktime_t *file, quicktime_moov_t *moov);
int quicktime_read_mdat(quicktime_t *file, quicktime_mdat_t *mdat, quicktime_atom_t *parent_atom);
int quicktime_write_mdat(quicktime_t *file, quicktime_mdat_t *mdat);
int quicktime_read_matrix(quicktime_t *file, quicktime_matrix_t *matrix);
int quicktime_write_matrix(quicktime_t *file, quicktime_matrix_t *matrix);
int quicktime_read_ctab(quicktime_t *file, quicktime_ctab_t *ctab);
int quicktime_read_trak(quicktime_t *file, quicktime_trak_t *trak, quicktime_atom_t *parent_atom);
int quicktime_write_trak(quicktime_t *file, quicktime_trak_t *trak, long moov_time_scale);
int quicktime_read_edts(quicktime_t *file, quicktime_edts_t *edts, quicktime_atom_t *edts_atom);
int quicktime_write_edts(quicktime_t *file, quicktime_edts_t *edts, long duration);
int quicktime_read_elst(quicktime_t *file, quicktime_elst_t *elst);
int quicktime_write_elst(quicktime_t *file, quicktime_elst_t *elst, long duration);
int quicktime_read_elst_table(quicktime_t *file, quicktime_elst_table_t *table);
int quicktime_write_elst_table(quicktime_t *file, quicktime_elst_table_t *table, long duration);
int quicktime_read_tkhd(quicktime_t *file, quicktime_tkhd_t *tkhd);
int quicktime_write_tkhd(quicktime_t *file, quicktime_tkhd_t *tkhd);
int quicktime_read_mvhd(quicktime_t *file, quicktime_mvhd_t *mvhd);
int quicktime_write_mvhd(quicktime_t *file, quicktime_mvhd_t *mvhd);
int quicktime_read_mdhd(quicktime_t *file, quicktime_mdhd_t *mdhd);
int quicktime_read_mdia(quicktime_t *file, quicktime_mdia_t *mdia, quicktime_atom_t *parent_atom);
int quicktime_write_mdia(quicktime_t *file, quicktime_mdia_t *mdia);
int quicktime_read_dref_table(quicktime_t *file, quicktime_dref_table_t *table);
int quicktime_write_dref_table(quicktime_t *file, quicktime_dref_table_t *table);
int quicktime_read_dref(quicktime_t *file, quicktime_dref_t *dref);
int quicktime_write_dref(quicktime_t *file, quicktime_dref_t *dref);
int quicktime_read_dinf(quicktime_t *file, quicktime_dinf_t *dinf, quicktime_atom_t *dinf_atom);
int quicktime_write_dinf(quicktime_t *file, quicktime_dinf_t *dinf);
int quicktime_read_minf(quicktime_t *file, quicktime_minf_t *minf, quicktime_atom_t *parent_atom);
int quicktime_read_vmhd(quicktime_t *file, quicktime_vmhd_t *vmhd);
int quicktime_write_vmhd(quicktime_t *file, quicktime_vmhd_t *vmhd);
int quicktime_read_smhd(quicktime_t *file, quicktime_smhd_t *smhd);
int quicktime_write_smhd(quicktime_t *file, quicktime_smhd_t *smhd);
int quicktime_read_stbl(quicktime_t *file, quicktime_minf_t *minf, quicktime_stbl_t *stbl, quicktime_atom_t *parent_atom);
int quicktime_write_stbl(quicktime_t *file, quicktime_minf_t *minf, quicktime_stbl_t *stbl);
int quicktime_read_stsd(quicktime_t *file, quicktime_minf_t *minf, quicktime_stsd_t *stsd);
int quicktime_read_stsd_table(quicktime_t *file, quicktime_minf_t *minf, quicktime_stsd_table_t *table);
int quicktime_write_stsd_table(quicktime_t *file, quicktime_minf_t *minf, quicktime_stsd_table_t *table);
int quicktime_read_stsd_audio(quicktime_t *file, quicktime_stsd_table_t *table, quicktime_atom_t *parent_atom);
int quicktime_write_stsd_audio(quicktime_t *file, quicktime_stsd_table_t *table);
int quicktime_read_stsd_video(quicktime_t *file, quicktime_stsd_table_t *table, quicktime_atom_t *parent_atom);
int quicktime_write_stsd_video(quicktime_t *file, quicktime_stsd_table_t *table);
int quicktime_read_stts(quicktime_t *file, quicktime_stts_t *stts);
int quicktime_write_stts(quicktime_t *file, quicktime_stts_t *stts);
int quicktime_read_stss(quicktime_t *file, quicktime_stss_t *stss);
int quicktime_write_stss(quicktime_t *file, quicktime_stss_t *stss);
int quicktime_read_stsc(quicktime_t *file, quicktime_stsc_t *stsc);
int quicktime_write_stsc(quicktime_t *file, quicktime_stsc_t *stsc);
int quicktime_read_stsz(quicktime_t *file, quicktime_stsz_t *stsz);
int quicktime_write_stsz(quicktime_t *file, quicktime_stsz_t *stsz);
int quicktime_read_stco(quicktime_t *file, quicktime_stco_t *stco);
int quicktime_write_stco(quicktime_t *file, quicktime_stco_t *stco);
int quicktime_read_hdlr(quicktime_t *file, quicktime_hdlr_t *hdlr);
int quicktime_write_hdlr(quicktime_t *file, quicktime_hdlr_t *hdlr);
int quicktime_read_udta(quicktime_t *file, quicktime_udta_t *udta, quicktime_atom_t *udta_atom);
int quicktime_write_udta(quicktime_t *file, quicktime_udta_t *udta);
int quicktime_read_udta_string(quicktime_t *file, char **string, int *size);
int quicktime_write_udta_string(quicktime_t *file, char *string, int size);

// routines for codecs
int quicktime_init_vcodecs(quicktime_video_map_t *vtrack);
int quicktime_init_acodecs(quicktime_audio_map_t *atrack);
int quicktime_delete_vcodecs(quicktime_video_map_t *vtrack);
int quicktime_delete_acodecs(quicktime_audio_map_t *atrack);
int quicktime_codecs_flush(quicktime_t *file);
// Most codecs don't specify the actual number of bits on disk in the stbl.
// Convert the samples to the number of bytes for reading depending on the codec.
long quicktime_samples_to_bytes(quicktime_trak_t *track, long samples);


// chunks always start on 1
// samples start on 0

// queries for every atom
// the starting sample in the given chunk
long quicktime_sample_of_chunk(quicktime_trak_t *trak, long chunk);

int quicktime_set_udta_string(char **string, int *size, char *new_string);

// number of samples in the chunk
long quicktime_chunk_samples(quicktime_trak_t *trak, long chunk);

// the chunk containing the sample
int quicktime_chunk_of_sample(long *chunk_sample, long *chunk, quicktime_trak_t *trak, long sample);

// the byte offset from mdat start of the chunk
long quicktime_chunk_to_offset(quicktime_trak_t *trak, long chunk);

// the chunk of any offset from mdat start
long quicktime_offset_to_chunk(long *chunk_offset, quicktime_trak_t *trak, long offset);

// the total number of samples in the track depending on the access mode
long quicktime_track_samples(quicktime_t *file, quicktime_trak_t *trak);

// length of track in timescale
int quicktime_trak_duration(quicktime_trak_t *trak, long *duration, long *timescale);

// total bytes between the two samples
long quicktime_sample_range_size(quicktime_trak_t *trak, long chunk_sample, long sample);

// update the position pointers in all the tracks after a set_position
int quicktime_update_positions(quicktime_t *file);

// converting between mdat offsets to samples
long quicktime_sample_to_offset(quicktime_trak_t *trak, long sample);
long quicktime_offset_to_sample(quicktime_trak_t *trak, long offset);

quicktime_trak_t* quicktime_add_trak(quicktime_moov_t *moov);
int quicktime_delete_trak(quicktime_moov_t *moov);
int quicktime_get_timescale(float frame_rate);

// update all the tables after writing a buffer
// set sample_size to 0 if no sample size should be set
int quicktime_update_tables(quicktime_t *file, 
							quicktime_trak_t *trak, 
							long offset, 
							long chunk, 
							long sample, 
							long samples, 
							long sample_size);
int quicktime_init_video_map(quicktime_video_map_t *vtrack, quicktime_trak_t *trak);
int quicktime_delete_video_map(quicktime_video_map_t *vtrack);
int quicktime_init_audio_map(quicktime_audio_map_t *atrack, quicktime_trak_t *trak);
int quicktime_delete_audio_map(quicktime_audio_map_t *atrack);
int quicktime_update_stco(quicktime_stco_t *stco, long chunk, long offset);
int quicktime_update_stsc(quicktime_stsc_t *stsc, long chunk, long samples);
int quicktime_update_stsz(quicktime_stsz_t *stsz, long sample, long sample_size);
int quicktime_trak_fix_counts(quicktime_t *file, quicktime_trak_t *trak);
int quicktime_update_durations(quicktime_moov_t *moov);
unsigned long quicktime_current_time();


// for making streamable
int quicktime_shift_offsets(quicktime_moov_t *moov, long offset);


#endif
