/* putvlc.cc, generation of variable length codes                            */

/* Copyright (C) 1996, MPEG Software Simulation Group. All Rights Reserved. */

/*
 * Disclaimer of Warranty
 *
 * These software programs are available to the user without any license fee or
 * royalty on an "as is" basis.  The MPEG Software Simulation Group disclaims
 * any and all warranties, whether express, implied, or statuary, including any
 * implied warranties or merchantability or of fitness for a particular
 * purpose.  In no event shall the copyright-holder be liable for any
 * incidental, punitive, or consequential damages of any kind whatsoever
 * arising from the use of these programs.
 *
 * This disclaimer of warranty extends to the user of these programs and user's
 * customers, employees, agents, transferees, successors, and assigns.
 *
 * The MPEG Software Simulation Group does not represent or warrant that the
 * programs furnished hereunder are free of infringement of any third-party
 * patents.
 *
 * Commercial implementations of MPEG-1 and MPEG-2 video, including shareware,
 * are subject to royalty fees to patent holders.  Many of these patents are
 * general enough such that they are unavoidable regardless of implementation
 * design.
 *
 */

/*  Elementary stream writer base class and file descriptor writing instance */
/*  (C) 2000/2001 Andrew Stevens */

/*  These modifications are free software; you can redistribute them
 *  and/or modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2 of
 *  the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 */



#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "global.h"
#include "mjpeg_logging.h"
#include "mpeg2encoder.hh"
#include "elemstrmwriter.hh"
#include "mpeg2coder.hh"
#include "vlc.h"


/* generate variable length code for DC coefficient (7.2.1) */
void MPEG2Coder::PutDC(const sVLCtable *tab, int val)
{
	int absval, size;

	absval = abs(val);
	if (absval>encparams.dctsatlim)
	{
		/* should never happen */
		mjpeg_error("Internal: DC value out of range (%d)",val);
		abort();
	}

	/* compute dct_dc_size */
	size = 0;

	while (absval)
	{
		absval >>= 1;
		size++;
	}

	/* generate VLC for dct_dc_size (Table B-12 or B-13) */
	writer.PutBits(tab[size].code,tab[size].len);

	/* append fixed length code (dc_dct_differential) */
	if (size!=0)
	{
		if (val>=0)
			absval = val;
		else
			absval = val + (1<<size) - 1; /* val + (2 ^ size) - 1 */
		writer.PutBits(absval,size);
	}
}

/* generate variable length code for DC coefficient (7.2.1) */
int MPEG2Coder::DC_bits(const sVLCtable *tab, int val)
{
	int absval, size;

	absval = abs(val);

	/* compute dct_dc_size */
	size = 0;

	while (absval)
	{
		absval >>= 1;
		size++;
	}

	/* generate VLC for dct_dc_size (Table B-12 or B-13) */
	return tab[size].len+size;
}


/* generate variable length code for first coefficient
 * of a non-intra block (7.2.2.2) */
void MPEG2Coder::PutACfirst(int run, int val)
{
	if (run==0 && (val==1 || val==-1)) /* these are treated differently */
		writer.PutBits(2|(val<0),2); /* generate '1s' (s=sign), (Table B-14, line 2) */
	else
		PutAC(run,val,0); /* no difference for all others */
}

/* generate variable length code for other DCT coefficients (7.2.2) */
void MPEG2Coder::PutAC(int run, int signed_level, int vlcformat)
{
	int level, len;
	const VLCtable *ptab = NULL;

	level = abs(signed_level);

	/* make sure run and level are valid */
	if (run<0 || run>63 || level==0 || level>encparams.dctsatlim)
	{
		if( signed_level != -(encparams.dctsatlim+1)) 	/* Negative range is actually 1 more */
		{
			mjpeg_error("Internal: AC value out of range (run=%d, signed_level=%d)",
						run,signed_level);
			abort();
		}
	}

	len = 0;

	if (run<2 && level<41)
	{
		/* vlcformat selects either of Table B-14 / B-15 */
		if (vlcformat)
			ptab = &dct_code_tab1a[run][level-1];
		else
			ptab = &dct_code_tab1[run][level-1];

		len = ptab->len;
	}
	else if (run<32 && level<6)
	{
		/* vlcformat selects either of Table B-14 / B-15 */
		if (vlcformat)
			ptab = &dct_code_tab2a[run-2][level-1];
		else
			ptab = &dct_code_tab2[run-2][level-1];

		len = ptab->len;
	}

	if (len!=0) /* a VLC code exists */
	{
		writer.PutBits(ptab->code,len);
		writer.PutBits(signed_level<0,1); /* sign */
	}
	else
	{
		/* no VLC for this (run, level) combination: use escape coding (7.2.2.3) */
		writer.PutBits(1l,6); /* Escape */
		writer.PutBits(run,6); /* 6 bit code for run */
		if (encparams.mpeg1)
		{
			/* ISO/IEC 11172-2 uses a 8 or 16 bit code */
			if (signed_level>127)
				writer.PutBits(0,8);
			if (signed_level<-127)
				writer.PutBits(128,8);
			writer.PutBits(signed_level,8);
		}
		else
		{
			/* ISO/IEC 13818-2 uses a 12 bit code, Table B-16 */
			writer.PutBits(signed_level,12);
		}
	}
}

