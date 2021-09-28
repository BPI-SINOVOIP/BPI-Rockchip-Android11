# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import tempfile

from autotest_lib.client.common_lib import file_utils

_URL_BASE = ('https://sites.google.com/a/chromium.org/dev/chromium-os'
             '/testing/power-testing/pltp')
_PLTG_URL = _URL_BASE + '/pltg'
_PLTU_URL = _URL_BASE + '/pltu'
_PLTP_URL = _URL_BASE + '/pltp'


def _get_content(url):
    """Reads the content of the file at the given |URL|.

    Args:
        url: URL to be fetched.

    Return:
        The content of the fetched file.
    """
    with tempfile.NamedTemporaryFile() as named_file:
        file_utils.download_file(url, named_file.name)
        return named_file.read().rstrip()


def use_gaia_login():
    """Returns whether Gaia login should be used by default for load testing."""
    res = _get_content(_PLTG_URL)
    return res == 'True' or res == 'true'


def get_username():
    """Returns username for load testing."""
    return _get_content(_PLTU_URL)


def get_password():
    """Returns password for load testing."""
    return _get_content(_PLTP_URL)
