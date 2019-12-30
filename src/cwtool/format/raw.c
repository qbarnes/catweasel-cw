/****************************************************************************
 ****************************************************************************
 *
 * format/raw.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>

#include "raw.h"
#include "../error.h"
#include "../debug.h"
#include "../verbose.h"
#include "../cwtool.h"
#include "../disk.h"
#include "../fifo.h"
#include "../format.h"
#include "bounds.h"




/****************************************************************************
 *
 * misc helper functions
 *
 ****************************************************************************/




/****************************************************************************
 * raw_histogram_long
 ****************************************************************************/
static int
raw_histogram_long(
	unsigned char			*data,
	int				size,
	int				track,
	int				xlated_track)

	{
	int				dist[128] = { }, d, i, max;

	for (i = max = 0; i < size; i++)
		{
		d = data[i] & 0x7f;
		dist[d]++;
		if (dist[d] > max) max = dist[d];
		}

	printf("=== track %3d[%3d], size %6d ======================================================================================================\n", track, xlated_track, size);
	for (i = 20; i-- > 0; )
		{
		printf("%5d ", (i * max) / 20);
		for (d = 0; d < 128; d++)
			{
			if (dist[d] > (i * max) / 20) printf("*");
			else printf(" ");
			}
		printf("\n");
		}
	printf("      00000000000000001111111111111111222222222222222233333333333333334444444444444444555555555555555566666666666666667777777777777777\n");
	printf("      0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\n");

	return (0);
	}



/****************************************************************************
 * raw_histogram_short
 ****************************************************************************/
static int
raw_histogram_short(
	unsigned char			*data,
	int				size,
	int				track,
	int				xlated_track)

	{
	int				dist[64] = { }, d, i, max;

	for (i = max = 0; i < size; i++)
		{
		d = (data[i] & 0x7f) / 2;
		dist[d]++;
		if (dist[d] > max) max = dist[d];
		}

	printf("=== track %3d[%3d], size %6d ======================================\n", track, xlated_track, size);
	for (i = 20; i-- > 0; )
		{
		printf("%5d ", (i * max) / 20);
		for (d = 0; d < 64; d++)
			{
			if (dist[d] > (i * max) / 20) printf("*");
			else printf(" ");
			}
		printf("\n");
		}
	printf("      0000000011111111222222223333333344444444555555556666666677777777\n");
	printf("      02468ace02468ace02468ace02468ace02468ace02468ace02468ace02468ace\n");

	return (0);
	}



/****************************************************************************
 * raw_div
 ****************************************************************************/
static int
raw_div(
	int				sum,
	int				count)

	{
	long long			s = sum;

	if (count == 0) return (0);
	s <<= 8;
	s /= (long long) count;
	return ((int) s);
	}



/****************************************************************************
 * raw_val
 ****************************************************************************/
static int
raw_val(
	struct bounds			*bnd,
	int				i)

	{
	return ((bnd[i].write >> 8) - 1);
	}



/****************************************************************************
 * raw_postcomp_lookup
 ****************************************************************************/
static void
raw_postcomp_lookup(
	struct bounds			*bnd,
	int				bnd_size,
	int				*lookup,
	int				area)

	{
	int				i, j, l, h, m, v;

	for (i = 0; i < 128; i++) lookup[i] = -1;

	for (i = 0; i < bnd_size; i++)
		{
		v = raw_val(bnd, i);
		l = v - area;
		h = v + area;
		if (i != 0)
			{
			m = (raw_val(bnd, i - 1) + v) / 2;
			l = (l < m + 1) ? m + 1 : l;
			}
		else l = (l < 0) ? 0 : l;
		if (i != bnd_size - 1)
			{
			m = (v + raw_val(bnd, i + 1)) / 2;
			h = (h > m - 1) ? m - 1 : h;
			}
		else h = (h >= 128) ? 127 : h;
		for (j = l; j <= h; j++) lookup[j] = i;
		}
	}



/****************************************************************************
 * raw_postcomp_calculate
 ****************************************************************************/
static int
raw_postcomp_calculate(
	struct bounds			*bnd,
	int				bnd_size,
	unsigned char			*data,
	char				*error,
	char				*done,
	int				len,
	int				stage,
	int				area)

	{
	int				lookup[128];
	int				d, e, i, j, p;

	/* create lookup table */

	raw_postcomp_lookup(bnd, bnd_size, lookup, area);

	/* iterate over data */

	for (i = 1, e = error[i] / 2, p = 0; i < len - 1; i++)
		{
		d = (data[i] + e) & 0x7f;
		e = error[i + 1] / 2;
		j = lookup[d];
		if ((done[i] >= stage) || (j == -1)) continue;
		d -= raw_val(bnd, j);
		error[i - 1] += d / 2;
		error[i]     -= d;
		error[i + 1] += d - (d / 2);
		done[i] = stage;
		p++;
		}
	return (p);
	}



