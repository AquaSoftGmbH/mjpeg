#include "main.h"
/*************************************************************************
    Funktionen zu Time_Code Berechnungen im System Clock Reference Format

    Functions for Time_Code computations in System Clock Reference Format

*************************************************************************/

void empty_timecode_struc (timecode)
Timecode_struc *timecode;
{
    timecode->msb=0;
    timecode->lsb=0;
	timecode->thetime = 0LL;
}

void offset_timecode (time1, time2, offset)
Timecode_struc *time1, *time2, *offset;
{
  offset->thetime = time2->thetime - time1->thetime;
  offset->msb = (unsigned long)(( offset->thetime >> 32) & 1);
  offset->lsb = (unsigned long)( offset->thetime & 0xffffffffLL);  
	/* Original code 
    offset->msb = time2->msb - time1->msb;
    offset->lsb = time2->lsb - time1->lsb;
	*/
}

void copy_timecode (time_original, time_copy)
Timecode_struc *time_original, *time_copy;
{
    time_copy->lsb=time_original->lsb;
    time_copy->msb=time_original->msb;
	time_copy->thetime=time_original->thetime;
}

void make_timecode (timestamp, pointer)
double timestamp;
Timecode_struc *pointer;
{
  /* Work-around for a tricky compiler bug.... */
  pointer->thetime = (unsigned long long) timestamp;
  pointer->msb = (unsigned long)(( pointer->thetime >> 32) & 1);
  pointer->lsb = (unsigned long)( pointer->thetime & 0xffffffffLL);
#ifdef ORIGINAL_CODE
    if (timestamp > MAX_FFFFFFFF)
    {
	  pointer->msb= 1;
	  timestamp -= MAX_FFFFFFFF;
	  pointer->lsb=(unsigned long)timestamp;
    } else
    {
	  pointer->msb=0;
	  pointer->lsb=(unsigned long)timestamp;
    }
#endif
}


void add_to_timecode (add, to)
Timecode_struc *add;
Timecode_struc *to;
{
  to->thetime += add->thetime;
  to->msb = (unsigned long)(( to->thetime >> 32) & 1);
  to-> lsb = (unsigned long)(to->thetime & 0xffffffffLL);
#ifdef ORIGINAL_CODE
    to->msb = (add->msb + to->msb);

    /* oberstes bit in beiden Feldern gesetzt */
    /* high bit set in both fields */

    if (((add->lsb & 0x80000000) & (to->lsb & 0x80000000))>>31)
    {
	to->msb = to->msb ^ 1;
	to->lsb = (to->lsb & 0x7fffffff)+(add->lsb & 0x7fffffff);
    }

    /* oberstes bit in einem der beiden gesetzt */
    /* high bit set in one of both fields */

    else if (((add->lsb & 0x80000000) | (to->lsb & 0x80000000))>>31)
    {
	to->msb = to->msb ^ 
	  ((((add->lsb & 0x7fffffff)+(to->lsb & 0x7fffffff)) & 0x80000000)>>31);
	to->lsb = ((to->lsb & 0x7fffffff)+(add->lsb & 0x7fffffff)^0x80000000);
    }

    /* kein Ueberlauf moeglich */
    /* no overflow possible */

    else
    {
	  to->lsb = to->lsb + add->lsb;
    }
#endif
}


/*************************************************************************
    Kopiert einen TimeCode in einen Bytebuffer. Dabei wird er nach
    MPEG-Verfahren in bits aufgesplittet.

    Makes a Copy of a TimeCode in a Buffer, splitting it into bitfields
    according to MPEG-System
*************************************************************************/

void buffer_timecode (pointer, marker, buffer)

Timecode_struc *pointer;
unsigned char  marker;
unsigned char **buffer;

{
    unsigned char temp;

    temp = (marker << 4) | (pointer->msb <<3) |
		((pointer->lsb >> 29) & 0x6) | 1;
    *((*buffer)++)=temp;
    temp = (pointer->lsb & 0x3fc00000) >> 22;
    *((*buffer)++)=temp;
    temp = ((pointer->lsb & 0x003f8000) >> 14) | 1;
    *((*buffer)++)=temp;
    temp = (pointer->lsb & 0x7f80) >> 7;
    *((*buffer)++)=temp;
    temp = ((pointer->lsb & 0x007f) << 1) | 1;
    *((*buffer)++)=temp;

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
  return TS1->thetime < TS2->thetime;
  /* Original code...
    double Time1;
    double Time2;

    Time1 = (TS1->msb * MAX_FFFFFFFF) + (TS1->lsb);
    Time2 = (TS2->msb * MAX_FFFFFFFF) + (TS2->lsb);

    return (Time1 <= Time2);
  */
}
