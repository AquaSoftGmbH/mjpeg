#include "buffer.hh"
#include <stdlib.h>

/******************************************************************
cleaned

	Remove entries from FIFO buffer list, if their DTS is 
	less than actual SCR. These packet data have been already
	decoded and have been removed from the system target
	decoder's elementary stream buffer.
******************************************************************/

void BufferModel::cleaned(clockticks SCR)
{
    BufferQueue *pointer;

    while ((first != NULL) && first->DTS < SCR)
    {
	  pointer = first;
	  first = first->next;
	  delete pointer;	
    }
}

/******************************************************************
	BufferModel::flush

	Remove all entries from FIFO buffer list, if their DTS is 
	less than actual SCR. These packet data have been already
	decoded and have been removed from the system target
	decoder's elementary stream buffer.
******************************************************************/

void BufferModel::flushed ()
{
    BufferQueue *pointer;

    while (first != NULL)
    {
	  pointer = first;
	  first = first->next;
	  delete pointer;	
    }
}

/******************************************************************
	BufferModel::Space

	returns free space in the buffer
******************************************************************/

unsigned int BufferModel::space ()
{
    unsigned int used_bytes;
    BufferQueue *pointer;

    pointer=first;
    used_bytes=0;

    while (pointer != NULL)
    {
		used_bytes += pointer->size;
		pointer = pointer->next;
    }

    return (max_size - used_bytes);

}

/******************************************************************
	Queue_Buffer

	adds entry into the buffer FIFO queue
******************************************************************/

void BufferModel::queued (unsigned int bytes,
					clockticks TS)
{
    BufferQueue *pointer;

    pointer=first;
    if (pointer==NULL)
    {
		first = new BufferQueue;
		first->size = bytes;
		first->next=NULL;
		first->DTS = TS;
    } 
	else
    {
		while ((pointer->next)!=NULL)
		{
			pointer = pointer->next;
		}

		pointer->next = (BufferQueue*) malloc (sizeof (BufferQueue));
		pointer->next->size = bytes;
		pointer->next->next = NULL;
		pointer->next->DTS = TS;
    }
}


void BufferModel::init ( unsigned int size)
{
    max_size = size;
    first = 0;
}
