/****************************************************************************
 ****************************************************************************
 *
 * disk.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>
#include <string.h>

#include "disk.h"
#include "error.h"
#include "debug.h"
#include "verbose.h"
#include "fifo.h"
#include "image.h"
#include "setvalue.h"
#include "string.h"




/****************************************************************************
 *
 * misc helper functions
 *
 ****************************************************************************/




/****************************************************************************
 * disk_sectors_init
 ****************************************************************************/
static int
disk_sectors_init(
	struct disk_sector		*dsk_sct,
	struct disk_track		*dsk_trk,
	struct fifo			*ffo,
	int				write)

	{
	struct disk_sector		dsk_sct2[CWTOOL_MAX_SECTOR];
	unsigned char			*data      = fifo_get_data(ffo);
	int				skew       = dsk_trk->skew;
	int				interleave = dsk_trk->interleave;
	int				errors     = 0x7fffffff;
	int				sectors, size, i, j;

	debug_error_condition(dsk_trk->fmt_dsc->get_sectors == NULL);
	debug_error_condition(dsk_trk->fmt_dsc->get_sector_size == NULL);
	sectors = dsk_trk->fmt_dsc->get_sectors(&dsk_trk->fmt);

	/* initialize sct2[] */

	if (write) errors = 0;
	for (i = j = 0; i < sectors; j += dsk_sct2[i++].size)
		{
		dsk_sct2[i] = (struct disk_sector)
			{
			.number = i,
			.data   = &data[j],
			.offset = j,
			.size   = dsk_trk->fmt_dsc->get_sector_size(&dsk_trk->fmt, i),
			.err    = { .flags = DISK_ERROR_FLAG_NOT_FOUND, .errors = errors }
			};
		}

	/* set fifo limits for write or read */

	size = dsk_trk->fmt_dsc->get_sector_size(&dsk_trk->fmt, -1);
	if (write) fifo_set_limit(ffo, size);
	else if (sectors > 0) fifo_set_wr_ofs(ffo, size);

	/* on write apply sector shuffling according to skew and interleave */

	for (i = 0; i < sectors; i++)
		{
		if (write) j = (skew + i * (interleave + 1)) % sectors;
		else j = i;
		dsk_sct[i] = dsk_sct2[j];
		}

	return (size);
	}



/****************************************************************************
 * disk_info_update
 ****************************************************************************/
static void
disk_info_update(
	struct disk_info		*dsk_nfo,
	struct disk_track		*dsk_trk,
	struct disk_sector		*dsk_sct,
	int				track,
	int				try,
	int				offset,
	int				summary)

	{
	int				sectors = dsk_trk->fmt_dsc->get_sectors(&dsk_trk->fmt);
	int				i, j;

	dsk_nfo->track        = track;
	dsk_nfo->try          = try;
	dsk_nfo->sectors_good = 0;
	dsk_nfo->sectors_weak = 0;
	dsk_nfo->sectors_bad  = 0;
	for (i = 0; i < sectors; i++)
		{
		j = dsk_sct[i].number;
		dsk_nfo->sct_nfo[track][j] = (struct disk_sector_info)
			{
			.flags  = dsk_sct[i].err.flags,
			.offset = offset + dsk_sct[i].offset
			};
		if (dsk_sct[i].err.errors > 0) dsk_nfo->sectors_bad++;
		else if (dsk_sct[i].err.warnings > 0) dsk_nfo->sectors_weak++;
		else dsk_nfo->sectors_good++;
		}
	if (! summary) return;
	dsk_nfo->sum.tracks++;
	dsk_nfo->sum.sectors_good += dsk_nfo->sectors_good;
	dsk_nfo->sum.sectors_weak += dsk_nfo->sectors_weak;
	dsk_nfo->sum.sectors_bad  += dsk_nfo->sectors_bad;
	}



/****************************************************************************
 * disk_info_update_path
 ****************************************************************************/
static void
disk_info_update_path(
	struct disk_info		*dsk_nfo,
	char				*path)

	{
	string_copy(dsk_nfo->path, CWTOOL_MAX_PATH_LEN, path);
	}



/****************************************************************************
 * disk_statistics_track
 ****************************************************************************/
