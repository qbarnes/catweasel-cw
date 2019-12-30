/****************************************************************************
 ****************************************************************************
 *
 * format.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_FORMAT_H
#define CWTOOL_FORMAT_H

#include "format/raw.h"
#include "format/gcr_g64.h"
#include "format/gcr_cbm.h"
#include "format/gcr_apple.h"
#include "format/fm_nec765.h"
#include "format/mfm_nec765.h"
#include "format/mfm_amiga.h"
#include "format/tbe_cw.h"
#include "format/fill.h"

union format
	{
	struct gcr_g64			gcr_g64;
	struct gcr_cbm			gcr_cbm;
	struct gcr_apple		gcr_apl;
	struct fm_nec765		fm_nec;
	struct mfm_nec765		mfm_nec;
	struct mfm_amiga		mfm_amg;
	struct tbe_cw			tbe_cw;
	struct fill			fll;
	};

#define FORMAT_FLAG_NONE		0
#define FORMAT_FLAG_GREEDY		(1 << 0)

#define FORMAT_OPTION_TYPE_BOOLEAN	1
#define FORMAT_OPTION_TYPE_INTEGER	2
#define FORMAT_OPTION_BOOLEAN(n, m, p)	{ .name = n, .magic = m, .params = p, .type = FORMAT_OPTION_TYPE_BOOLEAN }
#define FORMAT_OPTION_INTEGER(n, m, p)	{ .name = n, .magic = m, .params = p, .type = FORMAT_OPTION_TYPE_INTEGER }
#define FORMAT_OPTION_END		{ .name = NULL, .magic = 0, .params = 0, .type = 0 }

struct format_option
	{
	char				*name;
	int				magic;
	int				params;
	int				type;
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
	int				(*get_flags)(union format *);
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
