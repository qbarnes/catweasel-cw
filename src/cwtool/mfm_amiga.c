/****************************************************************************
 ****************************************************************************
 *
 * mfm_amiga.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>

#include "mfm_amiga.h"
#include "error.h"
#include "debug.h"
#include "verbose.h"
#include "cwtool.h"
#include "disk.h"
#include "fifo.h"
#include "format.h"
#include "mfm.h"
#include "mfmfm.h"
#include "raw.h"
#include "setvalue.h"




/****************************************************************************
 *
 * functions for sector and track handling
 *
 ****************************************************************************/




#define DATA_SIZE			540
#define FLAG_IGNORE_CHECKSUMS		(1 << 0)
#define FLAG_IGNORE_TRACK_MISMATCH	(1 << 1)
#define FLAG_IGNORE_FORMAT_BYTE		(1 << 2)



/****************************************************************************
 * mfm_amiga_shuffle
 ****************************************************************************/
static void
mfm_amiga_shuffle(
	unsigned char			*data,
	int				len)

	{
	unsigned char			tmp[DATA_SIZE];
	int				i, j, k;

	debug_error_condition((len % 2) != 0);
	debug_error_condition(len > sizeof (tmp));
	for (i = j = 0, k = len / 2; i < len; i += 2, j++, k++)
		{
		tmp[j] = (mfm_decode_table[(data[i] >> 1) & 0x55] << 4) |
			mfm_decode_table[(data[i + 1] >> 1) & 0x55];
		tmp[k] = (mfm_decode_table[data[i]        & 0x55] << 4) |
			mfm_decode_table[data[i + 1]        & 0x55];
		}
	for (i = 0; i < len; i++) data[i] = tmp[i];
	}



/****************************************************************************
 * mfm_amiga_unshuffle
 ****************************************************************************/
static void
mfm_amiga_unshuffle(
	unsigned char			*data,
	int				len)

	{
	unsigned char			tmp[DATA_SIZE];
	int				i, j, k;

	debug_error_condition((len % 2) != 0);
	debug_error_condition(len > sizeof (tmp));
	for (i = k = 0, j = len / 2; k < len; i++, j++)
		{
		tmp[k++] = (mfm_encode_table[data[i] >>   4] << 1) | mfm_encode_table[data[j] >>   4];
		tmp[k++] = (mfm_encode_table[data[i] & 0x0f] << 1) | mfm_encode_table[data[j] & 0x0f];
		}
	for (i = 0; i < len; i++) data[i] = tmp[i];
	}



/****************************************************************************
 * mfm_amiga_checksum
 ****************************************************************************/
static unsigned long
mfm_amiga_checksum(
	unsigned char			*data,
	int				len)

	{
	int				i;
	unsigned long			val;

	debug_error_condition((len % 4) != 0);
	for (val = i = 0; i < len; i += 4) val ^= mfm_read_ulong_le(&data[i]);
	val = ((val >> 1) & 0x55555555) ^ (val & 0x55555555);
	return (val);
	}



/****************************************************************************
 * mfm_amiga_read_sector2
 ****************************************************************************/
static int
mfm_amiga_read_sector2(
	struct fifo			*ffo_l1,
	struct mfm_amiga		*mfm_amg,
	struct disk_error		*dsk_err,
	unsigned char			*data)

	{
	int				bitofs;

	*dsk_err = (struct disk_error) { };
	if (mfm_read_sync(ffo_l1, mfm_amg->rw.sync_value, mfm_amg->rw.sync_length) == -1) return (-1);
	bitofs = fifo_get_rd_bitofs(ffo_l1);
	if (mfm_read_bytes(ffo_l1, dsk_err, data, DATA_SIZE) == -1) return (-1);
	mfm_amiga_unshuffle(data, 4);
	mfm_amiga_unshuffle(&data[4], 16);
	mfm_amiga_unshuffle(&data[20], 4);
	mfm_amiga_unshuffle(&data[24], 4);
	mfm_amiga_unshuffle(&data[28], 512);
	verbose(2, "rewinding to bit offset %d", bitofs);
	fifo_set_rd_bitofs(ffo_l1, bitofs);
	return (1);
	}



