/* detect.c, this file is part of the
 * AltiVec optimized library for MJPEG tools MPEG-1/2 Video Encoder
 * Copyright (C) 2002  James Klicman <james@klicman.org>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * This AltiVec detection code relies on the operating system to provide and
 * illegal instruction signal if AltiVec is not present. It is known to work
 * on Mac OS X and Linux.
 */

#ifdef HAVE_ALTIVEC_H
#include <altivec.h>
#endif

#include <stdio.h>
#include <signal.h>
#include <setjmp.h>

static sigjmp_buf jmpbuf;

static void sig_ill(int sig)
{
    siglongjmp(jmpbuf, 1);
}

/*
 * Functions containing AltiVec code have additional VRSAVE instructions
 * at the beginning and end of the function. To avoid executing any AltiVec
 * instructions before our signal handler is setup the AltiVec code is
 * encapsulated in a function.
 *
 * The automatic VRSAVE instructions can be disabled with
 * #pragma altivec_vrsave off
 *
 * Storing vector registers to memory shouldn't alter the state of the vectors
 * or the vector unit. The following function contains a single vector stvx
 * instruction.
 */
#pragma altivec_vrsave off
static void copy_v0()
{
    register vector unsigned char v0 asm ("v0");
    union {
	vector unsigned char align16;
	unsigned char v0[16];
    } copy;

    vec_st(v0, 0, copy.v0);
}

int detect_altivec()
{
    volatile int detected = 0; /* volatile (modified after sigsetjmp) */
    struct sigaction act, oact;

    act.sa_handler = sig_ill;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;

    if (sigaction(SIGILL, &act, &oact)) {
	perror("sigaction");
	return 0;
    }

    if (sigsetjmp(jmpbuf, 1))
	goto noAltiVec;

    /* try to read an AltiVec register */ 
    copy_v0();

    detected = 1;

noAltiVec:
    if (sigaction(SIGILL, &oact, (struct sigaction *)0))
	perror("sigaction");

    return detected;
}