/* generate variable length code for other DCT coefficients (7.2.2) */
int MPEG2Coder::AC_bits(int run, int signed_level, int vlcformat)
{
	int level;
	const VLCtable *ptab;

	level = abs(signed_level);

	if (run<2 && level<41)
	{
		/* vlcformat selects either of Table B-14 / B-15 */
		if (vlcformat)
			ptab = &dct_code_tab1a[run][level-1];
		else
			ptab = &dct_code_tab1[run][level-1];

		return ptab->len+1;
	}
	else if (run<32 && level<6)
	{
		/* vlcformat selects either of Table B-14 / B-15 */
		if (vlcformat)
			ptab = &dct_code_tab2a[run-2][level-1];
		else
			ptab = &dct_code_tab2[run-2][level-1];

		return ptab->len+1;
	}
	else
	{
		return 12+12;
	}
}

/* generate variable length code for macroblock_address_increment (6.3.16) */
void MPEG2Coder::PutAddrInc(int addrinc)
{
	while (addrinc>33)
	{
		writer.PutBits(0x08,11); /* macroblock_escape */
		addrinc-= 33;
	}
	assert( addrinc >= 1 && addrinc <= 33 );
	writer.PutBits(addrinctab[addrinc-1].code,addrinctab[addrinc-1].len);
}

int MPEG2Coder::AddrInc_bits(int addrinc)
{
	int bits = 0;
	while (addrinc>33)
	{
		bits += 11;
		addrinc-= 33;
	}
	return bits + addrinctab[addrinc-1].len;
}

/* generate variable length code for macroblock_type (6.3.16.1) */
void MPEG2Coder::PutMBType(int pict_type, int mb_type)
{
	writer.PutBits(mbtypetab[pict_type-1][mb_type].code,
				   mbtypetab[pict_type-1][mb_type].len);
}

int MPEG2Coder::MBType_bits( int pict_type, int mb_type)
{
	return mbtypetab[pict_type-1][mb_type].len;
}

/* generate variable length code for motion_code (6.3.16.3) */
void MPEG2Coder::PutMotionCode(int motion_code)
{
	int abscode;

	abscode = abs( motion_code );
	writer.PutBits(motionvectab[abscode].code,motionvectab[abscode].len);
	if (motion_code!=0)
		writer.PutBits(motion_code<0,1); /* sign, 0=positive, 1=negative */
}

int MPEG2Coder::MotionCode_bits( int motion_code )
{
	int abscode = (motion_code>=0) ? motion_code : -motion_code; 
	return 1+motionvectab[abscode].len;
}

/* generate variable length code for dmvector[t] (6.3.16.3), Table B-11 */
void MPEG2Coder::PutDMV(int dmv)
{
	if (dmv==0)
		writer.PutBits(0,1);
	else if (dmv>0)
		writer.PutBits(2,2);
	else
		writer.PutBits(3,2);
}

int MPEG2Coder::DMV_bits(int dmv)
{
	return dmv == 0 ? 1 : 2;
}

/* generate variable length code for coded_block_pattern (6.3.16.4)
 *
 * 4:2:2, 4:4:4 not implemented
 */
void MPEG2Coder::PutCPB(int cbp)
{
	writer.PutBits(cbptable[cbp].code,cbptable[cbp].len);
}

int MPEG2Coder::CBP_bits(int cbp)
{
	return cbptable[cbp].len;
}

/* 
 * Local variables:
 *  c-file-style: "stroustrup"
 *  tab-width: 4
 *  indent-tabs-mode: nil
 * End:
 */