/****************************************************************************
 * raw_postcomp_apply
 ****************************************************************************/
static int
raw_postcomp_apply(
	unsigned char			*data,
	char				*error,
	int				len)

	{
	int				e, i, p;

	for (i = p = 0; i < len; i++)
		{
		e = error[i] / 2;
		if (e == 0) continue;
		data[i] += e;
		p++;
		}
	return (p);
	}




/****************************************************************************
 *
 * functions for track handling
 *
 ****************************************************************************/




/****************************************************************************
 * raw_read_write_track
 ****************************************************************************/
static int
raw_read_write_track(
	struct fifo			*ffo_src,
	struct fifo			*ffo_dst)

	{
	int				size = fifo_get_wr_ofs(ffo_src);

	debug_error_condition(fifo_get_limit(ffo_dst) < size);
	if (fifo_copy_block(ffo_src, ffo_dst, size) == -1) return (0);
	fifo_set_flags(ffo_dst, fifo_get_flags(ffo_src));
	return (1);
	}



/****************************************************************************
 * raw_statistics
 ****************************************************************************/
static int
raw_statistics(
	union format			*fmt,
	struct fifo			*ffo_l0,
	int				track)

	{
	raw_histogram(ffo_l0, track, track);
	return (1);
	}



/****************************************************************************
 * raw_read_track
 ****************************************************************************/
static int
raw_read_track(
	union format			*fmt,
	struct fifo			*ffo_src,
	struct fifo			*ffo_dst,
	struct disk_sector		*dsk_sct,
	int				track)

	{
	return (raw_read_write_track(ffo_src, ffo_dst));
	}



/****************************************************************************
 * raw_write_track
 ****************************************************************************/
static int
raw_write_track(
	union format			*fmt,
	struct fifo			*ffo_src,
	struct disk_sector		*dsk_sct,
	struct fifo			*ffo_dst,
	int				track)

	{
	return (raw_read_write_track(ffo_src, ffo_dst));
	}




/****************************************************************************
 *
 * functions for configuration
 *
 ****************************************************************************/




/****************************************************************************
 * raw_set_defaults
 ****************************************************************************/
static void
raw_set_defaults(
	union format			*fmt)

	{
	debug(2, "setting defaults");
	}



/****************************************************************************
 * raw_set_dummy_option
 ****************************************************************************/
static int
raw_set_dummy_option(
	union format			*fmt,
	int				magic,
	int				val,
	int				ofs)

	{
	debug_error();
	return (0);
	}



/****************************************************************************
 * raw_get_sector_size
 ****************************************************************************/
static int
raw_get_sector_size(
	union format			*fmt,
	int				sector)

	{
	debug_error_condition(sector >= 0);
	return (CWTOOL_MAX_TRACK_SIZE);
	}



/****************************************************************************
 * raw_get_sectors
 ****************************************************************************/
static int
raw_get_sectors(
	union format			*fmt)

	{
	return (0);
	}



/****************************************************************************
 * raw_get_flags
 ****************************************************************************/
static int
raw_get_flags(
	union format			*fmt)

	{
	return (FORMAT_FLAG_GREEDY);
	}



/****************************************************************************
 * raw_dummy_options
 ****************************************************************************/
static struct format_option		raw_dummy_options[] =
	{
	FORMAT_OPTION_END
	};




/****************************************************************************
 *
 * used by external callers
 *
 ****************************************************************************/




/****************************************************************************
 * raw_format_desc
 ****************************************************************************/
struct format_desc			raw_format_desc =
	{
	.name             = "raw",
	.level            = 0,
	.set_defaults     = raw_set_defaults,
	.set_read_option  = raw_set_dummy_option,
	.set_write_option = raw_set_dummy_option,
	.set_rw_option    = raw_set_dummy_option,
	.get_sectors      = raw_get_sectors,
	.get_sector_size  = raw_get_sector_size,
	.get_flags        = raw_get_flags,
	.track_statistics = raw_statistics,
	.track_read       = raw_read_track,
	.track_write      = raw_write_track,
	.fmt_opt_rd       = raw_dummy_options,
	.fmt_opt_wr       = raw_dummy_options,
	.fmt_opt_rw       = raw_dummy_options
	};



/****************************************************************************
 * raw_read_lookup
 ****************************************************************************/
