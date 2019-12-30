/****************************************************************************
 ****************************************************************************
 *
 * file.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_FILE_H
#define CWTOOL_FILE_H

#define FILE_MODE_READ			1
#define FILE_MODE_WRITE			2
#define FILE_MODE_CREATE		3
#define FILE_MODE_TMP			4

#define FILE_FLAG_NONE			0
#define FILE_FLAG_RETURN		(1 << 0)

struct file
	{
	char				*path;
	int				fd;
	int				mode;
	int				allocated;
	};

extern int				file_open(struct file *, char *, int, int);
extern int				file_close(struct file *);
extern const char			*file_get_path(struct file *);
extern int				file_is_readable(struct file *);
extern int				file_is_writable(struct file *);
extern int				_file_ioctl(struct file *, int, unsigned long, int);
extern int				file_seek(struct file *, int, int);
extern int				file_read(struct file *, unsigned char *, int);
extern int				file_read_strict(struct file *, unsigned char *, int);
extern int				file_write(struct file *, unsigned char *, int);

#define file_ioctl(fil, cmd, arg, fl)	_file_ioctl(fil, cmd, (unsigned long) arg, fl)



#endif /* !CWTOOL_FILE_H */
/******************************************************** Karsten Scheibler */
