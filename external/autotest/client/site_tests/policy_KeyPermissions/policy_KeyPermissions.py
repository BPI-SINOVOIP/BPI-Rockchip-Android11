# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import utils

from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.enterprise import enterprise_policy_base


class policy_KeyPermissions(
        enterprise_policy_base.EnterprisePolicyTest):
    """
    Test effect of policy_KeyPermissions/allowCorporateKeyUsage policy.

    This test will utilize the certs api extension to prep the certificate
    and see if when the cert is 'selected' if the Certficate information GUI
    object appears.

    The wait_for_ui_obj and did_obj_not_load functions within self.ui will
    raise testErrors if:
        The object did not load in the wait_for_ui_obj() call.
        The object did load in the  did_obj_not_load() call.

    """
    version = 1
    QUERY = "document.querySelector('#{}').{}"

    def click_button(self, button_id):
        """Click an element given its ID."""
        cmd = self.QUERY.format(button_id, 'click()')
        self.tab.ExecuteJavaScript(cmd)

    def field_value(self, obj_id):
        """Return the value of a text field."""
        cmd = self.QUERY.format(obj_id, 'value')
        return self.tab.EvaluateJavaScript(cmd)

    def wait_for_extension(self):
        """Wait for the extension to install so we can open it."""
        def load_page():
            self.tab = self.navigate_to_url(self.EXTENSION_PAGE, self.tab)
            return self.tab.EvaluateJavaScript(
                "document.querySelector('#cert-enrollment') !== null")

        utils.poll_for_condition(
            load_page,
            timeout=15,
            sleep_interval=1,
            desc='Timed out waiting for extension to install.')

    def test_platform_keys(self, case):
        """
        Test the chrome.enterprise.platformKeys API.

        The following API methods are tested:
            - getToken
            - getCertificates
            - importCertificate
            - removeCertificate

        """
        try:
            self._click_generate()
        except error.TestFail:
            # On specific devices the very first time the extension page is
            # loaded, it will not load the certs. This is a small workaround to
            # fix that.
            self.navigate_to_url('https://google.com', self.tab)
            self._click_generate()

        # Click all the buttons needed to get the cert ready.
        self.click_button('sign')
        self.click_button('create-cert')
        self.click_button('import-cert')
        self.click_button('list-certs')
        self.click_button('select-certs')

        # Test if the cert was allowed.
        if case:
            self.ui.wait_for_ui_obj('Certificate information')
        else:
            self.ui.did_obj_not_load('Certificate information')

    def _click_generate(self):
        self.wait_for_extension()

        self.click_button('generate')
        error_id = 'generate-error'

        utils.poll_for_condition(
            lambda: 'OK' in self.field_value(error_id),
            timeout=45,
            exception=error.TestFail(
                'API error: %s' % self.field_value(error_id)))

    def run_once(self, case=None):
        """Setup and run the test configured for the specified test case."""

        EXTENSION_ID = 'hoppbgdeajkagempifacalpdapphfoai'
        self.EXTENSION_PAGE = ('chrome-extension://%s/main.html'
                               % EXTENSION_ID)
        self.tab = None

        self.setup_case(
            disable_default_apps=False,
            user_policies={
                'ExtensionInstallForcelist': [EXTENSION_ID],
                'DeveloperToolsAvailability': 1,
                'KeyPermissions':
                    {'hoppbgdeajkagempifacalpdapphfoai':
                        {'allowCorporateKeyUsage': case}}})
        self.ui.start_ui_root(self.cr)
        self.test_platform_keys(case)
