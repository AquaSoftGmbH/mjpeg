/*
 *  wave_io - convenience function for reading/writing basic WAVE files
 *
 *  Copyright (C) 2004, Michael Hanke
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <stdio.h>
#include <stdint.h>

typedef struct {
    char ChunkID[4];
    uint32_t ChunkSize;
    char Format[4];
    char Subchunk1ID[4];
    uint32_t Subchunk1Size;
    uint16_t AudioFormat;
    uint16_t NumChannels;
    uint32_t SampleRate;
    uint32_t ByteRate;
    uint16_t BlockAlign;
    uint16_t BitsPerSample;
    char Subchunk2ID[4];
    uint32_t Subchunk2Size;
} wave_header_t;

FILE *read_wave_header(wave_header_t **header, int fd);

FILE *write_wave_header(wave_header_t *header, int fd);

void close_wave_output(wave_header_t *header, FILE *fp, int nsamp);

int read_sample(wave_header_t *header, FILE *fp, float *samples);

int write_sample(wave_header_t *header, FILE *fp, float *samples);
