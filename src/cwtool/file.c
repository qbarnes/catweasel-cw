/****************************************************************************
 ****************************************************************************
 *
 * file.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include "file.h"
#include "error.h"
#include "debug.h"
#include "verbose.h"
#include "cwtool.h"
#include "fifo.h"
#include "ioctl.h"
#include "string.h"



#define HEADER_FLAG_WRITABLE		(1 << 0)
#define HEADER_FLAG_INDEXED		(1 << 1)

struct file_track_header
	{
	unsigned char			magic;
	unsigned char			track;
	unsigned char			clock;
	unsigned char			flags;
	unsigned char			size[4];
	};

static struct file_image		*fil_img[] =
	{
	&file_image_raw,
	&file_image_plain,
	NULL
	};




/****************************************************************************
 *
 * misc helper routines
 *
 ****************************************************************************/




/****************************************************************************
 * file_mode_read_size_check
 ****************************************************************************/
static int
file_mode_read_size_check(
	int				mode)

	{
	if (mode == FILE_MODE_READ) return (1);
	return (0);
	}



/****************************************************************************
 * file_mode_read_no_error
 ****************************************************************************/
static int
file_mode_read_no_error(
	int				mode)

	{
	if (mode == FILE_MODE_READ3) return (1);
	return (0);
	}



/****************************************************************************
 * file_mode_read
 ****************************************************************************/
static int
file_mode_read(
	int				mode)

	{
	if (mode == FILE_MODE_READ)  return (1);
	if (mode == FILE_MODE_READ2) return (1);
	if (mode == FILE_MODE_READ3) return (1);
	return (0);
	}



/****************************************************************************
 * file_mode_write
 ****************************************************************************/
static int
file_mode_write(
	int				mode)

	{
	if (mode == FILE_MODE_WRITE) return (1);
	return (0);
	}



/****************************************************************************
 * file_mode_valid
 ****************************************************************************/
#define file_mode_valid(mode)		((file_mode_read(mode)) || (file_mode_write(mode)))



/****************************************************************************
 * file_detect_type
 ****************************************************************************/
static int
file_detect_type(
	struct file			*fil)

	{
	static const char		magic[32] = "cwtool raw data";
	char				buffer[32], *type_name;
	int				i;

	/* check if we have a regular file or a catweasel device */

	debug_error_condition(! file_mode_valid(fil->mode));
	type_name = "device";
	fil->type = FILE_TYPE_DEVICE;
	fil->fli  = CW_FLOPPYINFO_INIT;
	if (ioctl(fil->fd, CW_IOC_GFLPARM, &fil->fli) == 0) goto end;

	/* fil->fd belongs to a regular file, check file header */

	if (file_mode_read(fil->mode))
		{
		if (file_read(fil, (unsigned char *) buffer, sizeof (buffer)) != sizeof (buffer)) error("file '%s' truncated", fil->path);
		for (i = 0; i < sizeof (magic); i++) if (magic[i] != buffer[i]) error("file '%s' has wrong magic", fil->path);
		}
	else file_write(fil, (unsigned char *) magic, sizeof (magic));
	type_name = "file";
	fil->type = FILE_TYPE_REGULAR;
end:
	verbose(1, "assuming '%s' is a %s", fil->path, type_name);
	return (1);
	}



/****************************************************************************
 * file_type
 ****************************************************************************/
static int
file_type(
	struct file			*fil)

	{
	return (fil->type);
	}



/****************************************************************************
 * file_ioctl
 ****************************************************************************/
