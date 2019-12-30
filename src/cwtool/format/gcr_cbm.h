/****************************************************************************
 ****************************************************************************
 *
 * format/gcr_cbm.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_GCR_CBM_H
#define CWTOOL_GCR_CBM_H

#include "bounds.h"

struct gcr_cbm
	{
	struct
		{
		unsigned short		sync_length;
		unsigned char		flags;
		unsigned char		reserved;
		}			rd;
	struct
		{
		unsigned short		prolog_length;
		unsigned short		epilog_length;
		unsigned char		prolog_value;
		unsigned char		epilog_value;
		unsigned short		sync_length;
		unsigned char		fill_length;
		unsigned char		fill_value;
		short			precomp[9];
		}			wr;
	struct
		{
		unsigned char		sectors;
		unsigned char		header_id;
		unsigned char		data_id;
		unsigned char		track_step;
		struct bounds		bnd[3];
		}			rw;
	};

/*
 * do not put this include at the beginning of the file, because ../format.h
 * also includes this file to construct union format. so the above struct
 * has to be known at time of inclusion
 */

#include "../format.h"

extern struct format_desc		gcr_cbm_format_desc;



#endif /* !CWTOOL_GCR_CBM_H */
/******************************************************** Karsten Scheibler */
