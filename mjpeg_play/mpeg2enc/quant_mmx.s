;
;  quantize_ni_mmx.s:  MMX optimized coefficient quantization sub-routine
;
;  Copyright (C) 2000 Andrew Stevens <as@comlab.ox.ac.uk>
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


global quantize_ni_mmx
; int quantize_ni_mmx(short *dst, short *src, 
;		              short *quant_mat, short *i_quant_mat,
;                     int imquant, int mquant, int sat_limit)

;  See quantize.c: quant_non_intra_inv()  for reference implementation in C...
		;;  mquant is not currently used.
; eax = row counter...
; ebx = pqm
; ecx = piqm  ; Matrix of quads first (2^16/quant) 
 			  ; then (2^16/quant)*(2^16%quant) the second part is for rounding
; edx = temp
; edi = psrc
; esi = pdst

; mm0 = [imquant|0..3]W
; mm1 = [sat_limit|0..3]W
; mm2 = *psrc -> src
; mm3 = rounding corrections... / temp
; mm4 = sign
; mm5 = nzflag accumulators
; mm6 = saturation accumulator...
; mm7 = temp

		;; 
		;;  private constants needed
		;; 

SECTION .data
align 16
satlim:	
			dw	1024-1
			dw	1024-1
			dw	1024-1
			dw	1024-1
			
			;; BUFFER NO LONGER USED DUE TO IMPROVED MAIN ROUTINE...
SECTION .bss
align 32
quant_buf:	resw 64
		
SECTION .text
		

align 32
quantize_ni_mmx:
	push ebp				; save frame pointer
	mov ebp, esp		; link
	push ebx
	push ecx
	push edx
	push esi     
	push edi

	mov edi, [ebp+8]    ; get dst
	mov esi, [ebp+12]	; get psrc
	mov ebx, [ebp+16]	; get pqm
	mov ecx,  [ebp+20]  ; get piqm
	movd mm0, [ebp+24]  ; get imquant (2^16 / mquant )
	movq mm1, mm0
	punpcklwd mm0, mm1  
	punpcklwd mm0, mm0    ; mm0 = [imquant|0..3]W
	
	pxor  mm6, mm6			; saturation / out-of-range accumulator(s)

	movd mm1, [ebp+32]  ; sat_limit
	movq mm2, mm1
	punpcklwd mm1, mm2   ; [sat_limit|0..3]W
	punpcklwd mm1, mm1   ; mm1 = [sat_limit|0..3]W
	
	pxor      mm5, mm5  ; Non-zero flag accumulator 
	mov eax,  16		; 16 quads to do 
	jmp nextquadniq

