#!/usr/bin/env python3
#
#   Copyright 2016 - The Android Open Source Project
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
"""
Fi Switching Methods 
"""
import time
from acts.controllers.anritsu_lib._anritsu_utils import AnritsuError
from acts.controllers.anritsu_lib.md8475a import CBCHSetup
from acts.controllers.anritsu_lib.md8475a import CTCHSetup
from acts.controllers.anritsu_lib.md8475a import MD8475A
from acts.test_utils.tel.anritsu_utils import cb_serial_number
from acts.test_utils.tel.anritsu_utils import set_system_model_lte
from acts.test_utils.tel.anritsu_utils import set_usim_parameters
from acts.test_utils.tel.anritsu_utils import set_post_sim_params
from acts.test_utils.tel.tel_test_utils import \
    ensure_preferred_network_type_for_subscription
from acts.test_utils.tel.tel_test_utils import ensure_phones_idle
from acts.test_utils.tel.tel_test_utils import toggle_airplane_mode
from acts.test_utils.tel.tel_test_utils import start_qxdm_loggers
from acts.test_utils.tel.TelephonyBaseTest import TelephonyBaseTest
from acts.test_utils.tel.tel_defines import NETWORK_MODE_LTE_GSM_WCDMA
from acts.test_utils.tel.tel_defines import RAT_FAMILY_LTE
from acts.test_decorators import test_tracker_info
from acts.test_utils.tel.tel_defines import RAT_LTE
from acts.test_utils.tel.tel_defines import CARRIER_SPT
from acts.test_utils.tel.tel_defines import CARRIER_TMO
from acts.test_utils.tel.tel_defines import CARRIER_USCC
from acts.test_utils.tel.tel_lookup_tables import operator_name_from_plmn_id
from acts.test_utils.tel.tel_test_utils import abort_all_tests
from acts.test_utils.tel.tel_test_utils import ensure_phone_subscription
from acts.test_utils.tel.tel_test_utils import ensure_wifi_connected
from acts.test_utils.tel.tel_test_utils import is_sim_ready
from acts.test_utils.tel.tel_test_utils import log_screen_shot
from acts.test_utils.tel.tel_test_utils import multithread_func
from acts.test_utils.tel.tel_test_utils import reboot_device
from acts.test_utils.tel.tel_test_utils import refresh_droid_config
from acts.test_utils.tel.tel_test_utils import send_dialer_secret_code
from acts.test_utils.tel.tel_test_utils import toggle_airplane_mode_by_adb
from acts.test_utils.tel.tel_test_utils import wait_for_state
from acts.test_utils.tel.tel_test_utils import add_google_account
from acts.test_utils.tel.tel_test_utils import remove_google_account

WAIT_TIME_BETWEEN_REG_AND_MSG = 15  # default 15 sec
CARRIER = None
CARRIER_AUTO = "auto"

_CARRIER_DIALER_CODE_LOOKUP = {
    CARRIER_AUTO: '342886',
    CARRIER_SPT: '34777',
    CARRIER_TMO: '34866',
    CARRIER_USCC: '34872'
}

_SWITCHING_PREF_FILE = (
    '/data/data/com.google.android.apps.tycho/shared_prefs/switching.xml')

_INTENT_FLAGS = int(0x00008000 | 0x10000000 | 0x00080000 | 0x00020000)
_TYCHO_PKG = 'com.google.android.apps.tycho'
_MAX_WAIT_TIME = 120
_TYCHO_VERBOSE_LOGGING_CMDS = [
    "setprop log.tag.Tycho VERBOSE",
    "CLASSPATH=/system/framework/am.jar su root app_process "
    "/system/bin com.android.commands.am.Am broadcast -a "
    "com.google.gservices.intent.action.GSERVICES_OVERRIDE "
    "-e tycho.enable_request_logging true",
    "CLASSPATH=/system/framework/am.jar su root app_process "
    "/system/bin com.android.commands.am.Am broadcast -a "
    "com.google.gservices.intent.action.GSERVICES_OVERRIDE "
    "-e tycho.enable_sensitive_logging true",
    "CLASSPATH=/system/framework/am.jar su root app_process "
    "/system/bin com.android.commands.am.Am broadcast -a "
    "com.google.gservices.intent.action.GSERVICES_OVERRIDE "
    "-e tycho.enable_ample_logging true"
]

