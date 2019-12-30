/****************************************************************************
 ****************************************************************************
 *
 * image/d64.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_IMAGE_D64_H
#define CWTOOL_IMAGE_D64_H

#include "../file.h"
#include "../cwtool.h"

struct image_d64
	{
	struct file			fil;
	int				offset;
	int				flags;
	int				sectors[CWTOOL_MAX_TRACK];
	unsigned char			errors[CWTOOL_MAX_TRACK][CWTOOL_MAX_SECTOR];
	};



#endif /* !CWTOOL_IMAGE_D64_H */
/******************************************************** Karsten Scheibler */
