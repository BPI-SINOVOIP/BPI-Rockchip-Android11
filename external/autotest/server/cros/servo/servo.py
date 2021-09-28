# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Expects to be run in an environment with sudo and no interactive password
# prompt, such as within the Chromium OS development chroot.

import ast
import logging
import os
import re
import socket
import time
import xmlrpclib

from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib import lsbrelease_utils
from autotest_lib.server import utils as server_utils
from autotest_lib.server.cros.servo import firmware_programmer
from autotest_lib.server.cros.faft.utils.config import Config as FAFTConfig

# Time to wait when probing for a usb device, it takes on avg 17 seconds
# to do a full probe.
_USB_PROBE_TIMEOUT = 40


# Regex to match XMLRPC errors due to a servod control not existing.
NO_CONTROL_RE = re.compile(r'No control named (\w*\.?\w*)')


# The minimum voltage on the charger port on servo v4 that is expected. This is
# to query whether a charger is plugged into servo v4 and thus pd control
# capabilities can be used.
V4_CHG_ATTACHED_MIN_VOLTAGE_MV = 4400

class ControlUnavailableError(error.TestFail):
    """Custom error class to indicate a control is unavailable on servod."""
    pass


def _extract_image_from_tarball(tarball, dest_dir, image_candidates):
    """Try extracting the image_candidates from the tarball.

    @param tarball: The path of the tarball.
    @param dest_path: The path of the destination.
    @param image_candidates: A tuple of the paths of image candidates.

    @return: The first path from the image candidates, which succeeds, or None
             if all the image candidates fail.
    """

    # Create the firmware_name subdirectory if it doesn't exist
    if not os.path.exists(dest_dir):
        os.mkdir(dest_dir)

    # Generate a list of all tarball files
    tarball_files = server_utils.system_output(
        ('tar tf %s' % tarball), timeout=120, ignore_status=True).splitlines()

    # Check if image candidates are in the list of tarball files
    for image in image_candidates:
        if image in tarball_files:
            # Extract and return the first image candidate found
            status = server_utils.system(
                ('tar xf %s -C %s %s' % (tarball, dest_dir, image)),
                timeout=120, ignore_status=True)
            if status == 0:
                return image
    return None


class _PowerStateController(object):

    """Class to provide board-specific power operations.

    This class is responsible for "power on" and "power off"
    operations that can operate without making assumptions in
    advance about board state.  It offers an interface that
    abstracts out the different sequences required for different
    board types.

    """
    # Constants acceptable to be passed for the `rec_mode` parameter
    # to power_on().
    #
    # REC_ON:  Boot the DUT in recovery mode, i.e. boot from USB or
    #   SD card.
    # REC_OFF:  Boot in normal mode, i.e. boot from internal storage.

    REC_ON = 'rec'
    REC_OFF = 'on'
    REC_ON_FORCE_MRC = 'rec_force_mrc'

    # Delay in seconds needed between asserting and de-asserting
    # warm reset.
    _RESET_HOLD_TIME = 0.5


    def __init__(self, servo):
        """Initialize the power state control.

        @param servo Servo object providing the underlying `set` and `get`
                     methods for the target controls.

        """
        self._servo = servo
        self.supported = self._servo.has_control('power_state')
        if not self.supported:
            logging.info('Servo setup does not support power-state operations. '
                         'All power-state calls will lead to error.TestFail')

    def _check_supported(self):
        """Throw an error if dts mode control is not supported."""
        if not self.supported:
            raise error.TestFail('power_state controls not supported')

    def reset(self):
        """Force the DUT to reset.

        The DUT is guaranteed to be on at the end of this call,
        regardless of its previous state, provided that there is
        working OS software. This also guarantees that the EC has
        been restarted.

        """
        self._check_supported()
        self._servo.set_nocheck('power_state', 'reset')

    def warm_reset(self):
        """Apply warm reset to the DUT.

        This asserts, then de-asserts the 'warm_reset' signal.
        Generally, this causes the board to restart.

        """
        # TODO: warm_reset support has added to power_state.py. Once it
        # available to labstation remove fallback method.
        self._check_supported()
        try:
            self._servo.set_nocheck('power_state', 'warm_reset')
        except error.TestFail as err:
            logging.info("Fallback to warm_reset control method")
            self._servo.set_get_all(['warm_reset:on',
                                 'sleep:%.4f' % self._RESET_HOLD_TIME,
                                 'warm_reset:off'])
    def power_off(self):
        """Force the DUT to power off.

        The DUT is guaranteed to be off at the end of this call,
        regardless of its previous state, provided that there is
        working EC and boot firmware.  There is no requirement for
        working OS software.

        """
        self._check_supported()
        self._servo.set_nocheck('power_state', 'off')

    def power_on(self, rec_mode=REC_OFF):
        """Force the DUT to power on.

        Prior to calling this function, the DUT must be powered off,
        e.g. with a call to `power_off()`.

        At power on, recovery mode is set as specified by the
        corresponding argument.  When booting with recovery mode on, it
        is the caller's responsibility to unplug/plug in a bootable
        external storage device.

        If the DUT requires a delay after powering on but before
        processing inputs such as USB stick insertion, the delay is
        handled by this method; the caller is not responsible for such
        delays.

        @param rec_mode Setting of recovery mode to be applied at
                        power on. default: REC_OFF aka 'off'

        """
        self._check_supported()
        self._servo.set_nocheck('power_state', rec_mode)


