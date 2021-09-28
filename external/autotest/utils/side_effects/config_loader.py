# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""This module provides functions for loading side_effects_config.json.
"""

import errno
import os

from google.protobuf import json_format

import common
from autotest_lib.utils.side_effects.proto import config_pb2

_SIDE_EFFECTS_CONFIG_FILE = 'side_effects_config.json'


def load(results_dir):
    """Load a side_effects_config.json file.

    @param results_dir: The path to the results directory containing the file.

    @returns: a side_effects.Config proto object if side_effects_config.json
              exists, None otherwise.

    @raises: json_format.ParseError if the content of side_effects_config.json
             is not valid JSON.
    """
    config_path = os.path.join(results_dir, _SIDE_EFFECTS_CONFIG_FILE)

    if not os.path.exists(config_path):
        return None

    with open(config_path, 'r') as config_file:
        content = config_file.read()
        config = config_pb2.Config()
        return json_format.Parse(content, config, ignore_unknown_fields=True)


def validate_tko(config):
    """Validate the tko field of the side_effects.Config.

    @param config: A side_effects.Config proto.

    @raises: ValueError if the tko field does not contain all required fields.
             OSError if one of the required files is missing.
    """
    _check_empty_fields({
        'TKO proxy socket': config.tko.proxy_socket,
        'TKO MySQL user': config.tko.mysql_user,
        'TKO MySQL password file': config.tko.mysql_password_file
    })

    _check_file_existence({
        'TKO proxy socket': config.tko.proxy_socket,
        'TKO MySQL password file': config.tko.mysql_password_file
    })


def validate_google_storage(config):
    """Validate the google_storage field of the side_effects.Config.

    @param config: A side_effects.Config proto.

    @raises: ValueError if the tko field does not contain all required fields.
             OSError if one of the required files is missing.
    """
    _check_empty_fields({
        'Google Storage bucket': config.google_storage.bucket,
        'Google Storage credentials file':
            config.google_storage.credentials_file
    })

    _check_file_existence({
        'Google Storage credentials file':
            config.google_storage.credentials_file
    })


def _check_empty_fields(fields):
    """Return a list of missing required TKO-related fields.

    @param fields: A dict mapping string field descriptions to string field
                   values.

    @raises: ValueError if at least one of the field values is empty.
    """
    empty_fields = []
    for description, value in fields.items():
        if not value:
            empty_fields.append(description)
    if empty_fields:
        raise ValueError('Missing required fields: ' + ', '.join(empty_fields))


def _check_file_existence(files):
    """Checks that all given files exist.

    @param files: A dict mapping string file descriptions to string file names.

    @raises: OSError if at least one of the files is missing.
    """
    missing_files = []
    for description, path in files.items():
        if not os.path.exists(path):
            missing_files.append(description + ': ' + path)
    if missing_files:
        raise OSError(errno.ENOENT, os.strerror(errno.ENOENT),
                      ', '.join(missing_files))
