#!/usr/bin/python2
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import itertools
import mock
import unittest

import common
from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib import hosts
from autotest_lib.client.common_lib.cros import retry
from autotest_lib.server.hosts import cros_firmware
from autotest_lib.server.hosts import cros_repair
from autotest_lib.server.hosts import repair_utils


CROS_VERIFY_DAG = (
    (repair_utils.SshVerifier, 'ssh', ()),
    (cros_repair.ServoTypeVerifier, 'servo_type', ()),
    (cros_repair.DevModeVerifier, 'devmode', ('ssh',)),
    (cros_repair.HWIDVerifier,    'hwid',    ('ssh',)),
    (cros_repair.ACPowerVerifier, 'power', ('ssh',)),
    (cros_repair.EXT4fsErrorVerifier, 'ext4', ('ssh',)),
    (cros_repair.WritableVerifier, 'writable', ('ssh',)),
    (cros_repair.TPMStatusVerifier, 'tpm', ('ssh',)),
    (cros_repair.UpdateSuccessVerifier, 'good_au', ('ssh',)),
    (cros_firmware.FirmwareStatusVerifier, 'fwstatus', ('ssh',)),
    (cros_firmware.FirmwareVersionVerifier, 'rwfw', ('ssh',)),
    (cros_repair.PythonVerifier, 'python', ('ssh',)),
    (repair_utils.LegacyHostVerifier, 'cros', ('ssh',)),
    (cros_repair.KvmExistsVerifier, 'ec_reset', ('ssh',)),
    (cros_repair.StopStartUIVerifier, 'stop_start_ui', ('ssh',)),
)

CROS_REPAIR_ACTIONS = (
    (repair_utils.RPMCycleRepair, 'rpm', (), ('ssh', 'power',)),
    (cros_repair.ServoSysRqRepair, 'sysrq', (), ('ssh',)),
    (cros_repair.ServoResetRepair, 'servoreset', (), ('ssh',)),
    (cros_firmware.FirmwareRepair,
     'firmware', (), ('ssh', 'fwstatus', 'good_au')),
    (cros_repair.CrosRebootRepair,
     'reboot', ('ssh',), ('devmode', 'writable',)),
    (cros_repair.ColdRebootRepair,
     'coldboot', ('ssh',), ('ec_reset',)),
    (cros_repair.AutoUpdateRepair,
     'au',
     ('ssh', 'writable', 'stop_start_ui', 'tpm', 'good_au', 'ext4'),
     ('power', 'rwfw', 'python', 'cros', 'ec_reset')),
    (cros_repair.PowerWashRepair,
     'powerwash',
     ('ssh', 'writable', 'stop_start_ui'),
     ('tpm', 'good_au', 'ext4', 'power', 'rwfw', 'python', 'cros', 'ec_reset')),
    (cros_repair.ServoInstallRepair,
     'usb',
     (),
     ('ssh', 'writable', 'stop_start_ui', 'tpm', 'good_au', 'ext4', 'power',
      'rwfw', 'python', 'cros', 'ec_reset')),
)

MOBLAB_VERIFY_DAG = (
    (repair_utils.SshVerifier, 'ssh', ()),
    (cros_repair.ACPowerVerifier, 'power', ('ssh',)),
    (cros_repair.PythonVerifier, 'python', ('ssh',)),
    (repair_utils.LegacyHostVerifier, 'cros', ('ssh',)),
)

MOBLAB_REPAIR_ACTIONS = (
    (repair_utils.RPMCycleRepair, 'rpm', (), ('ssh', 'power',)),
    (cros_repair.AutoUpdateRepair,
     'au', ('ssh',), ('power', 'python', 'cros',)),
)

JETSTREAM_VERIFY_DAG = (
    (repair_utils.SshVerifier, 'ssh', ()),
    (cros_repair.ServoTypeVerifier, 'servo_type', ()),
    (cros_repair.DevModeVerifier, 'devmode', ('ssh',)),
    (cros_repair.HWIDVerifier,    'hwid',    ('ssh',)),
    (cros_repair.ACPowerVerifier, 'power', ('ssh',)),
    (cros_repair.EXT4fsErrorVerifier, 'ext4', ('ssh',)),
    (cros_repair.WritableVerifier, 'writable', ('ssh',)),
    (cros_repair.TPMStatusVerifier, 'tpm', ('ssh',)),
    (cros_repair.UpdateSuccessVerifier, 'good_au', ('ssh',)),
    (cros_firmware.FirmwareStatusVerifier, 'fwstatus', ('ssh',)),
    (cros_firmware.FirmwareVersionVerifier, 'rwfw', ('ssh',)),
    (cros_repair.PythonVerifier, 'python', ('ssh',)),
    (repair_utils.LegacyHostVerifier, 'cros', ('ssh',)),
    (cros_repair.KvmExistsVerifier, 'ec_reset', ('ssh',)),
    (cros_repair.JetstreamTpmVerifier, 'jetstream_tpm', ('ssh',)),
    (cros_repair.JetstreamAttestationVerifier, 'jetstream_attestation',
     ('ssh',)),
    (cros_repair.JetstreamServicesVerifier, 'jetstream_services', ('ssh',)),
)

