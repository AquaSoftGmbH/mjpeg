
#ifndef WAV_IO_H
#define WAV_IO_H

int wav_read_header(FILE *fd, int *rate, int *chans, int *bits,
                    int *format, unsigned long *bytes);

#endif
