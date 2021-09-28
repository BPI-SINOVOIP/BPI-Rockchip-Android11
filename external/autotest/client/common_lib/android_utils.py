# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
This module provides utilities needed to provision and run test on Android
devices.
"""


import logging
import re

import common
from autotest_lib.client.common_lib import global_config


CONFIG = global_config.global_config

def get_config_value_regex(section, regex):
    """Get config values from global config based on regex of the key.

    @param section: Section of the config, e.g., CLIENT.
    @param regex: Regular expression of the key pattern.

    @return: A dictionary of all config values matching the regex. Value is
             assumed to be comma separated, and is converted to a list.
    """
    configs = CONFIG.get_config_value_regex(section, regex)
    result = {}
    for key, value in configs.items():
        match = re.match(regex, key)
        result[match.group(1)] = [v.strip() for v in value.split(',')
                                  if v.strip()]
    return result


class AndroidAliases(object):
    """Wrapper class for getting alias names for an android device.

    On android it is only possible to get a devices product name
    (eg. marlin, sailfish). However a product may have several aliases
    that it is called by such as the name of its board, or a public name,
    etc. This wrapper allows for mapping the product name to different
    aliases.

    Terms:
        product: The name a device reports itself as.
        board: The name of the hardware board in a device.
        alias: Some name a device is called, this includes both product and
               board.
    """

    # regex pattern for CLIENT/android_board_name[product]. For example,
    # global config can have following config in CLIENT section to indicate that
    # android product `zzz` has following board name.
    # xyz.
    # android_board_name_zzz: xyz
    BOARD_NAME_PATTERN = 'android_board_name_(.*)'


    # A dict of product:board for product board names, can be defined in global
    # config CLIENT/android_board_name_[product]
    board_name_map = get_config_value_regex('CLIENT', BOARD_NAME_PATTERN)

    @classmethod
    def get_board_name(cls, product):
        """Get the board name of a product.

        The board name of an android device is what the hardware is named.
        In many cases this is the same name as the reported product name,
        however some devices have boards that differ from the product name.

        @param product: The name of the product.
        @returns: The board name of the given product.
        """
        boards = cls.board_name_map.get(product, None)
        if boards:
            return boards[0]
        return product


class AndroidArtifacts(object):
    """A wrapper class for constants and methods related to artifacts.
    """

    BOOTLOADER_IMAGE = 'bootloader_image'
    DTB = 'dtb'
    RADIO_IMAGE = 'radio_image'
    TARGET_FILES = 'target_files'
    VENDOR_PARTITIONS = 'vendor_partitions'
    ZIP_IMAGE = 'zip_images'

    # (os, board) = 'artifacts'
    DEFAULT_ARTIFACTS_MAP = {
        ('android', 'default'): [BOOTLOADER_IMAGE, RADIO_IMAGE, ZIP_IMAGE],
        ('brillo', 'default'):  [ZIP_IMAGE, VENDOR_PARTITIONS],
        ('emulated_brillo', 'default'): [TARGET_FILES, DTB],
    }

    # Default artifacts for Android provision
    DEFAULT_ARTIFACTS_TO_BE_STAGED_FOR_IMAGE = (
            ','.join([BOOTLOADER_IMAGE, RADIO_IMAGE, ZIP_IMAGE]))

    # regex pattern for CLIENT/android_artifacts_[board]. For example, global
    # config can have following config in CLIENT section to indicate that
    # android board `xyz` needs to stage artifacts
    # ['bootloader_image', 'radio_image'] for provision.
    # android_artifacts_xyz: bootloader_image,radio_image
    ARTIFACTS_LIST_PATTERN = 'android_artifacts_(.*)'

    # A dict of board:artifacts, can be defined in global config
    # CLIENT/android_artifacts_[board]
    artifacts_map = get_config_value_regex('CLIENT', ARTIFACTS_LIST_PATTERN)

    @classmethod
    def get_artifacts_for_reimage(cls, board, os='android'):
        """Get artifacts need to be staged for reimage for given board.

        @param board: Name of the board.

        @return: A string of artifacts to be staged.
        """
        logging.debug('artifacts for %s %s', os, board)
        if board in cls.artifacts_map:
            logging.debug('Found override of artifacts for board %s: %s', board,
                          cls.artifacts_map[board])
            artifacts = cls.artifacts_map[board]
        elif (os, board) in cls.DEFAULT_ARTIFACTS_MAP:
            artifacts = cls.DEFAULT_ARTIFACTS_MAP[(os, board)]
        else:
            artifacts = cls.DEFAULT_ARTIFACTS_MAP[(os, 'default')]
        logging.debug('found %s', ','.join(artifacts))
        return ','.join(artifacts)