class _Uart(object):
    """Class to capture UART streams of CPU, EC, Cr50, etc."""
    _UartToCapture = ('cpu', 'ec', 'cr50', 'servo_v4', 'servo_micro', 'usbpd')

    def __init__(self, servo):
        self._servo = servo
        self._streams = []
        self.logs_dir = None

    def _start_stop_capture(self, uart, start):
        """Helper function to start/stop capturing on specified UART.

        @param uart:  The UART name to start/stop capturing.
        @param start:  True to start capturing, otherwise stop.

        @returns True if the operation completes successfully.
                 False if the UART capturing is not supported or failed due to
                 an error.
        """
        logging.debug('%s capturing %s UART.', 'Start' if start else 'Stop',
                      uart)
        uart_cmd = '%s_uart_capture' % uart
        target_level = 'on' if start else 'off'
        level = None
        if self._servo.has_control(uart_cmd):
            # Do our own implementation of set() here as not_applicable
            # should also count as a valid control.
            logging.debug('Trying to set %s to %s.', uart_cmd, target_level)
            try:
                self._servo.set_nocheck(uart_cmd, target_level)
                level = self._servo.get(uart_cmd)
            except error.TestFail as e:
                # Any sort of test failure here should not stop the test. This
                # is just to capture more output. Log and move on.
                logging.warning('Failed to set %s to %s. %s. Ignoring.',
                                uart_cmd, target_level, str(e))
            if level == target_level:
              logging.debug('Managed to set %s to %s.', uart_cmd, level)
            else:
              logging.debug('Failed to set %s to %s. Got %s.', uart_cmd,
                            target_level, level)
        return level == target_level

    def start_capture(self):
        """Start capturing UART streams."""
        for uart in self._UartToCapture:
            if self._start_stop_capture(uart, True):
                self._streams.append(('%s_uart_stream' % uart, '%s_uart.log' %
                                      uart))

    def dump(self):
        """Dump UART streams to log files accordingly."""
        if not self.logs_dir:
            return

        for stream, logfile in self._streams:
            logfile_fullname = os.path.join(self.logs_dir, logfile)
            try:
                content = self._servo.get(stream)
            except Exception as err:
                logging.warn('Failed to get UART log for %s: %s', stream, err)
                continue

            if content == 'not_applicable':
                logging.warn('%s is not applicable', stream)
                continue

            # The UART stream may contain non-printable characters, and servo
            # returns it in string representation. We use `ast.leteral_eval`
            # to revert it back.
            with open(logfile_fullname, 'a') as fd:
                try:
                    fd.write(ast.literal_eval(content))
                except ValueError:
                    logging.exception('Invalid value for %s: %r', stream,
                                      content)

    def stop_capture(self):
        """Stop capturing UART streams."""
        for uart in self._UartToCapture:
            try:
                self._start_stop_capture(uart, False)
            except Exception as err:
                logging.warn('Failed to stop UART logging for %s: %s', uart,
                             err)


