# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This class defines the CrosHost Label class."""

import collections
import logging
import os
import re

import common

from autotest_lib.client.bin import utils
from autotest_lib.client.common_lib import global_config
from autotest_lib.client.cros.audio import cras_utils
from autotest_lib.client.cros.video import constants as video_test_constants
from autotest_lib.server.cros.dynamic_suite import constants as ds_constants
from autotest_lib.server.hosts import base_label
from autotest_lib.server.hosts import common_label
from autotest_lib.server.hosts import servo_host
from autotest_lib.site_utils import hwid_lib

# pylint: disable=missing-docstring
LsbOutput = collections.namedtuple('LsbOutput', ['unibuild', 'board'])

# fallback values if we can't contact the HWID server
HWID_LABELS_FALLBACK = ['sku', 'phase', 'touchscreen', 'touchpad', 'variant', 'stylus']

# Repair and Deploy taskName
REPAIR_TASK_NAME = 'repair'
DEPLOY_TASK_NAME = 'deploy'


def _parse_lsb_output(host):
    """Parses the LSB output and returns key data points for labeling.

    @param host: Host that the command will be executed against
    @returns: LsbOutput with the result of parsing the /etc/lsb-release output
    """
    release_info = utils.parse_cmd_output('cat /etc/lsb-release',
                                          run_method=host.run)

    unibuild = release_info.get('CHROMEOS_RELEASE_UNIBUILD') == '1'
    return LsbOutput(unibuild, release_info['CHROMEOS_RELEASE_BOARD'])


class BoardLabel(base_label.StringPrefixLabel):
    """Determine the correct board label for the device."""

    _NAME = ds_constants.BOARD_PREFIX.rstrip(':')

    def generate_labels(self, host):
        # We only want to apply the board labels once, which is when they get
        # added to the AFE.  That way we don't have to worry about the board
        # label switching on us if the wrong builds get put on the devices.
        # crbug.com/624207 records one event of the board label switching
        # unexpectedly on us.
        board = host.host_info_store.get().board
        if board:
            return [board]
        for label in host._afe_host.labels:
            if label.startswith(self._NAME + ':'):
                return [label.split(':')[-1]]

        return [_parse_lsb_output(host).board]


class ModelLabel(base_label.StringPrefixLabel):
    """Determine the correct model label for the device."""

    _NAME = ds_constants.MODEL_LABEL

    def generate_labels(self, host):
        # Based on the issue explained in BoardLabel, return the existing
        # label if it has already been set once.
        model = host.host_info_store.get().model
        if model:
            return [model]
        for label in host._afe_host.labels:
            if label.startswith(self._NAME + ':'):
                return [label.split(':')[-1]]

        lsb_output = _parse_lsb_output(host)
        model = None

        if lsb_output.unibuild:
            test_label_cmd = 'cros_config / test-label'
            result = host.run(command=test_label_cmd, ignore_status=True)
            if result.exit_status == 0:
                model = result.stdout.strip()
            if not model:
                mosys_cmd = 'mosys platform model'
                result = host.run(command=mosys_cmd, ignore_status=True)
                if result.exit_status == 0:
                    model = result.stdout.strip()

        # We need some sort of backwards compatibility for boards that
        # are not yet supported with mosys and unified builds.
        # This is necessary so that we can begin changing cbuildbot to take
        # advantage of the model/board label differentiations for
        # scheduling, while still retaining backwards compatibility.
        return [model or lsb_output.board]


class DeviceSkuLabel(base_label.StringPrefixLabel):
    """Determine the correct device_sku label for the device."""

    _NAME =  ds_constants.DEVICE_SKU_LABEL

    def generate_labels(self, host):
        device_sku = host.host_info_store.get().device_sku
        if device_sku:
            return [device_sku]

        mosys_cmd = 'mosys platform sku'
        result = host.run(command=mosys_cmd, ignore_status=True)
        if result.exit_status == 0:
            return [result.stdout.strip()]

        return []

    def update_for_task(self, task_name):
        # This label is stored in the lab config, so only deploy tasks update it
        # or when no task name is mentioned.
        return task_name in (DEPLOY_TASK_NAME, '')


