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
; mm2 = rowsleft
; mm3 = 0
; mm4 = temp
; mm5 = temp
; mm6 = temp


align 32
qdist1_SSE:
	push ebp		; save stack pointer
	mov ebp, esp		; so that we can do this

	push ebx
	push ecx
	push edx
	push esi     

	pxor mm0, mm0		; zero acculumator
	pxor mm1, mm1
	pxor mm3, mm3
	mov eax, [ebp+8]	; get p1
	mov ebx, [ebp+12]	; get p2
	mov edx, [ebp+16]	; get qlx

	mov esi, [ebp+20]	; get rowsleft
	jmp nextrowqd		; snap to it
align 32
nextrowqd:
	mov ecx, [eax]				; load first 4 bytes of p1
	movd mm4, ecx

	mov  ecx, [ebx]             ; load 4 bytes of p2
    punpcklbw mm4, mm3			; mm4 = bytes 0..3 p1 (spaced out)
	movd mm5, ecx
	punpcklbw mm5, mm3          ; mm5 is 4 bytes p2 spaced out

	add eax, edx		; update pointer to next row
	add ebx, edx		; ditto
	sub esi, 1
		
	psadbw mm4, mm5	    		; compare to first 4 bytes of p1
	paddd mm0, mm4				; accumulate difference left block
	

	test esi, esi		; check rowsleft
	jnz nextrowqd		; rinse and repeat

	movd eax, mm0

	pop esi
	pop edx
	pop ecx
	pop ebx

	pop ebp			; restore stack pointer

	emms			; clear mmx registers
	ret			; we now return you to your regular programming