static void
disk_statistics_track(
	struct disk			*dsk,
	union image			*img,
	int				track)

	{
	struct disk_track		*dsk_trk = &dsk->trk[track];
	unsigned char			data[CWTOOL_MAX_TRACK_SIZE] = { };
	struct fifo			ffo = FIFO_INIT(data, sizeof (data));

	if (dsk_trk->fmt_dsc == NULL) goto done;
	debug_error_condition(dsk_trk->fmt_dsc->track_statistics == NULL);

	/* return if track is optional and we got no data */

	if (! dsk->img_dsc_l0->track_read(img, &dsk_trk->img_trk, &ffo, NULL, 0, track))
		{
		if (dsk_trk->img_trk.flags & IMAGE_TRACK_FLAG_OPTIONAL) goto done;
		error("no data available for track %d", track);
		}
	dsk_trk->fmt_dsc->track_statistics(&dsk_trk->fmt, &ffo, track);
done:
	dsk->img_dsc_l0->track_done(img, &dsk_trk->img_trk, track);
	}



/****************************************************************************
 * disk_read_track_greedy2
 ****************************************************************************/
static int
disk_read_track_greedy2(
	struct disk			*dsk,
	struct disk_sector		*dsk_sct,
	struct disk_option		*dsk_opt,
	struct disk_info		*dsk_nfo,
	union image			*img_src,
	union image			*img_dst,
	struct fifo			*ffo_src,
	struct fifo			*ffo_dst,
	int				track)

	{
	struct disk_track		*dsk_trk = &dsk->trk[track];
	int				offset   = dsk->img_dsc->offset(img_dst);
	int				t;

	for (t = 0; t <= dsk_opt->retry; t++)
		{
		fifo_reset(ffo_src);
		fifo_reset(ffo_dst);

		/*
		 * if this track is optional and we could not read
		 * it, because the drive only supports double steps,
		 * we simply ignore this track
		 */

		if (! dsk->img_dsc_l0->track_read(img_src, &dsk_trk->img_trk, ffo_src, NULL, 0, track)) break;
		if (! dsk_trk->fmt_dsc->track_read(&dsk_trk->fmt, ffo_src, ffo_dst, dsk_sct, track)) error("data too long on track %d", track);
		disk_info_update(dsk_nfo, dsk_trk, dsk_sct, track, t, offset, 0);
		if (dsk_opt->info_func != NULL) dsk_opt->info_func(dsk_nfo, 0);
		dsk->img_dsc->track_write(img_dst, &dsk_trk->img_trk, ffo_dst, dsk_sct, dsk_trk->fmt_dsc->get_sectors(&dsk_trk->fmt), track);
		}
	return (t);
	}



/****************************************************************************
 * disk_read_track_greedy
 ****************************************************************************/
static void
disk_read_track_greedy(
	struct disk			*dsk,
	struct disk_option		*dsk_opt,
	struct disk_info		*dsk_nfo,
	char				**path_src,
	union image			**img_src,
	int				img_src_count,
	union image			*img_dst,
	int				track)

	{
	struct disk_track		*dsk_trk = &dsk->trk[track];
	struct disk_sector		dsk_sct[CWTOOL_MAX_SECTOR] = { };
	unsigned char			data_src[CWTOOL_MAX_TRACK_SIZE] = { };
	unsigned char			data_dst[CWTOOL_MAX_TRACK_SIZE] = { };
	struct fifo			ffo_src = FIFO_INIT(data_src, sizeof (data_src));
	struct fifo			ffo_dst = FIFO_INIT(data_dst, sizeof (data_dst));
	int				offset  = dsk->img_dsc->offset(img_dst);
	int				i, t = 0;

	/*
	 * skip this track if ffo_dst would contain 0 bytes, this is the case
	 * with format fill
	 */

	if (disk_sectors_init(dsk_sct, dsk_trk, &ffo_dst, 0) == 0) goto done;
	debug_error_condition(dsk_trk->fmt_dsc->track_read == NULL);

	for (i = 0; i < img_src_count; i++)
		{
		disk_info_update_path(dsk_nfo, path_src[i]);
		t += disk_read_track_greedy2(dsk, dsk_sct, dsk_opt, dsk_nfo, img_src[i], img_dst, &ffo_src, &ffo_dst, track);
		}
	if ((t == 0) && (! (dsk_trk->img_trk.flags & IMAGE_TRACK_FLAG_OPTIONAL))) error("no data available for track %d", track);
	disk_info_update(dsk_nfo, dsk_trk, dsk_sct, track, t, offset, 1);
done:
	for (i = 0; i < img_src_count; i++) dsk->img_dsc_l0->track_done(img_src[i], &dsk_trk->img_trk, track);
	}



/****************************************************************************
 * disk_read_track_nongreedy2
 ****************************************************************************/
