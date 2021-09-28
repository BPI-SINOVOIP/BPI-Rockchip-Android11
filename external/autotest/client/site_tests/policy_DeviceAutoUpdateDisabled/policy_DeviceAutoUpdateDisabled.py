# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import math
import time

from autotest_lib.client.common_lib import autotemp
from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib import utils
from autotest_lib.client.cros.update_engine import nebraska_wrapper
from autotest_lib.client.cros.update_engine import update_engine_test
from autotest_lib.client.cros.enterprise import enterprise_policy_base


class policy_DeviceAutoUpdateDisabled(
        enterprise_policy_base.EnterprisePolicyTest,
        update_engine_test.UpdateEngineTest):
    """Test for the DeviceAutoUpdateDisabled policy."""
    version = 1
    _POLICY = 'DeviceAutoUpdateDisabled'


    def _test_update_disabled(self, port, should_update):
        """
        Main test function.

        Try to update and poll for start (or lack of start) to the update.
        Check whether an update request was sent.

        @param should_update: True or False whether the device should update.

        """
        # Log time is only in second accuracy.  Assume no update request has
        # occured since the current whole second started.
        start_time = math.floor(time.time())
        logging.info('Update test start time: %s', start_time)

        try:
            self._check_for_update(port=port, interactive=False)

            utils.poll_for_condition(
                    self._is_update_started,
                    timeout=60,
                    exception=error.TestFail('Update did not start!'))
        except error.TestFail as e:
            if should_update:
                raise e
        else:
            if not should_update:
                raise error.TestFail('Update started when it should not have!')

        update_time = self._get_time_of_last_update_request()
        logging.info('Last update time: %s', update_time)

        if should_update and (not update_time or update_time < start_time):
            raise error.TestFail('No update request was sent!')
        if not should_update and update_time and update_time >= start_time:
            raise error.TestFail('Update request was sent!')


    def run_once(self, case, image_url, enroll=True):
        """
        Entry point of this test.

        @param case: True, False, or None for the value of the update policy.
        @param image_url: Url of update image (this build).

        """
        # Because we are doing polimorphism and the EnterprisePolicyTest is
        # earlier in the python MRO, this class's initialize() will get called,
        # but not the UpdateEngineTest's initialize(). So we need to call it
        # manually.
        update_engine_test.UpdateEngineTest.initialize(self)

        self.setup_case(device_policies={self._POLICY: case}, enroll=enroll)

        metadata_dir = autotemp.tempdir()
        self._get_payload_properties_file(image_url, metadata_dir.name,
                                          target_version='999999.9.9')
        base_url = ''.join(image_url.rpartition('/')[0:2])
        with nebraska_wrapper.NebraskaWrapper(
                log_dir=self.resultsdir,
                update_metadata_dir=metadata_dir.name,
                update_payloads_address=base_url) as nebraska:

            update_url = nebraska.get_update_url()
            self._create_custom_lsb_release(update_url, build='1.1.1')

            # When policy is False or not set, user should update.
            self._test_update_disabled(port=nebraska.get_port(),
                                       should_update=case is not True)

        self.cleanup()
