/****************************************************************************
 ****************************************************************************
 *
 * image.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_IMAGE_H
#define CWTOOL_IMAGE_H

#include "cwtool.h"
#include "file.h"
#include "image_raw.h"
#include "image_g64.h"
#include "image_plain.h"

#define IMAGE_MODE_READ			1
#define IMAGE_MODE_READ_IGNORE_SIZE	2
#define IMAGE_MODE_WRITE		3
#define IMAGE_TYPE_REGULAR		1
#define IMAGE_TYPE_DEVICE		2

struct image
	{
	struct file			fil;
	struct cw_floppyinfo		fli;
	struct image_g64_track		g64_trk[IMAGE_G64_MAX_TRACKS];
	int				offset;
	int				type;
	int				end_seen;
	int				ignore_size;
	};

#define IMAGE_TRACK_INIT(t)		(struct image_track) { .timeout_read = t, .timeout_write = t }
#define IMAGE_TRACK_FLAG_INDEXED_READ	(1 << 0)
#define IMAGE_TRACK_FLAG_INDEXED_WRITE	(1 << 1)
#define IMAGE_TRACK_FLAG_FLIP_SIDE	(1 << 2)

struct image_track
	{
	unsigned char			flags;
	unsigned char			clock;
	unsigned char			side_offset;
	unsigned char			reserved;
	unsigned short			timeout_read;
	unsigned short			timeout_write;
	};

struct fifo;

struct image_desc
	{
	char				name[CWTOOL_MAX_NAME_LEN];
	int				level;
	int				(*open)(struct image *, const char *, int);
	int				(*close)(struct image *);
	int				(*type)(struct image *);
	int				(*offset)(struct image *);
	int				(*track_read)(struct image *, struct fifo *, struct image_track *, int);
	int				(*track_write)(struct image *, struct fifo *, struct image_track *, int);
	};

extern struct image_desc		*image_search_desc(const char *);
extern int				image_init(struct image *, int);



#endif /* !CWTOOL_IMAGE_H */
/******************************************************** Karsten Scheibler */
