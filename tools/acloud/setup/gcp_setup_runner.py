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
"""Gcloud setup runner."""

from __future__ import print_function
import logging
import os
import re
import subprocess

import six

from acloud import errors
from acloud.internal.lib import utils
from acloud.public import config
from acloud.setup import base_task_runner
from acloud.setup import google_sdk


logger = logging.getLogger(__name__)

# APIs that need to be enabled for GCP project.
_ANDROID_BUILD_SERVICE = "androidbuildinternal.googleapis.com"
_ANDROID_BUILD_MSG = (
    "This service (%s) help to download images from Android Build. If it isn't "
    "enabled, acloud only supports local images to create AVD."
    % _ANDROID_BUILD_SERVICE)
_COMPUTE_ENGINE_SERVICE = "compute.googleapis.com"
_COMPUTE_ENGINE_MSG = (
    "This service (%s) help to create instance in google cloud platform. If it "
    "isn't enabled, acloud can't work anymore." % _COMPUTE_ENGINE_SERVICE)
_OPEN_SERVICE_FAILED_MSG = (
    "\n[Open Service Failed]\n"
    "Service name: %(service_name)s\n"
    "%(service_msg)s\n")

_BUILD_SERVICE_ACCOUNT = "android-build-prod@system.gserviceaccount.com"
_BILLING_ENABLE_MSG = "billingEnabled: true"
_DEFAULT_SSH_FOLDER = os.path.expanduser("~/.ssh")
_DEFAULT_SSH_KEY = "acloud_rsa"
_DEFAULT_SSH_PRIVATE_KEY = os.path.join(_DEFAULT_SSH_FOLDER,
                                        _DEFAULT_SSH_KEY)
_DEFAULT_SSH_PUBLIC_KEY = os.path.join(_DEFAULT_SSH_FOLDER,
                                       _DEFAULT_SSH_KEY + ".pub")
_ENV_CLOUDSDK_PYTHON = "CLOUDSDK_PYTHON"
_GCLOUD_COMPONENT_ALPHA = "alpha"
# Regular expression to get project/zone information.
_PROJECT_RE = re.compile(r"^project = (?P<project>.+)")
_ZONE_RE = re.compile(r"^zone = (?P<zone>.+)")


def UpdateConfigFile(config_path, item, value):
    """Update config data.

    Case A: config file contain this item.
        In config, "project = A_project". New value is B_project
        Set config "project = B_project".
    Case B: config file didn't contain this item.
        New value is B_project.
        Setup config as "project = B_project".

    Args:
        config_path: String, acloud config path.
        item: String, item name in config file. EX: project, zone
        value: String, value of item in config file.

    TODO(111574698): Refactor this to minimize writes to the config file.
    TODO(111574698): Use proto method to update config.
    """
    write_lines = []
    find_item = False
    write_line = item + ": \"" + value + "\"\n"
    if os.path.isfile(config_path):
        with open(config_path, "r") as cfg_file:
            for read_line in cfg_file.readlines():
                if read_line.startswith(item + ":"):
                    find_item = True
                    write_lines.append(write_line)
                else:
                    write_lines.append(read_line)
    if not find_item:
        write_lines.append(write_line)
    with open(config_path, "w") as cfg_file:
        cfg_file.writelines(write_lines)


def SetupSSHKeys(config_path, private_key_path, public_key_path):
    """Setup the pair of the ssh key for acloud.config.

    User can use the default path: "~/.ssh/acloud_rsa".

    Args:
        config_path: String, acloud config path.
        private_key_path: Path to the private key file.
                          e.g. ~/.ssh/acloud_rsa
        public_key_path: Path to the public key file.
                         e.g. ~/.ssh/acloud_rsa.pub
    """
    private_key_path = os.path.expanduser(private_key_path)
    if (private_key_path == "" or public_key_path == ""
            or private_key_path == _DEFAULT_SSH_PRIVATE_KEY):
        utils.CreateSshKeyPairIfNotExist(_DEFAULT_SSH_PRIVATE_KEY,
                                         _DEFAULT_SSH_PUBLIC_KEY)
        UpdateConfigFile(config_path, "ssh_private_key_path",
                         _DEFAULT_SSH_PRIVATE_KEY)
        UpdateConfigFile(config_path, "ssh_public_key_path",
                         _DEFAULT_SSH_PUBLIC_KEY)


