#include "main.h"
/*************************************************************************
    Funktionen zu Time_Code Berechnungen im System Clock Reference Format

    Functions for Time_Code computations in System Clock Reference Format

*************************************************************************/

#ifdef ORIGINAL_CODE
void empty_timecode_struc (timecode)
Timecode_struc *timecode;
{
	timecode = 0LL;
}

void offset_timecode (time1, time2, offset)
Timecode_struc *time1, *time2, *offset;
{
  *offset = *time2 - *time1;

}

void copy_timecode (time_original, time_copy)
Timecode_struc *time_original, *time_copy;
{
	*time_copy=*time_original;
}




void make_timecode (timestamp, pointer)
double timestamp;
Timecode_struc *pointer;
{
  *pointer = (clockticks) timestamp;
}


void add_to_timecode (add, to)
Timecode_struc *add;
Timecode_struc *to;
{
  *to += *add;
}

/******************************************************************
	Comp_Timecode
	liefert TRUE zurueck, wenn TS1 <= TS2 ist.

	Yields TRUE, if TS1 <= TS2.
******************************************************************/

int comp_timecode (TS1, TS2)
Timecode_struc *TS1;
Timecode_struc *TS2;
{
  return *TS1 < *TS2;

}

#endif
