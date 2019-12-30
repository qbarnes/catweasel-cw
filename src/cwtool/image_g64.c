/****************************************************************************
 ****************************************************************************
 *
 * image_g64.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "debug.h"
#include "verbose.h"
#include "fifo.h"
#include "file.h"
#include "image.h"
#include "import.h"
#include "export.h"



#define MAGIC_SIZE			8
#define TRACK_SIZE			7928

struct g64_header
	{
	unsigned char			version;
	unsigned char			tracks;
	unsigned char			track_size[2];
	};



/****************************************************************************
 * image_g64_allocate_track
 ****************************************************************************/
static struct image_g64_track
image_g64_allocate_track(
	int				size,
	int				speed,
	int				used)

	{
	struct image_g64_track		img_g64_trk;

	img_g64_trk = (struct image_g64_track)
		{
		.data  = (unsigned char *) malloc(size),
		.size  = size,
		.speed = speed,
		.used  = used
		};
	if (img_g64_trk.data == NULL) error_oom();
	return (img_g64_trk);
	}



/****************************************************************************
 * image_g64_read_fill
 ****************************************************************************/
static int
image_g64_read_fill(
	struct image			*img,
	int				size)

	{
	unsigned char			buffer[0x10000];
	int				l, s = size;

	debug_error_condition(size < 0);
	while (s > 0)
		{
		l = size;
		if (l > sizeof (buffer)) l = sizeof (buffer);
		verbose(2, "reading %d fill bytes", l);
		s -= file_read_strict(&img->fil, buffer, l);
		}
	return (size);
	}



/****************************************************************************
 * image_g64_write_fill
 ****************************************************************************/
static int
image_g64_write_fill(
	struct image			*img,
	int				value,
	int				size)

	{
	unsigned char			buffer[0x10000];
	int				l, s = size;

	debug_error_condition(size < 0);
	memset(buffer, value, (size > sizeof (buffer)) ? sizeof (buffer) : size);
	while (s > 0)
		{
		l = size;
		if (l > sizeof (buffer)) l = sizeof (buffer);
		verbose(2, "writing %d fill bytes", l);
		s -= file_write(&img->fil, buffer, l);
		}
	return (size);
	}



/****************************************************************************
 * image_g64_read_data
 ****************************************************************************/
static void
image_g64_read_data(
	struct image			*img,
	int				tracks,
	int				max_track_size,
	int				offset)

	{
	unsigned char			buffer[4];
	unsigned int			track_offsets[IMAGE_G64_MAX_TRACKS];
	unsigned int			speed_offsets[IMAGE_G64_MAX_TRACKS];
	int				i, s, t;

	/* read track offsets */

	for (t = 0; t < tracks; t++, offset += 4)
		{
		verbose(2, "reading G64 track offset for track %d from '%s'", t, file_get_path(&img->fil));
		file_read_strict(&img->fil, buffer, 4);
		track_offsets[t] = import_ulong_le(buffer);
		verbose(2, "got track offset %d", track_offsets[t]);
		}

	/* read speed offsets */

	for (t = 0; t < tracks; t++, offset += 4)
		{
		verbose(2, "reading G64 speed offset for track %d from '%s'", t, file_get_path(&img->fil));
		file_read_strict(&img->fil, buffer, 4);
		speed_offsets[t] = import_ulong_le(buffer);
		verbose(2, "got speed offset %d", speed_offsets[t]);
		if (speed_offsets[t] > 3)  error("file '%s' uses unsupported speed zone map", file_get_path(&img->fil));
		}

	/* read track data */

	while (1)
		{

		/* find smallest track offset */

		for (i = s = 0, t = -1; i < tracks; i++)
			{
			if (track_offsets[i] == 0) continue;
			if ((t != -1) && (s < track_offsets[i])) continue;
			s = track_offsets[i];
			t = i;
			}
		if (t == -1) break;
		verbose(2, "reading G64 data for track %d from '%s'", t, file_get_path(&img->fil));

		/* skip fill bytes in between tracks (if any) */

		offset += image_g64_read_fill(img, track_offsets[t] - offset);

		/* read in data */

		file_read_strict(&img->fil, buffer, 2);
		s = import_ushort_le(buffer);
		if (s > max_track_size) error("track %d in file '%s' too large", t, file_get_path(&img->fil));
		img->g64_trk[t] = image_g64_allocate_track(s, speed_offsets[t], 0);
		file_read_strict(&img->fil, img->g64_trk[t].data, s);
		offset += s + 2;
		track_offsets[t] = 0;
		verbose(2, "got %d bytes", s);

		/* skip fill bytes */

		offset += image_g64_read_fill(img, max_track_size - s);
		}
	if (img->ignore_size) return;
	if (file_read(&img->fil, buffer, 1) == 0) return;
	error_warning("file '%s' has trailing junk", file_get_path(&img->fil));
	}



