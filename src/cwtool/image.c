/****************************************************************************
 ****************************************************************************
 *
 * image.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>

#include "image.h"
#include "error.h"
#include "debug.h"
#include "verbose.h"
#include "file.h"
#include "string.h"



extern struct image_desc		image_raw_desc;
extern struct image_desc		image_g64_desc;
extern struct image_desc		image_plain_desc;
static struct image_desc		*img_dsc[] =
	{
	&image_raw_desc,
	&image_g64_desc,
	&image_plain_desc,
	NULL
	};



/****************************************************************************
 * image_search_desc
 ****************************************************************************/
struct image_desc *
image_search_desc(
	const char			*name)

	{
	int				i;

	for (i = 0; img_dsc[i] != NULL; i++) if (string_equal(name, img_dsc[i]->name)) break;
	return (img_dsc[i]);
	}



/****************************************************************************
 * image_init
 ****************************************************************************/
int
image_init(
	struct image			*img,
	int				mode)

	{
	*img = (struct image) { };
	if (mode == IMAGE_MODE_READ_IGNORE_SIZE) mode = FILE_MODE_READ, img->ignore_size = 1;
	else if (mode == IMAGE_MODE_READ)        mode = FILE_MODE_READ;
	else if (mode == IMAGE_MODE_WRITE)       mode = FILE_MODE_WRITE;
	else debug_error();
	return (mode);
	}
/******************************************************** Karsten Scheibler */
