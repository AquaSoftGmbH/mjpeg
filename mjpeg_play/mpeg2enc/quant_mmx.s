;
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
;  quantize_ni_mmx.s:  MMX optimized coefficient quantization sub-routine



;;;		
;;;  void iquantize_non_intra_m1_mmx(int16_t *src, int16_t *dst, uint16_t
;;;                               *quant_mat)
;;; eax - block counter...
;;; edi - src
;;; esi - dst
;;; edx - quant_mat

		;; MMX Register usage
		;; mm7 = [1|0..3]W
		;; mm6 = [MAX_UINT16-2047|0..3]W
		;; mm5 = 0

				
global iquantize_non_intra_m1_mmx:function
align 32
iquantize_non_intra_m1_mmx:		
	push	ebp				; save frame pointer
	mov	ebp, esp		; link

	push	eax
	push	esi     
	push	edi
	push	edx

	mov	edi, [ebp+8]			; get psrc
	mov	esi, [ebp+12]			; get pdst
	mov	edx, [ebp+16]			; get quant table
	
	;; Load 1 into all 4 words of mm7
	pxor	mm7, mm7
	pcmpeqw	mm7, mm7
	psrlw	mm7, 15

	pxor	mm6, mm6

	mov	eax, 64			; 64 coeffs in a DCT block
		
.loop:
	movq	mm0, [edi]      ; mm0 = *psrc
	add	edi,8
	
	movq	mm2, mm0	; mm2 = TRUE where *psrc==0
	pcmpeqw	mm2, mm6
	
	movq	mm3, mm0	; mm3 = TRUE where *psrc<0
	psraw	mm3, 15

	;; Work with absolute value for convenience...
	pxor	mm0, mm3	; mm0 = abs(*psrc)
	psubw	mm0, mm3
	
	paddw	mm0, mm0	; mm0 = 2*abs(*psrc)
	paddw	mm0, mm7	; mm0 = 2*abs(*psrc) + 1
	
	movq	mm4, [edx]	; multiply by *quant_mat
	movq	mm1, mm0
	pmullw	mm0, mm4
	pmulhw	mm1, mm4
	add	edx, 8

	pcmpgtw	mm1, mm6	; if there was overflow, saturate low bits with all 1's
	por	mm0, mm1
	
	psrlw	mm0, 5		; divide by 32 (largest possible value = 65535/32 == 2047)

	;; zero case
	pandn	mm2, mm0	; set to 0 where *psrc==0

	;;  mismatch control
	movq	mm1, mm2
	psubw	mm2, mm7
	pcmpeqw	mm1, mm6 	; mm0 = v==0
	por	mm2, mm7
	pandn	mm1, mm2
		
	;; Handle zero case and restoring sign
	pxor	mm1, mm3	; retain original sign of *psrc
	psubw	mm1, mm3
	
	movq	[esi], mm1
	add	esi,8

	sub	eax, 4
	jnz	.loop
		
	pop	edx
	pop	edi
	pop	esi
	pop	eax
	
	pop	ebp			; restore stack pointer

	emms			; clear mmx registers
	ret			
						
;;;		
;;;  void iquantize_non_intra_mmx(int16_t *src, int16_t *dst, uint16_t
;;;										 *quant_mat)
;;; extmmx Inverse mpeg-2 quantisation routine.
;;; 
;;; eax - block counter...
;;; edi - src
;;; esi - dst
;;; edx - quant_mat

global iquantize_non_intra_m2_mmx:function
align 32
iquantize_non_intra_m2_mmx:
		;; mm0 *psrc, scratch
		;; mm1 *pdst
		;; mm2 TRUE if *psrc is 0, then scratch
		;; mm3 TRUE if *psrc is negative
		;; mm4 Partial sums 
		;; mm5 
		;; mm6 <0,0,0,0>
		;; mm7 <1,1,1,1>
		
		push ebp				; save frame pointer
		mov ebp, esp		; link

		push eax
		push esi     
		push edi
		push edx

		mov		edi, [ebp+8]			; get psrc
		mov		esi, [ebp+12]			; get pdst
		mov		edx, [ebp+16]			; get quant table

		;; Load 1 into all 4 words of mm7
		pxor	mm7, mm7
		pcmpeqw	mm7, mm7
		psrlw	mm7, 15

		mov		eax, 64			; 64 coeffs in a DCT block
		pxor	mm6, mm6
		pxor	mm4, mm4
