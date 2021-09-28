#!/usr/bin/env python
#
# Copyright 2016 - The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Config manager.

Three protobuf messages are defined in
   driver/internal/config/proto/internal_config.proto
   driver/internal/config/proto/user_config.proto

Internal config file     User config file
      |                         |
      v                         v
  InternalConfig           UserConfig
  (proto message)        (proto message)
        |                     |
        |                     |
        |->   AcloudConfig  <-|

At runtime, AcloudConfigManager performs the following steps.
- Load driver config file into a InternalConfig message instance.
- Load user config file into a UserConfig message instance.
- Create AcloudConfig using InternalConfig and UserConfig.

TODO:
  1. Add support for override configs with command line args.
  2. Scan all configs to find the right config for given branch and build_id.
     Raise an error if the given build_id is smaller than min_build_id
     only applies to release build id.
     Raise an error if the branch is not supported.

"""

import logging
import os

import six

from google.protobuf import text_format

# pylint: disable=no-name-in-module,import-error
from acloud import errors
from acloud.internal import constants
from acloud.internal.proto import internal_config_pb2
from acloud.internal.proto import user_config_pb2
from acloud.create import create_args


logger = logging.getLogger(__name__)

_CONFIG_DATA_PATH = os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "data")
_DEFAULT_CONFIG_FILE = "acloud.config"


def GetDefaultConfigFile():
    """Return path to default config file."""
    config_path = os.path.join(os.path.expanduser("~"), ".config", "acloud")
    # Create the default config dir if it doesn't exist.
    if not os.path.exists(config_path):
        os.makedirs(config_path)
    return os.path.join(config_path, _DEFAULT_CONFIG_FILE)


def GetAcloudConfig(args):
    """Helper function to initialize Config object.

    Args:
        args: Namespace object from argparse.parse_args.

    Return:
        An instance of AcloudConfig.
    """
    config_mgr = AcloudConfigManager(args.config_file)
    cfg = config_mgr.Load()
    cfg.OverrideWithArgs(args)
    return cfg


class AcloudConfig(object):
    """A class that holds all configurations for acloud."""

    REQUIRED_FIELD = [
        "machine_type", "network", "min_machine_size",
        "disk_image_name", "disk_image_mime_type"
    ]

    # pylint: disable=too-many-statements
    def __init__(self, usr_cfg, internal_cfg):
        """Initialize.

        Args:
            usr_cfg: A protobuf object that holds the user configurations.
            internal_cfg: A protobuf object that holds internal configurations.
        """
        self.service_account_name = usr_cfg.service_account_name
        # pylint: disable=invalid-name
        self.service_account_private_key_path = (
            usr_cfg.service_account_private_key_path)
        self.service_account_json_private_key_path = (
            usr_cfg.service_account_json_private_key_path)
        self.creds_cache_file = internal_cfg.creds_cache_file
        self.user_agent = internal_cfg.user_agent
        self.client_id = usr_cfg.client_id
        self.client_secret = usr_cfg.client_secret

        self.project = usr_cfg.project
        self.zone = usr_cfg.zone
        self.machine_type = (usr_cfg.machine_type or
                             internal_cfg.default_usr_cfg.machine_type)
        self.network = usr_cfg.network or internal_cfg.default_usr_cfg.network
        self.ssh_private_key_path = usr_cfg.ssh_private_key_path
        self.ssh_public_key_path = usr_cfg.ssh_public_key_path
        self.storage_bucket_name = usr_cfg.storage_bucket_name
        self.metadata_variable = {
            key: val for key, val in
            six.iteritems(internal_cfg.default_usr_cfg.metadata_variable)
        }
        self.metadata_variable.update(usr_cfg.metadata_variable)

        self.device_resolution_map = {
            device: resolution for device, resolution in
            six.iteritems(internal_cfg.device_resolution_map)
        }
        self.device_default_orientation_map = {
            device: orientation for device, orientation in
            six.iteritems(internal_cfg.device_default_orientation_map)
        }
        self.no_project_access_msg_map = {
            project: msg for project, msg in
            six.iteritems(internal_cfg.no_project_access_msg_map)
        }
        self.min_machine_size = internal_cfg.min_machine_size
        self.disk_image_name = internal_cfg.disk_image_name
        self.disk_image_mime_type = internal_cfg.disk_image_mime_type
        self.disk_image_extension = internal_cfg.disk_image_extension
        self.disk_raw_image_name = internal_cfg.disk_raw_image_name
        self.disk_raw_image_extension = internal_cfg.disk_raw_image_extension
        self.valid_branch_and_min_build_id = {
            branch: min_build_id for branch, min_build_id in
            six.iteritems(internal_cfg.valid_branch_and_min_build_id)
        }
        self.precreated_data_image_map = {
            size_gb: image_name for size_gb, image_name in
            six.iteritems(internal_cfg.precreated_data_image)
        }
        self.extra_data_disk_size_gb = (
            usr_cfg.extra_data_disk_size_gb or
            internal_cfg.default_usr_cfg.extra_data_disk_size_gb)
        if self.extra_data_disk_size_gb > 0:
            if "cfg_sta_persistent_data_device" not in usr_cfg.metadata_variable:
                # If user did not set it explicity, use default.
                self.metadata_variable["cfg_sta_persistent_data_device"] = (
                    internal_cfg.default_extra_data_disk_device)
            if "cfg_sta_ephemeral_data_size_mb" in usr_cfg.metadata_variable:
                raise errors.ConfigError(
                    "The following settings can't be set at the same time: "
                    "extra_data_disk_size_gb and"
                    "metadata variable cfg_sta_ephemeral_data_size_mb.")
            if "cfg_sta_ephemeral_data_size_mb" in self.metadata_variable:
                del self.metadata_variable["cfg_sta_ephemeral_data_size_mb"]

        # Additional scopes to be passed to the created instance
        self.extra_scopes = usr_cfg.extra_scopes

        # Fields that can be overriden by args
        self.orientation = usr_cfg.orientation
        self.resolution = usr_cfg.resolution

        self.stable_host_image_name = (
            usr_cfg.stable_host_image_name or
            internal_cfg.default_usr_cfg.stable_host_image_name)
        self.stable_host_image_project = (
            usr_cfg.stable_host_image_project or
            internal_cfg.default_usr_cfg.stable_host_image_project)
        self.kernel_build_target = internal_cfg.kernel_build_target

        self.emulator_build_target = internal_cfg.emulator_build_target
        self.stable_goldfish_host_image_name = (
            usr_cfg.stable_goldfish_host_image_name or
            internal_cfg.default_usr_cfg.stable_goldfish_host_image_name)
        self.stable_goldfish_host_image_project = (
            usr_cfg.stable_goldfish_host_image_project or
            internal_cfg.default_usr_cfg.stable_goldfish_host_image_project)

        self.stable_cheeps_host_image_name = (
            usr_cfg.stable_cheeps_host_image_name or
            internal_cfg.default_usr_cfg.stable_cheeps_host_image_name)
        self.stable_cheeps_host_image_project = (
            usr_cfg.stable_cheeps_host_image_project or
            internal_cfg.default_usr_cfg.stable_cheeps_host_image_project)

        self.extra_args_ssh_tunnel = usr_cfg.extra_args_ssh_tunnel

        self.common_hw_property_map = internal_cfg.common_hw_property_map
        self.hw_property = usr_cfg.hw_property

        self.launch_args = usr_cfg.launch_args
        self.instance_name_pattern = (
            usr_cfg.instance_name_pattern or
            internal_cfg.default_usr_cfg.instance_name_pattern)
        self.fetch_cvd_version = (
            usr_cfg.fetch_cvd_version or
            internal_cfg.default_usr_cfg.fetch_cvd_version)
        if usr_cfg.HasField("enable_multi_stage") is not None:
            self.enable_multi_stage = usr_cfg.enable_multi_stage
        elif internal_cfg.default_usr_cfg.HasField("enable_multi_stage"):
            self.enable_multi_stage = internal_cfg.default_usr_cfg.enable_multi_stage
        else:
            self.enable_multi_stage = False

        # Verify validity of configurations.
        self.Verify()

    def OverrideWithArgs(self, parsed_args):
        """Override configuration values with args passed in from cmd line.

        Args:
            parsed_args: Args parsed from command line.
        """
        if parsed_args.which == create_args.CMD_CREATE and parsed_args.spec:
            if not self.resolution:
                self.resolution = self.device_resolution_map.get(
                    parsed_args.spec, "")
            if not self.orientation:
                self.orientation = self.device_default_orientation_map.get(
                    parsed_args.spec, "")
        if parsed_args.email:
            self.service_account_name = parsed_args.email
        if parsed_args.service_account_json_private_key_path:
            self.service_account_json_private_key_path = (
                parsed_args.service_account_json_private_key_path)
        if parsed_args.which == "create_gf" and parsed_args.base_image:
            self.stable_goldfish_host_image_name = parsed_args.base_image
        if parsed_args.which == create_args.CMD_CREATE and not self.hw_property:
            flavor = parsed_args.flavor or constants.FLAVOR_PHONE
            self.hw_property = self.common_hw_property_map.get(flavor, "")
        if parsed_args.which in [create_args.CMD_CREATE, "create_cf"]:
            if parsed_args.network:
                self.network = parsed_args.network
            if parsed_args.multi_stage_launch is not None:
                self.enable_multi_stage = parsed_args.multi_stage_launch

    def OverrideHwPropertyWithFlavor(self, flavor):
        """Override hw configuration values with flavor name.

        HwProperty will be overrided according to the change of flavor.
        If flavor is None, set hw configuration with phone(default flavor).

        Args:
            flavor: string of flavor name.
        """
        self.hw_property = self.common_hw_property_map.get(
            flavor, constants.FLAVOR_PHONE)

    def Verify(self):
        """Verify configuration fields."""
        missing = [f for f in self.REQUIRED_FIELD if not getattr(self, f)]
        if missing:
            raise errors.ConfigError(
                "Missing required configuration fields: %s" % missing)
        if (self.extra_data_disk_size_gb and self.extra_data_disk_size_gb not in
                self.precreated_data_image_map):
            raise errors.ConfigError(
                "Supported extra_data_disk_size_gb options(gb): %s, "
                "invalid value: %d" % (self.precreated_data_image_map.keys(),
                                       self.extra_data_disk_size_gb))

    def SupportRemoteInstance(self):
        """Return True if gcp project is provided in config."""
        return True if self.project else False


class AcloudConfigManager(object):
    """A class that loads configurations."""

    _DEFAULT_INTERNAL_CONFIG_PATH = os.path.join(_CONFIG_DATA_PATH,
                                                 "default.config")

    def __init__(self,
                 user_config_path,
                 internal_config_path=_DEFAULT_INTERNAL_CONFIG_PATH):
        """Initialize with user specified paths to configs.

        Args:
            user_config_path: path to the user config.
            internal_config_path: path to the internal conifg.
        """
        self.user_config_path = user_config_path
        self._internal_config_path = internal_config_path

    def Load(self):
        """Load the configurations.

        Load user config with some special design.
        1. User specified user config:
            a.User config exist: Load config.
            b.User config didn't exist: Raise exception.
        2. User didn't specify user config, use default config:
            a.Default config exist: Load config.
            b.Default config didn't exist: provide empty usr_cfg.
        """
        internal_cfg = None
        usr_cfg = None
        try:
            with open(self._internal_config_path) as config_file:
                internal_cfg = self.LoadConfigFromProtocolBuffer(
                    config_file, internal_config_pb2.InternalConfig)
        except OSError as e:
            raise errors.ConfigError("Could not load config files: %s" % str(e))
        # Load user config file
        if self.user_config_path:
            if os.path.exists(self.user_config_path):
                with open(self.user_config_path, "r") as config_file:
                    usr_cfg = self.LoadConfigFromProtocolBuffer(
                        config_file, user_config_pb2.UserConfig)
            else:
                raise errors.ConfigError("The file doesn't exist: %s" %
                                         (self.user_config_path))
        else:
            self.user_config_path = GetDefaultConfigFile()
            if os.path.exists(self.user_config_path):
                with open(self.user_config_path, "r") as config_file:
                    usr_cfg = self.LoadConfigFromProtocolBuffer(
                        config_file, user_config_pb2.UserConfig)
            else:
                usr_cfg = user_config_pb2.UserConfig()
        return AcloudConfig(usr_cfg, internal_cfg)

    @staticmethod
    def LoadConfigFromProtocolBuffer(config_file, message_type):
        """Load config from a text-based protocol buffer file.

        Args:
            config_file: A python File object.
            message_type: A proto message class.

        Returns:
            An instance of type "message_type" populated with data
            from the file.
        """
        try:
            config = message_type()
            text_format.Merge(config_file.read(), config)
            return config
        except text_format.ParseError as e:
            raise errors.ConfigError("Could not parse config: %s" % str(e))