static int
file_ioctl(
	struct file			*fil,
	struct file_track		*fil_trk,
	int				timeout,
	int				track,
	int				cmd,
	int				mode,
	unsigned char			*data,
	int				size)

	{
	struct cw_trackinfo		tri = CW_TRACKINFO_INIT;
	int				result, flip = 0;

	if (fil_trk->flags & FILE_TRACK_FLAG_FLIP_SIDE) flip = 1;
	tri.clock   = fil_trk->clock;
	tri.timeout = timeout;
	tri.track   = track / 2;
	tri.side    = (track & 1) ^ flip;
	tri.mode    = mode;
	tri.data    = data;
	tri.size    = size;
	if (fil_trk->side_offset != 0)
		{
		tri.track = track;
		tri.side  = (track >= fil_trk->side_offset) ? 1 : 0;
		if (tri.side) tri.track -= fil_trk->side_offset;
		tri.side ^= flip;
		}
	if (tri.clock >= fil->fli.clock_max) error("error while accessing track %d, clock is not supported by device '%s'", track, fil->path);
	if (tri.track >= fil->fli.track_max) error("error while accessing track %d, track is not supported by device '%s'", track, fil->path);
	if (tri.side  >= fil->fli.side_max)  error("error while accessing track %d, side is not supported by device '%s'", track, fil->path);
	if (tri.mode  >= fil->fli.mode_max)  error("error while accessing track %d, mode is not supported by device '%s'", track, fil->path);
	result = ioctl(fil->fd, cmd, &tri);
	if (result == -1) error_perror2("error while accessing track %d on device '%s'", track, fil->path);
	return (result);
	}




/****************************************************************************
 *
 * routines for raw image format
 *
 ****************************************************************************/




/****************************************************************************
 * file_open_raw
 ****************************************************************************/
static int
file_open_raw(
	struct file			*fil,
	const char			*path,
	int				mode)

	{
	file_open(fil, path, mode);
	file_detect_type(fil);
	return (1);
	}



/****************************************************************************
 * file_close_raw
 ****************************************************************************/
static int
file_close_raw(
	struct file			*fil)

	{
	return (file_close(fil));
	}



/****************************************************************************
 * file_type_raw
 ****************************************************************************/
static int
file_type_raw(
	struct file			*fil)

	{
	return (file_type(fil));
	}



/****************************************************************************
 * file_read_raw_size
 ****************************************************************************/
static int
file_read_raw_size(
	struct file_track_header	*fil_trk_hdr)

	{
	int				size = fil_trk_hdr->size[3];

	size = (size << 8) | fil_trk_hdr->size[2];
	size = (size << 8) | fil_trk_hdr->size[1];
	size = (size << 8) | fil_trk_hdr->size[0];
	return (size);
	}



/****************************************************************************
 * file_read_raw2
 ****************************************************************************/
static char *
file_read_raw2(
	struct file			*fil,
	struct fifo			*ffo,
	struct file_track		*fil_trk,
	struct file_track_header	*fil_trk_hdr,
	int				track)

	{
	int				size = sizeof (struct file_track_header);

	if (file_read(fil, (unsigned char *) fil_trk_hdr, size) != size) return ("header truncated");
	if (fil_trk_hdr->magic != 0xca) return ("wrong header magic");
	size = file_read_raw_size(fil_trk_hdr);
	if (size > fifo_get_limit(ffo)) return ("track too large");
	if (file_read(fil, fifo_get_data(ffo), size) != size) return ("track truncated");
	if (fil_trk_hdr->track != track) goto end;
	if (fil_trk_hdr->flags & HEADER_FLAG_WRITABLE) fifo_set_flags(ffo, FIFO_FLAG_WRITABLE);
	if (fil_trk_hdr->flags & HEADER_FLAG_INDEXED)  fifo_set_flags(ffo, FIFO_FLAG_INDEXED);
	if (fil_trk_hdr->clock != fil_trk->clock) return ("wrong clock");
end:
	return (NULL);
	}



/****************************************************************************
 * file_read_raw
 ****************************************************************************/
static int
file_read_raw(
	struct file			*fil,
	struct fifo			*ffo,
	struct file_track		*fil_trk,
	int				track)

	{
	int				size = fifo_get_limit(ffo);
	int				mode = CW_TRACKINFO_MODE_INDEX_STORE;

	debug_error_condition(! file_mode_read(fil->mode));
	debug_error_condition((fil->type != FILE_TYPE_REGULAR) && (fil->type != FILE_TYPE_DEVICE));
	if (fil->type == FILE_TYPE_DEVICE)
		{
		if (fil_trk->flags & FILE_TRACK_FLAG_INDEXED_READ) mode = CW_TRACKINFO_MODE_INDEX_WAIT;
		else fifo_set_flags(ffo, FIFO_FLAG_INDEXED);
		size = file_ioctl(fil, fil_trk, fil_trk->timeout_read, track,
			CW_IOC_READ, mode, fifo_get_data(ffo), size);
		}
	else 
		{
		struct file_track_header	fil_trk_hdr;
		char				*msg;

		/*
		 * read as long as the wanted track appears, so it is
		 * possible to have raw data of the complete disk but reading
		 * only some tracks of it
		 */

		do
			{
			msg = file_read_raw2(fil, ffo, fil_trk, &fil_trk_hdr, track);
			if (msg != NULL) error("could not read track %d from '%s', %s", track, fil->path, msg);
			}
		while (fil_trk_hdr.track != track);
		size = file_read_raw_size(&fil_trk_hdr);
		}
	if (size < CWTOOL_MIN_TRACK_SIZE) error_warning("got only %d bytes while reading track %d", size, track);
	fifo_set_wr_ofs(ffo, size);
	return (1);
	}



