/****************************************************************************
 ****************************************************************************
 *
 * format/gcr_apple.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_GCR_APPLE_H
#define CWTOOL_GCR_APPLE_H

#include "bounds.h"

struct gcr_apple
	{
	struct
		{
		unsigned char		flags;
		unsigned char		reserved[3];
		}			rd;
	struct
		{
		unsigned short		prolog_length;
		unsigned short		epilog_length;
		unsigned short		fill_value1;
		unsigned short		fill_value2;
		unsigned char		fill_length1;
		unsigned char		fill_length2;
		short			precomp[9];
		}			wr;
	struct
		{
		unsigned char		sectors;
		unsigned char		volume_id;
		unsigned char		mode;
		unsigned char		track_step;
		unsigned int		sync_value1;
		unsigned int		sync_value2;
		struct bounds		bnd[3];
		}			rw;
	};

/*
 * do not put this include at the beginning of the file, because ../format.h
 * also includes this file to construct union format. so the above struct
 * has to be known at time of inclusion
 */

#include "../format.h"

extern struct format_desc		gcr_apple_format_desc;



#endif /* !CWTOOL_GCR_APPLE_H */
/******************************************************** Karsten Scheibler */
