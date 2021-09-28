# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import re

from autotest_lib.client.common_lib.cros import arc_common
from autotest_lib.client.common_lib.cros import arc_util
from autotest_lib.client.common_lib.cros import assistant_util
from autotest_lib.client.cros import constants
from autotest_lib.client.bin import utils
from telemetry.core import cros_interface, exceptions
from telemetry.internal.browser import browser_finder, browser_options
from telemetry.internal.browser import extension_to_load

import py_utils

Error = exceptions.Error


def NormalizeEmail(username):
    """Remove dots from username. Add @gmail.com if necessary.

    TODO(achuith): Get rid of this when crbug.com/358427 is fixed.

    @param username: username/email to be scrubbed.
    """
    parts = re.split('@', username)
    parts[0] = re.sub('\.', '', parts[0])

    if len(parts) == 1:
        parts.append('gmail.com')
    return '@'.join(parts)


class Chrome(object):
    """Wrapper for creating a telemetry browser instance with extensions.

    The recommended way to use this class is to create the instance using the
    with statement:

    >>> with chrome.Chrome(...) as cr:
    >>>     # Do whatever you need with cr.
    >>>     pass

    This will make sure all the clean-up functions are called.  If you really
    need to use this class without the with statement, make sure to call the
    close() method once you're done with the Chrome instance.
    """

    BROWSER_TYPE_LOGIN = 'system'
    BROWSER_TYPE_GUEST = 'system-guest'
    AUTOTEST_EXT_ID = 'behllobkkfkfnphdnhnkndlbkcpglgmj'

    def __init__(self, logged_in=True, extension_paths=None, autotest_ext=False,
                 num_tries=3, extra_browser_args=None,
                 clear_enterprise_policy=True, expect_policy_fetch=False,
                 dont_override_profile=False, disable_gaia_services=True,
                 disable_default_apps=True, auto_login=True, gaia_login=False,
                 username=None, password=None, gaia_id=None,
                 arc_mode=None, arc_timeout=None,
                 disable_arc_opt_in=True,
                 disable_arc_opt_in_verification=True,
                 disable_arc_cpu_restriction=True,
                 disable_app_sync=False,
                 disable_play_auto_install=False,
                 disable_locale_sync=True,
                 disable_play_store_auto_update=True,
                 enable_assistant=False,
                 enterprise_arc_test=False,
                 init_network_controller=False,
                 mute_audio=False,
                 proxy_server=None,
                 login_delay=0):
        """
        Constructor of telemetry wrapper.

        @param logged_in: Regular user (True) or guest user (False).
        @param extension_paths: path of unpacked extension to install.
        @param autotest_ext: Load a component extension with privileges to
                             invoke chrome.autotestPrivate.
        @param num_tries: Number of attempts to log in.
        @param extra_browser_args: Additional argument(s) to pass to the
                                   browser. It can be a string or a list.
        @param clear_enterprise_policy: Clear enterprise policy before
                                        logging in.
        @param expect_policy_fetch: Expect that chrome can reach the device
                                    management server and download policy.
        @param dont_override_profile: Don't delete cryptohome before login.
                                      Telemetry will output a warning with this
                                      option.
        @param disable_gaia_services: For enterprise autotests, this option may
                                      be used to enable policy fetch.
        @param disable_default_apps: For tests that exercise default apps.
        @param auto_login: Does not login automatically if this is False.
                           Useful if you need to examine oobe.
        @param gaia_login: Logs in to real gaia.
        @param username: Log in using this username instead of the default.
        @param password: Log in using this password instead of the default.
        @param gaia_id: Log in using this gaia_id instead of the default.
        @param arc_mode: How ARC instance should be started.  Default is to not
                         start.
        @param arc_timeout: Timeout to wait for ARC to boot.
        @param disable_arc_opt_in: For opt in flow autotest. This option is used
                                   to disable the arc opt in flow.
        @param disable_arc_opt_in_verification:
             Adds --disable-arc-opt-in-verification to browser args. This should
             generally be enabled when disable_arc_opt_in is enabled. However,
             for data migration tests where user's home data is already set up
             with opted-in state before login, this option needs to be set to
             False with disable_arc_opt_in=True to make ARC container work.
        @param disable_arc_cpu_restriction:
             Adds --disable-arc-cpu-restriction to browser args. This is enabled
             by default and will make tests run faster and is generally
             desirable unless a test is actually trying to test performance
             where ARC is running in the background for some porition of the
             test.
        @param disable_app_sync:
            Adds --arc-disable-app-sync to browser args and this disables ARC
            app sync flow. By default it is enabled.
        @param disable_play_auto_install:
            Adds --arc-disable-play-auto-install to browser args and this
            disables ARC Play Auto Install flow. By default it is enabled.
        @param enable_assistant: For tests that require to enable Google
                                  Assistant service. Default is False.
        @param enterprise_arc_test: Skips opt_in causing enterprise tests to fail
        @param disable_locale_sync:
            Adds --arc-disable-locale-sync to browser args and this
            disables locale sync between Chrome and Android container. In case
            of disabling sync, Android container is started with language and
            preference language list as it was set on the moment of starting
            full instance. Used to prevent random app restarts caused by racy
            locale change, coming from profile sync. By default locale sync is
            disabled.
        @param disable_play_store_auto_update:
            Adds --arc-play-store-auto-update=off to browser args and this
            disables Play Store, GMS Core and third-party apps auto-update.
            By default auto-update is off to have stable autotest environment.
        @param mute_audio: Mute audio.
        @param proxy_server: To launch the chrome with --proxy-server
            Adds '--proxy-server="http://$HTTP_PROXY:PORT"' to browser args. By
            default proxy-server is disabled
        @param login_delay: Time for idle in login screen to simulate the time
                            required for password typing.
        """
        self._autotest_ext_path = None

        # Force autotest extension if we need enable Play Store.
        if (utils.is_arc_available() and (arc_util.should_start_arc(arc_mode)
                                          or not disable_arc_opt_in)):
            autotest_ext = True

        if extension_paths is None:
            extension_paths = []

        finder_options = browser_options.BrowserFinderOptions()
        if proxy_server:
            finder_options.browser_options.AppendExtraBrowserArgs(
                ['--proxy-server="%s"' % proxy_server])
        if utils.is_arc_available() and arc_util.should_start_arc(arc_mode):
            if disable_arc_opt_in and disable_arc_opt_in_verification:
                finder_options.browser_options.AppendExtraBrowserArgs(
                    ['--disable-arc-opt-in-verification'])
            if disable_arc_cpu_restriction:
                finder_options.browser_options.AppendExtraBrowserArgs(
                    ['--disable-arc-cpu-restriction'])
            if disable_app_sync:
                finder_options.browser_options.AppendExtraBrowserArgs(
                    ['--arc-disable-app-sync'])
            if disable_play_auto_install:
                finder_options.browser_options.AppendExtraBrowserArgs(
                    ['--arc-disable-play-auto-install'])
            if disable_locale_sync:
                finder_options.browser_options.AppendExtraBrowserArgs(
                    ['--arc-disable-locale-sync'])
            if disable_play_store_auto_update:
                finder_options.browser_options.AppendExtraBrowserArgs(
                    ['--arc-play-store-auto-update=off'])
            logged_in = True

        if autotest_ext:
            self._autotest_ext_path = os.path.join(os.path.dirname(__file__),
                                                   'autotest_private_ext')
            extension_paths.append(self._autotest_ext_path)
            finder_options.browser_options.AppendExtraBrowserArgs(
                ['--whitelisted-extension-id=%s' % self.AUTOTEST_EXT_ID])

        self._browser_type = (self.BROWSER_TYPE_LOGIN
                              if logged_in else self.BROWSER_TYPE_GUEST)
        finder_options.browser_type = self.browser_type
        if extra_browser_args:
            finder_options.browser_options.AppendExtraBrowserArgs(
                extra_browser_args)

        # finder options must be set before parse_args(), browser options must
        # be set before Create().
        # TODO(crbug.com/360890) Below MUST be '2' so that it doesn't inhibit
        # autotest debug logs
        finder_options.verbosity = 2
        finder_options.CreateParser().parse_args(args=[])
        b_options = finder_options.browser_options
        b_options.disable_component_extensions_with_background_pages = False
        b_options.create_browser_with_oobe = True
        b_options.clear_enterprise_policy = clear_enterprise_policy
        b_options.dont_override_profile = dont_override_profile
        b_options.disable_gaia_services = disable_gaia_services
        b_options.disable_default_apps = disable_default_apps
        b_options.disable_component_extensions_with_background_pages = disable_default_apps
        b_options.disable_background_networking = False
        b_options.expect_policy_fetch = expect_policy_fetch
        b_options.auto_login = auto_login
        b_options.gaia_login = gaia_login
        b_options.mute_audio = mute_audio
        b_options.login_delay = login_delay

        if utils.is_arc_available() and not disable_arc_opt_in:
            arc_util.set_browser_options_for_opt_in(b_options)

        self.username = b_options.username if username is None else username
        self.password = b_options.password if password is None else password
        self.username = NormalizeEmail(self.username)
        b_options.username = self.username
        b_options.password = self.password
        self.gaia_id = b_options.gaia_id if gaia_id is None else gaia_id
        b_options.gaia_id = self.gaia_id

        self.arc_mode = arc_mode

        if logged_in:
            extensions_to_load = b_options.extensions_to_load
            for path in extension_paths:
                extension = extension_to_load.ExtensionToLoad(
                    path, self.browser_type)
                extensions_to_load.append(extension)
            self._extensions_to_load = extensions_to_load

        # Turn on collection of Chrome coredumps via creation of a magic file.
        # (Without this, Chrome coredumps are trashed.)
        open(constants.CHROME_CORE_MAGIC_FILE, 'w').close()

        self._browser_to_create = browser_finder.FindBrowser(
            finder_options)
        self._browser_to_create.SetUpEnvironment(b_options)
        for i in range(num_tries):
            try:
                self._browser = self._browser_to_create.Create()
                self._browser_pid = \
                    cros_interface.CrOSInterface().GetChromePid()
                if utils.is_arc_available():
                    if disable_arc_opt_in:
                        if arc_util.should_start_arc(arc_mode):
                            arc_util.enable_play_store(self.autotest_ext, True)
                    else:
                        if not enterprise_arc_test:
                            wait_for_provisioning = \
                                arc_mode != arc_common.ARC_MODE_ENABLED_ASYNC
                            arc_util.opt_in(
                                browser=self.browser,
                                autotest_ext=self.autotest_ext,
                                wait_for_provisioning=wait_for_provisioning)
                    arc_util.post_processing_after_browser(self, arc_timeout)
                if enable_assistant:
                    assistant_util.enable_assistant(self.autotest_ext)
                break
            except exceptions.LoginException as e:
                logging.error('Timed out logging in, tries=%d, error=%s',
                              i, repr(e))
                if i == num_tries-1:
                    raise
        if init_network_controller:
            self._browser.platform.network_controller.Open()

    def __enter__(self):
        return self

    def __exit__(self, *args):
        # Turn off collection of Chrome coredumps turned on in init.
        if os.path.exists(constants.CHROME_CORE_MAGIC_FILE):
            os.remove(constants.CHROME_CORE_MAGIC_FILE)
        self.close()

    @property
    def browser(self):
        """Returns a telemetry browser instance."""
        return self._browser

    def get_extension(self, extension_path, retry=5):
        """Fetches a telemetry extension instance given the extension path."""
        def _has_ext(ext):
            """
            Return True if the extension is fully loaded.

            Sometimes an extension will be in the _extensions_to_load, but not
            be fully loaded, and will error when trying to fetch from
            self.browser.extensions. Happens most common when ARC is enabled.
            This will add a wait/retry.

            @param ext: the extension to look for
            @returns True if found, False if not.
            """
            try:
                return bool(self.browser.extensions[ext])
            except KeyError:
                return False

        for ext in self._extensions_to_load:
            if extension_path == ext.path:
                utils.poll_for_condition(lambda: _has_ext(ext),
                                         timeout=retry)
                return self.browser.extensions[ext]
        return None

    @property
    def autotest_ext(self):
        """Returns the autotest extension."""
        return self.get_extension(self._autotest_ext_path)

    @property
    def login_status(self):
        """Returns login status."""
        ext = self.autotest_ext
        if not ext:
            return None

        ext.ExecuteJavaScript('''
            window.__login_status = null;
            chrome.autotestPrivate.loginStatus(function(s) {
              window.__login_status = s;
            });
        ''')
        return utils.poll_for_condition(
            lambda: ext.EvaluateJavaScript('window.__login_status'),
            timeout=10)

    def disable_dim_display(self):
        """Avoid dim display.

        @returns True if success otherwise False.
        """
        ext = self.autotest_ext
        if not ext:
            return False
        try:
            ext.ExecuteJavaScript(
                    '''chrome.power.requestKeepAwake("display")''')
        except:
            logging.error("failed to disable dim display")
            return False
        return True

    def get_visible_notifications(self):
        """Returns an array of visible notifications of Chrome.

        For specific type of each notification, please refer to Chromium's
        chrome/common/extensions/api/autotest_private.idl.
        """
        ext = self.autotest_ext
        if not ext:
            return None

        ext.ExecuteJavaScript('''
            window.__items = null;
            chrome.autotestPrivate.getVisibleNotifications(function(items) {
              window.__items  = items;
            });
        ''')
        if ext.EvaluateJavaScript('window.__items') is None:
            return None
        return ext.EvaluateJavaScript('window.__items')

    @property
    def browser_type(self):
        """Returns the browser_type."""
        return self._browser_type

    @staticmethod
    def did_browser_crash(func):
        """Runs func, returns True if the browser crashed, False otherwise.

        @param func: function to run.

        """
        try:
            func()
        except Error:
            return True
        return False

    @staticmethod
    def wait_for_browser_restart(func, browser):
        """Runs func, and waits for a browser restart.

        @param func: function to run.

        """
        _cri = cros_interface.CrOSInterface()
        pid = _cri.GetChromePid()
        Chrome.did_browser_crash(func)
        utils.poll_for_condition(
            lambda: pid != _cri.GetChromePid(), timeout=60)
        browser.WaitForBrowserToComeUp()

    def wait_for_browser_to_come_up(self):
        """Waits for the browser to come up. This should only be called after a
        browser crash.
        """
        def _BrowserReady(cr):
            tabs = []  # Wrapper for pass by reference.
            if self.did_browser_crash(
                    lambda: tabs.append(cr.browser.tabs.New())):
                return False
            try:
                tabs[0].Close()
            except:
                # crbug.com/350941
                logging.error('Timed out closing tab')
            return True
        py_utils.WaitFor(lambda: _BrowserReady(self), timeout=10)

    def close(self):
        """Closes the browser.
        """
        try:
            if utils.is_arc_available():
                arc_util.pre_processing_before_close(self)
        finally:
            # Calling platform.StopAllLocalServers() to tear down the telemetry
            # server processes such as the one started by
            # platform.SetHTTPServerDirectories().  Not calling this function
            # will leak the process and may affect test results.
            # (crbug.com/663387)
            self._browser.platform.StopAllLocalServers()
            self._browser.Close()
            self._browser_to_create.CleanUpEnvironment()
            self._browser.platform.network_controller.Close()
