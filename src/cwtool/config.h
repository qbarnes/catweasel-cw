/****************************************************************************
 ****************************************************************************
 *
 * config.h
 *
 ****************************************************************************
 ****************************************************************************/





#ifndef CWTOOL_CONFIG_H
#define CWTOOL_CONFIG_H

#include "cwtool.h"
#include "error.h"
#include "config/disk.h"
#include "config/drive.h"

#define CONFIG_MAX_PARAMS		CWTOOL_MAX_SECTOR

#define CONFIG_INIT(f, t, s)		(struct config) { .file = f, .text = t, .size = s }

struct config
	{
	const char			*file;
	const char			*text;
	int				size;
	int				ofs;
	int				line;
	int				line_ofs;
	};

#define config_error(cfg, msg...)						\
	do									\
		{								\
		error_warning("parse error in '%s' around line %d, char %d",	\
			cfg->file, cfg->line + 1, cfg->line_ofs + 1);		\
		error(msg);							\
		}								\
	while (0)

extern const char			config_default[];
extern void				config_error_invalid(struct config *, char *);
extern int				config_token(struct config *, char *, int);
extern int				config_string(struct config *, char *, int);
extern int				config_number(struct config *, char *, int);
extern int				config_boolean(struct config *, char *, int);
extern void				config_parse(const char *, const char *, int);



#endif /* !CWTOOL_CONFIG_H */
/******************************************************** Karsten Scheibler */
