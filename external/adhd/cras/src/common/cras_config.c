/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <pwd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

#include "cras_config.h"

const char *cras_config_get_system_socket_file_dir()
{
	/* This directory is created by the upstart script, eventually it would
	 * be nice to make this more dynamic, but it isn't needed right now for
	 * Chrome OS. */
	return CRAS_SOCKET_FILE_DIR;
}

int cras_fill_socket_path(enum CRAS_CONNECTION_TYPE conn_type, char *sock_path)
{
	const char *sock_file;
	const char *sock_dir;

	sock_dir = cras_config_get_system_socket_file_dir();
	if (sock_dir == NULL) {
		return -ENOTDIR;
	}

	switch (conn_type) {
	case CRAS_CONTROL:
		sock_file = CRAS_SOCKET_FILE;
		break;
	case CRAS_PLAYBACK:
		sock_file = CRAS_PLAYBACK_SOCKET_FILE;
		break;
	case CRAS_CAPTURE:
		sock_file = CRAS_CAPTURE_SOCKET_FILE;
		break;
	default:
		return -EINVAL;
	}

	snprintf(sock_path, CRAS_MAX_SOCKET_PATH_SIZE, "%s/%s", sock_dir,
		 sock_file);

	return 0;
}