/****************************************************************************
 * mfm_amiga_write_sector2
 ****************************************************************************/
static int
mfm_amiga_write_sector2(
	struct fifo			*ffo_l1,
	struct mfm_amiga		*mfm_amg,
	unsigned char			*data)

	{
	if (mfm_write_fill(ffo_l1, mfm_amg->wr.fill_value, mfm_amg->wr.fill_length) == -1) return (-1);
	if (mfm_write_sync(ffo_l1, mfm_amg->rw.sync_value, mfm_amg->rw.sync_length) == -1) return (-1);
	mfm_amiga_shuffle(data, 4);
	mfm_amiga_shuffle(&data[4], 16);
	mfm_amiga_shuffle(&data[20], 4);
	mfm_amiga_shuffle(&data[24], 4);
	mfm_amiga_shuffle(&data[28], 512);
	if (mfm_write_bytes(ffo_l1, data, DATA_SIZE) == -1) return (-1);
	return (1);
	}



/****************************************************************************
 * mfm_amiga_read_sector
 ****************************************************************************/
static int
mfm_amiga_read_sector(
	struct fifo			*ffo_l1,
	struct mfm_amiga		*mfm_amg,
	struct disk_sector		*dsk_sct,
	int				track)

	{
	struct disk_error		dsk_err;
	unsigned char			data[DATA_SIZE];
	int				result, sector;

	if (mfm_amiga_read_sector2(ffo_l1, mfm_amg, &dsk_err, data) == -1) return (-1);

	/* accept only valid sector numbers */

	sector = data[2];
	if (sector >= mfm_amg->rw.sectors)
		{
		verbose(1, "sector %d out of range", sector);
		return (0);
		}
	verbose(1, "got sector %d", sector);

	/* check sector quality */

	result = format_compare2("header xor checksum: got 0x%08x, expected: 0x%08x", mfm_read_ulong_le(&data[20]), mfm_amiga_checksum(data, 20));
	result += format_compare2("data xor checksum: got 0x%08x, expected: 0x%08x", mfm_read_ulong_le(&data[24]), mfm_amiga_checksum(&data[28], 512));
	if (result > 0) verbose(2, "checksum error on sector %d", sector);
	if (mfm_amg->rd.flags & FLAG_IGNORE_CHECKSUMS) disk_warning_add(&dsk_err, result);
	else disk_error_add(&dsk_err, DISK_ERROR_FLAG_CHECKSUM, result);

	result = format_compare2("track: got %d, expected %d", data[1], track);
	if (result > 0) verbose(2, "track mismatch on sector %d", sector);
	if (mfm_amg->rd.flags & FLAG_IGNORE_TRACK_MISMATCH) disk_warning_add(&dsk_err, result);
	else disk_error_add(&dsk_err, DISK_ERROR_FLAG_NUMBERING, result);

	result = format_compare2("format_byte: got 0x%02x, expected 0x%02x", data[0], mfm_amg->rw.format_byte);
	if (result > 0) verbose(2, "wrong format_byte on sector %d", sector);
	if (mfm_amg->rd.flags & FLAG_IGNORE_FORMAT_BYTE) disk_warning_add(&dsk_err, result);
	else disk_error_add(&dsk_err, DISK_ERROR_FLAG_ID, result);

	/*
	 * if the found sector is of better quality than the current one
	 * then take the data
	 */

	disk_set_sector_number(&dsk_sct[sector], sector);
	disk_sector_read(&dsk_sct[sector], &dsk_err, &data[28]);
	return (1);
	}



/****************************************************************************
 * mfm_amiga_write_sector
 ****************************************************************************/
static int
mfm_amiga_write_sector(
	struct fifo			*ffo_l1,
	struct mfm_amiga		*mfm_amg,
	struct disk_sector		*dsk_sct,
	int				track)

	{
	unsigned char			data[DATA_SIZE];
	int				i, sector = disk_get_sector_number(dsk_sct);

	verbose(1, "writing sector %d", sector);
	data[0] = mfm_amg->rw.format_byte;
	data[1] = track;
	data[2] = sector;
	data[3] = mfm_amg->rw.sectors - sector;
	for (i = 4; i < 20; i++) data[i] = 0;
	mfm_write_ulong_le(&data[20], mfm_amiga_checksum(data, 20));
	disk_sector_write(&data[28], dsk_sct);
	mfm_write_ulong_le(&data[24], mfm_amiga_checksum(&data[28], 512));
	return (mfm_amiga_write_sector2(ffo_l1, mfm_amg, data));
	}



