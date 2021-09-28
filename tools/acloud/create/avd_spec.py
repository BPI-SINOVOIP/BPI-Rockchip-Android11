#!/usr/bin/env python
#
# Copyright 2018 - The Android Open Source Project
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
r"""AVDSpec class.

AVDSpec will take in args from the user and be the main data type that will
get passed into the create classes. The inferring magic will happen within
initialization of AVDSpec (like LKGB build id, image branch, etc).
"""

import glob
import logging
import os
import re
import subprocess
import tempfile
import threading

from acloud import errors
from acloud.create import create_common
from acloud.internal import constants
from acloud.internal.lib import android_build_client
from acloud.internal.lib import auth
from acloud.internal.lib import utils
from acloud.list import list as list_instance
from acloud.public import config


logger = logging.getLogger(__name__)

# Default values for build target.
_BRANCH_RE = re.compile(r"^Manifest branch: (?P<branch>.+)")
_COMMAND_REPO_INFO = "repo info platform/tools/acloud"
_REPO_TIMEOUT = 3
_CF_ZIP_PATTERN = "*img*.zip"
_DEFAULT_BUILD_BITNESS = "x86"
_DEFAULT_BUILD_TYPE = "userdebug"
_ENV_ANDROID_PRODUCT_OUT = "ANDROID_PRODUCT_OUT"
_ENV_ANDROID_BUILD_TOP = "ANDROID_BUILD_TOP"
_GCE_LOCAL_IMAGE_CANDIDATES = ["avd-system.tar.gz",
                               "android_system_disk_syslinux.img"]
_LOCAL_ZIP_WARNING_MSG = "'adb sync' will take a long time if using images " \
                         "built with `m dist`. Building with just `m` will " \
                         "enable a faster 'adb sync' process."
_RE_ANSI_ESCAPE = re.compile(r"(\x9B|\x1B\[)[0-?]*[ -\/]*[@-~]")
_RE_FLAVOR = re.compile(r"^.+_(?P<flavor>.+)-img.+")
_RE_MEMORY = re.compile(r"(?P<gb_size>\d+)g$|(?P<mb_size>\d+)m$",
                        re.IGNORECASE)
_RE_INT = re.compile(r"^\d+$")
_RE_RES = re.compile(r"^(?P<x_res>\d+)x(?P<y_res>\d+)$")
_X_RES = "x_res"
_Y_RES = "y_res"
_COMMAND_GIT_REMOTE = ["git", "remote"]

# The branch prefix is necessary for the Android Build system to know what we're
# talking about. For instance, on an aosp remote repo in the master branch,
# Android Build will recognize it as aosp-master.
_BRANCH_PREFIX = {"aosp": "aosp-"}
_DEFAULT_BRANCH_PREFIX = "git_"
_DEFAULT_BRANCH = "aosp-master"

# The target prefix is needed to help concoct the lunch target name given a
# the branch, avd type and device flavor:
# aosp, cf and phone -> aosp_cf_x86_phone.
_BRANCH_TARGET_PREFIX = {"aosp": "aosp_"}


def EscapeAnsi(line):
    """Remove ANSI control sequences (e.g. temrinal color codes...)

    Args:
        line: String, one line of command output.

    Returns:
        String without ANSI code.
    """
    return _RE_ANSI_ESCAPE.sub('', line)


