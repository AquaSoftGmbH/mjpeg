#ifndef __AUNIT_H__
#define __AUNIT_H__

#include <config.h>
#include <inttypes.h>

typedef uint64_t clockticks;

class Aunit
{
public:
	inline Aunit() : length(0), PTS(0), DTS(0) {}
	inline void markempty() { length = 0; }
	unsigned int length;
    clockticks PTS;
    clockticks DTS;
    int        dorder;
};

class VAunit : public Aunit
{
public:
    unsigned int type;
    int		   porder;
	bool	   seq_header;
	bool	   end_seq;
};

class AAunit : public Aunit
{
};

#endif // __AUNIT_H__