def _InputIsEmpty(input_string):
    """Check input string is empty.

    Tool requests user to input client ID & client secret.
    This basic check can detect user input is empty.

    Args:
        input_string: String, user input string.

    Returns:
        Boolean: True if input is empty, False otherwise.
    """
    if input_string is None:
        return True
    if input_string == "":
        print("Please enter a non-empty value.")
        return True
    return False


class GoogleSDKBins(object):
    """Class to run tools in the Google SDK."""

    def __init__(self, google_sdk_folder):
        """GoogleSDKBins initialize.

        Args:
            google_sdk_folder: String, google sdk path.
        """
        self.gcloud_command_path = os.path.join(google_sdk_folder, "gcloud")
        self.gsutil_command_path = os.path.join(google_sdk_folder, "gsutil")
        # TODO(137195528): Remove python2 environment after acloud support python3.
        self._env = os.environ.copy()
        self._env[_ENV_CLOUDSDK_PYTHON] = "python2"

    def RunGcloud(self, cmd, **kwargs):
        """Run gcloud command.

        Args:
            cmd: String list, command strings.
                  Ex: [config], then this function call "gcloud config".
            **kwargs: dictionary of keyword based args to pass to func.

        Returns:
            String, return message after execute gcloud command.
        """
        return subprocess.check_output([self.gcloud_command_path] + cmd,
                                       env=self._env, **kwargs)

    def RunGsutil(self, cmd, **kwargs):
        """Run gsutil command.

        Args:
            cmd : String list, command strings.
                  Ex: [list], then this function call "gsutil list".
            **kwargs: dictionary of keyword based args to pass to func.

        Returns:
            String, return message after execute gsutil command.
        """
        return subprocess.check_output([self.gsutil_command_path] + cmd,
                                       env=self._env, **kwargs)


class GoogleAPIService(object):
    """Class to enable api service in the gcp project."""

    def __init__(self, service_name, error_msg, required=False):
        """GoogleAPIService initialize.

        Args:
            service_name: String, name of api service.
            error_msg: String, show messages if api service enable failed.
            required: Boolean, True for service must be enabled for acloud.
        """
        self._name = service_name
        self._error_msg = error_msg
        self._required = required

    def EnableService(self, gcloud_runner):
        """Enable api service.

        Args:
            gcloud_runner: A GcloudRunner class to run "gcloud" command.
        """
        try:
            gcloud_runner.RunGcloud(["services", "enable", self._name],
                                    stderr=subprocess.STDOUT)
        except subprocess.CalledProcessError as error:
            self.ShowFailMessages(error.output)

    def ShowFailMessages(self, error):
        """Show fail messages.

        Show the fail messages to hint users the impact if the api service
        isn't enabled.

        Args:
            error: String of error message when opening api service failed.
        """
        msg_color = (utils.TextColors.FAIL if self._required else
                     utils.TextColors.WARNING)
        utils.PrintColorString(
            error + _OPEN_SERVICE_FAILED_MSG % {
                "service_name": self._name,
                "service_msg": self._error_msg}
            , msg_color)

    @property
    def name(self):
        """Return name."""
        return self._name