class BrandCodeLabel(base_label.StringPrefixLabel):
    """Determine the correct brand_code (aka RLZ-code) for the device."""

    _NAME =  ds_constants.BRAND_CODE_LABEL

    def generate_labels(self, host):
        brand_code = host.host_info_store.get().brand_code
        if brand_code:
            return [brand_code]

        cros_config_cmd = 'cros_config / brand-code'
        result = host.run(command=cros_config_cmd, ignore_status=True)
        if result.exit_status == 0:
            return [result.stdout.strip()]

        return []


class BluetoothLabel(base_label.BaseLabel):
    """Label indicating if bluetooth is detected."""

    _NAME = 'bluetooth'

    def exists(self, host):
        # Based on crbug.com/966219, the label is flipping sometimes.
        # Potentially this is caused by testing itself.
        # Making this label permanently sticky.
        info = host.host_info_store.get()
        for label in info.labels:
            if label.startswith(self._NAME):
                return True

        result = host.run('test -d /sys/class/bluetooth/hci0',
                          ignore_status=True)

        return result.exit_status == 0


class ECLabel(base_label.BaseLabel):
    """Label to determine the type of EC on this host."""

    _NAME = 'ec:cros'

    def exists(self, host):
        cmd = 'mosys ec info'
        # The output should look like these, so that the last field should
        # match our EC version scheme:
        #
        #   stm | stm32f100 | snow_v1.3.139-375eb9f
        #   ti | Unknown-10de | peppy_v1.5.114-5d52788
        #
        # Non-Chrome OS ECs will look like these:
        #
        #   ENE | KB932 | 00BE107A00
        #   ite | it8518 | 3.08
        #
        # And some systems don't have ECs at all (Lumpy, for example).
        regexp = r'^.*\|\s*(\S+_v\d+\.\d+\.\d+-[0-9a-f]+)\s*$'

        ecinfo = host.run(command=cmd, ignore_status=True)
        if ecinfo.exit_status == 0:
            res = re.search(regexp, ecinfo.stdout)
            if res:
                logging.info("EC version is %s", res.groups()[0])
                return True
            logging.info("%s got: %s", cmd, ecinfo.stdout)
            # Has an EC, but it's not a Chrome OS EC
        logging.info("%s exited with status %d", cmd, ecinfo.exit_status)
        return False


class Cr50Label(base_label.StringPrefixLabel):
    """Label indicating the cr50 image type."""

    _NAME = 'cr50'

    def __init__(self):
        self.ver = None

    def exists(self, host):
        # Make sure the gsctool version command runs ok
        self.ver = host.run('gsctool -a -f', ignore_status=True)
        return self.ver.exit_status == 0

    def _get_version(self, region):
        """Get the version number of the given region"""
        return re.search(region + ' (\d+\.\d+\.\d+)', self.ver.stdout).group(1)

    def generate_labels(self, host):
        # Check the major version to determine prePVT vs PVT
        version = self._get_version('RW')
        major_version = int(version.split('.')[1])
        # PVT images have a odd major version prePVT have even
        return ['pvt' if (major_version % 2) else 'prepvt']


class Cr50RWKeyidLabel(Cr50Label):
    """Label indicating the cr50 RW version."""
    _REGION = 'RW'
    _NAME = 'cr50-rw-keyid'

    def _get_keyid_info(self, region):
        """Get the keyid of the given region."""
        match = re.search('keyids:.*%s (\S+)' % region, self.ver.stdout)
        keyid = match.group(1).rstrip(',')
        is_prod = int(keyid, 16) & (1 << 2)
        return [keyid, 'prod' if is_prod else 'dev']

    def generate_labels(self, host):
        """Get the key type."""
        return self._get_keyid_info(self._REGION)


class Cr50ROKeyidLabel(Cr50RWKeyidLabel):
    """Label indicating the RO key type."""
    _REGION = 'RO'
    _NAME = 'cr50-ro-keyid'


class Cr50RWVersionLabel(Cr50Label):
    """Label indicating the cr50 RW version."""
    _REGION = 'RW'
    _NAME = 'cr50-rw-version'

    def generate_labels(self, host):
        """Get the version and key type"""
        return [self._get_version(self._REGION)]


class Cr50ROVersionLabel(Cr50RWVersionLabel):
    """Label indicating the RO version."""
    _REGION = 'RO'
    _NAME = 'cr50-ro-version'


