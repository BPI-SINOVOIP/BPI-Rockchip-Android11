# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import os
import time

from autotest_lib.client.common_lib.cros import arc

from autotest_lib.client.common_lib.cros.arc import is_android_container_alive

from autotest_lib.client.bin import test
from autotest_lib.client.common_lib import error
from autotest_lib.client.common_lib.cros import chrome
from autotest_lib.client.common_lib.cros import enrollment
from autotest_lib.client.common_lib.cros import policy
from autotest_lib.client.cros import cryptohome
from autotest_lib.client.cros import httpd
from autotest_lib.client.cros.input_playback import keyboard
from autotest_lib.client.cros.enterprise import enterprise_policy_utils
from autotest_lib.client.cros.enterprise import policy_manager
from autotest_lib.client.cros.enterprise import enterprise_fake_dmserver
from autotest_lib.client.common_lib import ui_utils

from telemetry.core import exceptions

CROSQA_FLAGS = [
    '--gaia-url=https://gaiastaging.corp.google.com',
    '--lso-url=https://gaiastaging.corp.google.com',
    '--google-apis-url=https://www-googleapis-test.sandbox.google.com',
    '--oauth2-client-id=236834563817.apps.googleusercontent.com',
    '--oauth2-client-secret=RsKv5AwFKSzNgE0yjnurkPVI',
    ('--cloud-print-url='
     'https://cloudprint-nightly-ps.sandbox.google.com/cloudprint'),
    '--ignore-urlfetcher-cert-requests']
CROSALPHA_FLAGS = [
    ('--cloud-print-url='
     'https://cloudprint-nightly-ps.sandbox.google.com/cloudprint'),
    '--ignore-urlfetcher-cert-requests']
TESTDMS_FLAGS = [
    '--ignore-urlfetcher-cert-requests',
    '--disable-policy-key-verification']
FLAGS_DICT = {
    'prod': [],
    'crosman-qa': CROSQA_FLAGS,
    'crosman-alpha': CROSALPHA_FLAGS,
    'dm-test': TESTDMS_FLAGS,
    'dm-fake': TESTDMS_FLAGS
}
DMS_URL_DICT = {
    'prod': 'http://m.google.com/devicemanagement/data/api',
    'crosman-qa':
        'https://crosman-qa.sandbox.google.com/devicemanagement/data/api',
    'crosman-alpha':
        'https://crosman-alpha.sandbox.google.com/devicemanagement/data/api',
    'dm-test': 'http://chromium-dm-test.appspot.com/d/%s',
    'dm-fake': 'http://127.0.0.1:%d/'
}
DMSERVER = '--device-management-url=%s'
# Username and password for the fake dm server can be anything, since
# they are not used to authenticate against GAIA.
USERNAME = 'fake-user@managedchrome.com'
PASSWORD = 'fakepassword'
GAIA_ID = 'fake-gaia-id'


