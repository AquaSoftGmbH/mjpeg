;
;  fdist1_00.s:  MMX1 optimised 8*8 word absolute difference sum
;
;  Copyright (C) 2000 Andrew Stevens <as@comlab.ox.ac.uk>
;  Based on original GPL Copyright (C) 2000 Chris Atenasio <chris@crud.net>

;
;  This program is free software; you can redistribute it and/or
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


global fdist1_MMX

; int fdist1_MMX(unsigned char *blk1,unsigned char *blk2,int lx,int h);

; eax = p1 (init:		blk1)
; ebx = p2 (init:		 blk2)
; ecx = rowsleft (init:	 h)
; edx = lx;

; mm0 = distance accumulators (4 words)
; mm1 = temp 
; mm2 = temp 
; mm3 = temp
; mm4 = temp
; mm5 = temp 
; mm6 = 0
; mm7 = temp


align 32
fdist1_MMX:
	push ebp		; save stack pointer
	mov ebp, esp	; so that we can do this

	push ebx		; Saves registers (called saves convention in
	push ecx		; x86 GCC it seems)
	push edx		; 

	pxor mm0, mm0				; zero acculumators
	pxor mm6, mm6
	mov eax, [ebp+8]			; get p1
	mov ebx, [ebp+12]			; get p2
	mov edx, [ebp+16]			; get lx

	mov  ecx, [ebp+20]			; get rowsleft

	jmp nextrow					; snap to it
align 32
nextrow:
	movq mm4, [eax]		; load 8 bytes of p1 
	movq mm5, [ebx]		; load 8 bytes of p2
		
	movq mm7, mm4       ; mm5 = abs(*p1-*p2)
	psubusb mm7, mm5
	psubusb mm5, mm4
	add eax, edx		; update pointer to next row
	paddb   mm5,mm7	

		;;  Add the mm5 bytes to the accumulatores
	movq  mm7,mm5			
	punpcklbw mm7,mm6
	paddw mm0, mm7
	punpckhbw mm5,mm6
	add ebx, edx		; update pointer to next row
	paddw mm0, mm5
	
	movq mm4, [eax]		; load 8 bytes of p1 (next row) 
	movq mm5, [ebx]		; load 8 bytes of p2 (next row)
		
	movq mm7, mm4       ; mm5 = abs(*p1-*p2)
	psubusb mm7, mm5
	psubusb mm5, mm4
	add eax, edx		; update pointer to next row
	paddb   mm5,mm7	

		;;  Add the mm5 bytes to the accumulatores
	movq  mm7,mm5				
	punpcklbw mm7,mm6
	add ebx, edx		; update pointer to next row
	paddw mm0, mm7              
	punpckhbw mm5,mm6
	sub  ecx,2
	paddw mm0, mm5


	jnz nextrow

		;; Sum the Accumulators
	movq  mm1, mm0
	psrlq mm1, 16
	movq  mm2, mm0
	psrlq mm2, 32
	movq  mm3, mm0
	psrlq mm3, 48
	paddw mm0, mm1
	paddw mm2, mm3
	paddw mm0, mm2
		
	movd eax, mm0		; store return value
	and  eax, 0xffff
	
	pop edx			; pop pop
	pop ecx			; fizz fizz
	pop ebx			; ia86 needs a fizz instruction

	pop ebp			; restore stack pointer

	emms			; clear mmx registers
	ret			; we now return you to your regular programming
