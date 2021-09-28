# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import time

from autotest_lib.server import test
from autotest_lib.client.common_lib import error, utils
from autotest_lib.server.cros import gsutil_wrapper
from autotest_lib.server.cros.dynamic_suite import constants as ds_constants


class FingerprintTest(test.test):
    """Base class that sets up helpers for fingerprint tests."""
    version = 1

    _FINGERPRINT_BOARD_NAME_SUFFIX = '_fp'

    # Location of firmware from the build on the DUT
    _FINGERPRINT_BUILD_FW_DIR = '/opt/google/biod/fw'

    _GENIMAGES_SCRIPT_NAME = 'gen_test_images.sh'
    _GENIMAGES_OUTPUT_DIR_NAME = 'images'

    _TEST_IMAGE_FORMAT_MAP = {
        'TEST_IMAGE_ORIGINAL': '%s.bin',
        'TEST_IMAGE_DEV': '%s.dev',
        'TEST_IMAGE_CORRUPT_FIRST_BYTE': '%s_corrupt_first_byte.bin',
        'TEST_IMAGE_CORRUPT_LAST_BYTE': '%s_corrupt_last_byte.bin',
        'TEST_IMAGE_DEV_RB_ZERO': '%s.dev.rb0',
        'TEST_IMAGE_DEV_RB_ONE': '%s.dev.rb1',
        'TEST_IMAGE_DEV_RB_NINE': '%s.dev.rb9'
    }

    _ROLLBACK_INITIAL_BLOCK_ID = '1'
    _ROLLBACK_INITIAL_MIN_VERSION = '0'
    _ROLLBACK_INITIAL_RW_VERSION = '0'

    _SERVER_GENERATED_FW_DIR_NAME = 'generated_fw'

    _DUT_TMP_PATH_BASE = '/tmp/fp_test'

    # Name of key in "futility show" output corresponds to the signing key ID
    _FUTILITY_KEY_ID_KEY_NAME = 'ID'

    # Types of firmware
    _FIRMWARE_TYPE_RO = 'RO'
    _FIRMWARE_TYPE_RW = 'RW'

    # Types of signing keys
    _KEY_TYPE_DEV = 'dev'
    _KEY_TYPE_PRE_MP = 'premp'
    _KEY_TYPE_MP = 'mp'

    # EC board names for FPMCUs
    _FP_BOARD_NAME_DARTMONKEY = 'dartmonkey'
    _FP_BOARD_NAME_NOCTURNE = 'nocturne_fp'
    _FP_BOARD_NAME_NAMI = 'nami_fp'

    # Map from signing key ID to type of signing key
    _KEY_ID_MAP_ = {
        # dartmonkey
        '257a0aa3ac9e81aa4bc3aabdb6d3d079117c5799': _KEY_TYPE_MP,

        # nocturne
        '6f38c866182bd9bf7a4462c06ac04fa6a0074351': _KEY_TYPE_MP,
        'f6f7d96c48bd154dbae7e3fe3a3b4c6268a10934': _KEY_TYPE_PRE_MP,

        # nami
        '754aea623d69975a22998f7b97315dd53115d723': _KEY_TYPE_PRE_MP,
        '35486c0090ca390408f1fbbf2a182966084fe2f8': _KEY_TYPE_MP

    }

    # RO versions that are flashed in the factory
    # (for eternity for a given board)
    _GOLDEN_RO_FIRMWARE_VERSION_MAP = {
        _FP_BOARD_NAME_DARTMONKEY: 'dartmonkey_v2.0.2887-311310808',
        _FP_BOARD_NAME_NOCTURNE: 'nocturne_fp_v2.2.64-58cf5974e',
        _FP_BOARD_NAME_NAMI: 'nami_fp_v2.2.144-7a08e07eb',
    }

    _FIRMWARE_VERSION_SHA256SUM = 'sha256sum'
    _FIRMWARE_VERSION_RO_VERSION = 'ro_version'
    _FIRMWARE_VERSION_RW_VERSION = 'rw_version'
    _FIRMWARE_VERSION_KEY_ID = 'key_id'

    # Map of attributes for a given board's various firmware file releases
    #
    # Two purposes:
    #   1) Documents the exact versions and keys used for a given firmware file.
    #   2) Used to verify that files that end up in the build (and therefore
    #      what we release) is exactly what we expect.
    _FIRMWARE_VERSION_MAP = {
        _FP_BOARD_NAME_NOCTURNE: {
            'nocturne_fp_v2.2.110-b936c0a3c.bin': {
                _FIRMWARE_VERSION_SHA256SUM: '9da32787d68e2ac408be55a4d8c7de13e0f8c0a4a9912d69108b91794e90ee4b',
                _FIRMWARE_VERSION_RO_VERSION: 'nocturne_fp_v2.2.64-58cf5974e',
                _FIRMWARE_VERSION_RW_VERSION: 'nocturne_fp_v2.2.110-b936c0a3c',
                _FIRMWARE_VERSION_KEY_ID: '6f38c866182bd9bf7a4462c06ac04fa6a0074351',
            },
            'nocturne_fp_v2.2.191-1d529566e.bin': {
                _FIRMWARE_VERSION_SHA256SUM: 'e21223d6a3dbb7a6c6f2905f602f972b8a2b6d0fb52e262931745dae808e7c4d',
                _FIRMWARE_VERSION_RO_VERSION: 'nocturne_fp_v2.2.64-58cf5974e',
                _FIRMWARE_VERSION_RW_VERSION: 'nocturne_fp_v2.2.191-1d529566e',
                _FIRMWARE_VERSION_KEY_ID: '6f38c866182bd9bf7a4462c06ac04fa6a0074351',
            },
            'nocturne_fp_v2.0.3266-99b5e2c98.bin': {
                _FIRMWARE_VERSION_SHA256SUM: '73d822071518cf1b6e705d9c5903c2bcf37bae536784b275b96d916c44d3b6b7',
                _FIRMWARE_VERSION_RO_VERSION: 'nocturne_fp_v2.2.64-58cf5974e',
                _FIRMWARE_VERSION_RW_VERSION: 'nocturne_fp_v2.0.3266-99b5e2c98',
                _FIRMWARE_VERSION_KEY_ID: '6f38c866182bd9bf7a4462c06ac04fa6a0074351',
            },
        },
        _FP_BOARD_NAME_NAMI: {
            'nami_fp_v2.2.144-7a08e07eb.bin': {
                _FIRMWARE_VERSION_SHA256SUM: '375d7fcbb3f1fd8837b0572b4ef7fa848189d3ac53ced5dcb1abe3ddca9f11c4',
                _FIRMWARE_VERSION_RO_VERSION: 'nami_fp_v2.2.144-7a08e07eb',
                _FIRMWARE_VERSION_RW_VERSION: 'nami_fp_v2.2.144-7a08e07eb',
                _FIRMWARE_VERSION_KEY_ID: '35486c0090ca390408f1fbbf2a182966084fe2f8',
            },
            'nami_fp_v2.2.191-1d529566e.bin': {
                _FIRMWARE_VERSION_SHA256SUM: '684b63b1a2c929cf2be2fd564e9ae6032fdbbcfe0991d860c540420c40405776',
                _FIRMWARE_VERSION_RO_VERSION: 'nami_fp_v2.2.144-7a08e07eb',
                _FIRMWARE_VERSION_RW_VERSION: 'nami_fp_v2.2.191-1d529566e',
                _FIRMWARE_VERSION_KEY_ID: '35486c0090ca390408f1fbbf2a182966084fe2f8',
            },
            'nami_fp_v2.0.3266-99b5e2c98.bin': {
                _FIRMWARE_VERSION_SHA256SUM: '115bca7045428ce6639b41cc0fdc13d1ca414f6e76842e805a9fbb798a9cd7ad',
                _FIRMWARE_VERSION_RO_VERSION: 'nami_fp_v2.2.144-7a08e07eb',
                _FIRMWARE_VERSION_RW_VERSION: 'nami_fp_v2.0.3266-99b5e2c98',
                _FIRMWARE_VERSION_KEY_ID: '35486c0090ca390408f1fbbf2a182966084fe2f8',
            },
        },
        _FP_BOARD_NAME_DARTMONKEY: {
            'dartmonkey_v2.0.2887-311310808.bin': {
                _FIRMWARE_VERSION_SHA256SUM: '90716b73d1db5a1b6108530be1d11addf3b13e643bc6f96d417cbce383f3cb18',
                _FIRMWARE_VERSION_RO_VERSION: 'dartmonkey_v2.0.2887-311310808',
                _FIRMWARE_VERSION_RW_VERSION: 'dartmonkey_v2.0.2887-311310808',
                _FIRMWARE_VERSION_KEY_ID: '257a0aa3ac9e81aa4bc3aabdb6d3d079117c5799',
            },
            'dartmonkey_v2.0.3266-99b5e2c98.bin': {
                _FIRMWARE_VERSION_SHA256SUM: 'ac1c74b5d2676923f041ee1a27bf5b9892fab1d4f82fe924550a9b55917606ae',
                _FIRMWARE_VERSION_RO_VERSION: 'dartmonkey_v2.0.2887-311310808',
                _FIRMWARE_VERSION_RW_VERSION: 'dartmonkey_v2.0.3266-99b5e2c98',
                _FIRMWARE_VERSION_KEY_ID: '257a0aa3ac9e81aa4bc3aabdb6d3d079117c5799',
            }
        }
    }

    _BIOD_UPSTART_JOB_NAME = 'biod'
    # TODO(crbug.com/925545)
    _TIMBERSLIDE_UPSTART_JOB_NAME = \
        'timberslide LOG_PATH=/sys/kernel/debug/cros_fp/console_log'

    _INIT_ENTROPY_CMD = 'bio_wash --factory_init'

    _CROS_FP_ARG = '--name=cros_fp'
    _CROS_CONFIG_FINGERPRINT_PATH = '/fingerprint'
    _ECTOOL_RO_VERSION = 'RO version'
    _ECTOOL_RW_VERSION = 'RW version'
    _ECTOOL_FIRMWARE_COPY = 'Firmware copy'
    _ECTOOL_ROLLBACK_BLOCK_ID = 'Rollback block id'
    _ECTOOL_ROLLBACK_MIN_VERSION = 'Rollback min version'
    _ECTOOL_ROLLBACK_RW_VERSION = 'RW rollback version'

    @staticmethod
    def _parse_colon_delimited_output(ectool_output):
        """
        Converts ectool's (or any other tool with similar output) colon
        delimited output into python dict. Ignores any lines that do not have
        colons.

        Example:
        RO version:    nocturne_fp_v2.2.64-58cf5974e
        RW version:    nocturne_fp_v2.2.110-b936c0a3c

        becomes:
        {
          'RO version': 'nocturne_fp_v2.2.64-58cf5974e',
          'RW version': 'nocturne_fp_v2.2.110-b936c0a3c'
        }
        """
        ret = {}
        try:
            for line in ectool_output.strip().split('\n'):
                splits = line.split(':', 1)
                if len(splits) != 2:
                    continue
                key = splits[0].strip()
                val = splits[1].strip()
                ret[key] = val
        except:
            raise error.TestFail('Unable to parse ectool output: %s'
                                 % ectool_output)
        return ret

    def initialize(self, host, test_dir, use_dev_signed_fw=False,
                   enable_hardware_write_protect=True,
                   enable_software_write_protect=True,
                   force_firmware_flashing=False, init_entropy=True):
        """Performs initialization."""
        self.host = host
        self.servo = host.servo

        self._validate_compatible_servo_version()

        self.servo.initialize_dut()

        logging.info('HW write protect enabled: %s',
                     self.is_hardware_write_protect_enabled())

        # TODO(crbug.com/925545): stop timberslide so /var/log/cros_fp.log
        # continues to update after flashing.
        self._timberslide_running = self.host.upstart_status(
            self._TIMBERSLIDE_UPSTART_JOB_NAME)
        if self._timberslide_running:
            logging.info('Stopping %s', self._TIMBERSLIDE_UPSTART_JOB_NAME)
            self.host.upstart_stop(self._TIMBERSLIDE_UPSTART_JOB_NAME)

        self._biod_running = self.host.upstart_status(
            self._BIOD_UPSTART_JOB_NAME)
        if self._biod_running:
            logging.info('Stopping %s', self._BIOD_UPSTART_JOB_NAME)
            self.host.upstart_stop(self._BIOD_UPSTART_JOB_NAME)

        # create tmp working directory on device (automatically cleaned up)
        self._dut_working_dir = self.host.get_tmp_dir(
            parent=self._DUT_TMP_PATH_BASE)
        logging.info('Created dut_working_dir: %s', self._dut_working_dir)
        self.copy_files_to_dut(test_dir, self._dut_working_dir)

        self._build_fw_file = self.get_build_fw_file()
        self._validate_build_fw_file(self._build_fw_file)

        gen_script = os.path.abspath(os.path.join(self.autodir,
                                                  'server', 'cros', 'faft',
                                                  self._GENIMAGES_SCRIPT_NAME))
        self._dut_firmware_test_images_dir = \
            self._generate_test_firmware_images(gen_script,
                                                self._build_fw_file,
                                                self._dut_working_dir)
        logging.info('dut_firmware_test_images_dir: %s',
                     self._dut_firmware_test_images_dir)

        self._initialize_test_firmware_image_attrs(
            self._dut_firmware_test_images_dir)

        self._initialize_running_fw_version(use_dev_signed_fw,
                                            force_firmware_flashing)
        if init_entropy:
            self._initialize_fw_entropy()

        self._initialize_hw_and_sw_write_protect(enable_hardware_write_protect,
                                                 enable_software_write_protect)

    def cleanup(self):
        """Restores original state."""
        # Once the tests complete we need to make sure we're running the
        # original firmware (not dev version) and potentially reset rollback.
        self._initialize_running_fw_version(use_dev_signed_fw=False,
                                            force_firmware_flashing=False)
        self._initialize_fw_entropy()
        self._initialize_hw_and_sw_write_protect(
            enable_hardware_write_protect=True,
            enable_software_write_protect=True)
        if hasattr(self, '_biod_running') and self._biod_running:
            logging.info('Restarting biod')
            self.host.upstart_restart(self._BIOD_UPSTART_JOB_NAME)
        # TODO(crbug.com/925545)
        if hasattr(self, '_timberslide_running') and self._timberslide_running:
            logging.info('Restarting timberslide')
            self.host.upstart_restart(self._TIMBERSLIDE_UPSTART_JOB_NAME)

        super(FingerprintTest, self).cleanup()

    def after_run_once(self):
        """Logs which iteration just ran."""
        logging.info('successfully ran iteration %d', self.iteration)

    def _validate_compatible_servo_version(self):
        """Asserts if a compatible servo version is not attached."""
        servo_version = self.servo.get_servo_version()
        logging.info('servo version: %s', servo_version)

    def _generate_test_firmware_images(self, gen_script, build_fw_file,
                                       dut_working_dir):
        """
        Copies the fingerprint firmware from the DUT to the server running
        the tests, which runs a script to generate various test versions of
        the firmware.

        @return full path to location of test images on DUT
        """
        # create subdirectory under existing tmp dir
        server_tmp_dir = os.path.join(self.tmpdir,
                                      self._SERVER_GENERATED_FW_DIR_NAME)
        os.mkdir(server_tmp_dir)
        logging.info('server_tmp_dir: %s', server_tmp_dir)

        # Copy firmware from device to server
        self.get_files_from_dut(build_fw_file, server_tmp_dir)

        # Run the test image generation script on server
        pushd = os.getcwd()
        os.chdir(server_tmp_dir)
        cmd = ' '.join([gen_script,
                        self.get_fp_board(),
                        os.path.basename(build_fw_file)])
        self.run_server_cmd(cmd)
        os.chdir(pushd)

        # Copy resulting files to DUT tmp dir
        server_generated_images_dir = \
            os.path.join(server_tmp_dir, self._GENIMAGES_OUTPUT_DIR_NAME)
        self.copy_files_to_dut(server_generated_images_dir, dut_working_dir)

        return os.path.join(dut_working_dir, self._GENIMAGES_OUTPUT_DIR_NAME)

    def _initialize_test_firmware_image_attrs(self, dut_fw_test_images_dir):
        """Sets attributes with full path to test images on DUT.

        Example: self.TEST_IMAGE_DEV = /some/path/images/nocturne_fp.dev
        """
        for key, val in self._TEST_IMAGE_FORMAT_MAP.iteritems():
            full_path = os.path.join(dut_fw_test_images_dir,
                                     val % self.get_fp_board())
            setattr(self, key, full_path)

    def _initialize_running_fw_version(self, use_dev_signed_fw,
                                       force_firmware_flashing):
        """
        Ensures that the running firmware version matches build version
        and factory rollback settings; flashes to correct version if either
        fails to match is requested to force flashing.

        RO firmware: original version released at factory
        RW firmware: firmware from current build
        """
        build_rw_firmware_version = \
            self.get_build_rw_firmware_version(use_dev_signed_fw)
        golden_ro_firmware_version = \
            self.get_golden_ro_firmware_version(use_dev_signed_fw)
        logging.info('Build RW firmware version: %s', build_rw_firmware_version)
        logging.info('Golden RO firmware version: %s',
                     golden_ro_firmware_version)

        running_rw_firmware = self.ensure_running_rw_firmware()

        fw_versions_match = self.running_fw_version_matches_given_version(
            build_rw_firmware_version, golden_ro_firmware_version)

        if not running_rw_firmware or not fw_versions_match \
            or not self.is_rollback_set_to_initial_val() \
            or force_firmware_flashing:
            fw_file = self._build_fw_file
            if use_dev_signed_fw:
                fw_file = self.TEST_IMAGE_DEV
            self.flash_rw_ro_firmware(fw_file)
            if not self.running_fw_version_matches_given_version(
                build_rw_firmware_version, golden_ro_firmware_version):
                raise error.TestFail(
                    'Running firmware version does not match expected version')

    def _initialize_fw_entropy(self):
        """Sets the entropy (key) in FPMCU flash (if not set)."""
        result = self.run_cmd(self._INIT_ENTROPY_CMD)
        if result.exit_status != 0:
            raise error.TestFail('Unable to initialize entropy')

    def _initialize_hw_and_sw_write_protect(self, enable_hardware_write_protect,
                                            enable_software_write_protect):
        """Enables/disables hardware/software write protect."""
        # sw: 0, hw: 0 => initial_hw(0) -> sw(0) -> hw(0)
        # sw: 0, hw: 1 => initial_hw(0) -> sw(0) -> hw(1)
        # sw: 1, hw: 0 => initial_hw(1) -> sw(1) -> hw(0)
        # sw: 1, hw: 1 => initial_hw(1) -> sw(1) -> hw(1)
        hardware_write_protect_initial_enabled = True
        if not enable_software_write_protect:
            hardware_write_protect_initial_enabled = False

        self.set_hardware_write_protect(hardware_write_protect_initial_enabled)

        self.set_software_write_protect(enable_software_write_protect)
        self.set_hardware_write_protect(enable_hardware_write_protect)

    def get_fp_board(self):
        """Returns name of fingerprint EC.

        nocturne and nami are special cases and have "_fp" appended. Newer
        FPMCUs have unique names.
        See go/cros-fingerprint-firmware-branching-and-signing.
        """

        # For devices that don't have unibuild support (which is required to
        # use cros_config).
        # TODO(https://crrev.com/i/2313151): nami has unibuild support, but
        # needs its model.yaml updated.
        # TODO(https://crbug.com/1030862): remove when nocturne has cros_config
        #  support.
        board = self.host.get_board().replace(ds_constants.BOARD_PREFIX, '')
        if board == 'nami' or board == 'nocturne':
            return board + self._FINGERPRINT_BOARD_NAME_SUFFIX

        # Use cros_config to get fingerprint board.
        result = self._run_cros_config_cmd('board')
        if result.exit_status != 0:
            raise error.TestFail(
                'Unable to get fingerprint board with cros_config')
        return result.stdout.rstrip()

    def get_build_fw_file(self):
        """Returns full path to build FW file on DUT."""

        fp_board = self.get_fp_board()
        ls_cmd = 'ls ' + self._FINGERPRINT_BUILD_FW_DIR + '/' + fp_board \
                 + '*.bin'
        result = self.run_cmd(ls_cmd)
        if result.exit_status != 0:
            raise error.TestFail('Unable to find firmware from build on device')
        ret = result.stdout.rstrip()
        logging.info('Build firmware file: %s', ret)
        return ret

    def check_equal(self, a, b):
        """Raises exception if "a" does not equal "b"."""
        if a != b:
            raise error.TestFail('"%s" does not match expected "%s" for board '
                                 '%s' % (a, b, self.get_fp_board()))

    def _validate_build_fw_file(self, build_fw_file):
        """
        Checks that all attributes in the given firmware file match their
        expected values.
        """
        # check hash
        actual_hash = self._calculate_sha256sum(build_fw_file)
        expected_hash = self._get_expected_firmware_hash(build_fw_file)
        self.check_equal(actual_hash, expected_hash)

        # check signing key_id
        actual_key_id = self._read_firmware_key_id(build_fw_file)
        expected_key_id = self._get_expected_firmware_key_id(build_fw_file)
        self.check_equal(actual_key_id, expected_key_id)

        # check that signing key is "pre mass production" (pre-mp) or
        # "mass production" (MP) for firmware in the build
        key_type = self._get_key_type(actual_key_id)
        if not (key_type == self._KEY_TYPE_MP or
                key_type == self._KEY_TYPE_PRE_MP):
            raise error.TestFail('Firmware key type must be MP or PRE-MP '
                                 'for board: %s' % self.get_fp_board())

        # check ro_version
        actual_ro_version = self._read_firmware_ro_version(build_fw_file)
        expected_ro_version =\
            self._get_expected_firmware_ro_version(build_fw_file)
        self.check_equal(actual_ro_version, expected_ro_version)

        # check rw_version
        actual_rw_version = self._read_firmware_rw_version(build_fw_file)
        expected_rw_version =\
            self._get_expected_firmware_rw_version(build_fw_file)
        self.check_equal(actual_rw_version, expected_rw_version)

        logging.info("Validated build firmware metadata.")

    def _get_key_type(self, key_id):
        """Returns the key "type" for a given "key id"."""
        key_type = self._KEY_ID_MAP_.get(key_id)
        if key_type is None:
            raise error.TestFail('Unable to get key type for key id: %s'
                                 % key_id)
        return key_type

    def _get_expected_firmware_info(self, build_fw_file, info_type):
        """
        Returns expected firmware info for a given firmware file name.
        """
        build_fw_file_name = os.path.basename(build_fw_file)

        board = self.get_fp_board()
        board_expected_fw_info = self._FIRMWARE_VERSION_MAP.get(board)
        if board_expected_fw_info is None:
            raise error.TestFail('Unable to get firmware info for board: %s'
                                 % board)

        expected_fw_info = board_expected_fw_info.get(build_fw_file_name)
        if expected_fw_info is None:
            raise error.TestFail('Unable to get firmware info for file: %s'
                                 % build_fw_file_name)

        ret = expected_fw_info.get(info_type)
        if ret is None:
            raise error.TestFail('Unable to get firmware info type: %s'
                                 % info_type)

        return ret

    def _get_expected_firmware_hash(self, build_fw_file):
        """Returns expected hash of firmware file."""
        return self._get_expected_firmware_info(
            build_fw_file, self._FIRMWARE_VERSION_SHA256SUM)

    def _get_expected_firmware_key_id(self, build_fw_file):
        """Returns expected "key id" for firmware file."""
        return self._get_expected_firmware_info(
            build_fw_file, self._FIRMWARE_VERSION_KEY_ID)

    def _get_expected_firmware_ro_version(self, build_fw_file):
        """Returns expected RO version for firmware file."""
        return self._get_expected_firmware_info(
            build_fw_file, self._FIRMWARE_VERSION_RO_VERSION)

    def _get_expected_firmware_rw_version(self, build_fw_file):
        """Returns expected RW version for firmware file."""
        return self._get_expected_firmware_info(
            build_fw_file, self._FIRMWARE_VERSION_RW_VERSION)

    def _read_firmware_key_id(self, file_name):
        """Returns "key id" as read from the given file."""
        result = self._run_futility_show_cmd(file_name)
        parsed = self._parse_colon_delimited_output(result)
        key_id = parsed.get(self._FUTILITY_KEY_ID_KEY_NAME)
        if key_id is None:
            raise error.TestFail('Failed to get key ID for file: %s'
                                 % file_name)
        return key_id

    def _read_firmware_ro_version(self, file_name):
        """Returns RO firmware version as read from the given file."""
        return self._run_dump_fmap_cmd(file_name, 'RO_FRID')

    def _read_firmware_rw_version(self, file_name):
        """Returns RW firmware version as read from the given file."""
        return self._run_dump_fmap_cmd(file_name, 'RW_FWID')

    def _calculate_sha256sum(self, file_name):
        """Returns SHA256 hash of the given file contents."""
        result = self._run_sha256sum_cmd(file_name)
        return result.stdout.split()[0]

    def _get_running_firmware_info(self, key):
        """
        Returns requested firmware info (RW version, RO version, or firmware
        type).
        """
        result = self._run_ectool_cmd('version')
        parsed = self._parse_colon_delimited_output(result.stdout)
        if result.exit_status != 0:
            raise error.TestFail('Failed to get running firmware info')
        info = parsed.get(key)
        if info is None:
            raise error.TestFail(
                'Failed to get running firmware info: %s' % key)
        return info

    def get_running_rw_firmware_version(self):
        """Returns running RW firmware version."""
        return self._get_running_firmware_info(self._ECTOOL_RW_VERSION)

    def get_running_ro_firmware_version(self):
        """Returns running RO firmware version."""
        return self._get_running_firmware_info(self._ECTOOL_RO_VERSION)

    def get_running_firmware_type(self):
        """Returns type of firmware we are running (RW or RO)."""
        return self._get_running_firmware_info(self._ECTOOL_FIRMWARE_COPY)

    def _get_rollback_info(self, info_type):
        """Returns requested type of rollback info."""
        result = self._run_ectool_cmd('rollbackinfo')
        parsed = self._parse_colon_delimited_output(result.stdout)
        if result.exit_status != 0:
            raise error.TestFail('Failed to get rollback info')
        info = parsed.get(info_type)
        if info is None:
            raise error.TestFail('Failed to get rollback info: %s' % info_type)
        return info

    def get_rollback_id(self):
        """Returns rollback ID."""
        return self._get_rollback_info(self._ECTOOL_ROLLBACK_BLOCK_ID)

    def get_rollback_min_version(self):
        """Returns rollback min version."""
        return self._get_rollback_info(self._ECTOOL_ROLLBACK_MIN_VERSION)

    def get_rollback_rw_version(self):
        """Returns RW rollback version."""
        return self._get_rollback_info(self._ECTOOL_ROLLBACK_RW_VERSION)

    def _construct_dev_version(self, orig_version):
        """
        Given a "regular" version string from a signed build, returns the
        special "dev" version that we use when creating the test images.
        """
        fw_version = orig_version
        if len(fw_version) + len('.dev') > 31:
            fw_version = fw_version[:27]
        fw_version = fw_version + '.dev'
        return fw_version

    def get_golden_ro_firmware_version(self, use_dev_signed_fw):
        """Returns RO firmware version used in factory."""
        board = self.get_fp_board()
        golden_version = self._GOLDEN_RO_FIRMWARE_VERSION_MAP.get(board)
        if golden_version is None:
            raise error.TestFail('Unable to get golden RO version for board: %s'
                                 % board)
        if use_dev_signed_fw:
            golden_version = self._construct_dev_version(golden_version)
        return golden_version

    def get_build_rw_firmware_version(self, use_dev_signed_fw):
        """Returns RW firmware version from build (based on filename)."""
        fw_file = os.path.basename(self._build_fw_file)
        if not fw_file.endswith('.bin'):
            raise error.TestFail('Unexpected filename for RW firmware: %s'
                                 % fw_file)
        fw_version = fw_file[:-4]
        if use_dev_signed_fw:
            fw_version = self._construct_dev_version(fw_version)
        return fw_version

    def ensure_running_rw_firmware(self):
        """
        Check whether the device is running RW firmware. If not, try rebooting
        to RW.

        @return true if successfully verified running RW firmware, false
        otherwise.
        """
        try:
            if self.get_running_firmware_type() != self._FIRMWARE_TYPE_RW:
                self._reboot_ec()
                if self.get_running_firmware_type() != self._FIRMWARE_TYPE_RW:
                    # RW may be corrupted.
                    return False
        except:
            # We may not always be able to read the firmware version.
            # For example, if the firmware is erased due to RDP1, running any
            # commands (such as getting the version) won't work.
            return False
        return True

    def running_fw_version_matches_given_version(self, rw_version, ro_version):
        """
        Returns True if the running RO and RW firmware versions match the
        provided versions.
        """
        try:
            running_rw_firmware_version = self.get_running_rw_firmware_version()
            running_ro_firmware_version = self.get_running_ro_firmware_version()

            logging.info('RW firmware, running: %s, expected: %s',
                         running_rw_firmware_version, rw_version)
            logging.info('RO firmware, running: %s, expected: %s',
                         running_ro_firmware_version, ro_version)

            return (running_rw_firmware_version == rw_version and
                    running_ro_firmware_version == ro_version)
        except:
            # We may not always be able to read the firmware version.
            # For example, if the firmware is erased due to RDP1, running any
            # commands (such as getting the version) won't work.
            return False

    def is_rollback_set_to_initial_val(self):
        """
        Returns True if rollbackinfo matches the initial value that it
        should have coming from the factory.
        """
        return (self.get_rollback_id() ==
                self._ROLLBACK_INITIAL_BLOCK_ID
                and
                self.get_rollback_min_version() ==
                self._ROLLBACK_INITIAL_MIN_VERSION
                and
                self.get_rollback_rw_version() ==
                self._ROLLBACK_INITIAL_RW_VERSION)

    def _download_firmware(self, gs_path, dut_file_path):
        """Downloads firmware from Google Storage bucket."""
        bucket = os.path.dirname(gs_path)
        filename = os.path.basename(gs_path)
        logging.info('Downloading firmware, '
                     'bucket: %s, filename: %s, dest: %s',
                     bucket, filename, dut_file_path)
        gsutil_wrapper.copy_private_bucket(host=self.host,
                                           bucket=bucket,
                                           filename=filename,
                                           destination=dut_file_path)
        return os.path.join(dut_file_path, filename)

    def flash_rw_firmware(self, fw_path):
        """Flashes the RW (read-write) firmware."""
        flash_cmd = os.path.join(self._dut_working_dir,
                                 'flash_fp_rw.sh' + ' ' + fw_path)
        result = self.run_cmd(flash_cmd)
        if result.exit_status != 0:
            raise error.TestFail('Flashing RW firmware failed')

    def flash_rw_ro_firmware(self, fw_path):
        """Flashes *all* firmware (both RO and RW)."""
        self.set_hardware_write_protect(False)
        flash_cmd = 'flash_fp_mcu' + ' ' + fw_path
        logging.info('Running flash cmd: %s', flash_cmd)
        result = self.run_cmd(flash_cmd)
        self.set_hardware_write_protect(True)
        if result.exit_status != 0:
            raise error.TestFail('Flashing RW/RO firmware failed')

    def is_hardware_write_protect_enabled(self):
        """Returns state of hardware write protect."""
        fw_wp_state = self.servo.get('fw_wp_state')
        return fw_wp_state == 'on' or fw_wp_state == 'force_on'

    def set_hardware_write_protect(self, enable):
        """Enables or disables hardware write protect."""
        self.servo.set('fw_wp_state', 'force_on' if enable else 'force_off')

    def set_software_write_protect(self, enable):
        """Enables or disables software write protect."""
        arg  = 'enable' if enable else 'disable'
        self._run_ectool_cmd('flashprotect ' + arg)
        # TODO(b/116396469): The flashprotect command returns an error even on
        # success.
        # if result.exit_status != 0:
        #    raise error.TestFail('Failed to modify software write protect')

        # TODO(b/116396469): "flashprotect enable" command is slow, so wait for
        # it to complete before attempting to reboot.
        time.sleep(2)
        self._reboot_ec()

    def _reboot_ec(self):
        """Reboots the fingerprint MCU (FPMCU)."""
        self._run_ectool_cmd('reboot_ec')
        # TODO(b/116396469): The reboot_ec command returns an error even on
        # success.
        # if result.exit_status != 0:
        #    raise error.TestFail('Failed to reboot ec')
        time.sleep(2)

    def get_files_from_dut(self, src, dst):
        """Copes files from DUT to server."""
        logging.info('Copying files from (%s) to (%s).', src, dst)
        self.host.get_file(src, dst, delete_dest=True)

    def copy_files_to_dut(self, src_dir, dst_dir):
        """Copies files from server to DUT."""
        logging.info('Copying files from (%s) to (%s).', src_dir, dst_dir)
        self.host.send_file(src_dir, dst_dir, delete_dest=True)

    def run_server_cmd(self, command, timeout=60):
        """Runs command on server; return result with output and exit code."""
        logging.info('Server execute: %s', command)
        result = utils.run(command, timeout=timeout, ignore_status=True)
        logging.info('exit_code: %d', result.exit_status)
        logging.info('stdout:\n%s', result.stdout)
        logging.info('stderr:\n%s', result.stderr)
        return result

    def run_cmd(self, command, timeout=300):
        """Runs command on the DUT; return result with output and exit code."""
        logging.debug('DUT Execute: %s', command)
        result = self.host.run(command, timeout=timeout, ignore_status=True)
        logging.info('exit_code: %d', result.exit_status)
        logging.info('stdout:\n%s', result.stdout)
        logging.info('stderr:\n%s', result.stderr)
        return result

    def _run_ectool_cmd(self, command):
        """Runs ectool on DUT; return result with output and exit code."""
        cmd = 'ectool ' + self._CROS_FP_ARG + ' ' + command
        result = self.run_cmd(cmd)
        return result

    def _run_cros_config_cmd(self, command):
        """Runs cros_config on DUT; return result with output and exit code."""
        cmd = 'cros_config ' + self._CROS_CONFIG_FINGERPRINT_PATH + ' ' \
              + command
        result = self.run_cmd(cmd)
        return result

    def _run_dump_fmap_cmd(self, fw_file, section):
        """
        Runs "dump_fmap" on DUT for given file.
        Returns value of given section.
        """
        # Write result to stderr while redirecting stderr to stdout
        # and dropping stdout. This is done because dump_map only writes the
        # value read from a section to a file (will not just print it to
        # stdout).
        cmd = 'dump_fmap -x ' + fw_file + ' ' + section +\
              ':/dev/stderr /dev/stderr >& /dev/stdout > /dev/null'
        result = self.run_cmd(cmd)
        if result.exit_status != 0:
            raise error.TestFail('Failed to read section: %s' % section)
        return result.stdout.rstrip('\0')

    def _run_futility_show_cmd(self, fw_file):
        """
        Runs "futility show" on DUT for given file.
        Returns stdout on success.
        """
        futility_cmd = 'futility show ' + fw_file
        result = self.run_cmd(futility_cmd)
        if result.exit_status != 0:
            raise error.TestFail('Unable to run futility on device')
        return result.stdout

    def _run_sha256sum_cmd(self, file_name):
        """
        Runs "sha256sum" on DUT for given file.
        Returns stdout on success.
        """
        sha_cmd = 'sha256sum ' + file_name
        result = self.run_cmd(sha_cmd)
        if result.exit_status != 0:
            raise error.TestFail('Unable to calculate sha256sum on device')
        return result

    def run_test(self, test_name, *args):
        """Runs test on DUT."""
        logging.info('Running %s', test_name)
        # Redirecting stderr to stdout since some commands intentionally fail
        # and it's easier to read when everything ordered in the same output
        test_cmd = ' '.join([os.path.join(self._dut_working_dir, test_name)] +
                            list(args) + ['2>&1'])
        # Change the working dir so we can write files from within the test
        # (otherwise defaults to $HOME (/root), which is not usually writable)
        # Note that dut_working_dir is automatically cleaned up so tests don't
        # need to worry about files from previous invocations or other tests.
        test_cmd = '(cd ' + self._dut_working_dir + ' && ' + test_cmd + ')'
        logging.info('Test command: %s', test_cmd)
        result = self.run_cmd(test_cmd)
        if result.exit_status != 0:
            raise error.TestFail(test_name + ' failed')
