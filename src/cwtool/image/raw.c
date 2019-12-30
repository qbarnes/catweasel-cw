/****************************************************************************
 ****************************************************************************
 *
 * image/raw.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>

#include "raw.h"
#include "../error.h"
#include "../debug.h"
#include "../verbose.h"
#include "../cwtool.h"
#include "../fifo.h"
#include "../file.h"
#include "../image.h"
#include "../import.h"
#include "../export.h"



#define TYPE_DEVICE			1
#define TYPE_PIPE			2
#define TYPE_REGULAR			3

#define FLAG_SEARCH_HINTS		(1 << 0)

#define TRACK_FLAG_DONE			(1 << 0)
#define TRACK_FLAG_FOUND		(1 << 1)

#define HEADER_FLAG_WRITABLE		(1 << 0)
#define HEADER_FLAG_INDEX_STORED	(1 << 1)
#define HEADER_FLAG_INDEX_ALIGNED	(1 << 2)


struct track_header
	{
	unsigned char			magic;
	unsigned char			track;
	unsigned char			clock;
	unsigned char			flags;
	unsigned char			size[4];
	};




/****************************************************************************
 *
 * low level functions
 *
 ****************************************************************************/




/****************************************************************************
 * image_raw_track_translate
 ****************************************************************************/
static int
image_raw_track_translate(
	struct image_track		*img_trk,
	int				track)

	{
	int				flip = 0, t, s;

	debug(2, "untranslated track = %d", track);
	debug_error_condition((track < 0) || (track >= CWTOOL_MAX_TRACK));
	if (img_trk->flags & IMAGE_TRACK_FLAG_FLIP_SIDE) flip = 1;
	if (img_trk->side_offset != 0)
		{
		t = track;
		s = (t >= img_trk->side_offset) ? 1 : 0;
		if (s > 0) t -= img_trk->side_offset;
		s ^= flip;
		track = 2 * t + s;
		}
	else track ^= flip;
	debug_error_condition((track < 0) || (track >= CWTOOL_MAX_TRACK));
	debug(2, "translated track = %d", track);
	return (track);
	}



/****************************************************************************
 * image_raw_ioctl
 ****************************************************************************/
static int
image_raw_ioctl(
	struct image_raw		*img_raw,
	struct image_track		*img_trk,
	int				timeout,
	int				track,
	int				cmd,
	int				mode,
	unsigned char			*data,
	int				size)

	{
	struct cw_trackinfo		tri = CW_TRACKINFO_INIT;
	int				result = -1;

	tri.clock   = img_trk->clock;
	tri.timeout = timeout;
	tri.track   = track / 2;
	tri.side    = track & 1;
	tri.mode    = mode;
	tri.data    = data;
	tri.size    = size;

	/*
	 * i had to choose between two ways to implement support for drives
	 * with 40 tracks (like those old 5.25" 360K drives):
	 * 1. i keep the track numbering scheme and the driver signals
	 *    inaccessible tracks
	 * 2. the driver stepping routines stay unchanged and i just tell the
	 *    userspace, that this drive has half the tracks a normal drive
	 *    would have, so one step effectively is a doulbe step
	 *
	 * i decided to take the second approach, not because it is better
	 * than the first, only because it seemed easier to implement
	 */

	if (img_raw->fli.flags & CW_FLOPPYINFO_FLAG_DOUBLE_STEP)
		{
		if ((tri.track & 1) != 0)
			{
			if (! (img_trk->flags & IMAGE_TRACK_FLAG_OPTIONAL)) error_warning("error while accessing track %d, only double steps are supported by device '%s'", track, file_get_path(&img_raw->fil[0]));
			goto done;
			}
		tri.track /= 2;
		debug(2, "drive only supports double steps, so track is halved");
		}
	verbose(1, "accessing hardware track %d side %d with timeout %d ms on '%s'", tri.track, tri.side, tri.timeout, file_get_path(&img_raw->fil[0]));
	if (tri.clock >= img_raw->fli.clock_max) error("error while accessing track %d, clock is not supported by device '%s'", track, file_get_path(&img_raw->fil[0]));
	if (tri.track >= img_raw->fli.track_max) error("error while accessing track %d, track is not supported by device '%s'", track, file_get_path(&img_raw->fil[0]));
	if (tri.side  >= img_raw->fli.side_max)  error("error while accessing track %d, side is not supported by device '%s'", track, file_get_path(&img_raw->fil[0]));
	if (tri.mode  >= img_raw->fli.mode_max)  error("error while accessing track %d, mode is not supported by device '%s'", track, file_get_path(&img_raw->fil[0]));
	result = file_ioctl(&img_raw->fil[0], cmd, &tri, FILE_FLAG_NONE);
done:
	return (result);
	}