static int
disk_read_track_nongreedy2(
	struct disk			*dsk,
	struct disk_sector		*dsk_sct,
	struct disk_option		*dsk_opt,
	struct disk_info		*dsk_nfo,
	union image			*img_src,
	struct fifo			*ffo_src,
	struct fifo			*ffo_dst,
	int				offset,
	int				track)

	{
	struct disk_track		*dsk_trk = &dsk->trk[track];
	int				b, t;

	for (b = -1, t = 0; (b != 0) && (t <= dsk_opt->retry); t++)
		{
		fifo_reset(ffo_src);

		/*
		 * if this track is optional and we could not read
		 * it, because the drive only supports double steps,
		 * we simply ignore this track
		 */

		if (! dsk->img_dsc_l0->track_read(img_src, &dsk_trk->img_trk, ffo_src, NULL, 0, track)) break;
		if (! dsk_trk->fmt_dsc->track_read(&dsk_trk->fmt, ffo_src, ffo_dst, dsk_sct, track)) error("data too long on track %d", track);
		disk_info_update(dsk_nfo, dsk_trk, dsk_sct, track, t, offset, 0);
		if (dsk_opt->info_func != NULL) dsk_opt->info_func(dsk_nfo, 0);
		b = dsk_nfo->sectors_bad;
		}
	return (t);
	}



/****************************************************************************
 * disk_read_track_nongreedy
 ****************************************************************************/
static void
disk_read_track_nongreedy(
	struct disk			*dsk,
	struct disk_option		*dsk_opt,
	struct disk_info		*dsk_nfo,
	char				**path_src,
	union image			**img_src,
	int				img_src_count,
	union image			*img_dst,
	int				track)

	{
	struct disk_track		*dsk_trk = &dsk->trk[track];
	struct disk_sector		dsk_sct[CWTOOL_MAX_SECTOR] = { };
	unsigned char			data_src[CWTOOL_MAX_TRACK_SIZE] = { };
	unsigned char			data_dst[CWTOOL_MAX_TRACK_SIZE] = { };
	struct fifo			ffo_src = FIFO_INIT(data_src, sizeof (data_src));
	struct fifo			ffo_dst = FIFO_INIT(data_dst, sizeof (data_dst));
	int				offset  = dsk->img_dsc->offset(img_dst);
	int				i, t = 0;

	/*
	 * skip this track if ffo_dst would contain 0 bytes, this is the case
	 * with format fill
	 */

	if (disk_sectors_init(dsk_sct, dsk_trk, &ffo_dst, 0) == 0) goto done;
	debug_error_condition(dsk_trk->fmt_dsc->track_read == NULL);

	for (i = 0; i < img_src_count; i++)
		{
		disk_info_update_path(dsk_nfo, path_src[i]);
		t += disk_read_track_nongreedy2(dsk, dsk_sct, dsk_opt, dsk_nfo, img_src[i], &ffo_src, &ffo_dst, offset, track);
		if ((t > 0) && (dsk_nfo->sectors_bad == 0)) break;
		}
	if ((t == 0) && (! (dsk_trk->img_trk.flags & IMAGE_TRACK_FLAG_OPTIONAL))) error("no data available for track %d", track);
	disk_info_update(dsk_nfo, dsk_trk, dsk_sct, track, t, offset, 1);
	dsk->img_dsc->track_write(img_dst, &dsk_trk->img_trk, &ffo_dst, dsk_sct, dsk_trk->fmt_dsc->get_sectors(&dsk_trk->fmt), track);
done:
	for (i = 0; i < img_src_count; i++) dsk->img_dsc_l0->track_done(img_src[i], &dsk_trk->img_trk, track);
	}



/****************************************************************************
 * disk_read_track
 ****************************************************************************/
static void
disk_read_track(
	struct disk			*dsk,
	struct disk_option		*dsk_opt,
	struct disk_info		*dsk_nfo,
	char				**path_src,
	union image			**img_src,
	int				img_src_count,
	union image			*img_dst,
	int				track)

	{
	struct disk_track		*dsk_trk = &dsk->trk[track];

	/* skip this track if no format is defined */

	if (dsk_trk->fmt_dsc == NULL) return;
	debug_error_condition(dsk_trk->fmt_dsc->get_flags == NULL);
	if (dsk_trk->fmt_dsc->get_flags(&dsk_trk->fmt) & FORMAT_FLAG_GREEDY) disk_read_track_greedy(dsk, dsk_opt, dsk_nfo, path_src, img_src, img_src_count, img_dst, track);
	else disk_read_track_nongreedy(dsk, dsk_opt, dsk_nfo, path_src, img_src, img_src_count, img_dst, track);
	}



