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


global cpuid_flags

; int cpuid();
; eax = cpuid ret
; ebx = cpuid ret
; ecx = cpuid ret
; edx = cpuid ret

cpuid_flags:
	push ebp					; save frame pointer
	mov ebp, esp				; link

	push ebx
	push ecx
	push edx

	pushfd
	pop			eax
	mov			ecx, eax
	xor			eax,0x200000	; Toggle ID bit in EFlag
	push		eax
	popfd
	pushfd
	pop			eax
	xor			eax, ecx
	jz			nonmmx

	xor			eax, eax
	cpuid
	cmp			eax, 1
	jl			nonmmx

	mov			eax, 1
	cpuid
	or			edx, 1			; Make sure non-zero for > MMX CPU
	mov			eax, edx
nonmmx:	
	
	pop edx	
	pop ecx	
	pop ebx	

	pop ebp	
	emms
	ret	
