/* fast int primitives. min,max,rnddiv2
 *
 * WARNING: Assumes 2's complement arithmetic.
 */
static __inline__ int intmax( register int x, register int y )
{
	return x < y ? y : x;
}

static __inline__ int intmin( register int x, register int y )
{
	return x < y ? x : y;
}

static __inline__ int rnddiv2( int x )
{
	return (x+(x>0))>>1;
}