/****************************************************************************
 * disk_write_track
 ****************************************************************************/
static void
disk_write_track(
	struct disk			*dsk,
	struct disk_option		*dsk_opt,
	struct disk_info		*dsk_nfo,
	union image			*img_src,
	union image			*img_dst,
	int				track)

	{
	struct disk_track		*dsk_trk = &dsk->trk[track];
	struct disk_sector		dsk_sct[CWTOOL_MAX_SECTOR] = { };
	unsigned char			data_src[CWTOOL_MAX_TRACK_SIZE] = { };
	unsigned char			data_dst[CWTOOL_MAX_TRACK_SIZE] = { };
	struct fifo			ffo_src = FIFO_INIT(data_src, sizeof (data_src));
	struct fifo			ffo_dst = FIFO_INIT(data_dst, sizeof (data_dst));

	/* skip this track if no format is defined */

	if (dsk_trk->fmt_dsc == NULL) return;
	disk_sectors_init(dsk_sct, dsk_trk, &ffo_src, 1);
	debug_error_condition(dsk_trk->fmt_dsc->track_write == NULL);

	/*
	 * skip this track if we got 0 bytes but expected more than
	 * that from the image reader, but this will currently
	 * not happen. with format fill also 0 bytes are
	 * expected, so in this case we do not skip this track
	 */

	if (fifo_get_limit(&ffo_src) > 0)
		{
		dsk->img_dsc->track_read(img_src, &dsk_trk->img_trk, &ffo_src, dsk_sct, dsk_trk->fmt_dsc->get_sectors(&dsk_trk->fmt), track);
		if (fifo_get_wr_ofs(&ffo_src) == 0) return;
		}

	if (! dsk_trk->fmt_dsc->track_write(&dsk_trk->fmt, &ffo_src, dsk_sct, &ffo_dst, track)) error("data too long on track %d", track);

	/*
	 * if this track is optional and we could not write it,
	 * because the drive only supports double steps, we simply
	 * continue with the next track
	 */

	if (! dsk->img_dsc_l0->track_write(img_dst, &dsk_trk->img_trk, &ffo_dst, NULL, 0, track)) return;
	disk_info_update(dsk_nfo, dsk_trk, dsk_sct, track, 0, 0, 1);
	if (dsk_opt->info_func != NULL) dsk_opt->info_func(dsk_nfo, 0);
	}




/****************************************************************************
 *
 * used by external callers
 *
 ****************************************************************************/




/****************************************************************************
 * disk_get
 ****************************************************************************/
struct disk *
disk_get(
	int				i)

	{
	static struct disk		dsk[CWTOOL_MAX_DISK];
	static int			disks;

	if (i == -1)
		{
		if (disks >= CWTOOL_MAX_DISK) error_message("too many disks defined");
		debug(1, "request for unused disk struct, disks = %d", disks);
		return (&dsk[disks++]);
		}
	if ((i >= 0) && (i < disks)) return (&dsk[i]);
	return (NULL);
	}



/****************************************************************************
 * disk_search
 ****************************************************************************/
struct disk *
disk_search(
	const char			*name)

	{
	struct disk			*dsk;
	int				i;

	for (i = 0; (dsk = disk_get(i)) != NULL; i++) if (string_equal(name, dsk->name)) break;
	return (dsk);
	}



/****************************************************************************
 * disk_init_track_default
 ****************************************************************************/
struct disk_track *
disk_init_track_default(
	struct disk			*dsk)

	{
	struct disk_track		dsk_trk = DISK_TRACK_INIT;

	dsk->trk_def = dsk_trk;
	return (&dsk->trk_def);
	}



/****************************************************************************
 * disk_init
 ****************************************************************************/
int
disk_init(
	struct disk			*dsk,
	int				version)

	{
	*dsk = (struct disk)
		{
		.version    = version,
		.img_dsc_l0 = image_search_desc("raw"),
		.img_dsc    = image_search_desc("plain")
		};
	debug_error_condition((dsk->img_dsc_l0 == NULL) || (dsk->img_dsc == NULL));
	return (1);
	}



/****************************************************************************
 * disk_insert
 ****************************************************************************/
int
disk_insert(
	struct disk			*dsk)

	{
	struct disk			*dsk2;

	dsk2 = disk_search(dsk->name);
	if (dsk2 != NULL) *dsk2 = *dsk;
	else *disk_get(-1) = *dsk;
	return (1);
	}



/****************************************************************************
 * disk_copy
 ****************************************************************************/