class AccelsLabel(base_label.BaseLabel):
    """Determine the type of accelerometers on this host."""

    _NAME = 'accel:cros-ec'

    def exists(self, host):
        # Check to make sure we have ectool
        rv = host.run('which ectool', ignore_status=True)
        if rv.exit_status:
            logging.info("No ectool cmd found; assuming no EC accelerometers")
            return False

        # Check that the EC supports the motionsense command
        rv = host.run('ectool motionsense', ignore_status=True)
        if rv.exit_status:
            logging.info("EC does not support motionsense command; "
                         "assuming no EC accelerometers")
            return False

        # Check that EC motion sensors are active
        active = host.run('ectool motionsense active').stdout.split('\n')
        if active[0] == "0":
            logging.info("Motion sense inactive; assuming no EC accelerometers")
            return False

        logging.info("EC accelerometers found")
        return True


class ChameleonLabel(base_label.BaseLabel):
    """Determine if a Chameleon is connected to this host."""

    _NAME = 'chameleon'

    def exists(self, host):
        # See crbug.com/1004500#2 for details.
        # https://chromium.googlesource.com/chromiumos/third_party/autotest/+
        # /refs/heads/master/server/hosts/cros_host.py#335 shows that
        # _chameleon_host_list is not reliable.
        has_chameleon = len(host.chameleon_list) > 0
        # TODO(crbug.com/995900) -- debug why chameleon label is flipping
        try:
            logging.info("has_chameleon %s", has_chameleon)
            logging.info("chameleon_host_list %s",
                         getattr(host, "_chameleon_host_list", "NO_ATTRIBUTE"))
            logging.info("chameleon_list %s",
                         getattr(host, "chameleon_list", "NO_ATTRIBUTE"))
            logging.info("multi_chameleon %s",
                         getattr(host, "multi_chameleon", "NO_ATTRIBUTE"))
        except:
            pass
        return has_chameleon

    def update_for_task(self, task_name):
        # This label is stored in the state config, so only repair tasks update
        # it or when no task name is mentioned.
        return task_name in (REPAIR_TASK_NAME, '')


class ChameleonConnectionLabel(base_label.StringPrefixLabel):
    """Return the Chameleon connection label."""

    _NAME = 'chameleon'

    def exists(self, host):
        return len(host._chameleon_host_list) > 0


    def generate_labels(self, host):
        return [chameleon.get_label() for chameleon in host.chameleon_list]

    def update_for_task(self, task_name):
        # This label is stored in the lab config, so only deploy tasks update it
        # or when no task name is mentioned.
        return task_name in (DEPLOY_TASK_NAME, '')


class ChameleonPeripheralsLabel(base_label.StringPrefixLabel):
    """Return the Chameleon peripherals labels.

    The 'chameleon:bt_hid' label is applied if the bluetooth
    classic hid device, i.e, RN-42 emulation kit, is detected.

    Any peripherals plugged into the chameleon board would be
    detected and applied proper labels in this class.
    """

    _NAME = 'chameleon'

    def exists(self, host):
        return len(host._chameleon_host_list) > 0


    def generate_labels(self, host):
        labels_list = []

        for chameleon, chameleon_host in \
                        zip(host.chameleon_list, host._chameleon_host_list):
            labels = []
            try:
                bt_hid_device = chameleon.get_bluetooth_hid_mouse()
                if bt_hid_device.CheckSerialConnection():
                    labels.append('bt_hid')
            except:
                logging.error('Error with initializing bt_hid_mouse on '
                              'chameleon %s', chameleon_host.hostname)

            try:
                ble_hid_device = chameleon.get_ble_mouse()
                if ble_hid_device.CheckSerialConnection():
                    labels.append('bt_ble_hid')
            except:
                logging.error('Error with initializing ble_hid_mouse on '
                              'chameleon %s', chameleon_host.hostname)

            try:
                bt_a2dp_sink = chameleon.get_bluetooth_a2dp_sink()
                if bt_a2dp_sink.CheckSerialConnection():
                    labels.append('bt_a2dp_sink')
            except:
                logging.error('Error with initializing bt_a2dp_sink on '
                              'chameleon %s', chameleon_host.hostname)

            try:
                bt_base_device = chameleon.get_bluetooth_base()
                if bt_base_device.IsDetected():
                    labels.append('bt_base')
            except:
                logging.error('Error in detecting bt_base on '
                              'chameleon %s', chameleon_host.hostname)

            if labels != []:
                labels.append('bt_peer')

            if host.multi_chameleon:
                labels_list.append(labels)
            else:
                labels_list.extend(labels)


        logging.info('Bluetooth labels are %s', labels_list)
        return labels_list

    def update_for_task(self, task_name):
        # This label is stored in the lab config, so only deploy tasks update it
        # or when no task name is mentioned.
        return task_name in (DEPLOY_TASK_NAME, '')


