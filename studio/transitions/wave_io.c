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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "wave_io.h"

#define WAVE_IO_CHUNKID         0
#define WAVE_IO_CHUNKSIZE       4
#define WAVE_IO_FORMAT          8
#define WAVE_IO_SUBCHUNK1ID    12
#define WAVE_IO_SUBCHUNK1SIZE  16
#define WAVE_IO_AUDIOFORMAT    20
#define WAVE_IO_NUMCHANNELS    22
#define WAVE_IO_SAMPLERATE     24
#define WAVE_IO_BYTERATE       28
#define WAVE_IO_BLOCKALIGN     32
#define WAVE_IO_BITSPERSAMPLE  34
#define WAVE_IO_SUBCHUNK2ID    36
#define WAVE_IO_SUBCHUNK2SIZE  40

#define WAVE_IO_HEADER         44

#if defined WORDS_BIGENDIAN || defined WORD_BIGENDIAN
#define SWAP2B(X) (X)
#define SWAP2L(X) swap2(X)
#define SWAP4B(X) (X)
#define SWAP4L(X) swap4(X)
#else
#define SWAP2B(X) swap2(X)
#define SWAP2L(X) (X)
#define SWAP4B(X) swap4(X)
#define SWAP4L(X) (X)
#endif

#ifndef MAX
#define MAX(X,Y) ((X) < (Y) ? (Y) : (X))
#endif
#ifndef MIN
#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))
#endif

static int16_t swap2(int16_t val) {
    return ((val >> 8) & 0xFF) | (val << 8);
}

static int32_t swap4(int32_t val) {
    int32_t res;
    int i;

    res = 0;
    for (i = 0; i < 4; i++) {
	res <<= 8;
	res |= (val & 0xFF);
	val >>= 8;
    }
    return res;
}

static void errcl(char *msg, FILE *fp, wave_header_t *head) {
    fprintf(stderr, msg);
    fclose(fp);
    free(head);
    }


FILE *read_wave_header(wave_header_t **header, int fd)
{
    FILE *fp;
    char bh[WAVE_IO_HEADER];
    int rd;
    wave_header_t *head;

    head = (wave_header_t *) malloc(sizeof(wave_header_t));
    *header = head;
    if (head == NULL) return (NULL);
    if ((fp = fdopen(fd, "r")) == NULL) return(fp);

    rd = fread(bh, 1, WAVE_IO_HEADER, fp);
    if (rd != WAVE_IO_HEADER) {
	fclose(fp);
	free(head);
	*header = NULL;
	return (NULL);
    }

    memcpy(head->ChunkID, bh+WAVE_IO_CHUNKID, 4);
    memcpy(&(head->ChunkSize), bh+WAVE_IO_CHUNKSIZE, 4);
    memcpy(head->Format, bh+WAVE_IO_FORMAT, 4);
    memcpy(head->Subchunk1ID, bh+WAVE_IO_SUBCHUNK1ID, 4);
    memcpy(&(head->Subchunk1Size), bh+WAVE_IO_SUBCHUNK1SIZE, 4);
    memcpy(&(head->AudioFormat), bh+WAVE_IO_AUDIOFORMAT, 2);
    memcpy(&(head->NumChannels), bh+WAVE_IO_NUMCHANNELS, 2);
    memcpy(&(head->SampleRate), bh+WAVE_IO_SAMPLERATE, 4);
    memcpy(&(head->ByteRate), bh+WAVE_IO_BYTERATE, 4);
    memcpy(&(head->BlockAlign), bh+WAVE_IO_BLOCKALIGN, 2);
    memcpy(&(head->BitsPerSample), bh+WAVE_IO_BITSPERSAMPLE, 2);
    memcpy(head->Subchunk2ID, bh+WAVE_IO_SUBCHUNK2ID, 4);
    memcpy(&(head->Subchunk2Size), bh+WAVE_IO_SUBCHUNK2SIZE, 4);

    if (memcmp(head->ChunkID, "RIF", 3) || 
	memcmp(head->Format, "WAVE", 4)) {
	errcl("Error: unknown wave file format\n", fp, head);
	return(NULL);
    }

    if (memcmp(head->Subchunk1ID, "fmt ", 4)) {
	errcl("Error: Format chunk missing\n", fp, head);
	return(NULL);
    }

    switch (head->ChunkID[3]) {
	case 'F':
	    head->ChunkSize = SWAP4L(head->ChunkSize);
	    head->Subchunk1Size = SWAP4L(head->Subchunk1Size);
	    head->AudioFormat = SWAP2L(head->AudioFormat);
	    head->NumChannels = SWAP2L(head->NumChannels);
	    head->SampleRate = SWAP4L(head->SampleRate);
	    head->ByteRate = SWAP4L(head->ByteRate);
	    head->BlockAlign = SWAP2L(head->BlockAlign);
	    head->BitsPerSample = SWAP2L(head->BitsPerSample);
	    head->Subchunk2Size = SWAP4L(head->Subchunk2Size);
	    break;
	case 'X':
	    head->ChunkSize = SWAP4B(head->ChunkSize);
	    head->Subchunk1Size = SWAP4B(head->Subchunk1Size);
	    head->AudioFormat = SWAP2B(head->AudioFormat);
	    head->NumChannels = SWAP2B(head->NumChannels);
	    head->SampleRate = SWAP4B(head->SampleRate);
	    head->ByteRate = SWAP4B(head->ByteRate);
	    head->BlockAlign = SWAP2B(head->BlockAlign);
	    head->BitsPerSample = SWAP2B(head->BitsPerSample);
	    head->Subchunk2Size = SWAP4B(head->Subchunk2Size);
	    break;
	default:
	    errcl("Error: Unknown wave format\n", fp, head);
	    return(NULL);
    }

/* Check format chunk. Must be PCM */

    if (head->Subchunk1Size != 16) {
	errcl("Error: Wrong length in format subchunk\n", fp, head);
	return(NULL);
    }
    if (head->AudioFormat != 1) {
	errcl("Error: Only raw PCM accepted\n", fp, head);
	return(NULL);
    }
    if ((head->NumChannels != 1) && (head->NumChannels != 2)) {
	errcl("Error: Wrong number of channels\n", fp, head);
	return(NULL);
    }
    if ((head->SampleRate < 0) || (head->SampleRate > 200000)) {
	errcl("Error: Wrong sample rate\n", fp, head);
	return(NULL);
    }
    if ((head->BitsPerSample != 8) && (head->BitsPerSample != 16)) {
	errcl("Error: Sample size not 8 or 16\n", fp, head);
	return(NULL);
    }

/* Assign dependent values */
    head->BlockAlign = head->NumChannels*(head->BitsPerSample>>3);
    head->ByteRate = head->BlockAlign*head->SampleRate;

/* The actual data subchunk */
    if (memcmp(head->Subchunk2ID, "data", 4)) {
	errcl("Error: Data chunk missing\n", fp, head);
	return(NULL);
    }
/* Done */
    return(fp);
}