int
disk_copy(
	struct disk			*dsk_dst,
	struct disk			*dsk_src)

	{
	char				name[CWTOOL_MAX_NAME_LEN];
	char				info[CWTOOL_MAX_NAME_LEN];
	int				version;

	string_copy(name, CWTOOL_MAX_NAME_LEN, dsk_dst->name);
	string_copy(info, CWTOOL_MAX_NAME_LEN, dsk_dst->info);
	version = dsk_dst->version; 
	*dsk_dst = *dsk_src;
	string_copy(dsk_dst->name, CWTOOL_MAX_NAME_LEN, name);
	string_copy(dsk_dst->info, CWTOOL_MAX_NAME_LEN, info);
	dsk_dst->version = version;
	return (1);
	}



/****************************************************************************
 * disk_tracks_used
 ****************************************************************************/
int
disk_tracks_used(
	struct disk			*dsk)

	{
	struct disk_track		*dsk_trk;
	int				s, t, used;

	for (t = used = 0; t < CWTOOL_MAX_TRACK; t++)
		{
		dsk_trk = &dsk->trk[t];
		if (dsk_trk->fmt_dsc == NULL) continue;
		debug_error_condition(dsk_trk->fmt_dsc->get_sectors == NULL);
		debug_error_condition(dsk_trk->fmt_dsc->get_sector_size == NULL);
		s = dsk_trk->fmt_dsc->get_sectors(&dsk_trk->fmt);
		debug_error_condition((s < 0) || (s > CWTOOL_MAX_SECTOR));
		s = dsk_trk->fmt_dsc->get_sector_size(&dsk_trk->fmt, -1);
		debug_error_condition((s < 0) || (s > CWTOOL_MAX_TRACK_SIZE));
		dsk->size += s;
		used++;
		}
	return ((used > 0) ? 1 : 0);
	}



/****************************************************************************
 * disk_image_ok
 ****************************************************************************/
int
disk_image_ok(
	struct disk			*dsk)

	{
	struct disk_track		*dsk_trk;
	int				t;

	debug_error_condition(dsk->img_dsc == NULL);
	for (t = 0; t < CWTOOL_MAX_TRACK; t++)
		{
		dsk_trk = &dsk->trk[t];
		if (dsk_trk->fmt_dsc == NULL) continue;
		if (dsk_trk->fmt_dsc->level == -1) continue;
		if (dsk_trk->fmt_dsc->level != dsk->img_dsc->level) return (0);
		}
	return (1);
	}



/****************************************************************************
 * disk_check_track
 ****************************************************************************/
int
disk_check_track(
	int				track)

	{
	if ((track < 0) || (track >= CWTOOL_MAX_TRACK)) return (0);
	return (1);
	}



/****************************************************************************
 * disk_check_sectors
 ****************************************************************************/
int
disk_check_sectors(
	int				sectors)

	{
	if ((sectors < 0) || (sectors > CWTOOL_MAX_SECTOR)) return (0);
	return (1);
	}



/****************************************************************************
 * disk_set_image
 ****************************************************************************/
int
disk_set_image(
	struct disk			*dsk,
	struct image_desc		*img_dsc)

	{
	dsk->img_dsc = img_dsc;
	return (1);
	}



/****************************************************************************
 * disk_set_name
 ****************************************************************************/
int
disk_set_name(
	struct disk			*dsk,
	const char			*name)

	{
	struct disk			*dsk2 = disk_search(name);

	if (dsk2 != NULL) if (dsk->version <= dsk2->version) return (0);
	string_copy(dsk->name, CWTOOL_MAX_NAME_LEN, name);
	return (1);
	}



/****************************************************************************
 * disk_set_info
 ****************************************************************************/
int
disk_set_info(
	struct disk			*dsk,
	const char			*info)

	{
	string_copy(dsk->info, CWTOOL_MAX_NAME_LEN, info);
	return (1);
	}



/****************************************************************************
 * disk_set_track
 ****************************************************************************/
int
disk_set_track(
	struct disk			*dsk,
	struct disk_track		*dsk_trk,
	int				track)

	{
	if ((track < 0) || (track >= CWTOOL_MAX_TRACK)) return (0);
	debug(2, "setting track %d", track);
	dsk->trk[track] = *dsk_trk;
	return (1);
	}



/****************************************************************************
 * disk_set_format
 ****************************************************************************/
int
disk_set_format(
	struct disk_track		*dsk_trk,
	struct format_desc		*fmt_dsc)

	{
	dsk_trk->fmt_dsc = fmt_dsc;
	debug_error_condition(dsk_trk->fmt_dsc->set_defaults == NULL);
	dsk_trk->fmt_dsc->set_defaults(&dsk_trk->fmt);
	return (1);
	}