/****************************************************************************
 * mfm_amiga_statistics
 ****************************************************************************/
static int
mfm_amiga_statistics(
	union format			*fmt,
	struct fifo			*ffo_l0,
	int				track)

	{
	raw_histogram(ffo_l0, track);
	raw_precomp_statistics(ffo_l0, fmt->mfm_amg.rw.bnd, 3);
	return (1);
	}



/****************************************************************************
 * mfm_amiga_read_track
 ****************************************************************************/
static int
mfm_amiga_read_track(
	union format			*fmt,
	struct fifo			*ffo_l0,
	struct fifo			*ffo_l3,
	struct disk_sector		*dsk_sct,
	int				track)

	{
	unsigned char			data[CWTOOL_MAX_TRACK_SIZE];
	struct fifo			ffo_l1 = FIFO_INIT(data, sizeof (data));

	raw_read(ffo_l0, &ffo_l1, fmt->mfm_amg.rw.bnd, 3);
	while (mfm_amiga_read_sector(&ffo_l1, &fmt->mfm_amg, dsk_sct, track) != -1) ;
	return (1);
	}



/****************************************************************************
 * mfm_amiga_write_track
 ****************************************************************************/
static int
mfm_amiga_write_track(
	union format			*fmt,
	struct fifo			*ffo_l3,
	struct disk_sector		*dsk_sct,
	struct fifo			*ffo_l0,
	int				track)

	{
	unsigned char			data[CWTOOL_MAX_TRACK_SIZE];
	struct fifo			ffo_l1 = FIFO_INIT(data, sizeof (data));
	int				i;

	if (mfm_write_fill(&ffo_l1, fmt->mfm_amg.wr.prolog_value, fmt->mfm_amg.wr.prolog_length) == -1) return (0);
	for (i = 0; i < fmt->mfm_amg.rw.sectors; i++) if (mfm_amiga_write_sector(&ffo_l1, &fmt->mfm_amg, &dsk_sct[i], track) == -1) return (0);
	fifo_set_rd_ofs(ffo_l3, fifo_get_wr_ofs(ffo_l3));
	if (mfm_write_fill(&ffo_l1, fmt->mfm_amg.wr.epilog_value, fmt->mfm_amg.wr.epilog_length) == -1) return (0);
	fifo_write_flush(&ffo_l1);
	if (raw_write(&ffo_l1, ffo_l0, fmt->mfm_amg.rw.bnd, fmt->mfm_amg.wr.precomp, 3) == -1) return (0);
	return (1);
	}




/****************************************************************************
 *
 * functions for configuration
 *
 ****************************************************************************/




#define MAGIC_IGNORE_CHECKSUMS		1
#define MAGIC_IGNORE_TRACK_MISMATCH	2
#define MAGIC_IGNORE_FORMAT_BYTE	3
#define MAGIC_PROLOG_LENGTH		4
#define MAGIC_PROLOG_VALUE		5
#define MAGIC_EPILOG_LENGTH		6
#define MAGIC_EPILOG_VALUE		7
#define MAGIC_FILL_LENGTH		8
#define MAGIC_FILL_VALUE		9
#define MAGIC_PRECOMP			10
#define MAGIC_SECTORS			11
#define MAGIC_SYNC_LENGTH		12
#define MAGIC_SYNC_VALUE		13
#define MAGIC_FORMAT_BYTE		14
#define MAGIC_BOUNDS			15



/****************************************************************************
 * mfm_amiga_set_defaults
 ****************************************************************************/
