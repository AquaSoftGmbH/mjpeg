/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <string.h>
#include "config.h"
#include "mjpeg_types.h"
#include "yuv4mpeg.h"
#include "mjpeg_logging.h"
#include "cpu_accel.h"
#include "motionsearch.h"

class y4mstream
{
public:
  int fd_in;
  int fd_out;
  y4m_frame_info_t iframeinfo;
  y4m_stream_info_t istreaminfo;
  y4m_frame_info_t oframeinfo;
  y4m_stream_info_t ostreaminfo;

    y4mstream ()
  {
    fd_in = 0;
    fd_out = 1;
  };

};

class deinterlacer
{
public:
  int width;
  int height;
  int cwidth;
  int cheight;
  int field_order;
  int both_fields;
  int verbose;
  int input_chroma_subsampling;
  int output_chroma_subsampling;
  int vertical_overshot_luma;
  int vertical_overshot_chroma;
  int mark_moving_blocks;
  int motion_threshold;
  int just_anti_alias;

  y4mstream Y4MStream;

  uint8_t *inframe[3];
  uint8_t *inframe0[3];

  uint8_t *outframe[3];
  uint8_t *scratch;

  void initialize_memory (int w, int h, int cw, int ch)
  {
    int luma_size;
    int chroma_size;

    // Some functions need some vertical overshot area
    // above and below the image. So we make the buffer
    // a little bigger...
    vertical_overshot_luma = 32*w;
    vertical_overshot_chroma = 32*cw;
    luma_size = ( w * h ) + 2 * vertical_overshot_luma;
    chroma_size = ( cw * ch ) + 2 * vertical_overshot_chroma;
  
      inframe[0] = (uint8_t *) malloc (luma_size) + vertical_overshot_luma;
      inframe[1] = (uint8_t *) malloc (chroma_size) + vertical_overshot_chroma;
      inframe[2] = (uint8_t *) malloc (chroma_size) + vertical_overshot_chroma;

      inframe0[0] = (uint8_t *) malloc (luma_size) + vertical_overshot_luma;
      inframe0[1] = (uint8_t *) malloc (chroma_size) + vertical_overshot_chroma;
      inframe0[2] = (uint8_t *) malloc (chroma_size) + vertical_overshot_chroma;

      outframe[0] = (uint8_t *) malloc (luma_size) + vertical_overshot_luma;
      outframe[1] = (uint8_t *) malloc (chroma_size) + vertical_overshot_chroma;
      outframe[2] = (uint8_t *) malloc (chroma_size) + vertical_overshot_chroma;

      scratch = (uint8_t *) malloc (luma_size) + vertical_overshot_luma;

	  width = w;
      height = h;
      cwidth = cw;
      cheight = ch;
  }

   deinterlacer ()
  {
  mark_moving_blocks = 0;
  motion_threshold = 4;
  just_anti_alias = 0;
  }