/****************************************************************************
 * image_raw_seekable
 ****************************************************************************/
static int
image_raw_seekable(
	struct image_raw		*img_raw)

	{
	if ((file_seek(&img_raw->fil[0], 1, FILE_FLAG_RETURN) == 1) &&
		(file_seek(&img_raw->fil[0], 0, FILE_FLAG_RETURN) == 0)) return (1);
	file_open(&img_raw->fil[1], NULL, FILE_MODE_TMP, FILE_FLAG_NONE);
	return (0);
	}



/****************************************************************************
 * image_raw_read_track2
 ****************************************************************************/
static int
image_raw_read_track2(
	struct file			*fil,
	struct track_header		*trk_hdr,
	struct fifo			*ffo)

	{
	int				size = sizeof (struct track_header);

	if (file_read(fil, (unsigned char *) trk_hdr, size) == 0) return (0);
	if (trk_hdr->magic != 0xca) error("wrong header magic in file '%s'", file_get_path(fil));
	if (trk_hdr->track >= CWTOOL_MAX_TRACK) error("invalid track in file '%s'", file_get_path(fil));
	if (trk_hdr->clock >= CW_MAX_CLOCK) error("invalid clock in file '%s'", file_get_path(fil));
	size = import_ulong_le(trk_hdr->size);
	if (size > fifo_get_limit(ffo)) error("track %d too large in file '%s'", trk_hdr->track, file_get_path(fil));
	fifo_reset(ffo);
	if (trk_hdr->flags & HEADER_FLAG_WRITABLE)      fifo_set_flags(ffo, FIFO_FLAG_WRITABLE);
	if (trk_hdr->flags & HEADER_FLAG_INDEX_STORED)  fifo_set_flags(ffo, FIFO_FLAG_INDEX_STORED);
	if (trk_hdr->flags & HEADER_FLAG_INDEX_ALIGNED) fifo_set_flags(ffo, FIFO_FLAG_INDEX_ALIGNED);
	return (file_read_strict(fil, fifo_get_data(ffo), size));
	}



/****************************************************************************
 * image_raw_found
 ****************************************************************************/
static int
image_raw_found(
	struct image_raw		*img_raw,
	struct image_track		*img_trk,
	struct track_header		*trk_hdr,
	int				track)

	{
	int				flag1 = (img_trk->flags & IMAGE_TRACK_FLAG_INDEXED_READ) ? 1 : 0;
	int				flag2 = (trk_hdr->flags & HEADER_FLAG_INDEX_ALIGNED) ? 1 : 0;

	/*
	 * strictly check flags of available tracks. tracks generated for
	 * normal write cannot be written index aligned, because they
	 * contain prolog data
	 */

	if ((trk_hdr->track != track) || (trk_hdr->clock != img_trk->clock) || (flag1 != flag2)) return (0);
	img_raw->track_flags[track] |= TRACK_FLAG_FOUND;
	return (1);
	}



/****************************************************************************
 * image_raw_hint_store
 ****************************************************************************/
static void
image_raw_hint_store(
	struct image_raw		*img_raw,
	struct track_header		*trk_hdr,
	struct fifo			*ffo,
	int				size,
	int				offset)

	{
	int				file = 1;

	if (img_raw->track_flags[trk_hdr->track] & TRACK_FLAG_DONE) return;
	if (img_raw->hints >= IMAGE_RAW_MAX_HINT) error("file '%s' has too many tracks", file_get_path(&img_raw->fil[0]));
	if (img_raw->type == TYPE_PIPE)
		{
		debug(2, "appending track to '%s'", file_get_path(&img_raw->fil[1]));
		file   = 2;
		offset = file_seek(&img_raw->fil[1], -1, FILE_FLAG_NONE);
		file_write(&img_raw->fil[1], (unsigned char *) trk_hdr, sizeof (struct track_header));
		file_write(&img_raw->fil[1], fifo_get_data(ffo), size);
		}
	debug(2, "appending hint, hints = %d file = %d, track = %d, offset = %d", img_raw->hints, file, trk_hdr->track, offset);
	img_raw->hnt[img_raw->hints++] = (struct image_raw_hint)
		{
		.file   = file,
		.track  = trk_hdr->track,
		.clock  = trk_hdr->clock,
		.flags  = trk_hdr->flags,
		.offset = offset
		};
	}



