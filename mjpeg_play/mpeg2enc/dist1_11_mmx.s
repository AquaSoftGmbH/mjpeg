;
;;;  dist1_01_mmx.s:  
;;; 
;  Copyright (C) 2000 Andrew Stevens <as@comlab.ox.ac.uk>

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


global dist1_11_MMX

; int dist1_11_MMX(unsigned char *p1,unsigned char *p2,int lx,int h);

; esi = p1 (init:		blk1)
; edi = p2 (init:		blk2)
; ebx = p1+lx
; ecx = rowsleft (init:	 h)
; edx = lx;

; mm0 = distance accumulators (4 words)
; mm1 = bytes p2
; mm2 = bytes p1
; mm3 = bytes p1+lx
; I'd love to find someplace to stash p1+1 and p1+lx+1's bytes
; but I don't think thats going to happen in iA32-land...
; mm4 = temp 4 bytes in words interpolating p1, p1+1
; mm5 = temp 4 bytes in words from p2
; mm6 = temp comparison bit mask p1,p2
; mm7 = temp comparison bit mask p2,p1


align 32
dist1_11_MMX:
	push ebp		; save stack pointer
	mov ebp, esp	; so that we can do this

	push ebx		; Saves registers (called saves convention in
	push ecx		; x86 GCC it seems)
	push edx		; 
	push esi
	push edi
		
	pxor mm0, mm0				; zero acculumators

	mov esi, [ebp+8]			; get p1
	mov edi, [ebp+12]			; get p2
	mov edx, [ebp+16]			; get lx
	mov ecx, [ebp+20]			; rowsleft := h
	mov ebx, esi
    add ebx, edx		
	jmp nextrowmm11					; snap to it
