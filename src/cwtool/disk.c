/****************************************************************************
 ****************************************************************************
 *
 * disk.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>

#include "disk.h"
#include "error.h"
#include "debug.h"
#include "verbose.h"
#include "fifo.h"
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
	unsigned char			*data = fifo_get_data(ffo);
	int				sectors = dsk_trk->fmt_dsc->get_sectors(&dsk_trk->fmt);
	int				skew = dsk_trk->skew, interleave = dsk_trk->interleave;
	int				errors = 0x7fffffff;
	int				size, i, j;

	debug_error_condition(dsk_trk->fmt_dsc->get_sectors == NULL);
	debug_error_condition(dsk_trk->fmt_dsc->get_sector_size == NULL);

	/* initialize sct2[] */

	if (write) errors = 0;
	for (i = j = 0; i < sectors; j += dsk_sct2[i++].size)
		{
		dsk_sct2[i] = (struct disk_sector)
			{
			.number = i,
			.data   = &data[j],
			.size   = dsk_trk->fmt_dsc->get_sector_size(&dsk_trk->fmt, i),
			.err    = { .errors = errors }
			};
		}

	/* set fifo limits for write or read */

	size = dsk_trk->fmt_dsc->get_sector_size(&dsk_trk->fmt, -1);
	if (write) fifo_set_limit(ffo, size);
	else fifo_set_wr_ofs(ffo, size);

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
	struct disk_sector		*dsk_sct,
	int				track,
	int				sectors,
	int				try,
	int				summary)

	{
	int				i;

	dsk_nfo->track        = track;
	dsk_nfo->try          = try;
	dsk_nfo->sectors_good = 0;
	dsk_nfo->sectors_weak = 0;
	dsk_nfo->sectors_bad  = 0;
	for (i = 0; i < sectors; i++)
		{
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
 * disk_sector_do_copy
 ****************************************************************************/
static void
disk_sector_do_copy(
	unsigned char			*dst,
	unsigned char			*src,
	int				size)

	{
	int				i;

	for (i = 0; i < size; i++) dst[i] = src[i];
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
	struct disk			*dsk1,
	struct disk			*dsk2)

	{
	char				name[CWTOOL_MAX_NAME_LEN];
	char				info[CWTOOL_MAX_NAME_LEN];
	int				version;

	string_copy(name, dsk1->name);
	string_copy(info, dsk1->info);
	version = dsk1->version; 
	*dsk1 = *dsk2;
	string_copy(dsk1->name, name);
	string_copy(dsk1->info, info);
	dsk1->version = version;
	return (1);
	}



/****************************************************************************
 * disk_init_size
 ****************************************************************************/
int
disk_init_size(
	struct disk			*dsk)

	{
	struct disk_track		*dsk_trk;
	int				s, t;

	for (t = 0; t < CWTOOL_MAX_TRACK; t++)
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
		}
	return ((dsk->size > 0) ? 1 : 0);
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

	for (t = 0; t < CWTOOL_MAX_TRACK; t++)
		{
		dsk_trk = &dsk->trk[t];
		if (dsk_trk->fmt_dsc == NULL) continue;
		if (dsk_trk->fmt_dsc->level != dsk->fil_img->level) return (0);
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
	struct file_image		*fil_img)

	{
	dsk->fil_img = fil_img;
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
	string_copy(dsk->name, name);
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
	string_copy(dsk->info, info);
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
 * disk_set_indexed_read
 ****************************************************************************/
int
disk_set_indexed_read(
	struct disk_track		*dsk_trk,
	int				val)

	{
	return (setvalue_uchar_bit(&dsk_trk->fil_trk.flags, val, FILE_TRACK_FLAG_INDEXED_READ));
	}



/****************************************************************************
 * disk_set_indexed_write
 ****************************************************************************/
int
disk_set_indexed_write(
	struct disk_track		*dsk_trk,
	int				val)

	{
	return (setvalue_uchar_bit(&dsk_trk->fil_trk.flags, val, FILE_TRACK_FLAG_INDEXED_WRITE));
	}



/****************************************************************************
 * disk_set_flip_side
 ****************************************************************************/
int
disk_set_flip_side(
	struct disk_track		*dsk_trk,
	int				val)

	{
	return (setvalue_uchar_bit(&dsk_trk->fil_trk.flags, val, FILE_TRACK_FLAG_FLIP_SIDE));
	}



/****************************************************************************
 * disk_set_clock
 ****************************************************************************/
int
disk_set_clock(
	struct disk_track		*dsk_trk,
	int				clock)

	{
	if (clock == 14)      dsk_trk->fil_trk.clock = 0;
	else if (clock == 28) dsk_trk->fil_trk.clock = 1;
	else if (clock == 56) dsk_trk->fil_trk.clock = 2;
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
	return (setvalue_uchar(&dsk_trk->fil_trk.side_offset, side_offset, 0, CWTOOL_MAX_TRACK / 2));
	}



/****************************************************************************
 * disk_set_timeout_read
 ****************************************************************************/
int
disk_set_timeout_read(
	struct disk_track		*dsk_trk,
	int				timeout)

	{
	return (setvalue_ushort(&dsk_trk->fil_trk.timeout_read, timeout, CW_MIN_TIMEOUT + 1, CW_MAX_TIMEOUT - 1));
	}



/****************************************************************************
 * disk_set_timeout_write
 ****************************************************************************/
int
disk_set_timeout_write(
	struct disk_track		*dsk_trk,
	int				timeout)

	{
	return (setvalue_ushort(&dsk_trk->fil_trk.timeout_write, timeout, CW_MIN_TIMEOUT + 1, CW_MAX_TIMEOUT - 1));
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
 * disk_get_version
 ****************************************************************************/
int
disk_get_version(
	void)

	{
	static int			version;

	return (++version);
	}



/****************************************************************************
 * disk_get_name
 ****************************************************************************/
char *
disk_get_name(
	struct disk			*dsk)

	{
	debug_error_condition(dsk == NULL);
	return (dsk->name);
	}



/****************************************************************************
 * disk_get_info
 ****************************************************************************/
char *
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
	if (data != NULL) disk_sector_do_copy(dsk_sct->data, data, dsk_sct->size);
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
	disk_sector_do_copy(data, dsk_sct->data, dsk_sct->size);
	return (1);
	}



/****************************************************************************
 * disk_statistics
 ****************************************************************************/
int
disk_statistics(
	struct disk			*dsk,
	const char			*file)

	{
	struct file			fil;
	int				t;

	debug_error_condition(dsk->fil_img_l0->open == NULL);
	debug_error_condition(dsk->fil_img_l0->close == NULL);
	debug_error_condition(dsk->fil_img_l0->track_read == NULL);

	/* open file */

	dsk->fil_img_l0->open(&fil, file, FILE_MODE_READ);

	/* iterate over all defined tracks */

	for (t = 0; t < CWTOOL_MAX_TRACK; t++)
		{
		struct disk_track	*dsk_trk = &dsk->trk[t];
		unsigned char		data[CWTOOL_MAX_TRACK_SIZE] = { };
		struct fifo		ffo = FIFO_INIT(data, sizeof (data));

		if (dsk_trk->fmt_dsc == NULL) continue;
		debug_error_condition(dsk_trk->fmt_dsc->track_statistics == NULL);

		dsk->fil_img_l0->track_read(&fil, &ffo, &dsk_trk->fil_trk, t);
		dsk_trk->fmt_dsc->track_statistics(&dsk_trk->fmt, &ffo, t);
		}

	/* close file */

	dsk->fil_img_l0->close(&fil);

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
	const char			*file_src,
	const char			*file_dst)

	{
	struct disk_info		dsk_nfo = { };
	struct file			fil_src, fil_dst;
	int				t, retry = dsk_opt->retry;

	debug_error_condition(dsk->fil_img_l0->open == NULL);
	debug_error_condition(dsk->fil_img_l0->close == NULL);
	debug_error_condition(dsk->fil_img_l0->track_read == NULL);
	debug_error_condition(dsk->fil_img_l0->type == NULL);
	debug_error_condition(dsk->fil_img->open == NULL);
	debug_error_condition(dsk->fil_img->close == NULL);
	debug_error_condition(dsk->fil_img->track_write == NULL);

	/* open files */

	dsk->fil_img_l0->open(&fil_src, file_src, FILE_MODE_READ);
	dsk->fil_img->open(&fil_dst, file_dst, FILE_MODE_WRITE);

	/* if raw data is read from file, we do no retries */

	if (dsk->fil_img_l0->type(&fil_src) == FILE_TYPE_REGULAR) retry = 0;

	/* iterate over all defined tracks */

	for (t = 0; t < CWTOOL_MAX_TRACK; t++)
		{
		struct disk_track	*dsk_trk = &dsk->trk[t];
		struct disk_sector	dsk_sct[CWTOOL_MAX_SECTOR] = { };
		unsigned char		data_src[CWTOOL_MAX_TRACK_SIZE] = { };
		unsigned char		data_dst[CWTOOL_MAX_TRACK_SIZE] = { };
		struct fifo		ffo_src = FIFO_INIT(data_src, sizeof (data_src));
		struct fifo		ffo_dst = FIFO_INIT(data_dst, sizeof (data_dst));
		int			try;

		if (dsk_trk->fmt_dsc == NULL) continue;
		if (disk_sectors_init(dsk_sct, dsk_trk, &ffo_dst, 0) == 0) continue;
		debug_error_condition(dsk_trk->fmt_dsc->track_read == NULL);

		for (try = 0; try <= retry; try++)
			{
			fifo_reset(&ffo_src);
			dsk->fil_img_l0->track_read(&fil_src, &ffo_src, &dsk_trk->fil_trk, t);
			dsk_trk->fmt_dsc->track_read(&dsk_trk->fmt, &ffo_src, &ffo_dst, dsk_sct, t);
			disk_info_update(&dsk_nfo, dsk_sct, t, dsk_trk->fmt_dsc->get_sectors(&dsk_trk->fmt), try, 0);
			if (dsk_opt->info_func != NULL) dsk_opt->info_func(&dsk_nfo, 0);
			if (dsk_nfo.sectors_bad == 0) break;
			}
		disk_info_update(&dsk_nfo, dsk_sct, t, dsk_trk->fmt_dsc->get_sectors(&dsk_trk->fmt), try, 1);
		dsk->fil_img->track_write(&fil_dst, &ffo_dst, &dsk_trk->fil_trk, t);
		}
	if (dsk_opt->info_func != NULL) dsk_opt->info_func(&dsk_nfo, 1);

	/* close files */

	dsk->fil_img_l0->close(&fil_src);
	dsk->fil_img->close(&fil_dst);

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
	const char			*file_src,
	const char			*file_dst)

	{
	struct disk_info		dsk_nfo = { };
	struct file			fil_src, fil_dst;
	int				mode = (dsk_opt->flags & DISK_OPTION_FLAG_IGNORE_SIZE) ? FILE_MODE_READ2 : FILE_MODE_READ;
	int				t;

	debug_error_condition(dsk->fil_img->open == NULL);
	debug_error_condition(dsk->fil_img->close == NULL);
	debug_error_condition(dsk->fil_img->track_read == NULL);
	debug_error_condition(dsk->fil_img_l0->open == NULL);
	debug_error_condition(dsk->fil_img_l0->close == NULL);
	debug_error_condition(dsk->fil_img_l0->track_write == NULL);

	/* open files */

	dsk->fil_img->open(&fil_src, file_src, mode);
	dsk->fil_img_l0->open(&fil_dst, file_dst, FILE_MODE_WRITE);

	/* iterate over all defined tracks */

	for (t = 0; t < CWTOOL_MAX_TRACK; t++)
		{
		struct disk_track	*dsk_trk = &dsk->trk[t];
		struct disk_sector	dsk_sct[CWTOOL_MAX_SECTOR] = { };
		unsigned char		data_src[CWTOOL_MAX_TRACK_SIZE] = { };
		unsigned char		data_dst[CWTOOL_MAX_TRACK_SIZE] = { };
		struct fifo		ffo_src = FIFO_INIT(data_src, sizeof (data_src));
		struct fifo		ffo_dst = FIFO_INIT(data_dst, sizeof (data_dst));

		if (dsk_trk->fmt_dsc == NULL) continue;
		disk_sectors_init(dsk_sct, dsk_trk, &ffo_src, 1);
		debug_error_condition(dsk_trk->fmt_dsc->track_write == NULL);

		dsk->fil_img->track_read(&fil_src, &ffo_src, &dsk_trk->fil_trk, t);
		if (! dsk_trk->fmt_dsc->track_write(&dsk_trk->fmt, &ffo_src, dsk_sct, &ffo_dst, t)) error("data too long on track %d", t);
		fifo_set_rd_ofs(&ffo_src, fifo_get_wr_ofs(&ffo_src));
		dsk->fil_img_l0->track_write(&fil_dst, &ffo_dst, &dsk_trk->fil_trk, t);
		disk_info_update(&dsk_nfo, dsk_sct, t, dsk_trk->fmt_dsc->get_sectors(&dsk_trk->fmt), 0, 1);
		if (dsk_opt->info_func != NULL) dsk_opt->info_func(&dsk_nfo, 0);
		}
	if (dsk_opt->info_func != NULL) dsk_opt->info_func(&dsk_nfo, 1);

	/* close files */

	dsk->fil_img->close(&fil_src);
	dsk->fil_img_l0->close(&fil_dst);

	/* done */

	return (1);
	}
/******************************************************** Karsten Scheibler */