static void
mfm_amiga_set_defaults(
	union format			*fmt)

	{
	const static struct mfm_amiga	mfm_amg =
		{
		.rd =
			{
			.flags = 0
			},
		.wr =
			{
			.prolog_length = 0,
			.prolog_value  = 0x00,
			.epilog_length = 1,
			.epilog_value  = 0x00,
			.fill_length   = 2,
			.fill_value    = 0x00,
			.precomp       = { }
			},
		.rw =
			{
			.sectors     = 11,
			.sync_length = 2,
			.sync_value  = 0x4489,
			.format_byte = 0xff,
			.bnd         =
				{
				BOUNDS(0x0800, 0x1b52, 0x2300, 1),
				BOUNDS(0x2400, 0x297c, 0x3000, 2),
				BOUNDS(0x3100, 0x37a5, 0x4000, 3)
				}
			}
		};

	debug(2, "setting defaults");
	fmt->mfm_amg = mfm_amg;
	}



/****************************************************************************
 * mfm_amiga_set_read_option
 ****************************************************************************/
static int
mfm_amiga_set_read_option(
	union format			*fmt,
	int				magic,
	int				val,
	int				ofs)

	{
	debug(2, "setting read option magic = %d, val = %d, ofs = %d", magic, val, ofs);
	if (magic == MAGIC_IGNORE_CHECKSUMS)      return (setvalue_uchar_bit(&fmt->mfm_amg.rd.flags, val, FLAG_IGNORE_CHECKSUMS));
	if (magic == MAGIC_IGNORE_TRACK_MISMATCH) return (setvalue_uchar_bit(&fmt->mfm_amg.rd.flags, val, FLAG_IGNORE_TRACK_MISMATCH));
	debug_error_condition(magic != MAGIC_IGNORE_FORMAT_BYTE);
	return (setvalue_uchar_bit(&fmt->mfm_amg.rd.flags, val, FLAG_IGNORE_FORMAT_BYTE));
	}



/****************************************************************************
 * mfm_amiga_set_write_option
 ****************************************************************************/
static int
mfm_amiga_set_write_option(
	union format			*fmt,
	int				magic,
	int				val,
	int				ofs)

	{
	debug(2, "setting write option magic = %d, val = %d, ofs = %d", magic, val, ofs);
	if (magic == MAGIC_PROLOG_LENGTH) return (setvalue_ushort(&fmt->mfm_amg.wr.prolog_length, val, 0, 0xffff));
	if (magic == MAGIC_PROLOG_VALUE)  return (setvalue_uchar(&fmt->mfm_amg.wr.prolog_value, val, 0, 0xff));
	if (magic == MAGIC_EPILOG_LENGTH) return (setvalue_ushort(&fmt->mfm_amg.wr.epilog_length, val, 1, 0xffff));
	if (magic == MAGIC_EPILOG_VALUE)  return (setvalue_uchar(&fmt->mfm_amg.wr.epilog_value, val, 0, 0xff));
	if (magic == MAGIC_FILL_LENGTH)   return (setvalue_uchar(&fmt->mfm_amg.wr.fill_length, val, 1, 0xff));
	if (magic == MAGIC_FILL_VALUE)    return (setvalue_uchar(&fmt->mfm_amg.wr.fill_value, val, 0, 0xff));
	debug_error_condition(magic != MAGIC_PRECOMP);
	return (setvalue_short(&fmt->mfm_amg.wr.precomp[ofs], val, -0x4000, 0x4000));
	}



/****************************************************************************
 * mfm_amiga_set_rw_option
 ****************************************************************************/
static int
mfm_amiga_set_rw_option(
	union format			*fmt,
	int				magic,
	int				val,
	int				ofs)

	{
	debug(2, "setting rw option magic = %d, val = %d, ofs = %d", magic, val, ofs);
	if (magic == MAGIC_SECTORS)     return (setvalue_uchar(&fmt->mfm_amg.rw.sectors, val, 1, CWTOOL_MAX_SECTOR));
	if (magic == MAGIC_SYNC_LENGTH) return (setvalue_uchar(&fmt->mfm_amg.rw.sync_length, val, 0, 0xff));
	if (magic == MAGIC_SYNC_VALUE)  return (setvalue_ushort(&fmt->mfm_amg.rw.sync_value, val, 0, 0xffff));
	if (magic == MAGIC_FORMAT_BYTE) return (setvalue_uchar(&fmt->mfm_amg.rw.format_byte, val, 0, 0xff));
	debug_error_condition(magic != MAGIC_BOUNDS);
	return (setvalue_bounds(fmt->mfm_amg.rw.bnd, val, ofs));
	}