# pylint: disable=too-many-public-methods
class AVDSpec:
    """Class to store data on the type of AVD to create."""

    def __init__(self, args):
        """Process the args into class vars.

        Args:
            args: Namespace object from argparse.parse_args.
        """
        # Let's define the private class vars here and then process the user
        # args afterwards.
        self._client_adb_port = args.adb_port
        self._autoconnect = None
        self._instance_name_to_reuse = None
        self._unlock_screen = None
        self._report_internal_ip = None
        self._avd_type = None
        self._flavor = None
        self._image_source = None
        self._instance_type = None
        self._local_image_dir = None
        self._local_image_artifact = None
        self._local_system_image_dir = None
        self._local_tool_dirs = None
        self._image_download_dir = None
        self._num_of_instances = None
        self._no_pull_log = None
        self._remote_image = None
        self._system_build_info = None
        self._kernel_build_info = None
        self._hw_property = None
        self._remote_host = None
        self._host_user = None
        self._host_ssh_private_key_path = None
        # Create config instance for android_build_client to query build api.
        self._cfg = config.GetAcloudConfig(args)
        # Reporting args.
        self._serial_log_file = None
        # gpu and emulator_build_id is only used for goldfish avd_type.
        self._gpu = None
        self._emulator_build_id = None

        # Fields only used for cheeps type.
        self._stable_cheeps_host_image_name = None
        self._stable_cheeps_host_image_project = None
        self._username = None
        self._password = None

        # The maximum time in seconds used to wait for the AVD to boot.
        self._boot_timeout_secs = None
        # The maximum time in seconds used to wait for the instance ready.
        self._ins_timeout_secs = None

        # The local instance id
        self._local_instance_id = None

        self._ProcessArgs(args)

    def __repr__(self):
        """Let's make it easy to see what this class is holding."""
        # TODO: I'm pretty sure there's a better way to do this, but I'm not
        # quite sure what that would be.
        representation = []
        representation.append("")
        representation.append(" - instance_type: %s" % self._instance_type)
        representation.append(" - avd type: %s" % self._avd_type)
        representation.append(" - flavor: %s" % self._flavor)
        representation.append(" - autoconnect: %s" % self._autoconnect)
        representation.append(" - num of instances requested: %s" %
                              self._num_of_instances)
        representation.append(" - image source type: %s" %
                              self._image_source)
        image_summary = None
        image_details = None
        if self._image_source == constants.IMAGE_SRC_LOCAL:
            image_summary = "local image dir"
            image_details = self._local_image_dir
            representation.append(" - instance id: %s" % self._local_instance_id)
        elif self._image_source == constants.IMAGE_SRC_REMOTE:
            image_summary = "remote image details"
            image_details = self._remote_image
        representation.append(" - %s: %s" % (image_summary, image_details))
        representation.append(" - hw properties: %s" %
                              self._hw_property)
        return "\n".join(representation)

    def _ProcessArgs(self, args):
        """Main entry point to process args for the different type of args.

        Split up the arg processing into related areas (image, instance type,
        etc) so that we don't have one huge monolilthic method that does
        everything. It makes it easier to review, write tests, and maintain.

        Args:
            args: Namespace object from argparse.parse_args.
        """
        self._ProcessMiscArgs(args)
        self._ProcessImageArgs(args)
        self._ProcessHWPropertyArgs(args)

    def _ProcessImageArgs(self, args):
        """ Process Image Args.

        Args:
            args: Namespace object from argparse.parse_args.
        """
        # If user didn't specify --local-image, infer remote image args
        if args.local_image == "":
            self._image_source = constants.IMAGE_SRC_REMOTE
            if (self._avd_type == constants.TYPE_GF and
                    self._instance_type != constants.INSTANCE_TYPE_REMOTE):
                raise errors.UnsupportedInstanceImageType(
                    "unsupported creation of avd type: %s, "
                    "instance type: %s, image source: %s" %
                    (self._avd_type, self._instance_type, self._image_source))
            self._ProcessRemoteBuildArgs(args)
        else:
            self._image_source = constants.IMAGE_SRC_LOCAL
            self._ProcessLocalImageArgs(args)

        self.image_download_dir = (
            args.image_download_dir if args.image_download_dir
            else tempfile.gettempdir())

    @staticmethod
    def _ParseHWPropertyStr(hw_property_str):
        """Parse string to dict.

        Args:
            hw_property_str: A hw properties string.

        Returns:
            Dict converted from a string.

        Raises:
            error.MalformedHWPropertyError: If hw_property_str is malformed.
        """
        hw_dict = create_common.ParseHWPropertyArgs(hw_property_str)
        arg_hw_properties = {}
        for key, value in hw_dict.items():
            # Parsing HW properties int to avdspec.
            if key == constants.HW_ALIAS_RESOLUTION:
                match = _RE_RES.match(value)
                if match:
                    arg_hw_properties[_X_RES] = match.group("x_res")
                    arg_hw_properties[_Y_RES] = match.group("y_res")
                else:
                    raise errors.InvalidHWPropertyError(
                        "[%s] is an invalid resolution. Example:1280x800" % value)
            elif key in [constants.HW_ALIAS_MEMORY, constants.HW_ALIAS_DISK]:
                match = _RE_MEMORY.match(value)
                if match and match.group("gb_size"):
                    arg_hw_properties[key] = str(
                        int(match.group("gb_size")) * 1024)
                elif match and match.group("mb_size"):
                    arg_hw_properties[key] = match.group("mb_size")
                else:
                    raise errors.InvalidHWPropertyError(
                        "Expected gb size.[%s] is not allowed. Example:4g" % value)
            elif key in [constants.HW_ALIAS_CPUS, constants.HW_ALIAS_DPI]:
                if not _RE_INT.match(value):
                    raise errors.InvalidHWPropertyError(
                        "%s value [%s] is not an integer." % (key, value))
                arg_hw_properties[key] = value

        return arg_hw_properties

    def _ProcessHWPropertyArgs(self, args):
        """Get the HW properties from argparse.parse_args.

        This method will initialize _hw_property in the following
        manner:
        1. Get default hw properties from config.
        2. Override by hw_property args.

        Args:
            args: Namespace object from argparse.parse_args.
        """
        self._cfg.OverrideHwPropertyWithFlavor(self._flavor)
        self._hw_property = {}
        self._hw_property = self._ParseHWPropertyStr(self._cfg.hw_property)
        logger.debug("Default hw property for [%s] flavor: %s", self._flavor,
                     self._hw_property)

        if args.hw_property:
            arg_hw_property = self._ParseHWPropertyStr(args.hw_property)
            logger.debug("Use custom hw property: %s", arg_hw_property)
            self._hw_property.update(arg_hw_property)

    def _ProcessMiscArgs(self, args):
        """These args we can take as and don't belong to a group of args.

        Args:
            args: Namespace object from argparse.parse_args.
        """
        self._autoconnect = args.autoconnect
        self._unlock_screen = args.unlock_screen
        self._report_internal_ip = args.report_internal_ip
        self._avd_type = args.avd_type
        self._flavor = args.flavor or constants.FLAVOR_PHONE
        if args.remote_host:
            self._instance_type = constants.INSTANCE_TYPE_HOST
        else:
            self._instance_type = (constants.INSTANCE_TYPE_LOCAL
                                   if args.local_instance else
                                   constants.INSTANCE_TYPE_REMOTE)
        self._remote_host = args.remote_host
        self._host_user = args.host_user
        self._host_ssh_private_key_path = args.host_ssh_private_key_path
        self._local_instance_id = args.local_instance
        self._local_tool_dirs = args.local_tool
        self._num_of_instances = args.num
        self._no_pull_log = args.no_pull_log
        self._serial_log_file = args.serial_log_file
        self._emulator_build_id = args.emulator_build_id
        self._gpu = args.gpu

        self._stable_cheeps_host_image_name = args.stable_cheeps_host_image_name
        self._stable_cheeps_host_image_project = args.stable_cheeps_host_image_project
        self._username = args.username
        self._password = args.password

        self._boot_timeout_secs = args.boot_timeout_secs
        self._ins_timeout_secs = args.ins_timeout_secs

        if args.reuse_gce:
            if args.reuse_gce != constants.SELECT_ONE_GCE_INSTANCE:
                if list_instance.GetInstancesFromInstanceNames(
                        self._cfg, [args.reuse_gce]):
                    self._instance_name_to_reuse = args.reuse_gce
            if self._instance_name_to_reuse is None:
                instance = list_instance.ChooseOneRemoteInstance(self._cfg)
                self._instance_name_to_reuse = instance.name

    @staticmethod
    def _GetFlavorFromString(flavor_string):
        """Get flavor name from flavor string.

        Flavor string can come from the zipped image name or the lunch target.
        e.g.
        If flavor_string come from zipped name:aosp_cf_x86_phone-img-5455843.zip
        , then "phone" is the flavor.
        If flavor_string come from a lunch'd target:aosp_cf_x86_auto-userdebug,
        then "auto" is the flavor.

        Args:
            flavor_string: String which contains flavor.It can be a
                           build target or filename.

        Returns:
            String of flavor name. None if flavor can't be determined.
        """
        for flavor in constants.ALL_FLAVORS:
            if re.match(r"(.*_)?%s" % flavor, flavor_string):
                return flavor

        logger.debug("Unable to determine flavor from build target: %s",
                     flavor_string)
        return None

    def _ProcessLocalImageArgs(self, args):
        """Get local image path.

        Args:
            args: Namespace object from argparse.parse_args.
        """
        if self._avd_type == constants.TYPE_CF:
            self._ProcessCFLocalImageArgs(args.local_image, args.flavor)
        elif self._avd_type == constants.TYPE_GF:
            self._local_image_dir = self._ProcessGFLocalImageArgs(
                args.local_image)
            if args.local_system_image != "":
                self._local_system_image_dir = self._ProcessGFLocalImageArgs(
                    args.local_system_image)
        elif self._avd_type == constants.TYPE_GCE:
            self._local_image_artifact = self._GetGceLocalImagePath(
                args.local_image)
        else:
            raise errors.CreateError(
                "Local image doesn't support the AVD type: %s" % self._avd_type
            )

    @staticmethod
    def _GetGceLocalImagePath(local_image_dir):
        """Get gce local image path.

        Choose image file in local_image_dir over $ANDROID_PRODUCT_OUT.
        There are various img files so we prioritize returning the one we find
        first based in the specified order in _GCE_LOCAL_IMAGE_CANDIDATES.

        Args:
            local_image_dir: A string to specify local image dir.

        Returns:
            String, image file path if exists.

        Raises:
            errors.ImgDoesNotExist if image doesn't exist.
        """
        # IF the user specified a file, return it
        if local_image_dir and os.path.isfile(local_image_dir):
            return local_image_dir

        # If the user didn't specify a dir, assume $ANDROID_PRODUCT_OUT
        if not local_image_dir:
            local_image_dir = utils.GetBuildEnvironmentVariable(
                _ENV_ANDROID_PRODUCT_OUT)

        for img_name in _GCE_LOCAL_IMAGE_CANDIDATES:
            full_file_path = os.path.join(local_image_dir, img_name)
            if os.path.exists(full_file_path):
                return full_file_path

        raise errors.ImgDoesNotExist("Could not find any GCE images (%s), you "
                                     "can build them via \"m dist\"" %
                                     ", ".join(_GCE_LOCAL_IMAGE_CANDIDATES))

    @staticmethod
    def _ProcessGFLocalImageArgs(local_image_arg):
        """Get local built image path for goldfish.

        Args:
            local_image_arg: The path to the unzipped update package or SDK
                             repository, i.e., <target>-img-<build>.zip or
                             sdk-repo-<os>-system-images-<build>.zip.
                             If the value is empty, this method returns
                             ANDROID_PRODUCT_OUT in build environment.

        Returns:
            String, the path to the image directory.

        Raises:
            errors.GetLocalImageError if the directory is not found.
        """
        image_dir = (local_image_arg if local_image_arg else
                     utils.GetBuildEnvironmentVariable(
                         constants.ENV_ANDROID_PRODUCT_OUT))

        if not os.path.isdir(image_dir):
            raise errors.GetLocalImageError(
                "%s is not a directory." % image_dir)

        return image_dir

    def _ProcessCFLocalImageArgs(self, local_image_arg, flavor_arg):
        """Get local built image path for cuttlefish-type AVD.

        Two scenarios of using --local-image:
        - Without a following argument
          Set flavor string if the required images are in $ANDROID_PRODUCT_OUT,
        - With a following filename/dirname
          Set flavor string from the specified image/dir name.

        Args:
            local_image_arg: String of local image args.
            flavor_arg: String of flavor arg

        """
        flavor_from_build_string = None
        local_image_path = local_image_arg or utils.GetBuildEnvironmentVariable(
            _ENV_ANDROID_PRODUCT_OUT)

        if os.path.isfile(local_image_path):
            self._local_image_artifact = local_image_arg
            flavor_from_build_string = self._GetFlavorFromString(
                self._local_image_artifact)
            # Since file is provided and I assume it's a zip, so print the
            # warning message.
            utils.PrintColorString(_LOCAL_ZIP_WARNING_MSG,
                                   utils.TextColors.WARNING)
        else:
            self._local_image_dir = local_image_path
            # Since dir is provided, so checking that any images exist to ensure
            # user didn't forget to 'make' before launch AVD.
            image_list = glob.glob(os.path.join(self.local_image_dir, "*.img"))
            if not image_list:
                raise errors.GetLocalImageError(
                    "No image found(Did you choose a lunch target and run `m`?)"
                    ": %s.\n " % self.local_image_dir)

            try:
                flavor_from_build_string = self._GetFlavorFromString(
                    utils.GetBuildEnvironmentVariable(constants.ENV_BUILD_TARGET))
            except errors.GetAndroidBuildEnvVarError:
                logger.debug("Unable to determine flavor from env variable: %s",
                             constants.ENV_BUILD_TARGET)

        if flavor_from_build_string and not flavor_arg:
            self._flavor = flavor_from_build_string

    def _ProcessRemoteBuildArgs(self, args):
        """Get the remote build args.

        Some of the acloud magic happens here, we will infer some of these
        values if the user hasn't specified them.

        Args:
            args: Namespace object from argparse.parse_args.
        """
        self._remote_image = {}
        self._remote_image[constants.BUILD_BRANCH] = args.branch
        if not self._remote_image[constants.BUILD_BRANCH]:
            self._remote_image[constants.BUILD_BRANCH] = self._GetBuildBranch(
                args.build_id, args.build_target)

        self._remote_image[constants.BUILD_TARGET] = args.build_target
        if not self._remote_image[constants.BUILD_TARGET]:
            self._remote_image[constants.BUILD_TARGET] = self._GetBuildTarget(args)
        else:
            # If flavor isn't specified, try to infer it from build target,
            # if we can't, just default to phone flavor.
            self._flavor = args.flavor or self._GetFlavorFromString(
                self._remote_image[constants.BUILD_TARGET]) or constants.FLAVOR_PHONE
            # infer avd_type from build_target.
            for avd_type, avd_type_abbr in constants.AVD_TYPES_MAPPING.items():
                if re.match(r"(.*_)?%s_" % avd_type_abbr,
                            self._remote_image[constants.BUILD_TARGET]):
                    self._avd_type = avd_type
                    break

        self._remote_image[constants.BUILD_ID] = args.build_id
        if not self._remote_image[constants.BUILD_ID]:
            build_client = android_build_client.AndroidBuildClient(
                auth.CreateCredentials(self._cfg))

            self._remote_image[constants.BUILD_ID] = build_client.GetLKGB(
                self._remote_image[constants.BUILD_TARGET],
                self._remote_image[constants.BUILD_BRANCH])

        # Process system image and kernel image.
        self._system_build_info = {constants.BUILD_ID: args.system_build_id,
                                   constants.BUILD_BRANCH: args.system_branch,
                                   constants.BUILD_TARGET: args.system_build_target}
        self._kernel_build_info = {constants.BUILD_ID: args.kernel_build_id,
                                   constants.BUILD_BRANCH: args.kernel_branch,
                                   constants.BUILD_TARGET: args.kernel_build_target}

    @staticmethod
    def _GetGitRemote():
        """Get the remote repo.

        We'll go to a project we know exists (tools/acloud) and grab the git
        remote output from there.

        Returns:
            remote: String, git remote (e.g. "aosp").
        """
        try:
            android_build_top = os.environ[constants.ENV_ANDROID_BUILD_TOP]
        except KeyError:
            raise errors.GetAndroidBuildEnvVarError(
                "Could not get environment var: %s\n"
                "Try to run '#source build/envsetup.sh && lunch <target>'"
                % _ENV_ANDROID_BUILD_TOP
            )

        acloud_project = os.path.join(android_build_top, "tools", "acloud")
        return EscapeAnsi(subprocess.check_output(_COMMAND_GIT_REMOTE,
                                                  cwd=acloud_project).strip())

    def _GetBuildBranch(self, build_id, build_target):
        """Infer build branch if user didn't specify branch name.

        Args:
            build_id: String, Build id, e.g. "2263051", "P2804227"
            build_target: String, the build target, e.g. cf_x86_phone-userdebug

        Returns:
            String, name of build branch.
        """
        # Infer branch from build_target and build_id
        if build_id and build_target:
            build_client = android_build_client.AndroidBuildClient(
                auth.CreateCredentials(self._cfg))
            return build_client.GetBranch(build_target, build_id)

        return self._GetBranchFromRepo()

    def _GetBranchFromRepo(self):
        """Get branch information from command "repo info".

        If branch can't get from "repo info", it will be set as default branch
        "aosp-master".

        Returns:
            branch: String, git branch name. e.g. "aosp-master"
        """
        branch = None
        # TODO(149460014): Migrate acloud to py3, then remove this
        # workaround.
        env = os.environ.copy()
        env.pop("PYTHONPATH", None)
        logger.info("Running command \"%s\"", _COMMAND_REPO_INFO)
        process = subprocess.Popen(_COMMAND_REPO_INFO, shell=True, stdin=None,
                                   stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, env=env)
        timer = threading.Timer(_REPO_TIMEOUT, process.kill)
        timer.start()
        stdout, _ = process.communicate()
        if stdout:
            for line in stdout.splitlines():
                match = _BRANCH_RE.match(EscapeAnsi(line))
                if match:
                    branch_prefix = _BRANCH_PREFIX.get(self._GetGitRemote(),
                                                       _DEFAULT_BRANCH_PREFIX)
                    branch = branch_prefix + match.group("branch")
        timer.cancel()
        if branch:
            return branch
        utils.PrintColorString(
            "Unable to determine your repo branch, defaulting to %s"
            % _DEFAULT_BRANCH, utils.TextColors.WARNING)
        return _DEFAULT_BRANCH

    def _GetBuildTarget(self, args):
        """Infer build target if user doesn't specified target name.

        Target = {REPO_PREFIX}{avd_type}_{bitness}_{flavor}-
            {DEFAULT_BUILD_TARGET_TYPE}.
        Example target: aosp_cf_x86_phone-userdebug

        Args:
            args: Namespace object from argparse.parse_args.

        Returns:
            build_target: String, name of build target.
        """
        branch = re.split("-|_", self._remote_image[constants.BUILD_BRANCH])[0]
        return "%s%s_%s_%s-%s" % (
            _BRANCH_TARGET_PREFIX.get(branch, ""),
            constants.AVD_TYPES_MAPPING[args.avd_type],
            _DEFAULT_BUILD_BITNESS, self._flavor,
            _DEFAULT_BUILD_TYPE)

    @property
    def instance_type(self):
        """Return the instance type."""
        return self._instance_type

    @property
    def image_source(self):
        """Return the image type."""
        return self._image_source

    @property
    def hw_property(self):
        """Return the hw_property."""
        return self._hw_property

    @property
    def local_image_dir(self):
        """Return local image dir."""
        return self._local_image_dir

    @property
    def local_image_artifact(self):
        """Return local image artifact."""
        return self._local_image_artifact

    @property
    def local_system_image_dir(self):
        """Return local system image dir."""
        return self._local_system_image_dir

    @property
    def local_tool_dirs(self):
        """Return a list of local tool directories."""
        return self._local_tool_dirs

    @property
    def avd_type(self):
        """Return the avd type."""
        return self._avd_type

    @property
    def autoconnect(self):
        """autoconnect.

        args.autoconnect could pass as Boolean or String.

        Return: Boolean, True only if self._autoconnect is not False.
        """
        return self._autoconnect is not False

    @property
    def connect_adb(self):
        """Auto-connect to adb.

        Return: Boolean, whether autoconnect is enabled.
        """
        return self._autoconnect is not False

    @property
    def connect_vnc(self):
        """Launch vnc.

        Return: Boolean, True if self._autoconnect is 'vnc'.
        """
        return self._autoconnect == constants.INS_KEY_VNC

    @property
    def connect_webrtc(self):
        """Auto-launch webRTC AVD on the browser.

        Return: Boolean, True if args.autoconnect is "webrtc".
        """
        return self._autoconnect == constants.INS_KEY_WEBRTC

    @property
    def unlock_screen(self):
        """Return unlock_screen."""
        return self._unlock_screen

    @property
    def remote_image(self):
        """Return the remote image."""
        return self._remote_image

    @property
    def num(self):
        """Return num of instances."""
        return self._num_of_instances

    @property
    def report_internal_ip(self):
        """Return report internal ip."""
        return self._report_internal_ip

    @property
    def kernel_build_info(self):
        """Return kernel build info."""
        return self._kernel_build_info

    @property
    def flavor(self):
        """Return flavor."""
        return self._flavor

    @property
    def cfg(self):
        """Return cfg instance."""
        return self._cfg

    @property
    def image_download_dir(self):
        """Return image download dir."""
        return self._image_download_dir

    @image_download_dir.setter
    def image_download_dir(self, value):
        """Set image download dir."""
        self._image_download_dir = value

    @property
    def serial_log_file(self):
        """Return serial log file path."""
        return self._serial_log_file

    @property
    def gpu(self):
        """Return gpu."""
        return self._gpu

    @property
    def emulator_build_id(self):
        """Return emulator_build_id."""
        return self._emulator_build_id

    @property
    def client_adb_port(self):
        """Return the client adb port."""
        return self._client_adb_port

    @property
    def stable_cheeps_host_image_name(self):
        """Return the Cheeps host image name."""
        return self._stable_cheeps_host_image_name

    # pylint: disable=invalid-name
    @property
    def stable_cheeps_host_image_project(self):
        """Return the project hosting the Cheeps host image."""
        return self._stable_cheeps_host_image_project

    @property
    def username(self):
        """Return username."""
        return self._username

    @property
    def password(self):
        """Return password."""
        return self._password

    @property
    def boot_timeout_secs(self):
        """Return boot_timeout_secs."""
        return self._boot_timeout_secs

    @property
    def ins_timeout_secs(self):
        """Return ins_timeout_secs."""
        return self._ins_timeout_secs

    @property
    def system_build_info(self):
        """Return system_build_info."""
        return self._system_build_info

    @property
    def local_instance_id(self):
        """Return local_instance_id."""
        return self._local_instance_id

    @property
    def instance_name_to_reuse(self):
        """Return instance_name_to_reuse."""
        return self._instance_name_to_reuse

    @property
    def remote_host(self):
        """Return host."""
        return self._remote_host

    @property
    def host_user(self):
        """Return host_user."""
        return self._host_user

    @property
    def host_ssh_private_key_path(self):
        """Return host_ssh_private_key_path."""
        return self._host_ssh_private_key_path

    @property
    def no_pull_log(self):
        """Return no_pull_log."""
        return self._no_pull_log
