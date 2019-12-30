/****************************************************************************
 ****************************************************************************
 *
 * image_raw.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>

#include "error.h"
#include "debug.h"
#include "verbose.h"
#include "cwtool.h"
#include "fifo.h"
#include "file.h"
#include "image.h"
#include "import.h"
#include "export.h"



#define HEADER_FLAG_WRITABLE		(1 << 0)
#define HEADER_FLAG_INDEXED		(1 << 1)

struct track_header
	{
	unsigned char			magic;
	unsigned char			track;
	unsigned char			clock;
	unsigned char			flags;
	unsigned char			size[4];
	};



/****************************************************************************
 * image_raw_ioctl
 ****************************************************************************/
static int
image_raw_ioctl(
	struct image			*img,
	struct image_track		*img_trk,
	int				timeout,
	int				track,
	int				cmd,
	int				mode,
	unsigned char			*data,
	int				size)

	{
	struct cw_trackinfo		tri = CW_TRACKINFO_INIT;
	int				flip = 0;

	if (img_trk->flags & IMAGE_TRACK_FLAG_FLIP_SIDE) flip = 1;
	tri.clock   = img_trk->clock;
	tri.timeout = timeout;
	tri.track   = track / 2;
	tri.side    = (track & 1) ^ flip;
	tri.mode    = mode;
	tri.data    = data;
	tri.size    = size;
	if (img_trk->side_offset != 0)
		{
		tri.track = track;
		tri.side  = (track >= img_trk->side_offset) ? 1 : 0;
		if (tri.side) tri.track -= img_trk->side_offset;
		tri.side ^= flip;
		}
	verbose(1, "accessing raw track %d (hardware track %d side %d) with timeout %d ms on '%s'", track, tri.track, tri.side, tri.timeout, file_get_path(&img->fil));
	if (tri.clock >= img->fli.clock_max) error("error while accessing track %d, clock is not supported by device '%s'", track, file_get_path(&img->fil));
	if (tri.track >= img->fli.track_max) error("error while accessing track %d, track is not supported by device '%s'", track, file_get_path(&img->fil));
	if (tri.side  >= img->fli.side_max)  error("error while accessing track %d, side is not supported by device '%s'", track, file_get_path(&img->fil));
	if (tri.mode  >= img->fli.mode_max)  error("error while accessing track %d, mode is not supported by device '%s'", track, file_get_path(&img->fil));
	return (file_ioctl(&img->fil, cmd, &tri, 0));
	}



/****************************************************************************
 * image_raw_open
 ****************************************************************************/
static int
image_raw_open(
	struct image			*img,
	const char			*path,
	int				mode)

	{
	static const char		magic[32] = "cwtool raw data";
	char				buffer[32], *type_name;
	int				i;

	file_open(&img->fil, path, image_init(img, mode));

	/* check if we have a regular file or a catweasel device */

	type_name = "device";
	img->type = IMAGE_TYPE_DEVICE;
	img->fli  = CW_FLOPPYINFO_INIT;
	if (file_ioctl(&img->fil, CW_IOC_GFLPARM, &img->fli, 1) == 0) goto end;

	/* img->fil belongs to a regular file, check file header */

	if (file_get_mode(&img->fil) == FILE_MODE_READ)
		{
		file_read_strict(&img->fil, (unsigned char *) buffer, sizeof (buffer));
		for (i = 0; i < sizeof (magic); i++) if (magic[i] != buffer[i]) error("file '%s' has wrong magic", file_get_path(&img->fil));
		}
	else file_write(&img->fil, (unsigned char *) magic, sizeof (magic));
	type_name = "file";
	img->type = IMAGE_TYPE_REGULAR;
end:
	verbose(1, "assuming '%s' is a %s", file_get_path(&img->fil), type_name);
	return (1);
	}



/****************************************************************************
 * image_raw_close
 ****************************************************************************/
static int
image_raw_close(
	struct image			*img)

	{
	return (file_close(&img->fil));
	}



/****************************************************************************
 * image_raw_type
 ****************************************************************************/
static int
image_raw_type(
	struct image			*img)

	{
	return (img->type);
	}



/****************************************************************************
 * image_raw_offset
 ****************************************************************************/
static int
image_raw_offset(
	struct image			*img)

	{
	return (0);
	}



/****************************************************************************
 * image_raw_read2
 ****************************************************************************/
static char *
image_raw_read2(
	struct image			*img,
	struct fifo			*ffo,
	struct image_track		*img_trk,
	struct track_header		*trk_hdr,
	int				track)

	{
	int				size = sizeof (struct track_header);

	if (file_read(&img->fil, (unsigned char *) trk_hdr, size) != size) return ("header truncated");
	if (trk_hdr->magic != 0xca) return ("wrong header magic");
	size = import_ulong_le(trk_hdr->size);
	if (size > fifo_get_limit(ffo)) return ("track too large");
	if (file_read(&img->fil, fifo_get_data(ffo), size) != size) return ("track truncated");
	if (trk_hdr->track != track) goto end;
	if (trk_hdr->flags & HEADER_FLAG_WRITABLE) fifo_set_flags(ffo, FIFO_FLAG_WRITABLE);
	if (trk_hdr->flags & HEADER_FLAG_INDEXED) fifo_set_flags(ffo, FIFO_FLAG_INDEXED);
	if (trk_hdr->clock != img_trk->clock) return ("wrong clock");
end:
	return (NULL);
	}



/****************************************************************************
 * image_raw_read
 ****************************************************************************/