FILE *write_wave_header(wave_header_t *header, int fd)
{
    FILE *fp;
    char bh[WAVE_IO_HEADER];
    int rd;

    if ((fp = fdopen(fd, "w")) == NULL) return(fp);

/* Assume here that header information is OK */
    switch (header->ChunkID[3]) {
	case 'F':
	    header->ChunkSize = SWAP4L(header->ChunkSize);
	    header->Subchunk1Size = SWAP4L(header->Subchunk1Size);
	    header->AudioFormat = SWAP2L(header->AudioFormat);
	    header->NumChannels = SWAP2L(header->NumChannels);
	    header->SampleRate = SWAP4L(header->SampleRate);
	    header->ByteRate = SWAP4L(header->ByteRate);
	    header->BlockAlign = SWAP2L(header->BlockAlign);
	    header->BitsPerSample = SWAP2L(header->BitsPerSample);
	    header->Subchunk2Size = SWAP4L(header->Subchunk2Size);
	    break;
	case 'X':
	    header->ChunkSize = SWAP4B(header->ChunkSize);
	    header->Subchunk1Size = SWAP4B(header->Subchunk1Size);
	    header->AudioFormat = SWAP2B(header->AudioFormat);
	    header->NumChannels = SWAP2B(header->NumChannels);
	    header->SampleRate = SWAP4B(header->SampleRate);
	    header->ByteRate = SWAP4B(header->ByteRate);
	    header->BlockAlign = SWAP2B(header->BlockAlign);
	    header->BitsPerSample = SWAP2B(header->BitsPerSample);
	    header->Subchunk2Size = SWAP4B(header->Subchunk2Size);
	    break;
    }

    memcpy(bh+WAVE_IO_CHUNKID, header->ChunkID, 4);
    memcpy(bh+WAVE_IO_CHUNKSIZE, &(header->ChunkSize), 4);
    memcpy(bh+WAVE_IO_FORMAT, header->Format, 4);
    memcpy(bh+WAVE_IO_SUBCHUNK1ID, header->Subchunk1ID, 4);
    memcpy(bh+WAVE_IO_SUBCHUNK1SIZE, &(header->Subchunk1Size), 4);
    memcpy(bh+WAVE_IO_AUDIOFORMAT, &(header->AudioFormat), 2);
    memcpy(bh+WAVE_IO_NUMCHANNELS, &(header->NumChannels), 2);
    memcpy(bh+WAVE_IO_SAMPLERATE, &(header->SampleRate), 4);
    memcpy(bh+WAVE_IO_BYTERATE, &(header->ByteRate), 4);
    memcpy(bh+WAVE_IO_BLOCKALIGN, &(header->BlockAlign), 2);
    memcpy(bh+WAVE_IO_BITSPERSAMPLE, &(header->BitsPerSample), 2);
    memcpy(bh+WAVE_IO_SUBCHUNK2ID, header->Subchunk2ID, 4);
    memcpy(bh+WAVE_IO_SUBCHUNK2SIZE, &(header->Subchunk2Size), 4);

    rd = fwrite(bh, 1, WAVE_IO_HEADER, fp);
    switch (header->ChunkID[3]) {
	case 'F':
	    header->ChunkSize = SWAP4L(header->ChunkSize);
	    header->Subchunk1Size = SWAP4L(header->Subchunk1Size);
	    header->AudioFormat = SWAP2L(header->AudioFormat);
	    header->NumChannels = SWAP2L(header->NumChannels);
	    header->SampleRate = SWAP4L(header->SampleRate);
	    header->ByteRate = SWAP4L(header->ByteRate);
	    header->BlockAlign = SWAP2L(header->BlockAlign);
	    header->BitsPerSample = SWAP2L(header->BitsPerSample);
	    header->Subchunk2Size = SWAP4L(header->Subchunk2Size);
	    break;
	case 'X':
	    header->ChunkSize = SWAP4B(header->ChunkSize);
	    header->Subchunk1Size = SWAP4B(header->Subchunk1Size);
	    header->AudioFormat = SWAP2B(header->AudioFormat);
	    header->NumChannels = SWAP2B(header->NumChannels);
	    header->SampleRate = SWAP4B(header->SampleRate);
	    header->ByteRate = SWAP4B(header->ByteRate);
	    header->BlockAlign = SWAP2B(header->BlockAlign);
	    header->BitsPerSample = SWAP2B(header->BitsPerSample);
	    header->Subchunk2Size = SWAP4B(header->Subchunk2Size);
	    break;
    }

    if (rd != WAVE_IO_HEADER) {
	fprintf(stderr, "Error: Unable to write wave header\n");
	fclose(fp);
	return(NULL);
    }
    return(fp);
}