/****************************************************************************
 * disk_set_image_track_flag
 ****************************************************************************/
int
disk_set_image_track_flag(
	struct disk_track		*dsk_trk,
	int				val,
	int				bit)

	{
	return (setvalue_uchar_bit(&dsk_trk->img_trk.flags, val, bit));
	}



/****************************************************************************
 * disk_set_clock
 ****************************************************************************/
int
disk_set_clock(
	struct disk_track		*dsk_trk,
	int				clock)

	{
	if (clock == 14)      dsk_trk->img_trk.clock = 0;
	else if (clock == 28) dsk_trk->img_trk.clock = 1;
	else if (clock == 56) dsk_trk->img_trk.clock = 2;
	else return (0);
	return (1);
	}



/****************************************************************************
 * disk_set_side_offset
 ****************************************************************************/
int
disk_set_side_offset(
	struct disk_track		*dsk_trk,
	int				side_offset)

	{
	return (setvalue_uchar(&dsk_trk->img_trk.side_offset, side_offset, 0, CWTOOL_MAX_TRACK / 2));
	}



/****************************************************************************
 * disk_set_timeout_read
 ****************************************************************************/
int
disk_set_timeout_read(
	struct disk_track		*dsk_trk,
	int				timeout)

	{
	return (setvalue_ushort(&dsk_trk->img_trk.timeout_read, timeout, CW_MIN_TIMEOUT + 1, CW_MAX_TIMEOUT - 1));
	}



/****************************************************************************
 * disk_set_timeout_write
 ****************************************************************************/
int
disk_set_timeout_write(
	struct disk_track		*dsk_trk,
	int				timeout)

	{
	return (setvalue_ushort(&dsk_trk->img_trk.timeout_write, timeout, CW_MIN_TIMEOUT + 1, CW_MAX_TIMEOUT - 1));
	}



/****************************************************************************
 * disk_set_read_option
 ****************************************************************************/
int
disk_set_read_option(
	struct disk_track		*dsk_trk,
	struct format_option		*fmt_opt,
	int				num,
	int				ofs)

	{
	debug_error_condition(dsk_trk->fmt_dsc == NULL);
	debug_error_condition(dsk_trk->fmt_dsc->set_read_option == NULL);
	return (dsk_trk->fmt_dsc->set_read_option(&dsk_trk->fmt, fmt_opt->magic, num, ofs));
	}



/****************************************************************************
 * disk_set_write_option
 ****************************************************************************/
int
disk_set_write_option(
	struct disk_track		*dsk_trk,
	struct format_option		*fmt_opt,
	int				num,
	int				ofs)

	{
	debug_error_condition(dsk_trk->fmt_dsc == NULL);
	debug_error_condition(dsk_trk->fmt_dsc->set_write_option == NULL);
	return (dsk_trk->fmt_dsc->set_write_option(&dsk_trk->fmt, fmt_opt->magic, num, ofs));
	}



/****************************************************************************
 * disk_set_rw_option
 ****************************************************************************/
int
disk_set_rw_option(
	struct disk_track		*dsk_trk,
	struct format_option		*fmt_opt,
	int				num,
	int				ofs)

	{
	debug_error_condition(dsk_trk->fmt_dsc == NULL);
	debug_error_condition(dsk_trk->fmt_dsc->set_rw_option == NULL);
	return (dsk_trk->fmt_dsc->set_rw_option(&dsk_trk->fmt, fmt_opt->magic, num, ofs));
	}



/****************************************************************************
 * disk_set_skew
 ****************************************************************************/
int
disk_set_skew(
	struct disk_track		*dsk_trk,
	int				skew)

	{
	debug_error_condition(dsk_trk->fmt_dsc == NULL);
	debug_error_condition(dsk_trk->fmt_dsc->get_sectors == NULL);
	if (dsk_trk->fmt_dsc->get_sectors(&dsk_trk->fmt) < 2) return (0);
	return (setvalue_uchar(&dsk_trk->skew, skew, 0, CWTOOL_MAX_SECTOR - 1));
	}



/****************************************************************************
 * disk_set_interleave
 ****************************************************************************/
int
disk_set_interleave(
	struct disk_track		*dsk_trk,
	int				interleave)

	{
	debug_error_condition(dsk_trk->fmt_dsc == NULL);
	debug_error_condition(dsk_trk->fmt_dsc->get_sectors == NULL);
	if (dsk_trk->fmt_dsc->get_sectors(&dsk_trk->fmt) < 2) return (0);
	return (setvalue_uchar(&dsk_trk->interleave, interleave, 0, CWTOOL_MAX_SECTOR - 1));
	}



