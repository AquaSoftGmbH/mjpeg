/*
  Synchrolib - various useful synchronisation primitives

  (C) 2001 Andrew Stevens

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.


   Damn: shouldn't have to write this - but whenever you need
   something, well *that's* when you don't have Internet access.

   Bloody annyong that pthread_mutex's lock/unlock is only supposed to
   work properly if the same thread does the locking and unlocking. Gaah!
 */

#include <stdio.h>
#include "synchrolib.h"

/*********
 *
 *		  Synchronisation primitives
 *
 ********/

void sync_guard_init( sync_guard_t *guard, int init )
{
	pthread_mutex_init( &guard->mutex, NULL );
	pthread_cond_init( &guard->cond, NULL );
	guard->predicate = init;
}

void sync_guard_test( sync_guard_t *guard)
{
	pthread_mutex_lock( &guard->mutex );
	while( !guard->predicate )
	{
		pthread_cond_wait( &guard->cond, &guard->mutex );
	}
	pthread_mutex_unlock( &guard->mutex );
}

void sync_guard_update( sync_guard_t *guard, int predicate )
{
	pthread_mutex_lock( &guard->mutex );
	guard->predicate = predicate;
	pthread_cond_broadcast( &guard->cond );
	pthread_mutex_unlock( &guard->mutex );
}


void mp_semaphore_init( semaphore_t *sema, int init_count )
{
	pthread_mutex_init( &sema->mutex, NULL );
	pthread_cond_init( &sema->raised, NULL );
	sema->count = init_count;
}

void mp_semaphore_wait( semaphore_t *sema)
{
	pthread_mutex_lock( &sema->mutex );
	while( sema->count == 0 )
	{
		pthread_cond_wait( &sema->raised, &sema->mutex );
	}
	--(sema->count);
	pthread_mutex_unlock( &sema->mutex );
}

void mp_semaphore_signal( semaphore_t *sema, int count )
{
	pthread_mutex_lock( &sema->mutex );
	sema->count += count;
	pthread_cond_broadcast( &sema->raised );
	pthread_mutex_unlock( &sema->mutex );
}

void mp_semaphore_set( semaphore_t *sema )
{
	pthread_mutex_lock( &sema->mutex );
	sema->count = 1;
	pthread_cond_broadcast( &sema->raised );
	pthread_mutex_unlock( &sema->mutex );
}
