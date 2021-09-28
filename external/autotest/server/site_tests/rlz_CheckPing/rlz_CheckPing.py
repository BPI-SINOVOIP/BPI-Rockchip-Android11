# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import datetime

from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib.cros import tpm_utils
from autotest_lib.client.common_lib.cros import vpd_utils
from autotest_lib.server import autotest
from autotest_lib.server import test


class rlz_CheckPing(test.test):
    """ Tests we are sending the CAF and CAI RLZ pings for first user."""
    version = 1

    _CLIENT_TEST = 'desktopui_CheckRlzPingSent'
    _PING_VPD = 'should_send_rlz_ping'
    _DATE_VPD = 'rlz_embargo_end_date'

    def _check_rlz_brand_code(self):
        """Checks that we have an rlz brand code."""
        try:
            self._host.run('cros_config / brand-code')
        except error.AutoservRunError as e:
            raise error.TestFail('DUT is missing brand_code: %s.' %
                                 e.result_obj.stderr)


    def _set_vpd_values(self, should_send_rlz_ping, rlz_embargo_end_date):
        """
        Sets the required vpd values for the test.

        @param should_send_rlz_ping: Value to set should_send_rlz_ping. 1 to
                                     send the ping, 0 to not send it.
        @param embargo_date: Date for rlz_embargo_end_date value. This should
                             be a datetime.date object with the desired date
                             for rlz_embargo_end_date, or None to clear it and
                             proceed with rlz_embargo_end_date not set.

        """
        to_set = {self._PING_VPD: should_send_rlz_ping}
        if rlz_embargo_end_date:
            to_set[self._DATE_VPD] = rlz_embargo_end_date.isoformat()
        else:
            vpd_utils.vpd_delete(host=self._host, key=self._DATE_VPD)

        vpd_utils.vpd_set(host=self._host, vpd_dict=to_set, dump=True,
                          force_dump=True)


    def _check_rlz_vpd_settings_post_ping(self, should_send_rlz_ping,
                                          rlz_embargo_end_date):
        """
        Checks that rlz related vpd settings are correct after the test.
        In the typical case where the first-use event (CAF) ping is sent
        successfully, both should_send_rlz_ping and rlz_embargo_end_date VPD
        values should be cleared. If the CAF ping is not sent, they will
        remain unchanged.

        @param should_send_rlz_ping: Expected value for the
                                     should_send_rlz_ping VPD setting
                                     (0 or 1).
        @param rlz_embargo_end_date: Expected value of the
                                     rlz_embargo_end_date VPD setting.
                                     This argument should be None if expecting
                                     the value to be cleared, and a
                                     datetime.date object containing the
                                     expected date otherwise.

        """
        check_should_send_ping = int(
            vpd_utils.vpd_get(host=self._host, key=self._PING_VPD))
        check_date = vpd_utils.vpd_get(host=self._host,
                                       key=self._DATE_VPD)

        if rlz_embargo_end_date:
            rlz_embargo_end_date = rlz_embargo_end_date.isoformat()

        if check_date != rlz_embargo_end_date:
            raise error.TestFail('Unexpected value for rlz_embargo_end_date:'
                                 ' %s' % check_date)

        if check_should_send_ping != should_send_rlz_ping:
            raise error.TestFail('Unexpected value for should_send_rlz_ping:'
                                 ' %s' % check_should_send_ping)


    def run_once(self, host, ping_timeout=30, logged_in=True,
                 rlz_embargo_end_date=None, should_send_rlz_ping=1):
        """
        Configures the DUT to send RLZ pings. Checks for the RLZ client
        install (CAI) and first-use (CAF) pings.

        @param host: Host to run test on.
        @param ping_timeout: Delay time (seconds) before both CAI and CAF
                             pings are sent.
        @param logged_in: True for real login or False for guest mode.
        @param rlz_embargo_end_date: Date for rlz_embargo_end_date VPD
                                     setting. If None, no value will be set
                                     for rlz_embargo_end_date, and any
                                     existing value will be cleared. For a
                                     specific rlz_embargo_end_date, this
                                     argument should be a datetime.date
                                     object containing the desired date.
        @param should_send_rlz_ping: Value of the should_send_rlz_ping VPD
                                     setting. 1 to send the first-use (CAF)
                                     ping, 0 to not send it. The ping should
                                     not be sent when the VPD value is 0,
                                     which in the normal use-case occurs after
                                     the CAF ping has been sent. It is set to
                                     0 after the CAF ping to ensure it is not
                                     sent again in the device's lifetime.

        """
        self._host = host
        if 'veyron_rialto' in self._host.get_board():
            raise error.TestNAError('Skipping test on rialto device.')

        self._check_rlz_brand_code()

        # Clear TPM owner so we have no users on DUT.
        tpm_utils.ClearTPMOwnerRequest(self._host)

        # Setup DUT to send rlz pings after a short timeout.
        self._set_vpd_values(should_send_rlz_ping=should_send_rlz_ping,
                             rlz_embargo_end_date=rlz_embargo_end_date)
        self._host.reboot()

        # We expect CAF ping to be sent when:
        # 1. should_send_rlz_ping is 1
        # 2. rlz_embargo_end_date is missing or in the past
        if rlz_embargo_end_date:
            expect_caf = (bool(should_send_rlz_ping) and
                          (datetime.date.today() - rlz_embargo_end_date).days
                          > 0)
        else:
            expect_caf = bool(should_send_rlz_ping)

        # Login, do a Google search, verify events in RLZ Data file.
        client_at = autotest.Autotest(self._host)
        client_at.run_test(self._CLIENT_TEST, ping_timeout=ping_timeout,
                           logged_in=logged_in,
                           expect_caf_ping=expect_caf)
        client_at._check_client_test_result(self._host, self._CLIENT_TEST)

        if expect_caf:
            self._check_rlz_vpd_settings_post_ping(
                should_send_rlz_ping=0, rlz_embargo_end_date=None)
        else:
            self._check_rlz_vpd_settings_post_ping(
                should_send_rlz_ping=should_send_rlz_ping,
                rlz_embargo_end_date=rlz_embargo_end_date)