/****************************************************************************
 * image_g64_write_data
 ****************************************************************************/
static void
image_g64_write_data(
	struct image			*img,
	int				max_track_size,
	int				offset)

	{
	unsigned char			buffer[4];
	int				s, t;

	/* write track offsets */

	offset += 8 * IMAGE_G64_MAX_TRACKS;
	for (t = 0; t < IMAGE_G64_MAX_TRACKS; t++)
		{
		if (img->g64_trk[t].data == NULL) s = 0;
		else s = offset, offset += max_track_size + 2;
		export_ulong_le(buffer, s);
		verbose(2, "writing G64 track offset %d for track %d to '%s'", s, t, file_get_path(&img->fil));
		file_write(&img->fil, buffer, 4);
		}

	/* write speed offsets */

	for (t = 0; t < IMAGE_G64_MAX_TRACKS; t++)
		{
		export_ulong_le(buffer, img->g64_trk[t].speed);
		verbose(2, "writing G64 speed offset %d for track %d to '%s'", img->g64_trk[t].speed, t, file_get_path(&img->fil));
		file_write(&img->fil, buffer, 4);
		}

	/* write track data */

	for (t = 0; t < IMAGE_G64_MAX_TRACKS; t++)
		{
		if (img->g64_trk[t].data == NULL) continue;
		s = img->g64_trk[t].size;
		export_ushort_le(buffer, s);
		verbose(2, "writing G64 data for track %d with %d bytes to '%s'", t, s, file_get_path(&img->fil));
		file_write(&img->fil, buffer, 2);
		file_write(&img->fil, img->g64_trk[t].data, s);

		/* write fill bytes (if needed) */

		image_g64_write_fill(img, 0xaa, max_track_size - s);
		}
	}



/****************************************************************************
 * image_g64_open
 ****************************************************************************/
static int
image_g64_open(
	struct image			*img,
	const char			*path,
	int				mode)

	{
	struct g64_header		g64_hdr;
	static const char		magic[MAGIC_SIZE] = { 'G', 'C', 'R', '-', '1', '5', '4', '1' };
	char				buffer[MAGIC_SIZE];

	file_open(&img->fil, path, image_init(img, mode));
	if (file_get_mode(&img->fil) == FILE_MODE_READ)
		{
		int			track_size, i;

		file_read_strict(&img->fil, (unsigned char *) buffer, sizeof (buffer));
		file_read_strict(&img->fil, (unsigned char *) &g64_hdr, sizeof (g64_hdr));
		for (i = 0; i < sizeof (magic); i++) if (magic[i] != buffer[i]) error("file '%s' has wrong magic", file_get_path(&img->fil));
		if (g64_hdr.version != 0) error("file '%s' has wrong version", file_get_path(&img->fil));
		if (g64_hdr.tracks > IMAGE_G64_MAX_TRACKS) error("file '%s' has too many tracks", file_get_path(&img->fil));
		track_size = import_ushort_le(g64_hdr.track_size);
		image_g64_read_data(img, g64_hdr.tracks, track_size, sizeof (magic) + sizeof (g64_hdr));
		}
	else file_write(&img->fil, (unsigned char *) magic, sizeof (magic));
	return (1);
	}



/****************************************************************************
 * image_g64_close
 ****************************************************************************/
