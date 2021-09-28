#!/usr/bin/env python3
#
#   Copyright 2016 - The Android Open Source Project
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.

import logging
import mock
import os
import shutil
import tempfile
import unittest

from acts import logger
from acts.controllers import android_device
from acts.controllers.android_lib import errors

# Mock log path for a test run.
MOCK_LOG_PATH = "/tmp/logs/MockTest/xx-xx-xx_xx-xx-xx/"

# Mock start and end time of the adb cat.
MOCK_ADB_EPOCH_BEGIN_TIME = 191000123
MOCK_ADB_LOGCAT_BEGIN_TIME = logger.normalize_log_line_timestamp(
    logger.epoch_to_log_line_timestamp(MOCK_ADB_EPOCH_BEGIN_TIME))
MOCK_ADB_LOGCAT_END_TIME = "1970-01-02 21:22:02.000"

MOCK_SERIAL = 1
MOCK_RELEASE_BUILD_ID = "ABC1.123456.007"
MOCK_DEV_BUILD_ID = "ABC-MR1"
MOCK_NYC_BUILD_ID = "N4F27P"


def get_mock_ads(num):
    """Generates a list of mock AndroidDevice objects.

    The serial number of each device will be integer 0 through num - 1.

    Args:
        num: An integer that is the number of mock AndroidDevice objects to
            create.
    """
    ads = []
    for i in range(num):
        ad = mock.MagicMock(name="AndroidDevice", serial=i, h_port=None)
        ad.ensure_screen_on = mock.MagicMock(return_value=True)
        ads.append(ad)
    return ads


def mock_get_all_instances():
    return get_mock_ads(5)


def mock_list_adb_devices():
    return [ad.serial for ad in get_mock_ads(5)]


class MockAdbProxy(object):
    """Mock class that swaps out calls to adb with mock calls."""

    def __init__(self,
                 serial,
                 fail_br=False,
                 fail_br_before_N=False,
                 build_id=MOCK_RELEASE_BUILD_ID,
                 return_value=None):
        self.serial = serial
        self.fail_br = fail_br
        self.fail_br_before_N = fail_br_before_N
        self.return_value = return_value
        self.return_multiple = False
        self.build_id = build_id

    def shell(self, params, ignore_status=False, timeout=60):
        if params == "id -u":
            return "root"
        elif params == "bugreportz":
            if self.fail_br:
                return "OMG I died!\n"
            return "OK:/path/bugreport.zip\n"
        elif params == "bugreportz -v":
            if self.fail_br_before_N:
                return "/system/bin/sh: bugreportz: not found"
            return "1.1"
        else:
            if self.return_multiple:
                return self.return_value.pop(0)
            else:
                return self.return_value

    def getprop(self, params):
        if params == "ro.build.id":
            return self.build_id
        elif params == "ro.build.version.incremental":
            return "123456789"
        elif params == "ro.build.type":
            return "userdebug"
        elif params == "ro.build.product" or params == "ro.product.name":
            return "FakeModel"
        elif params == "sys.boot_completed":
            return "1"

    def devices(self):
        return "\t".join([str(self.serial), "device"])

    def bugreport(self, params, timeout=android_device.BUG_REPORT_TIMEOUT):
        expected = os.path.join(
            logging.log_path, "AndroidDevice%s" % self.serial,
            "AndroidDevice%s_%s.txt" %
            (self.serial,
             logger.normalize_log_line_timestamp(MOCK_ADB_LOGCAT_BEGIN_TIME)))
        assert expected in params, "Expected '%s', got '%s'." % (expected,
                                                                 params)

    def __getattr__(self, name):
        """All calls to the none-existent functions in adb proxy would
        simply return the adb command string.
        """

        def adb_call(*args, **kwargs):
            arg_str = ' '.join(str(elem) for elem in args)
            return arg_str

        return adb_call


class MockFastbootProxy():
    """Mock class that swaps out calls to adb with mock calls."""

    def __init__(self, serial):
        self.serial = serial

    def devices(self):
        return "xxxx\tdevice\nyyyy\tdevice"

    def __getattr__(self, name):
        def fastboot_call(*args):
            arg_str = ' '.join(str(elem) for elem in args)
            return arg_str

        return fastboot_call