class AudioLoopbackDongleLabel(base_label.BaseLabel):
    """Return the label if an audio loopback dongle is plugged in."""

    _NAME = 'audio_loopback_dongle'

    def exists(self, host):
        # Based on crbug.com/991285, AudioLoopbackDongle sometimes flips.
        # Ensure that AudioLoopbackDongle.exists returns True
        # forever, after it returns True *once*.
        if self._cached_exists(host):
            # If the current state is True, return it, don't run the command on
            # the DUT and potentially flip the state.
            return True
        # If the current state is not True, run the command on
        # the DUT. The new state will be set to whatever the command
        # produces.
        return self._host_run_exists(host)

    def _cached_exists(self, host):
        """Get the state of AudioLoopbackDongle in the data store"""
        info = host.host_info_store.get()
        for label in info.labels:
            if label.startswith(self._NAME):
                return True
        return False

    def _host_run_exists(self, host):
        """Detect presence of audio_loopback_dongle by physically
        running a command on the DUT."""
        nodes_info = host.run(command=cras_utils.get_cras_nodes_cmd(),
                              ignore_status=True).stdout
        if (cras_utils.node_type_is_plugged('HEADPHONE', nodes_info) and
            cras_utils.node_type_is_plugged('MIC', nodes_info)):
            return True
        return False

    def update_for_task(self, task_name):
        # This label is stored in the state config, so only repair tasks update
        # it or when no task name is mentioned.
        return task_name in (REPAIR_TASK_NAME, '')


class PowerSupplyLabel(base_label.StringPrefixLabel):
    """
    Return the label describing the power supply type.

    Labels representing this host's power supply.
         * `power:battery` when the device has a battery intended for
                extended use
         * `power:AC_primary` when the device has a battery not intended
                for extended use (for moving the machine, etc)
         * `power:AC_only` when the device has no battery at all.
    """

    _NAME = 'power'

    def __init__(self):
        self.psu_cmd_result = None


    def exists(self, host):
        self.psu_cmd_result = host.run(command='mosys psu type',
                                       ignore_status=True)
        return self.psu_cmd_result.stdout.strip() != 'unknown'


    def generate_labels(self, host):
        if self.psu_cmd_result.exit_status:
            # The psu command for mosys is not included for all platforms. The
            # assumption is that the device will have a battery if the command
            # is not found.
            return ['battery']
        return [self.psu_cmd_result.stdout.strip()]