static int
image_raw_read(
	struct image			*img,
	struct fifo			*ffo,
	struct image_track		*img_trk,
	int				track)

	{
	int				size = fifo_get_limit(ffo);
	int				mode = CW_TRACKINFO_MODE_INDEX_STORE;

	debug_error_condition(file_get_mode(&img->fil) != FILE_MODE_READ);
	debug_error_condition((img->type != IMAGE_TYPE_REGULAR) && (img->type != IMAGE_TYPE_DEVICE));
	if (img->type == IMAGE_TYPE_DEVICE)
		{
		if (img_trk->flags & IMAGE_TRACK_FLAG_INDEXED_READ) mode = CW_TRACKINFO_MODE_INDEX_WAIT;
		else fifo_set_flags(ffo, FIFO_FLAG_INDEXED);
		size = image_raw_ioctl(img, img_trk, img_trk->timeout_read, track,
			CW_IOC_READ, mode, fifo_get_data(ffo), size);
		}
	else 
		{
		struct track_header	trk_hdr;
		char			*msg;

		/*
		 * read as long as the wanted track appears, so it is
		 * possible to have raw data of the complete disk but reading
		 * only some tracks of it
		 */

		do
			{
			verbose(1, "trying to read raw track %d from '%s'", track, file_get_path(&img->fil));
			msg = image_raw_read2(img, ffo, img_trk, &trk_hdr, track);
			if (msg != NULL) error("could not read track %d from file '%s', %s", track, file_get_path(&img->fil), msg);
			size = import_ulong_le(trk_hdr.size);
			verbose(1, "got raw track %d with %d bytes", trk_hdr.track, size);
			}
		while (trk_hdr.track != track);
		}
	if (size < CWTOOL_MIN_TRACK_SIZE) error_warning("got only %d bytes while reading track %d", size, track);
	fifo_set_wr_ofs(ffo, size);
	return (1);
	}



/****************************************************************************
 * image_raw_write_prepare
 ****************************************************************************/
static void
image_raw_write_prepare(
	struct fifo			*ffo,
	int				track)

	{
	unsigned char			*data = fifo_get_data(ffo);
	int				size  = fifo_get_wr_ofs(ffo);
	int				mask  = 0xff;
	int				d, i, j;

	if (! (fifo_get_flags(ffo) & FIFO_FLAG_WRITABLE)) error_warning("data written on track %d may be corrupt", track);
	if (fifo_get_flags(ffo) & FIFO_FLAG_INDEXED) mask = 0x7f;
	for (i = j = 0; i < size; i++)
		{
		d = data[i] &= mask;
		if ((d > 0x02) && (d < 0x80)) continue;
		if (d < 0x03) d = 0x03;
		if (d > 0x7f) d = 0x7f;
		data[i] = d;
		j++;
		}
	if (j > 0) error_warning("had to modify catweasel counters %d times on track %d", j, track);
	}



/****************************************************************************
 * image_raw_write_flags
 ****************************************************************************/
static int
image_raw_write_flags(
	struct fifo			*ffo)

	{
	int				flags_in  = fifo_get_flags(ffo);
	int				flags_out = 0;

	if (flags_in & FIFO_FLAG_WRITABLE) flags_out |= HEADER_FLAG_WRITABLE;
	if (flags_in & FIFO_FLAG_INDEXED)  flags_out |= HEADER_FLAG_INDEXED;
	return (flags_out);
	}



/****************************************************************************
 * image_raw_write
 ****************************************************************************/
static int
image_raw_write(
	struct image			*img,
	struct fifo			*ffo,
	struct image_track		*img_trk,
	int				track)

	{
	int				size = fifo_get_wr_ofs(ffo);
	int				mode = CW_TRACKINFO_MODE_NORMAL;

	debug_error_condition(file_get_mode(&img->fil) != FILE_MODE_WRITE);
	debug_error_condition((img->type != IMAGE_TYPE_REGULAR) && (img->type != IMAGE_TYPE_DEVICE));
	if (size < CWTOOL_MIN_TRACK_SIZE) error_warning("got only %d bytes for writing track %d", size, track);
	if (img->type == IMAGE_TYPE_DEVICE)
		{
		if (img_trk->flags & IMAGE_TRACK_FLAG_INDEXED_WRITE) mode = CW_TRACKINFO_MODE_INDEX_WAIT;
		image_raw_write_prepare(ffo, track);
		size = image_raw_ioctl(img, img_trk, img_trk->timeout_write, track,
			CW_IOC_WRITE, mode, fifo_get_data(ffo), size);
		if (size < fifo_get_wr_ofs(ffo)) error_warning("could not write full track %d, write timed out", track);
		}
	else 
		{
		struct track_header	trk_hdr =
			{
			.magic = 0xca,
			.track = track,
			.clock = img_trk->clock,
			.flags = image_raw_write_flags(ffo)
			};

		export_ulong_le(trk_hdr.size, size);
		verbose(1, "writing raw track %d with %d bytes to '%s'", track, size, file_get_path(&img->fil));
		file_write(&img->fil, (unsigned char *) &trk_hdr, sizeof (trk_hdr));
		file_write(&img->fil, fifo_get_data(ffo), size);
		}
	fifo_set_rd_ofs(ffo, size);
	return (1);
	}



/****************************************************************************
 * image_raw_desc
 ****************************************************************************/
struct image_desc			image_raw_desc =
	{
	.name        = "raw",
	.level       = 0,
	.open        = image_raw_open,
	.close       = image_raw_close,
	.type        = image_raw_type,
	.offset      = image_raw_offset,
	.track_read  = image_raw_read,
	.track_write = image_raw_write
	};
/******************************************************** Karsten Scheibler */
