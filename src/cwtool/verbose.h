/****************************************************************************
 ****************************************************************************
 *
 * verbose.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_VERBOSE_H
#define CWTOOL_VERBOSE_H

#include <stdio.h>

#include "debug.h"

#define verbose(level, msg...)				\
	do						\
		{					\
		if (verbose_level < level) break;	\
		debug_head();				\
		fprintf(stderr, msg);			\
		fprintf(stderr, "\n");			\
		}					\
	while (0)

extern int				verbose_level;



#endif /* !CWTOOL_VERBOSE_H */
/******************************************************** Karsten Scheibler */
