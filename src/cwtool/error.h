/****************************************************************************
 ****************************************************************************
 *
 * error.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_ERROR_H
#define CWTOOL_ERROR_H

#include <stdio.h>
#include <stdlib.h>

#define error_perror()			_error_perror(__FILE__, __LINE__)
#define error_oom()			_error_oom(__FILE__, __LINE__)
#define error_internal()		_error_internal(__FILE__, __LINE__)
#define error_message(msg)		_error_message(__FILE__, __LINE__, msg)

#define error(msg...)					\
	do						\
		{					\
		_error_head(__FILE__, __LINE__);	\
		fprintf(stderr, msg);			\
		fprintf(stderr, "\n");			\
		exit(1);				\
		}					\
	while (0)

#define error_warning(msg...)				\
	do						\
		{					\
		_error_head(__FILE__, __LINE__);	\
		fprintf(stderr, msg);			\
		fprintf(stderr, "\n");			\
		}					\
	while (0)

#define error_perror2(msg...)				\
	do						\
		{					\
		error_warning(msg);			\
		error_perror();				\
		}					\
	while (0)

extern void				_error_head(char *, int);
extern void				_error_perror(char *, int);
extern void				_error_oom(char *, int);
extern void				_error_internal(char *, int);
extern void				_error_message(char *, int, char *);



#endif /* !CWTOOL_ERROR_H */
/******************************************************** Karsten Scheibler */