class EnterprisePolicyTest(arc.ArcTest, test.test):
    """Base class for Enterprise Policy Tests."""

    WEB_PORT = 8080
    WEB_HOST = 'http://localhost:%d' % WEB_PORT
    CHROME_POLICY_PAGE = 'chrome://policy'
    CHROME_VERSION_PAGE = 'chrome://version'

    def initialize(self, **kwargs):
        """
        Initialize test parameters.

        Consume the check_client_result parameter if this test was started
        from a server test.

        """
        self._initialize_enterprise_policy_test(**kwargs)

    def _initialize_enterprise_policy_test(
            self, env='dm-fake', dms_name=None,
            username=USERNAME, password=PASSWORD, gaia_id=GAIA_ID,
            set_auto_logout=None, **kwargs):
        """
        Initialize test parameters and fake DM Server.

        This function exists so that ARC++ tests (which inherit from the
        ArcTest class) can also initialize a policy setup.

        @param env: String environment of DMS and Gaia servers.
            If wanting to point to a 'real' server use 'dm-test'
        @param username: String user name login credential.
        @param password: String password login credential.
        @param gaia_id: String gaia_id login credential.
        @param dms_name: String name of test DM Server.
        @param kwargs: Not used.

        """
        self.env = env
        self.username = username
        self.password = password
        self.gaia_id = gaia_id
        self.set_auto_logout = set_auto_logout
        self.dms_name = dms_name
        self.dms_is_fake = (env == 'dm-fake')
        self.arc_enabled = False
        self._enforce_variable_restrictions()

        # Install protobufs and add import path.
        policy.install_protobufs(self.autodir, self.job)

        # Initialize later variables to prevent error after an early failure.
        self._web_server = None
        self.cr = None

        # Start AutoTest DM Server if using local fake server.
        if self.dms_is_fake:
            self.fake_dm_server = enterprise_fake_dmserver.FakeDMServer()
            self.fake_dm_server.start(self.tmpdir, self.debugdir)

        # Get enterprise directory of shared resources.
        client_dir = os.path.dirname(os.path.dirname(self.bindir))
        self.enterprise_dir = os.path.join(client_dir, 'cros/enterprise')

        if self.set_auto_logout is not None:
            self._auto_logout = self.set_auto_logout

        # Log the test context parameters.
        logging.info('Test Context Parameters:')
        logging.info('  Environment: %r', self.env)
        logging.info('  Username: %r', self.username)
        logging.info('  Password: %r', self.password)
        logging.info('  Test DMS Name: %r', self.dms_name)

    def check_page_readiness(self, tab, command):
        """
        Check to see if page has fully loaded.

        @param tab: chrome tab loading the page.
        @param command: JS command to be checked in the tab.

        @returns True if loaded and False if not.

        """
        try:
            tab.EvaluateJavaScript(command)
            return True
        except exceptions.EvaluateException:
            return False

    def cleanup(self):
        """Close out anything used by this test."""
        # Clean up AutoTest DM Server if using local fake server.
        if self.dms_is_fake:
            self.fake_dm_server.stop()

        # Stop web server if it was started.
        if self._web_server:
            self._web_server.stop()

        # Close Chrome instance if opened.
        if self.cr and self._auto_logout:
            self.cr.close()

        # Cleanup the ARC test if needed.
        if self.arc_enabled:
            super(EnterprisePolicyTest, self).cleanup()

    def start_webserver(self):
        """Set up HTTP Server to serve pages from enterprise directory."""
        self._web_server = httpd.HTTPListener(
            self.WEB_PORT, docroot=self.enterprise_dir)
        self._web_server.run()

    def _enforce_variable_restrictions(self):
        """Validate class-level test context parameters.

        @raises error.TestError if context parameter has an invalid value,
                or a combination of parameters have incompatible values.
        """
        # Verify |env| is a valid environment.
        if self.env not in FLAGS_DICT:
            raise error.TestError('Environment is invalid: %s' % self.env)

        # Verify test |dms_name| is given if |env| is 'dm-test'.
        if self.env == 'dm-test' and not self.dms_name:
            raise error.TestError('dms_name must be given when using '
                                  'env=dm-test.')
        if self.env != 'dm-test' and self.dms_name:
            raise error.TestError('dms_name must not be given when not using '
                                  'env=dm-test.')
        if self.env == 'prod' and self.username == USERNAME:
            raise error.TestError('Cannot user real DMS server when using '
                                  'a fake test account.')

    def setup_case(self,
                   user_policies={},
                   suggested_user_policies={},
                   device_policies={},
                   extension_policies={},
                   skip_policy_value_verification=False,
                   kiosk_mode=False,
                   enroll=False,
                   auto_login=True,
                   auto_logout=True,
                   init_network_controller=False,
                   arc_mode=None,
                   setup_arc=True,
                   use_clouddpc_test=None,
                   disable_default_apps=True,
                   real_gaia=False,
                   extension_paths=[],
                   extra_chrome_flags=[]):
        """Set up DMS, log in, and verify policy values.

        If the AutoTest fake DM Server is used, make a JSON policy blob
        and upload it to the fake DM server.

        Launch Chrome and sign in to Chrome OS. Examine the user's
        cryptohome vault, to confirm user is signed in successfully.

        @param user_policies: dict of mandatory user policies in
                name -> value format.
        @param suggested_user_policies: optional dict of suggested policies
                in name -> value format.
        @param device_policies: dict of device policies in
                name -> value format.
        @param extension_policies: dict of extension policies.
        @param skip_policy_value_verification: True if setup_case should not
                verify that the correct policy value shows on policy page.
        @param enroll: True for enrollment instead of login.
        @param auto_login: Sign in to chromeos.
        @param auto_logout: Sign out of chromeos when test is complete.
        @param init_network_controller: whether to init network controller.
        @param arc_mode: whether to enable arc_mode on chrome.chrome().
        @param setup_arc: whether to run setup_arc in arc.Arctest.
        @param use_clouddpc_test: whether to run the cloud dpc test.
        @param extension_paths: list of extensions to install.
        @param extra_chrome_flags: list of flags to add to Chrome.

        @raises error.TestError if cryptohome vault is not mounted for user.
        @raises error.TestFail if |policy_name| and |policy_value| are not
                shown on the Policies page.
        """

        # Need a real account, for now. Note: Even though the account is 'real'
        # you can still use a fake DM server.

        if (arc_mode and self.username == USERNAME) or real_gaia:
            self.username = 'tester50@managedchrome.com'
            self.password = 'Test0000'

        self.pol_manager = policy_manager.Policy_Manager(
            self.username,
            self.fake_dm_server if hasattr(self, "fake_dm_server") else None)

        self._auto_logout = auto_logout
        self._kiosk_mode = kiosk_mode

        self.pol_manager.configure_policies(user_policies,
                                            suggested_user_policies,
                                            device_policies,
                                            extension_policies)

        self._create_chrome(enroll=enroll,
                            auto_login=auto_login,
                            init_network_controller=init_network_controller,
                            extension_paths=extension_paths,
                            arc_mode=arc_mode,
                            disable_default_apps=disable_default_apps,
                            real_gaia=real_gaia,
                            extra_chrome_flags=extra_chrome_flags)

        self.pol_manager.obtain_policies_from_device(self.cr.autotest_ext)

        # Skip policy check upon request or if we enroll but don't log in.
        if not skip_policy_value_verification and auto_login:
            self.pol_manager.verify_policies()

        if arc_mode:
            self.start_arc(use_clouddpc_test, setup_arc)

    def update_DM_Info(self):
        """Force an update of the DM server from current policies."""
        if not self.pol_manager.fake_dm_server:
            raise error.TestError('Cannot autoupdate DM sever without the fake'
                                  ' DM Sever being configured')
        self.pol_manager.updateDMServer()

    def enable_Fake_DM_autoupdate(self):
        """Enable automatic DM server updates on policy change."""
        self.pol_manager.auto_updateDM = True

    def disable_Fake_DM_autoupdate(self):
        """Disable automatic DM server updates on policy change."""
        self.pol_manager.auto_updateDM = False

    def remove_policy(self,
                      name,
                      policy_type,
                      extID=None,
                      verify_policies=True):
        """
        Remove a policy from the currently configured policies.

        @param name: str, the name of the policy
        @param policy_type: str, the policy type. Must "extension" for
            extension policies, other for user/device/suggested
        @param ExtID: str, ID of extension to remove the policy from, if
            policy_type is extension
        @param verify_policies: bool, re-verify policies after removal of the
            specified policy

        """
        self.pol_manager.remove_policy(name, policy_type, extID)
        self.reload_policies(verify_policies)
        if verify_policies:
            self.pol_manager.verify_policies()

    def start_arc(self, use_clouddpc_test, setup_arc):
        """
        Start ARC when creating the chrome object.

        Specifically will create the ADB shell container for testing use.

        We are NOT going to use the arc class initialize, it overwrites the
        creation of chrome.chrome() in a way which cannot support the DM sever.

        Instead we check for the android container, and run arc_setup if
        needed. Note: To use the cloud dpc test, you MUST also run setup_arc

        @param setup_arc: whether to run setup_arc in arc.Arctest.
        @param use_clouddpc_test: bool, run_clouddpc_test() or not.

        """
        _APP_FILENAME = 'autotest-deps-cloudpctest-0.4.apk'
        _DEP_PACKAGE = 'CloudDPCTest-apks'
        _PKG_NAME = 'com.google.android.apps.work.clouddpc.e2etests'

        # By default on boot the container is alive, and will not close until
        # a user with ARC disabled logs in. This wait accounts for that.
        time.sleep(3)

        if use_clouddpc_test and not setup_arc:
            raise error.TestFail('For cloud DPC setup_arc cannot be disabled')

        if is_android_container_alive():
            logging.info('Android Container is alive!')
        else:
            logging.error('Android Container is not alive!')

        # Install the clouddpc test.
        if use_clouddpc_test:
            self.arc_setup(dep_packages=_DEP_PACKAGE,
                           apks=[_APP_FILENAME],
                           full_pkg_names=[_PKG_NAME])
            self.run_clouddpc_test()
        else:
            if setup_arc:
                self.arc_setup()

        self.arc_enabled = True

    def run_clouddpc_test(self):
        """
        Run clouddpc end-to-end test and fail this test if it fails.

        Assumes start_arc() was run with use_clouddpc_test.

        Determines the policy values to pass to the test from those set in
        Chrome OS.

        @raises error.TestFail if the test does not pass.

        """
        policy_blob = self.pol_manager.getCloudDpc()
        policy_blob_str = json.dumps(policy_blob, separators=(',', ':'))
        cmd = ('am instrument -w -e policy "%s" '
               'com.google.android.apps.work.clouddpc.e2etests/'
               '.ArcInstrumentationTestRunner') % policy_blob_str

        # Run the command as a shell script so that its length is not capped.
        temp_shell_script_path = '/sdcard/tmp.sh'
        arc.write_android_file(temp_shell_script_path, cmd)

        logging.info('Running clouddpc test with policy: %s', policy_blob_str)
        results = arc.adb_shell('sh ' + temp_shell_script_path,
                                ignore_status=True).strip()
        arc.remove_android_file(temp_shell_script_path)
        if results.find('FAILURES!!!') >= 0:
            logging.info('CloudDPC E2E Results:\n%s', results)
            err_msg = results.splitlines()[-1]
            raise error.TestFail('CloudDPC E2E failure: %s' % err_msg)

        logging.debug(results)
        logging.info('CloudDPC E2E test passed!')

    def _get_policy_value_from_new_tab(self, policy):
        """
        TO BE DEPRICATED DO NOT USE.

        Gets the value of the policy.

        """
        return self.pol_manager.get_policy_value_from_DUT(policy)

    def add_policies(self,
                     user={},
                     suggested_user={},
                     device={},
                     extension={},
                     new=False):
        """Add policies to the policy rules."""
        self.pol_manager.configure_policies(user=user,
                                            suggested_user=suggested_user,
                                            device=device,
                                            extension=extension,
                                            new=new)
        self.reload_policies(True)

    def update_policies(self, user_policies={}, suggested_user_policies={},
                        device_policies={}, extension_policies={}):
        """
        Change the policies to the ones provided.

        NOTE: This will override any current policies configured.

        @param user_policies: mandatory user policies -> values.
        @param suggested user_policies: suggested user policies -> values.
        @param device_policies: mandatory device policies -> values.
        @param extension_policies: extension policies.

        """
        self.add_policies(user_policies, suggested_user_policies,
                          device_policies, extension_policies, True)

    def reload_policies(self, wait_for_new=False):
        """
        Force a policy fetch.

        @param wait_for_new: bool, wait up to 1 second for the policy values
            from the API to update

        """
        enterprise_policy_utils.refresh_policies(self.cr.autotest_ext,
                                                 wait_for_new)

    def verify_extension_stats(self, extension_policies, sensitive_fields=[]):
        """
        Verify the extension policies match what is on chrome://policy.

        @param extension_policies: the dictionary of extension IDs mapping
            to download_url and secure_hash.
        @param sensitive_fields: list of fields that should have their value
            censored.
        @raises error.TestError: if the shown values do not match what we are
            expecting.

        """
        # Refetch the policies from the DUT
        self.pol_manager.obtain_policies_from_device()

        # For this method we are expecting the extension policies to be living
        # within the chrome file system.
        for ext_id in extension_policies.keys():
            filePath = extension_policies[ext_id]['download_url']
            actual_extension_policies = (
                self._get_extension_policies_from_file(filePath))

            # Obfuscate and strip the extra dict level
            self._configure_extension_file_policies(actual_extension_policies,
                                                    sensitive_fields)

            self.pol_manager.configure_extension_visual_policy(
                {ext_id: actual_extension_policies})

            self.pol_manager.verify_policies()

    def _configure_extension_file_policies(self, policies, sensitive_fields):
        """
        Given policies, change sensitive_fields to "********".

        @param policies: dict, the extension policies as loaded from the file.
        @param sensitive_fields: list or set

        """
        for policy_name, value in policies.items():
            if policy_name in sensitive_fields:
                policies[policy_name] = '*' * 8
            else:
                if 'Value' in value:
                    policies[policy_name] = value['Value']

    def _get_extension_policies_from_file(self, download_url):
        """
        Get the configured extension policies from file system.

        Extension tests will store the "actual" extensions as a file within
        ChromeOS. Open the file within the enterprise_dir, and return the
        policies from the file.

        @param download_url: URL of the download location

        """
        policy_file = os.path.join(self.enterprise_dir,
                                   download_url.split('/')[-1])

        with open(policy_file) as f:
            policies = json.load(f)
        return policies

    def verify_policy_value(self,
                            policy_name,
                            expected_value,
                            extensionID=None):
        """
        Verify the value of a single policy.

        Note: This will not format the expected value. (Ie it will not
        automatically change a password/sensitive field to ********)

        @param policy_name: the policy we are checking.
        @param expected_value: the expected value for policy_name.

        @raises error.TestError if value does not match expected.

        """
        self.pol_manager.verify_policy(policy_name,
                                       expected_value,
                                       extensionID)

    def _initialize_chrome_extra_flags(self):
        """
        Initialize flags used to create Chrome instance.

        @returns: list of extra Chrome flags.

        """
        # Construct DM Server URL flags if not using production server.
        env_flag_list = []
        if self.env != 'prod':
            if self.dms_is_fake:
                # Use URL provided by the fake AutoTest DM server.
                dmserver_str = (DMSERVER % self.fake_dm_server.server_url)
            else:
                # Use URL defined in the DMS URL dictionary.
                dmserver_str = (DMSERVER % (DMS_URL_DICT[self.env]))
                if self.env == 'dm-test':
                    dmserver_str = (dmserver_str % self.dms_name)

            # Merge with other flags needed by non-prod enviornment.
            env_flag_list = ([dmserver_str] + FLAGS_DICT[self.env])

        return env_flag_list

    def _enterprise_enroll(self, extra_flags, extension_paths, auto_login):
        """
        Enroll a device, configure self.cr, optionally login.

        @param extra_chrome_flags: list of flags to add.
        @param extension_paths: list of extensions to install.
        @param auto_login: sign in to chromeos.

        """
        self.cr = chrome.Chrome(
            auto_login=False,
            autotest_ext=True,
            extra_browser_args=extra_flags,
            extension_paths=extension_paths,
            expect_policy_fetch=True)

        if self.dms_is_fake:
            if self._kiosk_mode:
                enrollment.KioskEnrollment(
                    self.cr.browser, self.username, self.password,
                    self.gaia_id)
            else:
                enrollment.EnterpriseFakeEnrollment(
                    self.cr.browser, self.username, self.password,
                    self.gaia_id, auto_login=auto_login)
        else:
            enrollment.EnterpriseEnrollment(
                self.cr.browser, self.username, self.password,
                auto_login=auto_login)

    def _create_chrome(self,
                       enroll=False,
                       auto_login=True,
                       arc_mode=None,
                       real_gaia=False,
                       init_network_controller=False,
                       disable_default_apps=True,
                       extension_paths=[],
                       extra_chrome_flags=[]):
        """
        Create a Chrome object. Enroll and/or sign in.

        Function results in self.cr set as the Chrome object.

        @param enroll: enroll the device.
        @param auto_login: sign in to chromeos.
        @param arc_mode: enable arc mode.
        @param real_gaia: to login with a real gaia account or not.
        @param init_network_controller: whether to init network controller.
        @param disable_default_apps: disables default apps or not
            (e.g. the Files app).
        @param extension_paths: list of extensions to install.
        @param extra_chrome_flags: list of flags to add.
        """
        extra_flags = (self._initialize_chrome_extra_flags() +
                       extra_chrome_flags)

        logging.info('Chrome Browser Arguments:')
        logging.info('  extra_browser_args: %s', extra_flags)
        logging.info('  username: %s', self.username)
        logging.info('  password: %s', self.password)
        logging.info('  gaia_login: %s', not self.dms_is_fake)
        logging.info('  enroll: %s', enroll)

        if enroll:
            self._enterprise_enroll(extra_flags, extension_paths, auto_login)
        elif auto_login:
            # gaia_login (aka real login) is true on arc_mode, or real_gaia.
            # otherwise: gaia_login is False when a fake dm server is used.
            gaia_login = (True if (arc_mode or real_gaia)
                          else not self.dms_is_fake)
            # Do not disable_gaia_services, or disable_arc_opt_in
            # if using a gaia_login.
            disable_gaia_services = disable_arc_opt_in = not gaia_login
            enterprise_arc_test = gaia_login

            self.cr = chrome.Chrome(
                extra_browser_args=extra_flags,
                username=self.username,
                password=self.password,
                gaia_login=gaia_login,
                arc_mode=arc_mode,
                disable_arc_opt_in=disable_arc_opt_in,
                disable_gaia_services=disable_gaia_services,
                autotest_ext=True,
                enterprise_arc_test=enterprise_arc_test,
                init_network_controller=init_network_controller,
                expect_policy_fetch=True,
                extension_paths=extension_paths,
                disable_default_apps=disable_default_apps)

        else:
            self.cr = chrome.Chrome(
                auto_login=False,
                extra_browser_args=extra_flags,
                disable_gaia_services=self.dms_is_fake,
                autotest_ext=True,
                expect_policy_fetch=True)

        self.ui = ui_utils.UI_Handler()
        # Used by arc.py to determine the state of the chrome obj
        self.initialized = True
        if auto_login:
            if not cryptohome.is_vault_mounted(user=self.username,
                                               allow_fail=True):
                raise error.TestError('Expected to find a mounted vault for %s.'
                                      % self.username)

    def navigate_to_url(self, url, tab=None):
        """Navigate tab to the specified |url|. Create new tab if none given.

        @param url: URL of web page to load.
        @param tab: browser tab to load (if any).
        @returns: browser tab loaded with web page.
        @raises: telemetry TimeoutException if document ready state times out.
        """
        logging.info('Navigating to URL: %r', url)
        if not tab:
            tab = self.cr.browser.tabs.New()
            tab.Activate()
        tab.Navigate(url, timeout=8)
        tab.WaitForDocumentReadyStateToBeComplete()
        return tab

    def get_elements_from_page(self, tab, cmd):
        """Get collection of page elements that match the |cmd| filter.

        @param tab: tab containing the page to be scraped.
        @param cmd: JavaScript command to evaluate on the page.
        @returns object containing elements on page that match the cmd.
        @raises: TestFail if matching elements are not found on the page.
        """
        try:
            elements = tab.EvaluateJavaScript(cmd)
        except Exception as err:
            raise error.TestFail('Unable to find matching elements on '
                                 'the test page: %s\n %r' % (tab.url, err))
        return elements

    def log_out_via_keyboard(self):
        """Log out of the device using the keyboard shortcut."""
        _keyboard = keyboard.Keyboard()
        _keyboard.press_key('ctrl+shift+q')
        _keyboard.press_key('ctrl+shift+q')
        _keyboard.close()
