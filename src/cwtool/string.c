/****************************************************************************
 ****************************************************************************
 *
 * string.c
 *
 ****************************************************************************
 *
 * yes i know strcmp(), strlen() and strcpy(), i only wanted to reduce
 * library dependencies ;-)
 *
 ****************************************************************************
 ****************************************************************************/





#include "string.h"
#include "error.h"
#include "debug.h"
#include "verbose.h"



/****************************************************************************
 * string_equal
 ****************************************************************************/
int
string_equal(
	const char			*src1,
	const char			*src2)

	{
	while ((*src1 | *src2) != '\0') if (*src1++ != *src2++) return (0);
	return (1);
	}



/****************************************************************************
 * string_length
 ****************************************************************************/
int
string_length(
	const char			*src)

	{
	int				i = 0;

	while (src[i] != '\0') i++;
	return (i);
	}



/****************************************************************************
 * string_copy
 ****************************************************************************/
char *
string_copy(
	char				*dest,
	int				size,
	const char			*src)

	{
	char				*result = dest;

	do
		{
		if (--size < 0) error_message("string_copy() size exceeded");
		}
	while ((*dest++ = *src++) != '\0');
	return (result);
	}



/****************************************************************************
 * string_dot
 ****************************************************************************/
char *
string_dot(
	char				*dest,
	int				size,
	char				*src)

	{
	int				i;

	debug_error_condition(size < 4);
	if (string_length(src) < size) return (src);
	for (i = 0; i < size - 4; i++) dest[i] = src[i];
	dest[size - 4] = '.';
	dest[size - 3] = '.';
	dest[size - 2] = '.';
	dest[size - 1] = '\0';
	return (dest);
	}
/******************************************************** Karsten Scheibler */