static void war(FILE *fp)
{
    fprintf(stderr, "Warning: wave header not completed\n");
    fclose(fp);
}

void close_wave_output(wave_header_t *header, FILE *fp, int nsamp)
{
    uint32_t size2, size;
    int i, rd;

    size2 = header->BlockAlign*nsamp;
    size = size2+36;
    switch (header->ChunkID[3]) {
	case 'F':
	    size = SWAP4L(size);
	    size2 = SWAP4L(size2);
	    break;
	case 'X':
	    size = SWAP4B(size);
	    size2 = SWAP4B(size2);
	    break;
    }

    i = fseek(fp, 4L, SEEK_SET);
    if (i) {
	war(fp);
	return;
    }
    rd = fwrite(&size, 1, 4, fp);
    if (rd != 4) {
	war(fp);
	return;
    }
    i = fseek(fp, 40L, SEEK_SET);
    if (i) {
	war(fp);
	return;
    }
    rd = fwrite(&size2, 1, 4, fp);
    if (rd != 4) {
	war(fp);
	return;
    }
    fclose(fp);
    return;
}

int read_sample(wave_header_t *header, FILE *fp, float *samples)
{
    int i, rd;
    int16_t ts;

    samples[0] = samples[1] = 0.0;
    for (i = 0; i < 2; i++) {
	switch (header->BitsPerSample) {
	    case 8:
		ts = 0;
		rd = fread((void *) &ts, 1, 1, fp);
		if (rd != 1) return(1);
		switch(header->ChunkID[3]) {
		    case 'F':
			ts = SWAP2L(ts);
			break;
		    case 'X':
			ts = SWAP2B(ts);
			break;
		}
		samples[i] = ((float) (ts-128))/128.0;
		break;
	    case 16:
		rd = fread((void *) &ts, 1, 2, fp);
		if (rd != 2) return(1);
		switch(header->ChunkID[3]) {
		    case 'F':
			ts = SWAP2L(ts);
			break;
		    case 'X':
			ts = SWAP2B(ts);
			break;
		}
		samples[i] = ((float) ts)/32768.0;
		break;
	}
	if (header->NumChannels == 1) break;
    }

    return(0);
}

int write_sample(wave_header_t *header, FILE *fp, float *samples)
{
    int i, rd;
    int16_t ts;

    for (i = 0; i < 2; i++) {
	switch (header->BitsPerSample) {
	    case 8:
		ts = MAX(MIN(samples[i]*128,127.0),-128.0)+128;
		switch(header->ChunkID[3]) {
		    case 'F':
			ts = SWAP2L(ts);
			break;
		    case 'X':
			ts = SWAP2B(ts);
			break;
		}
		rd = fwrite((void *) &ts, 1, 1, fp);
		if (rd != 1) return(1);
		break;
	    case 16:
		ts = MAX(MIN(samples[i]*32768,32767.0),-32768.0);
		switch(header->ChunkID[3]) {
		    case 'F':
			ts = SWAP2L(ts);
			break;
		    case 'X':
			ts = SWAP2B(ts);
			break;
		}
		rd = fwrite((void *) &ts, 1, 2, fp);
		if (rd != 2) return(1);
		break;
	}
	if (header->NumChannels == 1) break;
    }

    return(0);
}
