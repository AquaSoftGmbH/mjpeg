;
;  qdist1_sse.s:  SSE optimized 4*4 sub-sampeld distance computation
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


global qdist1_SSE

; int qdist1_SSE(unsigned char *blk1,unsigned char *blk2,int qlx,int qh);

; eax = p1
; ebx = p2
; ecx = temp
; edx = qlx;
; esi = rowsleft

; mm0 = distance accumulator left block p1
; mm1 = distance accumulator right block p1
; mm2 = 0
; mm3 = 0
; mm4 = temp
; mm5 = temp
; mm6 = temp


align 32
qdist1_SSE:
	push ebp
	mov ebp, esp

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
	movq mm4, [eax]				; load 8 bytes of p1 (two blocks!)
	add eax, edx		; update pointer to next row
	movq mm6, mm4				  ;
	mov  ecx, [ebx]       ; load 4 bytes of p2
    punpcklbw mm4, mm2			; mm4 = bytes 0..3 p1 (spaced out)
	movd mm5, ecx
	punpcklbw mm5, mm2      ; mm5 = bytes 0..3 p2  (spaced out)
	psadbw mm4, mm5	    		; compare to left block
	add ebx, edx		; ditto

	punpckhbw mm6, mm2          ; mm6 = bytes 4..7 p1 (spaced out)

	paddd mm0, mm4				; accumulate difference left block

	psadbw mm6,mm5				; compare to right block
	

	paddd mm1, mm6				; accumulate difference right block
		
	sub esi, 1
	jnz nextrowqd

	movd eax, mm0
	movd ebx, mm1				
	sal  ebx, 16
	or   eax, ebx
	
	pop esi
	pop edx
	pop ecx
	pop ebx

	pop ebp			; restore stack pointer

	emms			; clear mmx registers
	ret			; we now return you to your regular programming
