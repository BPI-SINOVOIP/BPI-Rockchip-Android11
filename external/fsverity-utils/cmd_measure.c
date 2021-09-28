// SPDX-License-Identifier: GPL-2.0+
/*
 * The 'fsverity measure' command
 *
 * Copyright (C) 2018 Google LLC
 *
 * Written by Eric Biggers.
 */

#include <fcntl.h>
#include <stdlib.h>
#include <sys/ioctl.h>

#include "commands.h"
#include "fsverity_uapi.h"
#include "hash_algs.h"

/* Display the measurement of the given verity file(s). */
int fsverity_cmd_measure(const struct fsverity_command *cmd,
			 int argc, char *argv[])
{
	struct fsverity_digest *d = NULL;
	struct filedes file;
	char digest_hex[FS_VERITY_MAX_DIGEST_SIZE * 2 + 1];
	const struct fsverity_hash_alg *hash_alg;
	char _hash_alg_name[32];
	const char *hash_alg_name;
	int status;
	int i;

	if (argc < 2)
		goto out_usage;

	d = xzalloc(sizeof(*d) + FS_VERITY_MAX_DIGEST_SIZE);

	for (i = 1; i < argc; i++) {
		d->digest_size = FS_VERITY_MAX_DIGEST_SIZE;

		if (!open_file(&file, argv[i], O_RDONLY, 0))
			goto out_err;
		if (ioctl(file.fd, FS_IOC_MEASURE_VERITY, d) != 0) {
			error_msg_errno("FS_IOC_MEASURE_VERITY failed on '%s'",
					file.name);
			filedes_close(&file);
			goto out_err;
		}
		filedes_close(&file);

		ASSERT(d->digest_size <= FS_VERITY_MAX_DIGEST_SIZE);
		bin2hex(d->digest, d->digest_size, digest_hex);
		hash_alg = find_hash_alg_by_num(d->digest_algorithm);
		if (hash_alg) {
			hash_alg_name = hash_alg->name;
		} else {
			sprintf(_hash_alg_name, "ALG_%u", d->digest_algorithm);
			hash_alg_name = _hash_alg_name;
		}
		printf("%s:%s %s\n", hash_alg_name, digest_hex, argv[i]);
	}
	status = 0;
out:
	free(d);
	return status;

out_err:
	status = 1;
	goto out;

out_usage:
	usage(cmd, stderr);
	status = 2;
	goto out;
}