align 32
nextquadniq:
	movq mm2, [esi]				; mm0 = *psrc

	pxor    mm4, mm4
	pcmpgtw mm4, mm2       ; mm4 = *psrc < 0
	movq    mm7, mm2       ; mm7 = *psrc
	psllw   mm7, 1         ; mm7 = 2*(*psrc)
	pand    mm7, mm4       ; mm7 = 2*(*psrc)*(*psrc < 0)
	psubw   mm2, mm7       ; mm2 = abs(*psrc)

	;;
	;;  Check whether we'll saturate intermediate results
	;;  Eventually flag is low 8 bits of result
	;;
	
	movq    mm7, mm2
	pcmpgtw mm7, [satlim]    ; Tooo  big for 16 bit arithmetic :-( (should be *very* rare)
	movq    mm3, mm7
	psllw   mm3, 8
	por     mm7, mm3
	psrlw   mm7, 8
	por     mm6, mm7

	;;
	;; Carry on with the arithmetic...
	psllw   mm2, 5         ; mm2 = 32*abs(*psrc)
	movq    mm7, [ebx]     ; mm7 = *pqm>>1
	psrlw   mm7, 1
	paddw   mm2, mm7       ; mm2 = 32*abs(*psrc)+((*pqm)/2) = "p"

	
	;;
	;; Do the first multiplication.  Cunningly we've set things up so
	;; it is exactly the top 16 bits we're interested in...
	;;
	;; We need the low word results for a rounding correction.  
	;; This is *not* exact (that actual
    ;; correction the product abs(*psrc)*(*pqm)*(2^16%*qm) >> 16
    ;;  However we get very very few wrong and none too low (the most
    ;; important) and no errors for small coefficients (also important)
	;; 	if we simply add abs(*psrc)
	
	movq    mm3, mm2				
	pmullw  mm3, [ecx]          
	movq    mm7, mm2
	psrlw   mm7, 1            ; Want to see if adding p would carry into upper 16 bits
	psrlw   mm3, 1
	paddw   mm3, mm7
	psrlw   mm3, 15           ; High bit in lsb rest 0's
	pmulhw  mm2, [ecx]        ; mm2 = (p*iqm+p) >> IQUANT_SCALE_POW2 ~= p/*qm

	
	
	;;
	;; To hide the latency lets update some pointers...
	add   esi, 8					; 4 word's
	add   ecx, 8					; 4 word's
	sub   eax, 1

	;; Add rounding correction....
	paddw   mm2, mm3


	;;
	;; Do the second multiplication, again we ned to make a rounding adjustment
	movq    mm3, mm2				
	pmullw  mm3, mm0          
	movq    mm7, mm2
	psrlw   mm7, 1            ; Want to see if adding p would carry into upper 16 bits
	psrlw   mm3, 1
	paddw   mm3, mm7
	psrlw   mm3, 15           ; High bit in lsb rest 0's

	pmulhw  mm2, mm0     ; mm2 ~= (p/(qm*mquant)) 

	;;
	;; To hide the latency lets update some more pointers...
	add   edi, 8
	add   ebx, 8

	;; Correct rounding and the factor of two (we want p/(qm*2*mquant)
	paddw mm2, mm3
	psrlw mm2, 1


	;;
	;; Check for saturation eventual bits 8-15 of result...
	;;
	movq mm7, mm2
	pcmpgtw mm7, mm1
	movq  mm3, mm7
	psrlw mm3, 8
	por   mm7, mm3
	psllw mm7, 8
	por   mm6, mm7       ; Accumulate that bad things happened...

	;;
	;;  Accumulate non-zero flags
	por     mm5, mm2
	
	;;
	;; Now correct the sign mm4 = *psrc < 0
	;;
	
	pxor mm7, mm7        ; mm7 = -2*mm2
	psubw mm7, mm2
	psllw mm7, 1
	pand  mm7, mm4       ; mm7 = -2*mm2 * (*psrc < 0)
	paddw mm2, mm7       ; mm7 = samesign(*psrc, mm2 )
	
		;;
		;;  Store the quantised words....
		;;

	movq [edi-8], mm2
	test eax, eax
	
	jnz near nextquadniq

	;; Return saturation in low word and nzflag in high word of result dword 
		
	movq mm0, mm6
	psrlq mm0, 32
	por   mm6, mm0
	movd  eax, mm6
	mov   ebx, eax
	shr   ebx, 16
	or    eax, ebx
	and   eax, 0xffff		;; low word eax is saturation / out-of-range
	

;	mov   ecx, 8           ;; 8 pairs of quads...
;	mov   edi, [ebp+8]     ;; destination
;	mov   esi, quant_buf
;update:
;	movq  mm0, [esi]
;	movq  mm1, [esi+8]
;	add   esi, 16
;	movq  [edi], mm0
;	movq  [edi+8], mm1
;	add   edi, 16
;	sub   ecx, 1	
;	jnz   update
;skipupdate:

	movq  mm0, mm5
	psrlq mm0, 32
	por   mm5, mm0
	movd  edx, mm5
	mov   ebx, edx
	shl   ebx, 16
	or    edx, ebx
    and   edx, 0xffff0000  ;; hiwgh word ecx is nzflag
	or    eax, edx
	

	pop edi
	pop esi
	pop edx
	pop ecx
	pop ebx

	pop ebp			; restore stack pointer

	emms			; clear mmx registers
	ret			


; int quant_weight_coeff_sum_mmx( short *blk, unsigned short * i_quant_mat )
;
; Simply add up the sum of coefficients weighted by their quantisation coefficients

global quant_weight_coeff_sum_mmx
align 32
quant_weight_coeff_sum_mmx:
	push ebp				; save frame pointer
	mov ebp, esp		; link

	push ecx
	push esi     
	push edi

	mov edi, [ebp+8]	; get pdst
	mov esi, [ebp+12]	; get piqm

	mov ecx, 16			; 16 coefficient / quantiser quads to process...
	pxor mm6, mm6		; Accumulator
	pxor mm7, mm7		; Zero
quantsum:
	movq    mm0, [edi]
	movq    mm2, [esi]
	
	;;
	;;      Compute absolute value of coefficients...
	;;
	movq    mm1, mm7
	pcmpgtw mm1, mm0   ; (mm0 < 0 )
	movq	mm3, mm0
	psllw   mm3, 1     ; 2*mm0
	pand    mm3, mm1   ; 2*mm0 * (mm0 < 0)
	psubw   mm0, mm3   ; mm0 = abs(mm0)


	;;
	;; Compute the low and high words of the result....
	;; 
	movq    mm1, mm0	
	pmullw  mm0, mm2
	add		edi, 8
	add		esi, 8
	pmulhw  mm1, mm2
	
	movq      mm3, mm0
	punpcklwd  mm3, mm1
	punpckhwd  mm0, mm1
	paddd      mm6, mm3
	paddd      mm6, mm0
	
	
	sub ecx,	1
	jnz   quantsum

	movd   eax, mm6
	psrlq  mm6, 32
	movd   ecx, mm6
	add    eax, ecx
	
	pop edi
	pop esi
	pop ecx

	pop ebp			; restore stack pointer

	emms			; clear mmx registers
	ret			

			