int
raw_read_lookup(
	struct bounds			*bnd,
	int				bnd_size,
	int				*lookup)

	{
	int				i, j, l, h;

	for (i = j = 0; i < bnd_size; i++) if (bnd[i].count > j) j = bnd[i].count;
	for (i = 0, j++; i < 128; i++) lookup[i] = j;
	for (i = 0; i < bnd_size; i++)
		{
		debug_error_condition((bnd[i].read_low > 0x7fff) || (bnd[i].read_high > 0x7fff));
		debug_error_condition(bnd[i].read_low > bnd[i].read_high);
		l = (bnd[i].read_low  + 0xff) >> 8;
		h = (bnd[i].read_high + 0xff) >> 8;
		for (j = l; j <= h; j++) lookup[j] = bnd[i].count;
		}
	return (0);
	}



/****************************************************************************
 * raw_read_counter
 ****************************************************************************/
int
raw_read_counter(
	struct fifo			*ffo_l0,
	int				*lookup)

	{
	int				i = fifo_read_byte(ffo_l0);

	if (i == -1) return (-1);
	return (lookup[i & 0x7f]);
	}



/****************************************************************************
 * raw_write_counter
 ****************************************************************************/
int
raw_write_counter(
	struct fifo			*ffo_l0,
	struct raw_counter		*raw_cnt,
	int				i)

	{
	int				clip, val, precomp;

	if ((i < 0) || (i >= raw_cnt->bnd_size))
		{
		verbose(3, "could not convert invalid bit pattern at offset %d", fifo_get_wr_ofs(ffo_l0));
		raw_cnt->invalid++;
		i = raw_cnt->bnd_size - 1;
		}
	raw_cnt->this += raw_cnt->bnd[i].write;
	if (raw_cnt->last > 0)
		{
		precomp       = raw_cnt->precomp[raw_cnt->bnd_size * raw_cnt->last_i + i];
		raw_cnt->last -= precomp;
		raw_cnt->this += precomp;
		clip = 0, val = raw_cnt->last;
		if (val < 0x0300) clip = 1, val = 0x0300;
		if (val > 0x7fff) clip = 1, val = 0x7fff;
		if (clip) error_warning("precompensation lead to invalid catweasel counter 0x%04x at offset %d", raw_cnt->last, fifo_get_wr_ofs(ffo_l0));
		if (fifo_write_byte(ffo_l0, val >> 8) == -1) return (-1);
		}
	raw_cnt->last   = raw_cnt->this;
	raw_cnt->this   &= 0xff;
	raw_cnt->last_i = i; 
	return (0);
	}



/****************************************************************************
 * raw_postcomp
 ****************************************************************************/
int
raw_postcomp(
	struct fifo			*ffo,
	struct bounds			*bnd,
	int				bnd_size)

	{
	unsigned char			*data = fifo_get_data(ffo);
	int				len   = fifo_get_wr_ofs(ffo);
	char				error[CWTOOL_MAX_TRACK_SIZE] = { };
	char				done[CWTOOL_MAX_TRACK_SIZE] = { };
	int				stage, area;

	/*
	 * increasing number of stages produces sometimes very nice
	 * histograms, but will not improve readability of a disk
	 */

	if (bnd_size < 2) return (0);
	for (stage = 1; stage < 2; stage++) for (area = 2; area < 16; area++)
		{
		raw_postcomp_calculate(bnd, bnd_size, data, error, done, len, stage, area);
		}
	return (raw_postcomp_apply(data, error, len));
	}



/****************************************************************************
 * raw_read
 ****************************************************************************/
int
raw_read(
	struct fifo			*ffo_l0,
	struct fifo			*ffo_l1,
	struct bounds			*bnd,
	int				bnd_size)

	{
	int				i, lookup[128];

	/* create lookup table */

	raw_read_lookup(bnd, bnd_size, lookup);

	/* convert raw counter values to raw bits */

	debug(3, "raw_read ffo_l0->wr_ofs = %d, ffo_l1->limit = %d", fifo_get_wr_ofs(ffo_l0), fifo_get_limit(ffo_l1));
	while (1)
		{
		i = raw_read_counter(ffo_l0, lookup);
		if (i == -1) break;
		if (fifo_write_count(ffo_l1, i) == -1) debug_error();
		}
	fifo_write_flush(ffo_l1);
	debug(3, "raw_read ffo_l0->wr_ofs = %d, ffo_l1->wr_bitofs = %d", fifo_get_wr_ofs(ffo_l0), fifo_get_wr_bitofs(ffo_l1));
	return (0);
	}



/****************************************************************************
 * raw_write
 ****************************************************************************/
