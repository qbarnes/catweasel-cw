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
#include <sys/types.h>
#include <sys/ioctl.h>

#include "file.h"
#include "error.h"
#include "debug.h"
#include "verbose.h"
#include "string.h"



/****************************************************************************
 * file_open
 ****************************************************************************/
int
file_open(
	struct file			*fil,
	const char			*path,
	int				mode)

	{
	debug_error_condition((mode != FILE_MODE_READ) && (mode != FILE_MODE_READ_RETURN) && (mode != FILE_MODE_WRITE));
	*fil = (struct file) { .path = path, .fd = -1, .mode = mode };
	if (mode == FILE_MODE_WRITE)
		{
		if (string_equal(path, "-")) fil->fd = STDOUT_FILENO;
		else fil->fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
		}
	else 
		{
		if (string_equal(path, "-")) fil->fd = STDIN_FILENO;
		else fil->fd = open(path, O_RDONLY);
		}
	if (fil->fd == -1)
		{
		if (mode == FILE_MODE_READ_RETURN) return (0);
		error_perror2("error while opening '%s'", path);
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
 * file_get_mode
 ****************************************************************************/
int
file_get_mode(
	struct file			*fil)

	{
	return (fil->mode);
	}



/****************************************************************************
 * _file_ioctl
 ****************************************************************************/
int
_file_ioctl(
	struct file			*fil,
	int				cmd,
	unsigned long			arg,
	int				no_error)

	{
	int				result;

	while (1)
		{
		result = ioctl(fil->fd, cmd, arg);
		if (result != -1) break;
		if ((errno == EAGAIN) || (errno == EINTR)) continue;
		if (no_error) break;
		error_perror2("error while accessing device '%s'", fil->path);
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

	debug_error_condition((fil->mode != FILE_MODE_READ) && (fil->mode != FILE_MODE_READ_RETURN));
	while ((result > 0) && (size > 0))
		{
		result = read(fil->fd, &data[ofs], size);
		if (result == -1)
			{
			if ((errno == EAGAIN) || (errno == EINTR)) continue;
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
	const unsigned char		*data,
	int				size)

	{
	int				result, ofs = 0;

	debug_error_condition(fil->mode != FILE_MODE_WRITE);
	while (size > 0)
		{
		result = write(fil->fd, &data[ofs], size);
		if (result == -1)
			{
			if ((errno == EAGAIN) || (errno == EINTR)) continue;
			error_perror2("error while writing to '%s'", fil->path);
			}
		size -= result;
		ofs += result;
		}
	return (ofs);
	}
/******************************************************** Karsten Scheibler */
