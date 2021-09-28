#   Copyright 2019 - The Android Open Source Project
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
Sanity testing for RequestCellInfoUpdate() / GetAllCellInfo() API on Android Q
and regression check for GetAllCellInfo() on Android P
"""

import time
from acts.test_decorators import test_tracker_info
from acts.test_utils.tel.TelephonyBaseTest import TelephonyBaseTest
from acts.test_utils.tel.tel_test_utils import ensure_wifi_connected, \
    toggle_airplane_mode, ensure_phones_idle, start_qxdm_loggers
from acts.test_utils.wifi import wifi_test_utils
from acts.utils import disable_usb_charging, enable_usb_charging

NANO_TO_SEC = 1000000000
RATE_LIMIT_HIGH = 2
RATE_LIMIT_LOW = 10
CELL_INFO_UPDATE_WAIT_TIME_HIGH = 10
CELL_INFO_UPDATE_WAIT_TIME_LOW = 30
TIME_BETWEEN_CONSECUTIVE_API_CALLS = 0.1
# wait for 2 sec before start of new test case to account for previous
# calls to the API by previous test case.
WAIT_BEFORE_TEST_CASE_START = 2
WAIT_FOR_CELLULAR_CONNECTION = 20


class TelLiveCellInfoTest(TelephonyBaseTest):
    def setup_class(self):
        super().setup_class()
        self.ad = self.android_devices[0]
        self.wifi_network_ssid = self.user_params.get(
            "wifi_network_ssid") or self.user_params.get(
            "wifi_network_ssid_2g") or self.user_params.get(
            "wifi_network_ssid_5g")
        self.wifi_network_pass = self.user_params.get(
            "wifi_network_pass") or self.user_params.get(
            "wifi_network_pass_2g") or self.user_params.get(
            "wifi_network_ssid_5g")
        if self.ad.droid.connectivityCheckAirplaneMode():
            toggle_airplane_mode(self.log, self.ad, False)
            time.sleep(WAIT_FOR_CELLULAR_CONNECTION)

        return True

    def setup_test(self):
        return True

    def teardown_test(self):
        return True

    def teardown_class(self):
        return True

    def time_delta_in_sec(self, time1_ns, time2_ns):
        """To convert time stamps in nano seconds into seconds and return time
            delta between the two rounded to 3 digits.

         Args:
             time1_ns, time2_ns - time stamps in nano seconds

         Returns:
             delta between the two  time stamps in seconds
        """
        sec = round(abs((time1_ns - time2_ns) / NANO_TO_SEC), 3)
        return sec

    def nano_to_sec(self, nano_time):
        """ To convert and return time stamp from nano seconds to seconds,
            rounded to 3 digits.

        Args:
            nano_time - time in nano seconds

        Returns:
            time in seconds
        """
        sec = round(nano_time / NANO_TO_SEC, 3)
        return sec

    def request_cell_info_update_ts(self):
        """ SL4A API call for RequestCellInfoUpdate()

        Returns:
            Android system and modem timestamps for
            RequestCellInfoUpdate() API
        """
        system_ts = self.ad.droid.getSystemElapsedRealtimeNanos()
        self.log.info("System TS from boot: {}".format(system_ts))
        try:
            request_cell_info_update = \
                self.ad.droid.telephonyRequestCellInfoUpdate()[0]
        except Exception as e:
            self.log.error(
                'Failed to read request cell info update from device, please '
                'check if device is camped to cellular network.')
            return False
        modem_ts = request_cell_info_update['timestamp']
        self.log.info("Modem TS from boot: {}".format(modem_ts))
        return modem_ts, system_ts

    def _request_cell_info_update(self):
        time.sleep(WAIT_BEFORE_TEST_CASE_START)
        try:
            self.ad.droid.wakeUpNow()
            modem_ts, system_ts = self.request_cell_info_update_ts()
            if modem_ts > system_ts:
                self.log.info("Modem TS exceeds System TS by:{} secs".format(
                    self.time_delta_in_sec(modem_ts, system_ts)))
                return True
            else:
                return False
        except Exception as e:
            self.log.error("Exception during request_cell_info_update_ts():" +
                           str(e))
            return False

    def get_all_cell_info_ts(self):
        """SL4A API call for GetAllCellInfo()

        Returns:
            Android system and modem timestamp for GetAllCellInfo() API
        """
        system_ts = self.ad.droid.getSystemElapsedRealtimeNanos()
        self.log.info("System TS from boot: {}".format(system_ts))
        try:
            get_all_cell_info = self.ad.droid.telephonyGetAllCellInfo()[0]
        except Exception as e:
            self.log.error(
                'Failed to read get all cell info from device, please '
                'check if device is camped to cellular network.')
            return False
        modem_ts = get_all_cell_info['timestamp']
        self.log.info("Modem TS from boot: {}".format(modem_ts))
        return modem_ts, system_ts

    def _get_all_cell_info(self, apk_type):
        """
        Args:
            apk_type: SL4A APK type - 'Q' for android Q and 'P' for android P

        Returns:
            Result True if Pass, False if Fail.
        """
        time.sleep(WAIT_BEFORE_TEST_CASE_START)
        try:
            self.ad.droid.wakeUpNow()
            modem_ts, system_ts = self.get_all_cell_info_ts()
            if apk_type == 'Q':
                if modem_ts < system_ts:
                    self.log.info(
                        "System TS exceeds Modem TS by:{} secs".format(
                            self.time_delta_in_sec(modem_ts, system_ts)))
                    return True
                else:
                    return False
            elif apk_type == 'P':
                if modem_ts >= system_ts:
                    self.log.info(
                        "Modem TS exceeds System TS by:{} secs".format(
                            self.time_delta_in_sec(modem_ts, system_ts)))
                    return True
                else:
                    return False
        except Exception as e:
            self.log.error("Exception during get_all_cell_info_ts(): " +
                           str(e))
            return False

    def request_cell_info_update_rate_limit(self, margin, state):
        """Get time difference between two cell info updates.

        FOR Q APK: Gets the modem timestamp when RequestCellInfoUpdate()
        API is called and waits for it to update, and calculates the time
        delta between the modem timestamp update.

        Args:
            margin - time in seconds to wait for an updated modem timestamp
            after calling RequestCellInfoUpdate()
            state - UE power state (high or low)

        Returns:
            Time delta between two consecutive unique modem timestamps for
            margin (sec) corresponding to the UE state as passed in the
            arguments.
            False: if the value remains same for margin (sec) passed in method
            arguments.
        """
        try:
            ts1 = ts2 = self.ad.droid.telephonyRequestCellInfoUpdate()[0][
                'timestamp']
        except Exception as e:
            self.log.error(
                'Failed to read request cell info update from device, please '
                'check if device is camped to cellular network.')
            return False
        self.log.info("Modem timestamp: {}".format(ts1))
        timeout = time.time() + margin
        while ts1 == ts2:
            ts2 = self.ad.droid.telephonyRequestCellInfoUpdate()[0][
                'timestamp']
            time.sleep(TIME_BETWEEN_CONSECUTIVE_API_CALLS)
            if time.time() > timeout:
                self.log.info(
                    "Modem timestamp from RequestCellInfoUpdate() for {} "
                    "powered state "
                    "not refreshed for {} sec".format(state, margin))
                return False
        time_delta = self.nano_to_sec(ts2 - ts1)
        self.log.info("Updated Modem timestamp: {} in {} sec for "

                      "UE in {} powered state".format(ts2, time_delta, state))
        return time_delta

    def get_all_cell_info_rate_limit(self, margin, state):
        """Get time difference between two cell info updates

        FOR P APK: Gets the modem timestamp when GetAllCellInfo() API
        is called and waits for it to update, and calculates the time delta
        between the modem timestamp update.

        Args:
            margin - time in seconds to wait for an updated modem timestamp
            after calling GetAllCellInfo()
            state - UE power state (high or low)

        Returns:
            Time delta between two consecutive unique modem timestamps for
            margin (sec) corresponding to the UE state as passed in the
            arguments.
            False: if the value remains same for margin (sec) passed in
            method arguments.
        """
        try:
            ts1 = ts2 = self.ad.droid.telephonyGetAllCellInfo()[0]['timestamp']
        except Exception as e:
            self.log.error(
                'Failed to read get all cell info from device, please '
                'check if device is camped to cellular network.')
            return False
        self.log.info("Modem timestamp: {}".format(ts1))
        timeout = time.time() + margin
        while ts1 == ts2:
            ts2 = self.ad.droid.telephonyGetAllCellInfo()[0]['timestamp']
            time.sleep(TIME_BETWEEN_CONSECUTIVE_API_CALLS)
            if time.time() > timeout:
                self.log.info(
                    "Modem timestamp from GetAllCellInfo() for {} "
                    "powered state "
                    "not refreshed for {} sec".format(state, margin))
                return False
        time_delta = self.nano_to_sec(ts2 - ts1)
        self.log.info("Updated Modem timestamp: {} in {} sec for "
                      "UE in {} powered state".format(ts2, time_delta, state))
        return time_delta

    def _refresh_get_all_cell_info(self):
        try:
            modem_ts1, system_ts1 = self.get_all_cell_info_ts()
            modem_ts2, system_ts2 = self.request_cell_info_update_ts()
            modem_ts3, system_ts3 = self.get_all_cell_info_ts()
            if modem_ts3 > modem_ts1:
                self.log.info(
                    "Modem TS from GetAllCellInfo() is updated after "
                    "RequestCellInfoUpdate() is called"
                    " by :{} secs".format(
                        self.time_delta_in_sec(modem_ts3, modem_ts1)))
                return True
            else:
                return False
        except Exception as e:
            self.log.error("Exception during GetAllCellInfo() Refresh:" +
                           str(e))
            return False

    def _power_state_screen_off(self, apk_type):
        """
        Args:
            apk_type: SL4A APK type - 'Q' for android Q and 'P' for android P

        Returns:
            Result True if Pass, False if Fail.
        """
        try:
            self.ad.droid.goToSleepNow()
            if apk_type == 'Q':
                time_delta_low = self.request_cell_info_update_rate_limit(
                    CELL_INFO_UPDATE_WAIT_TIME_LOW, 'low')
            elif apk_type == 'P':
                time_delta_low = self.get_all_cell_info_rate_limit(
                    CELL_INFO_UPDATE_WAIT_TIME_LOW, 'low')
            if int(time_delta_low) == RATE_LIMIT_LOW:
                return True
            else:
                return False
        except Exception as e:
            self.log.error(
                "Exception during request_cell_info_update_rate_limit():" +
                str(e))
            return False

    def _power_state_screen_on_wifi_off(self, apk_type):
        """
        Args:
            apk_type: SL4A APK type - 'Q' for android Q and 'P' for android P

        Returns:
            Result True if Pass, False if Fail.
        """
        try:
            self.ad.droid.wakeUpNow()
            wifi_test_utils.wifi_toggle_state(
                self.ad, new_state=False, assert_on_fail=True)
            if apk_type == 'Q':
                time_delta = self.request_cell_info_update_rate_limit(
                    CELL_INFO_UPDATE_WAIT_TIME_HIGH, 'high')
            elif apk_type == 'P':
                time_delta = self.get_all_cell_info_rate_limit(
                    CELL_INFO_UPDATE_WAIT_TIME_HIGH, 'high')
            if int(time_delta) == RATE_LIMIT_HIGH:
                return True
            else:
                return False
        except Exception as e:
            self.log.error(
                "Exception during request_cell_info_update_rate_limit():" +
                str(e))
            return False

    def _rate_limit_charging_off(self, apk_type):
        """
        Args:
            apk_type: SL4A APK type - 'Q' for android Q and 'P' for android P

        Returns:
            Result True if Pass, False if Fail.
        """
        try:
            self.ad.droid.wakeUpNow()

            if not ensure_wifi_connected(self.log, self.ad,
                                         self.wifi_network_ssid,
                                         self.wifi_network_pass):
                self.ad.log.error("Failed to connect to wifi")
                return False
            """ Disable Charging """
            disable_usb_charging(self.ad)
            if apk_type == 'P':
                time_delta = self.get_all_cell_info_rate_limit(
                    CELL_INFO_UPDATE_WAIT_TIME_LOW, 'low')
            elif apk_type == 'Q':
                time_delta = self.request_cell_info_update_rate_limit(
                    CELL_INFO_UPDATE_WAIT_TIME_LOW, 'low')
            """ Enable Charging """
            enable_usb_charging(self.ad)
            if int(time_delta) == RATE_LIMIT_LOW:
                return True
            else:
                return False
        except Exception as e:
            self.log.error("Exception in rate_limit function:" + str(e))
            return False

    def _rate_limit_switch(self, apk_type):
        """
        Args:
            apk_type: SL4A APK type - 'Q' for android Q and 'P' for android P

        Returns:
            Result True if Pass, False if Fail.
        """
        try:
            pass_count = 0
            while pass_count != 3:
                """Put UE in sleep for low powered state"""
                self.ad.droid.goToSleepNow()
                if apk_type == 'P':
                    time_delta_low = self.get_all_cell_info_rate_limit(
                        CELL_INFO_UPDATE_WAIT_TIME_LOW, 'low')
                elif apk_type == 'Q':
                    time_delta_low = self.request_cell_info_update_rate_limit(
                        CELL_INFO_UPDATE_WAIT_TIME_LOW, 'low')
                if int(time_delta_low) == RATE_LIMIT_LOW:
                    """Wake up UE and turn ON WiFi for high powered state"""
                    self.ad.droid.wakeUpNow()
                    wifi_test_utils.wifi_toggle_state(
                        self.ad, new_state=False, assert_on_fail=True)
                    if apk_type == 'P':
                        time_delta_high = self.get_all_cell_info_rate_limit(
                            CELL_INFO_UPDATE_WAIT_TIME_HIGH, 'high')
                    elif apk_type == 'Q':
                        time_delta_high = \
                            self.request_cell_info_update_rate_limit(
                                CELL_INFO_UPDATE_WAIT_TIME_HIGH, 'high')
                    if int(time_delta_high) == RATE_LIMIT_HIGH:
                        pass_count += 1
                    else:
                        return False
                else:
                    return False
            return True
        except Exception as e:
            self.log.error("Exception during rate limit switch:" + str(e))
            return False

    """ Tests Start """
    """ Q Targeted APK Test Cases """

    @test_tracker_info(uuid="")
    @TelephonyBaseTest.tel_test_wrap
    def test_q_request_cell_info_update(self):
        """To verify a Q apk receives refreshed info when calling the
        RequestCellInfoUpdate() API

        Returns:
            True if pass; False if fail
        """
        return self._request_cell_info_update()

    @test_tracker_info(uuid="")
    @TelephonyBaseTest.tel_test_wrap
    def test_q_get_all_cell_info(self):
        """ To verify a Q apk always receives cached info and never gets
        refreshed info when calling the GetAllCellInfo() API.

        Returns:
            True if pass; False if fail
        """
        return self._get_all_cell_info(apk_type='Q')

    @test_tracker_info(uuid="")
    @TelephonyBaseTest.tel_test_wrap
    def test_q_get_all_cell_info_refreshed(self):
        """ To verify in a Q APK the cached info in GetAllCellInfo() is updated
        after RequestCellInfoUpdate() API provides a refreshed cell info
        from the modem.

        Returns:
            True if pass; False if fail
        """
        return self._refresh_get_all_cell_info()

    @test_tracker_info(uuid="")
    @TelephonyBaseTest.tel_test_wrap
    def test_q_request_cell_info_update_screen_off(self):
        """ To verify RATE_LIMIT_LOW is applied when the UE is in low powered
        state (Screen Off)

        Returns:
            True if Pass and False if Fail
        """
        return self._power_state_screen_off(apk_type='Q')

    @test_tracker_info(uuid="")
    @TelephonyBaseTest.tel_test_wrap
    def test_q_request_cell_info_update_screen_on_wifi_off(self):
        """ To verify RATE_LIMIT_HIGH is applied when the UE is in high powered
        state (Screen On / WiFi Off)

        Returns:
            True if Pass and False if Fail
        """
        return self._power_state_screen_on_wifi_off(apk_type='Q')

    @test_tracker_info(uuid="")
    @TelephonyBaseTest.tel_test_wrap
    def test_q_rate_limit_charging_off(self):
        """ To verify RATE_LIMIT_LOW is applied when the UE is in low powered
        state (Screen On / WiFi On/Phone NOT charging)

        Returns:
            True if Pass and False if Fail
        """
        return self._rate_limit_charging_off(apk_type='Q')

    @test_tracker_info(uuid="")
    @TelephonyBaseTest.tel_test_wrap
    def test_q_rate_limit_switch(self):
        """ To verify rate limiting while UE is moving between high-powered &
        low-powered states for 3 iterations.

        Returns:
            True if Pass and False if Fail.
        """
        return self._rate_limit_switch(apk_type='Q')

    """ P Targeted APK Test Cases """

    @test_tracker_info(uuid="")
    @TelephonyBaseTest.tel_test_wrap
    def test_p_get_all_cell_info(self):
        """ To verify a P apk always receives refreshed info when calling the
        GetAllCellInfo() API

        Returns:
            True if pass; False if fail
        """
        return self._get_all_cell_info(apk_type='P')

    @test_tracker_info(uuid="")
    @TelephonyBaseTest.tel_test_wrap
    def test_p_get_all_cell_info_screen_off(self):
        """ To verify RATE_LIMIT_LOW is applied when the UE is in low powered
        state (Screen OFF)

        Returns:
            True if Pass and False if Fail
        """
        return self._power_state_screen_off(apk_type='P')

    @test_tracker_info(uuid="")
    @TelephonyBaseTest.tel_test_wrap
    def test_p_get_all_cell_info_screen_on_wifi_off(self):
        """ To verify RATE_LIMIT_HIGH is applied when the UE is in high powered
        state (Screen ON/ WiFi OFF/ Cellular ON)

        Returns:
            True if Pass and False if Fail
        """
        return self._power_state_screen_on_wifi_off(apk_type='P')

    @test_tracker_info(uuid="")
    @TelephonyBaseTest.tel_test_wrap
    def test_p_rate_limit_charging_off(self):
        """ To verify RATE_LIMIT_LOW is applied when the UE is in low powered
        state (Screen On / WiFi On/Phone NOT charging)

        Returns:
            True if Pass and False if Fail
        """
        return self._rate_limit_charging_off(apk_type='P')

    @test_tracker_info(uuid="")
    @TelephonyBaseTest.tel_test_wrap
    def test_p_rate_limit_switch(self):
        """ To verify rate limiting while UE is moving between high-powered &
        low-powered states.

        Returns:
            True if Pass and False if Fail.
        """
        return self._rate_limit_switch(apk_type='P')

    """ Tests End """
