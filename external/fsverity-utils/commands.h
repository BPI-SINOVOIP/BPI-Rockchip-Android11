/* SPDX-License-Identifier: GPL-2.0+ */
#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdio.h>

#include "util.h"

struct fsverity_command;

void usage(const struct fsverity_command *cmd, FILE *fp);

int fsverity_cmd_enable(const struct fsverity_command *cmd,
			int argc, char *argv[]);
int fsverity_cmd_measure(const struct fsverity_command *cmd,
			 int argc, char *argv[]);
int fsverity_cmd_sign(const struct fsverity_command *cmd,
		      int argc, char *argv[]);

bool parse_block_size_option(const char *arg, u32 *size_ptr);
u32 get_default_block_size(void);
bool parse_salt_option(const char *arg, u8 **salt_ptr, u32 *salt_size_ptr);

#endif /* COMMANDS_H */
