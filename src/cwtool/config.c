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
#include "disk.h"
#include "format.h"
#include "image.h"
#include "string.h"



#define CONFIG_DISK			(1 << 0)
#define CONFIG_ENTER			(1 << 1)
#define CONFIG_LEAVE			(1 << 2)
#define CONFIG_TRACK			(1 << 3)
#define CONFIG_RW			(1 << 4)
#define CONFIG_READ			(1 << 5)
#define CONFIG_WRITE			(1 << 6)
#define CONFIG_PARAMS			(1 << 7)
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

struct config_params
	{
	int				ofs;
	int				size;
	struct format_option		*fmt_opt_rd;
	struct format_option		*fmt_opt_wr;
	struct format_option		*fmt_opt_rw;
	};



/****************************************************************************
 * config_error
 ****************************************************************************/
#define config_error(cfg, msg...)						\
	do									\
		{								\
		error_warning("parse error in '%s' around line %d, char %d",	\
			cfg->file, cfg->line + 1, cfg->line_ofs + 1);		\
		error(msg);							\
		}								\
	while (0)



/****************************************************************************
 * config_error_timeout
 ****************************************************************************/
static void
config_error_timeout(
	struct config			*cfg)

	{
	config_error(cfg, "invalid timeout value");
	}



/****************************************************************************
 * config_error_format
 ****************************************************************************/
static void
config_error_format(
	struct config			*cfg)

	{
	config_error(cfg, "no format specified");
	}




/****************************************************************************
 *
 * functions to extract or test characters, tokens, strings and numbers
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
	static const char		valid[] = "\n\r\t _{}#$%+-\"";

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
	if (c == '\n')
		{
		cfg->line++;
		cfg->line_ofs = 0;
		}
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
 * config_token
 ****************************************************************************/
static int
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
			debug(2, "got token '%s'", token);
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
static int
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
 * config_name
 ****************************************************************************/
