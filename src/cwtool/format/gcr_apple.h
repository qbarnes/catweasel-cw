/****************************************************************************
 ****************************************************************************
 *
 * format/gcr_apple.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_GCR_APPLE_H
#define CWTOOL_GCR_APPLE_H

#include "format/bounds.h"

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
		unsigned char		reserved[2];
		unsigned int		sync_value1;
		unsigned int		sync_value2;
		struct bounds		bnd[3];
		}			rw;
	};



#endif /* !CWTOOL_GCR_APPLE_H */
/******************************************************** Karsten Scheibler */
