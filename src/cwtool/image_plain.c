/****************************************************************************
 ****************************************************************************
 *
 * image_plain.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>

#include "error.h"
#include "debug.h"
#include "verbose.h"
#include "fifo.h"
#include "file.h"
#include "image.h"



/****************************************************************************
 * image_plain_open
 ****************************************************************************/
static int
image_plain_open(
	struct image			*img,
	const char			*path,
	int				mode)

	{
	file_open(&img->fil, path, image_init(img, mode));
	return (1);
	}



/****************************************************************************
 * image_plain_close
 ****************************************************************************/
static int
image_plain_close(
	struct image			*img)

	{
	unsigned char			buffer[1];

	if (file_get_mode(&img->fil) == FILE_MODE_WRITE) goto end;
	if ((img->end_seen) || (img->ignore_size)) goto end;
	if (file_read(&img->fil, buffer, 1) == 0) goto end;
	error_warning("file '%s' is larger than needed", file_get_path(&img->fil));
end:
	return (file_close(&img->fil));
	}



/****************************************************************************
 * image_plain_type
 ****************************************************************************/
static int
image_plain_type(
	struct image			*img)

	{
	debug_error();
	return (0);
	}



/****************************************************************************
 * image_plain_offset
 ****************************************************************************/
static int
image_plain_offset(
	struct image			*img)

	{
	return (img->offset);
	}



/****************************************************************************
 * image_plain_read
 ****************************************************************************/
static int
image_plain_read(
	struct image			*img,
	struct fifo			*ffo,
	struct image_track		*img_trk,
	int				track)

	{
	int				size = fifo_get_limit(ffo);

	debug_error_condition(file_get_mode(&img->fil) != FILE_MODE_READ);
	fifo_set_wr_ofs(ffo, size);
	if (img->end_seen) goto end;
	verbose(1, "reading plain track %d with %d bytes from '%s'", track, size, file_get_path(&img->fil));
	size = file_read(&img->fil, fifo_get_data(ffo), size);
	img->offset += size;
	if (size == fifo_get_limit(ffo)) goto end;
	if (! img->ignore_size) error_warning("file '%s' is shorter than needed", file_get_path(&img->fil));
	img->end_seen = 1;
end:
	return (1);
	}



/****************************************************************************
 * image_plain_write
 ****************************************************************************/
static int
image_plain_write(
	struct image			*img,
	struct fifo			*ffo,
	struct image_track		*img_trk,
	int				track)

	{
	int				size = fifo_get_wr_ofs(ffo);

	debug_error_condition(file_get_mode(&img->fil) != FILE_MODE_WRITE);
	verbose(1, "writing plain track %d with %d bytes to '%s'", track, size, file_get_path(&img->fil));
	file_write(&img->fil, fifo_get_data(ffo), size);
	fifo_set_rd_ofs(ffo, size);
	img->offset += size;
	return (1);
	}




/****************************************************************************
 *
 * used by external callers
 *
 ****************************************************************************/




/****************************************************************************
 * image_plain_desc
 ****************************************************************************/
struct image_desc			image_plain_desc =
	{
	.name        = "plain",
	.level       = 3,
	.open        = image_plain_open,
	.close       = image_plain_close,
	.type        = image_plain_type,
	.offset      = image_plain_offset,
	.track_read  = image_plain_read,
	.track_write = image_plain_write
	};
/******************************************************** Karsten Scheibler */
