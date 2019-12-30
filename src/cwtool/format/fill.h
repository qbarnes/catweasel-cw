/****************************************************************************
 ****************************************************************************
 *
 * format/fill.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_FILL_H
#define CWTOOL_FILL_H

struct fill
	{
	struct
		{
		unsigned char		mode;
		unsigned char		fill_value;
		unsigned char		reserved[2];
		unsigned int		fill_length;
		}			wr;
	};

/*
 * do not put this include at the beginning of the file, because ../format.h
 * also includes this file to construct union format. so the above struct
 * has to be known at time of inclusion
 */

#include "../format.h"

extern struct format_desc		fill_format_desc;



#endif /* !CWTOOL_FILL_H */
/******************************************************** Karsten Scheibler */
