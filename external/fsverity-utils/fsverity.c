// SPDX-License-Identifier: GPL-2.0+
/*
 * fs-verity userspace tool
 *
 * Copyright (C) 2018 Google LLC
 *
 * Written by Eric Biggers.
 */

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "commands.h"
#include "hash_algs.h"

static const struct fsverity_command {
	const char *name;
	int (*func)(const struct fsverity_command *cmd, int argc, char *argv[]);
	const char *short_desc;
	const char *usage_str;
} fsverity_commands[] = {
	{
		.name = "enable",
		.func = fsverity_cmd_enable,
		.short_desc = "Enable fs-verity on a file",
		.usage_str =
"    fsverity enable FILE\n"
"               [--hash-alg=HASH_ALG] [--block-size=BLOCK_SIZE] [--salt=SALT]\n"
"               [--signature=SIGFILE]\n"
	}, {
		.name = "measure",
		.func = fsverity_cmd_measure,
		.short_desc =
"Display the measurement of the given verity file(s)",
		.usage_str =
"    fsverity measure FILE...\n"
	}, {
		.name = "sign",
		.func = fsverity_cmd_sign,
		.short_desc = "Sign a file for fs-verity",
		.usage_str =
"    fsverity sign FILE OUT_SIGFILE --key=KEYFILE\n"
"               [--hash-alg=HASH_ALG] [--block-size=BLOCK_SIZE] [--salt=SALT]\n"
"               [--cert=CERTFILE]\n"
	}
};

static void usage_all(FILE *fp)
{
	int i;

	fputs("Usage:\n", fp);
	for (i = 0; i < ARRAY_SIZE(fsverity_commands); i++)
		fprintf(fp, "  %s:\n%s\n", fsverity_commands[i].short_desc,
			fsverity_commands[i].usage_str);
	fputs(
"  Standard options:\n"
"    fsverity --help\n"
"    fsverity --version\n"
"\n"
"Available hash algorithms: ", fp);
	show_all_hash_algs(fp);
	fputs("\nSee `man fsverity` for more details.\n", fp);
}

static void usage_cmd(const struct fsverity_command *cmd, FILE *fp)
{
	fprintf(fp, "Usage:\n%s", cmd->usage_str);
}

void usage(const struct fsverity_command *cmd, FILE *fp)
{
	if (cmd)
		usage_cmd(cmd, fp);
	else
		usage_all(fp);
}

#define PACKAGE_VERSION    "v0.0-alpha"
#define PACKAGE_BUGREPORT  "linux-fscrypt@vger.kernel.org"

static void show_version(void)
{
	static const char * const str =
"fsverity " PACKAGE_VERSION "\n"
"Copyright (C) 2018 Google LLC\n"
"License GPLv2+: GNU GPL version 2 or later <http://gnu.org/licenses/gpl.html>.\n"
"This is free software: you are free to change and redistribute it.\n"
"There is NO WARRANTY, to the extent permitted by law.\n"
"\n"
"Report bugs to " PACKAGE_BUGREPORT ".\n";
	fputs(str, stdout);
}

static void handle_common_options(int argc, char *argv[],
				  const struct fsverity_command *cmd)
{
	int i;

	for (i = 1; i < argc; i++) {
		const char *arg = argv[i];

		if (*arg++ != '-')
			continue;
		if (*arg++ != '-')
			continue;
		if (!strcmp(arg, "help")) {
			usage(cmd, stdout);
			exit(0);
		} else if (!strcmp(arg, "version")) {
			show_version();
			exit(0);
		} else if (!*arg) /* reached "--", no more options */
			return;
	}
}

static const struct fsverity_command *find_command(const char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(fsverity_commands); i++)
		if (!strcmp(name, fsverity_commands[i].name))
			return &fsverity_commands[i];
	return NULL;
}

bool parse_block_size_option(const char *arg, u32 *size_ptr)
{
	char *end;
	unsigned long n = strtoul(arg, &end, 10);

	if (*size_ptr != 0) {
		error_msg("--block-size can only be specified once");
		return false;
	}

	if (n <= 0 || n >= INT_MAX || !is_power_of_2(n) || *end != '\0') {
		error_msg("Invalid block size: %s.  Must be power of 2", arg);
		return false;
	}
	*size_ptr = n;
	return true;
}

bool parse_salt_option(const char *arg, u8 **salt_ptr, u32 *salt_size_ptr)
{
	if (*salt_ptr != NULL) {
		error_msg("--salt can only be specified once");
		return false;
	}
	*salt_size_ptr = strlen(arg) / 2;
	*salt_ptr = xmalloc(*salt_size_ptr);
	if (!hex2bin(arg, *salt_ptr, *salt_size_ptr)) {
		error_msg("salt is not a valid hex string");
		return false;
	}
	return true;
}

u32 get_default_block_size(void)
{
	long n = sysconf(_SC_PAGESIZE);

	if (n <= 0 || n >= INT_MAX || !is_power_of_2(n)) {
		fprintf(stderr,
			"Warning: invalid _SC_PAGESIZE (%ld).  Assuming 4K blocks.\n",
			n);
		return 4096;
	}
	return n;
}

int main(int argc, char *argv[])
{
	const struct fsverity_command *cmd;

	if (argc < 2) {
		error_msg("no command specified");
		usage_all(stderr);
		return 2;
	}

	cmd = find_command(argv[1]);

	handle_common_options(argc, argv, cmd);

	if (!cmd) {
		error_msg("unrecognized command: '%s'", argv[1]);
		usage_all(stderr);
		return 2;
	}
	return cmd->func(cmd, argc - 1, argv + 1);
}
