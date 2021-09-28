#ifndef RUNNER_SETTINGS_H
#define RUNNER_SETTINGS_H

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>
#include <stdio.h>
#include <glib.h>

enum {
	LOG_LEVEL_NORMAL = 0,
	LOG_LEVEL_QUIET = -1,
	LOG_LEVEL_VERBOSE = 1,
};

#define ABORT_TAINT   (1 << 0)
#define ABORT_LOCKDEP (1 << 1)
#define ABORT_ALL     (ABORT_TAINT | ABORT_LOCKDEP)

_Static_assert(ABORT_ALL == (ABORT_TAINT | ABORT_LOCKDEP), "ABORT_ALL must be all conditions bitwise or'd");

struct regex_list {
	char **regex_strings;
	GRegex **regexes;
	size_t size;
};

struct settings {
	int abort_mask;
	char *test_list;
	char *name;
	bool dry_run;
	struct regex_list include_regexes;
	struct regex_list exclude_regexes;
	bool sync;
	int log_level;
	bool overwrite;
	bool multiple_mode;
	int inactivity_timeout;
	int overall_timeout;
	bool use_watchdog;
	char *test_root;
	char *results_path;
	bool piglit_style_dmesg;
	int dmesg_warn_level;
	bool list_all;
};

/**
 * init_settings:
 *
 * Initializes a settings object to an empty state (all values NULL, 0
 * or false).
 *
 * @settings: Object to initialize. Storage for it must exist.
 */
void init_settings(struct settings *settings);

/**
 * free_settings:
 *
 * Releases all allocated resources for a settings object and
 * initializes it to an empty state (see #init_settings).
 *
 * @settings: Object to release and initialize.
 */
void free_settings(struct settings *settings);

/**
 * parse_options:
 *
 * Parses command line options and sets the settings object to
 * designated values.
 *
 * The function can be called again on the same settings object. The
 * old values will be properly released and cleared. On a parse
 * failure, the settings object will be in an empty state (see
 * #init_settings) and usage instructions will be printed with an
 * error message.
 *
 * @argc: Argument count
 * @argv: Argument array. First element is the program name.
 * @settings: Settings object to fill with values. Must have proper
 * storage.
 *
 * Returns: True on successful parse, false on error.
 */
bool parse_options(int argc, char **argv,
		   struct settings *settings);

/**
 * validate_settings:
 *
 * Checks the settings object against the system to see if executing
 * on it can be done. Checks pathnames for existence and access
 * rights. Note that this function will not check that the designated
 * job listing (through a test-list file or the -t/-x flags) yields a
 * non-zero amount of testing to be done. On errors, usage
 * instructions will be printed with an error message.
 *
 * @settings: Settings object to check.
 *
 * Returns: True on valid settings, false on any error.
 */
bool validate_settings(struct settings *settings);

/* TODO: Better place for this */
char *absolute_path(char *path);

/**
 * serialize_settings:
 *
 * Serializes the settings object to a file in the results_path
 * directory.
 *
 * @settings: Settings object to serialize.
 */
bool serialize_settings(struct settings *settings);

bool read_settings_from_file(struct settings *settings, FILE* f);
bool read_settings_from_dir(struct settings *settings, int dirfd);

#endif
