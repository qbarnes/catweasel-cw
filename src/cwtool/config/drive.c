/****************************************************************************
 ****************************************************************************
 *
 * config/drive.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>

#include "config/drive.h"
#include "error.h"
#include "debug.h"
#include "verbose.h"
#include "config.h"
#include "drive.h"
#include "string.h"



#define SCOPE_ENTER			(1 << 0)
#define SCOPE_LEAVE			(1 << 1)
#define SCOPE_FLAGS			(1 << 2)

static int				config_drive_directive(struct config *, struct drive *, int);




/****************************************************************************
 *
 * local functions
 *
 ****************************************************************************/




/****************************************************************************
 * config_drive_path
 ****************************************************************************/
static char *
config_drive_path(
	struct config			*cfg,
	char				*token,
	int				len)

	{
	static const char		valid[] = "_/.-";
	int				c, i, j;

	if (! config_string(cfg, token, len)) config_error(cfg, "drive path expected");
	if (token[0] != '/') config_error(cfg, "path must start with '/'"); 
	for (i = 1; token[i] != '\0'; i++)
		{
		c = token[i];
		if ((c >= '0') && (c <= '9')) continue;
		if ((c >= 'a') && (c <= 'z')) continue;
		if ((c >= 'A') && (c <= 'Z')) continue;
		for (j = 0; valid[j] != '\0'; j++) if (c == valid[j]) break;
		if (valid[j] != '\0') continue;
		config_error(cfg, "invalid character in path");
		}
	return (token);
	}



/****************************************************************************
 * config_drive_enter
 ****************************************************************************/
static int
config_drive_enter(
	struct config			*cfg,
	struct drive			*drv,
	int				scope)

	{
	scope &= ~SCOPE_ENTER;
	scope |= SCOPE_LEAVE;
	while (config_drive_directive(cfg, drv, scope)) ;
	return (1);
	}



/****************************************************************************
 * config_drive_info
 ****************************************************************************/
static int
config_drive_info(
	struct config			*cfg,
	struct drive			*drv)

	{
	char				info[CWTOOL_MAX_NAME_LEN];

	config_string(cfg, info, sizeof (info));
	if (! drive_set_info(drv, info)) debug_error();
	return (1);
	}



/****************************************************************************
 * config_drive_settle_time
 ****************************************************************************/
static int
config_drive_settle_time(
	struct config			*cfg,
	struct drive			*drv)

	{
	if (! drive_set_settle_time(drv, config_number(cfg, NULL, 0))) config_error(cfg, "invalid settle time value");
	return (1);
	}



/****************************************************************************
 * config_drive_step_time
 ****************************************************************************/
static int
config_drive_step_time(
	struct config			*cfg,
	struct drive			*drv)

	{
	if (! drive_set_step_time(drv, config_number(cfg, NULL, 0))) config_error(cfg, "invalid step time value");
	return (1);
	}



/****************************************************************************
 * config_drive_inverted_diskchange
 ****************************************************************************/
static int
config_drive_inverted_diskchange(
	struct config			*cfg,
	struct drive			*drv)

	{
	if (! drive_set_inverted_diskchange(drv, config_boolean(cfg, NULL, 0))) debug_error();
	return (1);
	}



/****************************************************************************
 * config_drive_ignore_diskchange
 ****************************************************************************/
static int
config_drive_ignore_diskchange(
	struct config			*cfg,
	struct drive			*drv)

	{
	if (! drive_set_ignore_diskchange(drv, config_boolean(cfg, NULL, 0))) debug_error();
	return (1);
	}



/****************************************************************************
 * config_drive_density
 ****************************************************************************/
static int
config_drive_density(
	struct config			*cfg,
	struct drive			*drv)

	{
	if (! drive_set_density(drv, config_boolean(cfg, NULL, 0))) debug_error();
	return (1);
	}



/****************************************************************************
 * config_drive_double_step
 ****************************************************************************/
static int
config_drive_double_step(
	struct config			*cfg,
	struct drive			*drv)

	{
	if (! drive_set_double_step(drv, config_boolean(cfg, NULL, 0))) debug_error();
	return (1);
	}



/****************************************************************************
 * config_drive_directive
 ****************************************************************************/
static int
config_drive_directive(
	struct config			*cfg,
	struct drive			*drv,
	int				scope)

	{
	char				token[CWTOOL_MAX_NAME_LEN];

	if (config_token(cfg, token, sizeof (token)) == 0)
		{
		if (scope & SCOPE_ENTER) config_error(cfg, "directive expected");
		config_error(cfg, "} expected");
		}
	if ((scope & SCOPE_ENTER) && (string_equal(token, "{"))) return (config_drive_enter(cfg, drv, scope));
	if ((scope & SCOPE_LEAVE) && (string_equal(token, "}"))) return (0);
	if (scope & SCOPE_FLAGS)
		{
		if (string_equal(token, "info"))                return (config_drive_info(cfg, drv));
		if (string_equal(token, "settle_time"))         return (config_drive_settle_time(cfg, drv));
		if (string_equal(token, "step_time"))           return (config_drive_step_time(cfg, drv));
		if (string_equal(token, "inverted_diskchange")) return (config_drive_inverted_diskchange(cfg, drv));
		if (string_equal(token, "ignore_diskchange"))   return (config_drive_ignore_diskchange(cfg, drv));
		if (string_equal(token, "density"))             return (config_drive_density(cfg, drv));
		if (string_equal(token, "double_step"))         return (config_drive_double_step(cfg, drv));
		}
	config_error_invalid(cfg, token);

	/* never reached, only to make gcc happy */

	return (0);
	}




/****************************************************************************
 *
 * used by external callers
 *
 ****************************************************************************/




/****************************************************************************
 * config_drive
 ****************************************************************************/
int
config_drive(
	struct config			*cfg,
	int				version)

	{
	char				token[CWTOOL_MAX_PATH_LEN];
	struct drive			drv;

	drive_init(&drv, version);
	config_drive_path(cfg, token, sizeof (token));
	if (! drive_set_path(&drv, token)) config_error(cfg, "already defined drive '%s' within this scope", token);
	config_drive_directive(cfg, &drv, SCOPE_ENTER | SCOPE_FLAGS);
	drive_insert(&drv);
	return (1);
	}
/******************************************************** Karsten Scheibler */