/****************************************************************************
 * image_raw_hint_search
 ****************************************************************************/
static int
image_raw_hint_search(
	struct image_raw		*img_raw,
	struct image_track		*img_trk,
	struct fifo			*ffo,
	int				track)

	{
	struct track_header		trk_hdr;
	int				file, h;

	for (h = 0; h < img_raw->hints; h++)
		{

		/*
		 * UGLY: not very clean, better give needed parameters
		 *       directly to image_raw_found instead using
		 *       struct track_header
		 */

		trk_hdr = (struct track_header)
			{
			.track = img_raw->hnt[h].track,
			.clock = img_raw->hnt[h].clock,
			.flags = img_raw->hnt[h].flags
			};
		if ((! image_raw_found(img_raw, img_trk, &trk_hdr, track)) ||
			(img_raw->hnt[h].file == 0)) continue;
		file = img_raw->hnt[h].file - 1;
		img_raw->hnt[h].file = 0;
		debug(2, "found hint, h = %d file = %d, track = %d, offset = %d", h, file, track, img_raw->hnt[h].offset);
		file_seek(&img_raw->fil[file], img_raw->hnt[h].offset, FILE_FLAG_NONE);
		return (image_raw_read_track2(&img_raw->fil[file], &trk_hdr, ffo));
		}

	/*
	 * check if we are looking for a track, which is not optional and we
	 * haven't seen yet
	 */

	if ((! (img_trk->flags & IMAGE_TRACK_FLAG_OPTIONAL)) && (! (img_raw->track_flags[track] & TRACK_FLAG_FOUND)))
		error_warning("track %d not found in file '%s'", track, file_get_path(&img_raw->fil[0]));
	return (-1);
	}



/****************************************************************************
 * image_raw_hint_invalidate
 ****************************************************************************/
static void
image_raw_hint_invalidate(
	struct image_raw		*img_raw,
	int				track)

	{
	int				h;

	img_raw->track_flags[track] |= TRACK_FLAG_DONE;
	for (h = 0; h < img_raw->hints; h++)
		{
		if ((img_raw->hnt[h].file == 0) || (img_raw->hnt[h].track != track)) continue;
		debug(2, "invalidating hint, h = %d", h);
		img_raw->hnt[h].file = 0;
		}
	}



/****************************************************************************
 * image_raw_read_track
 ****************************************************************************/
static int
image_raw_read_track(
	struct image_raw		*img_raw,
	struct image_track		*img_trk,
	struct fifo			*ffo,
	int				track)

	{
	struct track_header		trk_hdr;
	int				size, offset = 0;

	while (1)
		{

		/*
		 * search in previously stored tracks, if the image was
		 * completely read
		 */

		if (img_raw->flags & FLAG_SEARCH_HINTS) return (image_raw_hint_search(img_raw, img_trk, ffo, track));

		/* read next track */

		verbose(1, "trying to read raw track %d from '%s'", track, file_get_path(&img_raw->fil[0]));
		if (img_raw->type == TYPE_REGULAR) offset = file_seek(&img_raw->fil[0], -1, FILE_FLAG_NONE);
		size = image_raw_read_track2(&img_raw->fil[0], &trk_hdr, ffo);

		/* end of file reached, now search in stored tracks */

		if (size == 0)
			{
			img_raw->flags |= FLAG_SEARCH_HINTS;
			continue;
			}

		/* return if this is the track we are looking for */

		verbose(1, "got raw track %d with %d bytes from '%s'", trk_hdr.track, size, file_get_path(&img_raw->fil[0]));
		if (image_raw_found(img_raw, img_trk, &trk_hdr, track)) return (size);

		/*
		 * we got a currently unwanted track, store it for later
		 * usage
		 */

		image_raw_hint_store(img_raw, &trk_hdr, ffo, size, offset);

		/*
		 * if we would search in previously stored tracks also here,
		 * we would change file offset, this would influence later
		 * reads, so we will only search if all tracks were stored
		 */

		}
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
	if (fifo_get_flags(ffo) & FIFO_FLAG_INDEX_STORED) mask = 0x7f;
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
	struct image_track		*img_trk,
	struct fifo			*ffo)

	{
	int				flags_in1 = fifo_get_flags(ffo);
	int				flags_in2 = img_trk->flags;
	int				flags_out = 0;

	/*
	 * if input data was also raw, we get all needed flags with
	 * fifo_get_flags(), otherwise we also have to check if the data
	 * would be written index aligned on a real catweasel device
	 */

	if (flags_in1 & FIFO_FLAG_WRITABLE)             flags_out |= HEADER_FLAG_WRITABLE;
	if (flags_in1 & FIFO_FLAG_INDEX_STORED)         flags_out |= HEADER_FLAG_INDEX_STORED;
	if (flags_in1 & FIFO_FLAG_INDEX_ALIGNED)        flags_out |= HEADER_FLAG_INDEX_ALIGNED;
	if (flags_in2 & IMAGE_TRACK_FLAG_INDEXED_WRITE) flags_out |= HEADER_FLAG_INDEX_ALIGNED;
	return (flags_out);
	}