   ~deinterlacer ()
  {
    free (inframe[0] - vertical_overshot_luma);
    free (inframe[1] - vertical_overshot_chroma);
    free (inframe[2] - vertical_overshot_chroma);

    free (inframe0[0] - vertical_overshot_luma);
    free (inframe0[1] - vertical_overshot_chroma);
    free (inframe0[2] - vertical_overshot_chroma);

    free (outframe[0] - vertical_overshot_luma);
    free (outframe[1] - vertical_overshot_chroma);
    free (outframe[2] - vertical_overshot_chroma);

    free (scratch - vertical_overshot_chroma);

  }

void temporal_reconstruct_frame ( uint8_t * out,
								  uint8_t * in0,
								  uint8_t * in1,
								  int w,
								  int h,
								  int field)
{
int x,y;
uint32_t sad;
uint32_t min;
uint32_t var0,var1,var2,var3,var4;
int vx,vy,dx,dy,px,py;

int lx0[256];
int ly0[256];
int lx1[256];
int ly1[256];

// init vector-memory with zero-vectors
// these will be used to get more opticaly correct
// motion-vectors...

for( dx=0; dx<256; dx++)
	lx0[dx]=lx1[dx]=ly0[dx]=ly1[dx]=0;

// devide the image into 16x16 blocks
for(y=0;y<h;y+=16)
	{
	for(x=0;x<w;x+=16)
		{

		// center-block-variance
		var0 = psad_00 ( in0+(x  )+(y  )*w, in1+(x  )+(y  )*w, w, 16, 0x00ffffff);
		var0 /= 256;
		
		// top-left-block-variance (overlapped)
		var1 = psad_00 ( in0+(x-8)+(y-8)*w, in1+(x-8)+(y-8)*w, w, 16, 0x00ffffff);
		var1 /= 256;
		
		// top-right-block-variance
		var2 = psad_00 ( in0+(x+8)+(y-8)*w, in1+(x+8)+(y-8)*w, w, 16, 0x00ffffff);
		var2 /= 256;

		// bottom-left-block-variance
		var3 = psad_00 ( in0+(x-8)+(y+8)*w, in1+(x-8)+(y+8)*w, w, 16, 0x00ffffff);
		var3 /= 256;
		
		// bottom-right-block-variance
		var4 = psad_00 ( in0+(x+8)+(y+8)*w, in1+(x+8)+(y+8)*w, w, 16, 0x00ffffff);
		var4 /= 256;
		
		// if all block-variances are below the threshold we can savely assume
		// that there is no motion...
		if( ( var0 <= motion_threshold ) && 
			( var1 <= motion_threshold ) &&
			( var2 <= motion_threshold ) &&
			( var3 <= motion_threshold ) &&
			( var4 <= motion_threshold ) ) 
			{
			// use temporal interpolation for this block, we can do so, as it
			// is known to be non-moving...
			for(dy=0;dy<16;dy++)
				for(dx=0;dx<16;dx++)
					{
					*(out+(x+dx)+(y+dy)*w)=
						(
						*(in0+(x+dx)+(y+dy)*w)+
						*(in1+(x+dx)+(y+dy)*w)
						)/2;
					}
			// a non-moving block has a motion-vector of (0,0), so store this
			// in the vector-memory
			lx1[x/16]=0;
			ly1[x/16]=0;
			}
		else
			{
			// This block moves. So we need to find the best motion-compensated
			// interpolation we can get hands on...
			
			// reset search...
			min = 0x00ffffff;
			vx=vy=0;
			
			// try to get a prediction of the motion, first...
			if( (x/16)>=1 ) // a prediction is possible...
				{
				// FIXME: a median would be better, here...
				px = lx1[(x/16)-1]+lx0[(x/16)-1]+lx0[(x/16)];
				py = ly1[(x/16)-1]+ly0[(x/16)-1]+ly0[(x/16)];
				px /= 3;
				py /= 3;
				}
			else
				px=py=0; // a prediction is not possible...
						
			for(dy=-16;dy<16;dy+=2)
				for(dx=-16;dx<16;dx++)
					{
					if (field==0)
						{
						// do the motionsearch for the bottom field
						// calculate the block variance
						sad  = psad_00 ( in0+(x)+(y)*w+w, in1+(x+dx*2)+(y+dy*2)*w+w, w*2, 8, 0x00ffffff);
						sad += psad_00 ( in0+(x-dx*2)+(y-dy*2)*w+w, in1+(x)+(y)*w+w, w*2, 8, 0x00ffffff);
						sad += psad_00 ( in0+(x-dx)+(y-dy)*w+w, in1+(x+dx)+(y+dy)*w+w, w*2, 8, 0x00ffffff);
					
						// add a penalty for motion-vectors too far away from
						// the predicted location
						if( (x/16)>=1 ) // a prediction is possible...
							sad += 1024*(abs(px-dx)+abs(py-dy));
					
						if(min>sad)
							{
							vx=dx;
							vy=dy;
							min=sad;
							}
						}
					else
						{
						// do the motionsearch for the top field
						// calculate the block variance
						sad  = psad_00 ( in0+(x)+(y)*w, in1+(x+dx*2)+(y+dy*2)*w, w*2, 8, 0x00ffffff);
						sad += psad_00 ( in0+(x-dx*2)+(y-dy*2)*w, in1+(x)+(y)*w, w*2, 8, 0x00ffffff);
						sad += psad_00 ( in0+(x-dx)+(y-dy)*w, in1+(x+dx)+(y+dy)*w, w*2, 8, 0x00ffffff);
					
						// add a penalty for motion-vectors too far away from
						// the predicted location
						if( (x/16)>=1 ) // a prediction is possible...
							sad += 1024*(abs(px-dx)+abs(py-dy));
					
						if(min>sad)
							{
							vx=dx;
							vy=dy;
							min=sad;
							}
						}
					}
				
			// store the found vector in the vector-memory
			lx1[x/16]=vx;
			ly1[x/16]=vy;

			if (field==0)
			{
			// after we now (hopefuly) have the correct motion-vector for
			// the bottom field, use it to interpolate the missing data...
			for(dy=0;dy<18;dy+=2)
				for(dx=0;dx<18;dx++)
					{
					// just copy the top-field
					if((x+dx)<w)
					*(out+(x+dx)+(y+dy)*w)=*(in1+(x+dx)+(y+dy)*w);
					// insert the motion-compensated-interpolation for the 
					// bottom field
					if((x+dx)<w)
					*(out+(x+dx)+(y+dy+1)*w)=
						(
						*(in0+(x+dx-vx)+(y+dy-vy+1)*w)+
						*(in1+(x+dx+vx)+(y+dy+vy+1)*w)
						)/2;
					}
			}
			else
			{
			// after we now (hopefuly) have the correct motion-vector for
			// the top field, use it to interpolate the missing data...
			for(dy=0;dy<18;dy+=2)
				for(dx=0;dx<18;dx++)
					{
					// just copy the bottom-field
					if((x+dx)<w)
					*(out+(x+dx)+(y+dy+1)*w)=*(in1+(x+dx)+(y+dy+1)*w);
					// insert the motion-compensated-interpolation for the 
					// top field
					if((x+dx)<w)
					*(out+(x+dx)+(y+dy)*w)=
						(
						*(in0+(x+dx-vx)+(y+dy-vy)*w)+
						*(in1+(x+dx+vx)+(y+dy+vy)*w)
						)/2;
					}
			}
		
		if( field==0 )
			{
			// the only chance to protect the result agains false motion
			// is to apply a median-filter to the block
			for(dy=0;dy<18;dy+=2)
				for(dx=0;dx<18;dx++)
					{
					int a,b,c,d;
					
					a = *(out+(x+dx)+(y+dy+0)*w);
					b = *(out+(x+dx)+(y+dy+1)*w);
					c = *(out+(x+dx)+(y+dy+2)*w);
					
					if( a>b ) { d=a; a=b; b=d; }
					if( b>c ) { d=b; b=c; c=d; }
					if( a>b ) { d=a; a=b; b=d; }
				
					if((x+dx)<w)
					*(out+(x+dx)+(y+dy+1)*w)=b;
					}
			}
		else
			{
			for(dy=0;dy<18;dy+=2)
				for(dx=0;dx<18;dx++)
					{
					int a,b,c,d;
					
					a = *(out+(x+dx)+(y+dy-1)*w);
					b = *(out+(x+dx)+(y+dy+0)*w);
					c = *(out+(x+dx)+(y+dy+1)*w);
					
					if( a>b ) { d=a; a=b; b=d; }
					if( b>c ) { d=b; b=c; c=d; }
					if( a>b ) { d=a; a=b; b=d; }
				
					if((x+dx)<w)
					*(out+(x+dx)+(y+dy)*w)=b;
					}
			}
		
			// finaly we need to anti-alias the block...
			// This filter checks for the isophote-vector and
			// depending on how steep it is filters along that
			// vector with different filter-kernels...
			for(dy=0;dy<16;dy++)
				for(dx=0;dx<16;dx++)
					{
					int a,b,c,m;
					
					min = 0x00ffffff;
					px=0;
					
					for(vx=-2;vx<=2;vx++)
						{
						a = (
							1 * *(out+(x+dx-vx-1)+(y+dy-1)*w) +
							2 * *(out+(x+dx-vx  )+(y+dy-1)*w) +
							1 * *(out+(x+dx-vx+1)+(y+dy-1)*w) 
							)/ 4;

						b = (
							1 * *(out+(x+dx-1)+(y+dy+0)*w) +
							2 * *(out+(x+dx  )+(y+dy+0)*w) +
							1 * *(out+(x+dx+1)+(y+dy+0)*w) 
							)/ 4;
						
						c = (
							1 * *(out+(x+dx+vx-1)+(y+dy+1)*w) +
							2 * *(out+(x+dx+vx  )+(y+dy+1)*w) +
							1 * *(out+(x+dx+vx+1)+(y+dy+1)*w) 
							)/ 4;
						
						m = (a+b+c)/3;
						
						sad  = (m-a)*(m-a)+
							   (m-b)*(m-b)+
							   (m-c)*(m-c);
							   
						x -= 1;
						a = (
							1 * *(out+(x+dx-vx-1)+(y+dy-1)*w) +
							2 * *(out+(x+dx-vx  )+(y+dy-1)*w) +
							1 * *(out+(x+dx-vx+1)+(y+dy-1)*w) 
							)/ 4;

						b = (
							1 * *(out+(x+dx-1)+(y+dy+0)*w) +
							2 * *(out+(x+dx  )+(y+dy+0)*w) +
							1 * *(out+(x+dx+1)+(y+dy+0)*w) 
							)/ 4;
						
						c = (
							1 * *(out+(x+dx+vx-1)+(y+dy+1)*w) +
							2 * *(out+(x+dx+vx  )+(y+dy+1)*w) +
							1 * *(out+(x+dx+vx+1)+(y+dy+1)*w) 
							)/ 4;
						x +=1;
						
						m = (a+b+c)/3;
						
						sad += (m-a)*(m-a)+
							   (m-b)*(m-b)+
							   (m-c)*(m-c);
						
						x += 1;
						a = (
							1 * *(out+(x+dx-vx-1)+(y+dy-1)*w) +
							2 * *(out+(x+dx-vx  )+(y+dy-1)*w) +
							1 * *(out+(x+dx-vx+1)+(y+dy-1)*w) 
							)/ 4;

						b = (
							1 * *(out+(x+dx-1)+(y+dy+0)*w) +
							2 * *(out+(x+dx  )+(y+dy+0)*w) +
							1 * *(out+(x+dx+1)+(y+dy+0)*w) 
							)/ 4;
						
						c = (
							1 * *(out+(x+dx+vx-1)+(y+dy+1)*w) +
							2 * *(out+(x+dx+vx  )+(y+dy+1)*w) +
							1 * *(out+(x+dx+vx+1)+(y+dy+1)*w) 
							)/ 4;
						x -=1;
						
						m = (a+b+c)/3;
						
						sad += (m-a)*(m-a)+
							   (m-b)*(m-b)+
							   (m-c)*(m-c);
						
						if(sad<min)
							{
							min=sad;
							px=vx;
							}
						}

					if( px==0 )
						{
						// no filter for plain horizontal or vertical orientations
						*(scratch+(x+dx)+(y+dy)*w) = *(out+(x+dx)+(y+dy)*w) ;
						}
					else
					if( abs(px)==1 )
						{
						// gauss-filter for diagonal orientations
						*(scratch+(x+dx)+(y+dy)*w) = 
							( 2 * *(out+(x+dx)+(y+dy)*w) +
							  1 * *(out+(x+dx-px)+(y+dy-1)*w) +
							  1 * *(out+(x+dx+px)+(y+dy+1)*w) )/4;
						}
					else
					if( abs(px)==2 )
						{
						// bigger gauss kernel for even flatter angles
						*(scratch+(x+dx)+(y+dy)*w) = 
							( 4 * *(out+(x+dx+0)+(y+dy)*w) +
							  2 * *(out+(x+dx-1)+(y+dy)*w) +
							  2 * *(out+(x+dx+1)+(y+dy)*w) +
							  2 * *(out+(x+dx+0-px)+(y+dy-1)*w) +
							  1 * *(out+(x+dx-1-px)+(y+dy-1)*w) +
							  1 * *(out+(x+dx+1-px)+(y+dy-1)*w) +
							  2 * *(out+(x+dx+0+px)+(y+dy+1)*w) +
							  1 * *(out+(x+dx-1+px)+(y+dy+1)*w) +
							  1 * *(out+(x+dx+1+px)+(y+dy+1)*w) 
						)/16;
						}
					}	

			// if it is save to do so, copy the anti-aliassed block into
			// the outframe...
			for(dy=0;dy<16;dy++)
				for(dx=0;dx<16;dx++)
					{
					if( (y+dy)>=2 && (y+dy)<(h-2) && x>2 && x<(w-2) )
					*(out+(x+dx)+(y+dy)*w) = *(scratch+(x+dx)+(y+dy)*w);
					}
				
			if(mark_moving_blocks)
			for(dy=0;dy<16;dy++)
				for(dx=0;dx<16;dx++)
					{
					*(out+(x+dx)+(y+dy)*w) = 64+*(out+(x+dx)+(y+dy)*w)/2;
					}

				}
		}
	// before processing the next line of blocks, we save the motion-vectors
	// of this line in lx0 and ly0, so we can get a prediction of the motion
	// in the next line...
	for(dx=0;dx<(w/16);dx++)
		{
		lx0[dx]=lx1[dx];
		ly0[dx]=ly1[dx];
		}
	}
}

