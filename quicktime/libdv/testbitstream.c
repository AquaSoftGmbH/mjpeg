#include <glib.h>
#include <stdio.h>
#include <assert.h>

#include "bitstream.h"

static guint8 buffer[10000] = "Deliberately buried, eh?";

int offset = 0;
guint show_16(void)
{
    guint a, b, c;
    guint r = offset & 7;
    guint8 *s = &buffer[offset >> 3];

#if 1
    a = s[0] << (8 + r);
    b = s[1] << r;
    c = s[2] >> (8 - r);
    return (a | b | c) & 0xffff;
#else
    return ((((s[0] << 8) | s[1]) << r) | (s[2] >> (8 - r))) & 0xffff;
#endif
}

guint show_N(gint N)
{
    return show_16() >> (16 - N);
}

void advance_N(gint N)
{
    offset += N;
}

static void correctness(void)
{
    bitstream_t *bs;
    static const gint sizes[] = {
	8, 3, 4, 9, 1, 2, 9, 8, 7, 3, 2, 16, 0
    };
    static const gint answers[] = {
	0x44,
	0x3,
	0x2,
	0x16c,
	0,
	0x3,
	0x96,
	0x26,
	0x2b,
	0x4,
	0x2,
	0x6174
    };
    gint i;

    bs = bitstream_init();
    bitstream_new_buffer(bs, buffer, sizeof(buffer)); 
    
    for (i = 0; sizes[i] != 0; i++) {
	gint n = bitstream_get(bs, sizes[i]);
	//n = show_N(sizes[i]);
	//advance_N(sizes[i]);
	if (n != answers[i])
	    printf("Expected %#x, got %#x\n", answers[i], n);
    }
}

void performance(void) 
{
    bitstream_t *bs;
    const gint numbits = 8 * sizeof(buffer);
    static guint32 value;
    gint passes, i;
    
    bs = bitstream_init();

    for (passes = 30000; passes; passes--) {
	bitstream_new_buffer(bs, buffer, sizeof(buffer)); 

	for (i = 0; i < numbits; i += 11) {
	    value = bitstream_show(bs, 4);
	    bitstream_flush(bs, 4);
	    value = bitstream_show(bs, 7);
	    bitstream_flush(bs, 7);
	}
    }
}    

int main(int argc, char *argv[])
{
    performance(); 
    exit(0);
}

