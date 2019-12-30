/****************************************************************************
 ****************************************************************************
 *
 * cwtool.c
 *
 ****************************************************************************
 *
 * very short documentation
 *
 * - struct disk is the center of all things, this struct contains the
 *   layout of all supported disks.
 * - struct disk has an entry for each track (each track may have a
 *   different format)
 * - L0: catweasel raw counter values
 *   L1: raw bits (FM, MFM, GCR, ..., converted from/to L0 with bounds)
 *   L2: decoded FM, MFM, GCR, ... (with checksums and sector headers)
 *   L3: sector data bytes only
 * - if a disk should be read, disk_read() is called, it opens the given
 *   source and destination file, source file has to be L0 (either
 *   catweasel device or file containing raw counter values), destination
 *   file may be L0 - L3 depending on the selected format
 * - disk_write() is more or less inverse, source file may be L0 - L3,
 *   depending on selected format, destination file has to be L0
 * - plain image formats as ADF or D64 are L3, G64 is L1
 * - config.c translates a textual config file to entries in struct disk
 *   beside the built in directives it also understands format specific
 *   directives passed via struct format_desc
 * - struct fifo is the container for the data and some side information,
 *   its functions support bit and byte operations suitable for decoding
 *   and encoding
 *
 ****************************************************************************
 ****************************************************************************/





#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "cwtool.h"
#include "error.h"
#include "debug.h"
#include "verbose.h"
#include "config.h"
#include "disk.h"
#include "file.h"
#include "string.h"
#include "version.h"



#define PROGRAM_NAME			"cwtool"
#define VERSION_STRING			PROGRAM_NAME " " VERSION "-" VERSION_DATE

#define CWTOOL_MAX_CONFIG_FILES		64
#define CWTOOL_MAX_CONFIG_EVALS		64
#define CWTOOL_MAX_CONFIG_SIZE		0x10000
#define CWTOOL_MODE_VERSION		1
#define CWTOOL_MODE_DUMP		2
#define CWTOOL_MODE_LIST		3
#define CWTOOL_MODE_STATISTICS		4
#define CWTOOL_MODE_READ		5
#define CWTOOL_MODE_WRITE		6



char					program_name[] = PROGRAM_NAME;
static int				mode;
static char				*disk_name;
static char				*path_src;
static char				*path_dst;
static int				params;
static int				no_rcfiles;
static char				*config_files[CWTOOL_MAX_CONFIG_FILES];
static int				nconfig_files;
static char				*config_evals[CWTOOL_MAX_CONFIG_EVALS];
static int				nconfig_evals;
static char				config_type[CWTOOL_MAX_CONFIG_FILES + CWTOOL_MAX_CONFIG_EVALS];
static int				nconfig_types;
static int				retry = 5;
static int				ignore_size;
static FILE				*info_fh;



/****************************************************************************
 * do_snprintf2
 ****************************************************************************/
static int
do_snprintf2(
	int				len,
	int				max)

	{
	if ((len == -1) || (len >= max)) error_message("do_snprintf() size exceeded");
	return (len);
	}



/****************************************************************************
 * do_snprintf
 ****************************************************************************/
#define do_snprintf(string, max, args...)	do_snprintf2(snprintf(string, max, args), max)



/****************************************************************************
 * config_read_file
 ****************************************************************************/
static void
config_read_file(
	const char			*path,
	int				no_error)

	{
	struct file			fil;
	char				data[CWTOOL_MAX_CONFIG_SIZE];
	int				mode = (no_error) ? FILE_MODE_READ_RETURN : FILE_MODE_READ;
	int				size;

	if (! file_open(&fil, path, mode)) return;
	size = file_read(&fil, (unsigned char *) data, sizeof (data));
	if (size == sizeof (data)) error("file '%s' too large for config", path);
	file_close(&fil);
	config_parse(path, data, size);
	}



/****************************************************************************
 * config_read_rc_files
 ****************************************************************************/
static void
config_read_rc_files(
	void)

	{
	char			path[0x4000], *home;

	config_read_file("/etc/cwtoolrc", 1);
	home = getenv("HOME");
	if (home == NULL) return;
	do_snprintf(path, sizeof (path), "%s/.cwtoolrc", home);
	config_read_file(path, 1);
	}



/****************************************************************************
 * config_get
 ****************************************************************************/
