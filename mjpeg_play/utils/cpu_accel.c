/*
 * cpu_accel.c
 * Copyright (C) 1999-2000 Aaron Holtzman <aholtzma@ess.engr.uvic.ca>
 *
 * This file is part of mpeg2dec, a free MPEG-2 video stream decoder.
 *
 * mpeg2dec is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * mpeg2dec is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <config.h>
#include <stdio.h>
#include <signal.h>
#include <setjmp.h>
#include <stdlib.h>
#ifdef	HAVE_MALLOC_H
#include <malloc.h>
#endif
#include <unistd.h>
#include <errno.h>
#include "cpu_accel.h"
#include "mjpeg_logging.h"

#ifdef HAVE_ALTIVEC
extern int detect_altivec();
#endif

#ifdef HAVE_X86CPU 

/* Some miscelaneous stuff to allow checking whether SSE instructions cause
   illegal instruction errors.
*/

static sigjmp_buf sigill_recover;

static RETSIGTYPE sigillhandler(int sig )
{
	siglongjmp( sigill_recover, 1 );
}

typedef RETSIGTYPE (*__sig_t)(int);

static int testsseill()
{
	int illegal;
#if defined(__CYGWIN__)
	/* SSE causes a crash on CYGWIN, apparently.
	   Perhaps the wrong signal is being caught or something along
	   those line ;-) or maybe SSE itself won't work...
	*/
	illegal = 1;
#else
	__sig_t old_handler = signal( SIGILL, sigillhandler);
	if( sigsetjmp( sigill_recover, 1 ) == 0 )
	{
		asm ( "movups %xmm0, %xmm0" );
		illegal = 0;
	}
	else
		illegal = 1;
	signal( SIGILL, old_handler );
#endif
	return illegal;
}

static int x86_accel (void)
{
    int32_t eax, ebx, ecx, edx;
    int32_t AMD;
    int32_t caps;

	/* Slightly weirdified cpuid that preserves the ebx and edi required
	   by gcc for PIC offset table and frame pointer */
	   
#define cpuid(op,eax,ebx,ecx,edx)	\
    asm ( "pushl %%ebx\n" \
	      "cpuid\n" \
	      "movl   %%ebx, %%esi\n" \
	      "popl   %%ebx\n"  \
	 : "=a" (eax),			\
	   "=S" (ebx),			\
	   "=c" (ecx),			\
	   "=d" (edx)			\
	 : "a" (op)			\
	 : "cc", "edi")

    asm ("pushfl\n\t"
	 "popl %0\n\t"
	 "movl %0,%1\n\t"
	 "xorl $0x200000,%0\n\t"
	 "pushl %0\n\t"
	 "popfl\n\t"
	 "pushfl\n\t"
	 "popl %0"
         : "=a" (eax),
	       "=c" (ecx)
	 :
	 : "cc");


    if (eax == ecx)		// no cpuid
	return 0;

    cpuid (0x00000000, eax, ebx, ecx, edx);
    if (!eax)			// vendor string only
	return 0;

    AMD = (ebx == 0x68747541) && (ecx == 0x444d4163) && (edx == 0x69746e65);

    cpuid (0x00000001, eax, ebx, ecx, edx);
    if (! (edx & 0x00800000))	// no MMX
	return 0;

    caps = ACCEL_X86_MMX;
    /* If SSE capable CPU has same MMX extensions as AMD
	   and then some. However, to use SSE O.S. must have signalled
	   it use of FXSAVE/FXRSTOR through CR4.OSFXSR and hence FXSR (bit 24)
	   here
	*/
    if ((edx & 0x02000000))	
		caps = ACCEL_X86_MMX | ACCEL_X86_MMXEXT;
	if( (edx & 0x03000000) == 0x03000000 )
	{
		/* Check whether O.S. has SSE support... has to be done with
		   exception 'cos those Intel morons put the relevant bit
		   in a reg that is only accesible in ring 0... doh! 
		*/
		if( !testsseill() )
			caps |= ACCEL_X86_SSE;
	}

    cpuid (0x80000000, eax, ebx, ecx, edx);
    if (eax < 0x80000001)	// no extended capabilities
		return caps;

    cpuid (0x80000001, eax, ebx, ecx, edx);

    if (edx & 0x80000000)
	caps |= ACCEL_X86_3DNOW;

    if (AMD && (edx & 0x00400000))	// AMD MMX extensions
	{
		caps |= ACCEL_X86_MMXEXT;
	}

    return caps;
}
#endif

int32_t cpu_accel (void)
{
#ifdef HAVE_X86CPU 
    static int got_accel = 0;
    static int accel;

    if (!got_accel) {
		got_accel = 1;
		accel = x86_accel ();
    }

    return accel;
#elif defined(HAVE_ALTIVEC)
    return detect_altivec();
#else
    return 0;
#endif
}

/*****************************
 *
 * Allocate memory aligned to suit SIMD 
 *
 ****************************/

#define powerof2(x)     ((((x)-1)&(x))==0)

#if	!defined(HAVE_POSIX_MEMALIGN)

int
posix_memalign(void **ptr, size_t alignment, size_t size)
{
	void *mem;

	if	(alignment % sizeof (void *) != 0 || !powerof2(alignment) != 0)
		return(EINVAL);
	mem = malloc((size + alignment - 1) & ~(alignment - 1));
	if	(mem != NULL)
	{
		*ptr = mem;
		return(0);
	}
	return(ENOMEM);
}
#endif

#if	!defined(HAVE_MEMALIGN)
void *
memalign(size_t alignment, size_t size)
{

	if 	(alignment % sizeof (void *) || !powerof2(alignment))
	{
		errno = EINVAL;
		return(NULL);
	}
	return(malloc((size + alignment - 1) & ~(alignment - 1)));
}
#endif

/***********************
 * Implement fmax() for systems which do not have it.  Not a strictly
 * conforming implementation - we don't bother checking for NaN which if
 * mpeg2enc gets means big trouble I suspect ;)
************************/

#if	!defined(HAVE_FMAX)
double
fmax(double x, double y)
{
	if	(x > y)
		return(x);
	return(y);
}
#endif

void *bufalloc( size_t size )
{
	static size_t simd_alignment = 16;
	static int bufalloc_init = 0;
	int  pgsize;
	void *buf = NULL;

	if( !bufalloc_init )
	{
#ifdef HAVE_X86CPU 
		if( (cpu_accel() &  (ACCEL_X86_SSE|ACCEL_X86_3DNOW)) != 0 )
		{
			simd_alignment = 64;
			bufalloc_init = 1;
		}
#endif		
	}
		
	pgsize = sysconf(_SC_PAGESIZE);
/*
 * If posix_memalign fails it could be a broken glibc that caused the error,
 * so try again with a page aligned memalign request
*/
	if (posix_memalign( &buf, simd_alignment, size))
		buf = memalign(pgsize, size);
	if (buf && ((int)buf & (simd_alignment - 1)))
	{
		free(buf);
		buf = memalign(pgsize, size);
	}
	if (buf == NULL)
		mjpeg_error_exit1("malloc of %d bytes failed", size);
	if ((int)buf & (simd_alignment - 1))
		mjpeg_error_exit1("could not allocate %d bytes aligned on a %d byte boundary", size, simd_alignment);
	return buf;
}
