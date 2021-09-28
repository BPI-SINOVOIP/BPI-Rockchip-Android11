# Copyright (c) 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function
from __future__ import unicode_literals

import common
from autotest_lib.client.common_lib.global_config import global_config

FROM_AFE = "FROM_AFE"
FROM_HOST_CONFIG = "FROM_HOST_CONFIG"


def _config(_config_override):
        config = global_config if _config_override is None else _config_override
        enabled = config.get_config_value(
            'CROS', 'stable_version_config_repo_enable', type=bool, default=False
        )
        return config, enabled


def classify_board(board, _config_override=None):
    """
    determine what the appropriate information source is for a given board.

    @param board            string -- board name
    @param _config_override        -- optional global config object

    @returns FROM_AFE or FROM_HOST_CONFIG
    """
    config, enabled = _config(_config_override)
    if enabled:
        boards = config.get_config_value(
            'CROS', 'stable_version_config_repo_opt_in_boards', type=list, default=[],
        )
        if ':all' in boards or board in boards:
            return FROM_HOST_CONFIG
    return FROM_AFE


def classify_model(model, _config_override=None):
    """
    determine what the appropriate information source is for a given model.

    @param board            string -- board name
    @param _config_override        -- optional global config object

    @returns FROM_AFE or FROM_HOST_CONFIG
    """
    config, enabled = _config(_config_override)
    if enabled:
        models = config.get_config_value(
            'CROS', 'stable_version_config_repo_opt_in_models', type=list, default=[],
        )
        if ':all' in models or model in models:
            return FROM_HOST_CONFIG
    return FROM_AFE
