;
;  dist1_10.s:  SSE optimized version of distance comparision
;				half-pixel interpolation in y axis
;  Copyright (C) 2000 Andrew Stevens <as@comlab.ox.ac.uk>
;  From code Copyright (C) 2000 Chris Atenasio <chris@crud.net>
		

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


global dist1_10_SSE

; int dist1_10(char *blk1,char *blk2,int lx,int h);

; eax = p1
; ebx = p2
; ecx = counter temp
; edx = lx;
; edi = p1+lx  

; mm0 = distance accumulator
; mm2 = rowsleft
; mm3 = 2 (rows per loop)
; mm4 = temp
; mm5 = temp
; mm6 = temp


align 32
dist1_10_SSE:
	push ebp		; save stack pointer
	mov ebp, esp

	push ebx
	push ecx
	push edx
	push edi

	pxor mm0, mm0		; zero acculumator

	mov eax, [ebp+8]	; get p1
	mov ebx, [ebp+12]	; get p2
	mov edx, [ebp+16]	; get lx
	mov edi, eax
	add edi, edx
	mov ecx, [ebp+20]	; get rowsleft
	jmp nextrow10		; snap to it
align 32
nextrow10:
	movq mm4, [eax]				; load first 8 bytes of p1 (row 1)
	pavgb mm4, [edi]			; Interpolate...
	psadbw mm4, [ebx]			; compare to first 8 bytes of p2 (row 1)
	paddd mm0, mm4				; accumulate difference

	movq mm5, [eax+8]			; load next 8 bytes of p1 (row 1)
	pavgb mm5, [edi+8]			; Interpolate
	psadbw mm5, [ebx+8]			; compare to next 8 bytes of p2 (row 1)
	paddd mm0, mm5				; accumulate difference

	add eax, edx				; update pointer to next row
	add ebx, edx				; ditto
	add edi, edx

	movq mm6, [eax]				; load first 8 bytes of p1 (row 2)
	pavgb mm6, [edi]			; Interpolate
	psadbw mm6, [ebx]	; compare to first 8 bytes of p2 (row 2)
	paddd mm0, mm6		; accumulate difference
	
	movq mm7, [eax+8]	; load next 8 bytes of p1 (row 2)
	pavgb mm7, [edi+8]
	psadbw mm7, [ebx+8]	; compare to next 8 bytes of p2 (row 2)
	paddd mm0, mm7		; accumulate difference

	psubd mm2, mm3		; decrease rowsleft

	add eax, edx		; update pointer to next row
	add ebx, edx		; ditto
	add edi, edx
	
	sub ecx, 2			; check rowsleft (we're doing 2 at a time)
	jnz nextrow10		; rinse and repeat

	movd eax, mm0		; store return value
	
	pop edi
	pop edx	
	pop ecx	
	pop ebx	

	pop ebp			; restore stack pointer

	emms			; clear mmx registers
	ret			; we now return you to your regular programming
