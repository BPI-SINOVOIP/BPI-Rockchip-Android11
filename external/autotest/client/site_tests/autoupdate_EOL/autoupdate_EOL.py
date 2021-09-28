# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import datetime

from autotest_lib.client.bin import utils
from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib.cros import chrome
from autotest_lib.client.cros.update_engine import nebraska_wrapper
from autotest_lib.client.cros.update_engine import update_engine_test

class autoupdate_EOL(update_engine_test.UpdateEngineTest):
    """Tests end of life (EOL) behaviour."""
    version = 1

    _EXPECTED_FINAL_TITLE = 'Final software update'
    _DAYS_BEFORE_EOL_START_WARNING = 180
    # Value within {} expected to be number of days since epoch.
    _EXPECTED_EOL_DATE_TEMPLATE = 'EOL_DATE={}'
    # Value within {} expected to be the month and year.
    _EXPECTED_WARNING_TITLE = 'Updates end {}'
    _UNIX_EPOCH = datetime.datetime(1970, 1, 1)

    def cleanup(self):
        self._save_extra_update_engine_logs()
        super(autoupdate_EOL, self).cleanup()


    def _get_expected_eol_date(self, eol_date):
        """Figure out the expected eol date."""
        return self._UNIX_EPOCH + datetime.timedelta(eol_date)


    def _check_eol_info(self):
        """Checks update_engines eol status."""
        result = utils.run('update_engine_client --eol_status').stdout.strip()
        if self._EXPECTED_EOL_DATE not in result:
            raise error.TestFail('Expected date %s. Actual: %s' %
                                 (self._EXPECTED_EOL_DATE, result))


    def _check_eol_notification(self, eol_date):
        """Checks that we are showing an EOL notification to the user."""
        expected_eol_date = self._get_expected_eol_date(eol_date)
        expected_warning_begins_date = (expected_eol_date
                                        - datetime.timedelta(
                                          self._DAYS_BEFORE_EOL_START_WARNING))

        expected_final_title = self._EXPECTED_FINAL_TITLE
        expected_warning_title = (self._EXPECTED_WARNING_TITLE.
            format(expected_eol_date.strftime("%B %Y")))

        def find_notification(expected_title):
            """Helper to find notification."""
            notifications = self._cr.get_visible_notifications()
            return any([n['title'] == expected_title
                        for n in (notifications or [])])

        def check_eol_notifications():
            """Checks if correct notification is shown."""
            final_notification = find_notification(expected_final_title)
            warning_notification = find_notification(expected_warning_title)

            now = datetime.datetime.utcnow()
            if expected_eol_date <= now:
                return final_notification and not warning_notification
            elif expected_warning_begins_date <= now:
                return not final_notification and warning_notification
            return not final_notification and not warning_notification

        utils.poll_for_condition(condition=lambda: check_eol_notifications(),
                                 desc='End of Life Notification UI passed',
                                 timeout=5, sleep_interval=1)


    def _check_eol_settings(self, eol_date):
        """Check that the messages about EOL in Settings are correct."""
        tab = self._cr.browser.tabs[0]
        tab.Navigate('chrome://os-settings/help/details')
        tab.WaitForDocumentReadyStateToBeComplete()
        eol_js = '''
            settings.AboutPageBrowserProxyImpl.getInstance().getEndOfLifeInfo()
        '''
        eol_promise = tab.EvaluateJavaScript(eol_js, promise=True)
        expected_eol_date = self._get_expected_eol_date(eol_date)
        eol_msg = ('This device will get automatic software and security '
                   'updates until')
        if expected_eol_date <= datetime.datetime.utcnow():
            eol_msg = ('This device stopped getting automatic software and '
                       'security updates in')
        if eol_msg not in eol_promise['aboutPageEndOfLifeMessage']:
            raise error.TestFail('"%s" not found in Settings.' % eol_msg)


    def run_once(self, eol_date):
        """
        Checks that DUT behaves correctly in EOL scenarios.

        @param eol_date: the days from epoch value passed along to
                         NanoOmahaDevServer placed within the _eol_date tag
                         in the Omaha response.

        """
        self._EXPECTED_EOL_DATE = \
            self._EXPECTED_EOL_DATE_TEMPLATE.format(eol_date)

        # Start a Nebraska server to return a response with eol entry.
        with nebraska_wrapper.NebraskaWrapper(
                log_dir=self.resultsdir) as nebraska:
            # Try to update. It should fail with noupdate.
            self._check_for_update(port=nebraska.get_port(), ignore_status=True,
                                   wait_for_completion=True, eol_date=eol_date,
                                   no_update=True)

            self._check_eol_info()
            with chrome.Chrome(autotest_ext=True, logged_in=True) as cr:
                self._cr = cr
                self._check_eol_notification(eol_date)
                self._check_eol_settings(eol_date)

