;
;  fdist1_00.s:  SSE optimized 2*2 sub-sampled SAD computation.
;
;  Copyright (C) 2000 Andrew Stevens <as@comlab.ox.ac.uk>
		;; Yes - I tried prefetching.  Either makes no difference
		;; or makes things *slower* on Duron and PIII.
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
	push ebp		; save frame pointer
	mov ebp, esp

	push ebx	
	push ecx
	push edx	

	pxor mm0, mm0		; zero acculumator

	mov eax, [ebp+8]	; get p1
	mov ebx, [ebp+12]	; get p2
	mov edx, [ebp+16]	; get lx

	mov ecx, [ebp+20]
	jmp nextrowfd
align 32
nextrowfd:
	movq   mm4, [eax]	 ; load first 8 bytes of p1 (row 1) 
	add eax, edx		; update pointer to next row
	psadbw mm4, [ebx]	; compare to first 8 bytes of p2 (row 1)
	add ebx, edx		; ditto
	paddd  mm0, mm4		; accumulate difference
	

	movq mm6, [eax]		; load first 8 bytes of p1 (row 2)
	add eax, edx		; update pointer to next row
	psadbw mm6, [ebx]	; compare to first 8 bytes of p2 (row 2)
	add ebx, edx		; ditto
	paddd mm0, mm6		; accumulate difference
	

	sub ecx, 2
	jnz nextrowfd

	movd eax, mm0
	
	pop edx	
	pop ecx	
	pop ebx	

	pop ebp

	emms
	ret



global block_dist22_SSE

; void block_dist22_SSE(unsigned char *blk1,unsigned char *blk2,int flx,int fh, int* resvec);

; eax = p1
; ebx = p2
; ecx = counter temp
; edx = flx;

; mm0 = distance accumulator
; mm1 = distance accumulator
; mm2 = previous p1 row
; mm3 = previous p1 displaced by 1 byte...
; mm4 = temp
; mm5 = temp
; mm6 = temp
; mm7 = temp / 0 if first row 0xff otherwise


align 32
block_dist22_SSE:
	push ebp		; save frame pointer
	mov ebp, esp
	push eax
	push ebx	
	push ecx
	push edx	

	pxor mm0, mm0		; zero acculumator
	pxor mm1, mm1		; zero acculumator
	pxor mm2, mm2		; zero acculumator
	pxor mm3, mm3		; zero acculumator						

	mov eax, [ebp+8]	; get p1
	mov ebx, [ebp+12]	; get p2
	mov edx, [ebp+16]	; get lx
	mov ecx, [ebp+20]
	movq mm2, [eax+edx]
	movq mm3, [eax+edx+1]
	jmp nextrowbd22
align 32
nextrowbd22:
	movq   mm5, [ebx]			; load previous row reference block
								; mm2 /mm3 containts current row target block
		
	psadbw mm2, mm5				; Comparse (x+0,y+2)
	paddd  mm1, mm2
		
	psadbw mm3, mm5				; Compare (x+2,y+2)
	pshufw  mm1, mm1, 2*1 + 3 * 4 + 0 * 16 + 1 * 64
	paddd  mm1, mm3

	pshufw  mm1, mm1, 2*1 + 3 * 4 + 0 * 16 + 1 * 64 					

	movq mm2, [eax]				; Load current row traget block into mm2 / mm3
	movq mm6, mm2
	movq mm3, [eax+1]
	sub	   eax, edx
	sub	   ebx, edx
	prefetcht0 [eax]
	movq mm7, mm3		

	psadbw	mm6, mm5			; Compare (x+0,y+0)
	paddd   mm0, mm6
	pshufw  mm0, mm0, 2*1 + 3 * 4 + 0 * 16 + 1 * 64
	psadbw  mm7, mm5			; Compare (x+2,y+0)
	paddd   mm0, mm7
	pshufw  mm0, mm0, 2*1 + 3 * 4 + 0 * 16 + 1 * 64

	sub ecx, 1
	jnz nextrowbd22

	mov  eax, [ebp+24]
	movq [eax+0], mm0
	movq [eax+8], mm1
	pop edx	
	pop ecx	
	pop ebx	
	pop eax
	pop ebp

	emms
	ret
		