/****************************************************************************
 * mfm_amiga_get_sector_size
 ****************************************************************************/
static int
mfm_amiga_get_sector_size(
	union format			*fmt,
	int				sector)

	{
	debug_error_condition(sector >= fmt->mfm_amg.rw.sectors);
	if (sector < 0) return (512 * fmt->mfm_amg.rw.sectors);
	return (512);
	}



/****************************************************************************
 * mfm_amiga_get_sectors
 ****************************************************************************/
static int
mfm_amiga_get_sectors(
	union format			*fmt)

	{
	return (fmt->mfm_amg.rw.sectors);
	}



/****************************************************************************
 * mfm_amiga_read_options
 ****************************************************************************/
static struct format_option		mfm_amiga_read_options[] =
	{
	FORMAT_OPTION("ignore_checksums",      MAGIC_IGNORE_CHECKSUMS,      1),
	FORMAT_OPTION("ignore_track_mismatch", MAGIC_IGNORE_TRACK_MISMATCH, 1),
	FORMAT_OPTION("ignore_format_byte",    MAGIC_IGNORE_FORMAT_BYTE,    1),
	FORMAT_OPTION(NULL, 0, 0)
	};



/****************************************************************************
 * mfm_amiga_write_options
 ****************************************************************************/
static struct format_option		mfm_amiga_write_options[] =
	{
	FORMAT_OPTION("prolog_length", MAGIC_PROLOG_LENGTH, 1),
	FORMAT_OPTION("prolog_value",  MAGIC_PROLOG_VALUE,  1),
	FORMAT_OPTION("epilog_length", MAGIC_EPILOG_LENGTH, 1),
	FORMAT_OPTION("epilog_value",  MAGIC_EPILOG_VALUE,  1),
	FORMAT_OPTION("fill_length",   MAGIC_FILL_LENGTH,   1),
	FORMAT_OPTION("fill_value",    MAGIC_FILL_VALUE,    1),
	FORMAT_OPTION("precomp",       MAGIC_PRECOMP,       9),
	FORMAT_OPTION(NULL, 0, 0)
	};



/****************************************************************************
 * mfm_amiga_rw_options
 ****************************************************************************/
static struct format_option		mfm_amiga_rw_options[] =
	{
	FORMAT_OPTION("sectors",     MAGIC_SECTORS,     1),
	FORMAT_OPTION("sync_length", MAGIC_SYNC_LENGTH, 1),
	FORMAT_OPTION("sync_value",  MAGIC_SYNC_LENGTH, 1),
	FORMAT_OPTION("format_byte", MAGIC_FORMAT_BYTE, 1),
	FORMAT_OPTION("bounds",      MAGIC_BOUNDS,      9),
	FORMAT_OPTION(NULL, 0, 0)
	};




/****************************************************************************
 *
 * used by external callers
 *
 ****************************************************************************/




/****************************************************************************
 * mfm_amiga_format_desc
 ****************************************************************************/
struct format_desc			mfm_amiga_format_desc =
	{
	.name             = "mfm_amiga",
	.level            = 3,
	.set_defaults     = mfm_amiga_set_defaults,
	.set_read_option  = mfm_amiga_set_read_option,
	.set_write_option = mfm_amiga_set_write_option,
	.set_rw_option    = mfm_amiga_set_rw_option,
	.get_sectors      = mfm_amiga_get_sectors,
	.get_sector_size  = mfm_amiga_get_sector_size,
	.track_statistics = mfm_amiga_statistics,
	.track_read       = mfm_amiga_read_track,
	.track_write      = mfm_amiga_write_track,
	.fmt_opt_rd       = mfm_amiga_read_options,
	.fmt_opt_wr       = mfm_amiga_write_options,
	.fmt_opt_rw       = mfm_amiga_rw_options
	};
/******************************************************** Karsten Scheibler */
