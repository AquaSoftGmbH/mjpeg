;;; 
;;;  mblock_sad_mmxe.s:  
;;; 
;;; Enhanced MMX optimized Sum Absolute Differences routines for macroblocks
;;; (interpolated, 1-pel, 2*2 sub-sampled pel and 4*4 sub-sampled pel)
;
;  Original MMX sad_* Copyright (C) 2000 Chris Atenasio <chris@crud.net>
;  Enhanced MMX and rest Copyright (C) 2000 Andrew Stevens <as@comlab.ox.ac.uk>

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

SECTION .text

global	sad_00_mmxe:function

; int sad_00(char *blk1,char *blk2,int lx,int h,int distlim);
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


align 64
sad_00_mmxe:
	push ebp					; save frame pointer
	mov ebp, esp				; link

	push ebx

	pxor mm0, mm0		; zero acculumator

	mov eax, [ebp+8]	; get p1
sad_00_0misalign:		
	mov ebx, [ebp+12]	; get p2
	mov edx, [ebp+16]	; get lx

	mov ecx, [ebp+20]	; get rowsleft
	jmp .loop
align 16
.loop:
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

	sub  ecx, 2
	jnz .loop

	movd eax, mm0		; store return value
	
	pop ebx	

	pop ebp	
	emms
	ret	


				

global	sad_00_Ammxe:function			
		;; This is a special version that only does aligned accesses...
		;; Wonder if it'll make it faster on a P-III
		;; ANSWER:		 NO its slower hence no longer used.

; int sad_00(char *blk1,char *blk2,int lx,int h,int distlim);
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


align 64
sad_00_Ammxe:
	push ebp					; save frame pointer
	mov ebp, esp				; link

	push ebx
		
	pxor mm0, mm0		; zero acculumator

	mov eax, [ebp+8]	; get p1
	mov ebx, eax
	and ebx, 7					; Misalignment!
	cmp ebx, 0
	jz	near sad_00_0misalign
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
	; jmp .loop
align 16
.loop:
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
	jnz .loop

	movd eax, mm0		; store return value

	pop ebx	

	pop ebp	
	emms
	ret	


global	sad_01_mmxe:function

; int sad_01(char *blk1,char *blk2,int lx,int h);

; eax = p1
; ebx = p2
; ecx = counter temp
; edx = lx;

; mm0 = distance accumulator
; mm1 = distlim
; mm2 = rowsleft
; mm3 = 2 (rows per loop)
; mm4 = temp
; mm5 = temp
; mm6 = temp


align 64
sad_01_mmxe:
	push ebp	
	mov ebp, esp

	push ebx

	pxor mm0, mm0		; zero acculumator

	mov eax, [ebp+8]	; get p1
	mov ebx, [ebp+12]	; get p2
	mov edx, [ebp+16]	; get lx

	mov ecx, [ebp+20]	; get rowsleft
	jmp .loop		; snap to it
align 16
.loop:
	movq mm4, [eax]				; load first 8 bytes of p1 (row 1)
	pavgb mm4, [eax+1]			; Interpolate...
	psadbw mm4, [ebx]			; compare to first 8 bytes of p2 (row 1)
	paddd mm0, mm4				; accumulate difference

	movq mm5, [eax+8]			; load next 8 bytes of p1 (row 1)
	pavgb mm5, [eax+9]			; Interpolate
	psadbw mm5, [ebx+8]			; compare to next 8 bytes of p2 (row 1)
	paddd mm0, mm5				; accumulate difference

	add eax, edx				; update pointer to next row
	add ebx, edx				; ditto

	movq mm6, [eax]				; load first 8 bytes of p1 (row 2)
	pavgb mm6, [eax+1]			; Interpolate
	psadbw mm6, [ebx]	; compare to first 8 bytes of p2 (row 2)
	paddd mm0, mm6		; accumulate difference
	
	movq mm7, [eax+8]	; load next 8 bytes of p1 (row 2)
	pavgb mm7, [eax+9]
	psadbw mm7, [ebx+8]	; compare to next 8 bytes of p2 (row 2)
	paddd mm0, mm7		; accumulate difference

	add eax, edx		; update pointer to next row
	add ebx, edx		; ditto
	
	sub ecx, 2			; check rowsleft
	jnz .loop		; rinse and repeat

	movd eax, mm0		; store return value
	
	pop ebx	

	pop ebp			; restore stack pointer

	emms			; clear mmx registers
	ret			; we now return you to your regular programming


global	sad_10_mmxe:function

