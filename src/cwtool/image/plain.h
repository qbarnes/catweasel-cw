/****************************************************************************
 ****************************************************************************
 *
 * image/plain.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_IMAGE_PLAIN_H
#define CWTOOL_IMAGE_PLAIN_H

#include "../file.h"

struct image_plain
	{
	struct file			fil;
	int				offset;
	int				flags;
	};



#endif /* !CWTOOL_IMAGE_PLAIN_H */
/******************************************************** Karsten Scheibler */