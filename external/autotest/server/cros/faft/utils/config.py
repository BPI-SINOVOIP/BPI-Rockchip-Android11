# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import os

import common

CONFIG_DIR = os.path.join(
        os.path.dirname(os.path.realpath(__file__)), os.pardir, 'configs')


def _get_config_filepath(platform):
    """Find the JSON file containing the platform's config"""
    return os.path.join(CONFIG_DIR, '%s.json' % platform)


def _has_config_file(platform):
    """Determine whether the platform has a config file"""
    return os.path.isfile(_get_config_filepath(platform))


def _load_config(platform):
    """Load the platform's JSON config into a dict"""
    fp = _get_config_filepath(platform)
    with open(fp) as config_file:
        return json.load(config_file)


class Config(object):
    """Configuration for FAFT tests.

    This object is meant to be the interface to all configuration required
    by FAFT tests, including device specific overrides.

    It gets the values from the JSON files in CONFIG_DIR.
    Default values are declared in the DEFAULTS.json.
    Platform-specific overrides come from <platform>.json.
    If the platform has model-specific overrides, then those take precedence
    over the platform's config.
    If the platform inherits overrides from a parent platform, then the child
    platform's overrides take precedence over the parent's.

    TODO(gredelston): Move the JSON out of this Autotest, as per
    go/cros-fw-testing-configs

    @ivar platform: string containing the board name being tested.
    @ivar model: string containing the model name being tested
    """

    def __init__(self, platform, model=None):
        """Initialize an object with FAFT settings.

        @param platform: The name of the platform being tested.
        """
        # Load JSON in order of importance (model, platform, parent/s, DEFAULTS)
        self.platform = platform.rsplit('_', 1)[-1].lower().replace("-", "_")
        self._precedence_list = []
        self._precedence_names = []
        if _has_config_file(self.platform):
            platform_config = _load_config(self.platform)
            self._add_cfg_to_precedence(self.platform, platform_config)
            model_configs = platform_config.get('models', {})
            model_config = model_configs.get(model, None)
            if model_config is not None:
                self._add_cfg_to_precedence(
                        'MODEL:%s' % model, model_config, prepend=True)
                logging.debug('Using model override for %s', model)
            parent_platform = self._precedence_list[-1].get('parent', None)
            while parent_platform is not None:
                parent_config = _load_config(parent_platform)
                self._add_cfg_to_precedence(parent_platform, parent_config)
                parent_platform = self._precedence_list[-1].get('parent', None)
        else:
            logging.debug(
                    'No platform config file found at %s. Using default.',
                    _get_config_filepath(self.platform))
        default_config = _load_config('DEFAULTS')
        self._add_cfg_to_precedence('DEFAULTS', default_config)

        # Set attributes
        all_attributes = self._precedence_list[-1].keys()
        self.attributes = {}
        self.attributes['platform'] = self.platform
        for attribute in all_attributes:
            if attribute.endswith('.DOC') or attribute == 'models':
                continue
            for config_dict in self._precedence_list:
                if attribute in config_dict:
                    self.attributes[attribute] = config_dict[attribute]
                    break

    def _add_cfg_to_precedence(self, cfg_name, cfg, prepend=False):
        """Add a configuration to self._precedence_list.

        @ivar cfg_name: The name of the config.
        @ivar cfg: The config dict.
        @ivar prepend: If true, add to the beginning of self._precedence_list.
                       Otherwise, add it to the end.
        """
        position = 0 if prepend else len(self._precedence_list)
        self._precedence_list.insert(position, cfg)
        self._precedence_names.insert(position, cfg_name)

    def __getattr__(self, attr):
        if attr in self.attributes:
            return self.attributes[attr]
        raise AttributeError('FAFT config has no attribute named %s' % attr)

    def __str__(self):
        str_list = []
        str_list.append('----------[ FW Testing Config Variables ]----------')
        str_list.append('--- Precedence list: %s ---' % self._precedence_names)
        for attr in sorted(self.attributes):
            str_list.append('  %s: %s' % (attr, self.attributes[attr]))
        str_list.append('---------------------------------------------------')
        return '\n'.join(str_list)