/****************************************************************************
 *
 * interface functions
 *
 ****************************************************************************/




/****************************************************************************
 * image_raw_open
 ****************************************************************************/
static int
image_raw_open(
	union image			*img,
	char				*path,
	int				mode,
	int				flags)

	{
	static const char		magic[32] = "cwtool raw data";
	static const char		magic2[32] = "cwtool raw data 2";
	char				buffer[32], *type_name;
	int				i;

	image_open(img, &img->raw.fil[0], path, mode);

	/* check if we have a catweasel device */

	type_name     = "device";
	img->raw.type = TYPE_DEVICE;
	img->raw.fli  = CW_FLOPPYINFO_INIT;
	if (file_ioctl(&img->raw.fil[0], CW_IOC_GFLPARM, &img->raw.fli, FILE_FLAG_RETURN) == 0) goto done;

	/*
	 * check if we have a pipe or a regular file, check file header if
	 * file was opened for reading, or write file header
	 */

	type_name     = "pipe or a file";
	img->raw.type = TYPE_PIPE;
	if (file_is_readable(&img->raw.fil[0]))
		{
		if (! image_raw_seekable(&img->raw)) type_name = "pipe";
		else type_name = "file", img->raw.type = TYPE_REGULAR;
		file_read_strict(&img->raw.fil[0], (unsigned char *) buffer, sizeof (buffer));
		for (i = 0; i < sizeof (magic); i++) if ((magic[i] != buffer[i]) && (magic2[i] != buffer[i])) error("file '%s' has wrong magic", file_get_path(&img->raw.fil[0]));
		}
	else file_write(&img->raw.fil[0], (unsigned char *) magic2, sizeof (magic2));
done:
	verbose(1, "assuming '%s' is a %s", file_get_path(&img->raw.fil[0]), type_name);
	return (1);
	}



/****************************************************************************
 * image_raw_close
 ****************************************************************************/
static int
image_raw_close(
	union image			*img)

	{
	struct track_header		trk_hdr;
	unsigned char			data[CWTOOL_MAX_TRACK_SIZE];
	struct fifo			ffo = FIFO_INIT(data, sizeof (data));

	/* read remaining data if we have a pipe to prevent "broken pipe" */

	if ((file_is_readable(&img->raw.fil[0])) && (img->raw.type == TYPE_PIPE))
		{
		while (image_raw_read_track2(&img->raw.fil[0], &trk_hdr, &ffo) > 0) ;
		file_close(&img->raw.fil[1]);
		}
	return (image_close(img, &img->raw.fil[0]));
	}



/****************************************************************************
 * image_raw_offset
 ****************************************************************************/
static int
image_raw_offset(
	union image			*img)

	{

	/*
	 * image "raw" may be also used as destination while reading, so we
	 * have to allow calls to this function
	 */

	return (0);
	}



/*
 * when reading or writing a raw image to a file, things like side_offset or
 * flip_side have no effect. because they are applied twice, once to read the
 * track and once to write it. on read we translate the given track to the
 * "real" track number (depending on side_offset and flip_side, and if we
 * access a catweasel device also double_step), on write we do the same thing
 * again and write to the "real" track. So the only thing that is influenced
 * by side_offset and flip_side in this case is the order in which tracks are
 * written to the raw image
 */



/****************************************************************************
 * image_raw_read
 ****************************************************************************/
