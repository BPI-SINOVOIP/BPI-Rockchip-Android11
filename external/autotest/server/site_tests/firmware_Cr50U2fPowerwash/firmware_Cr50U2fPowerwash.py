# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import hashlib

from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib.cros import g2f_utils
from autotest_lib.client.common_lib.cros import tpm_utils
from autotest_lib.server import test

U2F_AUTH_ENFORCE=3

class firmware_Cr50U2fPowerwash(test.test):
    """
    A test that runs sanity checks for U2F register and authenticate functions,
    and checks that key handles are invalidated after TPM clear.
    """
    version = 1

    def parse_g2ftool_output(self, stdout):
      """Parses the key-value pairs returned by g2ftool

      @param stdout: g2ftool output.
      """
      return dict((k, v)
                  for k,v in (line.split('=')
                              for line in stdout.strip().split('\n')))

    def run_once(self, host=None):
        """Tests that U2F keys are invalidated by powerwash."""
        self.client = host
        self.servo = host.servo

        self.servo.initialize_dut()

        # Start by clearing TPM to make sure the device is in a known state.
        tpm_utils.ClearTPMOwnerRequest(self.client, wait_for_ready=True)

        # u2fd reads files from the user's home dir, so we need to log in.
        g2f_utils.ChromeOSLogin(self.client);

        # U2fd does will not start normally if the device has not gone
        # through OOBE. Force it to startup.
        cr50_dev = g2f_utils.StartU2fd(self.client)

        # Register requires physical presence.
        self.servo.power_short_press()

        # Register to create a new key handle.
        g2f_reg = g2f_utils.G2fRegister(
            self.client,
            cr50_dev,
            hashlib.sha256('test_challenge').hexdigest(),
            hashlib.sha256('test_application').hexdigest(),
            U2F_AUTH_ENFORCE)

        # Sanity check that we managed to register.
        if not g2f_reg.exit_status == 0:
          raise error.TestError('Register failed.')

        # Extract newly created key handle.
        key_handle = self.parse_g2ftool_output(g2f_reg.stdout)['key_handle']

        # Auth requires physical presence.
        self.servo.power_short_press()

        # Sanity check that we can authenticate with the new key handle.
        g2f_auth = g2f_utils.G2fAuth(
            self.client,
            cr50_dev,
            hashlib.sha256('test_challenge').hexdigest(),
            hashlib.sha256('test_application').hexdigest(),
            key_handle,
            U2F_AUTH_ENFORCE)

        if not g2f_auth.exit_status == 0:
          raise error.TestError('Authenticate failed.')

        # Clear TPM. We should no longer be able to authenticate with the
        # key handle after this.
        tpm_utils.ClearTPMOwnerRequest(self.client, wait_for_ready=True)

        # u2fd reads files from the user's home dir, so we need to log in.
        g2f_utils.ChromeOSLogin(self.client)

        # U2fd does will not start normally if the device has not gone
        # through OOBE. Force it to startup.
        cr50_dev = g2f_utils.StartU2fd(self.client)

        # Check the key handle is no longer valid.
        self.servo.power_short_press()
        g2f_auth_clear = g2f_utils.G2fAuth(
            self.client,
            cr50_dev,
            hashlib.sha256('test_challenge').hexdigest(),
            hashlib.sha256('test_application').hexdigest(),
            key_handle,
            U2F_AUTH_ENFORCE)

        if g2f_auth_clear.exit_status == 0:
          raise error.TestError('Authenticate succeeded; should have failed')


    def cleanup(self):
        """Leave the device in a predictable state"""
        g2f_utils.ChromeOSLogout(self.client)

        super(firmware_Cr50U2fPowerwash, self).cleanup()