class ActsAndroidDeviceTest(unittest.TestCase):
    """This test class has unit tests for the implementation of everything
    under acts.controllers.android_device.
    """

    def setUp(self):
        # Set log_path to logging since acts logger setup is not called.
        if not hasattr(logging, "log_path"):
            setattr(logging, "log_path", "/tmp/logs")
        # Creates a temp dir to be used by tests in this test class.
        self.tmp_dir = tempfile.mkdtemp()

    def tearDown(self):
        """Removes the temp dir.
        """
        shutil.rmtree(self.tmp_dir)

    # Tests for android_device module functions.
    # These tests use mock AndroidDevice instances.

    @mock.patch.object(
        android_device, "get_all_instances", new=mock_get_all_instances)
    @mock.patch.object(
        android_device, "list_adb_devices", new=mock_list_adb_devices)
    def test_create_with_pickup_all(self):
        pick_all_token = android_device.ANDROID_DEVICE_PICK_ALL_TOKEN
        actual_ads = android_device.create(pick_all_token)
        for actual, expected in zip(actual_ads, get_mock_ads(5)):
            self.assertEqual(actual.serial, expected.serial)

    def test_create_with_empty_config(self):
        expected_msg = android_device.ANDROID_DEVICE_EMPTY_CONFIG_MSG
        with self.assertRaisesRegex(errors.AndroidDeviceConfigError,
                                    expected_msg):
            android_device.create([])

    def test_create_with_not_list_config(self):
        expected_msg = android_device.ANDROID_DEVICE_NOT_LIST_CONFIG_MSG
        with self.assertRaisesRegex(errors.AndroidDeviceConfigError,
                                    expected_msg):
            android_device.create("HAHA")

    def test_get_device_success_with_serial(self):
        ads = get_mock_ads(5)
        expected_serial = 0
        ad = android_device.get_device(ads, serial=expected_serial)
        self.assertEqual(ad.serial, expected_serial)

    def test_get_device_success_with_serial_and_extra_field(self):
        ads = get_mock_ads(5)
        expected_serial = 1
        expected_h_port = 5555
        ads[1].h_port = expected_h_port
        ad = android_device.get_device(
            ads, serial=expected_serial, h_port=expected_h_port)
        self.assertEqual(ad.serial, expected_serial)
        self.assertEqual(ad.h_port, expected_h_port)

    def test_get_device_no_match(self):
        ads = get_mock_ads(5)
        expected_msg = ("Could not find a target device that matches condition"
                        ": {'serial': 5}.")
        with self.assertRaisesRegex(ValueError, expected_msg):
            ad = android_device.get_device(ads, serial=len(ads))

    def test_get_device_too_many_matches(self):
        ads = get_mock_ads(5)
        target_serial = ads[1].serial = ads[0].serial
        expected_msg = "More than one device matched: \[0, 0\]"
        with self.assertRaisesRegex(ValueError, expected_msg):
            ad = android_device.get_device(ads, serial=target_serial)

    def test_start_services_on_ads(self):
        """Makes sure when an AndroidDevice fails to start some services, all
        AndroidDevice objects get cleaned up.
        """
        msg = "Some error happened."
        ads = get_mock_ads(3)
        ads[0].start_services = mock.MagicMock()
        ads[0].clean_up = mock.MagicMock()
        ads[1].start_services = mock.MagicMock()
        ads[1].clean_up = mock.MagicMock()
        ads[2].start_services = mock.MagicMock(
            side_effect=errors.AndroidDeviceError(msg))
        ads[2].clean_up = mock.MagicMock()
        with self.assertRaisesRegex(errors.AndroidDeviceError, msg):
            android_device._start_services_on_ads(ads)
        ads[0].clean_up.assert_called_once_with()
        ads[1].clean_up.assert_called_once_with()
        ads[2].clean_up.assert_called_once_with()

    # Tests for android_device.AndroidDevice class.
    # These tests mock out any interaction with the OS and real android device
    # in AndroidDeivce.

    @mock.patch(
        'acts.controllers.adb.AdbProxy',
        return_value=MockAdbProxy(MOCK_SERIAL))
    @mock.patch(
        'acts.controllers.fastboot.FastbootProxy',
        return_value=MockFastbootProxy(MOCK_SERIAL))
    def test_AndroidDevice_instantiation(self, MockFastboot, MockAdbProxy):
        """Verifies the AndroidDevice object's basic attributes are correctly
        set after instantiation.
        """
        ad = android_device.AndroidDevice(serial=MOCK_SERIAL)
        self.assertEqual(ad.serial, 1)
        self.assertEqual(ad.model, "fakemodel")
        self.assertIsNone(ad.adb_logcat_process)
        expected_lp = os.path.join(logging.log_path,
                                   "AndroidDevice%s" % MOCK_SERIAL)
        self.assertEqual(ad.log_path, expected_lp)

    @mock.patch(
        'acts.controllers.adb.AdbProxy',
        return_value=MockAdbProxy(MOCK_SERIAL))
    @mock.patch(
        'acts.controllers.fastboot.FastbootProxy',
        return_value=MockFastbootProxy(MOCK_SERIAL))
    def test_AndroidDevice_build_info_release(self, MockFastboot,
                                              MockAdbProxy):
        """Verifies the AndroidDevice object's basic attributes are correctly
        set after instantiation.
        """
        ad = android_device.AndroidDevice(serial=1)
        build_info = ad.build_info
        self.assertEqual(build_info["build_id"], "ABC1.123456.007")
        self.assertEqual(build_info["build_type"], "userdebug")

    @mock.patch(
        'acts.controllers.adb.AdbProxy',
        return_value=MockAdbProxy(MOCK_SERIAL, build_id=MOCK_DEV_BUILD_ID))
    @mock.patch(
        'acts.controllers.fastboot.FastbootProxy',
        return_value=MockFastbootProxy(MOCK_SERIAL))
    def test_AndroidDevice_build_info_dev(self, MockFastboot, MockAdbProxy):
        """Verifies the AndroidDevice object's basic attributes are correctly
        set after instantiation.
        """
        ad = android_device.AndroidDevice(serial=1)
        build_info = ad.build_info
        self.assertEqual(build_info["build_id"], "123456789")
        self.assertEqual(build_info["build_type"], "userdebug")

    @mock.patch(
        'acts.controllers.adb.AdbProxy',
        return_value=MockAdbProxy(MOCK_SERIAL, build_id=MOCK_NYC_BUILD_ID))
    @mock.patch(
        'acts.controllers.fastboot.FastbootProxy',
        return_value=MockFastbootProxy(MOCK_SERIAL))
    def test_AndroidDevice_build_info_nyc(self, MockFastboot, MockAdbProxy):
        """Verifies the AndroidDevice object's build id is set correctly for
        NYC releases.
        """
        ad = android_device.AndroidDevice(serial=1)
        build_info = ad.build_info
        self.assertEqual(build_info["build_id"], MOCK_NYC_BUILD_ID)

    @mock.patch(
        'acts.controllers.adb.AdbProxy',
        return_value=MockAdbProxy(MOCK_SERIAL))
    @mock.patch(
        'acts.controllers.fastboot.FastbootProxy',
        return_value=MockFastbootProxy(MOCK_SERIAL))
    @mock.patch('os.makedirs')
    @mock.patch('acts.utils.exe_cmd')
    @mock.patch(
        'acts.controllers.android_device.AndroidDevice.device_log_path',
        new_callable=mock.PropertyMock)
    def test_AndroidDevice_take_bug_report(self, mock_log_path, exe_mock,
                                           mock_makedirs, FastbootProxy,
                                           MockAdbProxy):
        """Verifies AndroidDevice.take_bug_report calls the correct adb command
        and writes the bugreport file to the correct path.
        """
        ad = android_device.AndroidDevice(serial=MOCK_SERIAL)
        mock_log_path.return_value = os.path.join(
            logging.log_path, "AndroidDevice%s" % ad.serial)
        ad.take_bug_report("test_something", 234325.32)
        mock_makedirs.assert_called_with(mock_log_path(), exist_ok=True)

    @mock.patch(
        'acts.controllers.adb.AdbProxy',
        return_value=MockAdbProxy(MOCK_SERIAL, fail_br=True))
    @mock.patch(
        'acts.controllers.fastboot.FastbootProxy',
        return_value=MockFastbootProxy(MOCK_SERIAL))
    @mock.patch('os.makedirs')
    @mock.patch('acts.utils.exe_cmd')
    @mock.patch(
        'acts.controllers.android_device.AndroidDevice.device_log_path',
        new_callable=mock.PropertyMock)
    def test_AndroidDevice_take_bug_report_fail(self, mock_log_path, *_):
        """Verifies AndroidDevice.take_bug_report writes out the correct message
        when taking bugreport fails.
        """
        ad = android_device.AndroidDevice(serial=MOCK_SERIAL)
        mock_log_path.return_value = os.path.join(
            logging.log_path, "AndroidDevice%s" % ad.serial)
        expected_msg = "Failed to take bugreport on 1: OMG I died!"
        with self.assertRaisesRegex(errors.AndroidDeviceError, expected_msg):
            ad.take_bug_report("test_something", 4346343.23)

    @mock.patch(
        'acts.controllers.adb.AdbProxy',
        return_value=MockAdbProxy(MOCK_SERIAL, fail_br_before_N=True))
    @mock.patch(
        'acts.controllers.fastboot.FastbootProxy',
        return_value=MockFastbootProxy(MOCK_SERIAL))
    @mock.patch('os.makedirs')
    @mock.patch('acts.utils.exe_cmd')
    @mock.patch(
        'acts.controllers.android_device.AndroidDevice.device_log_path',
        new_callable=mock.PropertyMock)
    def test_AndroidDevice_take_bug_report_fallback(
            self, mock_log_path, exe_mock, mock_makedirs, FastbootProxy,
            MockAdbProxy):
        """Verifies AndroidDevice.take_bug_report falls back to traditional
        bugreport on builds that do not have bugreportz.
        """
        ad = android_device.AndroidDevice(serial=MOCK_SERIAL)
        mock_log_path.return_value = os.path.join(
            logging.log_path, "AndroidDevice%s" % ad.serial)
        ad.take_bug_report("test_something", MOCK_ADB_EPOCH_BEGIN_TIME)
        mock_makedirs.assert_called_with(mock_log_path(), exist_ok=True)

    @mock.patch(
        'acts.controllers.adb.AdbProxy',
        return_value=MockAdbProxy(MOCK_SERIAL))
    @mock.patch(
        'acts.controllers.fastboot.FastbootProxy',
        return_value=MockFastbootProxy(MOCK_SERIAL))
    @mock.patch('acts.libs.proc.process.Process')
    def test_AndroidDevice_start_adb_logcat(self, proc_mock, FastbootProxy,
                                            MockAdbProxy):
        """Verifies the AndroidDevice method start_adb_logcat. Checks that the
        underlying logcat process is started properly and correct warning msgs
        are generated.
        """
        with mock.patch(('acts.controllers.android_lib.logcat.'
                         'create_logcat_keepalive_process'),
                        return_value=proc_mock) as create_proc_mock:
            ad = android_device.AndroidDevice(serial=MOCK_SERIAL)
            ad.start_adb_logcat()
            # Verify start did the correct operations.
            self.assertTrue(ad.adb_logcat_process)
            log_dir = "AndroidDevice%s" % ad.serial
            create_proc_mock.assert_called_with(ad.serial, log_dir, '-b all')
            proc_mock.start.assert_called_with()
            # Expect warning msg if start is called back to back.
            expected_msg = "Android device .* already has a running adb logcat"
            proc_mock.is_running.return_value = True
            with self.assertLogs(level='WARNING') as log:
                ad.start_adb_logcat()
                self.assertRegex(log.output[0], expected_msg)

    @mock.patch(
        'acts.controllers.adb.AdbProxy',
        return_value=MockAdbProxy(MOCK_SERIAL))
    @mock.patch(
        'acts.controllers.fastboot.FastbootProxy',
        return_value=MockFastbootProxy(MOCK_SERIAL))
    @mock.patch('acts.controllers.android_lib.logcat.'
                'create_logcat_keepalive_process')
    def test_AndroidDevice_start_adb_logcat_with_user_param(
            self, create_proc_mock, FastbootProxy, MockAdbProxy):
        """Verifies that start_adb_logcat generates the correct adb logcat
        command if adb_logcat_param is specified.
        """
        ad = android_device.AndroidDevice(serial=MOCK_SERIAL)
        ad.adb_logcat_param = "-b radio"
        ad.start_adb_logcat()
        # Verify that create_logcat_keepalive_process is called with the
        # correct command.
        log_dir = "AndroidDevice%s" % ad.serial
        create_proc_mock.assert_called_with(ad.serial, log_dir, '-b radio')

    @mock.patch(
        'acts.controllers.adb.AdbProxy',
        return_value=MockAdbProxy(MOCK_SERIAL))
    @mock.patch(
        'acts.controllers.fastboot.FastbootProxy',
        return_value=MockFastbootProxy(MOCK_SERIAL))
    @mock.patch('acts.libs.proc.process.Process')
    def test_AndroidDevice_stop_adb_logcat(self, proc_mock, FastbootProxy,
                                           MockAdbProxy):
        """Verifies the AndroidDevice method stop_adb_logcat. Checks that the
        underlying logcat process is stopped properly and correct warning msgs
        are generated.
        """
        ad = android_device.AndroidDevice(serial=MOCK_SERIAL)
        ad.adb_logcat_process = proc_mock
        # Expect warning msg if stop is called before start.
        expected_msg = (
            "Android device .* does not have an ongoing adb logcat")
        proc_mock.is_running.return_value = False
        with self.assertLogs(level='WARNING') as log:
            ad.stop_adb_logcat()
            self.assertRegex(log.output[0], expected_msg)

        # Verify the underlying process is stopped.
        proc_mock.is_running.return_value = True
        ad.stop_adb_logcat()
        proc_mock.stop.assert_called_with()

    @mock.patch(
        'acts.controllers.adb.AdbProxy',
        return_value=MockAdbProxy(MOCK_SERIAL))
    def test_get_apk_process_id_process_cannot_find(self, adb_proxy):
        ad = android_device.AndroidDevice(serial=MOCK_SERIAL)
        ad.adb.return_value = "does_not_contain_value"
        self.assertEqual(None, ad.get_package_pid("some_package"))

    @mock.patch(
        'acts.controllers.adb.AdbProxy',
        return_value=MockAdbProxy(MOCK_SERIAL))
    def test_get_apk_process_id_process_exists_second_try(self, adb_proxy):
        ad = android_device.AndroidDevice(serial=MOCK_SERIAL)
        ad.adb.return_multiple = True
        ad.adb.return_value = ["", "system 1 2 3 4  S com.some_package"]
        self.assertEqual(1, ad.get_package_pid("some_package"))

    @mock.patch(
        'acts.controllers.adb.AdbProxy',
        return_value=MockAdbProxy(MOCK_SERIAL))
    def test_get_apk_process_id_bad_return(self, adb_proxy):
        ad = android_device.AndroidDevice(serial=MOCK_SERIAL)
        ad.adb.return_value = "bad_return_index_error"
        self.assertEqual(None, ad.get_package_pid("some_package"))

    @mock.patch(
        'acts.controllers.adb.AdbProxy',
        return_value=MockAdbProxy(MOCK_SERIAL))
    def test_get_apk_process_id_bad_return(self, adb_proxy):
        ad = android_device.AndroidDevice(serial=MOCK_SERIAL)
        ad.adb.return_value = "bad return value error"
        self.assertEqual(None, ad.get_package_pid("some_package"))

    @mock.patch(
        'acts.controllers.adb.AdbProxy',
        return_value=MockAdbProxy(MOCK_SERIAL))
    def test_ensure_verity_enabled_only_system_enabled(self, adb_proxy):
        ad = android_device.AndroidDevice(serial=MOCK_SERIAL)
        root_user_id = '0'

        ad.adb.get_user_id = mock.MagicMock()
        ad.adb.get_user_id.return_value = root_user_id

        ad.adb.getprop = mock.MagicMock(side_effect=[
            '',  # system.verified
            '2'
        ])  # vendor.verified
        ad.adb.ensure_user = mock.MagicMock()
        ad.reboot = mock.MagicMock()
        ad.ensure_verity_enabled()
        ad.reboot.assert_called_once()

        ad.adb.ensure_user.assert_called_with(root_user_id)

    @mock.patch(
        'acts.controllers.adb.AdbProxy',
        return_value=MockAdbProxy(MOCK_SERIAL))
    def test_ensure_verity_enabled_only_vendor_enabled(self, adb_proxy):
        ad = android_device.AndroidDevice(serial=MOCK_SERIAL)
        root_user_id = '0'

        ad.adb.get_user_id = mock.MagicMock()
        ad.adb.get_user_id.return_value = root_user_id

        ad.adb.getprop = mock.MagicMock(side_effect=[
            '2',  # system.verified
            ''
        ])  # vendor.verified
        ad.adb.ensure_user = mock.MagicMock()
        ad.reboot = mock.MagicMock()

        ad.ensure_verity_enabled()

        ad.reboot.assert_called_once()
        ad.adb.ensure_user.assert_called_with(root_user_id)

    @mock.patch(
        'acts.controllers.adb.AdbProxy',
        return_value=MockAdbProxy(MOCK_SERIAL))
    def test_ensure_verity_enabled_both_enabled_at_start(self, adb_proxy):
        ad = android_device.AndroidDevice(serial=MOCK_SERIAL)
        root_user_id = '0'

        ad.adb.get_user_id = mock.MagicMock()
        ad.adb.get_user_id.return_value = root_user_id

        ad.adb.getprop = mock.MagicMock(side_effect=[
            '2',  # system.verified
            '2'
        ])  # vendor.verified
        ad.adb.ensure_user = mock.MagicMock()
        ad.reboot = mock.MagicMock()
        ad.ensure_verity_enabled()

        assert not ad.reboot.called

    @mock.patch(
        'acts.controllers.adb.AdbProxy',
        return_value=MockAdbProxy(MOCK_SERIAL))
    def test_ensure_verity_disabled_system_already_disabled(self, adb_proxy):
        ad = android_device.AndroidDevice(serial=MOCK_SERIAL)
        root_user_id = '0'

        ad.adb.get_user_id = mock.MagicMock()
        ad.adb.get_user_id.return_value = root_user_id

        ad.adb.getprop = mock.MagicMock(side_effect=[
            '2',  # system.verified
            ''
        ])  # vendor.verified
        ad.adb.ensure_user = mock.MagicMock()
        ad.reboot = mock.MagicMock()
        ad.ensure_verity_disabled()

        ad.reboot.assert_called_once()

    @mock.patch(
        'acts.controllers.adb.AdbProxy',
        return_value=MockAdbProxy(MOCK_SERIAL))
    def test_ensure_verity_disabled_vendor_already_disabled(self, adb_proxy):
        ad = android_device.AndroidDevice(serial=MOCK_SERIAL)
        root_user_id = '0'

        ad.adb.get_user_id = mock.MagicMock()
        ad.adb.get_user_id.return_value = root_user_id

        ad.adb.getprop = mock.MagicMock(side_effect=[
            '',  # system.verified
            '2'
        ])  # vendor.verified
        ad.adb.ensure_user = mock.MagicMock()
        ad.reboot = mock.MagicMock()

        ad.ensure_verity_disabled()

        ad.reboot.assert_called_once()
        ad.adb.ensure_user.assert_called_with(root_user_id)

    @mock.patch(
        'acts.controllers.adb.AdbProxy',
        return_value=MockAdbProxy(MOCK_SERIAL))
    def test_ensure_verity_disabled_disabled_at_start(self, adb_proxy):
        ad = android_device.AndroidDevice(serial=MOCK_SERIAL)
        root_user_id = '0'

        ad.adb.get_user_id = mock.MagicMock()
        ad.adb.get_user_id.return_value = root_user_id

        ad.adb.getprop = mock.MagicMock(side_effect=[
            '',  # system.verified
            ''
        ])  # vendor.verified
        ad.adb.ensure_user = mock.MagicMock()
        ad.reboot = mock.MagicMock()

        ad.ensure_verity_disabled()

        assert not ad.reboot.called

    @mock.patch(
        'acts.controllers.adb.AdbProxy',
        return_value=MockAdbProxy(MOCK_SERIAL))
    def test_push_system_file(self, adb_proxy):
        ad = android_device.AndroidDevice(serial=MOCK_SERIAL)
        ad.ensure_verity_disabled = mock.MagicMock()
        ad.adb.remount = mock.MagicMock()
        ad.adb.push = mock.MagicMock()

        ret = ad.push_system_file('asdf', 'jkl')
        self.assertTrue(ret)

    @mock.patch(
        'acts.controllers.adb.AdbProxy',
        return_value=MockAdbProxy(MOCK_SERIAL))
    def test_push_system_file_returns_false_on_error(self, adb_proxy):
        ad = android_device.AndroidDevice(serial=MOCK_SERIAL)
        ad.ensure_verity_disabled = mock.MagicMock()
        ad.adb.remount = mock.MagicMock()
        ad.adb.push = mock.MagicMock(return_value='error')

        ret = ad.push_system_file('asdf', 'jkl')
        self.assertFalse(ret)


if __name__ == "__main__":
    unittest.main()
