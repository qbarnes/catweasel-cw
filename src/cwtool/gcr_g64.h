/****************************************************************************
 ****************************************************************************
 *
 * gcr_g64.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_GCR_G64_H
#define CWTOOL_GCR_G64_H

#include "bounds.h"

struct gcr_g64
	{
	struct
		{
		unsigned char		flags;
		unsigned char		sync_length;
		unsigned char		pad_length1;
		unsigned char		pad_value1;
		unsigned char		pad_length2;
		unsigned char		pad_value2;
		unsigned char		header_raw_id;
		unsigned char		speed;
		}			rd;
	struct
		{
		unsigned short		prolog_length;
		unsigned short		epilog_length;
		unsigned char		prolog_value;
		unsigned char		epilog_value;
		unsigned char		flags;
		unsigned char		reserved;
		short			precomp[4][9];
		}			wr;
	struct
		{
		struct bounds		bnd[4][3];
		}			rw;
	};



#endif /* !CWTOOL_GCR_G64_H */
/******************************************************** Karsten Scheibler */
