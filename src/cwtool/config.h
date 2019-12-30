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

#define CONFIG_MAX_PARAMS		CWTOOL_MAX_SECTOR

extern const char			config_default[];
extern void				config_parse(const char *, const char *, int);



#endif /* !CWTOOL_CONFIG_H */
/******************************************************** Karsten Scheibler */
