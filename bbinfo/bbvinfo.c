/*************************************************************************
  bbVINFO by Brent Beyeler, beyeler@home.com
*************************************************************************/

#include "bits.h"
#include <string.h>

#define FULL_PICTURE_START_CODE   0x00000100
#define FULL_USER_DATA_START_CODE 0x000001B2
#define FULL_SEQUENCE_HEADER_CODE 0x000001B3
#define FULL_SEQUENCE_ERROR_CODE  0x000001B4
#define FULL_EXTENSION_START_CODE 0x000001B5
#define FULL_SEQUENCE_END_CODE    0x000001B7
#define FULL_GROUP_START_CODE     0x000001B8

#define START_CODE_PREFIX    0x000001
#define PICTURE_START_CODE   0x00
#define USER_DATA_START_CODE 0xB2
#define SEQUENCE_HEADER_CODE 0xB3
#define SEQUENCE_ERROR_CODE  0xB4
#define EXTENSION_START_CODE 0xB5
#define SEQUENCE_END_CODE    0xB7
#define GROUP_START_CODE     0xB8

#define SEQUENCE_EXTENSION_ID                   1
#define SEQUENCE_DISPLAY_EXTENSION_ID           2
#define QUANT_MATRIX_EXTENSION_ID               3
#define SEQUENCE_SCALABLE_EXTENSION_ID          5
#define PICTURE_DISPLAY_EXTENSION_ID            7
#define PICTURE_CODING_EXTENSION_ID             8
#define PICTURE_SPATIAL_SCALABLE_EXTENSION_ID   9
#define PICTURE_TEMPORAL_SCALABLE_EXTENSION_ID 10

#define DATA_PARTITIONING    0
#define SPATIAL_SCALABILITY  1
#define SNR_SCALABILITY		  2
#define TEMPORAL_SCALABILITY 3


int verbose_level;
int col, mpeg2;
char oldLevel3[80];
int progressive_sequence, picture_structure, repeat_first_field;
int sequence_headers, sequence_extensions, user_data_bytes;
int vertical_size, horizontal_size, sequence_scalable_extension_present;
int scalable_mode, mb_row, mb_width, previous_macroblock_address;
int mb_column, macroblock_address;
int slice_vertical_position, slice_vertical_position_extension;

void zprintf(int level, char *tmpStr, int value)
{
  if (level <= verbose_level)
    printf(tmpStr, value);
}

unsigned int nextbits(int i)
{
  return look_ahead(i);
}


void next_start_code()
{
  if (seek_sync(START_CODE_PREFIX, 24))
    return;
  else
    exit(1);
}


void sequence_header()
{
  int i;

  sequence_headers++;
  zprintf(1, "\n\nsequence_header_code = 0x000001%02X\n", getbits(8));
  horizontal_size = getbits(12);
  mb_width = horizontal_size / 16;
  if (mb_width % 16)
    mb_width++;
  zprintf(2, "  horizontal_size_value = %d\n", horizontal_size);
  vertical_size = getbits(12);
  zprintf(2, "  vertical_size_value = %d\n", vertical_size);
  zprintf(2, "  aspect_ratio_information = %d\n", getbits(4));
  zprintf(2, "  frame_rate_code = %d\n", getbits(4));
  zprintf(2, "  bit_rate_value = %d\n", getbits(18));
  zprintf(2, "  marker bit = %d\n", get1bit());
  zprintf(2, "  vbv_buffer_size_value = %d\n", getbits(10));
  zprintf(2, "  constrained_parameters_flag = %d\n", get1bit());
  i = get1bit();
  zprintf(2, "  load_intra_quantiser_matrix = %d\n", i);
  if (i)
  {
    for (i = 0; i < 64; i++)
      getbits(8);
    zprintf(2, "    read %d intra_quantiser_matrix bytes = %d\n", i);
  }
  i = get1bit();
  zprintf(2, "  load_non_intra_quantiser_matrix = %d\n", i);
  if (i)
  {
    for (i = 0; i < 64; i++)
      getbits(8);
    zprintf(2, "    read %d non_intra_quantiser_matrix bytes = %d\n", i);
  }
  next_start_code();
}