JETSTREAM_REPAIR_ACTIONS = (
    (repair_utils.RPMCycleRepair, 'rpm', (), ('ssh', 'power',)),
    (cros_repair.ServoSysRqRepair, 'sysrq', (), ('ssh',)),
    (cros_repair.ServoResetRepair, 'servoreset', (), ('ssh',)),
    (cros_firmware.FirmwareRepair,
     'firmware', (), ('ssh', 'fwstatus', 'good_au')),
    (cros_repair.CrosRebootRepair,
     'reboot', ('ssh',), ('devmode', 'writable',)),
    (cros_repair.ColdRebootRepair,
     'coldboot', ('ssh',), ('ec_reset',)),
    (cros_repair.JetstreamTpmRepair,
     'jetstream_tpm_repair',
     ('ssh', 'writable', 'tpm', 'good_au', 'ext4'),
     ('power', 'rwfw', 'python', 'cros', 'jetstream_tpm',
      'jetstream_attestation')),
    (cros_repair.JetstreamServiceRepair,
     'jetstream_service_repair',
     ('ssh', 'writable', 'tpm', 'good_au', 'ext4', 'jetstream_tpm',
      'jetstream_attestation'),
     ('power', 'rwfw', 'python', 'cros', 'jetstream_tpm',
      'jetstream_attestation', 'jetstream_services')),
    (cros_repair.AutoUpdateRepair,
     'au',
     ('ssh', 'writable', 'tpm', 'good_au', 'ext4'),
     ('power', 'rwfw', 'python', 'cros', 'jetstream_tpm',
      'jetstream_attestation', 'jetstream_services')),
    (cros_repair.PowerWashRepair,
     'powerwash',
     ('ssh', 'writable'),
     ('tpm', 'good_au', 'ext4', 'power', 'rwfw', 'python', 'cros',
      'jetstream_tpm', 'jetstream_attestation', 'jetstream_services')),
    (cros_repair.ServoInstallRepair,
     'usb',
     (),
     ('ssh', 'writable', 'tpm', 'good_au', 'ext4', 'power', 'rwfw', 'python',
      'cros', 'jetstream_tpm', 'jetstream_attestation', 'jetstream_services')),
)

CRYPTOHOME_STATUS_OWNED = """{
   "installattrs": {
      "first_install": true,
      "initialized": true,
      "invalid": false,
      "lockbox_index": 536870916,
      "lockbox_nvram_version": 2,
      "secure": true,
      "size": 0,
      "version": 1
   },
   "mounts": [  ],
   "tpm": {
      "being_owned": false,
      "can_connect": true,
      "can_decrypt": false,
      "can_encrypt": false,
      "can_load_srk": true,
      "can_load_srk_pubkey": true,
      "enabled": true,
      "has_context": true,
      "has_cryptohome_key": false,
      "has_key_handle": false,
      "last_error": 0,
      "owned": true
   }
}
"""

CRYPTOHOME_STATUS_NOT_OWNED = """{
   "installattrs": {
      "first_install": true,
      "initialized": true,
      "invalid": false,
      "lockbox_index": 536870916,
      "lockbox_nvram_version": 2,
      "secure": true,
      "size": 0,
      "version": 1
   },
   "mounts": [  ],
   "tpm": {
      "being_owned": false,
      "can_connect": true,
      "can_decrypt": false,
      "can_encrypt": false,
      "can_load_srk": false,
      "can_load_srk_pubkey": false,
      "enabled": true,
      "has_context": true,
      "has_cryptohome_key": false,
      "has_key_handle": false,
      "last_error": 0,
      "owned": false
   }
}
"""

