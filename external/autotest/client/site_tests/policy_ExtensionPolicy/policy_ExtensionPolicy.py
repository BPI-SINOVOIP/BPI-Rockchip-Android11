# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import hashlib
import logging
import os

from autotest_lib.client.common_lib import error
from autotest_lib.client.cros.enterprise import enterprise_policy_base


class policy_ExtensionPolicy(enterprise_policy_base.EnterprisePolicyTest):
    version = 1


    def initialize(self, **kwargs):
        """
        Start webserver and set the extension policy file's path and checksum.

        """
        super(policy_ExtensionPolicy, self).initialize(**kwargs)
        self.start_webserver()

        # Location of the extension policy on the server.
        POLICY_FILE = 'extension_policy.json'
        policy_path = os.path.join(self.enterprise_dir, POLICY_FILE)
        self.EXTENSION_POLICY_URL = '%s/%s' % (self.WEB_HOST, POLICY_FILE)
        self.CHECKSUM = self.sha256sum(policy_path)


    def sha256sum(self, filepath):
        """
        Generate the SHA256 checksum of |filepath|.

        @param filepath: Path to file.

        @returns: SHA256 checksum as a hex string.

        """
        with open(filepath, 'rb') as f:
            return hashlib.sha256(f.read()).hexdigest()


    def run_once(self):
        """
        Setup and run the test configured for the specified test case.

        """
        extension_path = os.path.join(os.path.dirname(__file__),
                                      'policy_test_extension')

        self.setup_case(extension_paths=[extension_path])

        # The extension ID is required for setting the extension policy. But
        # the extension ID is assigned randomly, so we need to force install
        # the policy test extension first and then read its ID.
        extension_id = self.cr.get_extension(extension_path).extension_id
        extension_policies = {
            extension_id: {
                'download_url': self.EXTENSION_POLICY_URL,
                'secure_hash': self.CHECKSUM
            }
        }

        if self.dms_is_fake:
            # Update the server policies with the extension policies.
            self.add_policies(extension=extension_policies)

        # Ensure fields marked sensitive are censored in the policy tab.
        sensitive_fields = ['SensitiveStringPolicy', 'SensitiveDictPolicy']
        self.verify_extension_stats(extension_policies,
                                    sensitive_fields=sensitive_fields)