; int sad_10(char *blk1,char *blk2,int lx,int h);

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


align 64
sad_10_mmxe:
	push ebp		; save stack pointer
	mov ebp, esp

	push ebx

	pxor mm0, mm0		; zero acculumator

	mov eax, [ebp+8]	; get p1
	mov ebx, [ebp+12]	; get p2
	mov edx, [ebp+16]	; get lx
	mov ecx, [ebp+20]	; get rowsleft
	movq	mm4, [eax]
	movq	mm6, [eax+8]
	add	eax, edx
	jmp .loop		; snap to it
align 16
.loop:
	movq	mm5, [eax]	; load first 8 bytes of p1 (row 1)
	pavgb	mm4, mm5	; Interpolate...
	psadbw	mm4, [ebx]	; compare to first 8 bytes of p2 (row 1)
	paddd	mm0, mm4	; accumulate difference

	movq	mm7, [eax+8]	; load next 8 bytes of p1 (row 1)
	pavgb	mm6, mm7	; Interpolate
	psadbw	mm6, [ebx+8]	; compare to next 8 bytes of p2 (row 1)
	paddd	mm0, mm6	; accumulate difference

	add eax, edx		; update pointer to next row
	add ebx, edx		; ditto

	movq	mm4, [eax]	; load first 8 bytes of p1 (row 2)
	pavgb	mm5, mm4	; Interpolate
	psadbw	mm5, [ebx]	; compare to first 8 bytes of p2 (row 2)
	paddd	mm0, mm5	; accumulate difference
	
	movq	mm6, [eax+8]	; load next 8 bytes of p1 (row 2)
	pavgb	mm7, mm6
	psadbw	mm7, [ebx+8]	; compare to next 8 bytes of p2 (row 2)
	paddd	mm0, mm7	; accumulate difference

	add eax, edx		; update pointer to next row
	add ebx, edx		; ditto
	
	sub ecx, 2		; check rowsleft (we're doing 2 at a time)
	jnz .loop		; rinse and repeat

	movd eax, mm0		; store return value
	
	pop ebx	

	pop ebp			; restore stack pointer

	emms			; clear mmx registers
	ret			; we now return you to your regular programming


global	sad_11_mmxe:function

; int sad_11(char *blk1,char *blk2,int lx,int h);

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

	
align 64
sad_11_mmxe:
	push ebp		; save stack pointer

	push ebx		; save the pigs
	mov ebp, esp		; so that we can do this

	sub	esp, 8
	and	esp, -8

	pxor mm0, mm0		; zero acculumator
	pxor	mm1, mm1	; zero reg
	
	mov	eax, 2+2*65536	; load 2 into all words
	mov	[esp], eax
	mov	[esp+4], eax

	mov edx, [ebp+20]	; get lx

	mov eax, [ebp+12]	; get p1
	mov ebx, [ebp+16]	; get p2
	mov ecx, [ebp+24]	; get rowsleft
	
	movq	mm6, [eax]	; load first 8 bytes of p1 (row 1)
	movq	mm7, mm6
	punpcklbw mm6, mm1
	punpckhbw mm7, mm1
	movq	mm2, [eax+1]
	movq	mm3, mm2
	punpcklbw mm2, mm1
	punpckhbw mm3, mm1
	paddw	mm6, mm2
	paddw	mm7, mm3
	
	add	eax, edx
	jmp .loop		; snap to it
align 16
.loop:
	movq	mm2, [eax]
	movq	mm3, mm2
	punpcklbw mm2, mm1
	punpckhbw mm3, mm1

	movq	mm4, [eax+1]
	movq	mm5, mm4
	punpcklbw mm4, mm1
	punpckhbw mm5, mm1

	paddw	mm6, [esp]
	paddw	mm7, [esp]

	paddw	mm2, mm4
	paddw	mm3, mm5

	paddw	mm6, mm2
	paddw	mm7, mm3

	psrlw	mm6, 2
	psrlw	mm7, 2

	packuswb mm6, mm7
	psadbw	mm6, [ebx]

	paddd	mm0, mm6

	movq	mm6, mm2
	movq	mm7, mm3
	
	add	eax, edx
	add	ebx, edx

	sub	ecx, 1				; check rowsleft
	jnz	.loop				; rinse and repeat

	mov eax, [ebp+12]	; get p1
	mov ebx, [ebp+16]	; get p2
	mov ecx, [ebp+24]	; get rowsleft

	add	eax, 8
	add	ebx, 8
	
	movq	mm6, [eax]	; load first 8 bytes of p1 (row 1)
	movq	mm7, mm6
	punpcklbw mm6, mm1
	punpckhbw mm7, mm1
	movq	mm2, [eax+1]
	movq	mm3, mm2
	punpcklbw mm2, mm1
	punpckhbw mm3, mm1
	paddw	mm6, mm2
	paddw	mm7, mm3
	
	add	eax, edx
	jmp .loop2		; snap to it
align 16
.loop2:
	movq	mm2, [eax]
	movq	mm3, mm2
	punpcklbw mm2, mm1
	punpckhbw mm3, mm1

	movq	mm4, [eax+1]
	movq	mm5, mm4
	punpcklbw mm4, mm1
	punpckhbw mm5, mm1

	paddw	mm6, [esp]
	paddw	mm7, [esp]

	paddw	mm2, mm4
	paddw	mm3, mm5

	paddw	mm6, mm2
	paddw	mm7, mm3

	psrlw	mm6, 2
	psrlw	mm7, 2

	packuswb mm6, mm7
	psadbw	mm6, [ebx]

	paddd	mm0, mm6

	movq	mm6, mm2
	movq	mm7, mm3
	
	add	eax, edx
	add	ebx, edx

	sub	ecx, 1				; check rowsleft
	jnz	.loop2				; rinse and repeat

	movd	eax, mm0			; store return value

	mov	esp, ebp
	pop ebx			
	
	pop ebp			; restore stack pointer

	emms			; clear mmx registers
	ret			; we now return you to your regular programming

global	sad_sub22_mmxe:function

; int sad_sub22_mmxe(unsigned char *blk1,unsigned char *blk2,int flx,int fh);

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


align 64
sad_sub22_mmxe:
	push ebp		; save frame pointer
	mov ebp, esp

	push ebx	

	pxor mm0, mm0		; zero acculumator

	mov eax, [ebp+8]	; get p1
	mov ebx, [ebp+12]	; get p2
	mov edx, [ebp+16]	; get lx

	mov ecx, [ebp+20]
	jmp .loop
align 16
.loop:
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
	jnz .loop

	movd eax, mm0
	
	pop ebx	

	pop ebp

	emms
	ret





global	sad_sub44_mmxe:function

; int sad_sub44_mmxe(unsigned char *blk1,unsigned char *blk2,int qlx,int qh);

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


align 64
sad_sub44_mmxe:
	push ebp
	mov ebp, esp

	push ebx
	push esi     

	pxor mm0, mm0		; zero acculumator
	pxor mm1, mm1				
	pxor mm2, mm2
	mov eax, [ebp+8]	; get p1
	mov ebx, [ebp+12]	; get p2
	mov edx, [ebp+16]	; get qlx

	mov esi, [ebp+20]	; get rowsleft
	; jmp .loop		; snap to it
align 16
.loop: 
	movq mm4, [eax]				; load 8 bytes of p1 (two blocks!)
	add eax, edx		; update pointer to next row
	movq mm6, mm4				  ;
	mov  ecx, [ebx]       ; load 4 bytes of p2
    punpcklbw mm4, mm2			; mm4 = bytes 0..3 p1 (spaced out)
	movd mm5, ecx
	punpcklbw mm5, mm2      ; mm5 = bytes 0..3 p2  (spaced out)
	psadbw mm4, mm5	    		; compare to left block
	add ebx, edx		; ditto

;	punpckhbw mm6, mm2          ; mm6 = bytes 4..7 p1 (spaced out)

	paddd mm0, mm4				; accumulate difference left block

;	psadbw mm6,mm5				; compare to right block
	

;	paddd mm1, mm6				; accumulate difference right block
		
	sub esi, 1
	jnz .loop

	movd eax, mm0
;	movd ebx, mm1				
;	sal  ebx, 16
;	or   eax, ebx
	
	pop esi
	pop ebx

	pop ebp			; restore stack pointer

	emms			; clear mmx registers
	ret			; we now return you to your regular programming


;;; 
;;;  mblock_*nearest4_sad_mmxe.s:  
;;; 
;;; Enhanced MMX optimized Sum Absolute Differences routines for
;;; quads macroblocks offset by (0,0) (0,1) (1,0) (1,1) pel
;;; 

;;; Explanation: the motion compensation search at 1-pel and 2*2 sub-sampled
;;; evaluates macroblock quads.  A lot of memory accesses can be saved
;;; if each quad is done together rather than each macroblock in the
;;; quad handled individually.

;;; TODO:		Really there ought to be MMX versions and the function's
;;; specification should be documented...
;
; Copyright (C) 2000 Andrew Stevens <as@comlab.ox.ac.uk>	


;;; CURRENTLY not used but used in testing as reference for tweaks...
global	mblockq_sad_REF:function

; void mblockq_sad_REF(char *blk1,char *blk2,int lx,int h,int *weightvec);
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

align 64
mblockq_sad_REF:
	push ebp					; save frame pointer
	mov ebp, esp				; link
	push ebx
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

	; jmp .nextrow_block_d1
align 16
.nextrow_block_d1:		

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
	jz  .firstrow0

		;; Do the (0,+2) SAD
	sub ebx, edx
	psadbw mm6, [ebx]	; compare to next 8 bytes of p2 (row 1)
	paddd mm2, mm6		; accumulate difference
	psadbw mm7, [ebx+8]	;  next 8 bytes of p1 (row 1)
	add ebx, edx
	paddd mm2, mm7	

.firstrow0:

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
	jz  .firstrow1

		;; Do the (+2, +2 ) SAD
	sub ebx, edx
	psadbw mm6, [ebx]	; compare to 1st 8 bytes of prev p2 
	psadbw mm7, [ebx+8]	;  2nd 8 bytes of prev p2
	add ebx, edx
	paddd mm3, mm6		; accumulate difference
	paddd mm3, mm7	
.firstrow1:		

	add eax, edx				; update pointer to next row
	add ebx, edx		; ditto
		
	sub edi, 1
	jnz .nextrow_block_d1

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
	pop ebx	
		
	pop ebp	
	emms
	ret	



global	mblock_nearest4_sads_mmxe:function

; void mblock_nearest4_sads_mmxe(char *blk1,char *blk2,int lx,int h,int *weightvec);

; eax = temp min SAD
; ebx = p2
; ecx = p1
; edx = lx;
; edi = rowsleft
; esi = SAD cutoff
			
; mm0 = SAD (x+0,y+0),SAD (x+2,y+0)
; mm1 = SAD (x+0,y+2),SAD (x+2,y+2)

; mm2 = [ecx]   cache
; mm3 = [ecx+8] cache
; mm4 = [ecx+1] cache
; mm5 = [ecx+9] cache
; mm6 = [ebx]   cache
; mm7 = [ebx+8] cache			

align 64
mblock_nearest4_sads_mmxe:
	push ebp		; save frame pointer
	mov ebp, esp		; link
	push ebx
	push edi
	push esi

	mov ecx, [ebp+8]	; get p1
	pxor mm0, mm0		; zero accumulators
	pxor mm1, mm1
	mov ebx, [ebp+12]	; get p2
	mov edx, [ebp+16]	; get lx
	
	mov edi, [ebp+20]	; get rowsleft
	mov	esi, [ebp+28]

	movq	mm2, [ecx]
	movq	mm3, [ecx+8]
	movq	mm4, [ecx+1]
	movq	mm5, [ecx+9]

	;; the loop is too close to issue a jump, just no-op our way there
	; jmp	.loop
align 16
.loop:

		;; Do the (+0,+0) SAD
	movq	mm6, [ebx]		; 0.0.1.0 ld,  1st 8
	movq	mm7, [ebx+8]		; 0.0.2.0 ld,  2nd 8
	psadbw	mm2, mm6		; 0.0.1.1 SAD, 1st 8
	psadbw	mm3, mm7		; 0.0.2.1 SAD, 2nd 8
	paddd	mm0, mm2		; 0.0.1.2 acc, 1st 8
	paddd	mm0, mm3		; 0.0.2.2 acc, 2nd 8

		;; Do the (+2,0) SAD
	pshufw	mm0, mm0, 2*1 + 3*4 + 0*16 + 1*64 ; 2.0.0.0 shuffle
	
	psadbw	mm4, mm6		; 2.0.1.0 SAD, 1st 8
	psadbw	mm5, mm7		; 2.0.2.0 SAD, 2nd 8
	paddd	mm0, mm4		; 2.0.1.1 acc, 1st 8
	paddd	mm0, mm5		; 2.0.2.1 acc, 2nd 8

	pshufw	mm0, mm0, 2*1 + 3*4 + 0*16 + 1*64 ; 2.0.3.0 shuffle

	add	ecx, edx		; update pointer to next row
	add	ebx, edx		; update pointer to next row
		
	movq	mm3, mm1		; exit.0
	pminsw	mm3, mm0		; exit.1
	pshufw	mm2, mm3, 2*1 + 3*4 + 0*16 + 1*64 ; exit.2
	pminsw	mm3, mm2		; exit.3
	movd	eax, mm3		; exit.4
	cmp	eax, esi		; exit.5
	jg	.earlyexit		; exit.6
	
		;; Do the (0,+2) SAD
	movq	mm4, [ecx]		; 0.2.1.0 ld,  1st 8
	movq	mm5, [ecx+8]		; 0.2.2.0 ld,  2nd 8
	movq	mm2, mm4		; 0.2.1.1 mov, 1st 8
	movq	mm3, mm5		; 0.2.2.1 mov, 2nd 8
	psadbw	mm4, mm6		; 0.2.1.2 SAD, 1st 8
	psadbw	mm5, mm7		; 0.2.2.2 SAD, 2nd 8
	paddd	mm1, mm4		; 0.2.1.3 acc, 1st 8	
	paddd	mm1, mm5		; 0.2.2.3 acc, 2nd 8
	
		;; Do the (+2,+2) SAD
	pshufw	mm1, mm1, 2*1 + 3*4 + 0*16 + 1*64 ; 2.2.0.0 shuffle

	movq	mm4, [ecx+1]		; 2.2.1.0 ld,  1st 8
	movq	mm5, [ecx+9]		; 2.2.2.0 ld,  2nd 8
	psadbw	mm6, mm4		; 2.2.1.1 SAD, 1st 8
	psadbw	mm7, mm5		; 2.2.2.1 SAD, 2nd 8
	paddd	mm1, mm6		; 2.2.1.2 acc, 1st 8
	paddd	mm1, mm7		; 2.2.2.2 acc, 2nd 8
	
	pshufw	mm1, mm1, 2*1 + 3*4 + 0*16 + 1*64 ; 2.2.3.0 shuffle

	sub	edi,1			; loop.0 (sub avoids flag register stall on P4)
	jnz	.loop			; loop.1

	mov	ecx, [ebp+24]			; Weightvec
	movq	[ecx+0], mm0
	movq	[ecx+8], mm1

	pminsw	mm0,mm1
	pshufw	mm1,mm0, 2*1 + 3*4 + 0*16 + 1*64
	pminsw	mm0,mm1
	movd	eax,mm0

.earlyexit:
	pop esi		
	pop edi
	pop ebx	
		
	pop ebp	
	emms
	ret

global	mblock_sub22_nearest4_sads_mmxe:function

; void mblock_sub22_nearest4_sads_mmxe(unsigned char *blk1,unsigned char *blk2,int flx,int fh, int* resvec);

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


align 64
mblock_sub22_nearest4_sads_mmxe:
	push ebp		; save frame pointer
	mov ebp, esp
	push ebx	

	pxor mm0, mm0		; zero acculumator
	pxor mm1, mm1		; zero acculumator

	mov eax, [ebp+8]	; get p1
	mov ebx, [ebp+12]	; get p2
	mov edx, [ebp+16]	; get lx
	mov ecx, [ebp+20]
	movq mm2, [eax+edx]	; row[+0] load
	movq mm3, [eax+edx+1]	; row[+2] load
	; jmp .loop
align 16
.loop:
	movq	mm5, [ebx]	; load previous row reference block
		
	psadbw	mm2, mm5	; (+0,+2) SAD
	psadbw	mm3, mm5	; (+2,+2) SAD
	paddd	mm1, mm2	; (+0,+2) acc		
	pshufw	mm3, mm3, 2*1 + 3*4 + 0*16 + 1*64 ; (+2,+2) shufl
	paddd	mm1, mm3	; (+2,+2) acc

	movq	mm6, [eax]	; row[+0] load
	movq	mm3, [eax+1]	; row[+2] load
	sub	eax, edx
	sub	ebx, edx

	movq	mm2, mm6	; (+0,+0) ld
	psadbw	mm6, mm5	; (+0,+0) SAD
	psadbw	mm5, mm3	; (+2,+0) SAD
	paddd	mm0, mm6	; (+0,+0) acc
	pshufw	mm5, mm5, 2*1 + 3*4 + 0*16 + 1*64 ; (+2,0) shufl
	paddd	mm0, mm5	; (+2,+0) acc

	sub	ecx, 1
	jnz	.loop

	mov  eax, [ebp+24]
	movq [eax+0], mm0
	movq [eax+8], mm1
	pop ebx	
	pop ebp

	emms
	ret

		
