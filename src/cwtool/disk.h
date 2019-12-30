/****************************************************************************
 ****************************************************************************
 *
 * disk.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_DISK_H
#define CWTOOL_DISK_H

#include "cwtool.h"
#include "image.h"
#include "format.h"

#define DISK_TRACK_INIT			(struct disk_track) { .img_trk = IMAGE_TRACK_INIT(500) }

struct disk_track
	{
	struct image_track		img_trk;
	struct format_desc		*fmt_dsc;
	unsigned char			skew;
	unsigned char			interleave;
	unsigned char			reserved[2];
	union format			fmt;
	};

#define DISK_INIT(v)			(struct disk) { .version = v }

struct disk
	{
	char				name[CWTOOL_MAX_NAME_LEN];
	char				info[CWTOOL_MAX_NAME_LEN];
	int				size;
	int				version;
	struct image_desc		*img_dsc_l0;
	struct image_desc		*img_dsc;
	struct disk_track		trk[CWTOOL_MAX_TRACK];
	struct disk_track		trk_def;
	};

#define DISK_ERROR_FLAG_NOT_FOUND	(1 << 0)
#define DISK_ERROR_FLAG_ENCODING	(1 << 1)
#define DISK_ERROR_FLAG_ID		(1 << 2)
#define DISK_ERROR_FLAG_NUMBERING	(1 << 3)
#define DISK_ERROR_FLAG_SIZE		(1 << 4)
#define DISK_ERROR_FLAG_CHECKSUM	(1 << 5)

struct disk_error
	{
	int				flags;
	int				errors;
	int				warnings;
	};

struct disk_sector
	{
	int				number;
	unsigned char			*data;
	int				offset;
	int				size;
	struct disk_error		err;
	};

struct disk_summary
	{
	int				tracks;
	int				sectors_good;
	int				sectors_weak;
	int				sectors_bad;
	};

struct disk_sector_info
	{
	int				flags;
	int				offset;
	};

struct disk_info
	{
	int				track;
	int				try;
	int				sectors_good;
	int				sectors_weak;
	int				sectors_bad;
	struct disk_summary		sum;
	struct disk_sector_info		sct_nfo[CWTOOL_MAX_TRACK][CWTOOL_MAX_SECTOR];
	};

#define DISK_OPTION_INIT(i, r, f)	(struct disk_option) { .info_func = i, .retry = r, .flags = f }
#define DISK_OPTION_FLAG_NONE		0
#define DISK_OPTION_FLAG_IGNORE_SIZE	(1 << 0)

struct disk_option
	{
	void				(*info_func)(struct disk_info *, int);
	int				retry;
	int				flags;
	};

extern struct disk			*disk_get(int);
extern struct disk			*disk_search(const char *);
extern struct disk_track		*disk_init_track_default(struct disk *);
extern int				disk_init(struct disk *, int);
extern int				disk_insert(struct disk *);
extern int				disk_copy(struct disk *, struct disk *);
extern int				disk_tracks_used(struct disk *);
extern int				disk_image_ok(struct disk *);
extern int				disk_check_track(int);
extern int				disk_check_sectors(int);
extern int				disk_set_image(struct disk *, struct image_desc *);
extern int				disk_set_name(struct disk *, const char *);
extern int				disk_set_info(struct disk *, const char *);
extern int				disk_set_track(struct disk *, struct disk_track *, int);
extern int				disk_set_format(struct disk_track *, struct format_desc *);
extern int				disk_set_indexed_read(struct disk_track *, int);
extern int				disk_set_indexed_write(struct disk_track *, int);
extern int				disk_set_flip_side(struct disk_track *, int);
extern int				disk_set_clock(struct disk_track *, int);
extern int				disk_set_side_offset(struct disk_track *, int);
extern int				disk_set_timeout_read(struct disk_track *, int);
extern int				disk_set_timeout_write(struct disk_track *, int);
extern int				disk_set_read_option(struct disk_track *, struct format_option *, int, int);
extern int				disk_set_write_option(struct disk_track *, struct format_option *, int, int);
extern int				disk_set_rw_option(struct disk_track *, struct format_option *, int, int);
extern int				disk_set_skew(struct disk_track *, int);
extern int				disk_set_interleave(struct disk_track *, int);
extern int				disk_set_sector_number(struct disk_sector *, int);
extern int				disk_get_sector_number(struct disk_sector *);
extern int				disk_get_sectors(struct disk_track *);
extern int				disk_get_version(void);
extern char				*disk_get_name(struct disk *);
extern char				*disk_get_info(struct disk *);
extern struct format_desc		*disk_get_format(struct disk_track *);
extern struct format_option		*disk_get_read_options(struct disk_track *);
extern struct format_option		*disk_get_write_options(struct disk_track *);
extern struct format_option		*disk_get_rw_options(struct disk_track *);
extern int				disk_error_add(struct disk_error *, int, int);
extern int				disk_warning_add(struct disk_error *, int);
extern int				disk_sector_read(struct disk_sector *, struct disk_error *, unsigned char *);
extern int				disk_sector_write(unsigned char *, struct disk_sector *);
extern int				disk_statistics(struct disk *, const char *);
extern int				disk_read(struct disk *, struct disk_option *, const char *, const char *);
extern int				disk_write(struct disk *, struct disk_option *, const char *, const char *);



#endif /* !CWTOOL_DISK_H */
/******************************************************** Karsten Scheibler */