void sequence_extension()
{
  sequence_extensions++;
  zprintf(1, "\nextension_start_code = 0x000001%02X\n", getbits(8));
  zprintf(2, "  sequence_extension_id = %d\n", getbits(4));
  zprintf(2, "  profile_and_level_indication = %d\n", getbits(8));
  progressive_sequence = get1bit();
  zprintf(2, "  progressive_sequence = %d\n", progressive_sequence);
  zprintf(2, "  chroma_format = %d\n", getbits(2));
  zprintf(2, "  horizontal_size_extension = %d\n", getbits(2));
  zprintf(2, "  vertical_size_extension = %d\n", getbits(2));
  zprintf(2, "  bit_rate_extension = %d\n", getbits(12));
  zprintf(2, "  marker bit = %d\n", get1bit());
  zprintf(2, "  vbv_buffer_size_extension = %d\n", getbits(8));
  zprintf(2, "  low_delay = %d\n", get1bit());
  zprintf(2, "  frame_rate_extension_n = %d\n", getbits(2));
  zprintf(2, "  frame_rate_extension_d = %d\n", getbits(5));
  next_start_code();
}

void sequence_display_extension()
{
  int i;

  zprintf(1, "  sequence_display_extension_id = %01X\n", getbits(4));
  zprintf(2, "  video_format = %d\n", getbits(3));
  i = get1bit();
  zprintf(2, "  colour_description = %d\n", i);
  if (i)
  {
    zprintf(2, "  colour_primaries = %d\n", getbits(8));
    zprintf(2, "  transfer_characteristics = %d\n", getbits(8));
    zprintf(2, "  matrix_coefficients = %d\n", getbits(8));
  }
  zprintf(2, "  display_horizontal_size = %d\n", getbits(14));
  zprintf(2, "  marker bit = %d\n", get1bit());
  zprintf(2, "  display_vertical_size = %d\n", getbits(14));
  next_start_code();
}

void sequence_scalable_extension()
{
  int i;

  sequence_scalable_extension_present = 1;
  zprintf(1, "  sequence_scalable_extension_id = %01X\n", getbits(4));
  scalable_mode = getbits(2);
  zprintf(2, "  scalable_mode = %d\n", scalable_mode);
  zprintf(2, "  layer_id = %d\n", getbits(4));
  if (scalable_mode == SPATIAL_SCALABILITY)
  {
    zprintf(2, "  lower_layer_prediction_horizontal_size = %d\n", getbits(14));
    zprintf(2, "  marker bit = %d\n", get1bit());
    zprintf(2, "  lower_layer_prediction_vertical_size = %d\n", getbits(14));
    zprintf(2, "  horizontal_subsampling_factor_m = %d\n", getbits(5));
    zprintf(2, "  horizontal_subsampling_factor_n = %d\n", getbits(5));
    zprintf(2, "  vertical_subsampling_factor_m = %d\n", getbits(5));
    zprintf(2, "  vertical_subsampling_factor_n = %d\n", getbits(5));
  }
  if (scalable_mode == TEMPORAL_SCALABILITY)
  {
    i = get1bit();
    zprintf(2, "  picture_mux_enable = %d\n", i);
    if (i)
      zprintf(2, "  mux_to_progressive_sequence = %d\n", get1bit());
    zprintf(2, "  picture_mux_order = %d\n", getbits(3));
    zprintf(2, "  picture_mux_factor = %d\n", getbits(3));
  }
  next_start_code();
}

void quant_matrix_extension()
{
  int i;

  zprintf(1, "  quant_matrix_extension_id = %01X\n", getbits(4));
  i = get1bit();
  zprintf(2, "  load_intra_quantiser_matrix = %d\n", i);
  if (i)
  {
    for (i = 0; i < 64; i++)
      getbits(8);
    zprintf(2, "    read %d intra_quantiser_matrix bytes = %d\n", i);
  }
  i = get1bit();
  zprintf(2, "  load_non_intra_quantiser_matrix = %d\n", i);
  if (i)
  {
    for (i = 0; i < 64; i++)
      getbits(8);
    zprintf(2, "    read %d non_intra_quantiser_matrix bytes = %d\n", i);
  }
  i = get1bit();
  zprintf(2, "  load_chroma_intra_quantiser_matrix = %d\n", i);
  if (i)
  {
    for (i = 0; i < 64; i++)
      getbits(8);
    zprintf(2, "    read %d chroma_intra_quantiser_matrix bytes = %d\n", i);
  }
  i = get1bit();
  zprintf(2, "  load_chroma_non_intra_quantiser_matrix = %d\n", i);
  if (i)
  {
    for (i = 0; i < 64; i++)
      getbits(8);
    zprintf(2, "    read %d chroma_non_intra_quantiser_matrix bytes = %d\n", i);
  }
  next_start_code();
}

