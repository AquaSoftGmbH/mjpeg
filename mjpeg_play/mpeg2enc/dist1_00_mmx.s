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


global dist1_00_MMX

; int dist1_MMX(unsigned char *blk1,unsigned char *blk2,int lx,int h, int distlim);
; N.b. distlim is *ignored* as testing for it is more expensive than the
; occasional saving by aborting the computionation early...
; esi = p1 (init:		blk1)
; edi = p2 (init:		blk2)
; ebx = distlim  
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
dist1_00_MMX:
	push ebp		; save frame pointer
	mov ebp, esp

	push ebx		; Saves registers (called saves convention in
	push ecx		; x86 GCC it seems)
	push edx		; 
	push esi
	push edi
		
	pxor mm0, mm0				; zero acculumators
	pxor mm6, mm6
	mov esi, [ebp+8]			; get p1
	mov edi, [ebp+12]			; get p2
	mov edx, [ebp+16]			; get lx
	mov  ecx, [ebp+20]			; get rowsleft
	;mov ebx, [ebp+24]	        ; distlim
	jmp nextrowmm00
align 32
nextrowmm00:
	movq mm4, [esi]		; load first 8 bytes of p1 row 
	movq mm5, [edi]		; load first 8 bytes of p2 row
		
	movq mm7, mm4       ; mm5 = abs(mm4-mm5)
	psubusb mm7, mm5
	psubusb mm5, mm4
	paddb   mm5, mm7

	;;  Add the abs(mm4-mm5) bytes to the accumulators
	movq mm2, [esi+8]		; load second 8 bytes of p1 row (interleaved)
	movq  mm7,mm5				; mm7 := [i :	B0..3, mm1]W
	punpcklbw mm7,mm6
	movq mm3, [edi+8]		; load second 8 bytes of p2 row (interleaved)
	paddw mm0, mm7
	punpckhbw mm5,mm6
	paddw mm0, mm5

		;; This is logically where the mm2, mm3 loads would go...
		
	movq mm7, mm2       ; mm3 = abs(mm2-mm3)
	psubusb mm7, mm3
	psubusb mm3, mm2
	paddb   mm3, mm7

	;;  Add the abs(mm4-mm5) bytes to the accumulators
	movq  mm7,mm3				; mm7 := [i :	B0..3, mm1]W
	punpcklbw mm7,mm6
	punpckhbw mm3,mm6
	paddw mm0, mm7
	
	add esi, edx		; update pointer to next row
	add edi, edx		; ditto	

	paddw mm0, mm3



	sub  ecx,1
	jnz near nextrowmm00
		
returnmm00:	

		;; Sum the Accumulators
	movq  mm5, mm0				; mm5 := [W0+W2,W1+W3, mm0
	psrlq mm5, 32
	movq  mm4, mm0
	paddw mm4, mm5

	movq  mm7, mm4              ; mm6 := [W0+W2+W1+W3, mm0]
	psrlq mm7, 16
	paddw mm4, mm7
	movd eax, mm4		; store return value
	and  eax, 0xffff

	pop edi
	pop esi	
	pop edx	
	pop ecx	
	pop ebx	

	pop ebp	

	emms			; clear mmx registers
	ret	
