;
;  qdist1_mmx.s:  MMX optimized 4*4 sub-sampeld distance computation
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


global qdist1_MMX

; int qdist1_MMX(unsigned char *blk1,unsigned char *blk2,int qlx,int qh);

; eax = p1
; ebx = p2
; ecx = temp
; edx = qlx;
; esi = rowsleft

; mm0 = distance accumulator left block p1
; mm1 = distance accumulator right block p1
; mm2 = 0
; mm3 = right block of p1
; mm4 = left block of p1
; mm5 = p2
; mm6 = temp
; mm7 = temp

align 32
qdist1_MMX:
	push ebp		; save stack pointer
	mov ebp, esp		; so that we can do this

	push ebx
	push ecx
	push edx
	push esi     

	pxor mm0, mm0		; zero acculumator
	pxor mm1, mm1
	pxor mm2, mm2
	mov eax, [ebp+8]	; get p1
	mov ebx, [ebp+12]	; get p2
	mov edx, [ebp+16]	; get qlx
	mov esi, [ebp+20]	; get rowsleft
	jmp nextrowqd		; snap to it
align 32
nextrowqd:

		;;
		;; Beware loop obfuscated by interleaving to try to
		;; hide latencies...
		;; 
	movq mm4, [eax]				; mm4 =  first 4 bytes of p1 in words
	movq mm5, [ebx]             ; mm5 = 4 bytes of p2 in words
	movq mm3, mm4
	punpcklbw mm4, mm2			
	punpcklbw mm5, mm2
	
	movq mm7, mm4
	movq mm6, mm5
	psubusw mm7, mm5
	psubusw mm6, mm4
	
	add eax, edx		        ; update a pointer to next row
;	punpckhbw mm3, mm2			; mm3 = 2nd 4 bytes of p1 in words

	paddw   mm7, mm6
	paddw mm0, mm7				; Add absolute differences to left block accumulators
		
;	movq mm7,mm3
;	psubusw mm7, mm5
;	psubusw mm5, mm3

	add ebx, edx		; update a pointer to next row
	sub   esi, 1

;	paddw   mm7, mm5
;	paddw mm1, mm7				; Add absolute differences to right block accumulators
	

		
	jnz nextrowqd		

		;;		Sum the accumulators

	movq  mm4, mm0
	psrlq mm4, 32
	paddw mm0, mm4
	movq  mm6, mm0
	psrlq mm6, 16
	paddw mm0, mm6
	movd eax, mm0		; store return value

;	movq  mm4, mm1
;	psrlq mm4, 32
;	paddw mm1, mm4
;	movq  mm6, mm1
;	psrlq mm6, 16
;	paddw mm1, mm6
;	movd ebx, mm1

	and  eax, 0xffff		
;	sal  ebx, 16
;	or   eax, ebx
		
	pop esi
	pop edx
	pop ecx
	pop ebx

	pop ebp			; restore stack pointer

	emms			; clear mmx registers
	ret			; we now return you to your regular programming