void picture_display_extension()
{
  int i, number_of_frame_centre_offsets;

  zprintf(1, "  picture_display_extension_id = %01X\n", getbits(4));
  if ((progressive_sequence == 1) || (picture_structure == 3))
	 number_of_frame_centre_offsets = 1;
  else
	 if (repeat_first_field == 1)
		number_of_frame_centre_offsets = 3;
	 else
		number_of_frame_centre_offsets = 2;
  for (i = 0; i < number_of_frame_centre_offsets; i++)
  {
    zprintf(2, "  frame_centre_horizontal_offset = %d\n", getbits(16));
    zprintf(2, "  marker bit = %d\n", get1bit());
    zprintf(2, "  frame_centre_vertical_offset = %d\n", getbits(16));
    zprintf(2, "  marker bit = %d\n", get1bit());
  }
  next_start_code();
}

void picture_spatial_scalable_extension()
{
  zprintf(1, "  picture_spatial_scalable_extension_id = %01X\n", getbits(4));
  zprintf(2, "  lower_layer_temporal_reference = %d\n", getbits(10));
  zprintf(2, "  marker bit = %d\n", get1bit());
  zprintf(2, "  lower_layer_horizontal_offset = %d\n", getbits(15));
  zprintf(2, "  marker bit = %d\n", get1bit());
  zprintf(2, "  lower_layer_vertical_offset = %d\n", getbits(15));
  zprintf(2, "  spatial_temporal_weight_code_table_index = %d\n", getbits(2));
  zprintf(2, "  lower_layer_progressive_frame = %d\n", get1bit());
  zprintf(2, "  lower_layer_deinterlaced_field_select = %d\n", get1bit());
  next_start_code();
}

void picture_temporal_scalable_extension()
{
  zprintf(1, "  picture_temporal_scalable_extension_id = %01X\n", getbits(4));
  zprintf(2, "  reference_select_code = %d\n", getbits(2));
  zprintf(2, "  forward_temporal_reference = %d\n", getbits(10));
  zprintf(2, "  marker bit = %d\n", get1bit());
  zprintf(2, "  backward_temporal_reference = %d\n", getbits(10));
  next_start_code();
}


void extension_data(int i)
{
  while (nextbits(8) == EXTENSION_START_CODE)
  {
    zprintf(1, "\nextension_start_code = 0x000001%02X\n", getbits(8));
    if (i == 0)
    {
      if (nextbits(4) == SEQUENCE_DISPLAY_EXTENSION_ID)
        sequence_display_extension();
      if (nextbits(4) == SEQUENCE_SCALABLE_EXTENSION_ID)
        sequence_scalable_extension();
    }
    if (i == 2)
    {
      if (nextbits(4) == QUANT_MATRIX_EXTENSION_ID)
        quant_matrix_extension();
      if (nextbits(4) == PICTURE_DISPLAY_EXTENSION_ID)
        picture_display_extension();
      if (nextbits(4) == PICTURE_SPATIAL_SCALABLE_EXTENSION_ID)
        picture_spatial_scalable_extension();
      if (nextbits(4) == PICTURE_TEMPORAL_SCALABLE_EXTENSION_ID)
        picture_temporal_scalable_extension();
    }
  }
}

void user_data()
{
  zprintf(1, "\nuser_data_start_code = 0x000001%02X\n", getbits(8));
  while (nextbits(24) != START_CODE_PREFIX)
  {
    getbits(8);
    user_data_bytes++;
  }
  next_start_code();
}

void extension_and_user_data(int i)
{
  while ((nextbits(8) == EXTENSION_START_CODE) ||
         (nextbits(8) == USER_DATA_START_CODE))
  {
    if (i != 1)
      if (nextbits(8) == EXTENSION_START_CODE)
        extension_data(i);
    if (nextbits(8) == USER_DATA_START_CODE)
      user_data();
  }
}

