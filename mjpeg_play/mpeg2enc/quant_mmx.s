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
;;;  void iquantize_non_intra_m1_extmmx(int16_t *src, int16_t *dst, uint16_t
;;;										 *quant_mat)
;;; mmx/sse Inverse mpeg-1 quantisation routine.
;;; 
;;; eax - block counter...
;;; edi - src
;;; esi - dst
;;; edx - quant_mat

		;; MMX Register usage
		;; mm7 = [1|0..3]W
		;; mm6 = [2047|0..3]W
		;; mm5 = 0

				
global iquantize_non_intra_m1_extmmx:function
align 32
iquantize_non_intra_m1_extmmx:
		
		push ebp				; save frame pointer
		mov ebp, esp		; link

		push eax
		push esi     
		push edi
		push edx

		mov		edi, [ebp+8]			; get psrc
		mov		esi, [ebp+12]			; get pdst
		mov		edx, [ebp+16]			; get quant table
		mov		eax,1
		movd	mm7, eax
		punpcklwd	mm7, mm7
		punpckldq	mm7, mm7

		mov     eax, 2047
		movd	mm6, eax
		punpcklwd		mm6, mm6
		punpckldq		mm6, mm6

		mov		eax, 64			; 64 coeffs in a DCT block
		pxor	mm5, mm5
		
iquantize_loop_m1_extmmx:
		movq	mm0, [edi]      ; mm0 = *psrc
		add		edi,8
		pxor	mm1,mm1
		movq	mm2, mm0
		pcmpeqw	mm2, mm1		; mm2 = 1's for non-zero in mm0
		pcmpeqw	mm2, mm1

		;; Work with absolute value for convience...
		psubw   mm1, mm0        ; mm1 = -*psrc
		pmaxsw	mm1, mm0        ; mm1 = val = max(*psrc,-*psrc) = abs(*psrc)
		paddw   mm1, mm1		; mm1 *= 2;
		paddw	mm1, mm7		; mm1 += 1
		pmullw	mm1, [edx]		; mm1 = (val*2+1) * *quant_mat
		add		edx, 8
		psraw	mm1, 5			; mm1 = ((val*2+1) * *quant_mat)/32

		;; Now that nasty mis-match control

		movq	mm3, mm1
		pand	mm3, mm7
		pxor	mm3, mm7		; mm3 = ~(val&1) (in the low bits, others 0)
		movq    mm4, mm1
		pcmpeqw	mm4, mm5		; mm4 = (val == 0) 
		pxor	mm4, mm7		;  Low bits now (val != 0)
		pand	mm3, mm4		; mm3 =  (~(val&1))&(val!=0)

		psubw	mm1, mm3		; mm1 -= (~(val&1))&(val!=0)
		pminsw	mm1, mm6		; mm1 = saturated(res)

		;; Handle zero case and restoring sign
		pand	mm1, mm2		; Zero in the zero case
		pxor	mm3, mm3
		psubw	mm3, mm1		;  mm3 = - res
		paddw	mm3, mm3		;  mm3 = - 2*res
		pcmpgtw	mm0, mm5		;  mm0 = *psrc < 0
		pcmpeqw	mm0, mm5		;  mm0 = *psrc >= 0
		pand	mm3, mm0		;  mm3 = *psrc <= 0 ? -2 * res :	 0
		paddw	mm1, mm3		;  mm3 = samesign(*psrc,res)
		movq	[esi], mm1
		add		esi,8

		sub		eax, 4
		jnz		iquantize_loop_m1_extmmx
		
		pop	edx
		pop edi
		pop esi
		pop eax

		pop ebp			; restore stack pointer

		emms			; clear mmx registers
		ret			


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
		
		push ebp				; save frame pointer
		mov ebp, esp		; link

		push eax
		push esi     
		push edi
		push edx

		mov		edi, [ebp+8]			; get psrc
		mov		esi, [ebp+12]			; get pdst
		mov		edx, [ebp+16]			; get quant table
		mov		eax,1
		movd	mm7, eax
		punpcklwd	mm7, mm7
		punpckldq	mm7, mm7

		mov     eax, (0xffff-2047)
		movd	mm6, eax
		punpcklwd		mm6, mm6
		punpckldq		mm6, mm6

		mov		eax, 64			; 64 coeffs in a DCT block
		pxor	mm5, mm5
		
iquantize_m1_loop:
		movq	mm0, [edi]      ; mm0 = *psrc
		add		edi,8
		pxor    mm1, mm1		
		movq	mm2, mm0
		pcmpeqw	mm2, mm5		; mm2 = 1's for non-zero in mm0
		pcmpeqw	mm2, mm5

		;; Work with absolute value for convience...

		psubw   mm1, mm0        ; mm1 = -*psrc
		psllw	mm1, 1			; mm1 = -2*psrc
		movq	mm3, mm0		; mm3 = *psrc > 0
		pcmpgtw	mm3, mm5
		pcmpeqw mm3, mm5        ; mm3 = *psrc <= 0
		pand    mm3, mm1		; mm3 = (*psrc <= 0)*-2* *psrc
		movq	mm1, mm0        ; mm1 = (*psrc <= 0)*-2* *psrc + *psrc = abs(*psrc)
		paddw	mm1, mm3

		
		paddw   mm1, mm1		; mm1 *= 2;
		paddw	mm1, mm7		; mm1 += 1
		pmullw	mm1, [edx]		; mm1 = (val*2+1) * *quant_mat
		add		edx, 8
		psraw	mm1, 5			; mm1 = ((val*2+1) * *quant_mat)/32

		;; Now that nasty mis-match control

		movq	mm3, mm1
		pand	mm3, mm7
		pxor	mm3, mm7		; mm3 = ~(val&1) (in the low bits, others 0)
		movq    mm4, mm1
		pcmpeqw	mm4, mm5		; mm4 = (val == 0) 
		pxor	mm4, mm7		;  Low bits now (val != 0)
		pand	mm3, mm4		; mm3 =  (~(val&1))&(val!=0)

		psubw	mm1, mm3		; mm1 -= (~(val&1))&(val!=0)

		paddsw	mm1, mm6		; Will saturate if > 2047
		psubw	mm1, mm6		; 2047 if saturated... unchanged otherwise

		;; Handle zero case and restoring sign
		pand	mm1, mm2		; Zero in the zero case
		pxor	mm3, mm3
		psubw	mm3, mm1		;  mm3 = - res
		paddw	mm3, mm3		;  mm3 = - 2*res
		pcmpgtw	mm0, mm5		;  mm0 = *psrc < 0
		pcmpeqw	mm0, mm5		;  mm0 = *psrc >= 0
		pand	mm3, mm0		;  mm3 = *psrc <= 0 ? -2 * res :	 0
		paddw	mm1, mm3		;  mm3 = samesign(*psrc,res)
		movq	[esi], mm1
		add		esi,8

		sub		eax, 4
		jnz		near iquantize_m1_loop
		
		pop	edx
		pop edi
		pop esi
		pop eax

		pop ebp			; restore stack pointer

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
		
global quant_weight_coeff_sum_mmx_old:function
align 32
quant_weight_coeff_sum_mmx_old:
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
					
