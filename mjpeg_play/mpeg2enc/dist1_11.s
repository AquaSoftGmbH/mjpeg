;
;  dist1_10.s:  SSE optimized version of distance comparision
;				half-pixel interpolation in x and y axes
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


global dist1_11_SSE

; int dist1_11(char *blk1,char *blk2,int lx,int h,int distlim);

; eax = p1
; ebx = p2
; ecx = counter temp
; edx = lx;
; edi = p1+lx  

		  
; mm0 = distance accumulator
; mm1 = distlim
; mm2 = rowsleft
; mm3 = 2 (rows per loop)
; mm4 = temp
; mm5 = temp
; mm6 = temp


align 32
dist1_11_SSE:
	push ebp		; save stack pointer
	mov ebp, esp		; so that we can do this

	push ebx		; save the pigs
	push ecx		; make them squeal
	push edx		; lets have pigs for every meal

	pxor mm0, mm0		; zero acculumator

	mov eax, [ebp+8]	; get p1
	mov ebx, [ebp+12]	; get p2
	mov edx, [ebp+16]	; get lx
	mov edi, eax
	add edi, edx
	movd mm2, [ebp+20]	; get rowsleft
	movd mm1, [ebp+24]	; get distlim
	mov ecx,2		; 1 loop does 2 rows
	movd mm3, ecx		; MMX(TM) it!
	jmp nextrow11		; snap to it
align 32
nextrow11:
	movq mm4, [eax]				; load first 8 bytes of p1 (row 1)
	pavgb mm4, [edi]			; Interpolate...
	movq mm5, [eax+1]
	pavgb mm5, [edi+1]
	pavgb mm4, mm5
	psadbw mm4, [ebx]			; compare to first 8 bytes of p2 (row 1)
	paddd mm0, mm4				; accumulate difference

	movq mm6, [eax+8]			; load next 8 bytes of p1 (row 1)
	pavgb mm6, [edi+8]			; Interpolate
	movq mm7, [eax+9]
	pavgb mm7, [edi+9]
	pavgb mm6, mm7
	psadbw mm6, [ebx+8]			; compare to next 8 bytes of p2 (row 1)
	paddd mm0, mm6				; accumulate difference

	add eax, edx				; update pointer to next row
	add ebx, edx				; ditto
	add edi, edx

	movq mm4, [eax]				; load first 8 bytes of p1 (row 1)
	pavgb mm4, [edi]			; Interpolate...
	movq mm5, [eax+1]
	pavgb mm5, [edi+1]
	pavgb mm4, mm5
	psadbw mm4, [ebx]			; compare to first 8 bytes of p2 (row 1)
	paddd mm0, mm4				; accumulate difference

	movq mm6, [eax+8]			; load next 8 bytes of p1 (row 1)
	pavgb mm6, [edi+8]			; Interpolate
	movq mm7, [eax+9]
	pavgb mm7, [edi+9]
	pavgb mm6, mm7
	psadbw mm6, [ebx+8]			; compare to next 8 bytes of p2 (row 1)
	paddd mm0, mm6				; accumulate difference
		
	psubd mm2, mm3		; decrease rowsleft
	movq mm5, mm1		; copy distlim
	pcmpgtd mm5, mm0	; distlim > dist?
	pand mm2, mm5		; mask rowsleft with answer
	movd ecx, mm2		; move rowsleft to ecx

	add eax, edx		; update pointer to next row
	add ebx, edx		; ditto
	add edi, edx

			
	test ecx, ecx				; check rowsleft
	jnz near nextrow11			; rinse and repeat

	movd eax, mm0				; store return value
	
	pop edx			; pop pop
	pop ecx			; fizz fizz
	pop ebx			; ia86 needs a fizz instruction

	pop ebp			; restore stack pointer

	emms			; clear mmx registers
	ret			; we now return you to your regular programming
