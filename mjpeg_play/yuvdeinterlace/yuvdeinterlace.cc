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

#include "config.h"
#include <string.h>
#include <math.h>
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
  uint8_t *inframe1[3];
  uint8_t *inframe2[3];
  uint8_t *inframe3[3];

  uint8_t *outframe[3];
  uint8_t *scratch;

  void initialize_memory (int w, int h, int cw, int ch)
  {
    int luma_size;
    int chroma_size;

    // Some functions need some vertical overshoot area
    // above and below the image. So we make the buffer
    // a little bigger...
    vertical_overshot_luma = 32*w;
    vertical_overshot_chroma = 32*cw;
    luma_size = ( w * h ) + 2 * vertical_overshot_luma;
    chroma_size = ( cw * ch ) + 2 * vertical_overshot_chroma;
  
      inframe[0] = (uint8_t *) malloc (luma_size + vertical_overshot_luma);
      inframe[1] = (uint8_t *) malloc (chroma_size + vertical_overshot_chroma);
      inframe[2] = (uint8_t *) malloc (chroma_size + vertical_overshot_chroma);

      inframe0[0] = (uint8_t *) malloc (luma_size + vertical_overshot_luma);
      inframe0[1] = (uint8_t *) malloc (chroma_size + vertical_overshot_chroma);
      inframe0[2] = (uint8_t *) malloc (chroma_size + vertical_overshot_chroma);

      inframe1[0] = (uint8_t *) malloc (luma_size + vertical_overshot_luma);
      inframe1[1] = (uint8_t *) malloc (chroma_size + vertical_overshot_chroma);
      inframe1[2] = (uint8_t *) malloc (chroma_size + vertical_overshot_chroma);

      inframe2[0] = (uint8_t *) malloc (luma_size + vertical_overshot_luma);
      inframe2[1] = (uint8_t *) malloc (chroma_size + vertical_overshot_chroma);
      inframe2[2] = (uint8_t *) malloc (chroma_size + vertical_overshot_chroma);

      inframe3[0] = (uint8_t *) malloc (luma_size + vertical_overshot_luma);
      inframe3[1] = (uint8_t *) malloc (chroma_size + vertical_overshot_chroma);
      inframe3[2] = (uint8_t *) malloc (chroma_size + vertical_overshot_chroma);

      outframe[0] = (uint8_t *) malloc (luma_size + vertical_overshot_luma);
      outframe[1] = (uint8_t *) malloc (chroma_size + vertical_overshot_chroma);
      outframe[2] = (uint8_t *) malloc (chroma_size + vertical_overshot_chroma);

      scratch = (uint8_t *) malloc (luma_size + vertical_overshot_luma);

      width = w;
      height = h;
      cwidth = cw;
      cheight = ch;
  }

   deinterlacer ()
  {
  both_fields = 0;
  mark_moving_blocks = 0;
  motion_threshold = 4;
  just_anti_alias = 0;
  }

   ~deinterlacer ()
  {
    free (inframe[0]);
    free (inframe[1]);
    free (inframe[2]);

    free (inframe0[0]);
    free (inframe0[1]);
    free (inframe0[2]);

    free (inframe1[0]);
    free (inframe1[1]);
    free (inframe1[2]);

    free (inframe2[0]);
    free (inframe2[1]);
    free (inframe2[2]);

    free (inframe3[0]);
    free (inframe3[1]);
    free (inframe3[2]);

    free (outframe[0]);
    free (outframe[1]);
    free (outframe[2]);

    free (scratch);
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
int radius=8;
int bx,by,fx,fy;

static int lx0[256];
static int ly0[256];
static int lx1[256];
static int ly1[256];

// init vector-memory with zero-vectors
// these will be used to get more opticaly correct
// motion-vectors...

for( dx=0; dx<256; dx++)
	lx0[dx]=lx1[dx]=ly0[dx]=ly1[dx]=0;

memcpy ( in0-w*1,in0,w );
memcpy ( in0-w*2,in0,w );
memcpy ( in0-w*3,in0,w );
memcpy ( in0-w*4,in0,w );
memcpy ( in0-w*5,in0,w );
memcpy ( in0-w*6,in0,w );
memcpy ( in0-w*7,in0,w );

memcpy ( in1-w*1,in1,w );
memcpy ( in1-w*2,in1,w );
memcpy ( in1-w*3,in1,w );
memcpy ( in1-w*4,in1,w );
memcpy ( in1-w*5,in1,w );
memcpy ( in1-w*6,in1,w );
memcpy ( in1-w*7,in1,w );

memcpy ( in0+(h+0)*w,in0+(h-1)*w,w );
memcpy ( in0+(h+1)*w,in0+(h-1)*w,w );
memcpy ( in0+(h+2)*w,in0+(h-1)*w,w );
memcpy ( in0+(h+3)*w,in0+(h-1)*w,w );
memcpy ( in0+(h+4)*w,in0+(h-1)*w,w );
memcpy ( in0+(h+5)*w,in0+(h-1)*w,w );
memcpy ( in0+(h+6)*w,in0+(h-1)*w,w );
memcpy ( in0+(h+7)*w,in0+(h-1)*w,w );

memcpy ( in1+(h+0)*w,in1+(h-1)*w,w );
memcpy ( in1+(h+1)*w,in1+(h-1)*w,w );
memcpy ( in1+(h+2)*w,in1+(h-1)*w,w );
memcpy ( in1+(h+3)*w,in1+(h-1)*w,w );
memcpy ( in1+(h+4)*w,in1+(h-1)*w,w );
memcpy ( in1+(h+5)*w,in1+(h-1)*w,w );
memcpy ( in1+(h+6)*w,in1+(h-1)*w,w );
memcpy ( in1+(h+7)*w,in1+(h-1)*w,w );

// at first do an ELA-Interpolation of the reference field
// this is needed because we need inter-field(!) motion-vectors

memcpy(out,in1,w*h);
for(y=1-field;y<h;y+=2)
	for(x=0;x<w;x++)
	{
	int ELA_min=255;
	int ELA_dif;
	int a,b,c;

	a = *(out+(x)+(y-1)*w);
	c = *(out+(x)+(y+1)*w);

	*(out+(x)+(y)*w) = (a+c)/2;

	for(vx=0;vx<=5;vx++)
		{
		ELA_dif = abs ( *(out+(x-vx  )+(y-1)*w) - *(out+(x+vx  )+(y+1)*w) );
		b = ( *(out+(x-vx  )+(y-1)*w) + *(out+(x+vx  )+(y+1)*w) )/2;

		if( ELA_dif<ELA_min && ( (a<=b && b<=c) || (a>=b && b>=c) ) ) 
			{
			ELA_min=ELA_dif;
			*(out+(x)+(y)*w) = b;
			}

		ELA_dif = abs ( *(out+(x+vx  )+(y-1)*w) - *(out+(x-vx  )+(y+1)*w) );
		b = ( *(out+(x+vx  )+(y-1)*w) + *(out+(x-vx  )+(y+1)*w) )/2;

		if( ELA_dif<ELA_min && ( (a<=b && b<=c) || (a>=b && b>=c) ) ) 
			{
			ELA_min=ELA_dif;
			*(out+(x)+(y)*w) = b;
			}
		}
	}

// devide the image into 16x16 macro-blocks
for(y=0;y<h;y+=16)
	{
	for(x=0;x<w;x+=16)
		{
		// for testing purposes we will use a simple (and dumb...) full-search
		// this will later be replaced with an EPZS, again.

y -= 4;
		if(field==0)
			{
			min  = psad_00 ( in0+(x)+(y)*w+w, out+(x)+(y)*w+w, w*2,16, 0x00ffffff);
			}
		else
			{
			min  = psad_00 ( in0+(x)+(y)*w, out+(x)+(y)*w, w*2,16, 0x00ffffff);
			}
		bx=by=0;
		radius=8; // usualy larger radii do not make sense for deinterlacing
		for(dy=(-radius);dy<(+radius);dy+=2)
			for(dx=(-radius);dx<(+radius);dx++)
				{
				if (field==0)
					{
					// do the motionsearch for the bottom field
					// calculate the block variance
					sad  = psad_00 ( in0+(x+dx)+(y+dy)*w+w, out+(x)+(y)*w+w, w*2,16, 0x00ffffff);
				
					if(min>sad)
						{
						bx=dx;
						by=dy;
						min=sad;
						}
					}
				else
					{
					// do the motionsearch for the top field
					// calculate the block variance
					sad  = psad_00 ( in0+(x+dx)+(y+dy)*w, out+(x)+(y)*w, w*2,16, 0x00ffffff);
				
					if(min>sad)
						{
						bx=dx;
						by=dy;
						min=sad;
						}
					}
				}

		if(field==0)
			min = psad_00 ( in1+(x)+(y)*w+w, out+(x)+(y)*w+w, w*2,16, 0x00ffffff);
		else
			min = psad_00 ( in1+(x)+(y)*w, out+(x)+(y)*w, w*2,16, 0x00ffffff);
		fx=fy=0;
		radius=8;
		for(dy=(-radius);dy<(+radius);dy+=2)
			for(dx=(-radius);dx<(+radius);dx++)
				{
				if (field==0)
					{
					// do the motionsearch for the bottom field
					// calculate the block variance
					sad = psad_00 ( in1+(x+dx)+(y+dy)*w+w, out+(x)+(y)*w+w, w*2,16, 0x00ffffff);
				
					if(min>sad)
						{
						fx=dx;
						fy=dy;
						min=sad;
						}
					}
				else
					{
					// do the motionsearch for the top field
					// calculate the block variance
					sad = psad_00 ( in1+(x+dx)+(y+dy)*w, out+(x)+(y)*w, w*2,16, 0x00ffffff);
				
					if(min>sad)
						{
						fx=dx;
						fy=dy;
						min=sad;
						}
					}
				}
y += 4;

		// limit the vectors to the allowed range
		if( (x+fx)>width  || (x+fx)<0 ) fx=0;
		if( (y+fy)>height || (y+fy)<0 ) fy=0;
		if( (x+bx)>width  || (x+bx)<0 ) bx=0;
		if( (y+by)>height || (y+by)<0 ) by=0;
	
		if (field==0)
		{
		// after we now (hopefully) have the correct motion-vector for
		// the bottom field, use it to interpolate the missing data...
		for(dy=0;dy<16;dy+=2)
			for(dx=0;dx<16;dx++)
				{
				int v;
				int v0;
				// just copy the top-field
				//*(out+(x+dx)+(y+dy)*w)=*(in1+(x+dx)+(y+dy)*w);
					// insert the motion-compensated-interpolation for the 
				// bottom field
				v=  *(in1+(x+dx)+(y+dy-6)*w)* -0+
				    *(in1+(x+dx)+(y+dy-4)*w)* +0+
				    *(in1+(x+dx)+(y+dy-2)*w)*-26+
				    *(in1+(x+dx)+(y+dy  )*w)*+526+ 
				    *(in1+(x+dx)+(y+dy+2)*w)*+526+
				    *(in1+(x+dx)+(y+dy+4)*w)*-26+
				    *(in1+(x+dx)+(y+dy+6)*w)* +0+
				    *(in1+(x+dx)+(y+dy+8)*w)* -0;

				// highpass-part (previous field)
				
				v+= (
				    *(in0+(x+dx+bx)+(y+dy+by-7)*w)*  0+
				    *(in0+(x+dx+bx)+(y+dy+by-5)*w)* -0+
				    *(in0+(x+dx+bx)+(y+dy+by-3)*w)* +31+
				    *(in0+(x+dx+bx)+(y+dy+by-1)*w)*-116+
				    *(in0+(x+dx+bx)+(y+dy+by+1)*w)*+170+
				    *(in0+(x+dx+bx)+(y+dy+by+3)*w)*-116+
				    *(in0+(x+dx+bx)+(y+dy+by+5)*w)* +31+
				    *(in0+(x+dx+bx)+(y+dy+by+7)*w)* -0+
				    *(in0+(x+dx+bx)+(y+dy+by+9)*w)*  0
				    );

				v+= (
				    *(in1+(x+dx+fx)+(y+dy+fy-7)*w)*  0+
				    *(in1+(x+dx+fx)+(y+dy+fy-5)*w)* -0+
				    *(in1+(x+dx+fx)+(y+dy+fy-3)*w)* +31+
				    *(in1+(x+dx+fx)+(y+dy+fy-1)*w)*-116+
				    *(in1+(x+dx+fx)+(y+dy+fy+1)*w)*+170+
				    *(in1+(x+dx+fx)+(y+dy+fy+3)*w)*-116+
				    *(in1+(x+dx+fx)+(y+dy+fy+5)*w)* +31+
				    *(in1+(x+dx+fx)+(y+dy+fy+7)*w)* -0+
				    *(in1+(x+dx+fx)+(y+dy+fy+9)*w)*  -0
				    );

v /= 1000;
				v0 = (  *(in0+(x+dx+bx)+(y+dy+by+1)*w)+
					*(in1+(x+dx+fx)+(y+dy+fy+1)*w)
					)/2;

				v = v>255? 255:v;
				v = v<0  ?   0:v;

					//if(abs(v0-v)<8) 
					//	v=v0;

				*(out+(x+dx)+(y+dy+1)*w)=v;
				}
			}
			else
			{
			// after we now (hopefully) have the correct motion-vector for
			// the top field, use it to interpolate the missing data...
			for(dy=0;dy<16;dy+=2)
				for(dx=0;dx<16;dx++)
					{
					int v;
					int v0;
					// just copy the bottom-field
//			*(out+(x+dx)+(y+dy+1)*w)=*(in1+(x+dx)+(y+dy+1)*w);

					// insert the motion-compensated-interpolation for the 
					// bottom field

					//lowpass-part (current field)
					v = *(in1+(x+dx)+(y+dy-7)*w)*  -0+
					    *(in1+(x+dx)+(y+dy-5)*w)* +0+ 
					    *(in1+(x+dx)+(y+dy-3)*w)*-26+
					    *(in1+(x+dx)+(y+dy-1)*w)*+526+ 
					    *(in1+(x+dx)+(y+dy+1)*w)*+526+
					    *(in1+(x+dx)+(y+dy+3)*w)*-26+
					    *(in1+(x+dx)+(y+dy+5)*w)* +0+
					    *(in1+(x+dx)+(y+dy+7)*w)*  -0;

					//highpass-part (previous field)
					v+= ( *(in0+(x+dx+bx)+(y+dy+by-4)*w)* +31+
					      *(in0+(x+dx+bx)+(y+dy+by-2)*w)* -116+
					      *(in0+(x+dx+bx)+(y+dy+by  )*w)*+170+
					      *(in0+(x+dx+bx)+(y+dy+by+2)*w)* -116+
					      *(in0+(x+dx+bx)+(y+dy+by+4)*w)* +31 );

					//highpass-part (next field)
					v+= ( *(in1+(x+dx+fx)+(y+dy+fy-4)*w)* +31+
					      *(in1+(x+dx+fx)+(y+dy+fy-2)*w)* -116+
					      *(in1+(x+dx+fx)+(y+dy+fy  )*w)*+170+
					      *(in1+(x+dx+fx)+(y+dy+fy+2)*w)* -116+
					      *(in1+(x+dx+fx)+(y+dy+fy+4)*w)* +31 );
v /= 1000;
					v0 = (  *(in0+(x+dx+bx)+(y+dy+by)*w)+
						*(in1+(x+dx+fx)+(y+dy+fy)*w)
						)/2;

					v = v>255? 255:v;
					v = v<0  ?   0:v;

					//if(abs(v0-v)<8) 
					//	v=v0;

					*(out+(x+dx)+(y+dy)*w)=v;
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
}

  void deinterlace_motion_compensated ()
  {
  static int frame=0;

  if(field_order==0 && frame!=0)
  {	  
  temporal_reconstruct_frame (outframe[0], inframe[0], inframe0[0], width, height, 0 );
  temporal_reconstruct_frame (outframe[1], inframe[1], inframe0[1], cwidth, cheight, 0 );
  temporal_reconstruct_frame (outframe[2], inframe[2], inframe0[2], cwidth, cheight, 0 );
  
  antialias_plane (outframe[0],width,height);

  y4m_write_frame (Y4MStream.fd_out, &Y4MStream.ostreaminfo,
				   &Y4MStream.oframeinfo, outframe);

  if( both_fields==1 && frame!=0)
	  {
	  temporal_reconstruct_frame (outframe[0], inframe0[0], inframe[0], width, height, 1 );
	  temporal_reconstruct_frame (outframe[1], inframe0[1], inframe[1], cwidth, cheight, 1 );
	  temporal_reconstruct_frame (outframe[2], inframe0[2], inframe[2], cwidth, cheight, 1 );
  
	  antialias_plane (outframe[0],width,height);

	  y4m_write_frame (Y4MStream.fd_out, &Y4MStream.ostreaminfo,
				   &Y4MStream.oframeinfo, outframe);
	  }
  }
  else
  {	  
  temporal_reconstruct_frame (outframe[0], inframe0[0], inframe[0], width, height, 1 );
  temporal_reconstruct_frame (outframe[1], inframe0[1], inframe[1], cwidth, cheight, 1 );
  temporal_reconstruct_frame (outframe[2], inframe0[2], inframe[2], cwidth, cheight, 1 );
  
  //antialias_plane (outframe[0],width,height);

  //y4m_write_frame (Y4MStream.fd_out, &Y4MStream.ostreaminfo,
  //				   &Y4MStream.oframeinfo, outframe);

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
int vx;
uint32_t sad;
uint32_t min;
int dx;

for(y=0;y<h;y++)
for(x=0;x<w;x++)
{
	min=0x00ffffff;
	vx=0;
	for(dx=-2;dx<=2;dx++)
	{
	sad  = abs( *(out+(x+dx-3)+(y-1)*w) - *(out+(x-3)+(y+0)*w) );
	sad += abs( *(out+(x+dx-2)+(y-1)*w) - *(out+(x-2)+(y+0)*w) );
	sad += abs( *(out+(x+dx-1)+(y-1)*w) - *(out+(x-1)+(y+0)*w) );
	sad += abs( *(out+(x+dx+0)+(y-1)*w) - *(out+(x+0)+(y+0)*w) );
	sad += abs( *(out+(x+dx+1)+(y-1)*w) - *(out+(x+1)+(y+0)*w) );
	sad += abs( *(out+(x+dx+2)+(y-1)*w) - *(out+(x+2)+(y+0)*w) );
	sad += abs( *(out+(x+dx+3)+(y-1)*w) - *(out+(x+3)+(y+0)*w) );

	sad += abs( *(out+(x-dx-3)+(y+1)*w) - *(out+(x-3)+(y+0)*w) );
	sad += abs( *(out+(x-dx-2)+(y+1)*w) - *(out+(x-2)+(y+0)*w) );
	sad += abs( *(out+(x-dx-1)+(y+1)*w) - *(out+(x-1)+(y+0)*w) );
	sad += abs( *(out+(x-dx+0)+(y+1)*w) - *(out+(x+0)+(y+0)*w) );
	sad += abs( *(out+(x-dx+1)+(y+1)*w) - *(out+(x+1)+(y+0)*w) );
	sad += abs( *(out+(x-dx+2)+(y+1)*w) - *(out+(x+2)+(y+0)*w) );
	sad += abs( *(out+(x-dx+3)+(y+1)*w) - *(out+(x+3)+(y+0)*w) );

	if(sad<min)
		{
		min=sad;
		vx=dx;
		}
	}

	if(abs(vx)<=1)
		*(scratch+x+y*w)=(*(out+(x)+(y)*w)+*(out+(x+vx)+(y-1)*w)+*(out+(x-vx)+(y+1)*w))/3;
	else
	if(abs(vx)<=3)
		*(scratch+x+y*w)=
			(
			2 * *(out+(x-1)+(y)*w) + 
			4 * *(out+(x+0)+(y)*w) + 
			2 * *(out+(x+1)+(y)*w) + 
			1 * *(out+(x+vx-1)+(y-1)*w) +
			2 * *(out+(x+vx+0)+(y-1)*w) +
			1 * *(out+(x+vx+1)+(y-1)*w) +
 			1 * *(out+(x-vx-1)+(y+1)*w) +
 			2 * *(out+(x-vx+0)+(y+1)*w) +
 			1 * *(out+(x-vx+1)+(y+1)*w)  
			)/16;
	else
		*(scratch+x+y*w)=
			(
			2 * *(out+(x-1)+(y)*w) + 
			4 * *(out+(x-1)+(y)*w) + 
			8 * *(out+(x+0)+(y)*w) + 
			4 * *(out+(x+1)+(y)*w) + 
			2 * *(out+(x+1)+(y)*w) +

			1 * *(out+(x+vx-1)+(y-1)*w) + 
			2 * *(out+(x+vx-1)+(y-1)*w) + 
			4 * *(out+(x+vx+0)+(y-1)*w) + 
			2 * *(out+(x+vx+1)+(y-1)*w) + 
			1 * *(out+(x+vx+1)+(y-1)*w) +

			1 * *(out+(x-vx-1)+(y+1)*w) + 
			2 * *(out+(x-vx-1)+(y+1)*w) + 
			4 * *(out+(x-vx+0)+(y+1)*w) + 
			2 * *(out+(x-vx+1)+(y+1)*w) + 
			1 * *(out+(x-vx+1)+(y+1)*w) 

			)/40;

}	

for(y=2;y<(h-2);y++)
for(x=2;x<(w-2);x++)
{
	*(out+(x)+(y)*w) = 
		(
		*(out+(x)+(y)*w)+
		*(scratch+(x)+(y+0)*w)
		)/2;
}

}

void antialias_frame ()
{				
	antialias_plane ( inframe[0], width, height );
	antialias_plane ( inframe[1], cwidth, cheight );
	antialias_plane ( inframe[2], cwidth, cheight );

	  y4m_write_frame (Y4MStream.fd_out, &Y4MStream.ostreaminfo,
				   &Y4MStream.oframeinfo, inframe);
}

void deinterlace_ELA ()
{
int x,y;
int min,d,m,n;
int vx;
int i,j;
int c;

for(y=0;y<height;y+=2)
for(x=0;x<width;x++)
	{

	min=25600;
	i = *(inframe[0]+(x)+(y-1)*width) + *(inframe[0]+(x)+(y+1)*width);
	i /= 2;
	j = i;

	for(vx=0;vx<=3;vx++)
	{

		n = *(inframe[0]+(x-vx)+(y-1)*width) + *(inframe[0]+(x+vx)+(y+1)*width);
		n /= 2;

		d  = abs ( 	*(inframe[0]+(x-vx  )+(y-1)*width) -
				*(inframe[0]+(x+vx  )+(y+1)*width) );
		d += abs ( 	*(inframe[0]+(x-vx-1)+(y-1)*width) -
				*(inframe[0]+(x+vx-1)+(y+1)*width) );
		d += abs ( 	*(inframe[0]+(x-vx+1)+(y-1)*width) -
				*(inframe[0]+(x+vx+1)+(y+1)*width) );
		d += abs ( 	*(inframe[0]+(x-vx-2)+(y-1)*width) -
				*(inframe[0]+(x+vx-2)+(y+1)*width) );
		d += abs ( 	*(inframe[0]+(x-vx+2)+(y-1)*width) -
				*(inframe[0]+(x+vx+2)+(y+1)*width) );
		d += abs ( n - j )*3;

		if(d<min)
		{
		min=d;
		i = n;
		}

		n = *(inframe[0]+(x+vx)+(y-1)*width) + *(inframe[0]+(x-vx)+(y+1)*width);
		n /= 2;
		d = abs ( 	*(inframe[0]+(x+vx  )+(y-1)*width) -
				*(inframe[0]+(x-vx  )+(y+1)*width) );
		d += abs ( 	*(inframe[0]+(x+vx-1)+(y-1)*width) -
				*(inframe[0]+(x-vx-1)+(y+1)*width) );
		d += abs ( 	*(inframe[0]+(x+vx+1)+(y-1)*width) -
				*(inframe[0]+(x-vx+1)+(y+1)*width) );
		d += abs ( 	*(inframe[0]+(x+vx-2)+(y-1)*width) -
				*(inframe[0]+(x-vx-2)+(y+1)*width) );
		d += abs ( 	*(inframe[0]+(x+vx+2)+(y-1)*width) -
				*(inframe[0]+(x-vx+2)+(y+1)*width) );
		d += abs ( n - j )*3;

		if(d<min)
		{
		min=d;
		i=n;
		}
	}

	*(outframe[0]+x+(y-1)*width)=*(inframe[0]+x+(y-1)*width);
	*(outframe[0]+x+y*width)=i;
	}

  memcpy (outframe[1],inframe[1],cwidth*cheight);
  memcpy (outframe[2],inframe[2],cwidth*cheight);

//  y4m_write_frame (Y4MStream.fd_out, &Y4MStream.ostreaminfo,
//				   &Y4MStream.oframeinfo, outframe);

for(y=1;y<height;y+=2)
for(x=0;x<width;x++)
	{

	min=25600;
	i = *(inframe[0]+(x)+(y-1)*width) + *(inframe[0]+(x)+(y+1)*width);
	i /= 2;
	j = i;

	for(vx=0;vx<=3;vx++)
	{

		n = *(inframe[0]+(x-vx)+(y-1)*width) + *(inframe[0]+(x+vx)+(y+1)*width);
		n /= 2;

		d  = abs ( 	*(inframe[0]+(x-vx  )+(y-1)*width) -
				*(inframe[0]+(x+vx  )+(y+1)*width) );
		d += abs ( 	*(inframe[0]+(x-vx-1)+(y-1)*width) -
				*(inframe[0]+(x+vx-1)+(y+1)*width) );
		d += abs ( 	*(inframe[0]+(x-vx+1)+(y-1)*width) -
				*(inframe[0]+(x+vx+1)+(y+1)*width) );
		d += abs ( 	*(inframe[0]+(x-vx-2)+(y-1)*width) -
				*(inframe[0]+(x+vx-2)+(y+1)*width) );
		d += abs ( 	*(inframe[0]+(x-vx+2)+(y-1)*width) -
				*(inframe[0]+(x+vx+2)+(y+1)*width) );
		d += abs ( n - j )*3;

		if(d<min)
		{
		min=d;
		i = n;
		}

		n = *(inframe[0]+(x+vx)+(y-1)*width) + *(inframe[0]+(x-vx)+(y+1)*width);
		n /= 2;
		d = abs ( 	*(inframe[0]+(x+vx  )+(y-1)*width) -
				*(inframe[0]+(x-vx  )+(y+1)*width) );
		d += abs ( 	*(inframe[0]+(x+vx-1)+(y-1)*width) -
				*(inframe[0]+(x-vx-1)+(y+1)*width) );
		d += abs ( 	*(inframe[0]+(x+vx+1)+(y-1)*width) -
				*(inframe[0]+(x-vx+1)+(y+1)*width) );
		d += abs ( 	*(inframe[0]+(x+vx-2)+(y-1)*width) -
				*(inframe[0]+(x-vx-2)+(y+1)*width) );
		d += abs ( 	*(inframe[0]+(x+vx+2)+(y-1)*width) -
				*(inframe[0]+(x-vx+2)+(y+1)*width) );
		d += abs ( n - j )*3;

		if(d<min)
		{
		min=d;
		i=n;
		}
	}

	*(outframe[0]+x+(y-1)*width)=(*(outframe[0]+x+(y-1)*width)+*(inframe[0]+x+(y-1)*width))/2;
	*(outframe[0]+x+y*width)=(*(outframe[0]+x+y*width)+i)/2;
	}

  memcpy (outframe[1],inframe[1],cwidth*cheight);
  memcpy (outframe[2],inframe[2],cwidth*cheight);

  y4m_write_frame (Y4MStream.fd_out, &Y4MStream.ostreaminfo,
				   &Y4MStream.oframeinfo, outframe);

  }
};

int
main (int argc, char *argv[])
{
  int frame = 0;
  int errno = 0;
  int ss_h, ss_v;

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
	    mjpeg_log (LOG_INFO, "    assume progressive but aliased input.");
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

#ifdef HAVE_ALTIVEC
  reset_motion_simd("sad_00");
#endif

  // initialize stream-information 
  y4m_accept_extensions (1);
  y4m_init_stream_info (&YUVdeint.Y4MStream.istreaminfo);
  y4m_init_frame_info (&YUVdeint.Y4MStream.iframeinfo);
  y4m_init_stream_info (&YUVdeint.Y4MStream.ostreaminfo);
  y4m_init_frame_info (&YUVdeint.Y4MStream.oframeinfo);

/* open input stream */
  if ((errno = y4m_read_stream_header (YUVdeint.Y4MStream.fd_in,
			       &YUVdeint.Y4MStream.istreaminfo)) != Y4M_OK)
    {
    mjpeg_error_exit1("Couldn't read YUV4MPEG header: %s!",y4m_strerr(errno));
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
  switch (YUVdeint.input_chroma_subsampling)
         {
	 case Y4M_CHROMA_420JPEG:
	 case Y4M_CHROMA_420MPEG2:
	 case Y4M_CHROMA_420PALDV:
	 case Y4M_CHROMA_444:
	 case Y4M_CHROMA_422:
	 case Y4M_CHROMA_411:
	      ss_h = y4m_chroma_ss_x_ratio(YUVdeint.input_chroma_subsampling).d;
	      ss_v = y4m_chroma_ss_y_ratio(YUVdeint.input_chroma_subsampling).d;
	      YUVdeint.cwidth = YUVdeint.width / ss_h;
	      YUVdeint.cheight = YUVdeint.height / ss_v;
	      break;
	 default:
              mjpeg_error_exit1( "%s is not in supported chroma-format. Sorry.",
	          y4m_chroma_keyword(YUVdeint.input_chroma_subsampling));
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
		     "Unable to determine field-order from input-stream.");
	  mjpeg_log (LOG_ERROR,
		     "This is most likely the case when using mplayer to produce the input-stream.");
	  mjpeg_log (LOG_ERROR,
		     "Either the stream is misflagged or progressive...");
	  mjpeg_log (LOG_ERROR,
		     "I will stop here, sorry. Please choose a field-order");
	  mjpeg_log (LOG_ERROR,
		     "with -s0 or -s1. Otherwise I can't do anything for you. TERMINATED. Thanks...");
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

//YUVdeint.deinterlace_ELA ();
	  if (!YUVdeint.just_anti_alias)
		  YUVdeint.deinterlace_motion_compensated ();
	  else
		  YUVdeint.antialias_frame ();
	frame++;
    }
  return 0;
}
