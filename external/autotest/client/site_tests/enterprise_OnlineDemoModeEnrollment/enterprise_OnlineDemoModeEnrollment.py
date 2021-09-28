# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from autotest_lib.client.bin import test, utils
from autotest_lib.client.common_lib.cros import chrome, enrollment
from telemetry.core import exceptions

# Id of the Infinite Painter app.
_INFINITE_PAINTER_APP_ID = 'afihfgfghkmdmggakhkgnfhlikhdpima'

def _launch_arc_app(autotest_ext, app_id):
    try:
        autotest_ext.ExecuteJavaScript('''
            chrome.autotestPrivate.launchArcApp(
              '%s', /* app_id */
              '%s', /* intent */
              function(app_launched) {
                window.__app_launched = app_launched;
            });
        ''' % (app_id, 'intent'))
        return autotest_ext.EvaluateJavaScript('window.__app_launched')
    except exceptions.EvaluateException as e:
        pass
    return False

class enterprise_OnlineDemoModeEnrollment(test.test):
    """Enrolls to online demo mode."""
    version = 1


    def run_once(self):
        """Starts online demo mode enrollment. Waits for active session to start
           and launch an arc app.
        """
        with chrome.Chrome(
                auto_login=False,
                disable_gaia_services=False,
                autotest_ext=True,
                extra_browser_args='--force-devtools-available') as cr:
            enrollment.OnlineDemoMode(cr.browser)
            utils.poll_for_condition(
                    condition=lambda: _launch_arc_app(cr.autotest_ext,
                            _INFINITE_PAINTER_APP_ID),
                    desc='Launching the app %s' %
                            _INFINITE_PAINTER_APP_ID,
                    timeout=300,
                    sleep_interval=1)