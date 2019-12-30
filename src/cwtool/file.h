/****************************************************************************
 ****************************************************************************
 *
 * file.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_FILE_H
#define CWTOOL_FILE_H

#include "cwtool.h"

#define FILE_MODE_READ			1	/* normal */
#define FILE_MODE_READ2			2	/* do not check file size */
#define FILE_MODE_READ3			3	/* do not exit on error */
#define FILE_MODE_WRITE			4
#define FILE_TYPE_REGULAR		1
#define FILE_TYPE_DEVICE		2

struct file
	{
	const char			*path;
	int				fd;
	int				mode;
	int				type;
	int				end;
	struct cw_floppyinfo		fli;
	};

#define FILE_TRACK_INIT(t)		(struct file_track) { .timeout_read = t, .timeout_write = t }
#define FILE_TRACK_FLAG_INDEXED_READ	(1 << 0)
#define FILE_TRACK_FLAG_INDEXED_WRITE	(1 << 1)
#define FILE_TRACK_FLAG_FLIP_SIDE	(1 << 2)

struct file_track
	{
	unsigned char			flags;
	unsigned char			clock;
	unsigned char			side_offset;
	unsigned char			reserved;
	unsigned short			timeout_read;
	unsigned short			timeout_write;
	};

struct fifo;

struct file_image
	{
	char				name[CWTOOL_MAX_NAME_LEN];
	int				level;
	int				(*open)(struct file *, const char *, int);
	int				(*close)(struct file *);
	int				(*type)(struct file *);
	int				(*track_read)(struct file *, struct fifo *, struct file_track *, int);
	int				(*track_write)(struct file *, struct fifo *, struct file_track *, int);
	};

extern struct file_image		file_image_raw;
extern struct file_image		file_image_plain;
extern struct file_image		*file_search_image(const char *);
extern int				file_open(struct file *, const char *, int);
extern int				file_close(struct file *);
extern int				file_read(struct file *, unsigned char *, int);
extern int				file_write(struct file *, const unsigned char *, int);



#endif /* !CWTOOL_FILE_H */
/******************************************************** Karsten Scheibler */
