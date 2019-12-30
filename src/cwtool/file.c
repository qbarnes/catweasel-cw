/****************************************************************************
 ****************************************************************************
 *
 * file.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include "file.h"
#include "error.h"
#include "debug.h"
#include "verbose.h"
#include "cwtool.h"
#include "string.h"



extern char				program_name[];




/****************************************************************************
 *
 * misc helper functions
 *
 ****************************************************************************/




/****************************************************************************
 * file_tmp
 ****************************************************************************/
static char *
file_tmp(
	int				size)

	{
	static int			count;
	struct timeval			tv;
	char				*path    = malloc(size);
	char				*tmp_dir = getenv("TMPDIR");
	int				len;

	if (path == NULL) error_oom();
	if (tmp_dir == NULL) tmp_dir = "/tmp";
	if (gettimeofday(&tv, NULL) == -1) error_perror2("error while gettimeofday()");
	len = snprintf(path, size, "%s/%s-%010d-%010ld-%06ld-%05d", tmp_dir, program_name, count++, tv.tv_sec, tv.tv_usec, getpid());
	if ((len == -1) || (len >= size)) error("path for tmp file too long");
	return (path);
	}



/****************************************************************************
 * file_try_again
 ****************************************************************************/
static int
file_try_again(
	int				num)

	{
	struct timeval			tv = { .tv_sec = 0, .tv_usec = 100000 };

	/* if we got EAGAIN or EINTR we wait some time before trying again */

	if ((num != EAGAIN) && (num != EINTR)) return (0);
	select(0, NULL, NULL, NULL, &tv);
	return (1);
	}




/****************************************************************************
 *
 * used by external callers
 *
 ****************************************************************************/




/****************************************************************************
 * file_open
 ****************************************************************************/
int
file_open(
	struct file			*fil,
	char				*path,
	int				mode,
	int				flags)

	{
	debug_error_condition((mode != FILE_MODE_READ) && (mode != FILE_MODE_WRITE) && (mode != FILE_MODE_CREATE) && (mode != FILE_MODE_TMP));
	*fil = (struct file) { .path = path, .fd = -1, .mode = mode };

	/*
	 * check if we really have to open a file or just take the fds of
	 * stdin and stdout
	 */

	if (mode == FILE_MODE_READ)
		{
		if (! string_equal(path, "-"))
			{
			verbose(2, "opening '%s' for reading", path);
			fil->fd = open(path, O_RDONLY);
			}
		else fil->fd = STDIN_FILENO;
		}
	else if (mode == FILE_MODE_WRITE)
		{
		if (! string_equal(path, "-"))
			{
			verbose(2, "opening '%s' for writing", path);
			fil->fd = open(path, O_WRONLY);
			}
		else fil->fd = STDOUT_FILENO;
		}
	else if (mode == FILE_MODE_CREATE)
		{
		if (! string_equal(path, "-"))
			{
			verbose(2, "truncating or creating '%s' for writing", path);
			fil->fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
			}
		else fil->fd = STDOUT_FILENO;
		}
	else
		{
		if (path == NULL) fil->allocated = 1, path = file_tmp(CWTOOL_MAX_PATH_LEN);
		verbose(2, "opening temporary file '%s' for reading and writing", path);
		fil->fd = open(path, O_RDWR | O_CREAT | O_TRUNC | O_EXCL, S_IRUSR | S_IWUSR);
		}

	/* check if open was successful */

	if (fil->fd == -1)
		{
		if (flags & FILE_FLAG_RETURN) return (0);
		error_perror2("error while opening '%s'", path);
		}

	/*
	 * tmp files are unlinked after creation, just keeping the fd open is
	 * enough for our needs
	 */

	if (mode == FILE_MODE_TMP)
		{
		verbose(2, "unlinking temporary file '%s' but keeping it open, so it will consume disk space", path);
		if (unlink(path) == -1) error_perror2("error while unlinking '%s'", path);
		}
	return (1);
	}



