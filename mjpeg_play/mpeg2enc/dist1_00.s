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


global dist1_00_SSE

; int dist1_00(char *blk1,char *blk2,int lx,int h,int distlim);
; distlim unused - costs more to check than the savings of
; aborting the computation early from time to time...
; eax = p1
; ebx = p2
; ecx = rowsleft
; edx = lx;

; mm0 = distance accumulator
; mm1 = temp
; mm2 = temp
; mm3 = temp
; mm4 = temp
; mm5 = temp
; mm6 = temp


align 32
dist1_00_SSE:
	push ebp					; save frame pointer
	mov ebp, esp				; link

	push ebx
	push ecx
	push edx

	pxor mm0, mm0		; zero acculumator

	mov eax, [ebp+8]	; get p1
	mov ebx, [ebp+12]	; get p2
	mov edx, [ebp+16]	; get lx

	;movd mm2, [ebp+20]	; get rowsleft
	;mov ecx,2		; 1 loop does 2 rows
	;movd mm3, ecx		; MMX(TM) it!
	;movd mm1, [ebp+24]	; get distlim
	mov ecx, [ebp+20]	; get rowsleft
	jmp nextrow00		; snap to it
align 32
nextrow00:
	movq mm4, [eax]		; load first 8 bytes of p1 (row 1)
	psadbw mm4, [ebx]	; compare to first 8 bytes of p2 (row 1)
	paddd mm0, mm4		; accumulate difference
	
	movq mm5, [eax+8]	; load next 8 bytes of p1 (row 1)
	psadbw mm5, [ebx+8]	; compare to next 8 bytes of p2 (row 1)
	paddd mm0, mm5		; accumulate difference

	add eax, edx		; update pointer to next row
	add ebx, edx		; ditto

	movq mm6, [eax]		; load first 8 bytes of p1 (row 2)
	psadbw mm6, [ebx]	; compare to first 8 bytes of p2 (row 2)
	paddd mm0, mm6		; accumulate difference
	
	movq mm4, [eax+8]	; load next 8 bytes of p1 (row 2)
	psadbw mm4, [ebx+8]	; compare to next 8 bytes of p2 (row 2)
	paddd mm0, mm4		; accumulate difference

	;psubd mm2, mm3		; decrease rowsleft
	;movq mm5, mm1		; copy distlim
	;pcmpgtd mm5, mm0	; distlim > dist?
	;pand mm2, mm5		; mask rowsleft with answer
	;movd ecx, mm2		; move rowsleft to ecx

	add eax, edx		; update pointer to next row
	add ebx, edx		; ditto
	
	;test ecx, ecx		; check rowsleft
	sub  ecx, 2
	jnz nextrow00		; rinse and repeat

	movd eax, mm0		; store return value
	
	pop edx	
	pop ecx	
	pop ebx	

	pop ebp	
	emms
	ret	
