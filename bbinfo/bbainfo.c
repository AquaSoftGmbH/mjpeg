/*************************************************************************
  bbVINFO by Brent Beyeler, beyeler@home.com
*************************************************************************/

#include "bits.h"
#include <string.h>

#define SYNCWORD            0xFFF

#define LAYER_1                 3
#define LAYER_2                 2
#define LAYER_3                 1


double frequency [4] = {44.1, 48, 32, 0};
unsigned int slots [4] = {12, 144, 0, 0};
unsigned int bitrate_index [3][16] =
    {{0,32,64,96,128,160,192,224,256,288,320,352,384,416,448,0},
     {0,32,48,56,64,80,96,112,128,160,192,224,256,320,384,0},
     {0,32,40,48,56,64,80,96,112,128,160,192,224,256,320,0}};


int verbose_level;
unsigned int syncwords;
int col, layer, protection, crc_check, bit_rate, freq, padding_bit;
int private_bit, mode, mode_ext, copyright, original, emphasis;

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
  if (seek_sync(SYNCWORD, 12))
    return;
  else
    exit(1);
}

void header()
{
  int i;

  printf("\nbyte count = %.0f\n", bitcount() / 8.0);
  i = getbits(12);
  if (i != SYNCWORD)
  {
    printf("Syncword not found, bits = %03X\n", i);
    exit(1);
  }
  syncwords++;
  zprintf(1, "syncword = %03X\n", i);
  zprintf(1, "  ID = %d\n", get1bit());
  layer = getbits(2);
  zprintf(1, "  layer = %d\n", layer);
  protection = get1bit();
  zprintf(1, "  protection_bit = %d\n", protection);
  bit_rate = getbits(4);
  zprintf(1, "  bitrate_index = %d\n", bit_rate);
  freq = getbits(2);
  zprintf(1, "  sampling_frequency = %d\n", freq);
  padding_bit = get1bit();
  zprintf(1, "  padding_bit = %d\n", padding_bit);
  private_bit = get1bit();
  zprintf(1, "  private_bit = %d\n", private_bit);
  mode = getbits(2);
  zprintf(1, "  mode = %d\n", mode);
  mode_ext = getbits(2);
  zprintf(1, "  mode_extension = %d\n", mode_ext);
  copyright = get1bit();
  zprintf(1, "  copyright = %d\n", copyright);
  original = get1bit();
  zprintf(1, "  original/home = %d\n", original);
  emphasis = getbits(2);
  zprintf(1, "  emphasis = %d\n", emphasis);
}

void error_check()
{
  if (protection == 0)
  {
    crc_check = getbits(16);
    zprintf(1, "  CRC = %d\n", crc_check);
  }
}


void audio_data()
{
  int i, framesize, skip;

  framesize = bitrate_index[3 - layer][bit_rate] /
              frequency[freq] * slots[3 - layer];

  if (padding_bit)
    framesize++;
  skip = framesize - 4;
  if (skip & 0x1)
    getbits(8);

  if (skip & 0x2)
    getbits(16);

  skip = skip >> 2;

  for (i = 0; i < skip; i++)
    getbits(32);
}

void ancillary_data()
{
  if ((layer == 1) || (layer == 2))
    while ((nextbits(12) != SYNCWORD) && !end_bs())
      get1bit();
}

int main(int argc, char* argv[])
{
  char tmpStr[256];

  col = 0;
  verbose_level = 1;
  syncwords = 0;
  printf("bbAINFO - version 1.7, by Brent Beyeler (beyeler@home.com)\n");
  printf("   speed increases by, Apachez and Christian Vogelgsang\n\n");
  if (argc < 2)
  {
    printf("\nbbAINFO is an MPEG audio stream analyzer\n");
    printf("All it realy does is list the sync word headers\n\n");
    printf("Usage: bbAINFO  MPEG audio filename\n\n");
    printf("Examples:\n");
    printf("  To list all frames to file test.txt\n");
    printf("     bbAINFO test.mpg > test.txt\n\n");
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

  if (nextbits(12) != SYNCWORD)
  {
    printf("\nFile is not an MPEG Audio Stream\n");
    exit(1);
  }

  do
  {
    header();
    error_check();
    audio_data();
    ancillary_data();
  } while (!end_bs());

  printf("\nFound %u sync words\n", syncwords);
  printf("\nHeader info summary:\n");
  switch (layer)
  {
    case 0:
      printf("  layer = reserved\n");
      break;
    case LAYER_1:
      printf("  layer = 1\n");
      break;
    case LAYER_2:
      printf("  layer = 2\n");
      break;
    case LAYER_3:
      printf("  layer = 3\n");
      break;
  }
  printf("  bitrate = %d Kbps\n", bitrate_index[3 - layer][bit_rate]);
  printf("  frequency = %.1f kHz\n", frequency[freq]);
  if (protection)
    printf("  error protection = disabled\n");
  else
    printf("  error protection = enabled\n");
  printf("  private flag = %d\n", private_bit);
  switch (mode)
  {
    case 0:
      printf("  mode = stereo\n");
      break;
    case 1:
	   printf("  mode = joint_stereo (intensity_stereo and/or ms_stereo)\n");
      break;
    case 2:
	   printf("  mode = dual_channel\n");
      break;
    case 3:
	   printf("  mode = single_channel\n");
  }
  if (layer == LAYER_3)
  {
    switch (mode_ext)
    {
      case 0:
        printf("  mode ext = intensity stereo is off, ms stereo is off\n");
        break;
      case 1:
        printf("  mode ext = intensity stereo is on, ms stereo is off\n");
        break;
      case 2:
        printf("  mode ext = intensity stereo is off, ms stereo is on\n");
        break;
      case 3:
        printf("  mode ext = intensity stereo is on, ms stereo is on\n");
    }
  }
  else
  {
    if (mode == 1)
    {
      switch (mode_ext)
      {
        case 0:
          printf("  mode ext = subbands  4-31 in intensity_stereo, bound==4\n");
          break;
        case 1:
          printf("  mode ext = subbands  8-31 in intensity_stereo, bound==8\n");
          break;
        case 2:
          printf("  mode ext = subbands 12-31 in intensity_stereo, bound==12\n");
          break;
        case 3:
          printf("  mode ext = subbands 16-31 in intensity_stereo, bound==16\n");
      }
    }
  }
  printf("  copyright flag = %d\n", copyright);
  printf("  original flag = %d\n", original);
  switch (emphasis)
  {
    case 0:
      printf("  emphasis = none\n");
      break;
    case 1:
      printf("  emphasis = 50/15 microsec. emphasis\n");
      break;
    case 2:
      printf("  emphasis = reserved\n");
      break;
    case 3:
      printf("  emphasis = CCITT J.17\n");
  }
  finish_getbits();
  return (0);
}


