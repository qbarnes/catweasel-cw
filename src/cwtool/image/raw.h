/****************************************************************************
 ****************************************************************************
 *
 * image/raw.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_IMAGE_RAW_H
#define CWTOOL_IMAGE_RAW_H

#include "file.h"
#include "cwtool.h"

/*
 * if CWTOOL_MAX_RETRIES is 10 this effectively means we do one read and up
 * to 10 retries, so we need up to CWTOOL_MAX_RETRIES + 1 entries
 */

#define IMAGE_RAW_MAX_HINT		((CWTOOL_MAX_RETRIES + 1) * CWTOOL_MAX_TRACK)

struct image_raw_hint
	{
	unsigned char			file;
	unsigned char			track;
	unsigned char			clock;
	unsigned char			flags;
	int				offset;
	};

struct image_raw
	{
	struct file			fil[2];
	struct cw_floppyinfo		fli;
	struct image_raw_hint		hnt[IMAGE_RAW_MAX_HINT];
	int				hints;
	int				type;
	int				flags;
	int				track_flags[CWTOOL_MAX_TRACK];
	};



#endif /* !CWTOOL_IMAGE_RAW_H */
/******************************************************** Karsten Scheibler */