class StorageLabel(base_label.StringPrefixLabel):
    """
    Return the label describing the storage type.

    Determine if the internal device is SCSI or dw_mmc device.
    Then check that it is SSD or HDD or eMMC or something else.

    Labels representing this host's internal device type:
             * `storage:ssd` when internal device is solid state drive
             * `storage:hdd` when internal device is hard disk drive
             * `storage:mmc` when internal device is mmc drive
             * `storage:nvme` when internal device is NVMe drive
             * `storage:ufs` when internal device is ufs drive
             * None          When internal device is something else or
                             when we are unable to determine the type
    """

    _NAME = 'storage'

    def __init__(self):
        self.type_str = ''


    def exists(self, host):
        # The output should be /dev/mmcblk* for SD/eMMC or /dev/sd* for scsi
        rootdev_cmd = ' '.join(['. /usr/sbin/write_gpt.sh;',
                                '. /usr/share/misc/chromeos-common.sh;',
                                'load_base_vars;',
                                'get_fixed_dst_drive'])
        rootdev = host.run(command=rootdev_cmd, ignore_status=True)
        if rootdev.exit_status:
            logging.info("Fail to run %s", rootdev_cmd)
            return False
        rootdev_str = rootdev.stdout.strip()

        if not rootdev_str:
            return False

        rootdev_base = os.path.basename(rootdev_str)

        mmc_pattern = '/dev/mmcblk[0-9]'
        if re.match(mmc_pattern, rootdev_str):
            # Use type to determine if the internal device is eMMC or somthing
            # else. We can assume that MMC is always an internal device.
            type_cmd = 'cat /sys/block/%s/device/type' % rootdev_base
            type = host.run(command=type_cmd, ignore_status=True)
            if type.exit_status:
                logging.info("Fail to run %s", type_cmd)
                return False
            type_str = type.stdout.strip()

            if type_str == 'MMC':
                self.type_str = 'mmc'
                return True

        scsi_pattern = '/dev/sd[a-z]+'
        if re.match(scsi_pattern, rootdev.stdout):
            # Read symlink for /sys/block/sd* to determine if the internal
            # device is connected via ata or usb.
            link_cmd = 'readlink /sys/block/%s' % rootdev_base
            link = host.run(command=link_cmd, ignore_status=True)
            if link.exit_status:
                logging.info("Fail to run %s", link_cmd)
                return False
            link_str = link.stdout.strip()
            if 'usb' in link_str:
                return False
            elif 'ufs' in link_str:
              self.type_str = 'ufs'
              return True

            # Read rotation to determine if the internal device is ssd or hdd.
            rotate_cmd = str('cat /sys/block/%s/queue/rotational'
                              % rootdev_base)
            rotate = host.run(command=rotate_cmd, ignore_status=True)
            if rotate.exit_status:
                logging.info("Fail to run %s", rotate_cmd)
                return False
            rotate_str = rotate.stdout.strip()

            rotate_dict = {'0':'ssd', '1':'hdd'}
            self.type_str = rotate_dict.get(rotate_str)
            return True

        nvme_pattern = '/dev/nvme[0-9]+n[0-9]+'
        if re.match(nvme_pattern, rootdev_str):
            self.type_str = 'nvme'
            return True

        # All other internal device / error case will always fall here
        return False

    def generate_labels(self, host):
        return [self.type_str]


class ServoLabel(base_label.BaseLabel):
    """
    Label servo is applying if a servo is present.
    Label servo_state present always.
    """

    _NAME_OLD = 'servo'
    _NAME = 'servo_state'
    _NAME_WORKING = 'servo_state:WORKING'
    _NAME_BROKEN = 'servo_state:BROKEN'

    def get(self, host):
        if self.exists(host):
            return [self._NAME_OLD, self._NAME_WORKING]
        return [self._NAME_BROKEN]

    def get_all_labels(self):
        return set([self._NAME]), set([self._NAME_OLD])

    def exists(self, host):
        # Based on crbug.com/995900, Servo sometimes flips.
        # Ensure that ServoLabel.exists returns True
        # forever, after it returns True *once*.
        if self._cached_exists(host):
            # If the current state is True, return it, don't run the command on
            # the DUT and potentially flip the state.
            return True
        # If the current state is not True, run the command on
        # the DUT. The new state will be set to whatever the command
        # produces.
        return self._host_run_exists(host)

    def _cached_exists(self, host):
        """Get the state of Servo in the data store"""
        info = host.host_info_store.get()
        for label in info.labels:
            if label.startswith(self._NAME):
                if label.startswith(self._NAME_WORKING):
                    return True
            elif label.startswith(self._NAME_OLD):
                return True
        return False

    def _host_run_exists(self, host):
        """
        Check if the servo label should apply to the host or not.

        @returns True if a servo host is detected, False otherwise.
        """
        servo_host_hostname = None
        servo_args = servo_host.get_servo_args_for_host(host)
        if servo_args:
            servo_host_hostname = servo_args.get(servo_host.SERVO_HOST_ATTR)
        return (servo_host_hostname is not None
                and servo_host.servo_host_is_up(servo_host_hostname))

    def update_for_task(self, task_name):
        # This label is stored in the state config, so only repair tasks update
        # it or when no task name is mentioned.
        return task_name in (REPAIR_TASK_NAME, '')


class ArcLabel(base_label.BaseLabel):
    """Label indicates if host has ARC support."""

    _NAME = 'arc'

    @base_label.forever_exists_decorate
    def exists(self, host):
        return 0 == host.run(
            'grep CHROMEOS_ARC_VERSION /etc/lsb-release',
            ignore_status=True).exit_status


