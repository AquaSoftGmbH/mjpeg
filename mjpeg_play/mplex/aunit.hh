#ifndef __AUNIT_H__
#define __AUNIT_H__

#include <config.h>
#include <inttypes.h>

#include "bits.hh"

typedef uint64_t clockticks;

class Aunit
{
public:
	Aunit() : length(0), PTS(0), DTS(0) {}
	void markempty() { length = 0; }
	bitcount_t start;
	unsigned int length;
    clockticks PTS;
    int        dorder;
	// Used only for video AU's but otherwise
	// you have to go crazy on templates.
    clockticks DTS;
    int		   porder;
    unsigned int type;
	bool	   seq_header;
	bool	   end_seq;

};

typedef Aunit VAunit;

typedef Aunit AAunit;

#endif // __AUNIT_H__
