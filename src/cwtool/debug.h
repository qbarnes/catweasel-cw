/****************************************************************************
 ****************************************************************************
 *
 * debug.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_DEBUG_H
#define CWTOOL_DEBUG_H

#include <stdio.h>

#include "error.h"

#ifdef CWTOOL_DEBUG
#define debug_compiled_in		1

#define debug(level, msg...)					\
	do							\
		{						\
		if (debug_level < level) break;			\
		fprintf(stderr, "%s:%d: ", __FILE__, __LINE__);	\
		fprintf(stderr, msg);				\
		fprintf(stderr, "\n");				\
		}						\
	while (0)

#define debug_head()						\
	do							\
		{						\
		if (debug_level <= 0) break;			\
		fprintf(stderr, "%s:%d: ", __FILE__, __LINE__);	\
		}						\
	while (0)

#define debug_error()			error_internal()

#define debug_error_condition(cond)				\
	do							\
		{						\
		if (cond) error_message(#cond);			\
		}						\
	while (0)
#else /* CWTOOL_DEBUG */
#define debug_compiled_in		0
#define debug(level, msg...)		while (0)
#define debug_head()			while (0)
#define debug_error()			while (0)
#define debug_error_condition(cond)	while (0)
#endif /* CWTOOL_DEBUG */

extern int				debug_level;



#endif /* !CWTOOL_DEBUG_H */
/******************************************************** Karsten Scheibler */