.loop:
		movq	mm0, [edi]      ; mm0 = *psrc
		add	edi,8
	
		movq	mm2, mm0	; mm2 = TRUE where *psrc==0
		pcmpeqw	mm2, mm6
	
		movq	mm3, mm0	; mm3 = TRUE where *psrc<0
		psraw	mm3, 15

		;; Work with absolute value for convenience...
		pxor	mm0, mm3	; mm0 = abs(*psrc)
		psubw	mm0, mm3
	
		paddw	mm0, mm0	; mm0 = 2*abs(*psrc)
		paddw	mm0, mm7	; mm0 = 2*abs(*psrc) + 1
		pandn	mm2, mm0	; set to 0 where *psrc==0
	
		movq	mm1, [edx]	; multiply by *quant_mat
		movq	mm0, mm2
		pmulhw	mm2, mm1
		pmullw	mm0, mm1
		add	edx, 8
	
		pcmpgtw	mm2, mm6	; if there was overflow, saturate low bits with all 1's
		por	mm0, mm2
	
		psrlw	mm0, 5		; divide by 32 (largest possible value = 65535/32 == 2047)

		;; Accumulate sum...
		paddw	mm4, mm0
		
		
		;; Handle zero case and restoring sign
		pxor	mm0, mm3	; retain original sign of *psrc
		psubw	mm0, mm3
	
		movq	[esi], mm0
		add	esi,8

		sub	eax, 4
		jnz	.loop

		;; Mismatch control compute lower bits of sum...
		movq	mm5, mm4
		psrlq	mm5, 32
		paddw	mm4, mm5
		movq	mm5, mm4
		psrlq	mm5, 16
		paddw	mm4, mm5
		mov		esi, [ebp+12]			; get pdst
		movd	eax, mm4
		and		eax, 1
		xor		eax, 1
		xor		ax, [esi+63*2]
		mov		[esi+63*2], ax
		
		pop	edx
		pop edi
		pop esi
		pop eax

		pop ebp			; restore stack pointer

		emms			; clear mmx registers
		ret			
		

		
;;;  int32_t quant_weight_coeff_sum_mmx(int16_t *src, int16_t *i_quant_mat
;;; Simply add up the sum of coefficients weighted 
;;; by their quantisation coefficients
;;;                               )
;;; eax - block counter...
;;; edi - src
;;; esi - inverse quantisation coefficient matrix...

		;; MMX Register usage
		;; mm7 = [1|0..3]W
		;; mm6 = [2047|0..3]W
		;; mm5 = 0
		
global quant_weight_coeff_sum_mmx:function
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
.nquantsum:
	movq    mm0, [edi]
	movq    mm2, [edi+8]
	pxor    mm1, mm1
	pxor    mm3, mm3
	
	;;
	;;      Compute absolute value of coefficients...
	;;
	pcmpgtw mm1, mm0   ; (mm0 < 0 )
	pcmpgtw mm3, mm2   ; (mm0 < 0 )
	pxor	mm0, mm1
	pxor	mm2, mm3
	psubw	mm0, mm1
	psubw	mm2, mm3


	;;
	;; Compute the low and high words of the result....
	;;
	pmaddwd   mm0, [esi]
	pmaddwd   mm2, [esi+8]
	add		edi, 16
	add		esi, 16
	paddd      mm6, mm0
	paddd      mm6, mm2
	
	
	sub ecx,	2
	jnz   .nquantsum

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
					
