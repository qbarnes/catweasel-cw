/****************************************************************************
 ****************************************************************************
 *
 * format.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_FORMAT_H
#define CWTOOL_FORMAT_H

#include "gcr_cbm.h"
#include "gcr_apple.h"
#include "fm_nec765.h"
#include "mfm_nec765.h"
#include "mfm_amiga.h"
#include "tbe_cw.h"
#include "fill.h"

union format
	{
	struct gcr_cbm			gcr_cbm;
	struct gcr_apple		gcr_apl;
	struct fm_nec765		fm_nec;
	struct mfm_nec765		mfm_nec;
	struct mfm_amiga		mfm_amg;
	struct tbe_cw			tbe_cw;
	struct fill			fll;
	};

#define FORMAT_OPTION(n, m, p)		{ .name = n, .magic = m, .params = p }

struct format_option
	{
	char				*name;
	int				magic;
	int				params;
	};

struct fifo;
struct disk_sector;

struct format_desc
	{
	char				*name;
	int				level;
	void				(*set_defaults)(union format *);
	int				(*set_read_option)(union format *, int, int, int);
	int				(*set_write_option)(union format *, int, int, int);
	int				(*set_rw_option)(union format *, int, int, int);
	int				(*get_sectors)(union format *);
	int				(*get_sector_size)(union format *, int);
	int				(*track_statistics)(union format *, struct fifo *, int);
	int				(*track_read)(union format *, struct fifo *, struct fifo *, struct disk_sector *, int);
	int				(*track_write)(union format *, struct fifo *, struct disk_sector *, struct fifo *, int);
	struct format_option		*fmt_opt_rd;
	struct format_option		*fmt_opt_wr;
	struct format_option		*fmt_opt_rw;
	};

extern struct format_desc		*format_search_desc(const char *);
extern struct format_option		*format_search_option(struct format_option *, const char *);
extern int				format_compare2(const char *, unsigned long, unsigned long);
extern int				format_compare3(const char *, unsigned long, unsigned long, unsigned long);



#endif /* !CWTOOL_FORMAT_H */
/******************************************************** Karsten Scheibler */