class CtsArchLabel(base_label.StringLabel):
    """Labels to determine the abi of the CTS bundle (arm or x86 only)."""

    _NAME = ['cts_abi_arm', 'cts_abi_x86', 'cts_cpu_arm', 'cts_cpu_x86']

    def _get_cts_abis(self, arch):
        """Return supported CTS ABIs.

        @return List of supported CTS bundle ABIs.
        """
        cts_abis = {'x86_64': ['arm', 'x86'], 'arm': ['arm']}
        return cts_abis.get(arch, [])

    def _get_cts_cpus(self, arch):
        """Return supported CTS native CPUs.

        This is needed for CTS_Instant scheduling.
        @return List of supported CTS native CPUs.
        """
        cts_cpus = {'x86_64': ['x86'], 'arm': ['arm']}
        return cts_cpus.get(arch, [])

    def generate_labels(self, host):
        cpu_arch = host.get_cpu_arch()
        abi_labels = ['cts_abi_' + abi for abi in self._get_cts_abis(cpu_arch)]
        cpu_labels = ['cts_cpu_' + cpu for cpu in self._get_cts_cpus(cpu_arch)]
        return abi_labels + cpu_labels


class VideoGlitchLabel(base_label.BaseLabel):
    """Label indicates if host supports video glitch detection tests."""

    _NAME = 'video_glitch_detection'

    def exists(self, host):
        board = host.get_board().replace(ds_constants.BOARD_PREFIX, '')

        return board in video_test_constants.SUPPORTED_BOARDS


class InternalDisplayLabel(base_label.StringLabel):
    """Label that determines if the device has an internal display."""

    _NAME = 'internal_display'

    def generate_labels(self, host):
        from autotest_lib.client.cros.graphics import graphics_utils
        from autotest_lib.client.common_lib import utils as common_utils

        def __system_output(cmd):
            return host.run(cmd).stdout

        def __read_file(remote_path):
            return host.run('cat %s' % remote_path).stdout

        # Hijack the necessary client functions so that we can take advantage
        # of the client lib here.
        # FIXME: find a less hacky way than this
        original_system_output = utils.system_output
        original_read_file = common_utils.read_file
        utils.system_output = __system_output
        common_utils.read_file = __read_file
        try:
            return ([self._NAME]
                    if graphics_utils.has_internal_display()
                    else [])
        finally:
            utils.system_output = original_system_output
            common_utils.read_file = original_read_file


class LucidSleepLabel(base_label.BaseLabel):
    """Label that determines if device has support for lucid sleep."""

    # TODO(kevcheng): See if we can determine if this label is applicable a
    # better way (crbug.com/592146).
    _NAME = 'lucidsleep'
    LUCID_SLEEP_BOARDS = ['nocturne', 'poppy']

    def exists(self, host):
        board = host.get_board().replace(ds_constants.BOARD_PREFIX, '')
        return board in self.LUCID_SLEEP_BOARDS


def _parse_hwid_labels(hwid_info_list):
    if len(hwid_info_list) == 0:
        return hwid_info_list

    res = []
    # See crbug.com/997816#c7 for details of two potential formats of returns
    # from HWID server.
    if isinstance(hwid_info_list[0], dict):
        # Format of hwid_info:
        # [{u'name': u'sku', u'value': u'xxx'}, ..., ]
        for hwid_info in hwid_info_list:
            value = hwid_info.get('value', '')
            name = hwid_info.get('name', '')
            # There should always be a name but just in case there is not.
            if name:
                new_label = name if not value else '%s:%s' % (name, value)
                res.append(new_label)
    else:
        # Format of hwid_info:
        # [<DUTLabel name: 'sku' value: u'xxx'>, ..., ]
        for hwid_info in hwid_info_list:
            new_label = str(hwid_info)
            logging.info('processing hwid label: %s', new_label)
            res.append(new_label)

    return res