  void deinterlace_motion_compensated ()
  {
  static int frame=0;

  if(field_order==0 && frame!=0)
  {	  
  temporal_reconstruct_frame (outframe[0], inframe[0], inframe0[0], width, height, 0 );
  temporal_reconstruct_frame (outframe[1], inframe[1], inframe0[1], cwidth, cheight, 0 );
  temporal_reconstruct_frame (outframe[2], inframe[2], inframe0[2], cwidth, cheight, 0 );
  
  y4m_write_frame (Y4MStream.fd_out, &Y4MStream.ostreaminfo,
				   &Y4MStream.oframeinfo, outframe);

  if( both_fields==1 && frame!=0)
	  {
	  temporal_reconstruct_frame (outframe[0], inframe0[0], inframe[0], width, height, 1 );
	  temporal_reconstruct_frame (outframe[1], inframe0[1], inframe[1], cwidth, cheight, 1 );
	  temporal_reconstruct_frame (outframe[2], inframe0[2], inframe[2], cwidth, cheight, 1 );
  
	  y4m_write_frame (Y4MStream.fd_out, &Y4MStream.ostreaminfo,
				   &Y4MStream.oframeinfo, outframe);
	  }
  }
  else
  {	  
  temporal_reconstruct_frame (outframe[0], inframe0[0], inframe[0], width, height, 1 );
  temporal_reconstruct_frame (outframe[1], inframe0[1], inframe[1], cwidth, cheight, 1 );
  temporal_reconstruct_frame (outframe[2], inframe0[2], inframe[2], cwidth, cheight, 1 );
  
  y4m_write_frame (Y4MStream.fd_out, &Y4MStream.ostreaminfo,
				   &Y4MStream.oframeinfo, outframe);

  if( both_fields==1 && frame!=0)
	  {
	  temporal_reconstruct_frame (outframe[0], inframe[0], inframe0[0], width, height, 0 );
	  temporal_reconstruct_frame (outframe[1], inframe[1], inframe0[1], cwidth, cheight, 0 );
	  temporal_reconstruct_frame (outframe[2], inframe[2], inframe0[2], cwidth, cheight, 0 );
  
	  y4m_write_frame (Y4MStream.fd_out, &Y4MStream.ostreaminfo,
				   &Y4MStream.oframeinfo, outframe);
	  }
  }

  memcpy (inframe0[0], inframe[0], width*height);
  memcpy (inframe0[1], inframe[1], cwidth*cheight);
  memcpy (inframe0[2], inframe[2], cwidth*cheight);
  
  if(frame==0) frame++;
  }

void antialias_plane (uint8_t * out, int w, int h)
{
int x,y;
int a,b,c,m;
uint32_t min,sad;
int vx,px;

for(y=0;y<h;y++)
for(x=0;x<w;x++)
{
	min = 0x00ffffff;
	px=0;
					
	for(vx=-4;vx<=4;vx++)
		{
						a = (
							1 * *(out+(x-vx-1)+(y-1)*w) +
							2 * *(out+(x-vx  )+(y-1)*w) +
							1 * *(out+(x-vx+1)+(y-1)*w) 
							)/ 4;

						b = (
							1 * *(out+(x-1)+(y+0)*w) +
							2 * *(out+(x  )+(y+0)*w) +
							1 * *(out+(x+1)+(y+0)*w) 
							)/ 4;
						
						c = (
							1 * *(out+(x+vx-1)+(y+1)*w) +
							2 * *(out+(x+vx  )+(y+1)*w) +
							1 * *(out+(x+vx+1)+(y+1)*w) 
							)/ 4;
						
						m = (a+b+c)/3;
						
						sad  = (m-a)*(m-a)+
							   (m-b)*(m-b)*2+
							   (m-c)*(m-c);
						
						x--;
						a = (
							1 * *(out+(x-vx-1)+(y-1)*w) +
							2 * *(out+(x-vx  )+(y-1)*w) +
							1 * *(out+(x-vx+1)+(y-1)*w) 
							)/ 4;

						b = (
							1 * *(out+(x-1)+(y+0)*w) +
							2 * *(out+(x  )+(y+0)*w) +
							1 * *(out+(x+1)+(y+0)*w) 
							)/ 4;
						
						c = (
							1 * *(out+(x+vx-1)+(y+1)*w) +
							2 * *(out+(x+vx  )+(y+1)*w) +
							1 * *(out+(x+vx+1)+(y+1)*w) 
							)/ 4;
						
						m = (a+b+c)/3;
						
						sad += (m-a)*(m-a)+
							   (m-b)*(m-b)*2+
							   (m-c)*(m-c);
						x+=2;
						a = (
							1 * *(out+(x-vx-1)+(y-1)*w) +
							2 * *(out+(x-vx  )+(y-1)*w) +
							1 * *(out+(x-vx+1)+(y-1)*w) 
							)/ 4;

						b = (
							1 * *(out+(x-1)+(y+0)*w) +
							2 * *(out+(x  )+(y+0)*w) +
							1 * *(out+(x+1)+(y+0)*w) 
							)/ 4;
						
						c = (
							1 * *(out+(x+vx-1)+(y+1)*w) +
							2 * *(out+(x+vx  )+(y+1)*w) +
							1 * *(out+(x+vx+1)+(y+1)*w) 
							)/ 4;
						
						m = (a+b+c)/3;
						
						sad += (m-a)*(m-a)+
							   (m-b)*(m-b)*2+
							   (m-c)*(m-c);
						x--;
						
						if(sad<min)
							{
							min=sad;
							px=vx;
							}
						}

					if( abs(px)<=1 )
						{
						// gauss-filter for diagonal orientations
						*(scratch+(x)+(y)*w) = 
							( 2 * *(out+(x   )+(y  )*w) +
							  1 * *(out+(x-px)+(y-1)*w) +
							  1 * *(out+(x+px)+(y+1)*w) )/4;
						}
					else
					if( abs(px)>=2 )
						{
						// bigger gauss kernel for even flatter angles
						*(scratch+(x)+(y)*w) = 
							( 4 * *(out+(x+0)+(y)*w) +
							  2 * *(out+(x-1)+(y)*w) +
							  2 * *(out+(x+1)+(y)*w) +
							  2 * *(out+(x+0-px)+(y-1)*w) +
							  1 * *(out+(x-1-px)+(y-1)*w) +
							  1 * *(out+(x+1-px)+(y-1)*w) +
							  2 * *(out+(x+0+px)+(y+1)*w) +
							  1 * *(out+(x-1+px)+(y+1)*w) +
							  1 * *(out+(x+1+px)+(y+1)*w) 
						)/16;
						}
}	

for(y=2;y<(h-2);y++)
for(x=2;x<(w-2);x++)
{
	*(out+(x)+(y)*w) = (*(out+(x)+(y)*w)+*(scratch+(x)+(y)*w)*2)/3;
}
}

void antialias_frame ()
{				
	  antialias_plane ( inframe[0], width, height );
	  y4m_write_frame (Y4MStream.fd_out, &Y4MStream.ostreaminfo,
				   &Y4MStream.oframeinfo, inframe);
}

};

