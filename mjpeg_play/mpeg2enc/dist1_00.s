;
;  dist1_00.s:  SSE optimized SAD for macroblocks
;
;  Original Copyright (C) 2000 Chris Atenasio <chris@crud.net>
;  Enhancements Copyright (C) 2000 Andrew Stevens <as@comlab.ox.ac.uk>
		;; Yes, I tried prefetch-ing.  It makes no difference or makes
		;; stuff *slower*.

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
dist1_00_0misalign:		
	mov ebx, [ebp+12]	; get p2
	mov edx, [ebp+16]	; get lx

	mov ecx, [ebp+20]	; get rowsleft
	jmp nextrow00sse
align 32
nextrow00sse:
	movq mm4, [eax]		; load first 8 bytes of p1 (row 1)
	psadbw mm4, [ebx]	; compare to first 8 bytes of p2 (row 1)
	movq mm5, [eax+8]	; load next 8 bytes of p1 (row 1)
	add eax, edx		; update pointer to next row
	paddd mm0, mm4		; accumulate difference
	
	psadbw mm5, [ebx+8]	; compare to next 8 bytes of p2 (row 1)
	add ebx, edx		; ditto
	paddd mm0, mm5		; accumulate difference


	movq mm6, [eax]		; load first 8 bytes of p1 (row 2)
	psadbw mm6, [ebx]	; compare to first 8 bytes of p2 (row 2)
	movq mm4, [eax+8]	; load next 8 bytes of p1 (row 2)
	add eax, edx		; update pointer to next row
	paddd mm0, mm6		; accumulate difference
	
	psadbw mm4, [ebx+8]	; compare to next 8 bytes of p2 (row 2)
	add ebx, edx		; ditto
	paddd mm0, mm4		; accumulate difference

	;psubd mm2, mm3		; decrease rowsleft
	;movq mm5, mm1		; copy distlim
	;pcmpgtd mm5, mm0	; distlim > dist?
	;pand mm2, mm5		; mask rowsleft with answer
	;movd ecx, mm2		; move rowsleft to ecx

	;add eax, edx		; update pointer to next row
	;add ebx, edx		; ditto
	
	;test ecx, ecx		; check rowsleft
	sub  ecx, 2
	jnz nextrow00sse

	movd eax, mm0		; store return value
	
	pop edx	
	pop ecx	
	pop ebx	

	pop ebp	
	emms
	ret	

global block_dist1_SSE

; void block_dist1_SSE(char *blk1,char *blk2,int lx,int h,int *weightvec);
; distlim unused - costs more to check than the savings of
; aborting the computation early from time to time...
; eax = p1
; ebx = p2
; ecx = unused
; edx = lx;
; edi = rowsleft
; esi = h
		
; mm0 = SAD (x+0,y+0)
; mm1 = SAD (x+2,y+0)
; mm2 = SAD (x+0,y+2)
; mm3 = SAD (x+2,y+2)
; mm4 = temp
; mm5 = temp
; mm6 = temp
; mm7 = temp						

align 32
block_dist1_SSE:
	push ebp					; save frame pointer
	mov ebp, esp				; link
	push eax
	push ebx
	push ecx
	push edx
	push edi
	push esi

	pxor mm0, mm0		; zero accumulators
	pxor mm1, mm1
	pxor mm2, mm2
	pxor mm3, mm3
	mov eax, [ebp+8]	; get p1
	mov ebx, [ebp+12]	; get p2
	mov edx, [ebp+16]	; get lx
	
	mov edi, [ebp+20]	; get rowsleft
	mov esi, edi

	jmp nextrow_block_d1
align 32
nextrow_block_d1:		

		;; Do the (+0,+0) SAD
		
	movq mm4, [eax]		; load 1st 8 bytes of p1
	movq mm6, mm4
	movq mm5, [ebx]
	psadbw mm4, mm5	; compare to 1st 8 bytes of p2 
	paddd mm0, mm4		; accumulate difference
	movq mm4, [eax+8]	; load 2nd 8 bytes of p1
	movq mm7, mm4		
	psadbw mm4, [ebx+8]	; compare to 2nd 8 bytes of p2 
	paddd mm0, mm4		; accumulate difference

		
    cmp edi, esi
	jz  firstrow0

		;; Do the (0,+2) SAD
	sub ebx, edx
	psadbw mm6, [ebx]	; compare to next 8 bytes of p2 (row 1)
	paddd mm2, mm6		; accumulate difference
	psadbw mm7, [ebx+8]	;  next 8 bytes of p1 (row 1)
	add ebx, edx
	paddd mm2, mm7	

