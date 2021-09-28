# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from autotest_lib.server.cros.update_engine import update_engine_test


class autoupdate_CatchBadSignatures(update_engine_test.UpdateEngineTest):
    """Test to verify that update_engine correctly checks payload signatures."""
    version = 1

    # The test image to use and the values associated with it.
    _IMAGE_GS_URL='https://storage.googleapis.com/chromiumos-test-assets-public/autoupdate/autoupdate_CatchBadSignatures.bin'
    _IMAGE_PUBLIC_KEY2='LS0tLS1CRUdJTiBQVUJMSUMgS0VZLS0tLS0KTUlJQklqQU5CZ2txaGtpRzl3MEJBUUVGQUFPQ0FROEFNSUlCQ2dLQ0FRRUFxZE03Z25kNDNjV2ZRenlydDE2UQpESEUrVDB5eGcxOE9aTys5c2M4aldwakMxekZ0b01Gb2tFU2l1OVRMVXArS1VDMjc0ZitEeElnQWZTQ082VTVECkpGUlBYVXp2ZTF2YVhZZnFsalVCeGMrSlljR2RkNlBDVWw0QXA5ZjAyRGhrckduZi9ya0hPQ0VoRk5wbTUzZG8Kdlo5QTZRNUtCZmNnMUhlUTA4OG9wVmNlUUd0VW1MK2JPTnE1dEx2TkZMVVUwUnUwQW00QURKOFhtdzRycHZxdgptWEphRm1WdWYvR3g3K1RPbmFKdlpUZU9POUFKSzZxNlY4RTcrWlppTUljNUY0RU9zNUFYL2xaZk5PM1JWZ0cyCk83RGh6emErbk96SjNaSkdLNVI0V3daZHVobjlRUllvZ1lQQjBjNjI4NzhxWHBmMkJuM05wVVBpOENmL1JMTU0KbVFJREFRQUIKLS0tLS1FTkQgUFVCTElDIEtFWS0tLS0tCg=='

    def _check_signature(self, expected_log_messages,
                         failure_message, public_key=None):
        """Helper function for updating with a Canned Omaha response.

        @param expected_log_messages: A list of strings that are expected to be
             in the update_engine log.
        @param failure_message: The message for exception to raise on error.
        @param public_key: The public key to be passed to the update_engine.

        """

        # Runs the update on the DUT and expect it to fail.
        self._run_client_test_and_check_result('autoupdate_CannedOmahaUpdate',
                                               image_url=self._IMAGE_GS_URL,
                                               allow_failure=True,
                                               public_key=public_key)

        self._check_update_engine_log_for_entry(expected_log_messages,
                                                raise_error=True,
                                                err_str=failure_message)


    def _check_bad_metadata_signature(self):
        """Checks that update_engine rejects updates where the payload
        and Omaha response do not agree on the metadata signature."""

        expected_log_messages = [
                'Mandating payload hash checks since Omaha Response for '
                'unofficial build includes public RSA key',
                'Mandatory metadata signature validation failed']

        self._check_signature(expected_log_messages,
                              'Check for bad metadata signature failed.',
                              public_key=self._IMAGE_PUBLIC_KEY2)


    def _check_bad_payload_signature(self):
        """Checks that update_engine rejects updates where the payload
        signature does not match what is expected."""

        expected_log_messages = [
                'Mandating payload hash checks since Omaha Response for '
                'unofficial build includes public RSA key',
                'Metadata hash signature matches value in Omaha response.',
                'Public key verification failed, thus update failed']

        self._check_signature(expected_log_messages,
                              'Check for payload signature failed.')


    def run_once(self):
        """Runs the test on a DUT."""

        self._check_bad_metadata_signature()
        self._check_bad_payload_signature()
