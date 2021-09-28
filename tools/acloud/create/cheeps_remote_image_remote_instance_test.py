"""Tests for cloud.android.driver.public.actions.create_cheeps_actions."""
import unittest
import uuid

import mock

from acloud.create import cheeps_remote_image_remote_instance
from acloud.internal import constants
from acloud.internal.lib import android_build_client
from acloud.internal.lib import android_compute_client
from acloud.internal.lib import auth
from acloud.internal.lib import cheeps_compute_client
from acloud.internal.lib import driver_test_lib
from acloud.internal.lib import ssh


class CheepsRemoteImageRemoteInstanceTest(driver_test_lib.BaseDriverTest):
    """Test cheeps_remote_image_remote_instance."""

    IP = ssh.IP(external="127.0.0.1", internal="10.0.0.1")
    INSTANCE = "fake-instance"
    IMAGE = "fake-image"
    GPU = "nvidia-tesla-k80"
    CHEEPS_HOST_IMAGE_NAME = "fake-stable-host-image-name"
    CHEEPS_HOST_IMAGE_PROJECT = "fake-stable-host-image-project"
    ANDROID_BUILD_ID = 12345
    ANDROID_BUILD_TARGET = "fake-target"

    def setUp(self):
        """Set up the test."""
        super(CheepsRemoteImageRemoteInstanceTest, self).setUp()
        self.build_client = mock.MagicMock()
        self.Patch(
            android_build_client,
            "AndroidBuildClient",
            return_value=self.build_client)
        self.compute_client = mock.MagicMock()
        self.Patch(
            cheeps_compute_client,
            "CheepsComputeClient",
            return_value=self.compute_client)
        self.Patch(
            android_compute_client,
            "AndroidComputeClient",
            return_value=self.compute_client)
        self.Patch(auth, "CreateCredentials", return_value=mock.MagicMock())

        # Mock uuid
        fake_uuid = mock.MagicMock(hex="1234")
        self.Patch(uuid, "uuid4", return_value=fake_uuid)

        # Mock compute client methods
        self.compute_client.GetInstanceIP.return_value = self.IP
        self.compute_client.GenerateImageName.return_value = self.IMAGE
        self.compute_client.GenerateInstanceName.return_value = self.INSTANCE

    def _CreateCfg(self):
        """A helper method that creates a mock configuration object."""
        cfg = mock.MagicMock()
        cfg.service_account_name = "fake@service.com"
        cfg.service_account_private_key_path = "/fake/path/to/key"
        cfg.zone = "fake_zone"
        cfg.ssh_private_key_path = ""
        cfg.ssh_public_key_path = ""
        cfg.stable_cheeps_host_image_name = self.CHEEPS_HOST_IMAGE_NAME
        cfg.stable_cheeps_host_image_project = self.CHEEPS_HOST_IMAGE_PROJECT
        return cfg

    def _CreateAvdSpec(self, stable_cheeps_host_image_name=None,
                       stable_cheeps_host_image_project=None):
        avd_spec = mock.MagicMock()
        avd_spec.cfg = self._CreateCfg()
        avd_spec.remote_image = {constants.BUILD_ID: self.ANDROID_BUILD_ID,
                                 constants.BUILD_TARGET: self.ANDROID_BUILD_TARGET}
        avd_spec.autoconnect = False
        avd_spec.report_internal_ip = False
        avd_spec.stable_cheeps_host_image_name = stable_cheeps_host_image_name
        avd_spec.stable_cheeps_host_image_project = stable_cheeps_host_image_project
        return avd_spec

    def testCreate(self):
        """Test CreateDevices."""
        avd_spec = self._CreateAvdSpec()
        instance = cheeps_remote_image_remote_instance.CheepsRemoteImageRemoteInstance()
        report = instance.Create(avd_spec, no_prompts=False)

        # Verify
        self.compute_client.CreateInstance.assert_called_with(
            instance=self.INSTANCE,
            image_name=self.CHEEPS_HOST_IMAGE_NAME,
            image_project=self.CHEEPS_HOST_IMAGE_PROJECT,
            avd_spec=avd_spec)

        self.assertEqual(report.data, {
            "devices": [{
                "build_id": self.ANDROID_BUILD_ID,
                "instance_name": self.INSTANCE,
                "ip": self.IP.external,
            },],
        })
        self.assertEqual(report.command, "create_cheeps")
        self.assertEqual(report.status, "SUCCESS")

    def testStableCheepsHostImageArgsOverrideConfig(self):
        """Test that Cheeps host image specifed through args (which goes into
        avd_spec) override values set in Acloud config."""
        stable_cheeps_host_image_name = 'override-stable-host-image-name'
        stable_cheeps_host_image_project = 'override-stable-host-image-project'
        self.assertNotEqual(stable_cheeps_host_image_name,
                            self.CHEEPS_HOST_IMAGE_NAME)
        self.assertNotEqual(stable_cheeps_host_image_project,
                            self.CHEEPS_HOST_IMAGE_PROJECT)

        avd_spec = self._CreateAvdSpec(stable_cheeps_host_image_name,
                                       stable_cheeps_host_image_project)
        instance = cheeps_remote_image_remote_instance.CheepsRemoteImageRemoteInstance()
        instance.Create(avd_spec, no_prompts=False)

        # Verify
        self.compute_client.CreateInstance.assert_called_with(
            instance=self.INSTANCE,
            image_name=stable_cheeps_host_image_name,
            image_project=stable_cheeps_host_image_project,
            avd_spec=avd_spec)

if __name__ == "__main__":
    unittest.main()