void group_of_pictures_header()
{
  zprintf(1, "\ngroup_start_code = 0x000001%02X\n", getbits(8));
  zprintf(2, "  drop_frame_flag = %d\n", get1bit());
  zprintf(2, "  time_code_hours = %d\n", getbits(5));
  zprintf(2, "  time_code_minutes = %d\n", getbits(6));
  zprintf(2, "  marker_bit = %d\n", get1bit());
  zprintf(2, "  time_code_seconds = %d\n", getbits(6));
  zprintf(2, "  time_code_pictures = %d\n", getbits(6));
  zprintf(2, "  closed_gop = %d\n", get1bit());
  zprintf(2, "  broken_link = %d\n", get1bit());
  next_start_code();
}

void picture_header()
{
  int i;

  zprintf(1, "\npicture_header = 0x000001%02X\n", getbits(8));
  zprintf(2, "  temporal_reference = %d\n", getbits(10));
  i = getbits(3);
  zprintf(2, "  picture_coding_type = %d\n", i);
  zprintf(2, "  vbv_delay = %d\n", getbits(16));
  if (i == 2 || i == 3)
  {
    zprintf(2, "  full_pel_forward_vector = %d\n", get1bit());
    zprintf(2, "  forward_f_code = %d\n", getbits(3));
  }
  if (i == 3)
  {
    zprintf(2, "  full_pel_backward_vector = %d\n", get1bit());
    zprintf(2, "  backward_f_code = %d\n", getbits(3));
  }
  while (nextbits(1) == 1)
  {
    zprintf(2, "  extra_bit_picture = %d\n", get1bit());
    zprintf(2, "  extra_information_picture = %d\n", getbits(8));
  }
  zprintf(2, "  extra_bit_picture = %d\n", get1bit());
  next_start_code();
}

void picture_coding_extension()
{
  int i;

  zprintf(1, "\nextension_start_code = 0x000001%02X\n", getbits(8));
  zprintf(1, "  picture_coding_extension_id = %01X\n", getbits(4));
  zprintf(2, "  f_code[0][0] forward horizontal = %d\n", getbits(4));
  zprintf(2, "  f_code[0][1] forward vertical = %d\n", getbits(4));
  zprintf(2, "  f_code[1][0] backward horizontal = %d\n", getbits(4));
  zprintf(2, "  f_code[1][1] backward vertical = %d\n", getbits(4));
  zprintf(2, "  intra_dc_precision = %d\n", getbits(2));
  picture_structure = getbits(2);
  zprintf(2, "  picture_structure = %d\n", picture_structure);
  zprintf(2, "  top_field_first = %d\n", get1bit());
  zprintf(2, "  frame_pred_frame_dct = %d\n", get1bit());
  zprintf(2, "  concealment_motion_vectors = %d\n", get1bit());
  zprintf(2, "  q_scale_type= %d\n", get1bit());
  zprintf(2, "  intra_vlc_format = %d\n", get1bit());
  zprintf(2, "  alternate_scan = %d\n", get1bit());
  repeat_first_field = get1bit();
  zprintf(2, "  repeat_first_field = %d\n", repeat_first_field);
  zprintf(2, "  chroma_420_type = %d\n", get1bit());
  zprintf(2, "  progressive_frame = %d\n", get1bit());
  i = get1bit();
  zprintf(2, "  composite_display_flag = %d\n", i);
  if (i)
  {
    zprintf(2, "  v_axis = %d\n", get1bit());
    zprintf(2, "  field_sequence = %d\n", getbits(3));
    zprintf(2, "  sub_carrier = %d\n", get1bit());
    zprintf(2, "  burst_amplitude = %d\n", getbits(7));
    zprintf(2, "  sub_carrier_phase = %d\n", getbits(8));
  }
  next_start_code();
}

//void macroblock()
//{
//  while (nextbits(11) == 0x80)
//    zprintf(2, "  macroblock_escape = %d\n", getbits(11));
//
//}