/****************************************************************************
 * file_close
 ****************************************************************************/
int
file_close(
	struct file			*fil)

	{
	if (close(fil->fd) == -1) error_perror2("error while closing '%s'", fil->path);
	if (fil->allocated) free(fil->path);
	*fil = (struct file) { .fd = -1 };
	return (1);
	}



/****************************************************************************
 * file_get_path
 ****************************************************************************/
const char *
file_get_path(
	struct file			*fil)

	{
	return (fil->path);
	}



/****************************************************************************
 * file_is_readable
 ****************************************************************************/
int
file_is_readable(
	struct file			*fil)

	{
	if ((fil->mode == FILE_MODE_READ) || (fil->mode == FILE_MODE_TMP)) return (1);
	return (0);
	}



/****************************************************************************
 * file_is_writable
 ****************************************************************************/
int
file_is_writable(
	struct file			*fil)

	{
	if ((fil->mode == FILE_MODE_WRITE) || (fil->mode == FILE_MODE_CREATE) || (fil->mode == FILE_MODE_TMP)) return (1);
	return (0);
	}



/****************************************************************************
 * _file_ioctl
 ****************************************************************************/
int
_file_ioctl(
	struct file			*fil,
	int				cmd,
	unsigned long			arg,
	int				flags)

	{
	int				result;

	/*
	 * probably EAGAIN and EINTR as checked in file_try_again() never
	 * occur in combination with ioctl()
	 */

	while (1)
		{
		result = ioctl(fil->fd, cmd, arg);
		if (result != -1) break;
		if (file_try_again(errno)) continue;
		if (flags & FILE_FLAG_RETURN) break;
		error_perror2("error while accessing device '%s'", fil->path);
		}
	return (result);
	}



/****************************************************************************
 * file_seek
 ****************************************************************************/
int
file_seek(
	struct file			*fil,
	int				ofs,
	int				flags)

	{
	int				result, whence = SEEK_SET;

	/*
	 * probably EAGAIN and EINTR as checked in file_try_again() never
	 * occur in combination with lseek()
	 */

	if (ofs < 0) ofs = 0, whence = SEEK_CUR;
	while (1)
		{
		result = lseek(fil->fd, ofs, whence);
		if (result != -1) break;
		if (file_try_again(errno)) continue;
		if (flags & FILE_FLAG_RETURN) break;
		error_perror2("error while seeking '%s'", fil->path);
		}
	return (result);
	}



/****************************************************************************
 * file_read
 ****************************************************************************/
int
file_read(
	struct file			*fil,
	unsigned char			*data,
	int				size)

	{
	int				result = 1, ofs = 0;

	debug_error_condition(! file_is_readable(fil));
	while ((result > 0) && (size > 0))
		{
		result = read(fil->fd, &data[ofs], size);
		if (result == -1)
			{
			if (file_try_again(errno)) continue;
			error_perror2("error while reading from '%s'", fil->path);
			}
		size -= result;
		ofs += result;
		}
	return (ofs);
	}



/****************************************************************************
 * file_read_strict
 ****************************************************************************/
int
file_read_strict(
	struct file			*fil,
	unsigned char			*data,
	int				size)

	{
	if (file_read(fil, data, size) != size) error("file '%s' truncated", fil->path);
	return (size);
	}



/****************************************************************************
 * file_write
 ****************************************************************************/
int
file_write(
	struct file			*fil,
	unsigned char			*data,
	int				size)

	{
	int				result, ofs = 0;

	debug_error_condition(! file_is_writable(fil));
	while (size > 0)
		{
		result = write(fil->fd, &data[ofs], size);
		if (result == -1)
			{
			if (file_try_again(errno)) continue;
			error_perror2("error while writing to '%s'", fil->path);
			}
		size -= result;
		ofs += result;
		}
	return (ofs);
	}
/******************************************************** Karsten Scheibler */
