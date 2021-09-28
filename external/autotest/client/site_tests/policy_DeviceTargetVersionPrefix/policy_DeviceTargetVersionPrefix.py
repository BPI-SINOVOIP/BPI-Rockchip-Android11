# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import re

from autotest_lib.client.common_lib import autotemp
from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib import utils
from autotest_lib.client.cros.enterprise import enterprise_policy_base
from autotest_lib.client.cros.update_engine import nebraska_wrapper
from autotest_lib.client.cros.update_engine import update_engine_test


class policy_DeviceTargetVersionPrefix(
        enterprise_policy_base.EnterprisePolicyTest,
        update_engine_test.UpdateEngineTest):
    """
    Test for the DeviceTargetVersionPrefix policy.

    Cases:
    short_prefix - prefix value is a shortened string of the serving build.
    full_prefix - prefix value is a full string of the serving build.
    not_set - policy not set.

    This test only checks whether the update request has the correct values,
    since the client does not check if the version listed in the update
    returned by the AU devserver matches or not.

    """
    version = 1
    _POLICY_NAME = 'DeviceTargetVersionPrefix'
    _TEST_CASES = {'short_prefix': '4444.',
                   'full_prefix': '4444.4.4',
                   'notset': None}

    # The policy value -> what the device will send in the update request.
    _POLICY_TO_REQUEST = {'4444.': '4444', '4444.4.4': '4444.4.'}


    def _test_version_prefix(self, prefix_value, port):
        """
        Actual test.  Fail if update request doesn't match expected.

        @param prefix_value: the value of this policy.
        @param port: The port we should connect to Nebraska server.

        @raises error.TestFail if test does not pass.

        """
        # E.g. <updatecheck targetversionprefix="10718.25.0.">
        MATCH_STR = r'targetversionprefix="(.*?).?"'

        self._check_for_update(port=port)

        utils.poll_for_condition(
                self._is_update_started,
                timeout=60,
                exception=error.TestFail('Update did not start!'))

        latest_request = self._get_latest_initial_request()
        if not latest_request:
            raise error.TestFail('Could not find most recent update request!')

        search = re.search(MATCH_STR, latest_request)
        # If policy is not set, there should be no matches.
        if not prefix_value:
            if search:
                raise error.TestFail('Version prefix was set when the policy '
                                     'was not set!')
        # If policy is set, the value should be in the update request.
        else:
            if not search:
                raise error.TestFail('Update request did not contain target '
                                     'version prefix!')
            logging.info('Match value: %s', search.group(1))
            if search.group(1) != self._POLICY_TO_REQUEST[prefix_value]:
                raise error.TestFail('Version prefix in this update did not '
                                     'match the value expected from policy!')


    def run_once(self, case, image_url):
        """
        Entry point of this test.

        @param case: Name of the testcase to run.
        @param image_url: Url of update image (this build).

        """
        # Because we are doing polimorphism and the EnterprisePolicyTest is
        # earlier in the python MRO, this class's initialize() will get called,
        # but not the UpdateEngineTest's initialize(). So we need to call it
        # manually.
        update_engine_test.UpdateEngineTest.initialize(self)

        case_value = self._TEST_CASES[case]
        self.setup_case(device_policies={self._POLICY_NAME: case_value},
                        enroll=True)

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

            self._test_version_prefix(case_value, nebraska.get_port())