class HWIDLabel(base_label.StringLabel):
    """Return all the labels generated from the hwid."""

    # We leave out _NAME because hwid_lib will generate everything for us.

    def __init__(self):
        # Grab the key file needed to access the hwid service.
        self.key_file = global_config.global_config.get_config_value(
                'CROS', 'HWID_KEY', type=str)


    @staticmethod
    def _merge_hwid_label_lists(new, old):
        """merge a list of old and new values for hwid_labels.
        preferring new values if available

        @returns: list of labels"""
        # TODO(gregorynisbet): what is the appropriate way to merge
        # old and new information?
        retained = set(x for x in old)
        for label in new:
            key, sep, value = label.partition(':')
            # If we have a key-value key such as variant:aaa,
            # then we remove all the old labels with the same key.
            if sep:
                retained = set(x for x in retained if (not x.startswith(key + ':')))
        return list(sorted(retained.union(new)))


    def _hwid_label_names(self):
        """get the labels that hwid_lib controls.

        @returns: hwid_labels
        """
        all_hwid_labels, _ = self.get_all_labels()
        # If and only if get_all_labels was unsuccessful,
        # it will return a falsey value.
        out = all_hwid_labels or HWID_LABELS_FALLBACK

        # TODO(gregorynisbet): remove this
        # TODO(crbug.com/999785)
        if "sku" not in out:
            logging.info("sku-less label names %s", out)

        return out


    def _old_label_values(self, host):
        """get the hwid_lib labels on previous run

        @returns: hwid_labels"""
        out = []
        info = host.host_info_store.get()
        for hwid_label in self._hwid_label_names():
            for label in info.labels:
                # NOTE: we want *all* the labels starting
                # with this prefix.
                if label.startswith(hwid_label):
                    out.append(label)
        return out


    def generate_labels(self, host):
        # use previous values as default
        old_hwid_labels = self._old_label_values(host)
        logging.info("old_hwid_labels: %r", old_hwid_labels)
        hwid = host.run_output('crossystem hwid').strip()
        hwid_info_list = []
        try:
            hwid_info_response = hwid_lib.get_hwid_info(
                hwid=hwid,
                info_type=hwid_lib.HWID_INFO_LABEL,
                key_file=self.key_file,
            )
            logging.info("hwid_info_response: %r", hwid_info_response)
            hwid_info_list = hwid_info_response.get('labels', [])
        except hwid_lib.HwIdException as e:
            logging.info("HwIdException: %s", e)

        new_hwid_labels = _parse_hwid_labels(hwid_info_list)
        logging.info("new HWID labels: %r", new_hwid_labels)

        return HWIDLabel._merge_hwid_label_lists(
            old=old_hwid_labels,
            new=new_hwid_labels,
        )


    def get_all_labels(self):
        """We need to try all labels as a prefix and as standalone.

        We don't know for sure which labels are prefix labels and which are
        standalone so we try all of them as both.
        """
        all_hwid_labels = []
        try:
            all_hwid_labels = hwid_lib.get_all_possible_dut_labels(
                    self.key_file)
        except IOError:
            logging.error('Can not open key file: %s', self.key_file)
        except hwid_lib.HwIdException as e:
            logging.error('hwid service: %s', e)
        return all_hwid_labels, all_hwid_labels


class DetachableBaseLabel(base_label.BaseLabel):
    """Label indicating if device has detachable keyboard."""

    _NAME = 'detachablebase'

    def exists(self, host):
        return host.run('which hammerd', ignore_status=True).exit_status == 0


class FingerprintLabel(base_label.BaseLabel):
    """Label indicating whether device has fingerprint sensor."""

    _NAME = 'fingerprint'

    def exists(self, host):
        return host.run('test -c /dev/cros_fp',
                        ignore_status=True).exit_status == 0


class ReferenceDesignLabel(base_label.StringPrefixLabel):
    """Determine the correct reference design label for the device. """

    _NAME = 'reference_design'

    def __init__(self):
        self.response = None

    def exists(self, host):
        self.response = host.run('mosys platform family', ignore_status=True)
        return self.response.exit_status == 0

    def generate_labels(self, host):
        if self.exists(host):
            return [self.response.stdout.strip()]


CROS_LABELS = [
    AudioLoopbackDongleLabel(), #STATECONFIG
    ChameleonConnectionLabel(), #LABCONFIG
    ChameleonLabel(), #STATECONFIG
    ChameleonPeripheralsLabel(), #LABCONFIG
    common_label.OSLabel(),
    DeviceSkuLabel(), #LABCONFIG
    HWIDLabel(),
    ServoLabel(), #STATECONFIG
]

LABSTATION_LABELS = [
    common_label.OSLabel(),
]
