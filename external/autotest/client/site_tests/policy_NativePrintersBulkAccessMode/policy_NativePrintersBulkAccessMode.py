# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.enterprise import enterprise_policy_base


class policy_NativePrintersBulkAccessMode(
        enterprise_policy_base.EnterprisePolicyTest):
    """Verify behavior of NativePrinters user policy."""
    version = 1

    def initialize(self, **kwargs):
        """Initialize."""
        self._initialize_test_constants()
        self._initialize_enterprise_policy_test()


    def _initialize_test_constants(self):
        """Construct policy values as needed."""
        PRINTERS_URL = ('https://storage.googleapis.com/chromiumos-test-assets'
                        '-public/enterprise/printers.json')
        PRINTERS_HASH = ('7a052c5e4f23c159668148df2a3c202bed4d65749cab5ecd0f'
                         'a7db211c12a3b8') #sha256
        PRINTERS2_URL = ('https://storage.googleapis.com/chromiumos-test-assets'
                         '-public/enterprise/printers2.json')
        PRINTERS2_HASH = ('0d7344c989893cb97484d6111bf497a999333c177e1c991c06'
                          'db664c58e4b81a') #sha256
        # These strings are printer ids, defined in PRINTERS_URL.
        self.DEFINED_IDS = set(['wl', 'bl', 'other', 'both'])
        # These strings are printer ids, defined in PRINTERS2_URL.
        self.DEFINED_IDS2 = set(['wl2', 'bl2', 'other2', 'both2'])
        # Whitelist and blacklist, common for both sets of printers
        self.WHITELIST = ['both', 'both2', 'wl', 'wl2', 'otherwl']
        self.BLACKLIST = ['both', 'both2', 'bl', 'bl2', 'otherbl']

        self.user_policies = {
                'NativePrintersBulkConfiguration': {'url': PRINTERS_URL,
                                                    'hash': PRINTERS_HASH},
                'NativePrintersBulkWhitelist': self.WHITELIST,
                'NativePrintersBulkBlacklist': self.BLACKLIST}

        self.device_policies = {
                'DeviceNativePrinters': {'url': PRINTERS2_URL,
                                         'hash': PRINTERS2_HASH},
                'DeviceNativePrintersWhitelist': self.WHITELIST,
                'DeviceNativePrintersBlacklist': self.BLACKLIST}

        self.USER_ACCESS_MODE_NAME = 'NativePrintersBulkAccessMode'
        self.DEVICE_ACCESS_MODE_NAME = 'DeviceNativePrintersAccessMode'
        self.ACCESS_MODE_VALUES = {'allowall': 2,
                                   'whitelist': 1,
                                   'blacklist': 0}


    def _get_printer_ids(self):
        """
        Use autotest_private to read the ids of listed printers.

        @returns: a set of ids of printers that would be seen by a user under
                  Print Destinations.

        """
        self.cr.autotest_ext.ExecuteJavaScript(
                'window.__printers = null; '
                'chrome.autotestPrivate.getPrinterList(function(printers) {'
                '    window.__printers = printers;'
                '});')
        self.cr.autotest_ext.WaitForJavaScriptCondition(
                'window.__printers !== null')
        printers = self.cr.autotest_ext.EvaluateJavaScript(
                'window.__printers')
        logging.info('Printers found: %s', printers)

        if not isinstance(printers, list):
            raise error.TestFail('Received response is not a list!')

        ID_KEY = u'printerId'
        ids = set()
        for printer in printers:
            if ID_KEY in printer:
                ids.add(printer[ID_KEY].encode('ascii'))
            else:
                raise error.TestFail('Missing %s field!' % ID_KEY)
        logging.info('Found ids: %s', ids)

        if len(ids) < len(printers):
            raise error.TestFail('Received response contains duplicates!')

        return ids


    def _resultant_set_of_ids(self, access_mode, set_of_all_ids):
        """
        Calculates resultant set of printers identifiers depending on given
        access_mode.

        @param access_mode: one of strings: 'allowall', 'whitelist',
            'blacklist' or None (policy not set).
        @param set_of_all_ids: set of all printer identifiers.

        @returns: a set of ids of printers.

        """
        if access_mode is None:
            return set()
        if access_mode == 'blacklist':
            return set_of_all_ids - set(self.BLACKLIST)
        if access_mode == 'whitelist':
            return set_of_all_ids & set(self.WHITELIST)
        if access_mode == 'allowall':
            return set_of_all_ids
        raise Exception("Incorrect value of access mode")


    def _test_bulk_native_printers(self, case_device, case_user):
        """

        @param case_device: access mode set in device policies (one of strings:
                'allowall', 'whitelist', 'blacklist') or None (policy not set).
        @param case_user: access mode set in user policies (one of strings:
                'allowall', 'whitelist', 'blacklist') or None (policy not set).

        """
        printers = self._get_printer_ids()
        expected = self._resultant_set_of_ids(case_device, self.DEFINED_IDS2)
        expected |= self._resultant_set_of_ids(case_user, self.DEFINED_IDS)
        if printers != expected:
            raise error.TestFail('Did not see expected printer output! '
                                 'Expected %s, found %s.' % (expected,
                                                             printers))


    def run_once(self, case):
        """
        Entry point of this test.

        @param case: a tuple of two access modes, the first one is for device
                policies, the second one is for user policies. Each access mode
                equals one of the strings: 'allowall', 'whitelist', 'blacklist'
                or is set to None (what means 'policy not set').

        """
        case_device, case_user = case

        if case_device is None:
            self.device_policies = {}
            enroll = False
        else:
            assert(case_device in self.ACCESS_MODE_VALUES)
            self.device_policies[self.DEVICE_ACCESS_MODE_NAME] =\
                    self.ACCESS_MODE_VALUES[case_device]
            enroll = True

        if case_user is None:
            self.user_policies = {}
        else:
            assert(case_user in self.ACCESS_MODE_VALUES)
            self.user_policies[self.USER_ACCESS_MODE_NAME] =\
                    self.ACCESS_MODE_VALUES[case_user]

        self.setup_case(device_policies=self.device_policies,
                user_policies=self.user_policies, enroll=enroll)
        self._test_bulk_native_printers(case_device, case_user)
