/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef CRAS_BOARD_CONFIG_H_
#define CRAS_BOARD_CONFIG_H_

#include <stdint.h>

struct cras_board_config {
	int32_t default_output_buffer_size;
	int32_t aec_supported;
	int32_t aec_group_id;
};

/* Gets a configuration based on the config file specified.
 * Args:
 *    config_path - Path containing the config files.
 *    board_config - The returned configs.
 */
void cras_board_config_get(const char *config_path,
			   struct cras_board_config *board_config);

#endif /* CRAS_BOARD_CONFIG_H_ */