align 32
nextrowmm11:

		;; 
		;; First 8 bytes of row
		;; 
		
		;; First 4 bytes of 8

	movq mm4, [esi]             ; mm4 := first 4 bytes p1
	pxor mm7, mm7
	movq mm2, mm4				;  mm2 records all 8 bytes
	punpcklbw mm4, mm7            ;  First 4 bytes p1 in Words...
	
	movq mm6, [ebx]			    ;  mm6 := first 4 bytes p1+lx
	movq mm3, mm6               ;  mm3 records all 8 bytes
	punpcklbw mm6, mm7
	paddw mm4, mm6              


	movq mm5, [esi+1]			; mm5 := first 4 bytes p1+1
	punpcklbw mm5, mm7            ;  First 4 bytes p1 in Words...
	paddw mm4, mm5		
	movq mm6, [ebx+1]           ;  mm6 := first 4 bytes p1+lx+1
	punpcklbw mm6, mm7
	paddw mm4, mm6

	psrlw mm4, 2	            ; mm4 := First 4 bytes interpolated in words
		
	movq mm5, [edi]				; mm5:=first 4 bytes of p2 in words
	movq mm1, mm5
	punpcklbw mm5, mm7
			
	movq  mm7,mm4
	pcmpgtw mm7,mm5		; mm7 := [i : W0..3,mm4>mm5]

	movq  mm6,mm4		; mm6 := [i : W0..3, (mm4-mm5)*(mm4-mm5 > 0)]
 	psubw mm6,mm5
	pand  mm6, mm7

	paddw mm0, mm6				; Add to accumulator

	movq  mm6,mm5       ; mm6 := [i : W0..3,mm5>mm4]
	pcmpgtw mm6,mm4	    
 	psubw mm5,mm4		; mm5 := [i : B0..7, (mm5-mm4)*(mm5-mm4 > 0)]
	pand  mm5, mm6		

	paddw mm0, mm5				; Add to accumulator

		;; Second 4 bytes of 8
	
	movq mm4, mm2		    ; mm4 := Second 4 bytes p1 in words
	pxor  mm7, mm7
	punpckhbw mm4, mm7			
	movq mm6, mm3			; mm6 := Second 4 bytes p1+1 in words  
	punpckhbw mm6, mm7
	paddw mm4, mm6          

	movq mm5, [esi+1]			; mm5 := first 4 bytes p1+1
	punpckhbw mm5, mm7          ;  First 4 bytes p1 in Words...
	paddw mm4, mm5
	movq mm6, [ebx+1]           ;  mm6 := first 4 bytes p1+lx+1
	punpckhbw mm6, mm7
	paddw mm4, mm6

	psrlw mm4, 2	            ; mm4 := First 4 bytes interpolated in words
		
	movq mm5, mm1			; mm5:= second 4 bytes of p2 in words
	punpckhbw mm5, mm7
			
	movq  mm7,mm4
	pcmpgtw mm7,mm5		; mm7 := [i : W0..3,mm4>mm5]

	movq  mm6,mm4		; mm6 := [i : W0..3, (mm4-mm5)*(mm4-mm5 > 0)]
 	psubw mm6,mm5
	pand  mm6, mm7

	paddw mm0, mm6				; Add to accumulator

	movq  mm6,mm5       ; mm6 := [i : W0..3,mm5>mm4]
	pcmpgtw mm6,mm4	    
 	psubw mm5,mm4		; mm5 := [i : B0..7, (mm5-mm4)*(mm5-mm4 > 0)]
	pand  mm5, mm6		

	paddw mm0, mm5				; Add to accumulator


 		;;
		;; Second 8 bytes of row
		;; 
		;; First 4 bytes of 8

	movq mm4, [esi+8]             ; mm4 := first 4 bytes p1+8
	pxor mm7, mm7
	movq mm2, mm4				;  mm2 records all 8 bytes
	punpcklbw mm4, mm7            ;  First 4 bytes p1 in Words...
	
	movq mm6, [ebx+8]			    ;  mm6 := first 4 bytes p1+lx+8
	movq mm3, mm6               ;  mm3 records all 8 bytes
	punpcklbw mm6, mm7
	paddw mm4, mm6              


	movq mm5, [esi+9]			; mm5 := first 4 bytes p1+9
	punpcklbw mm5, mm7            ;  First 4 bytes p1 in Words...
	paddw mm4, mm5
	movq mm6, [ebx+9]           ;  mm6 := first 4 bytes p1+lx+9
	punpcklbw mm6, mm7
	paddw mm4, mm6

	psrlw mm4, 2	            ; mm4 := First 4 bytes interpolated in words
		
	movq mm5, [edi+8]				; mm5:=first 4 bytes of p2+8 in words
	movq mm1, mm5
	punpcklbw mm5, mm7
			
	movq  mm7,mm4
	pcmpgtw mm7,mm5		; mm7 := [i : W0..3,mm4>mm5]

	movq  mm6,mm4		; mm6 := [i : W0..3, (mm4-mm5)*(mm4-mm5 > 0)]
 	psubw mm6,mm5
	pand  mm6, mm7

	paddw mm0, mm6				; Add to accumulator

	movq  mm6,mm5       ; mm6 := [i : W0..3,mm5>mm4]
	pcmpgtw mm6,mm4	    
 	psubw mm5,mm4		; mm5 := [i : B0..7, (mm5-mm4)*(mm5-mm4 > 0)]
	pand  mm5, mm6		

	paddw mm0, mm5				; Add to accumulator

		;; Second 4 bytes of 8
	
	movq mm4, mm2		    ; mm4 := Second 4 bytes p1 in words
	pxor  mm7, mm7
	punpckhbw mm4, mm7			
	movq mm6, mm3			; mm6 := Second 4 bytes p1+1 in words  
	punpckhbw mm6, mm7
	paddw mm4, mm6          

	movq mm5, [esi+9]			; mm5 := first 4 bytes p1+1
	punpckhbw mm5, mm7          ;  First 4 bytes p1 in Words...
	paddw mm4, mm5	
	movq mm6, [ebx+9]           ;  mm6 := first 4 bytes p1+lx+1
	punpckhbw mm6, mm7
	paddw mm4, mm6
		
	psrlw mm4, 2	            ; mm4 := First 4 bytes interpolated in words

	movq mm5, mm1			; mm5:= second 4 bytes of p2 in words
	punpckhbw mm5, mm7
			
	movq  mm7,mm4
	pcmpgtw mm7,mm5		; mm7 := [i : W0..3,mm4>mm5]

	movq  mm6,mm4		; mm6 := [i : W0..3, (mm4-mm5)*(mm4-mm5 > 0)]
 	psubw mm6,mm5
	pand  mm6, mm7

	paddw mm0, mm6				; Add to accumulator

	movq  mm6,mm5       ; mm6 := [i : W0..3,mm5>mm4]
	pcmpgtw mm6,mm4	    
 	psubw mm5,mm4		; mm5 := [i : B0..7, (mm5-mm4)*(mm5-mm4 > 0)]
	pand  mm5, mm6		

	paddw mm0, mm5				; Add to accumulator


		;;
		;;	Loop termination condition... and stepping
		;;		

	add esi, edx		; update pointer to next row
	add edi, edx		; ditto
	add ebx, edx

	sub  ecx,1
	test ecx, ecx		; check rowsleft
	jnz near nextrowmm11
		
		;; Sum the Accumulators
	movq  mm4, mm0
	psrlq mm4, 32
	paddw mm0, mm4
	movq  mm6, mm0
	psrlq mm6, 16
	paddw mm0, mm6
	movd eax, mm0		; store return value
	and  eax, 0xffff
		
	pop edi
	pop esi	
	pop edx			
	pop ecx			
	pop ebx			

	pop ebp			; restore stack pointer

	emms			; clear mmx registers
	ret			; we now return you to your regular programming