class GcpTaskRunner(base_task_runner.BaseTaskRunner):
    """Runner to setup google cloud user information."""

    WELCOME_MESSAGE_TITLE = "Setup google cloud user information"
    WELCOME_MESSAGE = (
        "This step will walk you through gcloud SDK installation."
        "Then configure gcloud user information."
        "Finally enable some gcloud API services.")

    def __init__(self, config_path):
        """Initialize parameters.

        Load config file to get current values.

        Args:
            config_path: String, acloud config path.
        """
        # pylint: disable=invalid-name
        config_mgr = config.AcloudConfigManager(config_path)
        cfg = config_mgr.Load()
        self.config_path = config_mgr.user_config_path
        self.project = cfg.project
        self.zone = cfg.zone
        self.ssh_private_key_path = cfg.ssh_private_key_path
        self.ssh_public_key_path = cfg.ssh_public_key_path
        self.stable_host_image_name = cfg.stable_host_image_name
        self.client_id = cfg.client_id
        self.client_secret = cfg.client_secret
        self.service_account_name = cfg.service_account_name
        self.service_account_private_key_path = cfg.service_account_private_key_path
        self.service_account_json_private_key_path = cfg.service_account_json_private_key_path

    def ShouldRun(self):
        """Check if we actually need to run GCP setup.

        We'll only do the gcp setup if certain fields in the cfg are empty.

        Returns:
            True if reqired config fields are empty, False otherwise.
        """
        # We need to ensure the config has the proper auth-related fields set,
        # so config requires just 1 of the following:
        # 1. client id/secret
        # 2. service account name/private key path
        # 3. service account json private key path
        if ((not self.client_id or not self.client_secret)
                and (not self.service_account_name or not self.service_account_private_key_path)
                and not self.service_account_json_private_key_path):
            return True

        # If a project isn't set, then we need to run setup.
        return not self.project

    def _Run(self):
        """Run GCP setup task."""
        self._SetupGcloudInfo()
        SetupSSHKeys(self.config_path, self.ssh_private_key_path,
                     self.ssh_public_key_path)

    def _SetupGcloudInfo(self):
        """Setup Gcloud user information.
            1. Setup Gcloud SDK tools.
            2. Setup Gcloud project.
                a. Setup Gcloud project and zone.
                b. Setup Client ID and Client secret.
                c. Setup Google Cloud Storage bucket.
            3. Enable Gcloud API services.
        """
        google_sdk_init = google_sdk.GoogleSDK()
        try:
            google_sdk_runner = GoogleSDKBins(google_sdk_init.GetSDKBinPath())
            google_sdk_init.InstallGcloudComponent(google_sdk_runner,
                                                   _GCLOUD_COMPONENT_ALPHA)
            self._SetupProject(google_sdk_runner)
            self._EnableGcloudServices(google_sdk_runner)
            self._CreateStableHostImage()
        finally:
            google_sdk_init.CleanUp()

    def _CreateStableHostImage(self):
        """Create the stable host image."""
        # Write default stable_host_image_name with dummy value.
        # TODO(113091773): An additional step to create the host image.
        if not self.stable_host_image_name:
            UpdateConfigFile(self.config_path, "stable_host_image_name", "")


    def _NeedProjectSetup(self):
        """Confirm project setup should run or not.

        If the project settings (project name and zone) are blank (either one),
        we'll run the project setup flow. If they are set, we'll check with
        the user if they want to update them.

        Returns:
            Boolean: True if we need to setup the project, False otherwise.
        """
        user_question = (
            "Your default Project/Zone settings are:\n"
            "project:[%s]\n"
            "zone:[%s]\n"
            "Would you like to update them?[y/N]: \n") % (self.project, self.zone)

        if not self.project or not self.zone:
            logger.info("Project or zone is empty. Start to run setup process.")
            return True
        return utils.GetUserAnswerYes(user_question)

    def _NeedClientIDSetup(self, project_changed):
        """Confirm client setup should run or not.

        If project changed, client ID must also have to change.
        So tool will force to run setup function.
        If client ID or client secret is empty, tool force to run setup function.
        If project didn't change and config hold user client ID/secret, tool
        would skip client ID setup.

        Args:
            project_changed: Boolean, True for project changed.

        Returns:
            Boolean: True for run setup function.
        """
        if project_changed:
            logger.info("Your project changed. Start to run setup process.")
            return True
        elif not self.client_id or not self.client_secret:
            logger.info("Client ID or client secret is empty. Start to run setup process.")
            return True
        logger.info("Project was unchanged and client ID didn't need to changed.")
        return False

    def _SetupProject(self, gcloud_runner):
        """Setup gcloud project information.

        Setup project and zone.
        Setup client ID and client secret.
        Make sure billing account enabled in project.

        Args:
            gcloud_runner: A GcloudRunner class to run "gcloud" command.
        """
        project_changed = False
        if self._NeedProjectSetup():
            project_changed = self._UpdateProject(gcloud_runner)
        if self._NeedClientIDSetup(project_changed):
            self._SetupClientIDSecret()
        self._CheckBillingEnable(gcloud_runner)

    def _UpdateProject(self, gcloud_runner):
        """Setup gcloud project name and zone name and check project changed.

        Run "gcloud init" to handle gcloud project setup.
        Then "gcloud list" to get user settings information include "project" & "zone".
        Record project_changed for next setup steps.

        Args:
            gcloud_runner: A GcloudRunner class to run "gcloud" command.

        Returns:
            project_changed: True for project settings changed.
        """
        project_changed = False
        gcloud_runner.RunGcloud(["init"])
        gcp_config_list_out = gcloud_runner.RunGcloud(["config", "list"])
        for line in gcp_config_list_out.splitlines():
            project_match = _PROJECT_RE.match(line)
            if project_match:
                project = project_match.group("project")
                project_changed = (self.project != project)
                self.project = project
                continue
            zone_match = _ZONE_RE.match(line)
            if zone_match:
                self.zone = zone_match.group("zone")
                continue
        UpdateConfigFile(self.config_path, "project", self.project)
        UpdateConfigFile(self.config_path, "zone", self.zone)
        return project_changed

    def _SetupClientIDSecret(self):
        """Setup Client ID / Client Secret in config file.

        User can use input new values for Client ID and Client Secret.
        """
        print("Please generate a new client ID/secret by following the instructions here:")
        print("https://support.google.com/cloud/answer/6158849?hl=en")
        # TODO: Create markdown readme instructions since the link isn't too helpful.
        self.client_id = None
        self.client_secret = None
        while _InputIsEmpty(self.client_id):
            self.client_id = str(six.moves.input("Enter Client ID: ").strip())
        while _InputIsEmpty(self.client_secret):
            self.client_secret = str(six.moves.input("Enter Client Secret: ").strip())
        UpdateConfigFile(self.config_path, "client_id", self.client_id)
        UpdateConfigFile(self.config_path, "client_secret", self.client_secret)

    def _CheckBillingEnable(self, gcloud_runner):
        """Check billing enabled in gcp project.

        The billing info get by gcloud alpha command. Here is one example:
        $ gcloud alpha billing projects describe project_name
            billingAccountName: billingAccounts/011BXX-A30XXX-9XXXX
            billingEnabled: true
            name: projects/project_name/billingInfo
            projectId: project_name

        Args:
            gcloud_runner: A GcloudRunner class to run "gcloud" command.

        Raises:
            NoBillingError: gcp project doesn't enable billing account.
        """
        billing_info = gcloud_runner.RunGcloud(
            ["alpha", "billing", "projects", "describe", self.project])
        if _BILLING_ENABLE_MSG not in billing_info:
            raise errors.NoBillingError(
                "Please set billing account to project(%s) by following the "
                "instructions here: "
                "https://cloud.google.com/billing/docs/how-to/modify-project"
                % self.project)

    @staticmethod
    def _EnableGcloudServices(gcloud_runner):
        """Enable 3 Gcloud API services.

        1. Android build service
        2. Compute engine service
        To avoid confuse user, we don't show messages for services processing
        messages. e.g. "Waiting for async operation operations ...."

        Args:
            gcloud_runner: A GcloudRunner class to run "gcloud" command.
        """
        google_apis = [
            GoogleAPIService(_ANDROID_BUILD_SERVICE, _ANDROID_BUILD_MSG),
            GoogleAPIService(_COMPUTE_ENGINE_SERVICE, _COMPUTE_ENGINE_MSG, required=True)
        ]
        enabled_services = gcloud_runner.RunGcloud(
            ["services", "list", "--enabled", "--format", "value(NAME)"],
            stderr=subprocess.STDOUT).splitlines()

        for service in google_apis:
            if service.name not in enabled_services:
                service.EnableService(gcloud_runner)
