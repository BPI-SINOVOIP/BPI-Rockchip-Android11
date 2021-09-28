# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.enterprise import enterprise_policy_base
from telemetry.core.exceptions import EvaluateException


class policy_URLBlacklist(enterprise_policy_base.EnterprisePolicyTest):
    version = 1

    def initialize(self, **kwargs):
        """Initialize this test and set test constants."""
        super(policy_URLBlacklist, self).initialize(**kwargs)
        self.start_webserver()

        self.POLICY_NAME = 'URLBlacklist'
        self.URL_BASE = '%s/%s' % (self.WEB_HOST, 'website')
        self.ALL_URLS_LIST = [self.URL_BASE + website for website in
                              ['/website1.html',
                               '/website2.html',
                               '/website3.html']]

        self.SINGLE_BLACKLISTED_FILE = self.ALL_URLS_LIST[:1]
        self.MULTIPLE_BLACKLISTED_FILES = self.ALL_URLS_LIST[:2]
        self.BLACKLIST_WILDCARD = ['*']
        self.BLOCKED_USER_MESSAGE = 'Webpage Blocked'
        self.BLOCKED_ERROR_MESSAGE = 'ERR_BLOCKED_BY_ADMINISTRATOR'

        self.TEST_CASES = {
            'NotSet_Allowed': None,
            'SinglePage_Blocked': self.SINGLE_BLACKLISTED_FILE,
            'MultiplePages_Blocked': self.MULTIPLE_BLACKLISTED_FILES,
            'Wildcard_Blocked': self.BLACKLIST_WILDCARD,
        }

        # chrome://* URLs need to be accessible for the test to run.
        self.SUPPORTING_POLICIES = {'URLWhitelist': ['chrome://*']}


    def _scrape_text_from_webpage(self, tab):
        """
        Return a list of filtered text on the web page.

        @param tab: tab containing the website to be parsed.
        @raises: TestFail if the expected text was not found on the page.

        """
        parsed_message_string = ''
        parsed_message_list = []
        page_scrape_cmd = 'document.getElementById("main-message").innerText;'
        try:
            parsed_message_string = tab.EvaluateJavaScript(page_scrape_cmd)
        except Exception as err:
            raise error.TestFail('Unable to find the expected '
                                 'text content on the test '
                                 'page: %s\n %r' % (tab.url, err))
        logging.info('Parsed message: %s' % parsed_message_string)
        parsed_message_list = [str(word) for word in
                               parsed_message_string.split('\n') if word]
        return parsed_message_list


    def _is_url_blocked(self, url):
        """
        Return True if the URL is blocked else returns False.

        @param url: The URL to be checked whether it is blocked.

        """
        parsed_message_list = []
        tab = self.navigate_to_url(url)
        parsed_message_list = self._scrape_text_from_webpage(tab)
        if len(parsed_message_list) == 2 and \
                parsed_message_list[0] == 'Website enabled' and \
                parsed_message_list[1] == 'Website is enabled':
            return False

        # Check if accurate user error message is shown on the error page.
        if parsed_message_list[0] != self.BLOCKED_USER_MESSAGE or \
                parsed_message_list[1] != self.BLOCKED_ERROR_MESSAGE:
            logging.warning('The Blocked page user notification '
                            'messages, %s and %s are not displayed on '
                            'the blocked page. The messages may have '
                            'been modified. Please check and update the '
                            'messages in this file accordingly.',
                            self.BLOCKED_USER_MESSAGE,
                            self.BLOCKED_ERROR_MESSAGE)
        return True


    def _test_url_blacklist(self, policy_value):
        """
        Verify CrOS enforces URLBlacklist policy value.

        Navigate to all the websites in the ALL_URLS_LIST. Verify that
        the websites specified in the URLWhitelist policy value are allowed.
        Also verify that the websites not in the URLWhitelist policy value
        are blocked.

        @param policy_value: policy value expected.

        @raises: TestFail if url is blocked/not blocked based on the
                 corresponding policy values.

        """
        for url in self.ALL_URLS_LIST:
            url_is_blocked = self._is_url_blocked(url)
            if policy_value:
                should_be_blocked = (policy_value == self.BLACKLIST_WILDCARD or
                                     url in policy_value)
                if should_be_blocked and not url_is_blocked:
                    raise error.TestFail('The URL %s should have been blocked'
                                         ' by policy, but was allowed.' % url)
            elif url_is_blocked:
                raise error.TestFail('The URL %s should have been allowed'
                                     'by policy, but was blocked.' % url)


    def run_once(self, case):
        """
        Setup and run the test configured for the specified test case.

        @param case: Name of the test case to run.

        """
        case_value = self.TEST_CASES[case]
        self.SUPPORTING_POLICIES[self.POLICY_NAME] = case_value
        try:
            self.setup_case(user_policies=self.SUPPORTING_POLICIES)
        except EvaluateException:
            raise error.TestFail('chrome://policy was not whitelisted.')

        self._test_url_blacklist(case_value)