static char *
config_name(
	struct config			*cfg,
	char				*error,
	char				*token,
	int				len)

	{
	static const char		valid[] = "_/.-+";
	int				c, i, j;

	if (! config_string(cfg, token, len)) config_error(cfg, error);
	for (i = 0; token[i] != '\0'; i++)
		{
		c = token[i];
		if ((c >= 'a') && (c <= 'z')) continue;
		if ((c >= 'A') && (c <= 'Z')) continue;
		for (j = 0; valid[j] != '\0'; j++) if (c == valid[j]) break;
		if (valid[j] != '\0') continue;
		if ((i > 0) && (c >= '0') && (c <= '9')) continue;
		config_error(cfg, "invalid character in name");
		}
	return (token);
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
 * config_number2
 ****************************************************************************/
static int
config_number2(
	struct config			*cfg,
	char				*token,
	int				len)

	{
	int				num, sign = -1;

	if ((len > 1) && (token[0] == '-')) sign = 1;
	if ((len > 1) && (token[0] == '+')) sign = 0;
	if (sign >= 0) len--, token++;
	if ((len > 2) && (token[0] == '0') && (token[1] == 'x')) num = config_number_hex(cfg, &token[2]);
	else if ((len > 1) && (token[0] == '$')) num = config_number_hex(cfg, &token[1]);
	else if ((len > 1) && (token[0] == '%')) num = config_number_bin(cfg, &token[1]);
	else num = config_number_dec(cfg, token);
	if (sign == 1) num = -num;
	debug(2, "got number %d == 0x%x", num, num);
	return (num);
	}



/****************************************************************************
 * config_number
 ****************************************************************************/
static int
config_number(
	struct config			*cfg)

	{
	char				token[CWTOOL_MAX_NAME_LEN];
	int				len;

	len = config_token(cfg, token, sizeof (token));
	if (len == 0) config_error(cfg, "number expected");
	return (config_number2(cfg, token, len));
	}



/****************************************************************************
 * config_boolean
 ****************************************************************************/
static int
config_boolean(
	struct config			*cfg)

	{
	char				token[CWTOOL_MAX_NAME_LEN];
	int				len;
	int				num;

	len = config_token(cfg, token, sizeof (token));
	if (len == 0) config_error(cfg, "boolean expected");
	if ((string_equal(token, "no")) || (string_equal(token, "off"))) return (0);
	if ((string_equal(token, "yes")) || (string_equal(token, "on"))) return (1);
	num = config_number2(cfg, token, len);
	if ((num < 0) || (num > 1)) config_error(cfg, "not a boolean value '%s'", token);
	return (num);
	}



/****************************************************************************
 * config_trackvalue
 ****************************************************************************/
static int
config_trackvalue(
	struct config			*cfg)

	{
	int				track = config_number(cfg);

	if (! disk_check_track(track)) config_error(cfg, "invalid track value");
	return (track);
	}




/****************************************************************************
 *
 * functions for directive handling
 *
 ****************************************************************************/




static int				config_directive(struct config *, struct disk *, struct disk_track *, struct config_params *, int);



/****************************************************************************
 * config_disk
 ****************************************************************************/
static int
config_disk(
	struct config			*cfg,
	struct disk			*dsk)

	{
	char				token[CWTOOL_MAX_NAME_LEN];
	struct disk_track		*dsk_trk = disk_init_track_default(dsk);

	config_name(cfg, "disk name expected", token, sizeof (token));
	if (! disk_set_name(dsk, token)) config_error(cfg, "already defined disk '%s' within this scope", token);
	config_directive(cfg, dsk, dsk_trk, NULL, CONFIG_ENTER | CONFIG_TRACK | CONFIG_RW);
	if (! disk_tracks_used(dsk)) config_error(cfg, "no tracks used");
	if (! disk_image_ok(dsk)) config_error(cfg, "specified format and image are not on the same level");
	return (1);
	}



/****************************************************************************
 * config_enter
 ****************************************************************************/
static int
config_enter(
	struct config			*cfg,
	struct disk			*dsk,
	struct disk_track		*dsk_trk,
	struct config_params		*cfg_prm,
	int				allowed)

	{
	allowed &= ~CONFIG_ENTER;
	while (config_directive(cfg, dsk, dsk_trk, cfg_prm, allowed | CONFIG_LEAVE)) ;
	return (1);
	}



/****************************************************************************
 * config_info
 ****************************************************************************/
static int
config_info(
	struct config			*cfg,
	struct disk			*dsk)

	{
	char				info[CWTOOL_MAX_NAME_LEN];

	config_string(cfg, info, sizeof (info));
	if (! disk_set_info(dsk, info)) debug_error();
	return (1);
	}



/****************************************************************************
 * config_copy
 ****************************************************************************/
static int
config_copy(
	struct config			*cfg,
	struct disk			*dsk)

	{
	char				token[CWTOOL_MAX_NAME_LEN];
	struct disk			*dsk2;

	dsk2 = disk_search(config_name(cfg, "disk name expected", token, sizeof (token)));
	if (dsk2 == NULL) config_error(cfg, "unknown disk name '%s'", token);
	return (disk_copy(dsk, dsk2));
	}



/****************************************************************************
 * config_image
 ****************************************************************************/
static int
config_image(
	struct config			*cfg,
	struct disk			*dsk)

	{
	char				token[CWTOOL_MAX_NAME_LEN];
	struct image_desc		*img_dsc;

	img_dsc = image_search_desc(config_name(cfg, "image name expected", token, sizeof (token)));
	if (img_dsc == NULL) config_error(cfg, "unknown image name '%s'", token);
	return (disk_set_image(dsk, img_dsc));
	}



/****************************************************************************
 * config_track
 ****************************************************************************/
static int
config_track(
	struct config			*cfg,
	struct disk			*dsk,
	struct disk_track		*dsk_trk)

	{
	struct disk_track		dsk_trk2;
	int				track;

	dsk_trk2 = *dsk_trk;
	track    = config_trackvalue(cfg);
	config_directive(cfg, NULL, &dsk_trk2, NULL, CONFIG_ENTER | CONFIG_RW);
	if (! disk_set_track(dsk, &dsk_trk2, track)) debug_error();
	return (1);
	}



/****************************************************************************
 * config_track_range
 ****************************************************************************/
static int
config_track_range(
	struct config			*cfg,
	struct disk			*dsk,
	struct disk_track		*dsk_trk)

	{
	struct disk_track		dsk_trk2;
	int				track, end, step;

	dsk_trk2 = *dsk_trk;
	track    = config_trackvalue(cfg);
	end      = config_trackvalue(cfg);
	step     = config_trackvalue(cfg);
	if (step < 1) config_error(cfg, "invalid step size");
	config_directive(cfg, NULL, &dsk_trk2, NULL, CONFIG_ENTER | CONFIG_RW);
	for ( ; track <= end; track += step) if (! disk_set_track(dsk, &dsk_trk2, track)) debug_error();
	return (1);
	}



/****************************************************************************
 * config_format
 ****************************************************************************/
static int
config_format(
	struct config			*cfg,
	struct disk_track		*dsk_trk)

	{
	char				token[CWTOOL_MAX_NAME_LEN];
	struct format_desc		*fmt_dsc;

	fmt_dsc = format_search_desc(config_name(cfg, "format name expected", token, sizeof (token)));
	if (fmt_dsc == NULL) config_error(cfg, "unknown format '%s'", token);
	return (disk_set_format(dsk_trk, fmt_dsc));
	}



/****************************************************************************
 * config_clock
 ****************************************************************************/
static int
config_clock(
	struct config			*cfg,
	struct disk_track		*dsk_trk)

	{
	if (! disk_set_clock(dsk_trk, config_number(cfg))) config_error(cfg, "invalid clock value");
	return (1);
	}



/****************************************************************************
 * config_timeout_read
 ****************************************************************************/
static int
config_timeout_read(
	struct config			*cfg,
	struct disk_track		*dsk_trk)

	{
	if (! disk_set_timeout_read(dsk_trk, config_number(cfg))) config_error_timeout(cfg);
	return (1);
	}



/****************************************************************************
 * config_timeout_write
 ****************************************************************************/
static int
config_timeout_write(
	struct config			*cfg,
	struct disk_track		*dsk_trk)

	{
	if (! disk_set_timeout_write(dsk_trk, config_number(cfg))) config_error_timeout(cfg);
	return (1);
	}



/****************************************************************************
 * config_timeout
 ****************************************************************************/
static int
config_timeout(
	struct config			*cfg,
	struct disk_track		*dsk_trk)

	{
	int				val = config_number(cfg);

	if (! disk_set_timeout_read(dsk_trk, val)) config_error_timeout(cfg);
	if (! disk_set_timeout_write(dsk_trk, val)) config_error_timeout(cfg);
	return (1);
	}



/****************************************************************************
 * config_indexed_read
 ****************************************************************************/
static int
config_indexed_read(
	struct config			*cfg,
	struct disk_track		*dsk_trk)

	{
	if (! disk_set_indexed_read(dsk_trk, config_boolean(cfg))) debug_error();
	return (1);
	}



/****************************************************************************
 * config_indexed_write
 ****************************************************************************/
static int
config_indexed_write(
	struct config			*cfg,
	struct disk_track		*dsk_trk)

	{
	if (! disk_set_indexed_write(dsk_trk, config_boolean(cfg))) debug_error();
	return (1);
	}



/****************************************************************************
 * config_indexed
 ****************************************************************************/
static int
config_indexed(
	struct config			*cfg,
	struct disk_track		*dsk_trk)

	{
	int				val = config_boolean(cfg);

	if (! disk_set_indexed_read(dsk_trk, val)) debug_error();
	if (! disk_set_indexed_write(dsk_trk, val)) debug_error();
	return (1);
	}



/****************************************************************************
 * config_flip_side
 ****************************************************************************/
static int
config_flip_side(
	struct config			*cfg,
	struct disk_track		*dsk_trk)

	{
	if (! disk_set_flip_side(dsk_trk, config_boolean(cfg))) debug_error();
	return (1);
	}



/****************************************************************************
 * config_side_offset
 ****************************************************************************/
static int
config_side_offset(
	struct config			*cfg,
	struct disk_track		*dsk_trk)

	{
	if (! disk_set_side_offset(dsk_trk, config_number(cfg))) debug_error();
	return (1);
	}



/****************************************************************************
 * config_skew
 ****************************************************************************/
static int
config_skew(
	struct config			*cfg,
	struct disk_track		*dsk_trk)

	{
	if (! disk_set_skew(dsk_trk, config_number(cfg))) config_error(cfg, "invalid skew value");
	return (1);
	}



/****************************************************************************
 * config_interleave
 ****************************************************************************/
static int
config_interleave(
	struct config			*cfg,
	struct disk_track		*dsk_trk)

	{
	if (! disk_set_interleave(dsk_trk, config_number(cfg))) config_error(cfg, "invalid interleave value");
	return (1);
	}



/****************************************************************************
 * config_params
 ****************************************************************************/
static int
config_params(
	struct config			*cfg,
	struct disk_track		*dsk_trk,
	struct config_params		*cfg_prm,
	char				*token)

	{
	int				num, rd = 1, wr = 1, rw = 1;

	if (cfg_prm->ofs >= cfg_prm->size) config_error(cfg, "too many parameters");
	num = config_number2(cfg, token, string_length(token));
	if (cfg_prm->fmt_opt_rd != NULL) rd = disk_set_read_option(dsk_trk, cfg_prm->fmt_opt_rd, num, cfg_prm->ofs);
	if (cfg_prm->fmt_opt_wr != NULL) wr = disk_set_write_option(dsk_trk, cfg_prm->fmt_opt_wr, num, cfg_prm->ofs);
	if (cfg_prm->fmt_opt_rw != NULL) rw = disk_set_rw_option(dsk_trk, cfg_prm->fmt_opt_rw, num, cfg_prm->ofs);
	if ((! rd) || (! wr) || (! rw)) config_error(cfg, "invalid parameter value '%s'", token);
	cfg_prm->ofs++;
	return (1);
	}



/****************************************************************************
 * config_set_params
 ****************************************************************************/
static int
config_set_params(
	struct config			*cfg,
	struct disk_track		*dsk_trk,
	struct format_option		*fmt_opt_rd,
	struct format_option		*fmt_opt_wr,
	struct format_option		*fmt_opt_rw)
	

	{
	struct config_params		cfg_prm = { .fmt_opt_rd = fmt_opt_rd, .fmt_opt_wr = fmt_opt_wr, .fmt_opt_rw = fmt_opt_rw };

	if (fmt_opt_rw != NULL) cfg_prm.size = fmt_opt_rw->params;
	else cfg_prm.size = (fmt_opt_rd != NULL) ? fmt_opt_rd->params : fmt_opt_wr->params;

	/*
	 * if number of parameters is -1, then set it to number of sectors,
	 * directive sector_sizes is the only user of this
	 */

	if (cfg_prm.size < 0) cfg_prm.size = disk_get_sectors(dsk_trk);
	debug_error_condition(cfg_prm.size > CONFIG_MAX_PARAMS);
	config_directive(cfg, NULL, dsk_trk, &cfg_prm, CONFIG_ENTER | CONFIG_PARAMS);
	if (cfg_prm.ofs < cfg_prm.size) config_error(cfg, "too few parameters");
	return (1);
	}



/****************************************************************************
 * config_read
 ****************************************************************************/
static int
config_read(
	struct config			*cfg,
	struct disk_track		*dsk_trk,
	char				*name)

	{
	struct format_option		*fmt_opt;

	fmt_opt = format_search_option(disk_get_read_options(dsk_trk), name);
	if (fmt_opt == NULL) return (0);
	return (config_set_params(cfg, dsk_trk, fmt_opt, NULL, NULL));
	}



/****************************************************************************
 * config_write
 ****************************************************************************/
static int
config_write(
	struct config			*cfg,
	struct disk_track		*dsk_trk,
	char				*name)

	{
	struct format_option		*fmt_opt;

	fmt_opt = format_search_option(disk_get_write_options(dsk_trk), name);
	if (fmt_opt == NULL) return (0);
	return (config_set_params(cfg, dsk_trk, NULL, fmt_opt, NULL));
	}



/****************************************************************************
 * config_rw
 ****************************************************************************/
static int
config_rw(
	struct config			*cfg,
	struct disk_track		*dsk_trk,
	char				*name)

	{
	struct format_option		*fmt_opt_rd, *fmt_opt_wr, *fmt_opt_rw;

	fmt_opt_rd = format_search_option(disk_get_read_options(dsk_trk), name);
	fmt_opt_wr = format_search_option(disk_get_write_options(dsk_trk), name);
	fmt_opt_rw = format_search_option(disk_get_rw_options(dsk_trk), name);
	if (fmt_opt_rw == NULL)
		{
		if ((fmt_opt_rd == NULL) || (fmt_opt_wr == NULL)) return (0);
		debug_error_condition(fmt_opt_rd->params != fmt_opt_wr->params);
		}
	else debug_error_condition((fmt_opt_rd != NULL) || (fmt_opt_wr != NULL));
	return (config_set_params(cfg, dsk_trk, fmt_opt_rd, fmt_opt_wr, fmt_opt_rw));
	}



/****************************************************************************
 * config_directive
 ****************************************************************************/
static int
config_directive(
	struct config			*cfg,
	struct disk			*dsk,
	struct disk_track		*dsk_trk,
	struct config_params		*cfg_prm,
	int				allowed)

	{
	char				token[CWTOOL_MAX_NAME_LEN];

	if (! config_token(cfg, token, sizeof (token)))
		{
		if (allowed & CONFIG_DISK)  return (0);
		if (allowed & CONFIG_ENTER) config_error(cfg, "directive expected");
		if (allowed & CONFIG_LEAVE) config_error(cfg, "} expected");
		}
	if ((allowed & CONFIG_DISK)  && (string_equal(token, "disk"))) return (config_disk(cfg, dsk));
	if ((allowed & CONFIG_ENTER) && (string_equal(token, "{")))    return (config_enter(cfg, dsk, dsk_trk, cfg_prm, allowed));
	if ((allowed & CONFIG_LEAVE) && (string_equal(token, "}")))    return (0);
	if (allowed & CONFIG_TRACK)
		{
		if (string_equal(token, "info"))        return (config_info(cfg, dsk));
		if (string_equal(token, "copy"))        return (config_copy(cfg, dsk));
		if (string_equal(token, "image"))       return (config_image(cfg, dsk));
		if (string_equal(token, "track"))       return (config_track(cfg, dsk, dsk_trk));
		if (string_equal(token, "track_range")) return (config_track_range(cfg, dsk, dsk_trk));
		}
	if (allowed & CONFIG_RW)
		{
		if (string_equal(token, "format"))      return (config_format(cfg, dsk_trk));
		if (string_equal(token, "clock"))       return (config_clock(cfg, dsk_trk));
		if (string_equal(token, "timeout"))     return (config_timeout(cfg, dsk_trk));
		if (string_equal(token, "indexed"))     return (config_indexed(cfg, dsk_trk));
		if (string_equal(token, "flip_side"))   return (config_flip_side(cfg, dsk_trk));
		if (string_equal(token, "side_offset")) return (config_side_offset(cfg, dsk_trk));
		if (string_equal(token, "read"))        return (config_directive(cfg, NULL, dsk_trk, NULL, CONFIG_ENTER | CONFIG_READ));
		if (string_equal(token, "write"))       return (config_directive(cfg, NULL, dsk_trk, NULL, CONFIG_ENTER | CONFIG_WRITE));
		if (disk_get_format(dsk_trk) == NULL)   config_error_format(cfg);
		if (string_equal(token, "skew"))        return (config_skew(cfg, dsk_trk));
		if (string_equal(token, "interleave"))  return (config_interleave(cfg, dsk_trk));
		if (config_rw(cfg, dsk_trk, token))     return (1);
		}
	if (allowed & CONFIG_READ)
		{
		if (string_equal(token, "timeout"))   return (config_timeout_read(cfg, dsk_trk));
		if (string_equal(token, "indexed"))   return (config_indexed_read(cfg, dsk_trk));
		if (disk_get_format(dsk_trk) == NULL) config_error_format(cfg);
		if (config_read(cfg, dsk_trk, token)) return (1);
		}
	if (allowed & CONFIG_WRITE)
		{
		if (string_equal(token, "timeout"))    return (config_timeout_write(cfg, dsk_trk));
		if (string_equal(token, "indexed"))    return (config_indexed_write(cfg, dsk_trk));
		if (disk_get_format(dsk_trk) == NULL)  config_error_format(cfg);
		if (config_write(cfg, dsk_trk, token)) return (1);
		}
	if ((allowed & CONFIG_PARAMS) && (config_params(cfg, dsk_trk, cfg_prm, token))) return (1);
	config_error(cfg, "invalid directive '%s' within this scope", token);

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
 * config_parse
 ****************************************************************************/
void
config_parse(
	const char			*file,
	const char			*text,
	int				size)

	{
	struct config			cfg = CONFIG_INIT(file, text, size);
	int				version = disk_get_version();

	verbose(1, "reading config from '%s'", file);
	while (1)
		{
		struct disk		dsk;

		disk_init(&dsk, version);
		debug(1, "looking for next disk, version = %d", version);
		if (! config_directive(&cfg, &dsk, NULL, NULL, CONFIG_DISK)) break;
		disk_insert(&dsk);
		}
	}
/******************************************************** Karsten Scheibler */