_TYCHO_SERVER_LAB_OVERRIDE_CMD = (
    "am broadcast -a com.google.gservices.intent.action.GSERVICES_OVERRIDE -e "
    "url:tycho_server_endpoint https://android.googleapis.com/nova/nfe/ rewrite"
    " https://android.googleapis.com/lab/nova/nfe/")


class TychoClassId(object):
    """Tycho Activity/Service Classnames."""
    # Activities
    CARRIER_SETUP = 'CarrierSetupEntryPointTrampoline'
    INIT_ACTIVITY = 'InitActivity'
    # Services
    SYNC_SERVICE = 'services.SyncService'
    ACTIVATE_SUPER_NETWORK_SERVICE = 'services.SuperNetworkConfigurationService'


class ActionTypeId(object):
    """Andorid Action Type to trigger events."""
    MAIN = 'android.intent.action.MAIN'
    MASTER_CLEAR_NOTIFICATION = 'android.intent.action.MASTER_CLEAR_NOTIFICATION'
    TYCHO_ACTIVATE_SUPER_NETWORK = (
        'com.google.android.apps.tycho.ActionType.ACTIVATE_SUPER_NETWORK')


class TelLabProjectFiTest(TelephonyBaseTest):
    SERIAL_NO = cb_serial_number()

    def setup_class(self):
        super().setup_class()
        self.ad = self.android_devices[0]
        self.ad.sim_card = getattr(self.ad, "sim_card", None)
        self.md8475a_ip_address = self.user_params[
            "anritsu_md8475a_ip_address"]
        self.wlan_option = self.user_params.get("anritsu_wlan_option", False)
        self.md8475_version = self.user_params.get("md8475", "A")
        self.ad.adb.shell("settings put secure cmas_additional_broadcast_pkg "
                          "com.googlecode.android_scripting")
        self.wait_time_between_reg_and_msg = self.user_params.get(
            "wait_time_between_reg_and_msg", WAIT_TIME_BETWEEN_REG_AND_MSG)

        try:
            self.anritsu = MD8475A(self.md8475a_ip_address, self.wlan_option,
                                   self.md8475_version)
        except AnritsuError:
            self.log.error("Error in connecting to Anritsu Simulator")
            return False
        self.activation_attemps = self.user_params.get("activation_attemps", 3)
        return True

    def setup_test(self):
        if getattr(self, "qxdm_log", True):
            start_qxdm_loggers(self.log, self.android_devices)
        ensure_phones_idle(self.log, self.android_devices)
        toggle_airplane_mode(self.log, self.ad, True)
        return True

    def teardown_test(self):
        self.log.info("Stopping Simulation")
        self.anritsu.stop_simulation()
        toggle_airplane_mode(self.log, self.ad, True)

    def teardown_class(self):
        self.anritsu.disconnect()
        return True

    def _bring_up_callbox(
            self,
            set_simulation_func,
            rat):
        try:
            [self.bts1] = set_simulation_func(self.anritsu, self.user_params,
                                              self.ad.sim_card)
            set_usim_parameters(self.anritsu, self.ad.sim_card)
            if rat == RAT_LTE:
                set_post_sim_params(self.anritsu, self.user_params,
                                    self.ad.sim_card)
            self.anritsu.start_simulation()

            if rat == RAT_LTE:
                preferred_network_setting = NETWORK_MODE_LTE_GSM_WCDMA
                rat_family = RAT_FAMILY_LTE
            else:
                self.log.error("No valid RAT provided to bring up callbox.")
                return False

            if not ensure_preferred_network_type_for_subscription(
                    self.ad,
                    preferred_network_setting):
                self.log.error(
                    "Failed to set rat family {}, preferred network:{}".format(
                        rat_family, preferred_network_setting))
                return False

            if self.ad.droid.connectivityCheckAirplaneMode():
                toggle_airplane_mode(self.log, self.ad, False)
        except AnritsuError as e:
            self.log.error("Error in connection with Anritsu Simulator: " +
                           str(e))
            return False
        return True

    def pre_switching_callbox_setup(self):
        """ Setup environment to enable carrier switching
        Returns:
            True if pass; False if fail
        """
        return self._bring_up_callbox(set_system_model_lte, RAT_LTE)

    def _install_account_util(self, ad):
        account_util = self.user_params["account_util"]
        if isinstance(account_util, list):
            account_util = account_util[0]
        ad.log.info("Install account_util %s", account_util)
        ad.ensure_screen_on()
        ad.adb.install("-r %s" % account_util, timeout=300, ignore_status=True)
        time.sleep(3)
        if not ad.is_apk_installed("com.google.android.tradefed.account"):
            ad.log.info("com.google.android.tradefed.account is not installed")
            return False
        return True

    def _account_registration(self, ad):
        toggle_airplane_mode_by_adb(self.log, ad, new_state=False)
        for cmd in _TYCHO_VERBOSE_LOGGING_CMDS:
            ad.adb.shell(cmd)
        if hasattr(ad, "user_account"):
            ad.exit_setup_wizard()
            if not ad.is_apk_installed("com.google.android.tradefed.account"
                                       ) and \
                    self.user_params.get("account_util"):
                for _ in range(2):
                    if self._install_account_util(ad):
                        break
                else:
                    ad.log.error(
                        "Fail to install com.google.android.tradefed.account")
                    return False
            ad.force_stop_apk(_TYCHO_PKG)
            if not ensure_wifi_connected(self.log, ad, self.wifi_network_ssid,
                                         self.wifi_network_pass):
                ad.log.error("Failed to connect to wifi")
                return False
            ad.log.info("Add google account")
            if not add_google_account(ad):
                ad.log.error("Failed to add google account")
                return False
            ad.adb.shell(
                'am instrument -w -e account "%s@gmail.com" -e password '
                '"%s" -e sync true -e wait-for-checkin false '
                'com.google.android.tradefed.account/.AddAccount' %
                (ad.user_account, ad.user_password))
            ad.log.info("Enable and activate tycho apk")
            if not ad.is_apk_installed(_TYCHO_PKG):
                ad.log.info("%s is not installed", _TYCHO_PKG)
                return False
            ad.adb.shell('pm enable %s' % _TYCHO_PKG)
            # ad.adb.shell(_TYCHO_SERVER_LAB_OVERRIDE_CMD)
            for i in range(1, self.activation_attemps + 1):
                if i == self.activation_attemps:
                    ad.log.info("Reboot and try Fi activation again")
                    reboot_device(ad)
                self.activate_fi_account(ad)
                if not self.check_project_fi_activated(ad):
                    ad.log.error("Fail to activate Fi account on attempt-%s",
                                 i)
                    if i == self.activation_attemps:
                        return False
                else:
                    ad.log.info("Fi account is activated successfully")
                    break
        elif "Fi Network" in ad.adb.getprop("gsm.sim.operator.alpha"):
            ad.log.error("Google account is not provided for Fi Network")
            return False
        if not ensure_phone_subscription(self.log, ad):
            ad.log.error("Unable to find a valid subscription!")
            return False
        refresh_droid_config(self.log, ad)
        return True

    def start_service(self, ad, package, service_id, extras, action_type):
        """Starts the specified service.

        Args:
          ad: (android_device.AndroidDevice) device to start activity on
          package: (str) the package to start the service from
          service_id: (str) service to start
          extras: (dict) extras needed to specify with the activity id
          action_type: The action type id to create the intent
        """
        ad.log.info('Starting service %s/.%s.', package, service_id)
        intent = ad.droid.makeIntent(action_type, None, None, extras,
                                     ['android.intent.category.DEFAULT'],
                                     package, package + '.' + service_id,
                                     _INTENT_FLAGS)
        ad.droid.startServiceIntent(intent)

    def start_activity(self, ad, package, activity_id, extras=None):
        """Starts the specified activity.

        Args:
          ad: (android_device.AndroidDevice) device to start activity on
          package: (str) the package to start
          activity_id: (str) activity to start
          extras: (dict) extras needed to specify with the activity id
        """
        ad.log.info('Starting activity %s/.%s.', package, activity_id)
        intent = ad.droid.makeIntent(ActionTypeId.MAIN, None, None, extras,
                                     ['android.intent.category.LAUNCHER'],
                                     package, package + '.' + activity_id,
                                     _INTENT_FLAGS)
        ad.droid.startActivityIntent(intent, False)

    def activate_fi_account(self, ad):
        """Start Tycho InitActivity.

        For in-app Tycho activition (post-SUW tests), Tycho does not
        automatically trigger OMADM process. This method is used to start
        Tycho InitActivity before launching super network activation.

        The device will finally stay on Sprint network if everything goes well.

        Args:
          ad: Android device need to start Tycho InitActivity.
        """
        ad.force_stop_apk(_TYCHO_PKG)
        ad.send_keycode("HOME")
        extra = {'in_setup_wizard': False, 'force_show_account_chooser': False}
        self.start_activity(ad, _TYCHO_PKG, TychoClassId.INIT_ACTIVITY, extra)
        for _ in range(30):
            ad.send_keycode("WAKEUP")
            time.sleep(1)
            current_window = ad.get_my_current_focus_window()
            log_screen_shot(ad, self.test_name)
            if ad.adb.shell(
                    "settings get global device_provisioning_mobile_data"
            ) != "1":
                ad.adb.shell(
                    "settings put global device_provisioning_mobile_data 1")
            if 'SwitchConfirmDialogActivity' in current_window:
                ad.log.info("In Switch Confirmation Dialog")
                if ad.adb.getprop("ro.build.version.release")[0] not in ("8",
                                                                         "O"):
                    ad.send_keycode("TAB")
                ad.send_keycode("TAB")
                ad.send_keycode("ENTER")
                time.sleep(10)
            elif 'tycho.InitActivity' in current_window:
                ad.log.info("In Tycho InitActivity")
                ad.send_keycode("TAB")
                ad.send_keycode("TAB")
                ad.send_keycode("ENTER")
                time.sleep(10)

            elif 'tycho.AccountChooserActivity' in current_window:
                ad.send_keycode("ENTER")
            else:
                ad.log.info("Finished activation process")
                return

    def check_project_fi_activated(self, ad, retries=20):
        for _ in range(retries):
            if is_sim_ready(self.log, ad) and (
                    ad.droid.telephonyGetSimOperatorName() == "Fi Network"):
                ad.log.info("SIM state is READY, SIM operator is Fi")
                return True
            time.sleep(5)

    def start_tycho_activation(self, ad):
        """Start the Tycho client and register to cellular network.

        Starts Tycho within SUW:
         - Tycho is expected to follow the in-SUW work flow:
          - Tycho will perform TychoInit, handshake to server,
            account configuration, etc
          - If successful, Tycho will trigger a switch to Sprint Network
          - If successful, Tycho will start OMA-DM activation sessions

        The device will finally stay on Sprint network if everything goes well.

        Args:
          ad: Android device need to start Tycho activation.
        """
        extra = {'device_setup': True, 'has_account': True}
        self.start_activity(ad, _TYCHO_PKG, TychoClassId.CARRIER_SETUP, extra)

    def start_super_network_activation(self, ad):
        """Start the Super-Network activation.

        For in-app Tycho activition (post-SUW tests), this method starts
        super-network activation after Tycho is initialized.

        The device will finally stay on Sprint network if everything goes well.

        Args:
          ad: Android device need to start Tycho super network activation.
        """
        extra = {'in_setup_wizard': False, 'is_interactive': True}
        self.start_service(ad, _TYCHO_PKG,
                           TychoClassId.ACTIVATE_SUPER_NETWORK_SERVICE, extra,
                           ActionTypeId.TYCHO_ACTIVATE_SUPER_NETWORK)

    def get_active_carrier(self, ad):
        """Gets the active carrier profile value from the device.

        Args:
            ad: An AndroidDevice Object.

        Returns:
            (string) A key from the CARRIER_TO_MCC_MNC map representing the
            active carrier.

        Raises:
            KeyError: when an mcc_mnc code reported by the device is not a
            recognized Fi partner carrier.
        """
        mcc_mnc = ad.droid.telephonyGetSimOperator()
        if not mcc_mnc:
            return "UNKNOWN"
        try:
            return operator_name_from_plmn_id(mcc_mnc)
        except KeyError:
            ad.log.error('Unknown Mobile Country Code/Mobile Network Code %s',
                         mcc_mnc)
            raise

    def switch_sim(self, ad):
        """Requests switch between physical sim and esim.

        Args:
            ad: An AndroidDevice Object.
            timeout: (optional -- integer) the number of seconds in which a
                     switch should be completed.

        Raises:
            Error: whenever a device is not set to the desired carrier within
                   the timeout window.
        """
        old_sim_operator = ad.droid.telephonyGetSimOperatorName()
        ad.log.info("Before SIM switch, SIM operator = %s", old_sim_operator)
        send_dialer_secret_code(ad, "794824746")
        time.sleep(10)
        new_sim_operator = ad.droid.telephonyGetSimOperatorName()
        ad.log.info("After SIM switch, SIM operator = %s", new_sim_operator)
        refresh_droid_config(self.log, ad)
        return old_sim_operator != new_sim_operator

    def set_active_carrier(self,
                           ad,
                           carrier,
                           timeout=_MAX_WAIT_TIME,
                           check_interval=10):
        """Requests an active carrier to be set on the device sim.

        If switching to a different carrier, after the switch is completed
        auto-switching will be disabled. To re-enable, call
        enable_auto_switching.

        Args:
            ad: An AndroidDevice Object.
            carrier: (carrier_constants.Carrier) Which carrier to switch to.
            timeout: (optional -- integer) the number of seconds in which a
                     switch should be completed.

        Raises:
            Error: whenever a device is not set to the desired carrier within
                   the timeout window.
        """
        # If there's no need to switch, then don't.

        if self.pre_switching_callbox_setup():
            self.log.info("Turned ON Anritsu for Switching")
            pass
        else:
            self.log.error("Failed to Camp to Anritsu")
            return False

        max_time = timeout
        while max_time >= 0:
            if self.is_ready_to_make_carrier_switch(ad):
                break
            time.sleep(check_interval)
            max_time -= check_interval
        else:
            ad.log.error("Device stays in carrier switch lock state")
            return False
        if carrier == CARRIER_AUTO:
            send_dialer_secret_code(ad, _CARRIER_DIALER_CODE_LOOKUP[carrier])
            return True
        old_carrier = self.get_active_carrier(ad)
        if carrier == old_carrier:
            ad.log.info('Already on %s, so no need to switch', carrier)
            return True

        # Start switch on device, using events to verify that the switch starts.
        ad.log.info('Initiating unsolicited switch from %s to %s.',
                    old_carrier, carrier)
        send_dialer_secret_code(ad, _CARRIER_DIALER_CODE_LOOKUP[carrier])
        return self.wait_for_carrier_switch_completed(
            ad, carrier, timeout=timeout, check_interval=check_interval)

    def is_switching_silent(self, ad):
        """Checks if Tycho switching controller is in silent mode.

        Note that silent mode is a sign of airplane mode, not of a switching \
        lock.

        Args: ad: An AndroidDevice Object.

        Returns:
            A Boolean True if the preferences file reports True, False
            otherwise.
        """
        return "isInSilentMode\" value=\"true" in ad.adb.shell(
            "cat %s | grep isInSilentMode" % _SWITCHING_PREF_FILE,
            ignore_status=True)

    def is_switching_locked(self, ad):
        """Checks if Tycho switching controller is locked.

        Args: ad: An AndroidDevice Object.

        Returns:
            A Boolean True if the switching controller is locked for any reason,
            False otherwise.
        """
        return "switchingInProgress\" value=\"true" in ad.adb.shell(
            "cat %s | grep switchingInProgress" % _SWITCHING_PREF_FILE)

    def is_ready_to_make_carrier_switch(self, ad):
        """Checks if device is ready to make carrier switch.

        Args:
            ad: An AndroidDevice Object.

        Returns:
             A Boolean True if it is ready to make switch, False otherwise.
        """
        # Check Tycho switching controller states.
        if self.is_switching_silent(ad):
            ad.log.info(
                "Cannot make carrier switch: SwitchingController is in silent "
                "mode!")
            return False
        if self.is_switching_locked(ad):
            ad.log.info(
                "Cannot make carrier switch: SwitchingController is locked!")
            return False
        if self.is_carrier_switch_in_progress(ad):
            ad.log.info("Cannot make carrier switch: Switch in progress!")
            return False
        return True

    def is_carrier_switch_in_progress(self, ad):
        """Checks if Tycho says that a switch is currently in progress.

        Args:
            ad: An AndroidDevice Object.

        Returns:
             A Boolean True if the preferences file reports True, False
             otherwise.
        """
        switching_preferences = ad.adb.shell("cat %s" % _SWITCHING_PREF_FILE)
        return 'InProgress\" value=\"true' in switching_preferences

    def check_network_carrier(self, ad, carrier):
        current_carrier = self.get_active_carrier(ad)
        ad.log.info("Current network carrier is %s", current_carrier)
        is_in_switch = self.is_carrier_switch_in_progress(ad)
        ad.log.info("Device in carrier switch progress mode")
        return current_carrier == carrier and is_in_switch

    def wait_for_carrier_switch_completed(self,
                                          ad,
                                          carrier,
                                          timeout=_MAX_WAIT_TIME,
                                          check_interval=10):
        """Wait for carrier switch to complete.

        This function waits for a carrier switch to complete by monitoring the
        Tycho switching controller preference file.

        Args:
            ad: An Android device object.
            carrier: The target carrier network to switch to.
            timeout: (integer) Time wait for switch to complete.

        Return:
            True or False for successful/unsuccessful switch.
        """
        check_args = [ad, carrier]
        if wait_for_state(self.check_network_carrier, True, check_interval,
                          timeout, *check_args):
            ad.log.info("Switched to %s successfully", carrier)
            ad.send_keycode("ENTER")
            return True
        else:
            active_carrier = self.get_active_carrier(ad)
            if active_carrier == carrier:
                ad.log.info("Switched to %s successfully", carrier)
                return True
            ad.log.error("Carrier is %s. Fail to switch to %s", active_carrier,
                         carrier)
            return False

    def operator_network_switch(self, ad, carrier):
        if ad.droid.telephonyGetSimOperatorName() == "Google Fi":
            for i in range(3):
                if self.set_active_carrier(ad, carrier):
                    break
                elif i == 2:
                    ad.log.error("Failed to switch to %s", carrier)
                    return False
        if not ensure_phone_subscription(self.log, ad):
            ad.log.error("Unable to find a valid subscription!")
            return False
        refresh_droid_config(self.log, ad)
        return True

    def network_switch_test(self, carrier):
        tasks = [(self.operator_network_switch, [ad, carrier])
                 for ad in self.android_devices]
        if not multithread_func(self.log, tasks):
            abort_all_tests(self.log,
                            "Unable to switch to network %s" % carrier)
        return True

    """ Tests Begin """

    @test_tracker_info(uuid="4d92318e-4980-471a-882b-3136c5dda384")
    @TelephonyBaseTest.tel_test_wrap
    def test_project_fi_account_activation(self):
        """Test activate Fi account.

        Returns:
            True if success.
            False if failed.
        """
        tasks = [(self._account_registration, [ad])
                 for ad in self.android_devices]
        try:
            if not multithread_func(self.log, tasks):
                abort_all_tests(self.log, "Unable to activate Fi account!")
        except Exception as e:
            self.log.error(e)
            abort_all_tests(self.log, "Unable to activate Fi account!")
        return True

    @test_tracker_info(uuid="6bfbcc1d-e318-4964-bf36-5b82f086860d")
    @TelephonyBaseTest.tel_test_wrap
    def test_switch_to_tmobile_network(self):
        """Test switch to tmobile network.

        Returns:
            True if success.
            False if failed.
        """
        setattr(self.ad, "sim_card", "FiTMO")
        return self.network_switch_test(CARRIER_TMO)

    @test_tracker_info(uuid="4f27944d-f3c5-423d-b0c5-5c66dbb98376")
    @TelephonyBaseTest.tel_test_wrap
    def test_switch_to_sprint_network(self):
        """Test switch to sprint network.

        Returns:
            True if success.
            False if failed.
        """
        setattr(self.ad, "sim_card", "FiSPR")
        return self.network_switch_test(CARRIER_SPT)

    @test_tracker_info(uuid="5f30c9bd-b79e-4805-aa46-7855ed9023f0")
    @TelephonyBaseTest.tel_test_wrap
    def test_switch_to_uscc_network(self):
        """Test switch to uscc network.

        Returns:
            True if success.
            False if failed.
        """
        setattr(self.ad, "sim_card", "FiUSCC")
        return self.network_switch_test(CARRIER_USCC)

    @test_tracker_info(uuid="0b062751-d59d-420e-941e-3ffa02aea0d5")
    @TelephonyBaseTest.tel_test_wrap
    def test_switch_to_auto_network(self):
        """Test switch to auto network selection.

        Returns:
            True if success.
            False if failed.
        """
        return self.network_switch_test(CARRIER_AUTO)

    @test_tracker_info(uuid="13c5f080-69bf-42fd-86ed-c67b1984c347")
    @TelephonyBaseTest.tel_test_wrap
    def test_switch_between_sim(self):
        """Test switch between physical sim and esim.

        Returns:
            True if success.
            False if failed.
        """
        for ad in self.android_devices:
            self.switch_sim(ad)

    @test_tracker_info(uuid="")
    @TelephonyBaseTest.tel_test_wrap
    def test_remove_google_account(self):
        for ad in self.android_devices:
            remove_google_account(ad)


""" Tests End """
