# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import functools
import logging
import pprint
import re
import time

from autotest_lib.client.bin import utils
from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib.cros import cr50_utils
from autotest_lib.server.cros.servo import chrome_ec


def dts_control_command(func):
    """For methods that should only run when dts mode control is supported."""
    @functools.wraps(func)
    def wrapper(instance, *args, **kwargs):
        """Ignore those functions if dts mode control is not supported."""
        if instance._servo.dts_mode_is_valid():
            return func(instance, *args, **kwargs)
        logging.info('Servo setup does not support DTS mode. ignoring %s',
                     func.func_name)
    return wrapper


class ChromeCr50(chrome_ec.ChromeConsole):
    """Manages control of a Chrome Cr50.

    We control the Chrome Cr50 via the console of a Servo board. Chrome Cr50
    provides many interfaces to set and get its behavior via console commands.
    This class is to abstract these interfaces.
    """
    PROD_RW_KEYIDS = ['0x87b73b67', '0xde88588d']
    PROD_RO_KEYIDS = ['0xaa66150f']
    OPEN = 'open'
    UNLOCK = 'unlock'
    LOCK = 'lock'
    # The amount of time you need to show physical presence.
    PP_SHORT = 15
    PP_LONG = 300
    CCD_PASSWORD_RATE_LIMIT = 3
    IDLE_COUNT = 'count: (\d+)\s'
    SHORT_WAIT = 3
    # The version has four groups: the partition, the header version, debug
    # descriptor and then version string.
    # There are two partitions A and B. The active partition is marked with a
    # '*'. If it is a debug image '/DBG' is added to the version string. If the
    # image has been corrupted, the version information will be replaced with
    # 'Error'.
    # So the output may look something like this.
    #   RW_A:    0.0.21/cr50_v1.1.6133-fd788b
    #   RW_B:  * 0.0.22/DBG/cr50_v1.1.6138-b9f0b1d
    # Or like this if the region was corrupted.
    #   RW_A:  * 0.0.21/cr50_v1.1.6133-fd788b
    #   RW_B:    Error
    VERSION_FORMAT = '\nRW_(A|B): +%s +(\d+\.\d+\.\d+|Error)(/DBG)?(\S+)?\s'
    INACTIVE_VERSION = VERSION_FORMAT % ''
    ACTIVE_VERSION = VERSION_FORMAT % '\*'
    # Following lines of the version output may print the image board id
    # information. eg.
    # BID A:   5a5a4146:ffffffff:00007f00 Yes
    # BID B:   00000000:00000000:00000000 Yes
    # Use the first group from ACTIVE_VERSION to match the active board id
    # partition.
    BID_ERROR = 'read_board_id: failed'
    BID_FORMAT = ':\s+[a-f0-9:]+ '
    ACTIVE_BID = r'%s.*(\1%s|%s.*>)' % (ACTIVE_VERSION, BID_FORMAT,
            BID_ERROR)
    WAKE_CHAR = '\n\n'
    WAKE_RESPONSE = ['(>|Console is enabled)']
    START_UNLOCK_TIMEOUT = 20
    GETTIME = ['= (\S+)']
    FWMP_LOCKED_PROD = ["Managed device console can't be unlocked"]
    FWMP_LOCKED_DBG = ['Ignoring FWMP unlock setting']
    MAX_RETRY_COUNT = 5
    CCDSTATE_MAX_RETRY_COUNT = 20
    START_STR = ['(.*Console is enabled;)']
    REBOOT_DELAY_WITH_CCD = 60
    REBOOT_DELAY_WITH_FLEX = 3
    ON_STRINGS = ['enable', 'enabled', 'on']
    CONSERVATIVE_CCD_WAIT = 10
    CCD_SHORT_PRESSES = 5
    CAP_IS_ACCESSIBLE = 0
    CAP_SETTING = 1
    CAP_REQ = 2
    GET_CAP_TRIES = 5
    # Regex to match the valid capability settings.
    CAP_STATES = '(Always|Default|IfOpened|UnlessLocked)'
    # List of all cr50 ccd capabilities. Same order of 'ccd' output
    CAP_NAMES = [
        'UartGscRxAPTx', 'UartGscTxAPRx', 'UartGscRxECTx', 'UartGscTxECRx',
        'FlashAP', 'FlashEC', 'OverrideWP', 'RebootECAP', 'GscFullConsole',
        'UnlockNoReboot', 'UnlockNoShortPP', 'OpenNoTPMWipe', 'OpenNoLongPP',
        'BatteryBypassPP', 'UpdateNoTPMWipe', 'I2C', 'FlashRead',
        'OpenNoDevMode', 'OpenFromUSB'
    ]
    # There are two capability formats. Match both.
    #  UartGscRxECTx   Y 3=IfOpened
    #  or
    #  UartGscRxECTx   Y 0=Default (Always)
    # Make sure the last word is at the end of the line. The next line will
    # start with some whitespace, so account for that too.
    CAP_FORMAT = '\s+(Y|-) \d\=%s( \(%s\))?[\r\n]+\s*' % (CAP_STATES,
                                                          CAP_STATES)
    # Name each group, so we can use groupdict to extract all useful information
    # from the ccd outupt.
    CCD_FORMAT = [
        '(State: (?P<State>Opened|Locked|Unlocked))',
        '(Password: (?P<Password>set|none))',
        '(Flags: (?P<Flags>\S*))',
        '(Capabilities:.*(?P<Capabilities>%s))' %
                (CAP_FORMAT.join(CAP_NAMES) + CAP_FORMAT),
        '(TPM:(?P<TPM>[ \S]*)\r)',
    ]

    # CR50 Board Properties as defined in platform/ec/board/cr50/scratch-reg1.h
    BOARD_PROP = {
           'BOARD_SLAVE_CONFIG_SPI'      : 1 << 0,
           'BOARD_SLAVE_CONFIG_I2C'      : 1 << 1,
           'BOARD_NEEDS_SYS_RST_PULL_UP' : 1 << 5,
           'BOARD_USE_PLT_RESET'         : 1 << 6,
           'BOARD_WP_ASSERTED'           : 1 << 8,
           'BOARD_FORCING_WP'            : 1 << 9,
           'BOARD_NO_RO_UART'            : 1 << 10,
           'BOARD_CCD_STATE_MASK'        : 3 << 11,
           'BOARD_DEEP_SLEEP_DISABLED'   : 1 << 13,
           'BOARD_DETECT_AP_WITH_UART'   : 1 << 14,
           'BOARD_ITE_EC_SYNC_NEEDED'    : 1 << 15,
           'BOARD_WP_DISABLE_DELAY'      : 1 << 16,
           'BOARD_CLOSED_SOURCE_SET1'    : 1 << 17,
           'BOARD_CLOSED_LOOP_RESET'     : 1 << 18,
           'BOARD_NO_INA_SUPPORT'        : 1 << 19,
           'BOARD_ALLOW_CHANGE_TPM_MODE' : 1 << 20,
    }

    # CR50 reset flags as defined in platform ec_commands.h. These are only the
    # flags used by cr50.
    RESET_FLAGS = {
           'RESET_FLAG_OTHER'            : 1 << 0,
           'RESET_FLAG_BROWNOUT'         : 1 << 2,
           'RESET_FLAG_POWER_ON'         : 1 << 3,
           'RESET_FLAG_SOFT'             : 1 << 5,
           'RESET_FLAG_HIBERNATE'        : 1 << 6,
           'RESET_FLAG_RTC_ALARM'        : 3 << 7,
           'RESET_FLAG_WAKE_PIN'         : 1 << 8,
           'RESET_FLAG_HARD'             : 1 << 11,
           'RESET_FLAG_USB_RESUME'       : 1 << 14,
           'RESET_FLAG_RDD'              : 1 << 15,
           'RESET_FLAG_RBOX'             : 1 << 16,
           'RESET_FLAG_SECURITY'         : 1 << 17,
    }

    def __init__(self, servo, faft_config):
        """Initializes a ChromeCr50 object.

        @param servo: A servo object.
        @param faft_config: A faft config object.
        """
        super(ChromeCr50, self).__init__(servo, 'cr50_uart')
        self.faft_config = faft_config


    def wake_cr50(self):
        """Wake up cr50 by sending some linebreaks and wait for the response"""
        logging.debug(super(ChromeCr50, self).send_command_get_output(
                self.WAKE_CHAR, self.WAKE_RESPONSE))


    def send_command(self, commands):
        """Send command through UART.

        Cr50 will drop characters input to the UART when it resumes from sleep.
        If servo is not using ccd, send some dummy characters before sending the
        real command to make sure cr50 is awake.

        @param commands: the command string to send to cr50
        """
        if self._servo.main_device_is_flex():
            self.wake_cr50()
        super(ChromeCr50, self).send_command(commands)


    def set_cap(self, cap, setting):
        """Set the capability to setting

        @param cap: The capability string
        @param setting: The setting to set the capability to.
        """
        self.set_caps({ cap : setting })


    def set_caps(self, cap_dict):
        """Use cap_dict to set all the cap values

        Set all of the capabilities in cap_dict to the correct config.

        @param cap_dict: A dictionary with the capability as key and the desired
                         setting as values
        """
        for cap, config in cap_dict.iteritems():
            self.send_command('ccd set %s %s' % (cap, config))
        current_cap_settings = self.get_cap_dict(info=self.CAP_SETTING)
        for cap, config in cap_dict.iteritems():
            if (current_cap_settings[cap].lower() !=
                config.lower()):
                raise error.TestFail('Failed to set %s to %s' % (cap, config))


    def get_cap_overview(self, cap_dict):
        """Get a basic overview of the capability dictionary

        If all capabilities are set to Default, ccd has been reset to default.
        If all capabilities are set to Always, ccd is in factory mode.

        @param cap_dict: A dictionary of the capability settings
        @return: A tuple of the capability overview (in factory mode, is reset)
        """
        in_factory_mode = True
        is_reset = True
        for cap, cap_info in cap_dict.iteritems():
            cap_setting = cap_info[self.CAP_SETTING]
            if cap_setting != 'Always':
                in_factory_mode = False
            if cap_setting != 'Default':
                is_reset = False
        return in_factory_mode, is_reset


    def password_is_reset(self):
        """Returns True if the password is cleared"""
        return self.get_ccd_info()['Password'] == 'none'


    def ccd_is_reset(self):
        """Returns True if the ccd is reset

        The password must be cleared, write protect and battery presence must
        follow battery presence, and all capabilities must be Always
        """
        return (self.password_is_reset() and self.wp_is_reset() and
                self.batt_pres_is_reset() and
                self.get_cap_overview(self.get_cap_dict())[1])


    def wp_is_reset(self):
        """Returns True if wp is reset to follow batt pres at all times"""
        follow_batt_pres, _, follow_batt_pres_atboot, _ = self.get_wp_state()
        return follow_batt_pres and follow_batt_pres_atboot


    def get_wp_state(self):
        """Get the current write protect and atboot state

        The atboot setting cannot really be determined now if it is set to
        follow battery presence. It is likely to remain the same after reboot,
        but who knows. If the third element of the tuple is True, the last
        element will not be that useful

        @return: a tuple with the current write protect state
                (True if current state is to follow batt presence,
                 True if write protect is enabled,
                 True if current state is to follow batt presence atboot,
                 True if write protect is enabled atboot)
        """
        rv = self.send_command_retry_get_output('wp',
                ['Flash WP: (forced )?(enabled|disabled).*at boot: (forced )?'
                 '(follow|enabled|disabled)'], safe=True)[0]
        _, forced, enabled, _, atboot = rv
        logging.debug(rv)
        return (not forced, enabled =='enabled',
                atboot == 'follow', atboot == 'enabled')


    def in_dev_mode(self):
        """Return True if cr50 thinks the device is in dev mode"""
        return 'dev_mode' in self.get_ccd_info()['TPM']


    def get_ccd_info(self):
        """Get the current ccd state.

        Take the output of 'ccd' and convert it to a dictionary.

        @return: A dictionary with the ccd state name as the key and setting as
                 value.
        """
        matched_output = None
        original_timeout = float(self._servo.get('cr50_uart_timeout'))
        # Change the console timeout to 10s, it may take longer than 3s to read
        # ccd info
        self._servo.set_nocheck('cr50_uart_timeout', self.CONSERVATIVE_CCD_WAIT)
        for i in range(self.GET_CAP_TRIES):
          try:
            # If some ccd output is dropped and the output doesn't match the
            # expected ccd output format, send_command_get_output will wait the
            # full CONSERVATIVE_CCD_WAIT even though ccd is done printing. Use
            # re to search the command output instead of
            # send_safe_command_get_output, so we don't have to wait the full
            # timeout if output is dropped.
            rv = self.send_command_retry_get_output('ccd', ['ccd.*>'],
                    safe=True)[0]
            matched_output = re.search('.*'.join(self.CCD_FORMAT), rv,
                                       re.DOTALL)
            if matched_output:
                break
            logging.info('try %d: could not match ccd output %s', i, rv)
          except Exception, e:
            logging.info('try %d got error %s', i, str(e))

        self._servo.set_nocheck('cr50_uart_timeout', original_timeout)
        if not matched_output:
            raise error.TestFail('Could not get ccd output')
        logging.info('Current CCD settings:\n%s',
                     pprint.pformat(matched_output.groupdict()))
        return matched_output.groupdict()


    def get_cap(self, cap):
        """Returns the capabilitiy from the capability dictionary"""
        return self.get_cap_dict()[cap]


    def get_cap_dict(self, info=None):
        """Get the current ccd capability settings.

        The capability may be using the 'Default' setting. That doesn't say much
        about the ccd state required to use the capability. Return all ccd
        information in the cap_dict
        [is accessible, setting, requirement]

        @param info: Only fill the cap_dict with the requested information:
                     CAP_IS_ACCESSIBLE, CAP_SETTING, or CAP_REQ
        @return: A dictionary with the capability as the key a list of the
                 current settings as the value [is_accessible, setting,
                 requirement]
        """
        # Add whitespace at the end, so we can still match the last line.
        cap_info_str = self.get_ccd_info()['Capabilities'] + '\r\n'
        cap_settings = re.findall('(\S+) ' + self.CAP_FORMAT,
                                  cap_info_str)
        caps = {}
        for cap, accessible, setting, _, required in cap_settings:
            # If there's only 1 value after =, then the setting is the
            # requirement.
            if not required:
                required = setting
            cap_info = [accessible == 'Y', setting, required]
            if info is not None:
                caps[cap] = cap_info[info]
            else:
                caps[cap] = cap_info
        logging.debug(pprint.pformat(caps))
        return caps


    def send_command_get_output(self, command, regexp_list):
        """Send command through UART and wait for response.

        Cr50 will drop characters input to the UART when it resumes from sleep.
        If servo is not using ccd, send some dummy characters before sending the
        real command to make sure cr50 is awake.

        @param command: the command to send
        @param regexp_list: The list of regular expressions to match in the
                            command output
        @return: A list of matched output
        """
        if self._servo.main_device_is_flex():
            self.wake_cr50()

        # We have started prepending '\n' to separate cr50 console junk from
        # the real command. If someone is just searching for .*>, then they will
        # only get the output from the first '\n' we added. Raise an error to
        # change the test to look for something more specific ex command.*>.
        # cr50 will print the command in the output, so that is an easy way to
        # modify '.*>' to match the real command output.
        if '.*>' in regexp_list:
            raise error.TestError('Send more specific regexp %r %r' % (command,
                    regexp_list))

        # prepend \n to separate the command from any junk that may have been
        # sent to the cr50 uart.
        command = '\n' + command
        return super(ChromeCr50, self).send_command_get_output(command,
                                                               regexp_list)


    def send_safe_command_get_output(self, command, regexp_list,
            channel_mask=0x1):
        """Restrict the console channels while sending console commands.

        @param command: the command to send
        @param regexp_list: The list of regular expressions to match in the
                            command output
        @param channel_mask: The mask to pass to 'chan' prior to running the
                             command, indicating which channels should remain
                             enabled (0x1 is command output)
        @return: A list of matched output
        """
        self.send_command('chan save')
        self.send_command('chan 0x%x' % channel_mask)
        try:
            rv = self.send_command_get_output(command, regexp_list)
        finally:
            self.send_command('chan restore')
        return rv


    def send_command_retry_get_output(self, command, regexp_list, safe=False,
                                      compare_output=False):
        """Retry the command 5 times if you get a timeout or drop some output


        @param command: the command string
        @param regexp_list: the regex to search for
        @param safe: use send_safe_command_get_output if True otherwise use
                     send_command_get_output
        @param compare_output: look for reproducible output
        """
        send_command = (self.send_safe_command_get_output if safe else
                        self.send_command_get_output)
        err = 'no consistent output' if compare_output else 'unknown'
        past_rv = []
        for i in range(self.MAX_RETRY_COUNT):
            try:
                rv = send_command(command, regexp_list)
                if not compare_output or rv in past_rv:
                    return rv
                if past_rv:
                    logging.debug('%d %s not in %s', i, rv, past_rv)
                past_rv.append(rv)
            except Exception, e:
                err = str(e)
                logging.info('attempt %d %r: %s', i, command, str(e))
        if compare_output:
            logging.info('No consistent output for %r %s', command,
                         pprint.pformat(past_rv))
        raise error.TestError('Issue sending %r command: %s' % (command, err))


    def get_deep_sleep_count(self):
        """Get the deep sleep count from the idle task"""
        result = self.send_command_retry_get_output('idle', [self.IDLE_COUNT],
                                                    safe=True)
        return int(result[0][1])


    def clear_deep_sleep_count(self):
        """Clear the deep sleep count"""
        self.send_command('idle c')
        if self.get_deep_sleep_count():
            raise error.TestFail("Could not clear deep sleep count")


    def get_board_properties(self):
        """Get information from the version command"""
        rv = self.send_command_retry_get_output('brdprop',
                ['properties = (\S+)\s'], safe=True)
        return int(rv[0][1], 16)


    def uses_board_property(self, prop_name):
        """Returns 1 if the given property is set, or 0 otherwise

        @param prop_name: a property name in string type.
        """
        brdprop = self.get_board_properties()
        prop = self.BOARD_PROP[prop_name]
        return bool(brdprop & prop)


    def has_command(self, cmd):
        """Returns 1 if cr50 has the command 0 if it doesn't"""
        try:
            self.send_safe_command_get_output('help', [cmd])
        except:
            logging.info("Image does not include '%s' command", cmd)
            return 0
        return 1


    def reboot(self):
        """Reboot Cr50 and wait for cr50 to reset"""
        self.wait_for_reboot(cmd='reboot')


    def _uart_wait_for_reboot(self, cmd='\n', timeout=60):
        """Use uart to wait for cr50 to reboot.

        If a command is given run it and wait for cr50 to reboot. Monitor
        the cr50 uart to detect the reset. Wait up to timeout seconds
        for the reset.

        @param cmd: the command to run to reset cr50.
        @param timeout: seconds to wait to detect the reboot.
        """
        original_timeout = float(self._servo.get('cr50_uart_timeout'))
        # Change the console timeout to timeout, so we wait at least that long
        # for cr50 to print the start string.
        self._servo.set_nocheck('cr50_uart_timeout', timeout)
        try:
            self.send_command_get_output(cmd, self.START_STR)
            logging.debug('Detected cr50 reboot')
        except error.TestFail, e:
            logging.debug('Failed to detect cr50 reboot')
        # Reset the timeout.
        self._servo.set_nocheck('cr50_uart_timeout', original_timeout)


    def wait_for_reboot(self, cmd='\n', timeout=60):
        """Wait for cr50 to reboot

        Run the cr50 reset command. Wait for cr50 to reset and reenable ccd if
        necessary.

        @param cmd: the command to run to reset cr50.
        @param timeout: seconds to wait to detect the reboot.
        """
        if self._servo.main_device_is_ccd():
            self.send_command(cmd)
            # Cr50 USB is reset when it reboots. Wait for the CCD connection to
            # go down to detect the reboot.
            self.wait_for_ccd_disable(timeout, raise_error=False)
            self.ccd_enable()
        else:
            self._uart_wait_for_reboot(cmd, timeout)

        # On most devices, a Cr50 reset will cause an AP reset. Force this to
        # happen on devices where the AP is left down.
        if not self.faft_config.ap_up_after_cr50_reboot:
            logging.info('Resetting DUT after Cr50 reset')
            self._servo.get_power_state_controller().reset()


    def set_board_id(self, chip_bid, chip_flags):
        """Set the chip board id type and flags."""
        self.send_command('bid 0x%x 0x%x' % (chip_bid, chip_flags))


    def get_board_id(self):
        """Get the chip board id type and flags.

        bid_type_inv will be '' if the bid output doesn't show it. If no board
        id type inv is shown, then board id is erased will just check the type
        and flags.

        @returns a tuple (A string of bid_type:bid_type_inv:bid_flags,
                          True if board id is erased)
        """
        bid = self.send_command_retry_get_output('bid',
                    ['Board ID: (\S{8}):?(|\S{8}), flags (\S{8})\s'],
                    safe=True)[0][1:]
        bid_str = ':'.join(bid)
        bid_is_erased =  set(bid).issubset({'', 'ffffffff'})
        logging.info('chip board id: %s', bid_str)
        logging.info('chip board id is erased: %s',
                     'yes' if bid_is_erased else 'no')
        return bid_str, bid_is_erased


    def eraseflashinfo(self, retries=10):
        """Run eraseflashinfo.

        @returns True if the board id is erased
        """
        for i in range(retries):
            # The console could drop characters while matching 'eraseflashinfo'.
            # Retry if the command times out. It's ok to run eraseflashinfo
            # multiple times.
            rv = self.send_command_retry_get_output(
                    'eraseflashinfo', ['eraseflashinfo(.*)>'])[0][1].strip()
            logging.info('eraseflashinfo output: %r', rv)
            bid_erased = self.get_board_id()[1]
            eraseflashinfo_issue = 'Busy' in rv or 'do_flash_op' in rv
            if not eraseflashinfo_issue and bid_erased:
                break
            logging.info('Retrying eraseflashinfo')
        return bid_erased


    def rollback(self):
        """Set the reset counter high enough to force a rollback and reboot."""
        if not self.has_command('rollback'):
            raise error.TestError("need image with 'rollback'")

        inactive_partition = self.get_inactive_version_info()[0]

        self.wait_for_reboot(cmd='rollback')

        running_partition = self.get_active_version_info()[0]
        if inactive_partition != running_partition:
            raise error.TestError("Failed to rollback to inactive image")


    def rolledback(self):
        """Returns true if cr50 just rolled back"""
        return 'Rollback detected' in self.send_command_retry_get_output(
                'sysinfo', ['sysinfo.*>'], safe=True)[0]


    def get_version_info(self, regexp):
        """Get information from the version command"""
        return self.send_command_retry_get_output('ver', [regexp],
                                                  safe=True)[0][1::]


    def get_inactive_version_info(self):
        """Get the active partition, version, and hash"""
        return self.get_version_info(self.INACTIVE_VERSION)


    def get_active_version_info(self):
        """Get the active partition, version, and hash"""
        return self.get_version_info(self.ACTIVE_VERSION)


    def using_prod_rw_keys(self):
        """Returns True if the RW keyid is prod"""
        rv = self.send_command_retry_get_output('sysinfo',
                ['RW keyid:\s+(0x[0-9a-f]{8})'], safe=True)[0][1]
        logging.info('RW Keyid: 0x%s', rv)
        return rv in self.PROD_RW_KEYIDS


    def get_active_board_id_str(self):
        """Get the running image board id.

        @return: The board id string or None if the image does not support board
                 id or the image is not board id locked.
        """
        # Getting the board id from the version console command is only
        # supported in board id locked images .22 and above. Any image that is
        # board id locked will have support for getting the image board id.
        #
        # If board id is not supported on the device, return None. This is
        # still expected on all current non board id locked release images.
        try:
            version_info = self.get_version_info(self.ACTIVE_BID)
        except error.TestFail, e:
            logging.info(str(e))
            logging.info('Cannot use the version to get the board id')
            return None

        if self.BID_ERROR in version_info[4]:
            raise error.TestError(version_info)
        bid = version_info[4].split()[1]
        return cr50_utils.GetBoardIdInfoString(bid)


    def get_version(self):
        """Get the RW version"""
        return self.get_active_version_info()[1].strip()


    def ccd_is_enabled(self):
        """Return True if ccd is enabled.

        If the test is running through ccd, return the ccd_state value. If
        a flex cable is being used, use the CCD_MODE_L gpio setting to determine
        if Cr50 has ccd enabled.

        @return: 'off' or 'on' based on whether the cr50 console is working.
        """
        if self._servo.main_device_is_ccd():
            return self._servo.get('ccd_state') == 'on'
        else:
            return not bool(self.gpioget('CCD_MODE_L'))


    @dts_control_command
    def wait_for_stable_ccd_state(self, state, timeout, raise_error):
        """Wait up to timeout seconds for CCD to be 'on' or 'off'

        Verify ccd is off or on and remains in that state for 3 seconds.

        @param state: a string either 'on' or 'off'.
        @param timeout: time in seconds to wait
        @param raise_error: Raise TestFail if the value is state is not reached.
        @raise TestFail: if ccd never reaches the specified state
        """
        wait_for_enable = state == 'on'
        logging.info("Wait until ccd is %s", 'on' if wait_for_enable else 'off')
        enabled = utils.wait_for_value(self.ccd_is_enabled, wait_for_enable,
                                       timeout_sec=timeout)
        if enabled != wait_for_enable:
            error_msg = ("timed out before detecting ccd '%s'" %
                         ('on' if wait_for_enable else 'off'))
            if raise_error:
                raise error.TestFail(error_msg)
            logging.warning(error_msg)
        else:
            # Make sure the state doesn't change.
            enabled = utils.wait_for_value(self.ccd_is_enabled, not enabled,
                                           timeout_sec=self.SHORT_WAIT)
            if enabled != wait_for_enable:
                error_msg = ("CCD switched %r after briefly being %r" %
                             ('on' if enabled else 'off', state))
                if raise_error:
                    raise error.TestFail(error_msg)
                logging.info(error_msg)
        logging.info("ccd is %r", 'on' if enabled else 'off')


    @dts_control_command
    def wait_for_ccd_disable(self, timeout=60, raise_error=True):
        """Wait for the cr50 console to stop working"""
        self.wait_for_stable_ccd_state('off', timeout, raise_error)


    @dts_control_command
    def wait_for_ccd_enable(self, timeout=60, raise_error=False):
        """Wait for the cr50 console to start working"""
        self.wait_for_stable_ccd_state('on', timeout, raise_error)


    @dts_control_command
    def ccd_disable(self, raise_error=True):
        """Change the values of the CC lines to disable CCD"""
        logging.info("disable ccd")
        self._servo.set_dts_mode('off')
        self.wait_for_ccd_disable(raise_error=raise_error)


    @dts_control_command
    def ccd_enable(self, raise_error=False):
        """Reenable CCD and reset servo interfaces"""
        logging.info("reenable ccd")
        self._servo.set_dts_mode('on')
        # If the test is actually running with ccd, wait for USB communication
        # to come up after reset.
        if self._servo.main_device_is_ccd():
            time.sleep(self._servo.USB_DETECTION_DELAY)
        self.wait_for_ccd_enable(raise_error=raise_error)


    def _level_change_req_pp(self, level):
        """Returns True if setting the level will require physical presence"""
        testlab_pp = level != 'testlab open' and 'testlab' in level
        # If the level is open and the ccd capabilities say physical presence
        # is required, then physical presence will be required.
        open_pp = (level == 'open' and
                   not self.get_cap('OpenNoLongPP')[self.CAP_IS_ACCESSIBLE])
        return testlab_pp or open_pp


    def _state_to_bool(self, state):
        """Converts the state string to True or False"""
        # TODO(mruthven): compare to 'on' once servo is up to date in the lab
        return state.lower() in self.ON_STRINGS


    def testlab_is_on(self):
        """Returns True of testlab mode is on"""
        return self._state_to_bool(self._servo.get('cr50_testlab'))


    def set_ccd_testlab(self, state):
        """Set the testlab mode

        @param state: the desired testlab mode string: 'on' or 'off'
        @raise TestFail: if testlab mode was not changed
        """
        if self._servo.main_device_is_ccd():
            raise error.TestError('Cannot set testlab mode with CCD. Use flex '
                    'cable instead.')
        if not self.faft_config.has_powerbutton:
            raise error.TestError('No power button on device')

        request_on = self._state_to_bool(state)
        testlab_on = self.testlab_is_on()
        request_str = 'on' if request_on else 'off'

        if testlab_on == request_on:
            logging.info('ccd testlab already set to %s', request_str)
            return

        original_level = self.get_ccd_level()

        # We can only change the testlab mode when the device is open. If
        # testlab mode is already enabled, we can go directly to open using 'ccd
        # testlab open'. This will save 5 minutes, because we can skip the
        # physical presence check.
        if testlab_on:
            self.send_command('ccd testlab open')
        else:
            self.set_ccd_level('open')

        # Set testlab mode
        rv = self.send_command_get_output('ccd testlab %s' % request_str,
                ['ccd.*>'])[0]
        if 'Access Denied' in rv:
            raise error.TestFail("'ccd %s' %s" % (request_str, rv))

        # Press the power button once a second for 15 seconds.
        self.run_pp(self.PP_SHORT)

        self.set_ccd_level(original_level)
        if request_on != self.testlab_is_on():
            raise error.TestFail('Failed to set ccd testlab to %s' % state)


    def get_ccd_level(self):
        """Returns the current ccd privilege level"""
        return self._servo.get('cr50_ccd_level').lower()


    def set_ccd_level(self, level, password=''):
        """Set the Cr50 CCD privilege level.

        @param level: a string of the ccd privilege level: 'open', 'lock', or
                      'unlock'.
        @param password: send the ccd command with password. This will still
                         require the same physical presence.
        @raise TestFail: if the level couldn't be set
        """
        # TODO(mruthven): add support for CCD password
        level = level.lower()

        if level == self.get_ccd_level():
            logging.info('CCD privilege level is already %s', level)
            return

        if 'testlab' in level:
            raise error.TestError("Can't change testlab mode using "
                "ccd_set_level")

        testlab_on = self._state_to_bool(self._servo.get('cr50_testlab'))
        batt_is_disconnected = self.get_batt_pres_state()[1]
        req_pp = self._level_change_req_pp(level)
        has_pp = not self._servo.main_device_is_ccd()
        dbg_en = 'DBG' in self._servo.get('cr50_version')

        if req_pp and not has_pp:
            raise error.TestError("Can't change privilege level to '%s' "
                "without physical presence." % level)

        if not testlab_on and not has_pp:
            raise error.TestError("Wont change privilege level without "
                "physical presence or testlab mode enabled")

        original_timeout = float(self._servo.get('cr50_uart_timeout'))
        # Change the console timeout to CONSERVATIVE_CCD_WAIT, running 'ccd' may
        # take more than 3 seconds.
        self._servo.set_nocheck('cr50_uart_timeout', self.CONSERVATIVE_CCD_WAIT)
        # Start the unlock process.

        if level == 'open' or level == 'unlock':
            logging.info('waiting %d seconds, the minimum time between'
                         ' ccd password attempts',
                         self.CCD_PASSWORD_RATE_LIMIT)
            time.sleep(self.CCD_PASSWORD_RATE_LIMIT)

        try:
            cmd = 'ccd %s%s' % (level, (' ' + password) if password else '')
            # ccd command outputs on the rbox, ccd, and console channels,
            # respectively. Cr50 uses these channels to print relevant ccd
            # information.
            # Restrict all other channels.
            ccd_output_channels = 0x20000 | 0x8 | 0x1
            rv = self.send_safe_command_get_output(
                    cmd, [cmd + '(.*)>'],
                    channel_mask=ccd_output_channels)[0][1]
        finally:
            self._servo.set('cr50_uart_timeout', original_timeout)
        logging.info(rv)
        if 'ccd_open denied: fwmp' in rv:
            raise error.TestFail('FWMP disabled %r: %s' % (cmd, rv))
        if 'Access Denied' in rv:
            raise error.TestFail("%r %s" % (cmd, rv))
        if 'Busy' in rv:
            raise error.TestFail("cr50 is too busy to run %r: %s" % (cmd, rv))

        # Press the power button once a second, if we need physical presence.
        if req_pp and batt_is_disconnected:
            # DBG images have shorter unlock processes
            self.run_pp(self.PP_SHORT if dbg_en else self.PP_LONG)

        if level != self.get_ccd_level():
            raise error.TestFail('Could not set privilege level to %s' % level)

        logging.info('Successfully set CCD privelege level to %s', level)


    def run_pp(self, unlock_timeout):
        """Press the power button a for unlock_timeout seconds.

        This will press the power button many more times than it needs to be
        pressed. Cr50 doesn't care if you press it too often. It just cares that
        you press the power button at least once within the detect interval.

        For privilege level changes you need to press the power button 5 times
        in the short interval and then 4 times within the long interval.
        Short Interval
        100msec < power button press < 5 seconds
        Long Interval
        60s < power button press < 300s

        For testlab enable/disable you must press the power button 5 times
        spaced between 100msec and 5 seconds apart.
        """
        ap_on_before = self.ap_is_on()

        end_time = time.time() + unlock_timeout

        logging.info('Pressing power button for %ds to unlock the console.',
                     unlock_timeout)
        logging.info('The process should end at %s', time.ctime(end_time))

        # Press the power button once a second to unlock the console.
        while time.time() < end_time:
            self._servo.power_short_press()
            time.sleep(1)

        # If the last power button press left the AP powered off, and it was on
        # before, turn it back on.
        time.sleep(self.faft_config.shutdown)
        ap_on_after = self.ap_is_on()
        logging.debug('During run_pp, AP %s -> %s',
                'on' if ap_on_before else 'off',
                'on' if ap_on_after else 'off')
        if ap_on_before and not ap_on_after:
            self._servo.power_short_press()
            logging.debug('Pressing PP to turn back on')


    def gettime(self):
        """Get the current cr50 system time"""
        result = self.send_safe_command_get_output('gettime', [' = (.*) s'])
        return float(result[0][1])


    def servo_dts_mode_is_valid(self):
        """Returns True if cr50 registers change in servo dts mode."""
        # This is to test that Cr50 actually recognizes the change in ccd state
        # We cant do that with tests using ccd, because the cr50 communication
        # goes down once ccd is enabled.
        if not self._servo.dts_mode_is_safe():
            return False

        ccd_start = 'on' if self.ccd_is_enabled() else 'off'
        dts_start = self._servo.get_dts_mode()
        try:
            # Verify both ccd enable and disable
            self.ccd_disable(raise_error=True)
            self.ccd_enable(raise_error=True)
            rv = True
        except Exception, e:
            logging.info(e)
            rv = False
        self._servo.set_dts_mode(dts_start)
        self.wait_for_stable_ccd_state(ccd_start, 60, True)
        logging.info('Test setup does%s support servo DTS mode',
                '' if rv else 'n\'t')
        return rv


    def wait_until_update_is_allowed(self):
        """Wait until cr50 will be able to accept an update.

        Cr50 rejects any attempt to update if it has been less than 60 seconds
        since it last recovered from deep sleep or came up from reboot. This
        will wait until cr50 gettime shows a time greater than 60.
        """
        if self.get_active_version_info()[2]:
            logging.info("Running DBG image. Don't need to wait for update.")
            return
        cr50_time = self.gettime()
        if cr50_time < 60:
            sleep_time = 61 - cr50_time
            logging.info('Cr50 has been up for %ds waiting %ds before update',
                         cr50_time, sleep_time)
            time.sleep(sleep_time)


    def tpm_is_enabled(self):
        """Query the current TPM mode.

        @return:  True if TPM is enabled, False otherwise.
        """
        result = self.send_command_retry_get_output('sysinfo',
                ['(?i)TPM\s+MODE:\s+(enabled|disabled)'], safe=True)[0][1]
        logging.debug(result)

        return result.lower() == 'enabled'


    def get_keyladder_state(self):
        """Get the status of H1 Key Ladder.

        @return: The keyladder state string. prod or dev both mean enabled.
        """
        result = self.send_command_retry_get_output('sysinfo',
                ['(?i)Key\s+Ladder:\s+(enabled|prod|dev|disabled)'],
                safe=True)[0][1]
        logging.debug(result)
        return result


    def keyladder_is_disabled(self):
        """Get the status of H1 Key Ladder.

        @return: True if H1 Key Ladder is disabled. False otherwise.
        """
        return self.get_keyladder_state() == 'disabled'


    def get_sleepmask(self):
        """Returns the sleepmask as an int"""
        rv = self.send_command_retry_get_output('sleepmask',
                ['sleep mask: (\S{8})\s+'], safe=True)[0][1]
        logging.info('sleepmask %s', rv)
        return int(rv, 16)


    def get_ccdstate(self):
        """Return a dictionary of the ccdstate once it's done debouncing"""
        for i in range(self.CCDSTATE_MAX_RETRY_COUNT):
            rv = self.send_command_retry_get_output('ccdstate',
                    ['ccdstate(.*)>'], safe=True)[0][0]

            # Look for a line like 'AP: on' or 'AP: off'. 'debouncing' or
            # 'unknown' may appear transiently. 'debouncing' should transition
            # to 'on' or 'off' within 1 second, and 'unknown' should do so
            # within 20 seconds.
            if 'debouncing' not in rv and 'unknown' not in rv:
                break
            time.sleep(self.SHORT_WAIT)
        ccdstate = {}
        for line in rv.splitlines():
            line = line.strip()
            if ':' in line:
                k, v = line.split(':', 1)
                ccdstate[k.strip()] = v.strip()
        logging.info('Current CCD state:\n%s', pprint.pformat(ccdstate))
        return ccdstate


    def ap_is_on(self):
        """Get the power state of the AP.

        @return: True if the AP is on; False otherwise.
        """
        ap_state = self.get_ccdstate()['AP']
        if ap_state == 'on':
            return True
        elif ap_state == 'off':
            return False
        else:
            raise error.TestFail('Read unusable AP state from ccdstate: "%s"',
                                 ap_state)


    def gpioget(self, signal_name):
        """Get the current state of the signal

        @return an integer 1 or 0 based on the gpioget value
        """
        result = self.send_command_retry_get_output('gpioget',
                    ['(0|1)[ \S]*%s' % signal_name], safe=True)
        return int(result[0][1])


    def batt_pres_is_reset(self):
        """Returns True if batt pres is reset to always follow batt pres"""
        follow_bp, _, follow_bp_atboot, _ = self.get_batt_pres_state()
        return follow_bp and follow_bp_atboot


    def get_batt_pres_state(self):
        """Get the current and atboot battery presence state

        The atboot setting cannot really be determined now if it is set to
        follow battery presence. It is likely to remain the same after reboot,
        but who knows. If the third element of the tuple is True, the last
        element will not be that useful

        @return: a tuple of the current battery presence state
                 (True if current state is to follow batt presence,
                  True if battery is connected,
                  True if current state is to follow batt presence atboot,
                  True if battery is connected atboot)
        """
        # bpforce is added in 4.16. If the image doesn't have the command, cr50
        # always follows battery presence. In these images 'gpioget BATT_PRES_L'
        # accurately represents the battery presence state, because it can't be
        # overidden.
        if not self.has_command('bpforce'):
            batt_pres = not bool(self.gpioget('BATT_PRES_L'))
            return (True, batt_pres, True, batt_pres)

        # The bpforce command is very similar to the wp command. It just
        # substitutes 'connected' for 'enabled' and 'disconnected' for
        # 'disabled'.
        rv = self.send_command_retry_get_output('bpforce',
                ['batt pres: (forced )?(con|dis).*at boot: (forced )?'
                 '(follow|discon|con)'], safe=True)[0]
        _, forced, connected, _, atboot = rv
        logging.info(rv)
        return (not forced, connected == 'con', atboot == 'follow',
                atboot == 'con')


    def set_batt_pres_state(self, state, atboot):
        """Override the battery presence state.

        @param state: a string of the battery presence setting: 'connected',
                  'disconnected', or 'follow_batt_pres'
        @param atboot: True if we're overriding battery presence atboot
        """
        cmd = 'bpforce %s%s' % (state, ' atboot' if atboot else '')
        logging.info('running %r', cmd)
        self.send_command(cmd)


    def dump_nvmem(self):
        """Print nvmem objects."""
        rv = self.send_safe_command_get_output('dump_nvmem',
                                               ['dump_nvmem(.*)>'])[0][1]
        logging.info('NVMEM OUTPUT:\n%s', rv)


    def get_reset_cause(self):
        """Returns the reset flags for the last reset."""
        rv = self.send_command_retry_get_output('sysinfo',
                ['Reset flags:\s+0x([0-9a-f]{8})\s'], compare_output=True)[0][1]
        logging.info('reset cause: %s', rv)
        return int(rv, 16)


    def was_reset(self, reset_type):
        """Returns 1 if the reset type is found in the reset_cause.

        @param reset_type: reset name in string type.
        """
        reset_cause = self.get_reset_cause()
        reset_flag = self.RESET_FLAGS[reset_type]
        return bool(reset_cause & reset_flag)
