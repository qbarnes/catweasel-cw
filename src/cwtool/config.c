/****************************************************************************
 ****************************************************************************
 *
 * config.c
 *
 ****************************************************************************
 ****************************************************************************/





#include <stdio.h>

#include "config.h"
#include "error.h"
#include "debug.h"
#include "verbose.h"
#include "string.h"




/****************************************************************************
 *
 * helper functions
 *
 ****************************************************************************/




/****************************************************************************
 * config_is_space
 ****************************************************************************/
static int
config_is_space(
	int				c)

	{
	if ((c == '\t') || (c == '\n') || (c == '\r') || (c == ' ')) return (1);
	return (0);
	}



/****************************************************************************
 * config_get_char
 ****************************************************************************/
static int
config_get_char(
	struct config			*cfg,
	int				allow_all)

	{
	int				c, i;
	static const char		valid[] = "\n\r\t _{}#$%/+-\"";

	if (cfg->ofs >= cfg->size) return (0);
	c = cfg->text[cfg->ofs++];
	if (c == 0) config_error(cfg, "got \\0 character");
	if (allow_all) goto ok;
	if ((c >= '0') && (c <= '9')) goto ok;
	if ((c >= 'A') && (c <= 'Z')) goto ok;
	if ((c >= 'a') && (c <= 'z')) goto ok;
	for (i = 0; valid[i] != '\0'; i++) if (c == valid[i]) goto ok;
	config_error(cfg, "illegal character");
ok:
	cfg->line_ofs++;
	if (c == '\n') cfg->line++, cfg->line_ofs = 0;
	return (c);
	}



/****************************************************************************
 * config_comment
 ****************************************************************************/
static int
config_comment(
	int				c,
	int				*comment)

	{
	if ((*comment) && (c != 0) && (c != '\n')) return (1);
	*comment = 0;
	if (c != '#') return (0);
	*comment = 1;
	return (1);
	}



/****************************************************************************
 * config_number_hex
 ****************************************************************************/
static int
config_number_hex(
	struct config			*cfg,
	char				*token)

	{
	int				c, num = 0;

	while (*token != '\0')
		{
		c = *token++;
		if ((c >= '0') && (c <= '9')) c -= '0';
		else if ((c >= 'a') && (c <= 'f')) c -= 'a' - 10;
		else config_error(cfg, "not a hexadecimal number");
		if (num > (0x7fffffff - 15) / 16) config_error(cfg, "hexadecimal number too large");
		num = 16 * num + c;
		}
	debug_error_condition(num < 0);
	return (num);
	}



/****************************************************************************
 * config_number_dec
 ****************************************************************************/
static int
config_number_dec(
	struct config			*cfg,
	char				*token)

	{
	int				c, num = 0;

	while (*token != '\0')
		{
		c = *token++;
		if ((c < '0') || (c > '9')) config_error(cfg, "not a decimal number");
		if (num > (0x7fffffff - 9) / 10) config_error(cfg, "decimal number too large");
		num = 10 * num + c - '0';
		}
	debug_error_condition(num < 0);
	return (num);
	}



/****************************************************************************
 * config_number_bin
 ****************************************************************************/
static int
config_number_bin(
	struct config			*cfg,
	char				*token)

	{
	int				c, num = 0;

	while (*token != '\0')
		{
		c = *token++;
		if ((c < '0') || (c > '1')) config_error(cfg, "not a binary number");
		if (num > (0x7fffffff - 1) / 2) config_error(cfg, "binary number too large");
		num = 2 * num + c - '0';
		}
	debug_error_condition(num < 0);
	return (num);
	}



/****************************************************************************
 * config_top_directive
 ****************************************************************************/