class Servo(object):

    """Manages control of a Servo board.

    Servo is a board developed by hardware group to aide in the debug and
    control of various partner devices. Servo's features include the simulation
    of pressing the power button, closing the lid, and pressing Ctrl-d. This
    class manages setting up and communicating with a servo demon (servod)
    process. It provides both high-level functions for common servo tasks and
    low-level functions for directly setting and reading gpios.

    """

    # Power button press delays in seconds.
    #
    # The EC specification says that 8.0 seconds should be enough
    # for the long power press.  However, some platforms need a bit
    # more time.  Empirical testing has found these requirements:
    #   Alex: 8.2 seconds
    #   ZGB:  8.5 seconds
    # The actual value is set to the largest known necessary value.
    #
    # TODO(jrbarnette) Being generous is the right thing to do for
    # existing platforms, but if this code is to be used for
    # qualification of new hardware, we should be less generous.
    SHORT_DELAY = 0.1

    # Maximum number of times to re-read power button on release.
    GET_RETRY_MAX = 10

    # Delays to deal with DUT state transitions.
    SLEEP_DELAY = 6
    BOOT_DELAY = 10

    # Default minimum time interval between 'press' and 'release'
    # keyboard events.
    SERVO_KEY_PRESS_DELAY = 0.1

    # Time to toggle recovery switch on and off.
    REC_TOGGLE_DELAY = 0.1

    # Time to toggle development switch on and off.
    DEV_TOGGLE_DELAY = 0.1

    # Time between an usb disk plugged-in and detected in the system.
    USB_DETECTION_DELAY = 10
    # Time to keep USB power off before and after USB mux direction is changed
    USB_POWEROFF_DELAY = 2

    # Time to wait before timing out on servo initialization.
    INIT_TIMEOUT_SECS = 10


    def __init__(self, servo_host, servo_serial=None):
        """Sets up the servo communication infrastructure.

        @param servo_host: A ServoHost object representing
                           the host running servod.
        @type servo_host: autotest_lib.server.hosts.servo_host.ServoHost
        @param servo_serial: Serial number of the servo board.
        """
        # TODO(fdeng): crbug.com/298379
        # We should move servo_host object out of servo object
        # to minimize the dependencies on the rest of Autotest.
        self._servo_host = servo_host
        self._servo_serial = servo_serial
        self._server = servo_host.get_servod_server_proxy()
        self._servo_type = self.get_servo_version()
        self._power_state = _PowerStateController(self)
        self._uart = _Uart(self)
        self._usb_state = None
        self._programmer = None
        self._prev_log_inode = None
        self._prev_log_size = 0

    @property
    def servo_serial(self):
        """Returns the serial number of the servo board."""
        return self._servo_serial

    def rotate_servod_logs(self, filename=None, directory=None):
        """Save the latest servod log into a local directory, then rotate logs.

        The files will be <filename>.DEBUG, <filename>.INFO, <filename>.WARNING,
        or just <filename>.log if not using split level logging.

        @param filename: local filename prefix (no file extension) to use.
                         If None, rotate log but don't save it.
        @param directory: local directory to save logs into (if unset, use cwd)
        """
        if self.is_localhost():
            # Local servod usually runs without log-dir, so can't be collected.
            # TODO(crbug.com/1011516): allow localhost when rotation is enabled
            return

        log_dir = '/var/log/servod_%s' % self._servo_host.servo_port

        if filename:
            logging.info("Saving servod logs: %s/%s.*", directory or '.',
                         filename)
            # TODO(crrev.com/c/1793030): remove no-level case once CL is pushed
            for level_name in ('', 'DEBUG', 'INFO', 'WARNING'):

                remote_path = os.path.join(log_dir, 'latest')
                if level_name:
                    remote_path += '.%s' % level_name

                local_path = '%s.%s' % (filename, level_name or 'log')
                if directory:
                    local_path = os.path.join(directory, local_path)

                try:
                    self._servo_host.get_file(
                            remote_path, local_path, try_rsync=False)

                except error.AutoservRunError as e:
                    result = e.result_obj
                    if result.exit_status != 0:
                        stderr = result.stderr.strip()

                        # File not existing is okay, but warn for anything else.
                        if 'no such' not in stderr.lower():
                            logging.warn(
                                    "Couldn't retrieve servod log: %s",
                                    stderr or '\n%s' % result)

                try:
                    if os.stat(local_path).st_size == 0:
                        os.unlink(local_path)
                except EnvironmentError:
                    pass

        else:
            # No filename given, so caller wants to discard the log lines.
            # Remove the symlinks to prevent old log-dir links from being
            # picked up multiple times when using servod without log-dir.
            remote_path = os.path.join(log_dir, 'latest*')
            self._servo_host.run(
                    "rm %s" % remote_path,
                    stderr_tee=None, ignore_status=True)

        # Servod log rotation renames current log, then creates a new file with
        # the old name: log.<date> -> log.<date>.1.tbz2 -> log.<date>.2.tbz2

        # Must rotate after copying, or the copy would be the new, empty file.
        try:
            self.set_nocheck('rotate_servod_logs', 'yes')
        except ControlUnavailableError as e:
            # Missing control (possibly old servod)
            logging.warn("Couldn't rotate servod logs: %s", str(e))
        except error.TestFail:
            # Control exists but gave an error; don't let it fail the test.
            # The error is already logged in set_nocheck().
            pass

    def get_power_state_controller(self):
        """Return the power state controller for this Servo.

        The power state controller provides board-independent
        interfaces for reset, power-on, power-off operations.

        """
        return self._power_state


    def initialize_dut(self, cold_reset=False, enable_main=True):
        """Initializes a dut for testing purposes.

        This sets various servo signals back to default values
        appropriate for the target board.  By default, if the DUT
        is already on, it stays on.  If the DUT is powered off
        before initialization, its state afterward is unspecified.

        Rationale:  Basic initialization of servo sets the lid open,
        when there is a lid.  This operation won't affect powered on
        units; however, setting the lid open may power on a unit
        that's off, depending on the board type and previous state
        of the device.

        If `cold_reset` is a true value, the DUT and its EC will be
        reset, and the DUT rebooted in normal mode.

        @param cold_reset If True, cold reset the device after
                          initialization.
        @param enable_main If True, make sure the main servo device has
                           control of the dut.

        """
        if enable_main:
            self.enable_main_servo_device()

        try:
            self._server.hwinit()
        except socket.error as e:
            e.filename = '%s:%s' % (self._servo_host.hostname,
                                    self._servo_host.servo_port)
            raise
        self._usb_state = None
        if self.has_control('usb_mux_oe1'):
            self.set('usb_mux_oe1', 'on')
            self.switch_usbkey('off')
        else:
            logging.warning('Servod command \'usb_mux_oe1\' is not available. '
                            'Any USB drive related servo routines will fail.')
        self._uart.start_capture()
        if cold_reset:
            if not self._power_state.supported:
                logging.info('Cold-reset for DUT requested, but servo '
                             'setup does not support power_state. Skipping.')
            else:
                self._power_state.reset()
        logging.debug('Servo initialized, version is %s',
                      self._server.get_version())
        if self.has_control('init_keyboard'):
            # This indicates the servod version does not
            # have explicit keyboard initialization yet.
            # Ignore this.
            # TODO(coconutruben): change this back to set() about a month
            # after crrev.com/c/1586239 has been merged (or whenever that
            # logic is in the labstation images).
            self.set_nocheck('init_keyboard','on')


    def is_localhost(self):
        """Is the servod hosted locally?

        Returns:
          True if local hosted; otherwise, False.
        """
        return self._servo_host.is_localhost()


    def get_os_version(self):
        """Returns the chromeos release version."""
        lsb_release_content = self.system_output('cat /etc/lsb-release',
                                                 ignore_status=True)
        return lsbrelease_utils.get_chromeos_release_builder_path(
                    lsb_release_content=lsb_release_content)


    def get_servod_version(self):
        """Returns the servod version."""
        result = self._servo_host.run('servod --version')
        # TODO: use system_output once servod --version prints to stdout
        stdout = result.stdout.strip()
        return stdout if stdout else result.stderr.strip()


    def power_long_press(self):
        """Simulate a long power button press."""
        # After a long power press, the EC may ignore the next power
        # button press (at least on Alex).  To guarantee that this
        # won't happen, we need to allow the EC one second to
        # collect itself.
        # long_press is defined as 8.5s in servod
        self.set_nocheck('power_key', 'long_press')


    def power_normal_press(self):
        """Simulate a normal power button press."""
        # press is defined as 1.2s in servod
        self.set_nocheck('power_key', 'press')


    def power_short_press(self):
        """Simulate a short power button press."""
        # tab is defined as 0.2s in servod
        self.set_nocheck('power_key', 'tab')


    def power_key(self, press_secs='tab'):
        """Simulate a power button press.

        @param press_secs: int, float, str; time to press key in seconds or
                           known shorthand: 'tab' 'press' 'long_press'
        """
        self.set_nocheck('power_key', press_secs)


    def pwr_button(self, action='press'):
        """Simulate a power button press.

        @param action: str; could be press or could be release.
        """
        self.set_nocheck('pwr_button', action)


    def lid_open(self):
        """Simulate opening the lid and raise exception if all attempts fail"""
        self.set('lid_open', 'yes')


    def lid_close(self):
        """Simulate closing the lid and raise exception if all attempts fail

        Waits 6 seconds to ensure the device is fully asleep before returning.
        """
        self.set('lid_open', 'no')
        time.sleep(Servo.SLEEP_DELAY)


    def vbus_power_get(self):
        """Get current vbus_power."""
        return self.get('vbus_power')


    def volume_up(self, timeout=300):
        """Simulate pushing the volume down button.

        @param timeout: Timeout for setting the volume.
        """
        self.set_get_all(['volume_up:yes',
                          'sleep:%.4f' % self.SERVO_KEY_PRESS_DELAY,
                          'volume_up:no'])
        # we need to wait for commands to take effect before moving on
        time_left = float(timeout)
        while time_left > 0.0:
            value = self.get('volume_up')
            if value == 'no':
                return
            time.sleep(self.SHORT_DELAY)
            time_left = time_left - self.SHORT_DELAY
        raise error.TestFail("Failed setting volume_up to no")

    def volume_down(self, timeout=300):
        """Simulate pushing the volume down button.

        @param timeout: Timeout for setting the volume.
        """
        self.set_get_all(['volume_down:yes',
                          'sleep:%.4f' % self.SERVO_KEY_PRESS_DELAY,
                          'volume_down:no'])
        # we need to wait for commands to take effect before moving on
        time_left = float(timeout)
        while time_left > 0.0:
            value = self.get('volume_down')
            if value == 'no':
                return
            time.sleep(self.SHORT_DELAY)
            time_left = time_left - self.SHORT_DELAY
        raise error.TestFail("Failed setting volume_down to no")

    def ctrl_d(self, press_secs='tab'):
        """Simulate Ctrl-d simultaneous button presses.

        @param press_secs: int, float, str; time to press key in seconds or
                           known shorthand: 'tab' 'press' 'long_press'
        """
        self.set_nocheck('ctrl_d', press_secs)


    def ctrl_u(self, press_secs='tab'):
        """Simulate Ctrl-u simultaneous button presses.

        @param press_secs: int, float, str; time to press key in seconds or
                           known shorthand: 'tab' 'press' 'long_press'
        """
        self.set_nocheck('ctrl_u', press_secs)


    def ctrl_enter(self, press_secs='tab'):
        """Simulate Ctrl-enter simultaneous button presses.

        @param press_secs: int, float, str; time to press key in seconds or
                           known shorthand: 'tab' 'press' 'long_press'
        """
        self.set_nocheck('ctrl_enter', press_secs)


    def ctrl_key(self, press_secs='tab'):
        """Simulate Enter key button press.

        @param press_secs: int, float, str; time to press key in seconds or
                           known shorthand: 'tab' 'press' 'long_press'
        """
        self.set_nocheck('ctrl_key', press_secs)


    def enter_key(self, press_secs='tab'):
        """Simulate Enter key button press.

        @param press_secs: int, float, str; time to press key in seconds or
                           known shorthand: 'tab' 'press' 'long_press'
        """
        self.set_nocheck('enter_key', press_secs)


    def refresh_key(self, press_secs='tab'):
        """Simulate Refresh key (F3) button press.

        @param press_secs: int, float, str; time to press key in seconds or
                           known shorthand: 'tab' 'press' 'long_press'
        """
        self.set_nocheck('refresh_key', press_secs)


    def ctrl_refresh_key(self, press_secs='tab'):
        """Simulate Ctrl and Refresh (F3) simultaneous press.

        This key combination is an alternative of Space key.

        @param press_secs: int, float, str; time to press key in seconds or
                           known shorthand: 'tab' 'press' 'long_press'
        """
        self.set_nocheck('ctrl_refresh_key', press_secs)


    def imaginary_key(self, press_secs='tab'):
        """Simulate imaginary key button press.

        Maps to a key that doesn't physically exist.

        @param press_secs: int, float, str; time to press key in seconds or
                           known shorthand: 'tab' 'press' 'long_press'
        """
        self.set_nocheck('imaginary_key', press_secs)


    def sysrq_x(self, press_secs='tab'):
        """Simulate Alt VolumeUp X simulataneous press.

        This key combination is the kernel system request (sysrq) X.

        @param press_secs: int, float, str; time to press key in seconds or
                           known shorthand: 'tab' 'press' 'long_press'
        """
        self.set_nocheck('sysrq_x', press_secs)


    def toggle_recovery_switch(self):
        """Toggle recovery switch on and off."""
        self.enable_recovery_mode()
        time.sleep(self.REC_TOGGLE_DELAY)
        self.disable_recovery_mode()


    def enable_recovery_mode(self):
        """Enable recovery mode on device."""
        self.set('rec_mode', 'on')


    def disable_recovery_mode(self):
        """Disable recovery mode on device."""
        self.set('rec_mode', 'off')


    def toggle_development_switch(self):
        """Toggle development switch on and off."""
        self.enable_development_mode()
        time.sleep(self.DEV_TOGGLE_DELAY)
        self.disable_development_mode()


    def enable_development_mode(self):
        """Enable development mode on device."""
        self.set('dev_mode', 'on')


    def disable_development_mode(self):
        """Disable development mode on device."""
        self.set('dev_mode', 'off')

    def boot_devmode(self):
        """Boot a dev-mode device that is powered off."""
        self.power_short_press()
        self.pass_devmode()


    def pass_devmode(self):
        """Pass through boot screens in dev-mode."""
        time.sleep(Servo.BOOT_DELAY)
        self.ctrl_d()
        time.sleep(Servo.BOOT_DELAY)


    def get_board(self):
        """Get the board connected to servod."""
        return self._server.get_board()


    def get_base_board(self):
        """Get the board of the base connected to servod."""
        try:
            return self._server.get_base_board()
        except  xmlrpclib.Fault as e:
            # TODO(waihong): Remove the following compatibility check when
            # the new versions of hdctools are deployed.
            if 'not supported' in str(e):
                logging.warning('The servod is too old that get_base_board '
                        'not supported.')
                return ''
            raise


    def get_ec_active_copy(self):
        """Get the active copy of the EC image."""
        return self.get('ec_active_copy')


    def _get_xmlrpclib_exception(self, xmlexc):
        """Get meaningful exception string from xmlrpc.

        Args:
            xmlexc: xmlrpclib.Fault object

        xmlrpclib.Fault.faultString has the following format:

        <type 'exception type'>:'actual error message'

        Parse and return the real exception from the servod side instead of the
        less meaningful one like,
           <Fault 1: "<type 'exceptions.AttributeError'>:'tca6416' object has no
           attribute 'hw_driver'">

        Returns:
            string of underlying exception raised in servod.
        """
        return re.sub('^.*>:', '', xmlexc.faultString)

    def has_control(self, ctrl_name, prefix=''):
        """Query servod server to determine if |ctrl_name| is a valid control.

        @param ctrl_name Name of the control.
        @param prefix: prefix to route control to correct servo device.

        @returns: true if |ctrl_name| is a known control, false otherwise.
        """
        cltr_name = self._build_ctrl_name(ctrl_name, prefix)
        try:
            # If the control exists, doc() will work.
            self._server.doc(ctrl_name)
            return True
        except xmlrpclib.Fault as e:
            if re.search('No control %s' % ctrl_name,
                         self._get_xmlrpclib_exception(e)):
                return False
            raise e

    def _build_ctrl_name(self, ctrl_name, prefix):
        """Helper to build the control name if a prefix is used.

        @param ctrl_name Name of the control.
        @param prefix: prefix to route control to correct servo device.

        @returns: [|prefix|.]ctrl_name depending on whether prefix is non-empty.
        """
        assert ctrl_name
        if prefix:
            return '%s.%s' % (prefix, ctrl_name)
        return ctrl_name

    def get(self, ctrl_name, prefix=''):
        """Get the value of a gpio from Servod.

        @param ctrl_name Name of the control.
        @param prefix: prefix to route control to correct servo device.

        @returns: server response to |ctrl_name| request.

        @raise ControlUnavailableError: if |ctrl_name| not a known control.
        @raise error.TestFail: for all other failures doing get().
        """
        cltr_name = self._build_ctrl_name(ctrl_name, prefix)
        try:
            return self._server.get(ctrl_name)
        except  xmlrpclib.Fault as e:
            err_str = self._get_xmlrpclib_exception(e)
            err_msg = "Getting '%s' :: %s" % (ctrl_name, err_str)
            unknown_ctrl = re.findall(NO_CONTROL_RE, err_str)
            if unknown_ctrl:
                raise ControlUnavailableError('No control named %r' %
                                              unknown_ctrl[0])
            else:
                logging.error(err_msg)
                raise error.TestFail(err_msg)


    def set(self, ctrl_name, ctrl_value, prefix=''):
        """Set and check the value of a gpio using Servod.

        @param ctrl_name: Name of the control.
        @param ctrl_value: New setting for the control.
        @param prefix: prefix to route control to correct servo device.
        @raise error.TestFail: if the control value fails to change.
        """
        cltr_name = self._build_ctrl_name(ctrl_name, prefix)
        self.set_nocheck(ctrl_name, ctrl_value)
        retry_count = Servo.GET_RETRY_MAX
        actual_value = self.get(ctrl_name)
        while ctrl_value != actual_value and retry_count:
            logging.warning("%s != %s, retry %d", ctrl_name, ctrl_value,
                            retry_count)
            retry_count -= 1
            time.sleep(Servo.SHORT_DELAY)
            actual_value = self.get(ctrl_name)

        if ctrl_value != actual_value:
            raise error.TestFail(
                    'Servo failed to set %s to %s. Got %s.'
                    % (ctrl_name, ctrl_value, actual_value))


    def set_nocheck(self, ctrl_name, ctrl_value, prefix=''):
        """Set the value of a gpio using Servod.

        @param ctrl_name Name of the control.
        @param ctrl_value New setting for the control.
        @param prefix: prefix to route control to correct servo device.

        @raise ControlUnavailableError: if |ctrl_name| not a known control.
        @raise error.TestFail: for all other failures doing set().
        """
        cltr_name = self._build_ctrl_name(ctrl_name, prefix)
        # The real danger here is to pass a None value through the xmlrpc.
        assert ctrl_value is not None
        logging.debug('Setting %s to %r', ctrl_name, ctrl_value)
        try:
            self._server.set(ctrl_name, ctrl_value)
        except  xmlrpclib.Fault as e:
            err_str = self._get_xmlrpclib_exception(e)
            err_msg = "Setting '%s' :: %s" % (ctrl_name, err_str)
            unknown_ctrl = re.findall(NO_CONTROL_RE, err_str)
            if unknown_ctrl:
                raise ControlUnavailableError('No control named %r' %
                                              unknown_ctrl[0])
            else:
                logging.error(err_msg)
                raise error.TestFail(err_msg)


    def set_get_all(self, controls):
        """Set &| get one or more control values.

        @param controls: list of strings, controls to set &| get.

        @raise: error.TestError in case error occurs setting/getting values.
        """
        rv = []
        try:
            logging.debug('Set/get all: %s', str(controls))
            rv = self._server.set_get_all(controls)
        except xmlrpclib.Fault as e:
            # TODO(waihong): Remove the following backward compatibility when
            # the new versions of hdctools are deployed.
            if 'not supported' in str(e):
                logging.warning('The servod is too old that set_get_all '
                        'not supported. Use set and get instead.')
                for control in controls:
                    if ':' in control:
                        (name, value) = control.split(':')
                        if name == 'sleep':
                            time.sleep(float(value))
                        else:
                            self.set_nocheck(name, value)
                        rv.append(True)
                    else:
                        rv.append(self.get(name))
            else:
                err_msg = "Problem with '%s' :: %s" % \
                    (controls, self._get_xmlrpclib_exception(e))
                raise error.TestFail(err_msg)
        return rv


    # TODO(waihong) It may fail if multiple servo's are connected to the same
    # host. Should look for a better way, like the USB serial name, to identify
    # the USB device.
    # TODO(sbasi) Remove this code from autoserv once firmware tests have been
    # updated.
    def probe_host_usb_dev(self, timeout=_USB_PROBE_TIMEOUT):
        """Probe the USB disk device plugged-in the servo from the host side.

        It uses servod to discover if there is a usb device attached to it.

        @param timeout The timeout period when probing for the usb host device.

        @return: String of USB disk path (e.g. '/dev/sdb') or None.
        """
        # Set up Servo's usb mux.
        self.switch_usbkey('host')
        return self._server.probe_host_usb_dev(timeout) or None


    def image_to_servo_usb(self, image_path=None,
                           make_image_noninteractive=False):
        """Install an image to the USB key plugged into the servo.

        This method may copy any image to the servo USB key including a
        recovery image or a test image.  These images are frequently used
        for test purposes such as restoring a corrupted image or conducting
        an upgrade of ec/fw/kernel as part of a test of a specific image part.

        @param image_path Path on the host to the recovery image.
        @param make_image_noninteractive Make the recovery image
                                   noninteractive, therefore the DUT
                                   will reboot automatically after
                                   installation.
        """
        # We're about to start plugging/unplugging the USB key.  We
        # don't know the state of the DUT, or what it might choose
        # to do to the device after hotplug.  To avoid surprises,
        # force the DUT to be off.
        self._server.hwinit()
        if self.has_control('init_keyboard'):
            # This indicates the servod version does not
            # have explicit keyboard initialization yet.
            # Ignore this.
            # TODO(coconutruben): change this back to set() about a month
            # after crrev.com/c/1586239 has been merged (or whenever that
            # logic is in the labstation images).
            self.set_nocheck('init_keyboard','on')
        self._power_state.power_off()

        if image_path:
            # Set up Servo's usb mux.
            self.switch_usbkey('host')
            logging.info('Searching for usb device and copying image to it. '
                         'Please wait a few minutes...')
            if not self._server.download_image_to_usb(image_path):
                logging.error('Failed to transfer requested image to USB. '
                              'Please take a look at Servo Logs.')
                raise error.AutotestError('Download image to usb failed.')
            if make_image_noninteractive:
                logging.info('Making image noninteractive')
                if not self._server.make_image_noninteractive():
                    logging.error('Failed to make image noninteractive. '
                                  'Please take a look at Servo Logs.')

    def boot_in_recovery_mode(self):
        """Boot host DUT in recovery mode."""
        self._power_state.power_on(rec_mode=self._power_state.REC_ON)
        self.switch_usbkey('dut')


    def install_recovery_image(self, image_path=None,
                               make_image_noninteractive=False):
        """Install the recovery image specified by the path onto the DUT.

        This method uses google recovery mode to install a recovery image
        onto a DUT through the use of a USB stick that is mounted on a servo
        board specified by the usb_dev.  If no image path is specified
        we use the recovery image already on the usb image.

        @param image_path: Path on the host to the recovery image.
        @param make_image_noninteractive: Make the recovery image
                noninteractive, therefore the DUT will reboot automatically
                after installation.
        """
        self.image_to_servo_usb(image_path, make_image_noninteractive)
        # Give the DUT some time to power_off if we skip
        # download image to usb. (crbug.com/982993)
        if not image_path:
            time.sleep(10)
        self.boot_in_recovery_mode()


    def _scp_image(self, image_path):
        """Copy image to the servo host.

        When programming a firmware image on the DUT, the image must be
        located on the host to which the servo device is connected. Sometimes
        servo is controlled by a remote host, in this case the image needs to
        be transferred to the remote host. This adds the servod port number, to
        make sure tests for different DUTs don't trample on each other's files.

        @param image_path: a string, name of the firmware image file to be
               transferred.
        @return: a string, full path name of the copied file on the remote.
        """
        name = os.path.basename(image_path)
        remote_name = 'dut_%s.%s' % (self._servo_host.servo_port, name)
        dest_path = os.path.join('/tmp', remote_name)
        logging.info('Copying %s to %s', name, dest_path)
        self._servo_host.send_file(image_path, dest_path)
        return dest_path


    def system(self, command, timeout=3600):
        """Execute the passed in command on the servod host.

        @param command Command to be executed.
        @param timeout Maximum number of seconds of runtime allowed. Default to
                       1 hour.
        """
        logging.info('Will execute on servo host: %s', command)
        self._servo_host.run(command, timeout=timeout)


    def system_output(self, command, timeout=3600,
                      ignore_status=False, args=()):
        """Execute the passed in command on the servod host, return stdout.

        @param command a string, the command to execute
        @param timeout an int, max number of seconds to wait til command
               execution completes. Default to 1 hour.
        @param ignore_status a Boolean, if true - ignore command's nonzero exit
               status, otherwise an exception will be thrown
        @param args a tuple of strings, each becoming a separate command line
               parameter for the command
        @return command's stdout as a string.
        """
        return self._servo_host.run(command, timeout=timeout,
                                    ignore_status=ignore_status,
                                    args=args).stdout.strip()


    def get_servo_version(self, active=False):
        """Get the version of the servo, e.g., servo_v2 or servo_v3.

        @param active: Only return the servo type with the active device.
        @return: The version of the servo.

        """
        servo_type = self._server.get_version()
        if '_and_' not in servo_type or not active:
            return servo_type

        # If servo v4 is using ccd and servo micro, modify the servo type to
        # reflect the active device.
        active_device = self.get('active_v4_device')
        if active_device in servo_type:
            logging.info('%s is active', active_device)
            return 'servo_v4_with_' + active_device

        logging.warn("%s is active even though it's not in servo type",
                     active_device)
        return servo_type


    def get_main_servo_device(self):
        """Return the main servo device"""
        return self._servo_type.split('_with_')[-1].split('_and_')[0]


    def enable_main_servo_device(self):
        """Make sure the main device has control of the dut."""
        # Cr50 detects servo using the EC uart. It doesn't work well if the
        # board doesn't use EC uart. The lab active_v4_device doesn't handle
        # this correctly. Check ec_uart_pty before trying to change the active
        # device.
        # TODO(crbug.com/1016842): reenable the setting the main device when
        # active device works on labstations.
        return
        if not self.has_control('active_v4_device'):
            return
        self.set('active_v4_device', self.get_main_servo_device())


    def main_device_is_ccd(self):
        """Whether the main servo device (no prefixes) is a ccd device."""
        servo = self._server.get_version()
        return 'ccd_cr50' in servo and 'servo_micro' not in servo


    def main_device_is_flex(self):
        """Whether the main servo device (no prefixes) is a legacy device."""
        return not self.main_device_is_ccd()


    def main_device_is_active(self):
        """Return whether the main device is the active device.

        This is only relevant for a dual setup with ccd and legacy on the same
        DUT. The main device is the servo that has no prefix on its controls.
        This helper answers the question whether that device is also the
        active device or not.
        """
        # TODO(coconutruben): The current implementation of the dual setup only
        # ever has legacy as the main device. Therefore, it suffices to ask
        # whether the active device is ccd.
        if not self.dts_mode_is_valid():
            # Use dts support as a proxy to whether the servo setup could
            # support a dual role. Only those setups now support legacy and ccd.
            return True
        active_device = self.get('active_v4_device')
        return 'ccd_cr50' not in active_device

    def _initialize_programmer(self, rw_only=False):
        """Initialize the firmware programmer.

        @param rw_only: True to initialize a programmer which only
                        programs the RW portions.
        """
        if self._programmer:
            return
        # Initialize firmware programmer
        if self._servo_type.startswith('servo_v2'):
            self._programmer = firmware_programmer.ProgrammerV2(self)
            self._programmer_rw = firmware_programmer.ProgrammerV2RwOnly(self)
        # Both servo v3 and v4 use the same programming methods so just leverage
        # ProgrammerV3 for servo v4 as well.
        elif (self._servo_type.startswith('servo_v3') or
              self._servo_type.startswith('servo_v4')):
            self._programmer = firmware_programmer.ProgrammerV3(self)
            self._programmer_rw = firmware_programmer.ProgrammerV3RwOnly(self)
        else:
            raise error.TestError(
                    'No firmware programmer for servo version: %s' %
                    self._servo_type)


    def program_bios(self, image, rw_only=False):
        """Program bios on DUT with given image.

        @param image: a string, file name of the BIOS image to program
                      on the DUT.
        @param rw_only: True to only program the RW portion of BIOS.

        """
        self._initialize_programmer()
        if not self.is_localhost():
            image = self._scp_image(image)
        if rw_only:
            self._programmer_rw.program_bios(image)
        else:
            self._programmer.program_bios(image)


    def program_ec(self, image, rw_only=False):
        """Program ec on DUT with given image.

        @param image: a string, file name of the EC image to program
                      on the DUT.
        @param rw_only: True to only program the RW portion of EC.

        """
        self._initialize_programmer()
        if not self.is_localhost():
            image = self._scp_image(image)
        if rw_only:
            self._programmer_rw.program_ec(image)
        else:
            self._programmer.program_ec(image)


    def extract_ec_image(self, board, model, tarball_path):
        """Helper function to extract EC image from downloaded tarball.

        @param board: The DUT board name.
        @param model: The DUT model name.
        @param tarball_path: The path of the downloaded build tarball.

        @return: Path to extracted EC image.
        """

        # Ignore extracting EC image and re-programming if not a Chrome EC
        chrome_ec = FAFTConfig(board).chrome_ec
        if not chrome_ec:
            logging.info('Not a Chrome EC, ignore re-programming it')
            return None

        # Best effort; try to retrieve the EC board from the version as
        # reported by the EC.
        ec_board = None
        try:
            ec_board = self.get('ec_board')
        except Exception as err:
            logging.info('Failed to get ec_board value; ignoring')
            pass

        # Array of candidates for EC image
        ec_image_candidates = ['ec.bin',
                               '%s/ec.bin' % model,
                               '%s/ec.bin' % board]
        if ec_board:
          ec_image_candidates.append('%s/ec.bin' % ec_board)

        # Extract EC image from tarball
        dest_dir = os.path.join(os.path.dirname(tarball_path), 'EC')
        ec_image = _extract_image_from_tarball(tarball_path, dest_dir,
                                               ec_image_candidates)

        # Check if EC image was found and return path or raise error
        if ec_image:
            return os.path.join(dest_dir, ec_image)
        else:
            raise error.TestError('Failed to extract EC image from %s',
                                  tarball_path)


    def extract_bios_image(self, board, model, tarball_path):
        """Helper function to extract BIOS image from downloaded tarball.

        @param board: The DUT board name.
        @param model: The DUT model name.
        @param tarball_path: The path of the downloaded build tarball.

        @return: Path to extracted BIOS image.
        """

        # Array of candidates for BIOS image
        bios_image_candidates = ['image.bin',
                                 'image-%s.bin' % model,
                                 'image-%s.bin' % board]

        # Extract BIOS image from tarball
        dest_dir = os.path.join(os.path.dirname(tarball_path), 'BIOS')
        bios_image = _extract_image_from_tarball(tarball_path, dest_dir,
                                                 bios_image_candidates)

        # Check if BIOS image was found and return path or raise error
        if bios_image:
            return os.path.join(dest_dir, bios_image)
        else:
            raise error.TestError('Failed to extract BIOS image from %s',
                                  tarball_path)


    def _switch_usbkey_power(self, power_state, detection_delay=False):
        """Switch usbkey power.

        This function switches usbkey power by setting the value of
        'prtctl4_pwren'. If the power is already in the
        requested state, this function simply returns.

        @param power_state: A string, 'on' or 'off'.
        @param detection_delay: A boolean value, if True, sleep
                                for |USB_DETECTION_DELAY| after switching
                                the power on.
        """
        # TODO(kevcheng): Forgive me for this terrible hack. This is just to
        # handle beaglebones that haven't yet updated and have the
        # safe_switch_usbkey_power RPC.  I'll remove this once all beaglebones
        # have been updated and also think about a better way to handle
        # situations like this.
        try:
            self._server.safe_switch_usbkey_power(power_state)
        except Exception:
            self.set('prtctl4_pwren', power_state)
        if power_state == 'off':
            time.sleep(self.USB_POWEROFF_DELAY)
        elif detection_delay:
            time.sleep(self.USB_DETECTION_DELAY)


    def switch_usbkey(self, usb_state):
        """Connect USB flash stick to either host or DUT, or turn USB port off.

        This function switches the servo multiplexer to provide electrical
        connection between the USB port J3 and either host or DUT side. It
        can also be used to turn the USB port off.

        Switching to 'dut' or 'host' is accompanied by powercycling
        of the USB stick, because it sometimes gets wedged if the mux
        is switched while the stick power is on.

        @param usb_state: A string, one of 'dut', 'host', or 'off'.
                          'dut' and 'host' indicate which side the
                          USB flash device is required to be connected to.
                          'off' indicates turning the USB port off.

        @raise: error.TestError in case the parameter is not 'dut'
                'host', or 'off'.
        """
        if self.get_usbkey_direction() == usb_state:
            return

        if usb_state == 'off':
            self._switch_usbkey_power('off')
            self._usb_state = usb_state
            return
        elif usb_state == 'host':
            mux_direction = 'servo_sees_usbkey'
        elif usb_state == 'dut':
            mux_direction = 'dut_sees_usbkey'
        else:
            raise error.TestError('Unknown USB state request: %s' % usb_state)

        self._switch_usbkey_power('off')
        # TODO(kevcheng): Forgive me for this terrible hack. This is just to
        # handle beaglebones that haven't yet updated and have the
        # safe_switch_usbkey RPC.  I'll remove this once all beaglebones have
        # been updated and also think about a better way to handle situations
        # like this.
        try:
            self._server.safe_switch_usbkey(mux_direction)
        except Exception:
            self.set('usb_mux_sel1', mux_direction)
        time.sleep(self.USB_POWEROFF_DELAY)
        self._switch_usbkey_power('on', usb_state == 'host')
        self._usb_state = usb_state


    def get_usbkey_direction(self):
        """Get which side USB is connected to or 'off' if usb power is off.

        @return: A string, one of 'dut', 'host', or 'off'.
        """
        if not self._usb_state:
            if self.get('prtctl4_pwren') == 'off':
                self._usb_state = 'off'
            elif self.get('usb_mux_sel1').startswith('dut'):
                self._usb_state = 'dut'
            else:
                self._usb_state = 'host'
        return self._usb_state


    def set_servo_v4_role(self, role):
        """Set the power role of servo v4, either 'src' or 'snk'.

        It does nothing if not a servo v4.

        @param role: Power role for DUT port on servo v4, either 'src' or 'snk'.
        """
        if self._servo_type.startswith('servo_v4'):
            value = self.get('servo_v4_role')
            if value != role:
                self.set_nocheck('servo_v4_role', role)
            else:
                logging.debug('Already in the role: %s.', role)
        else:
            logging.debug('Not a servo v4, unable to set role to %s.', role)


    def supports_built_in_pd_control(self):
        """Return whether the servo type supports pd charging and control."""
        if 'servo_v4' not in self._servo_type:
            # Only servo v4 supports this feature.
            logging.info('%r type does not support pd control.',
                         self._servo_type)
            return False
        # On servo v4, it still needs to be the type-c version.
        if not self.get('servo_v4_type') == 'type-c':
            logging.info('PD controls require a type-c servo v4.')
            return False
        # Lastly, one cannot really do anything without a plugged in charger.
        chg_port_mv = self.get('ppchg5_mv')
        if chg_port_mv < V4_CHG_ATTACHED_MIN_VOLTAGE_MV:
            logging.warn('It appears that no charger is plugged into servo v4. '
                         'Charger port voltage: %dmV', chg_port_mv)
            return False
        logging.info('Charger port voltage: %dmV', chg_port_mv)
        return True

    def dts_mode_is_valid(self):
        """Return whether servo setup supports dts mode control for cr50."""
        if 'servo_v4' not in self._servo_type:
            # Only servo v4 supports this feature.
            logging.debug('%r type does not support dts mode control.',
                          self._servo_type)
            return False
        # On servo v4, it still needs ot be the type-c version.
        if not 'type-c' == self.get('servo_v4_type'):
            logging.info('DTS controls require a type-c servo v4.')
            return False
        return True

    def dts_mode_is_safe(self):
        """Return whether servo setup supports dts mode without losing access.

        DTS mode control exists but the main device might go through ccd.
        In that case, it's only safe to control dts mode if the main device
        is legacy as otherwise the connection to the main device cuts out.
        """
        return self.dts_mode_is_valid() and self.main_device_is_flex()

    def get_dts_mode(self):
        """Return servo dts mode.

        @returns: on/off whether dts is on or off
        """
        if not self.dts_mode_is_valid():
            logging.info('Not a valid servo setup. Unable to get dts mode.')
            return
        return self.get('servo_v4_dts_mode')

    def set_dts_mode(self, state):
        """Set servo dts mode to off or on.

        It does nothing if not a servo v4. Disable the ccd watchdog if we're
        disabling dts mode. CCD will disconnect. The watchdog only allows CCD
        to disconnect for 10 seconds until it kills servod. Disable the
        watchdog, so CCD can stay disconnected indefinitely.

        @param state: Set servo v4 dts mode 'off' or 'on'.
        """
        if not self.dts_mode_is_valid():
            logging.info('Not a valid servo setup. Unable to set dts mode %s.',
                         state)
            return

        # TODO(mruthven): remove watchdog check once the labstation has been
        # updated to have support for modifying the watchdog.
        set_watchdog = (self.has_control('watchdog') and
                        'ccd' in self._servo_type)
        enable_watchdog = state == 'on'

        if set_watchdog and not enable_watchdog:
            self.set_nocheck('watchdog_remove', 'ccd')

        self.set_nocheck('servo_v4_dts_mode', state)

        if set_watchdog and enable_watchdog:
            self.set_nocheck('watchdog_add', 'ccd')


    def _get_servo_type_fw_version(self, servo_type, prefix=''):
        """Helper to handle fw retrieval for micro/v4 vs ccd.

        @param servo_type: one of 'servo_v4', 'servo_micro', 'ccd_cr50'
        @param prefix: whether the control has a prefix

        @returns: fw version for non-ccd devices, cr50 version for ccd device
        """
        if servo_type == 'ccd_cr50':
            # ccd_cr50 runs on cr50, so need to query the cr50 fw.
            servo_type = 'cr50'
        cmd = '%s_version' % servo_type
        try:
            return self.get(cmd, prefix=prefix)
        except error.TestFail:
            # Do not fail here, simply report the version as unknown.
            logging.warn('Unable to query %r to get servo fw version.', cmd)
            return 'unknown'


    def get_servo_fw_versions(self):
        """Retrieve a summary of attached servos and their firmware.

        Note: that only the Google firmware owned servos supports this e.g.
        micro, v4, etc. For anything else, the dictionary will have no entry.
        If no device is has Google owned firmware (e.g. v3) then the result
        is an empty dictionary.

        @returns: dict, a collection of each attached servo & their firmware.
        """
        def get_fw_version_tag(tag, dev):
            return '%s_version.%s' % (dev, tag)

        fw_versions = {}
        if 'servo_v4' not in self._servo_type:
            return {}
        v4_tag = get_fw_version_tag('support', 'servo_v4')
        fw_versions[v4_tag] = self._get_servo_type_fw_version('servo_v4')
        if 'with' in self._servo_type:
            dut_devs = self._servo_type.split('_with_')[1].split('_and_')
            main_tag = get_fw_version_tag('main', dut_devs[0])
            fw_versions[main_tag] = self._get_servo_type_fw_version(dut_devs[0])
            if len(dut_devs) == 2:
                # Right now, the only way for this to happen is for a dual setup
                # to exist where ccd is attached on top of servo micro. Thus, we
                # know that the prefix is ccd_cr50 and the type is ccd_cr50.
                # TODO(coconutruben): If the new servod is not deployed by
                # the time that there are more cases of '_and_' devices,
                # this needs to be reworked.
                dual_tag = get_fw_version_tag('ccd_flex_secondary', dut_devs[1])
                fw = self._get_servo_type_fw_version(dut_devs[1], 'ccd_cr50')
                fw_versions[dual_tag] = fw
        return fw_versions

    @property
    def uart_logs_dir(self):
        """Return the directory to save UART logs."""
        return self._uart.logs_dir if self._uart else ""


    @uart_logs_dir.setter
    def uart_logs_dir(self, logs_dir):
        """Set directory to save UART logs.

        @param logs_dir  String of directory name."""
        if self._uart:
            self._uart.logs_dir = logs_dir


    def close(self):
        """Close the servo object."""
        if self._uart:
            self._uart.stop_capture()
            self._uart.dump()
            self._uart = None