firstrow0:

		;; Do the (+2,0) SAD
	
	movq mm4, [eax+1]
				
	movq mm6, mm4
	psadbw mm4, mm5	; compare to 1st 8 bytes of p2
	paddd mm1, mm4		; accumulate difference
	movq mm4, [eax+9]
	movq mm7, mm4
	psadbw mm4, [ebx+8]	; compare to 2nd 8 bytes of p2
	paddd mm1, mm4		; accumulate difference

    cmp edi, esi
	jz  firstrow1

		;; Do the (+2, +2 ) SAD
	sub ebx, edx
	psadbw mm6, [ebx]	; compare to 1st 8 bytes of prev p2 
	psadbw mm7, [ebx+8]	;  2nd 8 bytes of prev p2
	add ebx, edx
	paddd mm3, mm6		; accumulate difference
	paddd mm3, mm7	
firstrow1:		

	add eax, edx				; update pointer to next row
	add ebx, edx		; ditto
		
	sub edi, 1
	jnz near nextrow_block_d1

		;; Do the last row of the (0,+2) SAD

	movq mm4, [eax]		; load 1st 8 bytes of p1
	movq mm5, [eax+8]	; load 2nd 8 bytes of p1
	sub  ebx, edx
	psadbw mm4, [ebx]	; compare to next 8 bytes of p2 (row 1)
	psadbw mm5, [ebx+8]	;  next 8 bytes of p1 (row 1)
	paddd mm2, mm4		; accumulate difference
	paddd mm2, mm5
		
	movq mm4, [eax+1]
	movq mm5, [eax+9]
		
		;; Do the last row of rhw (+2, +2) SAD
	psadbw mm4, [ebx]	; compare to 1st 8 bytes of prev p2 
	psadbw mm5, [ebx+8]	;  2nd 8 bytes of prev p2
	paddd mm3, mm4		; accumulate difference
	paddd mm3, mm5
		

	mov eax, [ebp+24]			; Weightvec
	movd [eax+0], mm0
	movd [eax+4], mm1
	movd [eax+8], mm2
	movd [eax+12], mm3
		
	pop esi
	pop edi
	pop edx	
	pop ecx	
	pop ebx	
	pop eax
		
	pop ebp	
	emms
	ret	



global block_dist1_MMXE

; void block_dist1_MMXE(char *blk1,char *blk2,int lx,int h,int *weightvec);
; distlim unused - costs more to check than the savings of
; aborting the computation early from time to time...
; eax = p1
; ebx = p2
; ecx = unused
; edx = lx;
; edi = rowsleft
; esi = h
		
; mm0 = SAD (x+0,y+0),SAD (x+0,y+2)
; mm1 = SAD (x+2,y+0),SAD (x+2,y+2)
		
; mm4 = temp
; mm5 = temp
; mm6 = temp
; mm7 = temp						

align 32
block_dist1_MMXE:
	push ebp					; save frame pointer
	mov ebp, esp				; link
	push eax
	push ebx
	push ecx
	push edx
	push edi
	push esi

	mov eax, [ebp+8]	; get p1
	prefetcht0 [eax]
	pxor mm0, mm0		; zero accumulators
	pxor mm1, mm1
	mov ebx, [ebp+12]	; get p2
	mov edx, [ebp+16]	; get lx
	
	mov edi, [ebp+20]	; get rowsleft
	mov esi, edi

	jmp nextrow_block_e1
align 32
nextrow_block_e1:		

		;; Do the (+0,+0) SAD
	prefetcht0 [eax+edx]		
	movq mm4, [eax]		; load 1st 8 bytes of p1
	movq mm6, mm4
	movq mm5, [ebx]
	psadbw mm4, mm5	; compare to 1st 8 bytes of p2 
	paddd mm0, mm4		; accumulate difference
	movq mm4, [eax+8]	; load 2nd 8 bytes of p1
	movq mm7, mm4		
	psadbw mm4, [ebx+8]	; compare to 2nd 8 bytes of p2 
	paddd mm0, mm4		; accumulate difference

		
    cmp edi, esi
	jz  firstrowe0

		;; Do the (0,+2) SAD
	sub ebx, edx
	pshufw  mm0, mm0, 2*1 + 3 * 4 + 0 * 16 + 1 * 64
	movq   mm2, [ebx]
	psadbw mm6, mm2	    ; compare to next 8 bytes of p2 (row 1)
	paddd mm0, mm6		; accumulate difference
	movq  mm3, [ebx+8]
	psadbw mm7, mm3	;  next 8 bytes of p1 (row 1)
	add ebx, edx
	paddd mm0, mm7	
	pshufw  mm0, mm0, 2*1 + 3 * 4 + 0 * 16 + 1 * 64 
firstrowe0:

		;; Do the (+2,0) SAD
	
	movq mm4, [eax+1]
	movq mm6, mm4

	psadbw mm4, mm5	; compare to 1st 8 bytes of p2
	paddd mm1, mm4		; accumulate difference

	movq mm4, [eax+9]
	movq mm7, mm4

	psadbw mm4, [ebx+8]	; compare to 2nd 8 bytes of p2
	paddd mm1, mm4		; accumulate difference

    cmp edi, esi
	jz  firstrowe1

		;; Do the (+2, +2 ) SAD
	sub ebx, edx
	pshufw  mm1, mm1, 2*1 + 3 * 4 + 0 * 16 + 1 * 64 
	psadbw mm6, mm2	; compare to 1st 8 bytes of prev p2 
	psadbw mm7, mm3	;  2nd 8 bytes of prev p2
	add ebx, edx
	paddd mm1, mm6		; accumulate difference
	paddd mm1, mm7
	pshufw  mm1, mm1, 2*1 + 3 * 4 + 0 * 16 + 1 * 64 
