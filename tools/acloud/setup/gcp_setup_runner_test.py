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
"""Tests for acloud.setup.gcp_setup_runner."""

import unittest
import os
import mock
import six

# pylint: disable=no-name-in-module,import-error,no-member
from acloud import errors
from acloud.internal.lib import utils
from acloud.internal.proto import user_config_pb2
from acloud.public import config
from acloud.setup import gcp_setup_runner

_GCP_USER_CONFIG = """
[compute]
region = new_region
zone = new_zone
[core]
account = new@google.com
disable_usage_reporting = False
project = new_project
"""


def _CreateCfgFile():
    """A helper method that creates a mock configuration object."""
    default_cfg = """
project: "fake_project"
zone: "fake_zone"
storage_bucket_name: "fake_bucket"
client_id: "fake_client_id"
client_secret: "fake_client_secret"
"""
    return default_cfg


# pylint: disable=protected-access
class AcloudGCPSetupTest(unittest.TestCase):
    """Test GCP Setup steps."""

    def setUp(self):
        """Create config and gcp_env_runner."""
        self.cfg_path = "acloud_unittest.config"
        file_write = open(self.cfg_path, 'w')
        file_write.write(_CreateCfgFile().strip())
        file_write.close()
        self.gcp_env_runner = gcp_setup_runner.GcpTaskRunner(self.cfg_path)
        self.gcloud_runner = gcp_setup_runner.GoogleSDKBins("")

    def tearDown(self):
        """Remove temp file."""
        if os.path.isfile(self.cfg_path):
            os.remove(self.cfg_path)

    def testUpdateConfigFile(self):
        """Test update config file."""
        # Test update project field.
        gcp_setup_runner.UpdateConfigFile(self.cfg_path, "project",
                                          "test_project")
        cfg = config.AcloudConfigManager.LoadConfigFromProtocolBuffer(
            open(self.cfg_path, "r"), user_config_pb2.UserConfig)
        self.assertEqual(cfg.project, "test_project")
        self.assertEqual(cfg.ssh_private_key_path, "")
        # Test add ssh key path in config.
        gcp_setup_runner.UpdateConfigFile(self.cfg_path,
                                          "ssh_private_key_path", "test_path")
        cfg = config.AcloudConfigManager.LoadConfigFromProtocolBuffer(
            open(self.cfg_path, "r"), user_config_pb2.UserConfig)
        self.assertEqual(cfg.project, "test_project")
        self.assertEqual(cfg.ssh_private_key_path, "test_path")
        # Test config is not a file
        with mock.patch("os.path.isfile") as chkfile:
            chkfile.return_value = False
            gcp_setup_runner.UpdateConfigFile(self.cfg_path, "project",
                                              "test_project")
            cfg = config.AcloudConfigManager.LoadConfigFromProtocolBuffer(
                open(self.cfg_path, "r"), user_config_pb2.UserConfig)
            self.assertEqual(cfg.project, "test_project")

    @mock.patch("os.path.dirname", return_value="")
    @mock.patch("subprocess.check_output")
    def testSeupProjectZone(self, mock_runner, mock_path):
        """Test setup project and zone."""
        gcloud_runner = gcp_setup_runner.GoogleSDKBins(mock_path)
        self.gcp_env_runner.project = "fake_project"
        self.gcp_env_runner.zone = "fake_zone"
        mock_runner.side_effect = [0, _GCP_USER_CONFIG]
        self.gcp_env_runner._UpdateProject(gcloud_runner)
        self.assertEqual(self.gcp_env_runner.project, "new_project")
        self.assertEqual(self.gcp_env_runner.zone, "new_zone")

    @mock.patch.object(six.moves, "input")
    def testSetupClientIDSecret(self, mock_id):
        """Test setup client ID and client secret."""
        self.gcp_env_runner.client_id = "fake_client_id"
        self.gcp_env_runner.client_secret = "fake_client_secret"
        mock_id.side_effect = ["new_id", "new_secret"]
        self.gcp_env_runner._SetupClientIDSecret()
        self.assertEqual(self.gcp_env_runner.client_id, "new_id")
        self.assertEqual(self.gcp_env_runner.client_secret, "new_secret")

    @mock.patch.object(gcp_setup_runner, "UpdateConfigFile")
    @mock.patch.object(utils, "CreateSshKeyPairIfNotExist")
    def testSetupSSHKeys(self, mock_check, mock_update):
        """Test setup the pair of the ssh key for acloud.config."""
        # Test ssh key has already setup
        gcp_setup_runner.SetupSSHKeys(self.cfg_path,
                                      "fake_private_key_path",
                                      "fake_public_key_path")
        self.assertEqual(mock_update.call_count, 0)
        # Test if private_key_path is empty string
        with mock.patch('os.path.expanduser') as ssh_path:
            ssh_path.return_value = ""
            gcp_setup_runner.SetupSSHKeys(self.cfg_path,
                                          "fake_private_key_path",
                                          "fake_public_key_path")
            mock_check.assert_called_once_with(
                gcp_setup_runner._DEFAULT_SSH_PRIVATE_KEY,
                gcp_setup_runner._DEFAULT_SSH_PUBLIC_KEY)

            mock_update.assert_has_calls([
                mock.call(self.cfg_path, "ssh_private_key_path",
                          gcp_setup_runner._DEFAULT_SSH_PRIVATE_KEY),
                mock.call(self.cfg_path, "ssh_public_key_path",
                          gcp_setup_runner._DEFAULT_SSH_PUBLIC_KEY)])

    @mock.patch.object(gcp_setup_runner.GcpTaskRunner, "_CreateStableHostImage")
    @mock.patch.object(gcp_setup_runner.GcpTaskRunner, "_EnableGcloudServices")
    @mock.patch.object(gcp_setup_runner.GcpTaskRunner, "_SetupProject")
    @mock.patch.object(gcp_setup_runner, "GoogleSDKBins")
    def testSetupGcloudInfo(self, mock_sdk, mock_set, mock_run, mock_create):
        """test setup gcloud info"""
        with mock.patch("google_sdk.GoogleSDK"):
            self.gcp_env_runner._SetupGcloudInfo()
            mock_sdk.assert_called_once()
            mock_set.assert_called_once()
            mock_run.assert_called_once()
            mock_create.assert_called_once()

    @mock.patch.object(gcp_setup_runner, "UpdateConfigFile")
    def testCreateStableHostImage(self, mock_update):
        """test create stable hostimage."""
        # Test no need to create stable hose image name.
        self.gcp_env_runner.stable_host_image_name = "fake_host_image_name"
        self.gcp_env_runner._CreateStableHostImage()
        self.assertEqual(mock_update.call_count, 0)
        # Test need to reset stable hose image name.
        self.gcp_env_runner.stable_host_image_name = ""
        self.gcp_env_runner._CreateStableHostImage()
        self.assertEqual(mock_update.call_count, 1)

    @mock.patch.object(gcp_setup_runner.GcpTaskRunner, "_CheckBillingEnable")
    @mock.patch.object(gcp_setup_runner.GcpTaskRunner, "_NeedProjectSetup")
    @mock.patch.object(gcp_setup_runner.GcpTaskRunner, "_SetupClientIDSecret")
    @mock.patch.object(gcp_setup_runner.GcpTaskRunner, "_UpdateProject")
    def testSetupProjectNoChange(self, mock_setproj, mock_setid,
                                 mock_chkproj, mock_check_billing):
        """test setup project and project not be changed."""
        # Test project didn't change, and no need to setup client id/secret
        mock_chkproj.return_value = False
        self.gcp_env_runner.client_id = "test_client_id"
        self.gcp_env_runner._SetupProject(self.gcloud_runner)
        self.assertEqual(mock_setproj.call_count, 0)
        self.assertEqual(mock_setid.call_count, 0)
        mock_check_billing.assert_called_once()
        # Test project didn't change, but client_id is empty
        self.gcp_env_runner.client_id = ""
        self.gcp_env_runner._SetupProject(self.gcloud_runner)
        self.assertEqual(mock_setproj.call_count, 0)
        mock_setid.assert_called_once()
        self.assertEqual(mock_check_billing.call_count, 2)

    @mock.patch.object(gcp_setup_runner.GcpTaskRunner, "_CheckBillingEnable")
    @mock.patch.object(gcp_setup_runner.GcpTaskRunner, "_NeedProjectSetup")
    @mock.patch.object(gcp_setup_runner.GcpTaskRunner, "_SetupClientIDSecret")
    @mock.patch.object(gcp_setup_runner.GcpTaskRunner, "_UpdateProject")
    def testSetupProjectChanged(self, mock_setproj, mock_setid,
                                mock_chkproj, mock_check_billing):
        """test setup project when project changed."""
        mock_chkproj.return_value = True
        mock_setproj.return_value = True
        self.gcp_env_runner._SetupProject(self.gcloud_runner)
        mock_setproj.assert_called_once()
        mock_setid.assert_called_once()
        mock_check_billing.assert_called_once()

    @mock.patch.object(utils, "GetUserAnswerYes")
    def testNeedProjectSetup(self, mock_ans):
        """test need project setup."""
        # Test need project setup.
        self.gcp_env_runner.project = ""
        self.gcp_env_runner.zone = ""
        self.assertTrue(self.gcp_env_runner._NeedProjectSetup())
        # Test no need project setup and get user's answer.
        self.gcp_env_runner.project = "test_project"
        self.gcp_env_runner.zone = "test_zone"
        self.gcp_env_runner._NeedProjectSetup()
        mock_ans.assert_called_once()

    def testNeedClientIDSetup(self):
        """test need client_id setup."""
        # Test project changed.
        self.assertTrue(self.gcp_env_runner._NeedClientIDSetup(True))
        # Test project is not changed but client_id or client_secret is empty.
        self.gcp_env_runner.client_id = ""
        self.gcp_env_runner.client_secret = ""
        self.assertTrue(self.gcp_env_runner._NeedClientIDSetup(False))
        # Test no need client_id setup.
        self.gcp_env_runner.client_id = "test_client_id"
        self.gcp_env_runner.client_secret = "test_client_secret"
        self.assertFalse(self.gcp_env_runner._NeedClientIDSetup(False))

    @mock.patch("subprocess.check_output")
    def testEnableGcloudServices(self, mock_run):
        """test enable Gcloud services."""
        mock_run.return_value = ""
        self.gcp_env_runner._EnableGcloudServices(self.gcloud_runner)
        mock_run.assert_has_calls([
            mock.call(["gcloud", "services", "enable",
                       gcp_setup_runner._ANDROID_BUILD_SERVICE],
                      env=self.gcloud_runner._env, stderr=-2),
            mock.call(["gcloud", "services", "enable",
                       gcp_setup_runner._COMPUTE_ENGINE_SERVICE],
                      env=self.gcloud_runner._env, stderr=-2)])

    @mock.patch("subprocess.check_output")
    def testGoogleAPIService(self, mock_run):
        """Test GoogleAPIService"""
        api_service = gcp_setup_runner.GoogleAPIService("service_name",
                                                        "error_message")
        api_service.EnableService(self.gcloud_runner)
        mock_run.assert_has_calls([
            mock.call(["gcloud", "services", "enable", "service_name"],
                      env=self.gcloud_runner._env, stderr=-2)])

    @mock.patch("subprocess.check_output")
    def testCheckBillingEnable(self, mock_run):
        """Test CheckBillingEnable"""
        # Test billing account in gcp project already enabled.
        mock_run.return_value = "billingEnabled: true"
        self.gcp_env_runner._CheckBillingEnable(self.gcloud_runner)
        mock_run.assert_has_calls([
            mock.call(
                [
                    "gcloud", "alpha", "billing", "projects", "describe",
                    self.gcp_env_runner.project
                ],
                env=self.gcloud_runner._env)
        ])

        # Test billing account in gcp project was not enabled.
        mock_run.return_value = "billingEnabled: false"
        with self.assertRaises(errors.NoBillingError):
            self.gcp_env_runner._CheckBillingEnable(self.gcloud_runner)


if __name__ == "__main__":
    unittest.main()