/****************************************************************************
 * disk_set_sector_number
 ****************************************************************************/
int
disk_set_sector_number(
	struct disk_sector		*dsk_sct,
	int				number)

	{
	debug_error_condition((number < 0) || (number >= CWTOOL_MAX_SECTOR));
	dsk_sct->number = number;
	return (0);
	}



/****************************************************************************
 * disk_get_sector_number
 ****************************************************************************/
int
disk_get_sector_number(
	struct disk_sector		*dsk_sct)

	{
	return (dsk_sct->number);
	}



/****************************************************************************
 * disk_get_sectors
 ****************************************************************************/
int
disk_get_sectors(
	struct disk_track		*dsk_trk)

	{
	debug_error_condition(dsk_trk->fmt_dsc == NULL);
	debug_error_condition(dsk_trk->fmt_dsc->get_sectors == NULL);
	return (dsk_trk->fmt_dsc->get_sectors(&dsk_trk->fmt));
	}



/****************************************************************************
 * disk_get_name
 ****************************************************************************/
const char *
disk_get_name(
	struct disk			*dsk)

	{
	debug_error_condition(dsk == NULL);
	return (dsk->name);
	}



/****************************************************************************
 * disk_get_info
 ****************************************************************************/
const char *
disk_get_info(
	struct disk			*dsk)

	{
	debug_error_condition(dsk == NULL);
	return (dsk->info);
	}



/****************************************************************************
 * disk_get_format
 ****************************************************************************/
struct format_desc *
disk_get_format(
	struct disk_track		*dsk_trk)

	{
	debug_error_condition(dsk_trk == NULL);
	return (dsk_trk->fmt_dsc);
	}



/****************************************************************************
 * disk_get_read_options
 ****************************************************************************/
struct format_option *
disk_get_read_options(
	struct disk_track		*dsk_trk)

	{
	debug_error_condition(dsk_trk->fmt_dsc == NULL);
	debug_error_condition(dsk_trk->fmt_dsc->fmt_opt_rd == NULL);
	return (dsk_trk->fmt_dsc->fmt_opt_rd);
	}



/****************************************************************************
 * disk_get_write_options
 ****************************************************************************/
struct format_option *
disk_get_write_options(
	struct disk_track		*dsk_trk)

	{
	debug_error_condition(dsk_trk->fmt_dsc == NULL);
	debug_error_condition(dsk_trk->fmt_dsc->fmt_opt_wr == NULL);
	return (dsk_trk->fmt_dsc->fmt_opt_wr);
	}



/****************************************************************************
 * disk_get_rw_options
 ****************************************************************************/
struct format_option *
disk_get_rw_options(
	struct disk_track		*dsk_trk)

	{
	debug_error_condition(dsk_trk->fmt_dsc == NULL);
	debug_error_condition(dsk_trk->fmt_dsc->fmt_opt_rw == NULL);
	return (dsk_trk->fmt_dsc->fmt_opt_rw);
	}



/****************************************************************************
 * disk_error_add
 ****************************************************************************/
int
disk_error_add(
	struct disk_error		*dsk_err,
	int				flags,
	int				add)

	{
	if (add > 0) dsk_err->flags |= flags, dsk_err->errors += add;
	return (1);
	}



/****************************************************************************
 * disk_warning_add
 ****************************************************************************/
int
disk_warning_add(
	struct disk_error		*dsk_err,
	int				add)

	{
	if (add > 0) dsk_err->warnings += add;
	return (1);
	}



/****************************************************************************
 * disk_sector_read
 ****************************************************************************/
int
disk_sector_read(
	struct disk_sector		*dsk_sct,
	struct disk_error		*dsk_err,
	unsigned char			*data)

	{
	if (((dsk_sct->err.errors == dsk_err->errors) && (dsk_sct->err.warnings < dsk_err->warnings)) ||
		(dsk_sct->err.errors < dsk_err->errors)) return (0);
	dsk_sct->err = *dsk_err;
	if (data != NULL) memcpy(dsk_sct->data, data, dsk_sct->size);
	return (1);
	}



/****************************************************************************
 * disk_sector_write
 ****************************************************************************/
int
disk_sector_write(
	unsigned char			*data,
	struct disk_sector		*dsk_sct)

	{
	memcpy(data, dsk_sct->data, dsk_sct->size);
	return (1);
	}



/****************************************************************************
 * disk_statistics
 ****************************************************************************/
