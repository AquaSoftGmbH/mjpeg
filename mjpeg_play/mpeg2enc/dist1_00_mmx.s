;
;  fdist1_00.s:  MMX1 optimised 8*8 word absolute difference sum
;
;  Copyright (C) 2000 Andrew Stevens <as@comlab.ox.ac.uk>
;  Based on original GPL Copyright (C) 2000 Chris Atenasio <chris@crud.net>

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


global dist1_00_MMX

; int dist1_MMX(unsigned char *blk1,unsigned char *blk2,int lx,int h, int distlim);

; esi = p1 (init:		blk1)
; edi = p2 (init:		blk2)
; ebx = distlim  
; ecx = rowsleft (init:	 h)
; edx = lx;

; mm0 = distance accumulators (4 words)
; mm1 = temp (evtl. pos. differences *p1 - *p2)
; mm2 = temp 
; mm3 = [0..7|-128]
; mm4 = temp
; mm5 = temp (evtl. pos. differences *p2 - *p1)
; mm6 = temp
; mm7 = temp


align 32
dist1_00_MMX:
	push ebp		; save frame pointer
	mov ebp, esp

	push ebx		; Saves registers (called saves convention in
	push ecx		; x86 GCC it seems)
	push edx		; 
	push esi
	push edi
		
	pxor mm0, mm0				; zero acculumators

	mov esi, [ebp+8]			; get p1
	mov edi, [ebp+12]			; get p2
	mov edx, [ebp+16]			; get lx
	mov  ecx, [ebp+20]			; get rowsleft
	mov ebx, [ebp+24]	        ; distlim
	mov  eax, 0x80808080		;  mm3 := [0..7,-0x80 (MINSIGNEDBYTE)]
	movd mm3, eax
	movd mm4, eax
	punpcklwd mm3, mm4
	jmp nextrowmm00					; snap to it
align 32
nextrowmm00:
	movq mm4, [esi]		; load first 8 bytes of p1 row 
	movq mm5, [edi]		; load first 8 bytes of p2 row
		
	movq  mm6, mm4		; mm6 :	= [i : B0..7,mm4-0x80]B
	paddb mm6, mm3

    movq  mm7, mm5		;  mm7 :=  [i :	B0..7,mm5-0x80]B
	paddb mm7, mm3

	movq  mm2,mm6
	pcmpgtb mm2,mm7		; mm2 := [i : B0..7,mm4>mm5]

	movq  mm1,mm4		; mm1 := [i : B0..7, (mm4-mm5)*(mm4-mm5 > 0)]B <==
 	psubb mm1,mm5
	pand  mm1, mm2		

	pcmpgtb mm7,mm6	    ; mm7 := [i : B0..7,mm4>mm5]B
 	psubb mm5,mm4		; mm5 := [i : B0..7, (mm5-mm4)*(mm5-mm4 > 0)]B <==
	pand  mm5, mm7		

		
		;;  Add the mm1 bytes to the accumulators
	movq  mm7,mm1				; mm7 := [i :	B0..3, mm1]W
	pxor  mm6,mm6
	punpcklbw mm7,mm6
		
	paddw mm0, mm7

	pxor  mm6,mm6               ; mm1 :	=  [i :	B4..7, mm1]W
	punpckhbw mm1,mm6

	paddw mm0, mm1

		;;	Add the mm5 bytes to the accumulators
	movq  mm7,mm5				; mm7 := [i :	B0..3, mm5]W
	pxor  mm6,mm6
	punpcklbw mm7,mm6
		
	paddw mm0, mm7

	pxor  mm6,mm6               ; mm5 :	=  [i :	B4..7, mm5]W
	punpckhbw mm5,mm6

	paddw mm0, mm5

	movq mm4, [esi+8]		; load second 8 bytes of p1 row 
	movq mm5, [edi+8]		; load second 8 bytes of p2 row
		
	movq  mm6, mm4		; mm6 :	= [i : B0..7,mm4-0x80]B
	paddb mm6, mm3

    movq  mm7, mm5		;  mm7 :=  [i :	B0..7,mm5-0x80]B
	paddb mm7, mm3

	movq  mm2,mm6
	pcmpgtb mm2,mm7		; mm2 := [i : B0..7,mm4>mm5]

	movq  mm1,mm4		; mm1 := [i : B0..7, (mm4-mm5)*(mm4-mm5 > 0)]B <==
 	psubb mm1,mm5
	pand  mm1, mm2		

	pcmpgtb mm7,mm6	    ; mm7 := [i : B0..7,mm4>mm5]B
 	psubb mm5,mm4		; mm5 := [i : B0..7, (mm5-mm4)*(mm5-mm4 > 0)]B <==
	pand  mm5, mm7		

		;;  Add the mm1 bytes to the accumulators
	movq  mm7,mm1				; mm7 := [i :	B0..3, mm1]W
	pxor  mm6,mm6
	punpcklbw mm7,mm6
		
	paddw mm0, mm7

	pxor  mm6,mm6               ; mm1 :	=  [i :	B4..7, mm1]W
	punpckhbw mm1,mm6

	paddw mm0, mm1

		;;	Add the mm5 bytes to the accumulators
	movq  mm7,mm5				; mm7 := [i :	B0..3, mm5]W
	pxor  mm6,mm6
	punpcklbw mm7,mm6
		
	paddw mm0, mm7

	pxor  mm6,mm6               ; mm5 :	=  [i :	B4..7, mm5]W
	punpckhbw mm5,mm6

	paddw mm0, mm5

	add esi, edx		; update pointer to next row
	add edi, edx		; ditto

		;; Sum the Accumulators
	movq  mm5, mm0				; mm5 := [W0+W2,W1+W3, mm0
	psrlq mm5, 32
	movq  mm6, mm0
	paddw mm6, mm5

	movq  mm7, mm6              ; mm6 := [W0+W2+W1+W3, mm0]
	psrlq mm7, 16
	paddw mm6, mm7
		
	movd eax, mm6		; store return value
	and  eax, 0xffff
	cmp ebx, eax		;  Return early if distlim exceeded
	jle  returnmm00
			
	sub  ecx,1
	test ecx, ecx		; check rowsleft
	jnz near nextrowmm00
		
returnmm00:	


	pop edi
	pop esi	
	pop edx	
	pop ecx	
	pop ebx	

	pop ebp	

	emms			; clear mmx registers
	ret	
