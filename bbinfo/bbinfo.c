/*************************************************************************
  bbINFO by Brent Beyeler, beyeler@home.com
*************************************************************************/

#include "bits.h"
#include <string.h>

#define MPEG_PROGRAM_END_CODE    0x000001B9
#define PACK_START_CODE          0x000001BA
#define SYSTEM_HEADER_START_CODE 0x000001BB
#define PACKET_START_CODE_PREFIX 0x000001

#define PROGRAM_STREAM_MAP       0xBC
#define PRIVATE_STREAM_1         0xBD
#define PADDING_STREAM           0xBE
#define PRIVATE_STREAM_2         0xBF
#define ECM_STREAM               0xF0
#define EMM_STREAM               0xF1
#define PROGRAM_STREAM_DIRECTORY 0xFF
#define DSMCC_STREAM             0xF2
#define ITUTRECH222TYPEE_STREAM  0xF8
#define VIDEO_STREAM_0           0xE0
#define VIDEO_STREAM_F           0xEF
#define AUDIO_STREAM_0           0xC0
#define AUDIO_STREAM_1F          0xDF
#define SUBSTREAM_AC3_0          0x80
#define SUBSTREAM_AC3_8          0x87
#define SUBSTREAM_DTS_0          0x88
#define SUBSTREAM_DTS_8          0x8F
#define SUBSTREAM_PCM_0          0xA0
#define SUBSTREAM_PCM_F          0xAF
#define SUBSTREAM_SUBPIC_0       0x20
#define SUBSTREAM_SUBPIC_1F      0x3F

#define PICTURE_START_CODE       0x00
#define USER_DATA_START_CODE     0xB2
#define SEQUENCE_HEADER_CODE     0xB3
#define EXTENSION_START_CODE     0xB5
#define SEQUENCE_END_CODE        0xB7
#define GROUP_START_CODE         0xB8
#define PICTURE_CODING_EXTENSION 0x04

#define FAST_FORWARD 0
#define SLOW_MOTION  1
#define FREEZE_FRAME 2
#define FAST_REVERSE 3
#define SLOW_REVERSE 4

static int verbose_level;
static int col;
static char oldLevel3[80];

void zprintf(int level, char *tmpStr, int value)
{
  if (verbose_level > 2)
  {
    if (level > 2)
    {
      if ((col + strlen(tmpStr) > 80) || (tmpStr[2] != oldLevel3[2]))
      {
        col = 0;
        printf("\n");
      }
      printf(tmpStr);
      col += strlen(tmpStr);
      strcpy(oldLevel3, tmpStr);
    }
  }
  else
    if (level <= verbose_level)
      printf(tmpStr, value);
}