/****************************************************************************
 * file_write_raw_prepare
 ****************************************************************************/
static void
file_write_raw_prepare(
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
 * file_write_raw_flags
 ****************************************************************************/
static int
file_write_raw_flags(
	struct fifo			*ffo)

	{
	int				flags_in  = fifo_get_flags(ffo);
	int				flags_out = 0;

	if (flags_in & FIFO_FLAG_WRITABLE) flags_out |= HEADER_FLAG_WRITABLE;
	if (flags_in & FIFO_FLAG_INDEXED)  flags_out |= HEADER_FLAG_INDEXED;
	return (flags_out);
	}



/****************************************************************************
 * file_write_raw
 ****************************************************************************/
static int
file_write_raw(
	struct file			*fil,
	struct fifo			*ffo,
	struct file_track		*fil_trk,
	int				track)

	{
	int				size = fifo_get_wr_ofs(ffo);
	int				mode = CW_TRACKINFO_MODE_NORMAL;

	debug_error_condition(! file_mode_write(fil->mode));
	debug_error_condition((fil->type != FILE_TYPE_REGULAR) && (fil->type != FILE_TYPE_DEVICE));
	if (size < CWTOOL_MIN_TRACK_SIZE) error_warning("got only %d bytes for writing track %d", size, track);
	if (fil->type == FILE_TYPE_DEVICE)
		{
		if (fil_trk->flags & FILE_TRACK_FLAG_INDEXED_WRITE) mode = CW_TRACKINFO_MODE_INDEX_WAIT;
		file_write_raw_prepare(ffo, track);
		size = file_ioctl(fil, fil_trk, fil_trk->timeout_write, track,
			CW_IOC_WRITE, mode, fifo_get_data(ffo), size);
		if (size < fifo_get_wr_ofs(ffo)) error_warning("could not write full track %d, write timed out", track);
		}
	else 
		{
		struct file_track_header	fil_trk_hdr =
			{
			.magic = 0xca,
			.track = track,
			.clock = fil_trk->clock,
			.flags = file_write_raw_flags(ffo),
			.size  = { size & 0xff, (size >> 8) & 0xff, (size >> 16) & 0xff, (size >> 24) & 0xff }
			};

		file_write(fil, (unsigned char *) &fil_trk_hdr, sizeof (fil_trk_hdr));
		file_write(fil, fifo_get_data(ffo), size);
		}
	fifo_set_rd_ofs(ffo, size);
	return (1);
	}




/****************************************************************************
 *
 * routines for plain image format
 *
 ****************************************************************************/




/****************************************************************************
 * file_open_plain
 ****************************************************************************/
static int
file_open_plain(
	struct file			*fil,
	const char			*path,
	int				mode)

	{
	file_open(fil, path, mode);
	return (1);
	}



/****************************************************************************
 * file_close_plain
 ****************************************************************************/
static int
file_close_plain(
	struct file			*fil)

	{
	unsigned char			buffer[1];

	if (fil->end) goto end;
	if (! file_mode_read_size_check(fil->mode)) goto end;
	if (file_read(fil, buffer, 1) == 0) goto end;
	error_warning("file '%s' is larger than needed", fil->path);
end:
	return (file_close(fil));
	}



/****************************************************************************
 * file_type_plain
 ****************************************************************************/
static int
file_type_plain(
	struct file			*fil)

	{
	return (file_type(fil));
	}



/****************************************************************************
 * file_read_plain
 ****************************************************************************/
static int
file_read_plain(
	struct file			*fil,
	struct fifo			*ffo,
	struct file_track		*fil_trk,
	int				track)

	{
	int				size = fifo_get_limit(ffo);

	debug_error_condition(! file_mode_read(fil->mode));
	fifo_set_wr_ofs(ffo, size);
	if (fil->end) goto end;
	size = file_read(fil, fifo_get_data(ffo), size);
	if (size == fifo_get_limit(ffo)) goto end;
	if (file_mode_read_size_check(fil->mode)) error_warning("file '%s' is shorter than needed", fil->path);
	fil->end = 1;
end:
	return (1);
	}



/****************************************************************************
 * file_write_plain
 ****************************************************************************/
static int
file_write_plain(
	struct file			*fil,
	struct fifo			*ffo,
	struct file_track		*fil_trk,
	int				track)

	{
	int				size;

	debug_error_condition(! file_mode_write(fil->mode));
	size = file_write(fil, fifo_get_data(ffo), fifo_get_wr_ofs(ffo));
	fifo_set_rd_ofs(ffo, size);
	return (1);
	}




/****************************************************************************
 *
 * used by external callers
 *
 ****************************************************************************/




/****************************************************************************
 * file_image_raw
 ****************************************************************************/
struct file_image			file_image_raw =
	{
	.name        = "raw",
	.level       = 0,
	.open        = file_open_raw,
	.close       = file_close_raw,
	.type        = file_type_raw,
	.track_read  = file_read_raw,
	.track_write = file_write_raw
	};



/****************************************************************************
 * file_image_plain
 ****************************************************************************/
struct file_image			file_image_plain =
	{
	.name        = "plain",
	.level       = 3,
	.open        = file_open_plain,
	.close       = file_close_plain,
	.type        = file_type_plain,
	.track_read  = file_read_plain,
	.track_write = file_write_plain
	};



/****************************************************************************
 * file_search_image
 ****************************************************************************/
struct file_image *
file_search_image(
	const char			*name)

	{
	int				i;

	for (i = 0; fil_img[i] != NULL; i++) if (string_equal(name, fil_img[i]->name)) break;
	return (fil_img[i]);
	}



/****************************************************************************
 * file_open
 ****************************************************************************/
int
file_open(
	struct file			*fil,
	const char			*path,
	int				mode)

	{
	*fil = (struct file)
		{
		.path = path,
		.fd   = -1,
		.mode = mode,
		.type = FILE_TYPE_REGULAR
		};

	debug_error_condition(! file_mode_valid(mode));
	if (file_mode_write(mode))
		{
		if (string_equal(path, "-")) fil->fd = STDOUT_FILENO;
		else fil->fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
		}
	else 
		{
		if (string_equal(path, "-")) fil->fd = STDIN_FILENO;
		else fil->fd = open(path, O_RDONLY);
		}
	if (fil->fd == -1)
		{
		if (file_mode_read_no_error(fil->mode)) return (0);
		error_perror2("error while opening '%s'", path);
		}
	return (1);
	}



/****************************************************************************
 * file_close
 ****************************************************************************/
int
file_close(
	struct file			*fil)

	{
	if (close(fil->fd) == -1) error_perror2("error while closing '%s'", fil->path);
	fil->fd = -1;
	return (1);
	}



/****************************************************************************
 * file_read
 ****************************************************************************/
int
file_read(
	struct file			*fil,
	unsigned char			*data,
	int				size)

	{
	int				result = 1, ofs = 0;

	while ((result > 0) && (size > 0))
		{
		result = read(fil->fd, &data[ofs], size);
		if ((result == -1) && (errno == EAGAIN)) continue;
		if (result == -1) error_perror2("error while reading from '%s'", fil->path);
		size -= result;
		ofs += result;
		}
	return (ofs);
	}



/****************************************************************************
 * file_write
 ****************************************************************************/
int
file_write(
	struct file			*fil,
	const unsigned char		*data,
	int				size)

	{
	int				result, ofs = 0;

	while (size > 0)
		{
		result = write(fil->fd, &data[ofs], size);
		if ((result == -1) && (errno == EAGAIN)) continue;
		if (result == -1) error_perror2("error while writing to '%s'", fil->path);
		size -= result;
		ofs += result;
		}
	return (ofs);
	}
/******************************************************** Karsten Scheibler */
