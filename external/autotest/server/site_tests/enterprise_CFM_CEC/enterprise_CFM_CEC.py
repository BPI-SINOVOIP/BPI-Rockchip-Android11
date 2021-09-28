# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LISENCE file.

import logging
import re
import os
import time

from autotest_lib.client.common_lib import error
from autotest_lib.server import test

CEC_COMMAND = "sudo -u chronos dbus-send --system --print-reply \
--type=method_call --dest=org.chromium.CecService   \
/org/chromium/CecService org.chromium.CecService."
CEC_CMD_STANDBY = "SendStandByToAllDevices"
CEC_CMD_IMAGEON = "SendWakeUpToAllDevices"
CEC_CMD_DISP_STATUS = "GetTvsPowerStatus"

CHAMELEON_ROOT = '/home/root/'

STATUS_ERROR = 0
STATUS_ON = 3
STATUS_OFF = 4
STATUS_TO_ON = 5
STATUS_TO_OFF = 6

class enterprise_CFM_CEC(test.test):
    """
    Test CEC feature for display power control.
    """

    version = 1
    # TODO: how to get whether it connects to chameleon board
    chameleon_mode = True

    def initialize(self):
        """ initialize is a stub function."""
        pass

    # Simply run a command in machine and return result messages.
    def _shcmd(self, cmd):
        """ A simple wrapper for remote shell command execution.
        @param cmd: shell command for Fizz
        """
        logging.info('CMD: [%s]', cmd)
        try:
            result = self._client.run(cmd)
            if result is None:
                return result
            if result.stderr:
                logging.info('CMD ERR:\n' + result.stderr)
            logging.info('CMD OUT:\n' + result.stdout)
            return result
        except Exception as e:
            logging.info('run command failed. ' + str(e))

    def run_once(self, host=None):
        """
        Test scenario:

            If system does not support cec port, we simply throw an exception.

            Generally we are using the build-in cecservice for this test. This
        service supports multiple features including turning TV(display)
        power on/off and monitor its power status.

            Following is test plan:
            0.0 Copy two python files to chameleon
            0.1 enable chameleon as hdmi mode
            0.2 run cec service on chameleon

            0.3 Make sure chrome box is running cecservice
            0.4 Make sure chrome box's /dev/cecX is open
            0.5 Run TV power status to check configuration correct
            (end of step 0)


            1.0 Send TV turn-off command
            1.1 Check TV power status to check whether it is off.
            (end of step 1)

            2.0 Send TV turn-on command
            2.1 Check TV power status to check whether it is on.
            (end of step 2)
            At every end of step, we have to decide whether we stop this test
        right now or continue next ones.
            Note that we could turn on TV first and then turn off it,
        according to its initial power status.

            3.0 Stop cec service on chameleon
            3.1 Remove python files from chameleon

        @param host: host object machine.
        """

        self._client = host
        self.chameleon = host.chameleon

        ## TODO check same port
        #Step 0.0 - 0.2
        self.copy_cecservice()
        self.cec_service_init()


        # Step 0.3 - 0.5
        if not self.is_cecservice_running():
            self.cec_cleanup()
            raise error.TestFail("CEC service is not running.")
        if not self.is_cec_available():
            self.cec_cleanup()
            raise error.TestFail("/dev/cecX port is not open.")
        status = self.check_display_status()
        if STATUS_ERROR == status:
            self.cec_cleanup()
            raise error.TestFail("CEC communication is not good.")

        # Step 1 & 2
        if STATUS_ON == status:
            self.test_turn_off()
            if not self.chameleon_mode:
                time.sleep(5)
            self.test_turn_on()
        else:
            self.test_turn_on()
            if not self.chameleon_mode:
                time.sleep(5)
            self.test_turn_off()

        # Step 3
        self.cec_cleanup()

    # Chameleon
    def copy_cecservice(self):
        """ copy python files under ./chameleon_cecservice to chameleon.
            In that folder, we have two files for cecservice.
        """
        current_dir = os.path.dirname(os.path.realpath(__file__))
        base_dir = current_dir + '/chameleon_cecservice/'
        self.chameleon.host.send_file(base_dir + 'cec_service', CHAMELEON_ROOT)
        self.chameleon.host.send_file(base_dir + 'it6803.py', CHAMELEON_ROOT)

    def cec_service_init(self):
        """ Setup chameleon board as a hdmi mode.
            Run cec service on chameleon
        """
        self.chameleon.host.run('/home/root/setup hdmi')
        self.chameleon.host.run('(/home/root/cec_service start) '\
            '</dev/null >/dev/null 2>&1 & echo -n $!')

    def cec_cleanup(self):
        """ Stop cec service on chameleon.
            Delete files new coming on chameleon.
        """
        if self.chameleon_mode:
            stop_cmd = 'kill $(ps | grep \'cec_service\' | awk \'{print $1}\')'
            self.chameleon.host.run(stop_cmd)
            cleanup_cmd = 'rm /home/root/cec_service /home/root/it6803*'
            self.chameleon.host.run(cleanup_cmd)

    # Fizz
    def is_cecservice_running(self):
        """ try to confirm that current box is running cecservice.
        @return: whether cecservice is running in Fizz.
        """
        cmd = 'initctl list | grep cecservice'
        result = self._shcmd(cmd)
        if result is None:
            return False;
        if "running" not in result.stdout:
            return False;
        return True;

    def is_cec_available(self):
        """ try to get whether the system makes /dev/cecX open.
        @return: whether /dev/cecX is open in Fizz
        """
        cmd = "ls /dev/cec*"
        result = self._shcmd(cmd)
        if result is None:
            return False;
        return True;

    def check_display_status(self):
        """ try to confirm that current box connected to a display
        which supports cec feature.
        @return: current display power status
        """
        cmd = CEC_COMMAND + CEC_CMD_DISP_STATUS
        result = self._shcmd(cmd).stdout
        status = re.findall('int32 \\d', result)
        for s in status:
            code = int(s[-1]);
            if code == STATUS_ON or code == STATUS_TO_ON:
                return STATUS_ON
            if code == STATUS_OFF or code == STATUS_TO_OFF:
                return STATUS_OFF
        return STATUS_ERROR

    def display_on(self):
        """ send a power turn on cec message """
        self._shcmd(CEC_COMMAND + CEC_CMD_IMAGEON)

    def display_off(self):
        """ send a power turn off cec message"""
        self._shcmd(CEC_COMMAND + CEC_CMD_STANDBY)

    def test_turn_on(self):
        """ test sending turn_on cec message process. """
        self.display_on()
        if not self.chameleon_mode:
            time.sleep(10)
        status = self.check_display_status()
        if STATUS_ON != status:
            self.cec_cleanup()
            raise error.TestFail("CEC display image on does not work.")

    def test_turn_off(self):
        """ test sending turn_off cec message process. """
        self.display_off()
        if not self.chameleon_mode:
            time.sleep(1)
        status = self.check_display_status()
        if STATUS_OFF != status:
            self.cec_cleanup()
            raise error.TestFail("CEC display standby does not work.")