int
main (int argc, char *argv[])
{
  int frame = 0;
  int errno = 0;

  deinterlacer YUVdeint;

  char c;

  YUVdeint.field_order = -1;

  mjpeg_log (LOG_INFO, "-------------------------------------------------");
  mjpeg_log (LOG_INFO, "       Motion-Compensating-Deinterlacer          ");
  mjpeg_log (LOG_INFO, "-------------------------------------------------");

  while ((c = getopt (argc, argv, "hvds:t:ma")) != -1)
    {
      switch (c)
	{
	case 'h':
	  {
	    mjpeg_log (LOG_INFO, " Usage of the deinterlacer");
	    mjpeg_log (LOG_INFO, " -------------------------");
	    mjpeg_log (LOG_INFO, " -v be verbose");
	    mjpeg_log (LOG_INFO, " -d output both fields");
	    mjpeg_log (LOG_INFO, " -m mark moving blocks");
	    mjpeg_log (LOG_INFO, " -t [nr] (default 4) motion threshold.");
	    mjpeg_log (LOG_INFO, "         0 -> every block is moving.");
	    mjpeg_log (LOG_INFO, " -a just antialias the frames! This will");
	    mjpeg_log (LOG_INFO, "    assume progressive but aliassed input.");
	    mjpeg_log (LOG_INFO, "    you can use this to improve badly deinterlaced");
	    mjpeg_log (LOG_INFO, "    footage. EG: deinterlaced with cubic-interpolation");
	    mjpeg_log (LOG_INFO, "    or worse...");
	  
	    mjpeg_log (LOG_INFO,
		       " -s [n=0/1] forces field-order in case of misflagged streams");
	    mjpeg_log (LOG_INFO, "    -s0 is top-field-first");
	    mjpeg_log (LOG_INFO, "    -s1 is bottom-field-first");
	    exit (0);
	    break;
	  }
	case 'v':
	  {
	    YUVdeint.verbose = 1;
	    break;
	  }
	case 'd':
	  {
	    YUVdeint.both_fields = 1;
	    mjpeg_log (LOG_INFO,
		       "Regenerating both fields. Please fix the Framerate.");
	    break;
	  }
	case 'm':
	  {
	    YUVdeint.mark_moving_blocks = 1;
	    mjpeg_log (LOG_INFO,
		       "I will mark detected moving blocks for you, so you can");
	    mjpeg_log (LOG_INFO,
		       "fine-tune the motion-threshold (-t)...");
	    break;
	  }
	case 'a':
	  {
	    YUVdeint.just_anti_alias = 1;
		YUVdeint.field_order = 0; // just to prevent the program to barf in this case
	    mjpeg_log (LOG_INFO,
		       "I will just anti-alias the frames. make sure they are progressive!");
	    break;
	  }
	case 't':
	  {
	    YUVdeint.motion_threshold = atoi (optarg);
		mjpeg_log (LOG_INFO, "motion-threshold set to : %i",YUVdeint.motion_threshold);
	  break;
	  }
	case 's':
	  {
	    YUVdeint.field_order = atoi (optarg);
	    if (YUVdeint.field_order != 0)
	      {
		mjpeg_log (LOG_INFO, "forced top-field-first!");
		YUVdeint.field_order = 1;
	      }
	    else
	      {
		mjpeg_log (LOG_INFO, "forced bottom-field-first!");
		YUVdeint.field_order = 0;
	      }
	    break;
	  }
	}
    }

  // initialize motionsearch-library      
  init_motion_search ();

  // initialize stream-information 
  y4m_accept_extensions (1);
  y4m_init_stream_info (&YUVdeint.Y4MStream.istreaminfo);
  y4m_init_frame_info (&YUVdeint.Y4MStream.iframeinfo);
  y4m_init_stream_info (&YUVdeint.Y4MStream.ostreaminfo);
  y4m_init_frame_info (&YUVdeint.Y4MStream.oframeinfo);

/* open input stream */
  if ((errno =
       y4m_read_stream_header (YUVdeint.Y4MStream.fd_in,
			       &YUVdeint.Y4MStream.istreaminfo)) != Y4M_OK)
    {
      mjpeg_log (LOG_ERROR, "Couldn't read YUV4MPEG header: %s!",
		 y4m_strerr (errno));
      exit (1);
    }

  /* get format information */
  YUVdeint.width = y4m_si_get_width (&YUVdeint.Y4MStream.istreaminfo);
  YUVdeint.height = y4m_si_get_height (&YUVdeint.Y4MStream.istreaminfo);
  YUVdeint.input_chroma_subsampling =
    y4m_si_get_chroma (&YUVdeint.Y4MStream.istreaminfo);
  mjpeg_log (LOG_INFO, "Y4M-Stream is %ix%i(%s)", YUVdeint.width,
	     YUVdeint.height,
	     y4m_chroma_keyword (YUVdeint.input_chroma_subsampling));

  /* if chroma-subsampling isn't supported bail out ... */
  if (YUVdeint.input_chroma_subsampling == Y4M_CHROMA_420JPEG ||
      YUVdeint.input_chroma_subsampling == Y4M_CHROMA_420MPEG2 ||
      YUVdeint.input_chroma_subsampling == Y4M_CHROMA_420PALDV)
    {
      YUVdeint.cwidth = YUVdeint.width / 2;
      YUVdeint.cheight = YUVdeint.height / 2;
    }
  else if (YUVdeint.input_chroma_subsampling == Y4M_CHROMA_444)
    {
      YUVdeint.cwidth = YUVdeint.width;
      YUVdeint.cheight = YUVdeint.height;
    }
  else if (YUVdeint.input_chroma_subsampling == Y4M_CHROMA_422)
    {
      YUVdeint.cwidth = YUVdeint.width / 2;
      YUVdeint.cheight = YUVdeint.height;
    }
  else if (YUVdeint.input_chroma_subsampling == Y4M_CHROMA_411)
    {
      YUVdeint.cwidth = YUVdeint.width / 4;
      YUVdeint.cheight = YUVdeint.height;
    }
  else
    {
      mjpeg_log (LOG_ERROR,
		 "Y4M-Stream is not in a supported chroma-format. Sorry.");
      exit (-1);
    }

  /* the output is progressive 4:2:0 MPEG 1 */
  y4m_si_set_interlace (&YUVdeint.Y4MStream.ostreaminfo, Y4M_ILACE_NONE);
  y4m_si_set_chroma (&YUVdeint.Y4MStream.ostreaminfo,
		     YUVdeint.input_chroma_subsampling);
  y4m_si_set_width (&YUVdeint.Y4MStream.ostreaminfo, YUVdeint.width);
  y4m_si_set_height (&YUVdeint.Y4MStream.ostreaminfo, YUVdeint.height);
  y4m_si_set_framerate (&YUVdeint.Y4MStream.ostreaminfo,
			y4m_si_get_framerate (&YUVdeint.Y4MStream.istreaminfo));
  y4m_si_set_sampleaspect (&YUVdeint.Y4MStream.ostreaminfo,
			   y4m_si_get_sampleaspect (&YUVdeint.Y4MStream.istreaminfo));

/* check for field dominance */

  if (YUVdeint.field_order == -1)
    {
      /* field-order was not specified on commandline. So we try to
       * get it from the stream itself...
       */

      if (y4m_si_get_interlace (&YUVdeint.Y4MStream.istreaminfo) ==
	  Y4M_ILACE_TOP_FIRST)
	{
	  /* got it: Top-field-first... */
	  mjpeg_log (LOG_INFO, " Stream is interlaced, top-field-first.");
	  YUVdeint.field_order = 1;
	}
      else if (y4m_si_get_interlace (&YUVdeint.Y4MStream.istreaminfo) ==
	       Y4M_ILACE_BOTTOM_FIRST)
	{
	  /* got it: Bottom-field-first... */
	  mjpeg_log (LOG_INFO, " Stream is interlaced, bottom-field-first.");
	  YUVdeint.field_order = 0;
	}
      else
	{
	  mjpeg_log (LOG_ERROR,
		     "Unable to determine field-order from input-stream.  ");
	  mjpeg_log (LOG_ERROR,
		     "This is most likely the case when using mplayer to  ");
	  mjpeg_log (LOG_ERROR,
		     "produce my input-stream.                            ");
	  mjpeg_log (LOG_ERROR,
		     "                                                    ");
	  mjpeg_log (LOG_ERROR,
		     "Either the stream is misflagged or progressive...   ");
	  mjpeg_log (LOG_ERROR,
		     "I will stop here, sorry. Please choose a field-order");
	  mjpeg_log (LOG_ERROR,
		     "with -s0 or -s1. Otherwise I can't do anything for  ");
	  mjpeg_log (LOG_ERROR, "you. TERMINATED. Thanks...");
	  exit (-1);
	}
    }

  // initialize deinterlacer internals
  YUVdeint.initialize_memory (YUVdeint.width, YUVdeint.height,
			      YUVdeint.cwidth, YUVdeint.cheight);

  /* write the outstream header */
  y4m_write_stream_header (YUVdeint.Y4MStream.fd_out, &YUVdeint.Y4MStream.ostreaminfo);

  /* read every frame until the end of the input stream and process it */
  while (Y4M_OK == (errno = y4m_read_frame (YUVdeint.Y4MStream.fd_in,
					    &YUVdeint.Y4MStream.istreaminfo,
					    &YUVdeint.Y4MStream.iframeinfo,
					    YUVdeint.inframe)))
    {
	  if(!YUVdeint.just_anti_alias)
		  YUVdeint.deinterlace_motion_compensated ();
	  else
		  YUVdeint.antialias_frame ();

	frame++;
    }

  return 0;
};
