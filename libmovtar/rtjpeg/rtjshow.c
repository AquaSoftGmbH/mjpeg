/*
 * Copyright (C) 2001 Calle Lejdfors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Please send all bugreports to: Calle Lejdfors <calle@nsc.liu.se>
 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <assert.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <SDL/SDL.h>

#include "RTjpeg.h"
#include "format.h"

/*
 * This program displays an RTjpeg image using SDL.
 */ 

SDL_Surface *screen;


void print_help( void )
{
  printf( "usage: rtjshow <filename>\n" );
}

void sig_handler( int i ) 
{
  SDL_Quit(); /* abort */
}

int main( int argc, char** argv )
{
  unsigned int w, h;
  int tmp;
  FILE* f;
  struct stat st;
  struct rtj_header hdr;
  __u8 *source;
  __u8 *target;
  
  if ( argc != 2 ) {
    print_help();
    exit(0);
  }

  if ( stat( argv[1], &st ) != 0 ) {
    perror( "stat:" );
    exit(1);
  }
  
  f = fopen( argv[1] , "r" );
  
  if ( !rtj_read_header(&hdr,f) ) {
    fprintf( stderr, "This doesn't seem to be a RTJPEG header.\n" );
    return 1;
  }


  w = hdr.width;
  h = hdr.height;
  
  source = malloc( w*h*3 ); /* size of YUV picture info */
  
  assert( source != NULL ); 
  
  tmp = fread( source, 1, w*h*3, f );

  /*  assert( tmp == w*h*3  ); */
  
  fprintf( stderr, "Read %i bytes from file.\n", tmp );
  
  RTjpeg_init_decompress( hdr.tbls, w, h );
  target = malloc( w*h*3 );
  
  RTjpeg_decompressYUV420( source, target );
  
  
  if ( SDL_Init(SDL_INIT_VIDEO) < 0 ) {
    perror( "SDL_init:" );
    exit(1);
  }    
  
  /* setup signal handlers */
  signal(SIGINT,sig_handler);
  atexit( SDL_Quit );
  
  screen = SDL_SetVideoMode( w, h, 24, SDL_HWSURFACE );
  SDL_EventState(SDL_KEYDOWN, SDL_ENABLE);
  SDL_EventState(SDL_MOUSEMOTION, SDL_IGNORE);
  SDL_EventState(SDL_ACTIVEEVENT, SDL_IGNORE);

  assert( screen != NULL );
  
  if ( SDL_MUSTLOCK( screen ) ) {
    if ( SDL_LockSurface(screen) < 0 ) {
      assert(0);
      exit(1);
    }
  }
  
  /* copy image to screen and convert to RGB32 */
  RTjpeg_yuvrgb24( target, screen->pixels, w*3 );   
  
  if ( SDL_MUSTLOCK( screen ) ) {
    SDL_UnlockSurface(screen);
  }

  SDL_UpdateRect(screen, 0, 0, w, h);

  pause();
  
  return 0;
}
