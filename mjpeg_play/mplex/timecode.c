#include "main.h"
/*************************************************************************
    Funktionen zu Time_Code Berechnungen im System Clock Reference Format

    Functions for Time_Code computations in System Clock Reference Format

*************************************************************************/

void empty_timecode_struc (timecode)
Timecode_struc *timecode;
{
	timecode->thetime = 0LL;
}

void offset_timecode (time1, time2, offset)
Timecode_struc *time1, *time2, *offset;
{
  offset->thetime = time2->thetime - time1->thetime;

}

void copy_timecode (time_original, time_copy)
Timecode_struc *time_original, *time_copy;
{
	time_copy->thetime=time_original->thetime;
}

/* Find the timecode corresponding to given position in the system stream
   (assuming the SCR starts at 0 at the beginning of the stream 
   */

void bytepos_timecode(long long bytepos, Timecode_struc *ts)
{
	 ts->thetime = (bytepos*CLOCKS)/((long long)dmux_rate);
}


void make_timecode (timestamp, pointer)
double timestamp;
Timecode_struc *pointer;
{
  pointer->thetime = (clockticks) timestamp;
}


void add_to_timecode (add, to)
Timecode_struc *add;
Timecode_struc *to;
{
  to->thetime += add->thetime;
}


/*************************************************************************
    Kopiert einen TimeCode in einen Bytebuffer. Dabei wird er nach
    MPEG-Verfahren in bits aufgesplittet.

    Makes a Copy of a TimeCode in a Buffer, splitting it into bitfields
    for MPEG-1/2 DTS/PTS fields and MPEG-1 pack scr fields
*************************************************************************/

void buffer_dtspts_mpeg1scr_timecode (Timecode_struc *pointer,
									 unsigned char  marker,
									 unsigned char **buffer)

{
	clockticks thetime_base;
    unsigned char temp;
    unsigned int msb, lsb;
     
 	/* MPEG-1 uses a 90KHz clock, extended to 300*90KHz = 27Mhz in MPEG-2 */
	/* For these fields we only encode to MPEG-1 90Khz resolution... */
	
	thetime_base = pointer->thetime /300;
	msb = (thetime_base >> 32) & 1;
	lsb = (thetime_base & 0xFFFFFFFFLL);
		
    temp = (marker << 4) | (msb <<3) |
		((lsb >> 29) & 0x6) | 1;
    *((*buffer)++)=temp;
    temp = (lsb & 0x3fc00000) >> 22;
    *((*buffer)++)=temp;
    temp = ((lsb & 0x003f8000) >> 14) | 1;
    *((*buffer)++)=temp;
    temp = (lsb & 0x7f80) >> 7;
    *((*buffer)++)=temp;
    temp = ((lsb & 0x007f) << 1) | 1;
    *((*buffer)++)=temp;

}

/*************************************************************************
    Makes a Copy of a TimeCode in a Buffer, splitting it into bitfields
    for MPEG-2 pack scr fields  which use the full 27Mhz resolution
    
    Did they *really* need to put a 27Mhz
    clock source into the system stream.  Does anyone really need it
    for their decoders?  Get real... I guess they thought it might allow
    someone somewhere to save on a proper clock circuit.
*************************************************************************/


void buffer_mpeg2scr_timecode( Timecode_struc *pointer,
								unsigned char **buffer
							 )
{
 	clockticks thetime_base;
	unsigned int thetime_ext;
    unsigned char temp;
    unsigned int msb, lsb;
     
	thetime_base = pointer->thetime /300;
	thetime_ext =  pointer->thetime % 300;
	msb = (thetime_base>> 32) & 1;
	lsb = thetime_base & 0xFFFFFFFFLL;


      temp = (MARKER_MPEG2_SCR << 6) | (msb << 5) |
		  ((lsb >> 27) & 0x18) | 0x4 | ((lsb >> 28) & 0x3);
      *((*buffer)++)=temp;
      temp = (lsb & 0x0ff00000) >> 20;
      *((*buffer)++)=temp;
      temp = ((lsb & 0x000f8000) >> 12) | 0x4 |
             ((lsb & 0x00006000) >> 13);
      *((*buffer)++)=temp;
      temp = (lsb & 0x00001fe0) >> 5;
      *((*buffer)++)=temp;
      temp = ((lsb & 0x0000001f) << 3) | 0x4 |
             ((thetime_ext & 0x00000180) >> 7);
      *((*buffer)++)=temp;
      temp = ((thetime_ext & 0x0000007F) << 1) | 1;
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

}
