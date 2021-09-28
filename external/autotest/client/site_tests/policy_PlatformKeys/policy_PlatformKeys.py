# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import re
import utils

from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.enterprise import enterprise_policy_base


class policy_PlatformKeys(enterprise_policy_base.EnterprisePolicyTest):
    version = 1


    def initialize(self, **kwargs):
        """Set some global variables."""
        super(policy_PlatformKeys, self).initialize(**kwargs)
        # PlatformKeys extension ID.
        self.EXTENSION_ID = 'hoppbgdeajkagempifacalpdapphfoai'
        self.EXTENSION_PAGE = ('chrome-extension://%s/main.html'
                               % self.EXTENSION_ID)
        self.tab = None


    def click_button(self, id):
        """Click an element given its ID."""
        self.tab.ExecuteJavaScript(
                "document.querySelector('#%s').click()" % id)


    def field_value(self, id):
        """Return the value of a text field."""
        return self.tab.EvaluateJavaScript(
            "document.querySelector('#%s').value" % id)


    def call_api(self, button_id, field_id=None):
        """
        Call the API by clicking a button and checking its output fields.

        @param button_id: ID of the button element.
        @param field_id: Text field output is printed to (if any).

        @returns: Output of the call, if any.
        @raises error.TestFail: If the API call fails.

        """
        error_id = button_id + '-error'
        self.click_button(button_id)

        # Wait for the API to return 'OK' and raise an error if it doesn't.
        utils.poll_for_condition(
                lambda: 'OK' in self.field_value(error_id),
                timeout=5,
                exception=error.TestFail(
                    'API error: %s' % self.field_value(error_id)))

        if field_id:
            field = self.field_value(field_id)
            return field


    def create_certificate(self):
        """Return a certificate using the generated public key."""
        cert = self.call_api('create-cert', 'certificate')
        return cert.rstrip()


    def list_certificates(self):
        """Fetch all certificates and parse them into a list."""
        raw_certs = self.call_api('list-certs', 'certificates')

        if raw_certs:
            pattern = re.compile('-----BEGIN CERTIFICATE-----.*?'
                                 '-----END CERTIFICATE-----', flags=re.DOTALL)
            certs = re.findall(pattern, raw_certs)
        else:
            certs = []

        return certs


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


    def test_platform_keys(self):
        """
        Test the chrome.enterprise.platformKeys API.

        The following API methods are tested:
            - getToken
            - getCertificates
            - importCertificate
            - removeCertificate

        """
        self.wait_for_extension()

        if self.list_certificates():
            raise error.TestFail('Certificates list should be empty at start.')

        public_key = self.call_api('generate', 'public-key')

        certificate = self.create_certificate()
        self.call_api('import-cert')

        installed_certs = self.list_certificates()
        if len(installed_certs) != 1:
            raise error.TestFail('There should be 1 certificate instead of %s.'
                                 % len(installed_certs))

        if installed_certs[0] != certificate:
            raise error.TestFail('Installed certificate does not match '
                                 'expected certificate. %s != %s' %
                                 (installed_certs[0], certificate))

        self.call_api('remove-cert')

        if self.list_certificates():
            raise error.TestFail('All certificates should have been removed '
                                 'at the end of the test.')


    def run_once(self):
        """Setup and run the test configured for the specified test case."""
        self.setup_case(user_policies={
            'ExtensionInstallForcelist': [self.EXTENSION_ID],
            'DeveloperToolsAvailability': 1
        })

        self.test_platform_keys()
