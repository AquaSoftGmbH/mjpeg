/**********************************************************************
Copyright (c) 1991 MPEG/audio software simulation group, All Rights Reserved
bitstrm.h
**********************************************************************/

/***********************************************************************
*
*  Global Conditional Compile Switches
*
***********************************************************************/

#define      UNIX            /* Unix conditional compile switch */
/* #define      MACINTOSH       Macintosh conditional compile switch */
#define      MS_DOS          /* IBM PC conditional compile switch */
/* #define      MSC60            Compiled for MS_DOS with MSC v6.0 */
#define      AIX             /* AIX conditional compile switch    */
/* #define      CONVEX           CONVEX conditional compile switch */

#if defined(MSC60) 
#ifndef MS_DOS
#define MS_DOS
#endif
#ifndef PROTO_ARGS
#define PROTO_ARGS
#endif
#endif

/* 
 * Don't define FAR to far unless you're willing to clean up the 
 * prototypes
 */
#define FAR /*far*/

#ifdef __STDC__
#ifndef PROTO_ARGS
#define PROTO_ARGS
#endif
#endif

#ifdef CONVEX
#define SEEK_SET        0
#define SEEK_CUR        1
#define SEEK_END        2
#endif


/***********************************************************************
*
*  Global Include Files
*
***********************************************************************/

#include        <stdio.h>
#include        <string.h>
#include        <math.h>

#ifdef  UNIX
#include        <unistd.h>
#endif  /* UNIX */

#ifdef  MACINTOSH
#include        <stdlib.h>
#include        <console.h>
#endif  /* MACINTOSH */

/* #ifdef  MS_DOS
#include        <stdlib.h>
#ifdef MSC60
#include        <memory.h>
#else
#include        <alloc.h>
#include        <mem.h> */
/* #endif */  /* MSC60 */
/* #endif */  /* MS_DOS */

/***********************************************************************
*
*  Global Definitions
*
***********************************************************************/

/* General Definitions */

#ifdef  MS_DOS
#define         FLOAT                   double
#else
#define         FLOAT                   float
#endif

#define         FALSE                   0
#define         TRUE                    1
#define         NULL_CHAR               '\0'

#define         BITS_IN_A_BYTE          8
#define         WORD                    16


/* "bit_stream.h" Definitions */

#define         MINIMUM         4    /* Minimum size of the buffer in bytes */
#define         MAX_LENGTH      32   /* Maximum length of word written or
                                        read from bit stream */
#define         READ_MODE       0
#define         WRITE_MODE      1
#define         ALIGNING        8
#define         BINARY          0
#define         ASCII           1
#define         BS_FORMAT       BINARY /* BINARY or ASCII = 2x bytes */
#define         BUFFER_SIZE     4096

#define         MIN(A, B)       ((A) < (B) ? (A) : (B))
#define         MAX(A, B)       ((A) > (B) ? (A) : (B))

/* "bit_stream.h" Type Definitions */

typedef struct  bit_stream_struc {
    FILE        *pt;            /* pointer to bit stream device */
    unsigned char *buf;         /* bit stream buffer */
    int         buf_size;       /* size of buffer (in number of bytes) */
    long long        totbit;         /* bit counter of bit stream */
    int         buf_byte_idx;   /* pointer to top byte in buffer */
    int         buf_bit_idx;    /* pointer to top bit of top byte in buffer */
    int         mode;           /* bit stream open in read or write mode */
    int         eob;            /* end of buffer index */
    int         eobs;           /* end of bit stream flag */
    char        format;
    
    /* format of file in rd mode (BINARY/ASCII) */
} Bit_stream_struc;


/***********************************************************************
*
*  Global Function Prototype Declarations
*
***********************************************************************/

/* The following functions are in the file "common.c" */

#ifdef  PROTO_ARGS
extern int            memcheck(char*, int, int);
extern void           FAR *mem_alloc(unsigned long, char*);
extern void           mem_free(void**);
extern void            refill_buffer(Bit_stream_struc*);
extern void           empty_buffer(Bit_stream_struc*, int);
extern void           open_bit_stream_w(Bit_stream_struc*, char*, int);
extern void           open_bit_stream_r(Bit_stream_struc*, char*, int);
extern void           close_bit_stream_r(Bit_stream_struc*);
extern void           close_bit_stream_w(Bit_stream_struc*);
extern void           alloc_buffer(Bit_stream_struc*, int);
extern void           desalloc_buffer(Bit_stream_struc*);
extern void           back_track_buffer(Bit_stream_struc*, int);
extern unsigned int   get1bit(Bit_stream_struc*);
extern void           put1bit(Bit_stream_struc*, int);
extern unsigned long  look_ahead(Bit_stream_struc*, int);
extern unsigned long  getbits(Bit_stream_struc*, int);
extern void           putbits(Bit_stream_struc*, unsigned int, int);
extern void           byte_ali_putbits(Bit_stream_struc*, unsigned int, int);
extern unsigned long  byte_ali_getbits(Bit_stream_struc*, int);
extern unsigned long long  sstell(Bit_stream_struc*);
extern int            end_bs(Bit_stream_struc*);
extern int            seek_sync(Bit_stream_struc*, long, int);

#else
extern int            memcheck();
extern void           FAR *mem_alloc();
extern void           mem_free();
extern int            refill_buffer();
extern void           empty_buffer();
extern void           open_bit_stream_w();
extern void           open_bit_stream_r();
extern void           close_bit_stream_r();
extern void           close_bit_stream_w();
extern void           alloc_buffer();
extern void           desalloc_buffer();
extern void           back_track_buffer();
extern unsigned int   get1bit();
extern void           put1bit();
extern unsigned long  look_ahead();
extern unsigned long  getbits();
extern void           putbits();
extern void           byte_ali_putbits();
extern unsigned long  byte_ali_getbits();
extern unsigned long long  sstell();
extern int            end_bs();
extern int            seek_sync();

#endif
