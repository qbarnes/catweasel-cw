/****************************************************************************
 ****************************************************************************
 *
 * format/fill.c
 *
 ****************************************************************************
 ****************************************************************************/





/*
 * UGLY: cosmetical: to consolidate naming it would be better to prepend
 *       format_ to each function and FORMAT_ for defines for all files in
 *       this directory
 */



#include <stdio.h>
#include <string.h>

#include "error.h"
#include "debug.h"
#include "verbose.h"
#include "cwtool.h"
#include "disk.h"
#include "fifo.h"
#include "format.h"
#include "format/raw.h"
#include "format/setvalue.h"




/****************************************************************************
 *
 * functions for sector and track handling
 *
 ****************************************************************************/




/****************************************************************************
 * fill_statistics
 ****************************************************************************/
static int
fill_statistics(
	union format			*fmt,
	struct fifo			*ffo_l0,
	int				track)

	{
	raw_histogram(ffo_l0, track);
	return (1);
	}



/****************************************************************************
 * fill_read_track
 ****************************************************************************/
static int
fill_read_track(
	union format			*fmt,
	struct fifo			*ffo_l0,
	struct fifo			*ffo_l3,
	struct disk_sector		*dsk_sct,
	int				track)

	{
	debug_error();
	return (0);
	}



/****************************************************************************
 * fill_write_track
 ****************************************************************************/
static int
fill_write_track(
	union format			*fmt,
	struct fifo			*ffo_l3,
	struct disk_sector		*dsk_sct,
	struct fifo			*ffo_l0,
	int				track)

	{
	unsigned char			*data = fifo_get_data(ffo_l0);
	unsigned char			val = fmt->fll.wr.fill_value;
	int				len = fmt->fll.wr.fill_length;

	debug_error_condition(len > fifo_get_limit(ffo_l0));
	memset(data, val, len);
	fifo_set_wr_ofs(ffo_l0, len);
	fifo_set_flags(ffo_l0, FIFO_FLAG_WRITABLE);
	return (1);
	}




/****************************************************************************
 *
 * functions for configuration
 *
 ****************************************************************************/




#define MAGIC_FILL_VALUE		1
#define MAGIC_FILL_LENGTH		2



/****************************************************************************
 * fill_set_defaults
 ****************************************************************************/
static void
fill_set_defaults(
	union format			*fmt)

	{
	const static struct fill	fll =
		{
		.wr =
			{
			.fill_value  = 0x78,
			.fill_length = CWTOOL_MAX_TRACK_SIZE
			}
		};

	debug(2, "setting defaults");
	fmt->fll = fll;
	}



/****************************************************************************
 * fill_set_write_option
 ****************************************************************************/
static int
fill_set_write_option(
	union format			*fmt,
	int				magic,
	int				val,
	int				ofs)

	{
	debug(2, "setting write option magic = %d, val = %d, ofs = %d", magic, val, ofs);
	if (magic == MAGIC_FILL_VALUE) return (setvalue_uchar(&fmt->fll.wr.fill_value, val, 3, 0x7f));
	debug_error_condition(magic != MAGIC_FILL_LENGTH);
	return (setvalue_uint(&fmt->fll.wr.fill_length, val, CWTOOL_MIN_TRACK_SIZE, CWTOOL_MAX_TRACK_SIZE));
	}



/****************************************************************************
 * fill_set_dummy_option
 ****************************************************************************/
static int
fill_set_dummy_option(
	union format			*fmt,
	int				magic,
	int				val,
	int				ofs)

	{
	debug_error();
	return (0);
	}



/****************************************************************************
 * fill_get_sector_size
 ****************************************************************************/
static int
fill_get_sector_size(
	union format			*fmt,
	int				sector)

	{
	debug_error_condition(sector != -1);
	return (0);
	}



/****************************************************************************
 * fill_get_sectors
 ****************************************************************************/
static int
fill_get_sectors(
	union format			*fmt)

	{
	return (0);
	}



/****************************************************************************
 * fill_get_flags
 ****************************************************************************/
static int
fill_get_flags(
	union format			*fmt)

	{
	return (FORMAT_FLAG_NONE);
	}



/****************************************************************************
 * fill_write_options
 ****************************************************************************/
static struct format_option		fill_write_options[] =
	{
	FORMAT_OPTION_INTEGER("fill_value",  MAGIC_FILL_VALUE,  1),
	FORMAT_OPTION_INTEGER("fill_length", MAGIC_FILL_LENGTH, 1),
	FORMAT_OPTION_END
	};



/****************************************************************************
 * fill_dummy_options
 ****************************************************************************/
static struct format_option		fill_dummy_options[] =
	{
	FORMAT_OPTION_END
	};




/****************************************************************************
 *
 * used by external callers
 *
 ****************************************************************************/




/****************************************************************************
 * fill_format_desc
 ****************************************************************************/
struct format_desc			fill_format_desc =
	{
	.name             = "fill",
	.level            = -1,
	.set_defaults     = fill_set_defaults,
	.set_read_option  = fill_set_dummy_option,
	.set_write_option = fill_set_write_option,
	.set_rw_option    = fill_set_dummy_option,
	.get_sectors      = fill_get_sectors,
	.get_sector_size  = fill_get_sector_size,
	.get_flags        = fill_get_flags,
	.track_statistics = fill_statistics,
	.track_read       = fill_read_track,
	.track_write      = fill_write_track,
	.fmt_opt_rd       = fill_dummy_options,
	.fmt_opt_wr       = fill_write_options,
	.fmt_opt_rw       = fill_dummy_options
	};
/******************************************************** Karsten Scheibler */
