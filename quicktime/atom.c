#include <ctype.h>
#include "quicktime.h"





int quicktime_atom_reset(quicktime_atom_t *atom)
{
	atom->end = 0;
	atom->type[0] = atom->type[1] = atom->type[2] = atom->type[3] = 0;
	return 0;
}

int quicktime_atom_read_header(quicktime_t *file, quicktime_atom_t *atom)
{
	char header[10];
	int result;
	long size2;

	atom->start = quicktime_position(file);

	quicktime_atom_reset(atom);

	if(!quicktime_read_data(file, header, HEADER_LENGTH)) return 1;
	result = quicktime_atom_read_type(header, atom->type);
	atom->size = quicktime_atom_read_size(header);
	atom->end = atom->start + atom->size;

/* Skip placeholder atom */
	if(quicktime_match_32(atom->type, "wide"))
	{
		atom->start = quicktime_position(file);
		quicktime_atom_reset(atom);
		if(!quicktime_read_data(file, header, HEADER_LENGTH)) return 1;
		result = quicktime_atom_read_type(header, atom->type);
		atom->size -= 8;
		if(!atom->size)
		{
/* Wrapper ended.  Get new atom size */
			atom->size = quicktime_atom_read_size(header);
		}
		atom->end = atom->start + atom->size;
	}
	else
/* Get extended size */
	if(atom->size == 1)
	{
		if(!quicktime_read_data(file, header, HEADER_LENGTH)) return 1;
		atom->size = quicktime_atom_read_size64(header);
	}

/*printf("%c%c%c%c\n", atom->type[0], atom->type[1], atom->type[2], atom->type[3]); */
	return result;
}

int quicktime_atom_write_header(quicktime_t *file, quicktime_atom_t *atom, char *text)
{
	atom->start = quicktime_position(file);
	quicktime_write_int32(file, 0);
	quicktime_write_char32(file, text);
}

int quicktime_atom_write_footer(quicktime_t *file, quicktime_atom_t *atom)
{
	atom->end = quicktime_position(file);
	quicktime_set_position(file, atom->start);
	quicktime_write_int32(file, atom->end - atom->start);
	quicktime_set_position(file, atom->end);
}

int quicktime_atom_is(quicktime_atom_t *atom, char *type)
{
	if(atom->type[0] == type[0] &&
		atom->type[1] == type[1] &&
		atom->type[2] == type[2] &&
		atom->type[3] == type[3])
	return 1;
	else
	return 0;
}

long quicktime_atom_read_size(char *data)
{
	unsigned long result;
	unsigned long a, b, c, d;
	
	a = (unsigned char)data[0];
	b = (unsigned char)data[1];
	c = (unsigned char)data[2];
	d = (unsigned char)data[3];

	result = (a<<24) | (b<<16) | (c<<8) | d;
	if(result < HEADER_LENGTH) result = HEADER_LENGTH;
	return (long)result;
}

long quicktime_atom_read_size64(char *data)
{
	unsigned long result;
	unsigned long a, b, c, d, e, f, g, h;
	
	a = (unsigned char)data[0];
	b = (unsigned char)data[1];
	c = (unsigned char)data[2];
	d = (unsigned char)data[3];
	e = (unsigned char)data[0];
	f = (unsigned char)data[1];
	g = (unsigned char)data[2];
	h = (unsigned char)data[3];

	result = (a<<56) | (b<<48) | (c<<40) | (d<<32) | (e<<24) | (f<<16) | (g<<8) | h;
	if(result < HEADER_LENGTH) result = HEADER_LENGTH;
	return (long)result;
}

int quicktime_atom_read_type(char *data, char *type)
{
	type[0] = data[4];
	type[1] = data[5];
	type[2] = data[6];
	type[3] = data[7];

/*printf("%c%c%c%c ", type[0], type[1], type[2], type[3]); */
/* need this for quicktime_check_sig */
	if(isalpha(type[0]) && isalpha(type[1]) && isalpha(type[2]) && isalpha(type[3]))
	return 0;
	else
	return 1;
}

int quicktime_atom_skip(quicktime_t *file, quicktime_atom_t *atom)
{
	return quicktime_set_position(file, atom->end);
/*	return fseek(file->stream, atom->end, SEEK_SET); */
}