static int
config_top_directive(
	struct config			*cfg,
	int				version)

	{
	char				token[CWTOOL_MAX_NAME_LEN];

	debug(2, "looking for next top directive, version = %d", version);
	if (! config_token(cfg, token, sizeof (token))) return (0);
	if (string_equal(token, "disk"))  return (config_disk(cfg, version));
	if (string_equal(token, "drive")) return (config_drive(cfg, version));
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
 * config_default
 ****************************************************************************/
const char				config_default[] =
#include "cwtoolrc.c"
;



/****************************************************************************
 * config_error_invalid
 ****************************************************************************/
void
config_error_invalid(
	struct config			*cfg,
	char				*token)

	{
	config_error(cfg, "invalid directive '%s' within this scope", token);
	}



/****************************************************************************
 * config_token
 ****************************************************************************/
int
config_token(
	struct config			*cfg,
	char				*token,
	int				size)

	{
	int				c, i, comment;

	for (i = comment = 0, token[0] = '\0'; i < size - 1; )
		{
		c = config_get_char(cfg, comment);
		if (config_comment(c, &comment)) continue;
		if ((c == 0) || (config_is_space(c)))
			{
			if ((c != 0) && (i == 0)) continue;
			debug(3, "got token '%s'", token);
			return (i);
			}
		token[i++] = c;
		token[i] = '\0';
		}
	config_error(cfg, "token too long");

	/* never reached, only to make gcc happy */

	return (0);
	}



/****************************************************************************
 * config_string
 ****************************************************************************/
int
config_string(
	struct config			*cfg,
	char				*string,
	int				size)

	{
	int				c, i, comment, quote;

	for (i = comment = quote = 0, string[0] = '\0'; i < size - 1; )
		{
		c = config_get_char(cfg, comment | quote);
		if ((! comment) && (c == '"'))
			{
			if (quote) return (i);
			quote = 1;
			continue;
			}
		if (! quote)
			{
			if (config_comment(c, &comment)) continue;
			if (config_is_space(c)) continue;
			c = 0;
			}
		if (c == 0) config_error(cfg, "\" expected");
		if (c == '\n') config_error(cfg, "string exceeds line");
		string[i++] = c;
		string[i] = '\0';
		}
	config_error(cfg, "string too long");

	/* never reached, only to make gcc happy */

	return (0);
	}



/****************************************************************************
 * config_number
 ****************************************************************************/
int
config_number(
	struct config			*cfg,
	char				*token,
	int				len)

	{
	char				token2[CWTOOL_MAX_NAME_LEN];
	int				num, sign = -1;

	if (token == NULL) token = token2, len = config_token(cfg, token2, sizeof (token2));
	if (len <= 0) config_error(cfg, "number expected");
	if ((len > 1) && (token[0] == '-')) sign = 1;
	if ((len > 1) && (token[0] == '+')) sign = 0;
	if (sign >= 0) len--, token++;
	if ((len > 2) && (token[0] == '0') && (token[1] == 'x')) num = config_number_hex(cfg, &token[2]);
	else if ((len > 1) && (token[0] == '$')) num = config_number_hex(cfg, &token[1]);
	else if ((len > 1) && (token[0] == '%')) num = config_number_bin(cfg, &token[1]);
	else num = config_number_dec(cfg, token);
	if (sign == 1) num = -num;
	debug(3, "got number %d == 0x%x", num, num);
	return (num);
	}



/****************************************************************************
 * config_boolean
 ****************************************************************************/
int
config_boolean(
	struct config			*cfg,
	char				*token,
	int				len)

	{
	char				token2[CWTOOL_MAX_NAME_LEN];
	int				num;

	if (token == NULL) token = token2, len = config_token(cfg, token2, sizeof (token2));
	if (len <= 0) config_error(cfg, "boolean expected");
	if ((string_equal(token, "no")) || (string_equal(token, "off"))) return (0);
	if ((string_equal(token, "yes")) || (string_equal(token, "on"))) return (1);
	num = config_number(cfg, token, len);
	if ((num < 0) || (num > 1)) config_error(cfg, "not a boolean value '%s'", token);
	return (num);
	}



/****************************************************************************
 * config_parse
 ****************************************************************************/
void
config_parse(
	const char			*file,
	const char			*text,
	int				size)

	{
	struct config			cfg = CONFIG_INIT(file, text, size);
	static int			version;

	version++;
	verbose(1, "reading config from '%s'", file);
	while (config_top_directive(&cfg, version)) ;
	}
/******************************************************** Karsten Scheibler */