static void
config_get(
	void)

	{
	char				number[128];
	int				c, e, f;

	config_parse("(builtin config)", config_default, string_length(config_default));
	if (! no_rcfiles) config_read_rc_files();
	for (c = e = f = 0; c < nconfig_types; c++)
		{
		if (config_type[c] == 'f')
			{
			config_read_file(config_files[f++], 0);
			continue;
			}
		do_snprintf(number, sizeof (number), "-e/--evaluate #%d", e + 1);
		config_parse(number, config_evals[e], string_length(config_evals[e]));
		e++;
		}
	}



/****************************************************************************
 * cmd_version
 ****************************************************************************/
static void
cmd_version(
	void)

	{
	printf(VERSION_STRING "\n");
	}



/****************************************************************************
 * cmd_dump
 ****************************************************************************/
static void
cmd_dump(
	void)

	{
	printf("# builtin default config (" VERSION_STRING ")\n\n%s", config_default);
	}



/****************************************************************************
 * get_line
 ****************************************************************************/
static char *
get_line(
	struct disk			*dsk,
	char				*line,
	int				size)

	{
	const static char		space[] = "                ";
	char				*name = disk_get_name(dsk);
	char				*info = disk_get_info(dsk);
	int				i = 0;

	while ((name[i] != '\0') && (space[i] != '\0')) i++;
	if ((string_length(info) == 0) || (verbose_level < 0)) do_snprintf(line, size, "%s", name);
	else do_snprintf(line, size, "%s%s %s", name, &space[i], info);
	return (line);
	}



/****************************************************************************
 * cmd_list
 ****************************************************************************/
static void
cmd_list(
	void)

	{
	struct disk			*dsk;
	char				line[3 * CWTOOL_MAX_NAME_LEN];
	int				i;

	printf("Available disk names are:\n");
	for (i = 0; (dsk = disk_get(i)) != NULL; i++) printf("%s\n", get_line(dsk, line, sizeof (line)));
	}



/****************************************************************************
 * cmd_statistics
 ****************************************************************************/
static void
cmd_statistics(
	struct disk			*dsk)

	{
	disk_statistics(dsk, path_src);
	}



/****************************************************************************
 * info_init
 ****************************************************************************/
static void
info_init(
	void)

	{
	info_fh = fdopen(STDERR_FILENO, "a");
	if (info_fh == NULL) error_perror2("could not reopen stderr");
	setvbuf(info_fh, NULL, _IONBF, 0);
	}



/****************************************************************************
 * info_error_line
 ****************************************************************************/
#define info_error_line(args...)					\
	do								\
		{							\
		if ((verbose_level != 0) || (debug_level != 0))		\
			{						\
			verbose(0, args);				\
			break;						\
			}						\
									\
		/*							\
		 * using stderr instead of info_fh for line		\
		 * buffering						\
		 */							\
									\
		fprintf(stderr, args);					\
		fprintf(stderr, "\n");					\
		}							\
	while (0)



/****************************************************************************
 * info_error_sector
 ****************************************************************************/
static int
info_error_sector(
	char				*line,
	int				max,
	struct disk_sector_info		*dsk_sct_nfo,
	int				sector)

	{
	char				*reason = "mx";

	if (dsk_sct_nfo->flags == DISK_ERROR_FLAG_NOT_FOUND) reason = "nf";
	if (dsk_sct_nfo->flags == DISK_ERROR_FLAG_ENCODING)  reason = "en";
	if (dsk_sct_nfo->flags == DISK_ERROR_FLAG_ID)        reason = "id";
	if (dsk_sct_nfo->flags == DISK_ERROR_FLAG_NUMBERING) reason = "nu";
	if (dsk_sct_nfo->flags == DISK_ERROR_FLAG_SIZE)      reason = "si";
	if (dsk_sct_nfo->flags == DISK_ERROR_FLAG_CHECKSUM)  reason = "cs";
	return (do_snprintf(line, max, " %02d=%s@0x%06x", sector, reason, dsk_sct_nfo->offset));
	}



/****************************************************************************
 * info_error_details
 ****************************************************************************/