static int
image_raw_read(
	union image			*img,
	struct image_track		*img_trk,
	struct fifo			*ffo,
	struct disk_sector		*dsk_sct,
	int				sectors,
	int				track)

	{
	int				size = fifo_get_limit(ffo);
	int				mode = CW_TRACKINFO_MODE_INDEX_STORE;
	int				flag = FIFO_FLAG_INDEX_STORED;

	track = image_raw_track_translate(img_trk, track);
	debug_error_condition(! file_is_readable(&img->raw.fil[0]));
	debug_error_condition((img->raw.type != TYPE_DEVICE) && (img->raw.type != TYPE_PIPE) && (img->raw.type != TYPE_REGULAR));
	if (img->raw.type == TYPE_DEVICE)
		{
		if (img_trk->flags & IMAGE_TRACK_FLAG_INDEXED_READ) mode = CW_TRACKINFO_MODE_INDEX_WAIT, flag = FIFO_FLAG_INDEX_ALIGNED;
		fifo_set_flags(ffo, flag);
		size = image_raw_ioctl(&img->raw, img_trk, img_trk->timeout_read, track,
			CW_IOC_READ, mode, fifo_get_data(ffo), size);
		}
	else size = image_raw_read_track(&img->raw, img_trk, ffo, track);
	if (size == -1) return (0);
	if (size < CWTOOL_MIN_TRACK_SIZE) error_warning("got only %d bytes while reading track %d", size, track);
	fifo_set_wr_ofs(ffo, size);
	return (1);
	}



/****************************************************************************
 * image_raw_write
 ****************************************************************************/
static int
image_raw_write(
	union image			*img,
	struct image_track		*img_trk,
	struct fifo			*ffo,
	struct disk_sector		*dsk_sct,
	int				sectors,
	int				track)

	{
	int				size = fifo_get_wr_ofs(ffo);
	int				mode = CW_TRACKINFO_MODE_NORMAL;

	track = image_raw_track_translate(img_trk, track);
	debug_error_condition(! file_is_writable(&img->raw.fil[0]));
	debug_error_condition((img->raw.type != TYPE_DEVICE) && (img->raw.type != TYPE_PIPE) && (img->raw.type != TYPE_REGULAR));
	if (size < CWTOOL_MIN_TRACK_SIZE)
		{
		error_warning("got only %d bytes for writing track %d, will skip it", size, track);
		goto done;
		}
	if (img->raw.type == TYPE_DEVICE)
		{
		if (img_trk->flags & IMAGE_TRACK_FLAG_INDEXED_WRITE) mode = CW_TRACKINFO_MODE_INDEX_WAIT;
		image_raw_write_prepare(ffo, track);
		size = image_raw_ioctl(&img->raw, img_trk, img_trk->timeout_write, track,
			CW_IOC_WRITE, mode, fifo_get_data(ffo), size);
		}
	else 
		{
		struct track_header	trk_hdr =
			{
			.magic = 0xca,
			.track = track,
			.clock = img_trk->clock,
			.flags = image_raw_write_flags(img_trk, ffo)
			};

		export_ulong_le(trk_hdr.size, size);
		verbose(1, "writing raw track %d with %d bytes to '%s'", track, size, file_get_path(&img->raw.fil[0]));
		file_write(&img->raw.fil[0], (unsigned char *) &trk_hdr, sizeof (trk_hdr));
		file_write(&img->raw.fil[0], fifo_get_data(ffo), size);
		}
	if (size == -1) return (0);
	if (size < fifo_get_wr_ofs(ffo)) error_warning("could not write full track %d, write timed out", track);
done:
	fifo_set_rd_ofs(ffo, size);
	return (1);
	}



/****************************************************************************
 * image_raw_done
 ****************************************************************************/
static int
image_raw_done(
	union image			*img,
	struct image_track		*img_trk,
	int				track)

	{
	track = image_raw_track_translate(img_trk, track);
	if (img->raw.type != TYPE_DEVICE) image_raw_hint_invalidate(&img->raw, track);
	return (1);
	}




/****************************************************************************
 *
 * used by external callers
 *
 ****************************************************************************/




/****************************************************************************
 * image_raw_desc
 ****************************************************************************/
struct image_desc			image_raw_desc =
	{
	.name        = "raw",
	.level       = 0,
	.open        = image_raw_open,
	.close       = image_raw_close,
	.offset      = image_raw_offset,
	.track_read  = image_raw_read,
	.track_write = image_raw_write,
	.track_done  = image_raw_done
	};
/******************************************************** Karsten Scheibler */