static int
image_g64_close(
	struct image			*img)

	{
	struct g64_header		g64_hdr;
	int				s, t;

	if (file_get_mode(&img->fil) == FILE_MODE_WRITE)
		{

		/*
		 * currently no track will be larger than TRACK_SIZE,
		 * because larger tracks are truncated in image_g64_write()
		 * so the warning will never occur
		 */

		for (s = TRACK_SIZE, t = 0; t < IMAGE_G64_MAX_TRACKS; t++) if (img->g64_trk[t].size > s) s = img->g64_trk[t].size;
		if (s != TRACK_SIZE) error_warning("file '%s' contains large tracks, file may be unusable with other software", file_get_path(&img->fil));
		g64_hdr = (struct g64_header) { .tracks = IMAGE_G64_MAX_TRACKS };
		export_ushort_le(g64_hdr.track_size, s);
		file_write(&img->fil, (unsigned char *) &g64_hdr, sizeof (g64_hdr));
		image_g64_write_data(img, s, MAGIC_SIZE + sizeof (g64_hdr));
		}
	else
		{
		if (! img->ignore_size) for (t = 0; t < IMAGE_G64_MAX_TRACKS; t++)
			{
			if ((img->g64_trk[t].data == NULL) || (img->g64_trk[t].used)) continue;
			error_warning("track %d from file '%s' was not used", t, file_get_path(&img->fil));
			}
		}
	for (t = 0; t < IMAGE_G64_MAX_TRACKS; t++) if (img->g64_trk[t].data != NULL) free(img->g64_trk[t].data);
	return (file_close(&img->fil));
	}



/****************************************************************************
 * image_g64_type
 ****************************************************************************/
static int
image_g64_type(
	struct image			*img)

	{
	debug_error();
	return (0);
	}



/****************************************************************************
 * image_g64_offset
 ****************************************************************************/
static int
image_g64_offset(
	struct image			*img)

	{
	return (0);
	}



/****************************************************************************
 * image_g64_read
 ****************************************************************************/
static int
image_g64_read(
	struct image			*img,
	struct fifo			*ffo,
	struct image_track		*img_trk,
	int				track)

	{
	int				size;

	debug_error_condition(file_get_mode(&img->fil) != FILE_MODE_READ);
	if (track >= IMAGE_G64_MAX_TRACKS) error("track value %d out of range for G64 image format", track); 
	if (img->g64_trk[track].data != NULL)
		{
		img->g64_trk[track].used = 1;
		size = img->g64_trk[track].size;
		debug_error_condition(fifo_get_limit(ffo) < size);
		verbose(1, "reading G64 track %d with %d bytes from memory", track, size);
		fifo_write_block(ffo, img->g64_trk[track].data, size);
		fifo_set_speed(ffo, img->g64_trk[track].speed);
		}
	else
		{
		if (! img->ignore_size) error_warning("track %d in file '%s' contains no data", track, file_get_path(&img->fil));
		memset(fifo_get_data(ffo), 0x55, TRACK_SIZE);
		fifo_set_wr_ofs(ffo, TRACK_SIZE);
		}
	return (1);
	}



/****************************************************************************
 * image_g64_write
 ****************************************************************************/
static int
image_g64_write(
	struct image			*img,
	struct fifo			*ffo,
	struct image_track		*img_trk,
	int				track)

	{
	int				size = fifo_get_wr_ofs(ffo);

	debug_error_condition(file_get_mode(&img->fil) != FILE_MODE_WRITE);
	if (track >= IMAGE_G64_MAX_TRACKS) error("track value %d out of range for G64 image format", track);

	/*
	 * changing this limit to a larger value than TRACK_SIZE may cause
	 * image_g64_close() to print out warnings
	 */

	if (size > TRACK_SIZE)
		{
		error_warning("too much data on track %d for G64 image format, will truncate it", track);
		size = TRACK_SIZE;
		}
	debug_error_condition(img->g64_trk[track].data != NULL);
	verbose(1, "writing G64 track %d with %d bytes to memory", track, size);
	img->g64_trk[track] = image_g64_allocate_track(size, fifo_get_speed(ffo), 1);
	fifo_read_block(ffo, img->g64_trk[track].data, size);
	return (1);
	}



/****************************************************************************
 * image_g64_desc
 ****************************************************************************/
struct image_desc			image_g64_desc =
	{
	.name        = "g64",
	.level       = 1,
	.open        = image_g64_open,
	.close       = image_g64_close,
	.type        = image_g64_type,
	.offset      = image_g64_offset,
	.track_read  = image_g64_read,
	.track_write = image_g64_write
	};
/******************************************************** Karsten Scheibler */