CRYPTOHOME_STATUS_CANNOT_LOAD_SRK = """{
   "installattrs": {
      "first_install": true,
      "initialized": true,
      "invalid": false,
      "lockbox_index": 536870916,
      "lockbox_nvram_version": 2,
      "secure": true,
      "size": 0,
      "version": 1
   },
   "mounts": [  ],
   "tpm": {
      "being_owned": false,
      "can_connect": true,
      "can_decrypt": false,
      "can_encrypt": false,
      "can_load_srk": false,
      "can_load_srk_pubkey": false,
      "enabled": true,
      "has_context": true,
      "has_cryptohome_key": false,
      "has_key_handle": false,
      "last_error": 0,
      "owned": true
   }
}
"""

TPM_STATUS_READY = """
TPM Enabled: true
TPM Owned: true
TPM Being Owned: false
TPM Ready: true
TPM Password: 9eaee4da8b4c
"""

TPM_STATUS_NOT_READY = """
TPM Enabled: true
TPM Owned: false
TPM Being Owned: true
TPM Ready: false
TPM Password:
"""


class CrosRepairUnittests(unittest.TestCase):
    # pylint: disable=missing-docstring

    maxDiff = None

    def test_cros_repair(self):
        verify_dag = cros_repair._cros_verify_dag()
        self.assertTupleEqual(verify_dag, CROS_VERIFY_DAG)
        self.check_verify_dag(verify_dag)
        repair_actions = cros_repair._cros_repair_actions()
        self.assertTupleEqual(repair_actions, CROS_REPAIR_ACTIONS)
        self.check_repair_actions(verify_dag, repair_actions)

    def test_moblab_repair(self):
        verify_dag = cros_repair._moblab_verify_dag()
        self.assertTupleEqual(verify_dag, MOBLAB_VERIFY_DAG)
        self.check_verify_dag(verify_dag)
        repair_actions = cros_repair._moblab_repair_actions()
        self.assertTupleEqual(repair_actions, MOBLAB_REPAIR_ACTIONS)
        self.check_repair_actions(verify_dag, repair_actions)

    def test_jetstream_repair(self):
        verify_dag = cros_repair._jetstream_verify_dag()
        self.assertTupleEqual(verify_dag, JETSTREAM_VERIFY_DAG)
        self.check_verify_dag(verify_dag)
        repair_actions = cros_repair._jetstream_repair_actions()
        self.assertTupleEqual(repair_actions, JETSTREAM_REPAIR_ACTIONS)
        self.check_repair_actions(verify_dag, repair_actions)

    def check_verify_dag(self, verify_dag):
        """Checks that dependency labels are defined."""
        labels = [n[1] for n in verify_dag]
        for node in verify_dag:
            for dep in node[2]:
                self.assertIn(dep, labels)

    def check_repair_actions(self, verify_dag, repair_actions):
        """Checks that dependency and trigger labels are defined."""
        verify_labels = [n[1] for n in verify_dag]
        for action in repair_actions:
            deps = action[2]
            triggers = action[3]
            for label in deps + triggers:
                self.assertIn(label, verify_labels)

    def test_get_cryptohome_status_owned(self):
        mock_host = mock.Mock()
        mock_host.run.return_value.stdout = CRYPTOHOME_STATUS_OWNED
        status = cros_repair.CryptohomeStatus(mock_host)
        self.assertDictEqual({
            'being_owned': False,
            'can_connect': True,
            'can_decrypt': False,
            'can_encrypt': False,
            'can_load_srk': True,
            'can_load_srk_pubkey': True,
            'enabled': True,
            'has_context': True,
            'has_cryptohome_key': False,
            'has_key_handle': False,
            'last_error': 0,
            'owned': True,
            }, status['tpm'])
        self.assertTrue(status.tpm_enabled)
        self.assertTrue(status.tpm_owned)
        self.assertTrue(status.tpm_can_load_srk)
        self.assertTrue(status.tpm_can_load_srk_pubkey)

    def test_get_cryptohome_status_not_owned(self):
        mock_host = mock.Mock()
        mock_host.run.return_value.stdout = CRYPTOHOME_STATUS_NOT_OWNED
        status = cros_repair.CryptohomeStatus(mock_host)
        self.assertDictEqual({
            'being_owned': False,
            'can_connect': True,
            'can_decrypt': False,
            'can_encrypt': False,
            'can_load_srk': False,
            'can_load_srk_pubkey': False,
            'enabled': True,
            'has_context': True,
            'has_cryptohome_key': False,
            'has_key_handle': False,
            'last_error': 0,
            'owned': False,
        }, status['tpm'])
        self.assertTrue(status.tpm_enabled)
        self.assertFalse(status.tpm_owned)
        self.assertFalse(status.tpm_can_load_srk)
        self.assertFalse(status.tpm_can_load_srk_pubkey)

    @mock.patch.object(cros_repair, '_is_virtual_machine')
    def test_tpm_status_verifier_owned(self, mock_is_virt):
        mock_is_virt.return_value = False
        mock_host = mock.Mock()
        mock_host.run.return_value.stdout = CRYPTOHOME_STATUS_OWNED
        tpm_verifier = cros_repair.TPMStatusVerifier('test', [])
        tpm_verifier.verify(mock_host)

    @mock.patch.object(cros_repair, '_is_virtual_machine')
    def test_tpm_status_verifier_not_owned(self, mock_is_virt):
        mock_is_virt.return_value = False
        mock_host = mock.Mock()
        mock_host.run.return_value.stdout = CRYPTOHOME_STATUS_NOT_OWNED
        tpm_verifier = cros_repair.TPMStatusVerifier('test', [])
        tpm_verifier.verify(mock_host)

    @mock.patch.object(cros_repair, '_is_virtual_machine')
    def test_tpm_status_verifier_cannot_load_srk_pubkey(self, mock_is_virt):
        mock_is_virt.return_value = False
        mock_host = mock.Mock()
        mock_host.run.return_value.stdout = CRYPTOHOME_STATUS_CANNOT_LOAD_SRK
        tpm_verifier = cros_repair.TPMStatusVerifier('test', [])
        with self.assertRaises(hosts.AutoservVerifyError) as ctx:
            tpm_verifier.verify(mock_host)
        self.assertEqual('Cannot load the TPM SRK',
                         ctx.exception.message)

    def test_jetstream_tpm_owned(self):
        mock_host = mock.Mock()
        mock_host.run.side_effect = [
            mock.Mock(stdout=CRYPTOHOME_STATUS_OWNED),
            mock.Mock(stdout=TPM_STATUS_READY),
        ]
        tpm_verifier = cros_repair.JetstreamTpmVerifier('test', [])
        tpm_verifier.verify(mock_host)

    @mock.patch.object(retry.logging, 'warning')
    @mock.patch.object(retry.time, 'time')
    @mock.patch.object(retry.time, 'sleep')
    def test_jetstream_tpm_not_owned(self, mock_sleep, mock_time, mock_logging):
        mock_time.side_effect = itertools.count(0, 20)
        mock_host = mock.Mock()
        mock_host.run.return_value.stdout = CRYPTOHOME_STATUS_NOT_OWNED
        tpm_verifier = cros_repair.JetstreamTpmVerifier('test', [])
        with self.assertRaises(hosts.AutoservVerifyError) as ctx:
            tpm_verifier.verify(mock_host)
        self.assertEqual('TPM is not owned', ctx.exception.message)

    @mock.patch.object(retry.logging, 'warning')
    @mock.patch.object(retry.time, 'time')
    @mock.patch.object(retry.time, 'sleep')
    def test_jetstream_tpm_not_ready(self, mock_sleep, mock_time, mock_logging):
        mock_time.side_effect = itertools.count(0, 20)
        mock_host = mock.Mock()
        mock_host.run.side_effect = itertools.cycle([
            mock.Mock(stdout=CRYPTOHOME_STATUS_OWNED),
            mock.Mock(stdout=TPM_STATUS_NOT_READY),
        ])
        tpm_verifier = cros_repair.JetstreamTpmVerifier('test', [])
        with self.assertRaises(hosts.AutoservVerifyError) as ctx:
            tpm_verifier.verify(mock_host)
        self.assertEqual('TPM is not ready', ctx.exception.message)

    @mock.patch.object(retry.logging, 'warning')
    @mock.patch.object(retry.time, 'time')
    @mock.patch.object(retry.time, 'sleep')
    def test_jetstream_cryptohome_missing(self, mock_sleep, mock_time,
                                          mock_logging):
        mock_time.side_effect = itertools.count(0, 20)
        mock_host = mock.Mock()
        mock_host.run.side_effect = error.AutoservRunError('test', None)
        tpm_verifier = cros_repair.JetstreamTpmVerifier('test', [])
        with self.assertRaises(hosts.AutoservVerifyError) as ctx:
            tpm_verifier.verify(mock_host)
        self.assertEqual('Could not determine TPM status',
                         ctx.exception.message)


if __name__ == '__main__':
    unittest.main()
