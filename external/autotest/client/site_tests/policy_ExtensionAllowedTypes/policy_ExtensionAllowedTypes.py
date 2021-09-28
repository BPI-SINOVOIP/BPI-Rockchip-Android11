# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.enterprise import enterprise_policy_base
import utils


class policy_ExtensionAllowedTypes(
        enterprise_policy_base.EnterprisePolicyTest):
    """
    Test effect of policy_ExtensionAllowedTypes policy on Chrome OS.

    """
    version = 1

    result_lookup = {'theme': 'rgb(0, 0, 0)',
                     None: 'rgb(255, 255, 255)'}

    EXTENSION_ID = 'aghfnjkcakhmadgdomlmlhhaocbkloab'
    EXTENSION_PAGE = ('chrome://extensions/?id=%s'
                           % EXTENSION_ID)

    button_name = 'dd-Va g-c-wb g-eg-ua-Uc-c-za g-c-Oc-td-jb-oa g-c'
    button_obj = ("document.getElementsByClassName('{}')[0]"
                  .format(button_name))

    download_url = ('https://chrome.google.com/webstore/detail/just-black/{}'
                    .format(EXTENSION_ID))

    get_bg_rbg = "document.getElementsByTagName('body')[0].style.background"

    def _extension_check(self):
        """
        Checks if the allowed extension types works, or not by attempting to
        apply a theme to chrome, and checking if its installed or not, based
        off the case. This test will navigate to the chromestore page, and
        click the button to apply the theme. Then it will open a newtab and
        check the background rbg. If/when this test fails in the future, check
        button_name first, then the extension_id, as they are the most likely
        to be changed in the future and (currently) out of the tests control.

        """

        tab = self.navigate_to_url(self.download_url)
        self.wait_for_page_load(tab)
        self._download_app(tab)
        self.wait_for_extension(tab)

    def wait_for_page_load(self, tab):
        """
        Wait for the theme download page to load

        @param tab: tab obj, must have already navigated to the DOWNLOAD_URL.
        """

        def load_ext_page():
            return tab.EvaluateJavaScript(
                '{} !== undefined'.format(self.button_obj))

        utils.poll_for_condition(
            load_ext_page,
            exception=error.TestError('Page no load :('),
            timeout=15,
            sleep_interval=1,
            desc='Timed out waiting for extension to install.')

    def _download_app(self, tab):
        """Clicks the 'Add to Chrome' button."""
        tab.EvaluateJavaScript('{}.click()'.format(self.button_obj))

    def wait_for_extension(self, tab):
        """Wait for the extension to install so we can open it."""

        def load_page():
            tab = self.navigate_to_url('chrome://newtab')
            i_t = tab.EvaluateJavaScript(self.get_bg_rbg)
            return self.expected == i_t

        utils.poll_for_condition(
            load_page,
            exception=error.TestError('Theme not installed :('),
            timeout=15,
            sleep_interval=1,
            desc='Timed out waiting for extension to install.')

    def run_once(self, case):
        """
        Setup and run the test configured for the specified test case.

        @param case: Name of the test case to run.

        """
        self.expected = self.result_lookup[case]

        # Must allow the extension by default for autotest to work.
        extensions_allowed = ['extension']
        if case:
            extensions_allowed.append('theme')

        pol = {'ExtensionAllowedTypes': extensions_allowed}
        self.setup_case(user_policies=pol)
        self._extension_check()