int
disk_statistics(
	struct disk			*dsk,
	char				*path)

	{
	union image			img;
	int				t;

	debug_error_condition(dsk->img_dsc_l0->open == NULL);
	debug_error_condition(dsk->img_dsc_l0->close == NULL);
	debug_error_condition(dsk->img_dsc_l0->track_read == NULL);

	/* open image */

	dsk->img_dsc_l0->open(&img, path, IMAGE_MODE_READ, IMAGE_FLAG_NONE);

	/* iterate over all defined tracks */

	for (t = 0; t < CWTOOL_MAX_TRACK; t++) disk_statistics_track(dsk, &img, t);

	/* close image */

	dsk->img_dsc_l0->close(&img);

	/* done */

	return (1);
	}



/****************************************************************************
 * disk_read
 ****************************************************************************/
int
disk_read(
	struct disk			*dsk,
	struct disk_option		*dsk_opt,
	char				**path_src,
	int				path_src_count,
	char				*path_dst)

	{

	/*
	 * it would be no good idea to put img_src[] onto the stack, because
	 * it could exceed the allowed stack size (especially
	 * struct image_raw is large). so we just put an array of pointers
	 * on the stack and malloc each struct image later
	 */

	struct disk_info		dsk_nfo = { };
	union image			*img_src[CWTOOL_MAX_IMAGES], img_dst;
	int				i, t;

	debug_error_condition(dsk->img_dsc_l0->open == NULL);
	debug_error_condition(dsk->img_dsc_l0->close == NULL);
	debug_error_condition(dsk->img_dsc_l0->track_read == NULL);
	debug_error_condition(dsk->img_dsc->open == NULL);
	debug_error_condition(dsk->img_dsc->close == NULL);
	debug_error_condition(dsk->img_dsc->track_write == NULL);

	/* open images */

	for (i = 0; i < path_src_count; i++)
		{
		img_src[i] = (union image *) malloc(sizeof (union image));
		if (img_src[i] == NULL) error_oom();
		dsk->img_dsc_l0->open(img_src[i], path_src[i], IMAGE_MODE_READ, IMAGE_FLAG_NONE);
		}
	dsk->img_dsc->open(&img_dst, path_dst, IMAGE_MODE_WRITE, IMAGE_FLAG_NONE);

	/* iterate over all tracks */

	for (t = 0; t < CWTOOL_MAX_TRACK; t++) disk_read_track(dsk, dsk_opt, &dsk_nfo, path_src, img_src, path_src_count, &img_dst, t);
	if (dsk_opt->info_func != NULL) dsk_opt->info_func(&dsk_nfo, 1);

	/* close images */

	for (i = 0; i < path_src_count; i++)
		{
		dsk->img_dsc_l0->close(img_src[i]);
		free(img_src[i]);
		}
	dsk->img_dsc->close(&img_dst);

	/* done */

	return (1);
	}



/****************************************************************************
 * disk_write
 ****************************************************************************/
int
disk_write(
	struct disk			*dsk,
	struct disk_option		*dsk_opt,
	char				*path_src,
	char				*path_dst)

	{
	struct disk_info		dsk_nfo = { };
	union image			img_src, img_dst;
	int				flags = (dsk_opt->flags & DISK_OPTION_FLAG_IGNORE_SIZE) ? IMAGE_FLAG_IGNORE_SIZE : IMAGE_FLAG_NONE;
	int				t;

	debug_error_condition(dsk->img_dsc->open == NULL);
	debug_error_condition(dsk->img_dsc->close == NULL);
	debug_error_condition(dsk->img_dsc->track_read == NULL);
	debug_error_condition(dsk->img_dsc_l0->open == NULL);
	debug_error_condition(dsk->img_dsc_l0->close == NULL);
	debug_error_condition(dsk->img_dsc_l0->track_write == NULL);

	/* open images */

	dsk->img_dsc->open(&img_src, path_src, IMAGE_MODE_READ, flags);
	dsk->img_dsc_l0->open(&img_dst, path_dst, IMAGE_MODE_WRITE, IMAGE_FLAG_NONE);

	/* iterate over all tracks */

	for (t = 0; t < CWTOOL_MAX_TRACK; t++) disk_write_track(dsk, dsk_opt, &dsk_nfo, &img_src, &img_dst, t);
	if (dsk_opt->info_func != NULL) dsk_opt->info_func(&dsk_nfo, 1);

	/* close images */

	dsk->img_dsc->close(&img_src);
	dsk->img_dsc_l0->close(&img_dst);

	/* done */

	return (1);
	}
/******************************************************** Karsten Scheibler */
