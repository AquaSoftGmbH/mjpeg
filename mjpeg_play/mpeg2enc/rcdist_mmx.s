;
; rcdist.s:  MMX1 optimised 16*16 row and column sum difference....
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


global rcdist_mmx

; int rcdist_mmx(unsigned char *org, unsigned short *blksums,int lx,int h);
; N.b. comparison row/col sums come as 16 row sums followed by 16 column sums...

; esi = p1 (init:		blk1)
; edi =
; eax = sum absolute differences accumulator...
; ebx = 
; ecx = rowsleft (init:	 h)
; edx = lx;

; mm0 = col acc cols 0..3
; mm1 = col acc cols 4..7
; mm2 = col acc cols 8..11
; mm3 = col acc cols 12..15
; mm4 = row sum acc...
; mm5
; mm6
; mm7

align 32
rcdist_mmx:
	push ebp		; save frame pointer
	mov ebp, esp

	push ebx		; Saves registers (called saves convention in
	push ecx		; x86 GCC it seems)
	push edx		; 
	push esi
	push edi
	pxor mm0, mm0				; zero column acculumators
	pxor mm1, mm1
	pxor mm2, mm2
	pxor mm3, mm3			
	pxor mm6, mm6
	pxor mm7, mm7
	mov esi, [ebp+8]			; get p1
	mov edi, [ebp+12]			; get p2
	mov edx, [ebp+16]			; get lx
	mov ecx, [ebp+20]		  ; get rowsleft
	xor eax, eax              ; Zero accumulator
	jmp rcdrows
align 32
rcdrows:
	pxor mm4, mm4            ; Zero row acc
	;;
	;; Update row and col sums... with 1st 8 bytes of row
	;;
	movq mm6, [esi]	
	movq mm5, mm6
	punpcklbw  mm6, mm7     
	punpckhbw  mm5, mm7	
	paddw mm0, mm6           ; col sum 1st 4 bytes
	paddw mm4, mm6           ; row sum 1st 4 bytes
	paddw mm1, mm5           ; col sum 2nd 4 bytes
	paddw mm4, mm5           ; row sum 2nd 4 bytes

	;;
	;; Update row and col sums... with 2nd 8 bytes of row
	;;
	movq mm6, [esi+8]	
	movq mm5, mm6
	punpcklbw  mm6, mm7     
	punpckhbw  mm5, mm7	
	paddw mm2, mm6           ; col sum 1st 4 bytes
	paddw mm4, mm6           ; row sum 1st 4 bytes
	paddw mm3, mm5           ; col sum 2nd 4 bytes
	paddw mm4, mm5           ; row sum 2nd 4 bytes

	;;
	;; Update accumulator with absolute row difference...
	;;
	movq mm5, mm4
	psrlq mm5, 32
	paddw mm4, mm5

	movq  mm5, mm4
	psrlq mm5, 16
	paddw mm4, mm5
	
	mov   bx, [edi]
	movd  mm5, ebx
	
	movq  mm6, mm4
	psubusw mm6, mm5
	psubusw mm5, mm4
	paddw   mm6, mm5
	movd    ebx, mm6
	and     ebx, 0xffff
	add     eax, ebx   

	add esi, edx
	add edi, 2
	
	sub ecx, 1
	jnz rcdrows
	

	;;
	;; Now accumulate the column sum differences... in mm5 conveniently edi points to the first
	;; column entry...
	;; 
	
	movq mm6, mm0
	movq mm5, [edi]
	psubusw mm0, mm5
	psubusw mm5, mm6
	paddw   mm5, mm0
	
	movq mm4, mm1
	movq mm6, [edi+8]
	psubusw mm1, mm6
	psubusw mm6, mm4
	paddw   mm5, mm1
	paddw   mm5, mm6
	
	movq mm4, mm2
	movq mm6, [edi+16]
	psubusw mm2, mm6
	psubusw mm6, mm4
	paddw   mm5, mm2
	paddw   mm5, mm6

	movq mm4, mm3
	movq mm6, [edi+24]
	psubusw mm3, mm6
	psubusw mm6, mm4
	paddw   mm5, mm3
	paddw   mm5, mm6
		
	;;
	;; Final accumulation...
	;;
	
	movq mm6, mm5
	psrlq mm6, 32
	paddw mm6, mm5
	movq mm5, mm6
	psrlq mm6, 16
	paddw mm6, mm5
	
	movd  ebx, mm6
	and   ebx, 0xffff
	add   eax, ebx
		
	
	pop edi
	pop esi	
	pop edx	
	pop ecx	
	pop ebx	

	pop ebp	

	emms			; clear mmx registers
	ret	
