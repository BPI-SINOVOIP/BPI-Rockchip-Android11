#!/usr/bin/env python2
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""library functions to prepare a DUT for lab deployment.

This library will be shared between Autotest and Skylab DUT deployment tools.
"""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import contextlib
import time

import common
import logging
from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib import utils
from autotest_lib.server import hosts
from autotest_lib.server import site_utils as server_utils
from autotest_lib.server.hosts import host_info
from autotest_lib.server.hosts import servo_host


_FIRMWARE_UPDATE_TIMEOUT = 600


@contextlib.contextmanager
def create_cros_host(hostname, board, model, servo_hostname, servo_port,
                servo_serial=None, logs_dir=None):
    """Yield a server.hosts.CrosHost object to use for DUT preparation.

    This object contains just enough inventory data to be able to prepare the
    DUT for lab deployment. It does not contain any reference to AFE / Skylab so
    that DUT preparation is guaranteed to be isolated from the scheduling
    infrastructure.

    @param hostname:        FQDN of the host to prepare.
    @param board:           The autotest board label for the DUT.
    @param model:           The autotest model label for the DUT.
    @param servo_hostname:  FQDN of the servo host controlling the DUT.
    @param servo_port:      Servo host port used for the controlling servo.
    @param servo_serial:    (Optional) Serial number of the controlling servo.
    @param logs_dir:        (Optional) Directory to save logs obtained from the
                            host.

    @yield a server.hosts.Host object.
    """
    labels = [
            'board:%s' % board,
            'model:%s' % model,
    ]
    attributes = {
            servo_host.SERVO_HOST_ATTR: servo_hostname,
            servo_host.SERVO_PORT_ATTR: servo_port,
    }
    if servo_serial is not None:
        attributes[servo_host.SERVO_SERIAL_ATTR] = servo_serial

    store = host_info.InMemoryHostInfoStore(info=host_info.HostInfo(
            labels=labels,
            attributes=attributes,
    ))
    machine_dict = _get_machine_dict(hostname, store)
    host = hosts.create_host(machine_dict)
    servohost = servo_host.ServoHost(
            **servo_host.get_servo_args_for_host(host))
    _prepare_servo(servohost)
    host.set_servo_host(servohost)
    host.servo.uart_logs_dir = logs_dir
    try:
        yield host
    finally:
        host.close()


@contextlib.contextmanager
def create_labstation_host(hostname, board, model):
    """Yield a server.hosts.LabstationHost object to use for labstation
    preparation.

    This object contains just enough inventory data to be able to prepare the
    labstation for lab deployment. It does not contain any reference to
    AFE / Skylab so that DUT preparation is guaranteed to be isolated from
    the scheduling infrastructure.

    @param hostname:        FQDN of the host to prepare.
    @param board:           The autotest board label for the DUT.
    @param model:           The autotest model label for the DUT.

    @yield a server.hosts.Host object.
    """
    labels = [
        'board:%s' % board,
        'model:%s' % model,
        'os:labstation'
        ]

    store = host_info.InMemoryHostInfoStore(info=host_info.HostInfo(
        labels=labels,
    ))
    machine_dict = _get_machine_dict(hostname, store)
    host = hosts.create_host(machine_dict)
    try:
        yield host
    finally:
        host.close()


def _get_machine_dict(hostname, host_info_store):
    """Helper function to generate a machine_dic to feed hosts.create_host.

    @param hostname
    @param host_info_store

    @return A dict that hosts.create_host can consume.
    """
    return {'hostname': hostname,
            'host_info_store': host_info_store,
            'afe_host': server_utils.EmptyAFEHost(),
            }


def download_image_to_servo_usb(host, build):
    """Download the given image to the USB attached to host's servo.

    @param host   A server.hosts.Host object.
    @param build  A Chrome OS version string for the build to download.
    """
    _, update_url = host.stage_image_for_servo(build)
    host.servo.image_to_servo_usb(update_url)


def power_cycle_via_servo(host):
    """Power cycle a host though it's attached servo.

    @param host   A server.hosts.Host object.
    """
    logging.info("Shutting down the host...")
    host.halt()

    logging.info('Power cycling DUT through servo...')
    host.servo.get_power_state_controller().power_off()
    host.servo.switch_usbkey('off')
    time.sleep(host.SHUTDOWN_TIMEOUT)
    # N.B. The Servo API requires that we use power_on() here
    # for two reasons:
    #  1) After turning on a DUT in recovery mode, you must turn
    #     it off and then on with power_on() once more to
    #     disable recovery mode (this is a Parrot specific
    #     requirement).
    #  2) After power_off(), the only way to turn on is with
    #     power_on() (this is a Storm specific requirement).
    time.sleep(host.SHUTDOWN_TIMEOUT)
    host.servo.get_power_state_controller().power_on()

    logging.info('Waiting for DUT to come back up.')
    if not host.wait_up(timeout=host.BOOT_TIMEOUT):
        raise error.AutoservError('DUT failed to come back after %d seconds' %
                                  host.BOOT_TIMEOUT)


def verify_boot_into_rec_mode(host):
    """Verify that we can boot into USB when in recover mode, and reset tpm.

    The new deploy process will install test image before firmware update, so
    we don't need boot into recovery mode during deploy, but we still want to
    make sure that DUT can boot into recover mode as it's critical for
    auto-repair capability.

    @param host   servers.host.Host object.
    """
    logging.info("Shutting down DUT...")
    host.halt()
    host.servo.get_power_state_controller().power_off()
    time.sleep(host.SHUTDOWN_TIMEOUT)
    logging.info("Booting DUT into recovery mode...")
    host.servo.boot_in_recovery_mode()

    if not host.wait_up(timeout=host.USB_BOOT_TIMEOUT):
        raise Exception('DUT failed to boot into recovery mode.')

    logging.info('Resetting the TPM status')
    try:
        host.run('chromeos-tpm-recovery')
    except error.AutoservRunError:
        logging.warn('chromeos-tpm-recovery is too old.')

    logging.info("Rebooting host into normal mode.")
    power_cycle_via_servo(host)
    logging.info("Verify boot into recovery mode completed successfully.")


def install_test_image(host):
    """Initial install a test image on a DUT.

    This function assumes that the required image is already downloaded onto the
    USB key connected to the DUT via servo, and the DUT is in dev mode with
    dev_boot_usb enabled.

    @param host   servers.host.Host object.
    """
    servo = host.servo
    # First power on.  We sleep to allow the firmware plenty of time
    # to display the dev-mode screen; some boards take their time to
    # be ready for the ctrl+U after power on.
    servo.get_power_state_controller().power_off()
    time.sleep(host.SHUTDOWN_TIMEOUT)
    servo.switch_usbkey('dut')
    servo.get_power_state_controller().power_on()

    # Dev mode screen should be up now:  type ctrl+U and wait for
    # boot from USB to finish.
    time.sleep(10)
    servo.ctrl_u()

    if not host.wait_up(timeout=host.USB_BOOT_TIMEOUT):
        raise Exception('DUT failed to boot from USB for install test image.')

    host.run('chromeos-install --yes', timeout=host.INSTALL_TIMEOUT)

    logging.info("Rebooting DUT to boot from hard drive.")
    power_cycle_via_servo(host)
    logging.info("Install test image completed successfully.")


def reinstall_test_image(host):
    """Install the test image of given build to DUT.

    This function assumes that the required image is already downloaded onto the
    USB key connected to the DUT via servo.

    @param host   servers.host.Host object.
    """
    host.servo_install()


def flash_firmware_using_servo(host, build):
    """Flash DUT firmware directly using servo.

    Rather than running `chromeos-firmwareupdate` on DUT, we can flash DUT
    firmware directly using servo (run command `flashrom`, etc. on servo). In
    this way, we don't require DUT to be in dev mode and with dev_boot_usb
    enabled."""
    host.firmware_install(build)


def install_firmware(host):
    """Install dev-signed firmware after removing write-protect.

    At start, it's assumed that hardware write-protect is disabled,
    the DUT is in dev mode, and the servo's USB stick already has a
    test image installed.

    The firmware is installed by powering on and typing ctrl+U on
    the keyboard in order to boot the test image from USB.  Once
    the DUT is booted, we run a series of commands to install the
    read-only firmware from the test image.  Then we clear debug
    mode, and shut down.

    @param host   Host instance to use for servo and ssh operations.
    """
    # Disable software-controlled write-protect for both FPROMs, and
    # install the RO firmware.
    for fprom in ['host', 'ec']:
        host.run('flashrom -p %s --wp-disable' % fprom,
                 ignore_status=True)

    fw_update_log = '/mnt/stateful_partition/home/root/cros-fw-update.log'
    pid = _start_firmware_update(host, fw_update_log)
    _wait_firmware_update_process(host, pid)
    _check_firmware_update_result(host, fw_update_log)

    # Get us out of dev-mode and clear GBB flags.  GBB flags are
    # non-zero because boot from USB was enabled.
    logging.info("Resting gbb flags and disable dev mode.")
    host.run('/usr/share/vboot/bin/set_gbb_flags.sh 0',
             ignore_status=True)
    host.run('crossystem disable_dev_request=1',
             ignore_status=True)

    logging.info("Rebooting DUT in normal mode(non-dev).")
    power_cycle_via_servo(host)
    logging.info("Install firmware completed successfully.")



def _start_firmware_update(host, result_file):
    """Run `chromeos-firmwareupdate` in background.

    In scenario servo v4 type C, some boards of DUT may lose ethernet
    connectivity on firmware update. There's no way to bring it back except
    rebooting the system.

    @param host         Host instance to use for servo and ssh operations.
    @param result_file  Path on DUT to save operation logs.

    @returns The process id."""
    # TODO(guocb): Use `make_dev_firmware` to re-sign from MP to test/dev.
    fw_update_cmd = 'chromeos-firmwareupdate --mode=factory --force'

    cmd = [
        "date > %s" % result_file,
        "nohup %s &>> %s" % (fw_update_cmd, result_file),
        "/usr/local/bin/hooks/check_ethernet.hook"
    ]
    return host.run_background(';'.join(cmd))


def _wait_firmware_update_process(host, pid, timeout=_FIRMWARE_UPDATE_TIMEOUT):
    """Wait `chromeos-firmwareupdate` to finish.

    @param host     Host instance to use for servo and ssh operations.
    @param pid      The process ID of `chromeos-firmwareupdate`.
    @param timeout  Maximum time to wait for firmware updating.
    """
    try:
        utils.poll_for_condition(
            lambda: host.run('ps -f -p %s' % pid, timeout=20).exit_status,
            exception=Exception(
                    "chromeos-firmwareupdate (pid: %s) didn't complete in %s "
                    'seconds.' % (pid, timeout)),
            timeout=_FIRMWARE_UPDATE_TIMEOUT,
            sleep_interval=10,
        )
    except error.AutoservRunError:
        # We lose the connectivity, so the DUT should be booting up.
        if not host.wait_up(timeout=host.USB_BOOT_TIMEOUT):
            raise Exception(
                    'DUT failed to boot up after firmware updating.')


def _check_firmware_update_result(host, result_file):
    """Check if firmware updating is good or not.

    @param host         Host instance to use for servo and ssh operations.
    @param result_file  Path of the file saving output of
                        `chromeos-firmwareupdate`.
    """
    fw_update_was_good = ">> DONE: Firmware updater exits successfully."
    result = host.run('cat %s' % result_file)
    if result.stdout.rstrip().rsplit('\n', 1)[1] != fw_update_was_good:
        raise Exception("chromeos-firmwareupdate failed!")


def _prepare_servo(servohost):
    """Prepare servo connected to host for installation steps.

    @param servohost  A server.hosts.servo_host.ServoHost object.
    """
    # Stopping `servod` on the servo host will force `repair()` to
    # restart it.  We want that restart for a few reasons:
    #   + `servod` caches knowledge about the image on the USB stick.
    #     We want to clear the cache to force the USB stick to be
    #     re-imaged unconditionally.
    #   + If there's a problem with servod that verify and repair
    #     can't find, this provides a UI through which `servod` can
    #     be restarted.
    servohost.run('stop servod PORT=%d' % servohost.servo_port,
                  ignore_status=True)
    servohost.repair()

    # Don't timeout probing for the host usb device, there could be a bunch
    # of servos probing at the same time on the same servo host.  And
    # since we can't pass None through the xml rpcs, use 0 to indicate None.
    if not servohost.get_servo().probe_host_usb_dev(timeout=0):
        raise Exception('No USB stick detected on Servo host')


def setup_labstation(host):
    """Do initial setup for labstation host.

    @param host    A LabstationHost object.

    """
    try:
        if not host.is_labstation():
            raise Exception('Current OS on host %s is not a labstation image.'
                            % host.hostname)
    except AttributeError:
        raise Exception('Unable to verify host has a labstation image, this can'
                        ' be caused by host is unsshable.')

    try:
        # TODO: we should setup hwid and serial number for DUT in deploy script
        #  as well, which is currently obtained from repair job.
        info = host.host_info_store.get()
        hwid = host.run('crossystem hwid', ignore_status=True).stdout
        if hwid:
            info.attributes['HWID'] = hwid

        serial_number = host.run('vpd -g serial_number',
                                 ignore_status=True).stdout
        if serial_number:
            info.attributes['serial_number'] = serial_number
        if info != host.host_info_store.get():
            host.host_info_store.commit(info)
    except Exception as e:
        raise Exception('Failed to get HWID & Serial Number for host %s: %s'
                        % (host.hostname, str(e)))