int
raw_write(
	struct fifo			*ffo_l1,
	struct fifo			*ffo_l0,
	struct bounds			*bnd,
	short				*precomp,
	int				bnd_size)

	{
	struct raw_counter		raw_cnt = RAW_COUNTER_INIT(bnd, precomp, bnd_size);
	int				i, j, lookup[128];

	/* create lookup table */

	for (i = 0; i < 128; i++) lookup[i] = -1;
	for (i = 0; i < bnd_size; i++)
		{
		j = bnd[i].count;
		debug_error_condition(j >= 128);
		lookup[j] = i;
		}

	/* convert raw bits to raw counter values and do precompensation */

	debug(3, "raw_write ffo_l1->wr_bitofs = %d, ffo_l0->limit = %d", fifo_get_wr_bitofs(ffo_l1), fifo_get_limit(ffo_l0));
	fifo_set_flags(ffo_l0, FIFO_FLAG_WRITABLE);
	while (1)
		{
		i = fifo_read_count(ffo_l1);
		if (i == -1) break;
		if (i >= 128) i = 127;
		if (raw_write_counter(ffo_l0, &raw_cnt, lookup[i]) == -1) return (-1);
		}
	if (raw_cnt.invalid > 0) error_warning("could not convert %d invalid bit patterns", raw_cnt.invalid);
	debug(3, "raw_write ffo_l1->rd_bitofs = %d, ffo_l0->wr_ofs = %d", fifo_get_rd_bitofs(ffo_l1), fifo_get_wr_ofs(ffo_l0));

	return (0);
	}



/****************************************************************************
 * raw_histogram
 ****************************************************************************/
int
raw_histogram(
	struct fifo			*ffo,
	int				track,
	int				xlated_track)

	{
	unsigned char			*data = fifo_get_data(ffo);
	int				size = fifo_get_wr_ofs(ffo);

	if (verbose_level >= -2) return (raw_histogram_long(data, size, track, xlated_track));
	return (raw_histogram_short(data, size, track, xlated_track));
	}



/****************************************************************************
 * raw_postcomp_histogram
 ****************************************************************************/
int
raw_postcomp_histogram(
	struct fifo			*ffo,
	struct bounds			*bnd,
	int				bnd_size,
	int				track,
	int				xlated_track)

	{
	int				p;

	if (verbose_level >= 0)
		{
		p = raw_postcomp(ffo, bnd, bnd_size);
		printf("postcompensated %d values\n", p);
		raw_histogram(ffo, track, xlated_track);
		raw_precomp_statistics(ffo, bnd, bnd_size);
		}
	return (0);
	}



/****************************************************************************
 * raw_precomp_statistics
 ****************************************************************************/
int
raw_precomp_statistics(
	struct fifo			*ffo,
	struct bounds			*bnd,
	int				bnd_size)

	{
	unsigned char			*data = fifo_get_data(ffo);
	int				size = fifo_get_wr_ofs(ffo);
	int				c1, c2, d1, d2, i, j, lookup[128];
	int				sum1[8][8] = { }, sum2[8][8] = { }, count[8][8] = { };
	int				sum = 0, bits1 = 0, bits2 = 0;

	if (verbose_level < -1) return (0);

	/* create lookup table */

	debug_error_condition(bnd_size > 8);
	for (i = 0; i < 128; i++) lookup[i] = -1;
	for (i = 0; i < bnd_size; i++)
		{
		debug_error_condition((bnd[i].read_low > 0x7fff) || (bnd[i].read_high > 0x7fff));
		debug_error_condition(bnd[i].read_low > bnd[i].read_high);
		for (j = bnd[i].read_low >> 8; j <= bnd[i].read_high >> 8; j++) lookup[j] = i;
		}

	/* iterate over data */

	for (i = 0, c1 = -1, d1 = 0; i < size; i++, c1 = c2, d1 = d2)
		{
		d2 = data[i] & 0x7f;
		c2 = lookup[d2];
		if ((c1 == -1) || (c2 == -1)) continue;
		sum1[c1][c2] += d1;
		sum2[c1][c2] += d2;
		count[c1][c2]++;
		sum   += d2;
		bits1 += c2;
		bits2++;
		}

	/* print out precomp statistics */

	for (i = 0; i < bnd_size; i++)
		{
		printf("average last/this:");
		for (j = 0; j < bnd_size; j++) printf(" 0x%04x/0x%04x", raw_div(sum1[i][j], count[i][j]), raw_div(sum2[i][j], count[i][j]));
		printf("\n");
		}

	/* print out suggested write bounds */

	i = raw_div(sum , bits1 + bits2);
	printf("suggested FM  write bounds: 0x%04x 0x%04x\n", i + 0x100, 2 * i + 0x100);
	printf("suggested GCR write bounds: 0x%04x 0x%04x 0x%04x\n", i + 0x100, 2 * i + 0x100, 3 * i + 0x100);
	i = raw_div(sum , bits1 + 2 * bits2);
	printf("suggested MFM write bounds: 0x%04x 0x%04x 0x%04x\n", 2 * i + 0x100, 3 * i + 0x100, 4 * i + 0x100);

	return (0);
	}
/******************************************************** Karsten Scheibler */