static void
info_error_details(
	struct disk_info		*dsk_nfo)

	{
	char				line[1024];
	int				i, l, s, t;

	info_error_line("details for bad sectors:");
	for (t = 0; t < CWTOOL_MAX_TRACK; t++)
		{
		for (i = s = 0; s < CWTOOL_MAX_SECTOR; s++) if (dsk_nfo->sct_nfo[t][s].flags) i++;
		if (i == 0) continue;
		for (s = 0; s < CWTOOL_MAX_SECTOR; )
			{
			l = do_snprintf(line, sizeof (line), "track %3d:", t);
			for (i = 0; (i < 4) && (s < CWTOOL_MAX_SECTOR); s++)
				{
				if (! dsk_nfo->sct_nfo[t][s].flags) continue;
				l += info_error_sector(&line[l], sizeof (line) - l, &dsk_nfo->sct_nfo[t][s], s);
				i++;
				}
			if (i > 0) info_error_line(line);
			}
		}
	}



/****************************************************************************
 * info_print
 ****************************************************************************/
static void
info_print(
	struct disk_info		*dsk_nfo,
	int				summary)

	{
	const static char		space[] = "                                                                               ";
	char				line[1024], end = '\r';
	int				selector = 0, i = 0;

	if (verbose_level < 0) return;

	if (mode == CWTOOL_MODE_WRITE) selector += 4;
	if (summary)
		{
		end = '\n';
		selector += 2;
		if (dsk_nfo->sum.sectors_good + dsk_nfo->sum.sectors_weak + dsk_nfo->sum.sectors_bad > 0) selector++;
		}
	else if (dsk_nfo->sectors_good + dsk_nfo->sectors_weak + dsk_nfo->sectors_bad > 0) selector++;

	if (selector == 0) do_snprintf(line, sizeof (line), "reading track %3d try %2d (sectors: none)",
		dsk_nfo->track, dsk_nfo->try);
	if (selector == 1) do_snprintf(line, sizeof (line), "reading track %3d try %2d (sectors: good %2d weak %2d bad %2d)",
		dsk_nfo->track, dsk_nfo->try, dsk_nfo->sectors_good,
		dsk_nfo->sectors_weak, dsk_nfo->sectors_bad);
	if (selector == 2) do_snprintf(line, sizeof (line), "%3d tracks read",
		dsk_nfo->sum.tracks);
	if (selector == 3) do_snprintf(line, sizeof (line), "%3d tracks read (sectors: good %4d weak %4d bad %4d)",
		dsk_nfo->sum.tracks, dsk_nfo->sum.sectors_good,
		dsk_nfo->sum.sectors_weak, dsk_nfo->sum.sectors_bad);
	if (selector == 4) do_snprintf(line, sizeof (line), "writing track %3d (sectors: none)",
		dsk_nfo->track);
	if (selector == 5) do_snprintf(line, sizeof (line), "writing track %3d (sectors: %2d)",
		dsk_nfo->track, dsk_nfo->sectors_good);
	if (selector == 6) do_snprintf(line, sizeof (line), "%3d tracks written",
		dsk_nfo->sum.tracks);
	if (selector == 7) do_snprintf(line, sizeof (line), "%3d tracks written (sectors: %4d)",
		dsk_nfo->sum.tracks, dsk_nfo->sum.sectors_good);

	while ((line[i] != '\0') && (space[i] != '\0')) i++;
	if ((verbose_level != 0) || (debug_level != 0)) verbose(0, "%s", line);
	else fprintf(info_fh, "%s%s%c", line, &space[i], end);

	if ((summary) && (dsk_nfo->sum.sectors_bad > 0)) info_error_details(dsk_nfo);
	}



/****************************************************************************
 * cmd_read
 ****************************************************************************/
static void
cmd_read(
	struct disk			*dsk)

	{
	int				flags = (ignore_size) ? DISK_OPTION_FLAG_IGNORE_SIZE : DISK_OPTION_FLAG_NONE;
	struct disk_option		dsk_opt = DISK_OPTION_INIT(info_print, retry, flags);

	disk_read(dsk, &dsk_opt, path_src, path_dst);
	}



/****************************************************************************
 * cmd_write
 ****************************************************************************/
static void
cmd_write(
	struct disk			*dsk)

	{
	int				flags = (ignore_size) ? DISK_OPTION_FLAG_IGNORE_SIZE : DISK_OPTION_FLAG_NONE;
	struct disk_option		dsk_opt = DISK_OPTION_INIT(info_print, retry, flags);

	disk_write(dsk, &dsk_opt, path_src, path_dst);
	}



/****************************************************************************
 * cmdline_fill_space
 ****************************************************************************/
static void
cmdline_fill_space(
	char				*string)

	{
	while (*string != '\0') *string++ = ' ';
	}