int main(int argc, char* argv[])
{
  unsigned long i, j, k, PES_packet_length, PES_header_data_length;
  unsigned long PTS_DTS_flags, ESCR_flag, ES_rate_flag, DSM_trick_mode_flag;
  unsigned long additional_copy_info_flag, PES_CRC_flag, PES_extension_flag;
  unsigned long trick_mode_control, PES_private_data_flag, pack_header_field_flag;
  unsigned long program_packet_sequence_counter_flag, PSTD_buffer_flag;
  unsigned long PES_extension_flag_2, PES_extension_field_length;
  unsigned long bytes_used, video_sync[16], audio_sync[32];
  unsigned long ac3_bytes[8], dts_bytes[8], pcm_bytes[16], subpic_bytes[32];
  bool firsttime = true;
  unsigned long pack_packets, system_packets;
  unsigned long stream_id, substream_id, SCR_value, PTS_value, DTS_value;
  unsigned long stream_packets[256], stream_bytes[256];
  unsigned long substream_packets[256], substream_bytes[256];
  char level3[80], tmpStr[256];
  bool streams_found[256], vob_flag, mpeg2;
  long double scr_value, pts_value, dts_value;
  double bit_count;
  int offset;


  for (i = 0; i < 16; i++)
  {
    video_sync[i] = 0;
    pcm_bytes[i] = 0;
  }

  for (i = 0; i < 32; i++)
  {
    audio_sync[i] = 0;
    subpic_bytes[i] = 0;
  }

  for (i = 0; i < 256; i++)
  {
    streams_found[i] = false;
    stream_packets[i] = 0;
    stream_bytes[i] = 0;
    substream_packets[i] = 0;
    substream_bytes[i] = 0;
  }
  for (i = 0; i < 8; i++)
  {
    ac3_bytes[i] = 0;
    dts_bytes[i] = 0;
  }
  bit_count = 0.0;
  pack_packets = 0;
  system_packets = 0;
  col = 0;
  strcpy(level3, "");
  strcpy(oldLevel3, "");
  verbose_level = 1;
  printf("bbINFO - version 1.7, by Brent Beyeler (beyeler@home.com)\n");
  printf("  speed increases by, Apachez and Christian Vogelgsang\n\n");
  if (argc < 2)
  {
    printf("\nbbINFO is an MPEG program stream analyzer\n\n");
    printf("Usage: bbINFO  MPEG filename  <verbose level 1..4, default = 1>\n\n");
    printf("Examples:\n");
    printf("  To list the timing information for all packets\n");
    printf("     bbINFO test.mpg 1\n\n");
    printf("  To list all packets in detail (can generate huge files)\n");
    printf("     bbINFO test.mpg 2\n\n");
    printf("  To list all packets visually saving to file test.txt\n");
    printf("     bbINFO test.vob 3 > test.txt\n\n");
    printf("  To list all packets visually including PTS, DTS and P-STD info\n");
    printf("     bbINFO test.vob 4\n");
	 exit (1);
  }

  init_getbits(argv[1]);
  strcpy(tmpStr, argv[1]);
//  strlwr(tmpStr);
  vob_flag = (strstr(tmpStr, ".vob") != NULL);
  if (argc > 2)
  {
    sscanf(argv[2], "%d", &verbose_level);

    if (verbose_level < 1)
      verbose_level = 1;
    if (verbose_level > 4)
      verbose_level = 4;
  }

  do
  {
    i = getbits(32);
nextpass:
    if (firsttime)
    {
      if (i != PACK_START_CODE)
      {
        i = getbits(32);
        goto nextpass;
      }
      if (look_ahead(2) == 1)
      {
        mpeg2 = true;
        printf("\nFile %s is an MPEG-2 Program Stream\n", argv[1]);
      }
      else
      {
        mpeg2 = false;
        printf("\nFile %s is an MPEG-1 Program Stream\n", argv[1]);
      }
      firsttime = false;
    }
    switch (i)
    {
      case PACK_START_CODE:
        pack_packets++;
        if (bit_count != 0.0)
        {
          j = ((unsigned long)(bitcount() - bit_count)) >> 3;
          zprintf(2, "\npack size = %d\n", j);
        }
        bit_count = bitcount();
        if (verbose_level < 3)
          zprintf(1, "\n\nPACK #%d, ", pack_packets - 1);
        if (verbose_level == 1)
          zprintf(1, "pack_start_code = %08X, ", i);
        else
          zprintf(1, "pack_start_code = %08X\n", i);
        j = getbits(2);
        if (mpeg2)
          zprintf(2, "  01 = %d\n", j);
        else
        {
          j = (j << 2) | getbits(2);
          zprintf(2, "  0010 = %d\n", j);
        }
        if (verbose_level == 1)
        {
          SCR_value = getbits(3);
          if (SCR_value & 0x00000004)
            scr_value = 4294967296.0;
          else
            scr_value = 0.0;
          SCR_value = SCR_value << 30;
          get1bit();
          SCR_value |= (getbits(15) << 15);
          get1bit();
          SCR_value |= getbits(15);
          scr_value += (long double) SCR_value;
          scr_value *= 300.0;
          get1bit();
          if (mpeg2)
            scr_value += (long double) getbits(9);
          printf("SCR = %Lf ms\n", scr_value / 27000.0);
          scr_value /= 300.0;
        }
        else
        {
          zprintf(2, "  system_clock_reference_base [32..30] = %d\n", getbits(3));
          zprintf(2, "  marker bit = %d\n", get1bit());
          zprintf(2, "  system_clock_reference_base [29..15] = %d\n", getbits(15));
          zprintf(2, "  marker bit = %d\n", get1bit());
          zprintf(2, "  system_clock_reference_base [14..0] = %d\n", getbits(15));
          zprintf(2, "  marker bit = %d\n", get1bit());
          if (mpeg2)
            zprintf(2, "  system_clock_reference_extension = %d\n", getbits(9));
        }
        zprintf(2, "  marker bit = %d\n", get1bit());
        zprintf(2, "  program_mux_rate = %d\n", getbits(22));
        zprintf(2, "  marker bit = %d\n", get1bit());
        if (mpeg2)
        {
          zprintf(2, "  marker bit = %d\n", get1bit());
          zprintf(2, "  reserved = %d\n", getbits(5));
          i = getbits(3);
          zprintf(2, "  pack_stuffing_length = %d\n", i);
          for (j = 0; j < i; j++)
            zprintf(2, "    stuffing byte = %d\n", getbits(8));
        }
        if (verbose_level > 2)
        {
          if (strlen(level3))
            zprintf(3, level3, 0);
          strcpy(level3, " P");
        }
        break;

      case SYSTEM_HEADER_START_CODE:
        system_packets++;
        zprintf(1, "\nsystem_header_start_code = %08X\n", i);
        zprintf(2, "  header_length = %d\n", getbits(16));
        zprintf(2, "  marker bit = %d\n", get1bit());
        zprintf(2, "  rate_bound = %d\n", getbits(22));
        zprintf(2, "  marker bit = %d\n", get1bit());
        zprintf(2, "  audio_bound = %d\n", getbits(6));
        zprintf(2, "  fixed_flag = %d\n", get1bit());
        zprintf(2, "  CSPS_flag = %d\n", get1bit());
        zprintf(2, "  system_audio_lock_flag = %d\n", get1bit());
        zprintf(2, "  system_video_lock_flag = %d\n", get1bit());
        zprintf(2, "  marker bit = %d\n", get1bit());
        zprintf(2, "  video_bound = %d\n", getbits(5));
        if (mpeg2)
        {
          zprintf(2, "  packet_rate_restriction_flag = %d\n", get1bit());
          zprintf(2, "  reserved_bits = %d\n", getbits(7));
        }
        else
          zprintf(2, "  reserved_bits = %d\n", getbits(8));
        while (look_ahead(1) == 1)
        {
          zprintf(2, "  stream_id = %02X\n", getbits(8));
          zprintf(2, "  11 = %d\n", getbits(2));
          zprintf(2, "  P-STD_buffer_bound_scale = %d\n", get1bit());
          zprintf(2, "  P-STD_buffer_size_bound = %d\n", getbits(13));
        }
        if (verbose_level > 2)
          strcat(level3, "S");
        break;

      case MPEG_PROGRAM_END_CODE:
        zprintf(1, "\n\nmpeg_program_end_code = %08X\n", i);
        if (verbose_level > 2)
          strcat(level3, "E");
        break;

      default:
        if ((i >> 8) != PACKET_START_CODE_PREFIX)
        {
//          printf("\n\nUnknown bits in stream = %08X at byte %.0f\n", i, bitcount() / 8 - 4);
//          printf("\nAttempting resynch ... ");
          if (seek_sync(PACKET_START_CODE_PREFIX, 24))
          {
            i = 0x00000100 | getbits(8);
//            printf("succesfully resynched\n\n");
            goto nextpass;
          }
          if (end_bs())
            break;
          printf("\n\nUnknown bits in stream, could not resynch\n");
          exit(1);
        }
        if (verbose_level != 1)
          zprintf(1, "\npacket_start_code_prefix = %06X\n", i >> 8);
        stream_id = i & 0x000000FF;
        if (!streams_found[stream_id])
          streams_found[stream_id] = true;
        stream_packets[stream_id]++;

        switch (stream_id)
        {
          case PROGRAM_STREAM_MAP:
            zprintf(1, "  stream_id = %02X  Program Stream Map\n", stream_id);
            break;
          case PRIVATE_STREAM_1:
            if (verbose_level == 1)
              zprintf(1, "  stream_id = %02X  Private Stream 1, ", stream_id);
            else
              zprintf(1, "  stream_id = %02X  Private Stream 1\n", stream_id);
            break;
          case PRIVATE_STREAM_2:
            zprintf(1, "  stream_id = %02X  Private Stream 2\n", stream_id);
            break;
          case ECM_STREAM:
            zprintf(1, "  stream_id = %02X  ECM Stream\n", stream_id);
            break;
          case EMM_STREAM:
            zprintf(1, "  stream_id = %02X  EMM Stream\n", stream_id);
            break;
          case PROGRAM_STREAM_DIRECTORY:
            zprintf(1, "  stream_id = %02X  Program Stream Directory\n", stream_id);
            break;
          case DSMCC_STREAM:
            zprintf(1, "  stream_id = %02X  DSMCC Stream\n", stream_id);
            break;
          case ITUTRECH222TYPEE_STREAM:
            zprintf(1, "  stream_id = %02X  ITU-T Rec. H.222.1 type E\n", stream_id);
            break;
          case PADDING_STREAM:
            if (verbose_level == 1)
              zprintf(1, "  stream_id = %02X  Padding Stream, ", stream_id);
            else
              zprintf(1, "  stream_id = %02X  Padding Stream\n", stream_id);
            break;
          default:
            zprintf(1, "  packet #%d\n", stream_packets[stream_id] - 1);
            if ((stream_id >= 0xE0) && (stream_id <= 0xEF))
            {
              sprintf(tmpStr, "  stream_id = %%02X  Video Stream %d, ", stream_id - 0xE0);
              if (verbose_level != 1)
                strcat(tmpStr, "\n");
              zprintf(1, tmpStr, stream_id);
            }
            else
              if ((stream_id >= 0xC0) && (stream_id <= 0xDF))
              {
                sprintf(tmpStr, "  stream_id = %%02X  MPEG Audio Stream %d, ", stream_id - 0xC0);
                if (verbose_level != 1)
                  strcat(tmpStr, "\n");
                zprintf(1, tmpStr, stream_id);
              }
              else
                if (verbose_level == 1)
                  zprintf(1, "  stream_id = %02X, ", stream_id);
                else
                  zprintf(1, "  stream_id = %02X\n", stream_id);
        }
        PES_packet_length = getbits(16);
        zprintf(2, "  packet_length = %d\n", PES_packet_length);
        switch (stream_id)
        {
          case PROGRAM_STREAM_MAP:
          case ECM_STREAM:
          case EMM_STREAM:
          case PROGRAM_STREAM_DIRECTORY:
          case DSMCC_STREAM:
          case ITUTRECH222TYPEE_STREAM:
            for (j = 0; j < PES_packet_length; j++)
              getbits(8);
            if (verbose_level > 2)
              strcat(level3, "o");
            zprintf(2, "  read %d PES packet data bytes\n", j);
            stream_bytes[stream_id] += j;
            break;

          case PRIVATE_STREAM_2:
            for (j = 0; j < PES_packet_length; j++)
              getbits(8);
            if (verbose_level > 2)
              strcat(level3, "2");
            zprintf(2, "  read %d PES packet data bytes\n", j);
            stream_bytes[stream_id] += j;
            break;

          case PADDING_STREAM:
            bytes_used = 0;
            if (!mpeg2)
            {
              /* flush stuffing bytes */
              while (look_ahead(1) == 1)
              {
                getbits(8);
                bytes_used++;
              }
              zprintf(2, "  read %d stuffing bytes\n", bytes_used);

              if (look_ahead(2) == 1)
              {
                if (verbose_level == 2)
                  printf("  P-STD buffer fields\n");
                zprintf(2, "    01 = %d\n", getbits(2));
                zprintf(2, "    P-STD_buffer_scale = %d\n", get1bit());
                zprintf(2, "    P-STD_buffer_size = %d\n", getbits(13));
                bytes_used += 2;
                if (verbose_level > 3)
                  strcat(level3, "P-STD");
              }

              if (look_ahead(4) == 2)
              {
                if (verbose_level == 1)
                {
                  getbits(4);
                  PTS_value = getbits(3);
                  if (PTS_value & 0x00000004)
                    pts_value = 4294967296.0;
                  else
                    pts_value = 0.0;
                  PTS_value = PTS_value << 30;
                  get1bit();
                  PTS_value |= (getbits(15) << 15);
                  get1bit();
                  PTS_value |= getbits(15);
                  pts_value += (long double) PTS_value;
                  get1bit();
                  printf("PTS = %Lf ms\n", pts_value / 90.0);
                }
                else
                {
                  if (verbose_level == 2)
                    printf("  PTS fields\n");
                  zprintf(2, "    0010 = %d\n", getbits(4));
                  zprintf(2, "    PTS[32..30] = %d\n", getbits(3));
                  zprintf(2, "    marker_bit = %d\n", get1bit());
                  zprintf(2, "    PTS[29..15] = %d\n", getbits(15));
                  zprintf(2, "    marker_bit = %d\n", get1bit());
                  zprintf(2, "    PTS[14..0] = %d\n", getbits(15));
                  zprintf(2, "    marker_bit = %d\n", get1bit());
                  if (verbose_level > 3)
                    strcat(level3, "PTS");
                }
                bytes_used += 5;
              }
              else
                if (look_ahead(4) == 3)
                {
                  if (verbose_level == 1)
                  {
                    getbits(4);
                    PTS_value = getbits(3);
                    if (PTS_value & 0x00000004)
                      pts_value = 4294967296.0;
                    else
                      pts_value = 0.0;
                    PTS_value = PTS_value << 30;
                    get1bit();
                    PTS_value |= (getbits(15) << 15);
                    get1bit();
                    PTS_value |= getbits(15);
                    pts_value += (long double) PTS_value;
                    get1bit();
                    printf("PTS = %Lf ms, ", pts_value / 90.0);
                    getbits(4);
                    DTS_value = getbits(3);
                    if (DTS_value & 0x00000004)
                      dts_value = 4294967296.0;
                    else
                      dts_value = 0.0;
                    DTS_value = DTS_value << 30;
                    get1bit();
                    DTS_value |= (getbits(15) << 15);
                    get1bit();
                    DTS_value |= getbits(15);
                    dts_value += (long double) DTS_value;
                    get1bit();
                    printf("DTS = %Lf ms\n", dts_value / 90.0);
                  }
                  else
                  {
                    if (verbose_level == 2)
                      printf("  PTS fields\n");
                    zprintf(2, "    0011 = %d\n", getbits(4));
                    zprintf(2, "    PTS[32..30] = %d\n", getbits(3));
                    zprintf(2, "    marker_bit = %d\n", get1bit());
                    zprintf(2, "    PTS[29..15] = %d\n", getbits(15));
                    zprintf(2, "    marker_bit = %d\n", get1bit());
                    zprintf(2, "    PTS[14..0] = %d\n", getbits(15));
                    zprintf(2, "    marker_bit = %d\n", get1bit());
                    if (verbose_level == 2)
                      printf("  DTS fields\n");
                    zprintf(2, "    0001 = %d\n", getbits(4));
                    zprintf(2, "    DTS[32..30] = %d\n", getbits(3));
                    zprintf(2, "    marker_bit = %d\n", get1bit());
                    zprintf(2, "    DTS[29..15] = %d\n", getbits(15));
                    zprintf(2, "    marker_bit = %d\n", get1bit());
                    zprintf(2, "    DTS[14..0] = %d\n", getbits(15));
                    zprintf(2, "    marker_bit = %d\n", get1bit());
                    if (verbose_level > 3)
                      strcat(level3, "PTSDTS");
                  }
                  bytes_used += 10;
                }
                else
                {
                  if (verbose_level == 1)
                    printf("\n");
                  j = getbits(8);
                  if (j != 0x0f)
                  {
                    printf("  Invalid bits in MPEG-1 file\n");
                    exit (1);
                  }
                  zprintf(2, "  no_timestamps marker = %d\n", j);
                  bytes_used++;
                }
            }
            for (j = 0; j < PES_packet_length - bytes_used; j++)
              getbits(8);
            if (verbose_level > 2)
              strcat(level3, "p");
            zprintf(2, "  read %d padding_bytes\n", j);
            stream_bytes[stream_id] += j;
            break;

          default:
            bytes_used = 0;
            if (mpeg2)
            {
              zprintf(2, "  10 = %d\n", getbits(2));
              zprintf(2, "  PES_scrambling_control = %d\n", getbits(2));
              zprintf(2, "  PES_priority = %d\n", get1bit());
              zprintf(2, "  data_alignment_indicator = %d\n", get1bit());
              zprintf(2, "  copyright = %d\n", get1bit());
              zprintf(2, "  original_or_copy = %d\n", get1bit());
              PTS_DTS_flags = getbits(2);
              zprintf(2, "  PTS_DTS_flags = %d\n", PTS_DTS_flags);
              ESCR_flag = get1bit();
              zprintf(2, "  ESCR_flag = %d\n", ESCR_flag);
              ES_rate_flag = get1bit();
              zprintf(2, "  ES_rate_flag = %d\n", ES_rate_flag);
              DSM_trick_mode_flag = get1bit();
              zprintf(2, "  DSM_trick_mode_flag = %d\n", DSM_trick_mode_flag);
              additional_copy_info_flag = get1bit();
              zprintf(2, "  additional_copy_info_flag = %d\n", additional_copy_info_flag);
              PES_CRC_flag = get1bit();
              zprintf(2, "  PES_CRC_flag = %d\n", PES_CRC_flag);
              PES_extension_flag = get1bit();
              zprintf(2, "  PES_extension_flag = %d\n", PES_extension_flag);
              PES_header_data_length = getbits(8);
              zprintf(2, "  PES_header_data_length = %d\n", PES_header_data_length);

              if ((verbose_level == 1) && (PTS_DTS_flags != 2) && (PTS_DTS_flags != 3))
                printf("\n");
              if (PTS_DTS_flags == 2)
              {
                if (verbose_level == 1)
                {
                  getbits(4);
                  PTS_value = getbits(3);
                  if (PTS_value & 0x00000004)
                    pts_value = 4294967296.0;
                  else
                    pts_value = 0.0;
                  PTS_value = PTS_value << 30;
                  get1bit();
                  PTS_value |= (getbits(15) << 15);
                  get1bit();
                  PTS_value |= getbits(15);
                  pts_value += (long double) PTS_value;
                  get1bit();
                  if (pts_value <= scr_value)
                    printf("PTS = %Lf ms, underflow\n", pts_value / 90.0);
                  else
                    printf("PTS = %Lf ms\n", pts_value / 90.0);
                }
                else
                {
                  if (verbose_level == 2)
                    printf("  PTS fields\n");
                  zprintf(2, "    0010 = %d\n", getbits(4));
                  zprintf(2, "    PTS[32..30] = %d\n", getbits(3));
                  zprintf(2, "    marker_bit = %d\n", get1bit());
                  zprintf(2, "    PTS[29..15] = %d\n", getbits(15));
                  zprintf(2, "    marker_bit = %d\n", get1bit());
                  zprintf(2, "    PTS[14..0] = %d\n", getbits(15));
                  zprintf(2, "    marker_bit = %d\n", get1bit());
                  if (verbose_level > 3)
                    strcat(level3, "PTS");
                }
                bytes_used += 5;
              }

              if (PTS_DTS_flags == 3)
              {
                if (verbose_level == 1)
                {
                  getbits(4);
                  PTS_value = getbits(3);
                  if (PTS_value & 0x00000004)
                    pts_value = 4294967296.0;
                  else
                    pts_value = 0.0;
                  PTS_value = PTS_value << 30;
                  get1bit();
                  PTS_value |= (getbits(15) << 15);
                  get1bit();
                  PTS_value |= getbits(15);
                  pts_value += (long double) PTS_value;
                  get1bit();
                  if (pts_value <= scr_value)
                    printf("PTS = %Lf ms underflow, ", pts_value / 90.0);
                  else
                    printf("PTS = %Lf ms, ", pts_value / 90.0);
                  getbits(4);
                  DTS_value = getbits(3);
                  if (DTS_value & 0x00000004)
                    dts_value = 4294967296.0;
                  else
                    dts_value = 0.0;
                  DTS_value = DTS_value << 30;
                  get1bit();
                  DTS_value |= (getbits(15) << 15);
                  get1bit();
                  DTS_value |= getbits(15);
                  dts_value += (long double) DTS_value;
                  get1bit();
                  if (dts_value <= scr_value)
                    printf("DTS = %Lf ms underflow\n", dts_value / 90.0);
                  else
                    printf("DTS = %Lf ms\n", dts_value / 90.0);
                }
                else
                {
                  if (verbose_level == 2)
                    printf("  PTS fields\n");
                  zprintf(2, "    0011 = %d\n", getbits(4));
                  zprintf(2, "    PTS[32..30] = %d\n", getbits(3));
                  zprintf(2, "    marker_bit = %d\n", get1bit());
                  zprintf(2, "    PTS[29..15] = %d\n", getbits(15));
                  zprintf(2, "    marker_bit = %d\n", get1bit());
                  zprintf(2, "    PTS[14..0] = %d\n", getbits(15));
                  zprintf(2, "    marker_bit = %d\n", get1bit());
                  if (verbose_level == 2)
                    printf("  DTS fields\n");
                  zprintf(2, "    0001 = %d\n", getbits(4));
                  zprintf(2, "    DTS[32..30] = %d\n", getbits(3));
                  zprintf(2, "    marker_bit = %d\n", get1bit());
                  zprintf(2, "    DTS[29..15] = %d\n", getbits(15));
                  zprintf(2, "    marker_bit = %d\n", get1bit());
                  zprintf(2, "    DTS[14..0] = %d\n", getbits(15));
                  zprintf(2, "    marker_bit = %d\n", get1bit());
                  if (verbose_level > 3)
                    strcat(level3, "PTSDTS");
                }
                bytes_used += 10;
              }

              if (ESCR_flag == 1)
              {
                if (verbose_level == 2)
                  printf("  ESCR fields\n");
                zprintf(2, "    reserved = %d\n", getbits(2));
                zprintf(2, "    ESCR_base[32..30] = %d\n", getbits(3));
                zprintf(2, "    marker_bit = %d\n", get1bit());
                zprintf(2, "    ESCR_base[29..15] = %d\n", getbits(15));
                zprintf(2, "    marker_bit = %d\n", get1bit());
                zprintf(2, "    ESCR_base[14..0] = %d\n", getbits(15));
                zprintf(2, "    marker_bit = %d\n", get1bit());
                zprintf(2, "    ESCR_extension = %d\n", getbits(9));
                zprintf(2, "    marker_bit = %d\n", get1bit());
                bytes_used += 6;
              }

              if (ES_rate_flag == 1)
              {
                if (verbose_level == 2)
                  printf("  ES rate fields\n");
                zprintf(2, "    marker_bit = %d\n", get1bit());
                zprintf(2, "    ES_rate = %d\n", getbits(22));
                zprintf(2, "    marker_bit = %d\n", get1bit());
                bytes_used += 3;
              }

              if (DSM_trick_mode_flag == 1)
              {
                trick_mode_control = getbits(3);
                if (verbose_level == 2)
                  printf("  DSM trick mode fields\n");
                zprintf(2, "    trick_mode_control = %d\n", trick_mode_control);
                switch (trick_mode_control)
                {
                  case FAST_FORWARD:
                    if (verbose_level == 2)
                      printf("    fast forward fields\n");
                    zprintf(2, "      field_id = %d\n", getbits(2));
                    zprintf(2, "      intra_slice_refresh = %d\n", get1bit());
                    zprintf(2, "      frequency_truncation = %d\n", getbits(2));
                    break;

                  case SLOW_MOTION:
                    if (verbose_level == 2)
                      printf("    slow motion fields\n");
                    zprintf(2, "      rep_cntrl = %d\n", getbits(5));
                    break;

                  case FREEZE_FRAME:
                    if (verbose_level == 2)
                      printf("    freeze frame fields\n");
                    zprintf(2, "      field_id = %d\n", getbits(2));
                    zprintf(2, "      reserved = %d\n", getbits(3));
                    break;

                  case FAST_REVERSE:
                    if (verbose_level == 2)
                      printf("    fast reverse fields\n");
                    zprintf(2, "      field_id = %d\n", getbits(2));
                    zprintf(2, "      intra_slice_refresh = %d\n", get1bit());
                    zprintf(2, "      frequency_truncation = %d\n", getbits(2));
                    break;

                  case SLOW_REVERSE:
                    if (verbose_level == 2)
                      printf("    slow reverse fields\n");
                    zprintf(2, "      rep_cntrl = %d\n", getbits(5));
                    break;

                  default:
                    if (verbose_level == 2)
                      printf("    reserve fields\n");
                    zprintf(2, "      reserved = %d\n", getbits(5));
                    break;
                }
                bytes_used++;
              }

              if (additional_copy_info_flag == 1)
              {
                if (verbose_level == 2)
                  printf("  additional copy info fields\n");
                zprintf(2, "    marker_bit = %d\n", get1bit());
                zprintf(2, "    additional_copy_info = %d\n", getbits(7));
                bytes_used++;
              }

              if (PES_CRC_flag == 1)
              {
                if (verbose_level == 2)
                  printf("  PES CRC field\n");
                zprintf(2, "    previous_PES_packet_CRC = %d\n", getbits(16));
                bytes_used += 2;
              }

              if (PES_extension_flag == 1)
              {
                if (verbose_level == 2)
                  printf("  PES exten2sion fields\n");
                PES_private_data_flag = get1bit();
                zprintf(2, "    PES_private_data_flag = %d\n", PES_private_data_flag);
                pack_header_field_flag = get1bit();
                zprintf(2, "    pack_header_field_flag = %d\n", pack_header_field_flag);
                program_packet_sequence_counter_flag = get1bit();
                zprintf(2, "    program_packet_sequence_counter_flag = %d\n", program_packet_sequence_counter_flag);
                PSTD_buffer_flag = get1bit();
                zprintf(2, "    P-STD_buffer_flag = %d\n", PSTD_buffer_flag);
                zprintf(2, "    reserved = %d\n", getbits(3));
                PES_extension_flag_2 = get1bit();
                zprintf(2, "    PES_extension_flag_2 = %d\n", PES_extension_flag_2);
                bytes_used++;

                if (PES_private_data_flag == 1)
                {
                  if (verbose_level == 2)
                    printf("    PES private data fields\n");
                  for (j = 0; j < 128; j++)
                    getbits(8);
                  zprintf(2, "      read %d bytes of private data\n", j);
                  bytes_used += 16;
                }

                if (pack_header_field_flag == 1)
                {
                  printf("    pack header field flag value not allowed in program streams\n");
                exit(1);
                }

                if (program_packet_sequence_counter_flag == 1)
                {
                  if (verbose_level == 2)
                    printf("    program packet sequence counter fields\n");
                  zprintf(2, "      marker_bit = %d\n", get1bit());
                  zprintf(2, "      program_packet_sequence_counter = %d\n", getbits(7));
                  zprintf(2, "      marker_bit = %d\n", get1bit());
                  zprintf(2, "      MPEG1_MPEG2_identifier = %d\n", get1bit());
                  zprintf(2, "      original_stuff_length = %d\n", getbits(6));
                  bytes_used += 2;
                }

                if (PSTD_buffer_flag == 1)
                {
                  if (verbose_level == 2)
                    printf("    P-STD buffer fields\n");
                  zprintf(2, "      01 = %d\n", getbits(2));
                  zprintf(2, "      P-STD_buffer_scale = %d\n", get1bit());
                  zprintf(2, "      P-STD_buffer_size = %d\n", getbits(13));
                  bytes_used += 2;
                  if (verbose_level > 3)
                    strcat(level3, "P-STD");
                }

                if (PES_extension_flag_2 == 1)
                {
                  if (verbose_level == 2)
                    printf("    PES extension 2 fields\n");
                  zprintf(2, "      marker_bit = %d\n", get1bit());
                  PES_extension_field_length = getbits(7);
                  zprintf(2, "      PES_extension_field_length = %d\n", PES_extension_field_length);
                  bytes_used++;
                  for (j = 0; j < PES_extension_field_length; j++)
                  {
                    getbits(8);
                    bytes_used++;
                  }
                  zprintf(2, "      read %d bytes of PES extension field data\n", j);
                }
              }
              for (j = 0; j < PES_header_data_length - bytes_used; j++)
                getbits(8);
              if (j)
                zprintf(2, "  read %d stuffing bytes\n", j);

              if (verbose_level == 1)
              {
                if ((stream_id >= VIDEO_STREAM_0) && (stream_id <= VIDEO_STREAM_F))
                {
                  i = stream_id - VIDEO_STREAM_0;
                  j = 0;
                  while (j < PES_packet_length - PES_header_data_length - 3)
                  {
                    k = getbits(8);
                    switch (video_sync[i])
                    {
                      case 0:
                        if (k == 0x00)
                          video_sync[i] = 1;
                        break;
                      case 1:
                        if (k == 0x00)
                          video_sync[i] = 2;
                        else
                          video_sync[i] = 0;
                        break;
                      case 2:
                        if (k != 0x01)
                        {
                          if (k != 0x00)
                            video_sync[i] = 0;
                        }
                        else
                        {
                          offset = j - 2;
                          switch (look_ahead(8))
                          {
                            case USER_DATA_START_CODE:
                              printf("    %d - user_data_start_code\n", offset);
                              break;
                            case SEQUENCE_HEADER_CODE:
                              printf("    %d - sequence_header_code\n", offset);
                              break;
                            case EXTENSION_START_CODE:
                              printf("    %d - extension_start_code\n", offset);
                              break;
                            case SEQUENCE_END_CODE:
                              printf("    %d - sequence_end_code\n", offset);
                              break;
                            case GROUP_START_CODE:
                              printf("    %d - group_of_pictures header\n", offset);
                              break;
                            case PICTURE_START_CODE:
                              printf("    %d - picture_start_code\n", offset);
                              break;
                          }
                          video_sync[i] = 0;
                        }
                    }
                    j++;
                  }
                }
                else
                  if ((stream_id >= AUDIO_STREAM_0) && (stream_id <= AUDIO_STREAM_1F))
                  {
                    i = stream_id - AUDIO_STREAM_0;
                    for (j = 0; j < PES_packet_length - PES_header_data_length - 3; j++)
                    {
                      k = getbits(8);
                      switch (audio_sync[i])
                      {
                        case 0:
                          if (k == 0xFF)
                            audio_sync[i] = 1;
                          break;
                        case 1:
                          if ((k & 0xF0) == 0xF0)
                          {
                            offset = j - 1;
                            printf("    %d - audio syncword\n", offset);
                          }
                          audio_sync[i] = 0;
                      }
                    }
                  }
                  else
                    if ((stream_id == PRIVATE_STREAM_1) && (vob_flag))
                    {
                      substream_id = getbits(8);
                      if ((substream_id >= SUBSTREAM_AC3_0) && (substream_id <= SUBSTREAM_AC3_8))
                      {
                        zprintf(1, "    substream id = 0x%02X  ", substream_id);
                        zprintf(1, "AC3 Audio stream %d, ", substream_id - SUBSTREAM_AC3_0);
                        zprintf(1, "num of syncwords = 0x%02X, ", getbits(8));
                        zprintf(1, "first offset = 0x%04X\n", getbits(16));
                        for (j = 1; j < PES_packet_length - PES_header_data_length - 6; j++)
                          getbits(8);
                      }
                      else
                         if ((substream_id >= SUBSTREAM_DTS_0) && (substream_id <= SUBSTREAM_DTS_8))
                         {
                           zprintf(1, "    substream id = 0x%02X  ", substream_id);
                           zprintf(1, "DTS Audio stream %d\n", substream_id - SUBSTREAM_DTS_0);
                           for (j = 1; j < PES_packet_length - PES_header_data_length - 3; j++)
                             getbits(8);
                         }
                         else
                           if ((substream_id >= SUBSTREAM_PCM_0) && (substream_id <= SUBSTREAM_PCM_F))
                            {
                              zprintf(1, "    substream id = 0x%02X  ", substream_id);
                              zprintf(1, "PCM Audio stream %d\n", substream_id - SUBSTREAM_PCM_0);
                              for (j = 1; j < PES_packet_length - PES_header_data_length - 3; j++)
                                getbits(8);
                            }
                            else
                              if ((substream_id >= SUBSTREAM_SUBPIC_0) && (substream_id <= SUBSTREAM_SUBPIC_1F))
                               {
                                 zprintf(1, "    substream id = 0x%02X  ", substream_id);
                                 zprintf(1, "Subpicture stream %d\n", substream_id - SUBSTREAM_SUBPIC_0);
                                 for (j = 1; j < PES_packet_length - PES_header_data_length - 3; j++)
                                   getbits(8);
                               }
                               else
                               {
                                 zprintf(1, "    substream id = 0x%02X\n", substream_id);
                                 for (j = 1; j < PES_packet_length - PES_header_data_length - 3; j++)
                                   getbits(8);
                               }
                    }
                    else
                      for (j = 0; j < PES_packet_length - PES_header_data_length - 3; j++)
                        getbits(8);
              }
              else
              {
                if ((stream_id == PRIVATE_STREAM_1) && (vob_flag))
                  substream_id = getbits(8);
                else
                  getbits(8);
                for (j = 0; j < PES_packet_length - PES_header_data_length - 4; j++)
                  getbits(8);
                j++;
              }
            }
            else /* mpeg-1 */
            {
              /* flush stuffing bytes */
              while (look_ahead(1) == 1)
              {
                getbits(8);
                bytes_used++;
              }
              zprintf(2, "  read %d stuffing bytes\n", bytes_used);

              if (look_ahead(2) == 1)
              {
                if (verbose_level == 2)
                  printf("  P-STD buffer fields\n");
                zprintf(2, "    01 = %d\n", getbits(2));
                zprintf(2, "    P-STD_buffer_scale = %d\n", get1bit());
                zprintf(2, "    P-STD_buffer_size = %d\n", getbits(13));
                bytes_used += 2;
                if (verbose_level > 3)
                  strcat(level3, "P-STD");
              }

              if (look_ahead(4) == 2)
              {
                if (verbose_level == 1)
                {
                  getbits(4);
                  PTS_value = getbits(3);
                  if (PTS_value & 0x00000004)
                    pts_value = 4294967296.0;
                  else
                    pts_value = 0.0;
                  PTS_value = PTS_value << 30;
                  get1bit();
                  PTS_value |= (getbits(15) << 15);
                  get1bit();
                  PTS_value |= getbits(15);
                  pts_value += (long double) PTS_value;
                  get1bit();
                  if (pts_value <= scr_value)
                    printf("PTS = %Lf ms underflow\n", pts_value / 90.0);
                  else
                    printf("PTS = %Lf ms\n", pts_value / 90.0);
                }
                else
                {
                  if (verbose_level == 2)
                    printf("  PTS fields\n");
                  zprintf(2, "    0010 = %d\n", getbits(4));
                  zprintf(2, "    PTS[32..30] = %d\n", getbits(3));
                  zprintf(2, "    marker_bit = %d\n", get1bit());
                  zprintf(2, "    PTS[29..15] = %d\n", getbits(15));
                  zprintf(2, "    marker_bit = %d\n", get1bit());
                  zprintf(2, "    PTS[14..0] = %d\n", getbits(15));
                  zprintf(2, "    marker_bit = %d\n", get1bit());
                  if (verbose_level > 3)
                    strcat(level3, "PTS");
                }
                bytes_used += 5;
              }
              else
                if (look_ahead(4) == 3)
                {
                  if (verbose_level == 1)
                  {
                    getbits(4);
                    PTS_value = getbits(3);
                    if (PTS_value & 0x00000004)
                      pts_value = 4294967296.0;
                    else
                      pts_value = 0.0;
                    PTS_value = PTS_value << 30;
                    get1bit();
                    PTS_value |= (getbits(15) << 15);
                    get1bit();
                    PTS_value |= getbits(15);
                    pts_value += (long double) PTS_value;
                    get1bit();
                    if (pts_value <= scr_value)
                      printf("PTS = %Lf ms underflow, ", pts_value / 90.0);
                    else
                      printf("PTS = %Lf ms, ", pts_value / 90.0);
                    getbits(4);
                    DTS_value = getbits(3);
                    if (DTS_value & 0x00000004)
                      dts_value = 4294967296.0;
                    else
                      dts_value = 0.0;
                    DTS_value = DTS_value << 30;
                    get1bit();
                    DTS_value |= (getbits(15) << 15);
                    get1bit();
                    DTS_value |= getbits(15);
                    dts_value += (long double) DTS_value;
                    get1bit();
                    if (dts_value <= scr_value)
                      printf("DTS = %Lf ms underflow\n", dts_value / 90.0);
                    else
                      printf("DTS = %Lf ms\n", dts_value / 90.0);
                  }
                  else
                  {
                    if (verbose_level == 2)
                      printf("  PTS fields\n");
                    zprintf(2, "    0011 = %d\n", getbits(4));
                    zprintf(2, "    PTS[32..30] = %d\n", getbits(3));
                    zprintf(2, "    marker_bit = %d\n", get1bit());
                    zprintf(2, "    PTS[29..15] = %d\n", getbits(15));
                    zprintf(2, "    marker_bit = %d\n", get1bit());
                    zprintf(2, "    PTS[14..0] = %d\n", getbits(15));
                    zprintf(2, "    marker_bit = %d\n", get1bit());
                    if (verbose_level == 2)
                      printf("  DTS fields\n");
                    zprintf(2, "    0001 = %d\n", getbits(4));
                    zprintf(2, "    DTS[32..30] = %d\n", getbits(3));
                    zprintf(2, "    marker_bit = %d\n", get1bit());
                    zprintf(2, "    DTS[29..15] = %d\n", getbits(15));
                    zprintf(2, "    marker_bit = %d\n", get1bit());
                    zprintf(2, "    DTS[14..0] = %d\n", getbits(15));
                    zprintf(2, "    marker_bit = %d\n", get1bit());
                    if (verbose_level > 3)
                      strcat(level3, "PTSDTS");
                  }
                  bytes_used += 10;
                }
                else
                {
                  if (verbose_level == 1)
                    printf("\n");
                  j = getbits(8);
                  if (j != 0x0f)
                  {
                    printf("  Invalid bits in MPEG-1 file\n");
                    exit (1);
                  }
                  zprintf(2, "  no_timestamps marker = %d\n", j);
                  bytes_used++;
                }
              if (verbose_level == 1)
              {
                if ((stream_id >= VIDEO_STREAM_0) && (stream_id <= VIDEO_STREAM_F))
                {
                  i = stream_id - VIDEO_STREAM_0;
                  for (j = 0; j < PES_packet_length - bytes_used; j++)
                  {
                    k = getbits(8);
                    switch (video_sync[i])
                    {
                      case 0:
                        if (k == 0x00)
                          video_sync[i] = 1;
                        break;
                      case 1:
                        if (k == 0x00)
                          video_sync[i] = 2;
                        else
                          video_sync[i] = 0;
                        break;
                      case 2:
                        if (k != 0x01)
                        {
                          if (k != 0x00)
                            video_sync[i] = 0;
                        }
                        else
                        {
                          offset = j - 2;
                          switch (look_ahead(8))
                          {
                            case USER_DATA_START_CODE:
                              printf("    %d - user_data_start_code\n", offset);
                              break;
                            case SEQUENCE_HEADER_CODE:
                              printf("    %d - sequence_header_code\n", offset);
                              break;
                            case EXTENSION_START_CODE:
                              printf("    %d - extension_start_code\n", offset);
                              break;
                            case SEQUENCE_END_CODE:
                              printf("    %d - sequence_end_code\n", offset);
                              break;
                            case GROUP_START_CODE:
                              printf("    %d - group_of_pictures header\n", offset);
                              break;
                            case PICTURE_START_CODE:
                              printf("    %d - picture_start_code\n", offset);
                              break;
                          }
                          video_sync[i] = 0;
                        }
                    }
                  }
                }
                else
                  if ((stream_id >= AUDIO_STREAM_0) && (stream_id <= AUDIO_STREAM_1F))
                  {
                    i = stream_id - AUDIO_STREAM_0;
                    for (j = 0; j < PES_packet_length - bytes_used; j++)
                    {
                      k = getbits(8);
                      switch (audio_sync[i])
                      {
                        case 0:
                          if (k == 0xFF)
                            audio_sync[i] = 1;
                          break;
                        case 1:
                          if ((k & 0xF0) == 0xF0)
                          {
                            offset = j - 1;
                            printf("    %d - audio syncword\n", offset);
                          }
                          audio_sync[i] = 0;
                      }
                    }
                  }
                  else
                    for (j = 0; j < PES_packet_length - bytes_used; j++)
                      getbits(8);
              }
              else
              for (j = 0; j < PES_packet_length - bytes_used; j++)
                getbits(8);
            }

            if (j)
            {
              zprintf(2, "  read %d PES packet data bytes\n", j);
              stream_bytes[stream_id] += j;
              if ((stream_id == PRIVATE_STREAM_1) && (vob_flag))
              {
                substream_packets[substream_id]++;
                substream_bytes[substream_id] += j;
                if ((substream_id >= SUBSTREAM_AC3_0) && (substream_id <= SUBSTREAM_AC3_8))
                  ac3_bytes[substream_id - SUBSTREAM_AC3_0] += j - 4;
                if ((substream_id >= SUBSTREAM_DTS_0) && (substream_id <= SUBSTREAM_DTS_8))
                  dts_bytes[substream_id - SUBSTREAM_DTS_0] += j - 1;
                if ((substream_id >= SUBSTREAM_PCM_0) && (substream_id <= SUBSTREAM_PCM_F))
                  pcm_bytes[substream_id - SUBSTREAM_PCM_0] += j - 1;
                if ((substream_id >= SUBSTREAM_SUBPIC_0) && (substream_id <= SUBSTREAM_SUBPIC_1F))
                  subpic_bytes[substream_id - SUBSTREAM_SUBPIC_0] += j - 1;
              }
            }
            if (verbose_level > 2)
            {
              if ((stream_id >= 0xE0) && (stream_id <= 0xEF))
              {
                strcat(level3, "v");
                sprintf(tmpStr, "%d", stream_id - 0xE0);
                strcat(level3, tmpStr);
              }
              else
                if ((stream_id >= 0xC0) && (stream_id <= 0xDF))
                {
                  strcat(level3, "a");
                  sprintf(tmpStr, "%d", stream_id - 0xC0);
                  strcat(level3, tmpStr);
                }
                else
                  if (stream_id == PRIVATE_STREAM_1)
                  {
                    strcat(level3, "1");
                    if (vob_flag)
                    {
                      sprintf(tmpStr, "%02X", substream_id);
                      strcat(level3, tmpStr);
                    }
                  }
                  else
                    strcat(level3, "o");
            }
            break;
        }
    }
  } while ((i != MPEG_PROGRAM_END_CODE) && (!end_bs()));

  if ((verbose_level > 2) && (strlen(level3)))
    zprintf(3, level3, 0);

  printf("\n\nSummary:\n");
  printf("\nMPEG Packs = %u\n", pack_packets);
  if (system_packets)
    printf("System headers = %u\n", system_packets);
  for (i = 0; i < 256; i++)
  {
    if (stream_packets[i])
    {
      switch (i)
      {
        case PROGRAM_STREAM_MAP:
          printf("Program Stream Map packets = %u, total bytes = %u\n", stream_packets[i], stream_bytes[i]);
          break;
        case PRIVATE_STREAM_1:
          printf("Private Stream 1 packets = %u, total bytes = %u\n", stream_packets[i], stream_bytes[i]);
          if (vob_flag)
          {
            for (j = 0; j < 256; j++)
            {
              if (substream_packets[j])
              {
                if ((j >= SUBSTREAM_AC3_0) && (j <= SUBSTREAM_AC3_8))
                  printf("    AC3 Audio stream %u packets = %u, total bytes = %u\n", j - SUBSTREAM_AC3_0, substream_packets[j], ac3_bytes[j]);
                else
                  if ((j >= SUBSTREAM_DTS_0) && (j <= SUBSTREAM_DTS_8))
                    printf("    DTS Audio stream %u packets = %u, total bytes = %u\n", j - SUBSTREAM_DTS_0, substream_packets[j], dts_bytes[j]);
                  else
                    if ((j >= SUBSTREAM_PCM_0) && (j <= SUBSTREAM_PCM_F))
                      printf("    PCM Audio stream %u packets = %u, total bytes = %u\n", j - SUBSTREAM_PCM_0, substream_packets[j], pcm_bytes[j]);
                    else
                      if ((j >= SUBSTREAM_SUBPIC_0) && (j <= SUBSTREAM_SUBPIC_1F))
                        printf("    Subpicture stream %u packets = %u, total bytes = %u\n", j - SUBSTREAM_SUBPIC_0, substream_packets[j], subpic_bytes[j]);
                      else
                        printf("    Substream 0x%02X packets = %u, total bytes = %u\n", j, substream_packets[j], substream_bytes[j]);
              }
            }
          }
          break;
        case PRIVATE_STREAM_2:
          printf("Private Stream 2 packets = %u, total bytes = %u\n", stream_packets[i], stream_bytes[i]);
          break;
        case ECM_STREAM:
          printf("ECM Stream packets = %u, total bytes = %u\n", stream_packets[i], stream_bytes[i]);
          break;
        case EMM_STREAM:
          printf("EMM Stream packets = %u, total bytes = %u\n", stream_packets[i], stream_bytes[i]);
          break;
        case PROGRAM_STREAM_DIRECTORY:
          printf("Program Stream Directory packets = %u, total bytes = %u\n", stream_packets[i], stream_bytes[i]);
          break;
        case DSMCC_STREAM:
          printf("DSMCC Stream packets = %u, total bytes = %u\n", stream_packets[i], stream_bytes[i]);
          break;
        case ITUTRECH222TYPEE_STREAM:
          printf("ITU-T Rec. H.222.1 type E packets = %u, total bytes = %u\n", stream_packets[i], stream_bytes[i]);
          break;
        case PADDING_STREAM:
          printf("Padding Stream packets = %u, total bytes = %u\n", stream_packets[i], stream_bytes[i]);
          break;
        default:
          if ((i >= 0xE0) && (i <= 0xEF))
            printf("Video stream %d packets = %u, total bytes = %u\n", i - 0xE0, stream_packets[i], stream_bytes[i]);
          else
            if ((i >= 0xC0) && (i <= 0xDF))
              printf("MPEG Audio stream %d packets = %u, total bytes = %u\n", i - 0xC0, stream_packets[i], stream_bytes[i]);
            else
              printf("Other stream 0x%02X packets = %u, total bytes = %u\n", i, stream_packets[i], stream_bytes[i]);
      }
    }
  }
  finish_getbits();
  return (0);
}


