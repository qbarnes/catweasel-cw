/****************************************************************************
 ****************************************************************************
 *
 * error.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "debug.h"



extern char				program_name[];



/****************************************************************************
 * _error_head
 ****************************************************************************/
void
_error_head(
	char				*file,
	int				line)

	{
	if (debug_level > 0) fprintf(stderr, "%s:%s:%d: ", program_name, file, line);
	else fprintf(stderr, "%s: ", program_name);
	}



/****************************************************************************
 * _error_perror
 ****************************************************************************/
void
_error_perror(
	char				*file,
	int				line)

	{
	_error_head(file, line);
	fprintf(stderr, "%s\n", strerror(errno));
	exit(1);
	}



/****************************************************************************
 * _error_oom
 ****************************************************************************/
void
_error_oom(
	char				*file,
	int				line)

	{
	_error_head(file, line);
	fprintf(stderr, "could not allocate memory\n");
	exit(1);
	}



/****************************************************************************
 * _error_internal
 ****************************************************************************/
void
_error_internal(
	char				*file,
	int				line)

	{
	_error_head(file, line);
	fprintf(stderr, "internal error\n");
	exit(1);
	}



/****************************************************************************
 * _error_message
 ****************************************************************************/
void
_error_message(
	char				*file,
	int				line,
	char				*msg)

	{
	_error_head(file, line);
	fprintf(stderr, "%s\n", msg);
	exit(1);
	}
/******************************************************** Karsten Scheibler */
