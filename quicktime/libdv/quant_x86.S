/*

The pattern for dv_88_areas looks like this:	
    0  0  1  1  1  2  2   
 0  0  1  1  1  2  2  2   
 0  1  1  1  2  2  2  3   
 1  1  1  2  2  2  3  3   
 1  1  2  2  2  3  3  3   
 1  2  2  2  3  3  3  3   
 2  2  2  3  3  3  3  3   
 2  2  3  3  3  3  3  3

Note
[1] matrix element [0][0] is untouched.
[2] all values in the same diagonal are equal
	
This implementation works by loading the four shift values in turn,
and shifting all the appropriate array elements.
	
*/

#define DV_WEIGHT_BIAS	 6
	
/*
void quant_88_inverse(dv_coeff_t *block,int qno,int class)
*/

.text
	.align	4
.globl	quant_88_inverse
quant_88_inverse:	
	pushl	%ebx
	pushl	%esi

#define ARGn(N)  (12+(4*(N)))(%esp)

	/*
		eax	
		ebx	extra
		ecx
		edx	pq
		esi	block
	*/
	
	/*  pq = dv_quant_shifts[qno + dv_quant_offset[class]]; */
	movl	ARGn(1),%eax	/* qno */
	movl	ARGn(2),%ebx	/* class */
	movzbl	dv_quant_offset(%ebx),%ecx
	addl	%ecx,%eax
	leal	dv_quant_shifts(,%eax,4),%edx	/* edx is pq */

	/* extra = (class == 3); */
				/*  0   1   2   3 */
	subl	$3,%ebx		/* -3  -2  -1   0 */
	sarl	$31,%ebx	/* -1  -1  -1   0 */
	incl	%ebx		/*  0   0   0   1 */
	addl	$DV_WEIGHT_BIAS,%ebx
				/* ebx is extra */

	movl	ARGn(0),%esi	/* esi is block */

	/* Pick up each of the factors into %ecx, then shift the
	appropriate coefficients.  The pattern here is taken from
	dv_88_areas; it's arranged by diagonals for clarity. */

#define ADDR(row,col)	(2*(8*row+col))(%esi)
#define MSHIFT(row,col)	\
	shlw	%cl,ADDR(row,col)

	movl	$DV_WEIGHT_BIAS,%ecx
	MSHIFT(0,0)
	
	/* 0 */
	movzbl	(%edx),%ecx
	addl	%ebx,%ecx
	MSHIFT(0,1)
	MSHIFT(1,0)
	
	MSHIFT(0,2)
	MSHIFT(1,1)
	MSHIFT(2,0)

	/* 1 */	
	movzbl	1(%edx),%ecx
	addl	%ebx,%ecx
	MSHIFT(0,3)
	MSHIFT(1,2)
	MSHIFT(2,1)
	MSHIFT(3,0)

	MSHIFT(0,4)
	MSHIFT(1,3)
	MSHIFT(2,2)
	MSHIFT(3,1)
	MSHIFT(4,0)

	MSHIFT(0,5)
	MSHIFT(1,4)
	MSHIFT(2,3)
	MSHIFT(3,2)
	MSHIFT(4,1)
	MSHIFT(5,0)
		
	/* 2 */	
	movzbl	2(%edx),%ecx
	addl	%ebx,%ecx
	MSHIFT(0,6)
	MSHIFT(1,5)
	MSHIFT(2,4)
	MSHIFT(3,3)
	MSHIFT(4,2)
	MSHIFT(5,1)
	MSHIFT(6,0)
	
	MSHIFT(0,7)
	MSHIFT(1,6)
	MSHIFT(2,5)
	MSHIFT(3,4)
	MSHIFT(4,3)
	MSHIFT(5,2)
	MSHIFT(6,1)
	MSHIFT(7,0)
	
	MSHIFT(1,7)
	MSHIFT(2,6)
	MSHIFT(3,5)
	MSHIFT(4,4)
	MSHIFT(5,3)
	MSHIFT(6,2)
	MSHIFT(7,1)

	/* 3 */	
	movzbl	3(%edx),%ecx
	addl	%ebx,%ecx
	MSHIFT(2,7)
	MSHIFT(3,6)
	MSHIFT(4,5)
	MSHIFT(5,4)
	MSHIFT(6,3)
	MSHIFT(7,2)
	
	MSHIFT(3,7)
	MSHIFT(4,6)
	MSHIFT(5,5)
	MSHIFT(6,4)
	MSHIFT(7,3)
	
	MSHIFT(4,7)
	MSHIFT(5,6)
	MSHIFT(6,5)
	MSHIFT(7,4)
	
	MSHIFT(5,7)
	MSHIFT(6,6)
	MSHIFT(7,5)
	
	MSHIFT(6,7)
	MSHIFT(7,6)
	
	MSHIFT(7,7)
		
#undef ARGn
	popl	%esi
	popl	%ebx
	ret
