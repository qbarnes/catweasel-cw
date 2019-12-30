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
#define FILE_MODE_READ_RETURN		2
#define FILE_MODE_WRITE			3

struct file
	{
	const char			*path;
	int				fd;
	int				mode;
	};

extern int				file_open(struct file *, const char *, int);
extern int				file_close(struct file *);
extern const char			*file_get_path(struct file *);
extern int				file_get_mode(struct file *);
extern int				_file_ioctl(struct file *, int, unsigned long, int);
extern int				file_read(struct file *, unsigned char *, int);
extern int				file_read_strict(struct file *, unsigned char *, int);
extern int				file_write(struct file *, const unsigned char *, int);

#define file_ioctl(fil, cmd, arg, ne)	_file_ioctl(fil, cmd, (unsigned long) arg, ne)



#endif /* !CWTOOL_FILE_H */
/******************************************************** Karsten Scheibler */
