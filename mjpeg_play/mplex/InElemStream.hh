/******************************
 * 
 * InElemStream
 *
 * Top-level encapsulation of an input elementary stream.
 * N.b. the underlying stream proper may of course be
 * wrapped up in something else so the source of the
 * raw data is always a *parser*.  For true elementary
 * streams this is of course just NULL
 *
 ****************************/


/* For the moment we stick the parser class here until things
   stabilise when it'll wander off into its own file.  For
   the moment the InElemStream class itself is mostly just a wrapper
   for the old access unit vectors...
*/

class InStreamParse
{
	open( char * );
	read( 
}
