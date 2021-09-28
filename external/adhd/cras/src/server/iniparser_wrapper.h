/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef INIPARSER_WRAPPER_H_
#define INIPARSER_WRAPPER_H_

#include <iniparser.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static inline dictionary *iniparser_load_wrapper(const char *ini_name)
{
	struct stat st;
	int rc = stat(ini_name, &st);
	if (rc < 0)
		return NULL;
	return iniparser_load(ini_name);
}

#endif /* INIPARSER_WRAPPER_H_ */