firstrowe1:		

	add eax, edx				; update pointer to next row
	add ebx, edx		; ditto
		
	sub edi, 1
	jnz near nextrow_block_e1

		;; Do the last row of the (0,+2) SAD
	pshufw  mm0, mm0, 2*1 + 3 * 4 + 0 * 16 + 1 * 64
	movq mm4, [eax]		; load 1st 8 bytes of p1
	movq mm5, [eax+8]	; load 2nd 8 bytes of p1
	sub  ebx, edx
	psadbw mm4, [ebx]	; compare to next 8 bytes of p2 (row 1)
	psadbw mm5, [ebx+8]	;  next 8 bytes of p1 (row 1)
	paddd mm0, mm4		; accumulate difference
	paddd mm0, mm5

		
		;; Do the last row of rhw (+2, +2) SAD
	pshufw  mm1, mm1, 2*1 + 3 * 4 + 0 * 16 + 1 * 64				
	movq mm4, [eax+1]
	movq mm5, [eax+9]

	psadbw mm4, [ebx]	; compare to 1st 8 bytes of prev p2 
	psadbw mm5, [ebx+8]	;  2nd 8 bytes of prev p2
	paddd mm1, mm4		; accumulate difference
	paddd mm1, mm5
		

	mov eax, [ebp+24]			; Weightvec
	movd [eax+8], mm0
	pshufw  mm0, mm0, 2*1 + 3 * 4 + 0 * 16 + 1 * 64
	movd [eax+12], mm1
	pshufw  mm1, mm1, 2*1 + 3 * 4 + 0 * 16 + 1 * 64
	movd [eax+0], mm0
	movd [eax+4], mm1
		
	pop esi
	pop edi
	pop edx	
	pop ecx	
	pop ebx	
	pop eax
		
	pop ebp	
	emms
	ret	




				

global dist1_00_ASSE
		;; This is a special version that only does aligned accesses...
		;; Wonder if it'll make it faster on a P-III
		;; ANSWER:		 NO its slower

; int dist1_00(char *blk1,char *blk2,int lx,int h,int distlim);
; distlim unused - costs more to check than the savings of
; aborting the computation early from time to time...
; eax = p1
; ebx = p2
; ecx = rowsleft
; edx = lx;

; mm0 = distance accumulator
; mm1 = temp
; mm2 = right shift to adjust for mis-align
; mm3 = left shift to adjust for mis-align
; mm4 = temp
; mm5 = temp
; mm6 = temp


align 32
dist1_00_ASSE:
	push ebp					; save frame pointer
	mov ebp, esp				; link

	push ebx
	push ecx
	push edx
		
	pxor mm0, mm0		; zero acculumator

	mov eax, [ebp+8]	; get p1
	mov ebx, eax
	and ebx, 7					; Misalignment!
	cmp ebx, 0
	jz	near dist1_00_0misalign
	sub eax, ebx				; Align eax
	mov ecx, 8					; ecx = 8-misalignment
	sub ecx, ebx
	shl ebx, 3					; Convert into bit-shifts...
	shl ecx, 3					
	movd mm2, ebx				; mm2 = shift to start msb
	movd mm3, ecx				; mm3 = shift to end lsb

	mov ebx, [ebp+12]	; get p2
	mov edx, [ebp+16]	; get lx
	mov ecx, [ebp+20]	; get rowsleft
	jmp nextrow00ssea
align 32
nextrow00ssea:
	movq mm4, [eax]				; load first 8 bytes of aligned p1 (row 1)
	movq mm5, [eax+8]			; load next 8 bytes of aligned p1 (row 1)
	movq mm6, mm5
	psrlq mm4, mm2				; mm4 first 8 bytes of p1 proper
	psllq mm5, mm3
	por	  mm4, mm5
	psadbw mm4, [ebx]	; compare to first 8 bytes of p2 

	movq mm7, [eax+16]			; load last 8 bytes of aligned p1
	add eax, edx		; update pointer to next row
	psrlq mm6, mm2				; mm6 2nd 8 bytes of p1 proper
	psllq mm7, mm3
	por   mm6, mm7


	paddd mm0, mm4		; accumulate difference
	
	psadbw mm6, [ebx+8]	; compare to next 8 bytes of p2 (row 1)
	add ebx, edx		; ditto
	paddd mm0, mm6		; accumulate difference

	sub  ecx, 1
	jnz nextrow00ssea

	movd eax, mm0		; store return value

	pop edx	
	pop ecx	
	pop ebx	

	pop ebp	
	emms
	ret	
		