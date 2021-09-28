# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file contains utility functions to get and set stable versions for given
# boards.

import common
import logging
import django.core.exceptions
from autotest_lib.client.common_lib import global_config
from autotest_lib.frontend import setup_django_environment
from autotest_lib.frontend.afe import models


# Name of the default board. For boards that don't have stable version
# explicitly set, version for the default board will be used.
DEFAULT = 'DEFAULT'

# Type of metadata to store stable_version changes.
_STABLE_VERSION_TYPE = 'stable_version'

def get_all():
    """Get stable versions of all boards.

    @return: A dictionary of boards and stable versions.
    """
    versions = dict([(v.board, v.version)
                     for v in models.StableVersion.objects.all()])
    # Set default to the global config value of CROS.stable_cros_version if
    # there is no entry in afe_stable_versions table.
    if not versions:
        versions = {DEFAULT: global_config.global_config.get_config_value(
                            'CROS', 'stable_cros_version')}
    return versions


def get(board=DEFAULT):
    """Get stable version for the given board.

    @param board: Name of the board, default to value `DEFAULT`.

    @return: Stable version of the given board. If the given board is not listed
             in afe_stable_versions table, DEFAULT will be used.
             Return global_config value of CROS.stable_cros_version if
             afe_stable_versions table does not have entry of board DEFAULT.
    """
    try:
        return models.StableVersion.objects.get(board=board).version
    except django.core.exceptions.ObjectDoesNotExist:
        if board == DEFAULT:
            return global_config.global_config.get_config_value(
                    'CROS', 'stable_cros_version')
        else:
            return get(board=DEFAULT)


def set(version, board=DEFAULT):
    """Set stable version for the given board.

    @param version: The new value of stable version for given board.
    @param board: Name of the board, default to value `DEFAULT`.
    """

    logging.warning("stable_version_utils::set: attmpted to set stable version. setting the stable version is not permitted")
    return None    


def delete(board):
    """Delete stable version record for the given board.

    @param board: Name of the board.
    """
    logging.warning("stable_version_utils::set: attmpted to delete stable version. deleting the stable version is not permitted")
    return None
