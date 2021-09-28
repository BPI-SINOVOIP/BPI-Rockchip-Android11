/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <syslog.h>

#include "cras_board_config.h"
#include "iniparser_wrapper.h"

/* Allocate 63 chars + 1 for null where declared. */
static const unsigned int MAX_INI_NAME_LEN = 63;
static const unsigned int MAX_KEY_LEN = 63;
static const int32_t DEFAULT_OUTPUT_BUFFER_SIZE = 512;
static const int32_t AEC_SUPPORTED_DEFAULT = 0;
static const int32_t AEC_GROUP_ID_DEFAULT = -1;

#define CONFIG_NAME "board.ini"
#define DEFAULT_OUTPUT_BUF_SIZE_INI_KEY "output:default_output_buffer_size"
#define AEC_SUPPORTED_INI_KEY "processing:aec_supported"
#define AEC_GROUP_ID_INI_KEY "processing:group_id"

void cras_board_config_get(const char *config_path,
			   struct cras_board_config *board_config)
{
	char ini_name[MAX_INI_NAME_LEN + 1];
	char ini_key[MAX_KEY_LEN + 1];
	dictionary *ini;

	board_config->default_output_buffer_size = DEFAULT_OUTPUT_BUFFER_SIZE;
	board_config->aec_supported = AEC_SUPPORTED_DEFAULT;
	board_config->aec_group_id = AEC_GROUP_ID_DEFAULT;
	if (config_path == NULL)
		return;

	snprintf(ini_name, MAX_INI_NAME_LEN, "%s/%s", config_path, CONFIG_NAME);
	ini_name[MAX_INI_NAME_LEN] = '\0';
	ini = iniparser_load_wrapper(ini_name);
	if (ini == NULL) {
		syslog(LOG_DEBUG, "No ini file %s", ini_name);
		return;
	}

	snprintf(ini_key, MAX_KEY_LEN, DEFAULT_OUTPUT_BUF_SIZE_INI_KEY);
	ini_key[MAX_KEY_LEN] = 0;
	board_config->default_output_buffer_size =
		iniparser_getint(ini, ini_key, DEFAULT_OUTPUT_BUFFER_SIZE);

	snprintf(ini_key, MAX_KEY_LEN, AEC_SUPPORTED_INI_KEY);
	ini_key[MAX_KEY_LEN] = 0;
	board_config->aec_supported =
		iniparser_getint(ini, ini_key, AEC_SUPPORTED_DEFAULT);

	snprintf(ini_key, MAX_KEY_LEN, AEC_GROUP_ID_INI_KEY);
	ini_key[MAX_KEY_LEN] = 0;
	board_config->aec_group_id =
		iniparser_getint(ini, ini_key, AEC_GROUP_ID_DEFAULT);

	iniparser_freedict(ini);
	syslog(LOG_DEBUG, "Loaded ini file %s", ini_name);
}