/****************************************************************************
 * cmdline_usage
 ****************************************************************************/
static void
cmdline_usage(
	void)

	{
	char				space1[] = VERSION_STRING;
	char				space2[] = PROGRAM_NAME;

	cmdline_fill_space(space1);
	cmdline_fill_space(space2);
	printf(VERSION_STRING " | written by Karsten Scheibler\n"
		"%s | http://unusedino.de/cw/\n"
		"%s | cw@unusedino.de\n\n"
		"Usage: %s -V\n"
		"or:    %s -D\n"
		"or:    %s -L [-v] [-n] [-f <file>] [-e <config>]\n"
		"or:    %s -S [-v] [-n] [-f <file>] [-e <config>]\n"
		"       %s    [--] <diskname> <srcfile>\n"
		"or:    %s -R [-v] [-n] [-f <file>] [-e <config>] [-r <num>]\n"
		"       %s    [--] <diskname> <srcfile> <dstfile>\n"
		"or:    %s -W [-v] [-n] [-f <file>] [-e <config>] [-s]\n"
		"       %s    [--] <diskname> <srcfile> <dstfile>\n\n"
		"  -V            print out version\n"
		"  -D            dump builtin config\n"
		"  -L            list available disk names\n"
		"  -S            print out statistics\n"
		"  -R            read disk\n"
		"  -W            write disk\n"
		"  -v            be more verbose\n"
#ifdef CWTOOL_DEBUG
		"  -d            increase debug level\n"
#endif /* CWTOOL_DEBUG */
		"  -n            do not read rc files\n"
		"  -f <file>     read additional config file\n"
		"  -e <config>   evaluate given string as config\n"
		"  -r <num>      number of retries if errors occur\n"
		"  -s            ignore size\n"
		"  -h            this help\n",
		space1, space1, program_name, program_name, program_name,
		program_name, space2, program_name, space2, program_name,
		space2);
	exit(0);
	}



/****************************************************************************
 * cmdline_check_arg
 ****************************************************************************/
static char *
cmdline_check_arg(
	char				*msg,
	char				*file)

	{
	if (file == NULL) error("%s expects a file name", msg);
	return (file);
	}



/****************************************************************************
 * cmdline_check_stdin
 ****************************************************************************/
static char *
cmdline_check_stdin(
	char				*msg,
	char				*file)

	{
	static int			stdin_count = 0;

	cmdline_check_arg(msg, file);
	if (string_equal(file, "-")) stdin_count++;
	if (stdin_count > 1) error("%s used stdin although already used before", msg);
	return (file);
	}



/****************************************************************************
 * cmdline_check_stdout
 ****************************************************************************/
static char *
cmdline_check_stdout(
	char				*msg,
	char				*file)

	{
	static int			stdout_count = 0;

	cmdline_check_arg(msg, file);
	if (string_equal(file, "-")) stdout_count++;
	if (stdout_count > 1) error("%s used stdout although already used before", msg);
	return (file);
	}



/****************************************************************************
 * cmdline_params
 ****************************************************************************/
static int
cmdline_params(
	void)

	{
	if (mode == CWTOOL_MODE_READ)       return (3);
	if (mode == CWTOOL_MODE_WRITE)      return (3);
	if (mode == CWTOOL_MODE_STATISTICS) return (2);
	return (0);
	}



/****************************************************************************
 * cmdline_parse
 ****************************************************************************/