void slice()
{
  slice_vertical_position = getbits(8);
  zprintf(3, "\nslice_start_code = 0x000001%02X\n", slice_vertical_position);
  if (vertical_size > 2800)
  {
    slice_vertical_position_extension = getbits(3);
    zprintf(3, "  slice_vertical_position_extension = %d\n", slice_vertical_position_extension);
	 mb_row = (slice_vertical_position_extension << 7) + slice_vertical_position - 1;
  }
  else
	 mb_row = slice_vertical_position - 1;
  previous_macroblock_address = (mb_row * mb_width) - 1;

  if (sequence_scalable_extension_present == 1)
    if (scalable_mode == DATA_PARTITIONING)
      zprintf(3, "  priority_breakpoint = %d\n", getbits(7));
  zprintf(3, "  quantiser_scale_code = %d\n", getbits(5));
  if (nextbits(1) == 1)
  {
    zprintf(3, "  intra_slice_flag = %d\n", get1bit());
    zprintf(3, "  intra_slice = %d\n", get1bit());
    zprintf(3, "  reserve_bits = %d\n", getbits(7));
    while (nextbits(1) == 1)
    {
      zprintf(3, "  extra_bit_slice = %d\n", get1bit());
      zprintf(3, "  extra_information_slice = %d\n", getbits(8));
    }
  }
  zprintf(3, "  extra_bit_slice = %d\n", get1bit());
//  do
//    macroblock();
//  while (nextbits(23) != 0);
  next_start_code();
}

void picture_data()
{
  do
    slice();
  while (nextbits(8) > 0 && nextbits(8) < 0xB0);
//  next_start_code();
}

int main(int argc, char* argv[])
{
  char tmpStr[256];

  col = 0;
  verbose_level = 1;
  sequence_headers = 0;
  sequence_extensions = 0;
  user_data_bytes = 0;
  sequence_scalable_extension_present = 0;
  printf("bbVINFO - version 1.7, by Brent Beyeler (beyeler@home.com)\n");
  printf("   speed increases by, Apachez and Christian Vogelgsang\n\n");
  if (argc < 2)
  {
    printf("\nbbVINFO is an MPEG video stream analyzer\n\n");
    printf("Usage: bbVINFO  MPEG video filename  <verbose level 1..3, default = 1>\n\n");
    printf("Examples:\n");
    printf("  To list all packet start codes (excluding slice codes) to file test.txt\n");
    printf("     bbVINFO test.mpg 1 > test.txt\n\n");
    printf("  To list all packets (excluding slice packets) in detail\n");
    printf("     bbVINFO test.vob 2\n\n");
    printf("  To list all packets (including slice packets) in detail to file test.txt\n");
    printf("     bbVINFO test.mpg 3 > test.txt\n\n");
	 exit (1);
  }

  init_getbits(argv[1]);
  strcpy(tmpStr, argv[1]);
//  strlwr(tmpStr);
  if (argc > 2)
  {
    sscanf(argv[2], "%d", &verbose_level);

    if (verbose_level < 1)
      verbose_level = 1;
    if (verbose_level > 3)
      verbose_level = 3;
  }

  next_start_code();
  if (getbits(8) != SEQUENCE_HEADER_CODE)
  {
    printf("\nFile is not an MPEG Video Stream\n");
    exit(1);
  }

  next_start_code();

  if (getbits(8) != EXTENSION_START_CODE)
    mpeg2 = 0;
  else
    mpeg2 = 1;

  printf("\nFile %s is an MPEG-%d video stream\n", argv[1], mpeg2 + 1);

  finish_getbits();
  init_getbits(argv[1]);

  next_start_code();
  sequence_header();

  if (mpeg2)
    sequence_extension();

  do
  {
    extension_and_user_data(0);
    do
    {
      if (nextbits(8) == GROUP_START_CODE)
      {
        group_of_pictures_header();
        extension_and_user_data(1);
      }
      picture_header();
      if (mpeg2)
        picture_coding_extension();
      extension_and_user_data(2);
      picture_data();
    } while ((nextbits(8) == PICTURE_START_CODE) ||
             (nextbits(8) == GROUP_START_CODE));
    if (nextbits(8) != SEQUENCE_END_CODE)
    {
      sequence_header();
      if (mpeg2)
        sequence_extension();
    }
  } while (nextbits(8) != SEQUENCE_END_CODE);
  zprintf(1, "\nsequence_end_code = 0x000001%02X\n", getbits(8));

  finish_getbits();
  return (0);
}


