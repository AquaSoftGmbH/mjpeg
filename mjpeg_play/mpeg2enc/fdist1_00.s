;
;  dist1_00.s:  SSE optimized version of the noninterpolative section of
;               dist1(), part of the motion vector detection code.  Runs
;               mpeg2enc -r11 at about 450% the speed of the original on
;               my Athlon.
;
;  Copyright (C) 2000 Chris Atenasio <chris@crud.net>

;
;  This program is free software; you can reaxstribute it and/or
;  modify it under the terms of the GNU General Public License
;  as published by the Free Software Foundation; either version 2
;  of the License, or (at your option) any later version.
;
;  This program is distributed in the hope that it will be useful,
;  but WITHOUT ANY WARRANTY; without even the implied warranty of
;  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;  GNU General Public License for more details.
;
;  You should have received a copy of the GNU General Public License
;  along with this program; if not, write to the Free Software
;  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
;
;
;


global fdist1_SSE

; int fdist1_SSE(unsigned char *blk1,unsigned char *blk2,int flx,int fh);

; eax = p1
; ebx = p2
; ecx = counter temp
; edx = flx;

; mm0 = distance accumulator
; mm2 = rowsleft
; mm3 = 2 (rows per loop)
; mm4 = temp
; mm5 = temp
; mm6 = temp


align 32
fdist1_SSE:
	push ebp		; save stack pointer
	mov ebp, esp		; so that we can do this

	push ebx		; save the pigs
	push ecx		; make them squeal
	push edx		; lets have pigs for every meal

	pxor mm0, mm0		; zero acculumator

	mov eax, [ebp+8]	; get p1
	mov ebx, [ebp+12]	; get p2
	mov edx, [ebp+16]	; get lx

	movd mm2, [ebp+20]	; get rowsleft
	mov ecx,2		; 1 loop does 2 rows of 8 8-bit words
	movd mm3, ecx		; MMX(TM) it!
	jmp nextrowfd		; snap to it
align 32
nextrowfd:
	movq mm4, [eax]		; load first 8 bytes of p1 (row 1)
	psadbw mm4, [ebx]	; compare to first 8 bytes of p2 (row 1)
	paddd mm0, mm4		; accumulate difference
	
	add eax, edx		; update pointer to next row
	add ebx, edx		; ditto

	movq mm6, [eax]		; load first 8 bytes of p1 (row 2)
	psadbw mm6, [ebx]	; compare to first 8 bytes of p2 (row 2)
	paddd mm0, mm6		; accumulate difference
	

	psubd mm2, mm3		; decrease rowsleft
	movd ecx, mm2		; move rowsleft to ecx

	add eax, edx		; update pointer to next row
	add ebx, edx		; ditto
	
	test ecx, ecx		; check rowsleft
	jnz nextrowfd		; rinse and repeat

	movd eax, mm0		; store return value
	
	pop edx			; pop pop
	pop ecx			; fizz fizz
	pop ebx			; ia86 needs a fizz instruction

	pop ebp			; restore stack pointer

	emms			; clear mmx registers
	ret			; we now return you to your regular programming