static void
cmdline_parse(
	int				argc,
	char				**argv)

	{
	char				*arg;
	int				args, ignore = 0;

	for (args = 0, argv++; (arg = *argv++) != NULL; args++)
		{
		if ((arg[0] != '-') || (string_equal(arg, "-")) || (ignore))
			{
			if (mode == 0) goto bad_option;
			if (params >= cmdline_params()) error_message("too many parameters given");
			if (params == 0) disk_name = arg;
			if (params == 1) path_src = cmdline_check_stdin("<srcfile>", arg);
			if (params == 2) path_dst = cmdline_check_stdout("<dstfile>", arg);
			params++;
			}
		else if (string_equal(arg, "--"))
			{
			ignore = 1;
			}
		else if ((string_equal(arg, "-h")) || (string_equal(arg, "--help")))
			{
			cmdline_usage();
			}
		else if (((string_equal(arg, "-V")) || (string_equal(arg, "--version"))) && (args == 0))
			{
			mode = CWTOOL_MODE_VERSION;
			}
		else if (((string_equal(arg, "-D")) || (string_equal(arg, "--dump"))) && (args == 0))
			{
			mode = CWTOOL_MODE_DUMP;
			}
		else if (((string_equal(arg, "-L")) || (string_equal(arg, "--list"))) && (args == 0))
			{
			verbose_level = -1;
			mode = CWTOOL_MODE_LIST;
			}
		else if (((string_equal(arg, "-S")) || (string_equal(arg, "--statistics"))) && (args == 0))
			{
			verbose_level = -2;
			mode = CWTOOL_MODE_STATISTICS;
			}
		else if (((string_equal(arg, "-R")) || (string_equal(arg, "--read"))) && (args == 0))
			{
			verbose_level = -1;
			mode = CWTOOL_MODE_READ;
			}
		else if (((string_equal(arg, "-W")) || (string_equal(arg, "--write"))) && (args == 0))
			{
			verbose_level = -1;
			mode = CWTOOL_MODE_WRITE;
			}
		else if ((mode == 0) || (mode == CWTOOL_MODE_VERSION) || (mode == CWTOOL_MODE_DUMP))
			{
			goto bad_option;
			}
		else if ((string_equal(arg, "-v")) || (string_equal(arg, "--verbose")))
			{
			verbose_level++;
			}
#ifdef CWTOOL_DEBUG
		else if ((string_equal(arg, "-d")) || (string_equal(arg, "--debug")))
			{
			debug_level++;
			}
#endif /* CWTOOL_DEBUG */
		else if ((string_equal(arg, "-n")) || (string_equal(arg, "--no-rcfiles")))
			{
			no_rcfiles = 1;
			}
		else if ((string_equal(arg, "-f")) || (string_equal(arg, "--file")))
			{
			if (nconfig_files >= CWTOOL_MAX_CONFIG_FILES) error_message("too many config files specified");
			config_files[nconfig_files++] = cmdline_check_stdin("-f/--file", *argv++);
			config_type[nconfig_types++]  = 'f';
			}
		else if ((string_equal(arg, "-e")) || (string_equal(arg, "--evaluate")))
			{
			if (nconfig_evals >= CWTOOL_MAX_CONFIG_EVALS) error_message("too many -e/--evaluate options");
			config_evals[nconfig_evals++] = cmdline_check_stdin("-e/--evaluate", *argv++);
			config_type[nconfig_types++]  = 'e';
			}
		else if (((string_equal(arg, "-r")) || (string_equal(arg, "--retry"))) && (mode == CWTOOL_MODE_READ))
			{
			int		i = 0;

			if (*argv != NULL) i = sscanf(*argv++, "%d", &retry);
			if ((i != 1) || (retry < 0) || (retry > 99)) error_message("-r/--retry expects a valid number of retries");
			}
		else if (((string_equal(arg, "-s")) || (string_equal(arg, "--ignore-size"))) && (mode == CWTOOL_MODE_WRITE))
			{
			ignore_size = 1;
			}
		else
			{
		bad_option:
			error("unrecognized option '%s'", arg);
			}
		}
	if ((params < cmdline_params()) || (mode == 0)) error_message("too few parameters given");
	}



/****************************************************************************
 * main
 ****************************************************************************/
int
main(
	int				argc,
	char				**argv)

	{
	struct disk			*dsk;

	setlinebuf(stderr);

	/* parse cmdline */

	cmdline_parse(argc, argv);

	/* decide what to do */

	if (mode == CWTOOL_MODE_VERSION)
		{
		cmd_version();
		goto end;
		}
	if (mode == CWTOOL_MODE_DUMP)
		{
		cmd_dump();
		goto end;
		}
	config_get();
	if (mode == CWTOOL_MODE_LIST)
		{
		cmd_list();
		goto end;
		}
	dsk = disk_search(disk_name);
	if (dsk == NULL) error("unknown disk name '%s'", disk_name);
	if (mode == CWTOOL_MODE_STATISTICS)
		{
		cmd_statistics(dsk);
		goto end;
		}
	info_init();
	if (mode == CWTOOL_MODE_READ)
		{
		cmd_read(dsk);
		goto end;
		}
	if (mode == CWTOOL_MODE_WRITE)
		{
		cmd_write(dsk);
		goto end;
		}
	debug_error();
end:
	return (0);
	}
/******************************************************** Karsten Scheibler */
