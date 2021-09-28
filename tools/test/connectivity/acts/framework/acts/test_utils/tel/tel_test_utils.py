#!/usr/bin/env python3
#
#   Copyright 2016 - Google
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

from future import standard_library
standard_library.install_aliases()

import concurrent.futures
import json
import logging
import re
import os
import urllib.parse
import time
import acts.controllers.iperf_server as ipf
import shutil
import struct

from acts import signals
from acts import utils
from queue import Empty
from acts.asserts import abort_all
from acts.asserts import fail
from acts.controllers.adb import AdbError
from acts.controllers.android_device import list_adb_devices
from acts.controllers.android_device import list_fastboot_devices
from acts.controllers.android_device import DEFAULT_QXDM_LOG_PATH
from acts.controllers.android_device import DEFAULT_SDM_LOG_PATH
from acts.controllers.android_device import SL4A_APK_NAME
from acts.libs.proc import job
from acts.test_utils.tel.loggers.protos.telephony_metric_pb2 import TelephonyVoiceTestResult
from acts.test_utils.tel.tel_defines import CarrierConfigs
from acts.test_utils.tel.tel_defines import AOSP_PREFIX
from acts.test_utils.tel.tel_defines import CARD_POWER_DOWN
from acts.test_utils.tel.tel_defines import CARD_POWER_UP
from acts.test_utils.tel.tel_defines import CAPABILITY_CONFERENCE
from acts.test_utils.tel.tel_defines import CAPABILITY_VOLTE
from acts.test_utils.tel.tel_defines import CAPABILITY_VOLTE_PROVISIONING
from acts.test_utils.tel.tel_defines import CAPABILITY_VOLTE_OVERRIDE_WFC_PROVISIONING
from acts.test_utils.tel.tel_defines import CAPABILITY_VT
from acts.test_utils.tel.tel_defines import CAPABILITY_WFC
from acts.test_utils.tel.tel_defines import CAPABILITY_WFC_MODE_CHANGE
from acts.test_utils.tel.tel_defines import CARRIER_UNKNOWN
from acts.test_utils.tel.tel_defines import CARRIER_FRE
from acts.test_utils.tel.tel_defines import COUNTRY_CODE_LIST
from acts.test_utils.tel.tel_defines import DATA_STATE_CONNECTED
from acts.test_utils.tel.tel_defines import DATA_STATE_DISCONNECTED
from acts.test_utils.tel.tel_defines import DATA_ROAMING_ENABLE
from acts.test_utils.tel.tel_defines import DATA_ROAMING_DISABLE
from acts.test_utils.tel.tel_defines import GEN_4G
from acts.test_utils.tel.tel_defines import GEN_UNKNOWN
from acts.test_utils.tel.tel_defines import INCALL_UI_DISPLAY_BACKGROUND
from acts.test_utils.tel.tel_defines import INCALL_UI_DISPLAY_FOREGROUND
from acts.test_utils.tel.tel_defines import INVALID_SIM_SLOT_INDEX
from acts.test_utils.tel.tel_defines import INVALID_SUB_ID
from acts.test_utils.tel.tel_defines import MAX_SAVED_VOICE_MAIL
from acts.test_utils.tel.tel_defines import MAX_SCREEN_ON_TIME
from acts.test_utils.tel.tel_defines import MAX_WAIT_TIME_ACCEPT_CALL_TO_OFFHOOK_EVENT
from acts.test_utils.tel.tel_defines import MAX_WAIT_TIME_AIRPLANEMODE_EVENT
from acts.test_utils.tel.tel_defines import MAX_WAIT_TIME_CALL_DROP
from acts.test_utils.tel.tel_defines import MAX_WAIT_TIME_CALL_INITIATION
from acts.test_utils.tel.tel_defines import MAX_WAIT_TIME_CALLEE_RINGING
from acts.test_utils.tel.tel_defines import MAX_WAIT_TIME_CONNECTION_STATE_UPDATE
from acts.test_utils.tel.tel_defines import MAX_WAIT_TIME_DATA_SUB_CHANGE
from acts.test_utils.tel.tel_defines import MAX_WAIT_TIME_CALL_IDLE_EVENT
from acts.test_utils.tel.tel_defines import MAX_WAIT_TIME_NW_SELECTION
from acts.test_utils.tel.tel_defines import MAX_WAIT_TIME_SMS_RECEIVE
from acts.test_utils.tel.tel_defines import MAX_WAIT_TIME_SMS_SENT_SUCCESS
from acts.test_utils.tel.tel_defines import MAX_WAIT_TIME_TELECOM_RINGING
from acts.test_utils.tel.tel_defines import MAX_WAIT_TIME_VOICE_MAIL_COUNT
from acts.test_utils.tel.tel_defines import MAX_WAIT_TIME_VOLTE_ENABLED
from acts.test_utils.tel.tel_defines import MAX_WAIT_TIME_WFC_DISABLED
from acts.test_utils.tel.tel_defines import MAX_WAIT_TIME_WFC_ENABLED
from acts.test_utils.tel.tel_defines import WAIT_TIME_FOR_DATA_STALL
from acts.test_utils.tel.tel_defines import WAIT_TIME_FOR_NW_VALID_FAIL
from acts.test_utils.tel.tel_defines import WAIT_TIME_FOR_DATA_STALL_RECOVERY
from acts.test_utils.tel.tel_defines import NETWORK_MODE_LTE_ONLY
from acts.test_utils.tel.tel_defines import NETWORK_CONNECTION_TYPE_CELL
from acts.test_utils.tel.tel_defines import NETWORK_CONNECTION_TYPE_WIFI
from acts.test_utils.tel.tel_defines import NETWORK_SERVICE_DATA
from acts.test_utils.tel.tel_defines import NETWORK_SERVICE_VOICE
from acts.test_utils.tel.tel_defines import PHONE_NUMBER_STRING_FORMAT_7_DIGIT
from acts.test_utils.tel.tel_defines import PHONE_NUMBER_STRING_FORMAT_10_DIGIT
from acts.test_utils.tel.tel_defines import PHONE_NUMBER_STRING_FORMAT_11_DIGIT
from acts.test_utils.tel.tel_defines import PHONE_NUMBER_STRING_FORMAT_12_DIGIT
from acts.test_utils.tel.tel_defines import RAT_FAMILY_GSM
from acts.test_utils.tel.tel_defines import RAT_FAMILY_LTE
from acts.test_utils.tel.tel_defines import RAT_FAMILY_WLAN
from acts.test_utils.tel.tel_defines import RAT_FAMILY_WCDMA
from acts.test_utils.tel.tel_defines import RAT_1XRTT
from acts.test_utils.tel.tel_defines import RAT_UNKNOWN
from acts.test_utils.tel.tel_defines import SERVICE_STATE_EMERGENCY_ONLY
from acts.test_utils.tel.tel_defines import SERVICE_STATE_IN_SERVICE
from acts.test_utils.tel.tel_defines import SERVICE_STATE_MAPPING
from acts.test_utils.tel.tel_defines import SERVICE_STATE_OUT_OF_SERVICE
from acts.test_utils.tel.tel_defines import SERVICE_STATE_POWER_OFF
from acts.test_utils.tel.tel_defines import SIM_STATE_ABSENT
from acts.test_utils.tel.tel_defines import SIM_STATE_LOADED
from acts.test_utils.tel.tel_defines import SIM_STATE_NOT_READY
from acts.test_utils.tel.tel_defines import SIM_STATE_PIN_REQUIRED
from acts.test_utils.tel.tel_defines import SIM_STATE_READY
from acts.test_utils.tel.tel_defines import SIM_STATE_UNKNOWN
from acts.test_utils.tel.tel_defines import TELEPHONY_STATE_IDLE
from acts.test_utils.tel.tel_defines import TELEPHONY_STATE_OFFHOOK
from acts.test_utils.tel.tel_defines import TELEPHONY_STATE_RINGING
from acts.test_utils.tel.tel_defines import VOICEMAIL_DELETE_DIGIT
from acts.test_utils.tel.tel_defines import WAIT_TIME_1XRTT_VOICE_ATTACH
from acts.test_utils.tel.tel_defines import WAIT_TIME_ANDROID_STATE_SETTLING
from acts.test_utils.tel.tel_defines import WAIT_TIME_BETWEEN_STATE_CHECK
from acts.test_utils.tel.tel_defines import MAX_WAIT_TIME_FOR_STATE_CHANGE
from acts.test_utils.tel.tel_defines import WAIT_TIME_CHANGE_DATA_SUB_ID
from acts.test_utils.tel.tel_defines import WAIT_TIME_IN_CALL
from acts.test_utils.tel.tel_defines import WAIT_TIME_LEAVE_VOICE_MAIL
from acts.test_utils.tel.tel_defines import WAIT_TIME_REJECT_CALL
from acts.test_utils.tel.tel_defines import WAIT_TIME_VOICE_MAIL_SERVER_RESPONSE
from acts.test_utils.tel.tel_defines import WFC_MODE_DISABLED
from acts.test_utils.tel.tel_defines import WFC_MODE_CELLULAR_PREFERRED
from acts.test_utils.tel.tel_defines import WFC_MODE_WIFI_ONLY
from acts.test_utils.tel.tel_defines import WFC_MODE_WIFI_PREFERRED
from acts.test_utils.tel.tel_defines import TYPE_MOBILE
from acts.test_utils.tel.tel_defines import TYPE_WIFI
from acts.test_utils.tel.tel_defines import EventCallStateChanged
from acts.test_utils.tel.tel_defines import EventActiveDataSubIdChanged
from acts.test_utils.tel.tel_defines import EventConnectivityChanged
from acts.test_utils.tel.tel_defines import EventDataConnectionStateChanged
from acts.test_utils.tel.tel_defines import EventDataSmsReceived
from acts.test_utils.tel.tel_defines import EventMessageWaitingIndicatorChanged
from acts.test_utils.tel.tel_defines import EventServiceStateChanged
from acts.test_utils.tel.tel_defines import EventMmsSentFailure
from acts.test_utils.tel.tel_defines import EventMmsSentSuccess
from acts.test_utils.tel.tel_defines import EventMmsDownloaded
from acts.test_utils.tel.tel_defines import EventSmsReceived
from acts.test_utils.tel.tel_defines import EventSmsDeliverFailure
from acts.test_utils.tel.tel_defines import EventSmsDeliverSuccess
from acts.test_utils.tel.tel_defines import EventSmsSentFailure
from acts.test_utils.tel.tel_defines import EventSmsSentSuccess
from acts.test_utils.tel.tel_defines import CallStateContainer
from acts.test_utils.tel.tel_defines import DataConnectionStateContainer
from acts.test_utils.tel.tel_defines import MessageWaitingIndicatorContainer
from acts.test_utils.tel.tel_defines import NetworkCallbackContainer
from acts.test_utils.tel.tel_defines import ServiceStateContainer
from acts.test_utils.tel.tel_defines import CARRIER_VZW, CARRIER_ATT, \
    CARRIER_BELL, CARRIER_ROGERS, CARRIER_KOODO, CARRIER_VIDEOTRON, CARRIER_TELUS
from acts.test_utils.tel.tel_lookup_tables import connection_type_from_type_string
from acts.test_utils.tel.tel_lookup_tables import is_valid_rat
from acts.test_utils.tel.tel_lookup_tables import get_allowable_network_preference
from acts.test_utils.tel.tel_lookup_tables import get_voice_mail_count_check_function
from acts.test_utils.tel.tel_lookup_tables import get_voice_mail_check_number
from acts.test_utils.tel.tel_lookup_tables import get_voice_mail_delete_digit
from acts.test_utils.tel.tel_lookup_tables import network_preference_for_generation
from acts.test_utils.tel.tel_lookup_tables import operator_name_from_network_name
from acts.test_utils.tel.tel_lookup_tables import operator_name_from_plmn_id
from acts.test_utils.tel.tel_lookup_tables import rat_families_for_network_preference
from acts.test_utils.tel.tel_lookup_tables import rat_family_for_generation
from acts.test_utils.tel.tel_lookup_tables import rat_family_from_rat
from acts.test_utils.tel.tel_lookup_tables import rat_generation_from_rat
from acts.test_utils.tel.tel_subscription_utils import get_default_data_sub_id, get_subid_from_slot_index
from acts.test_utils.tel.tel_subscription_utils import get_outgoing_message_sub_id
from acts.test_utils.tel.tel_subscription_utils import get_outgoing_voice_sub_id
from acts.test_utils.tel.tel_subscription_utils import get_incoming_voice_sub_id
from acts.test_utils.tel.tel_subscription_utils import get_incoming_message_sub_id
from acts.test_utils.tel.tel_subscription_utils import set_subid_for_outgoing_call
from acts.test_utils.wifi import wifi_test_utils
from acts.test_utils.wifi import wifi_constants
from acts.utils import adb_shell_ping
from acts.utils import load_config
from acts.utils import start_standing_subprocess
from acts.utils import stop_standing_subprocess
from acts.logger import epoch_to_log_line_timestamp
from acts.logger import normalize_log_line_timestamp
from acts.utils import get_current_epoch_time
from acts.utils import exe_cmd


WIFI_SSID_KEY = wifi_test_utils.WifiEnums.SSID_KEY
WIFI_PWD_KEY = wifi_test_utils.WifiEnums.PWD_KEY
WIFI_CONFIG_APBAND_2G = wifi_test_utils.WifiEnums.WIFI_CONFIG_APBAND_2G
WIFI_CONFIG_APBAND_5G = wifi_test_utils.WifiEnums.WIFI_CONFIG_APBAND_5G
WIFI_CONFIG_APBAND_AUTO = wifi_test_utils.WifiEnums.WIFI_CONFIG_APBAND_AUTO
log = logging
STORY_LINE = "+19523521350"
CallResult = TelephonyVoiceTestResult.CallResult.Value


class TelTestUtilsError(Exception):
    pass


class TelResultWrapper(object):
    """Test results wrapper for Telephony test utils.

    In order to enable metrics reporting without refactoring
    all of the test utils this class is used to keep the
    current return boolean scheme in tact.
    """

    def __init__(self, result_value):
        self._result_value = result_value

    @property
    def result_value(self):
        return self._result_value

    @result_value.setter
    def result_value(self, result_value):
        self._result_value = result_value

    def __bool__(self):
        return self._result_value == CallResult('SUCCESS')


def abort_all_tests(log, msg):
    log.error("Aborting all ongoing tests due to: %s.", msg)
    abort_all(msg)


def get_phone_number_by_adb(ad):
    return phone_number_formatter(
        ad.adb.shell("service call iphonesubinfo 13"))


def get_iccid_by_adb(ad):
    return ad.adb.shell("service call iphonesubinfo 11")


def get_operator_by_adb(ad):
    operator = ad.adb.getprop("gsm.sim.operator.alpha")
    if "," in operator:
        operator = operator.strip()[0]
    return operator


def get_plmn_by_adb(ad):
    plmn_id = ad.adb.getprop("gsm.sim.operator.numeric")
    if "," in plmn_id:
        plmn_id = plmn_id.strip()[0]
    return plmn_id


def get_sub_id_by_adb(ad):
    return ad.adb.shell("service call iphonesubinfo 5")


def setup_droid_properties_by_adb(log, ad, sim_filename=None):

    sim_data = None
    if sim_filename:
        try:
            sim_data = load_config(sim_filename)
        except Exception:
            log.warning("Failed to load %s!", sim_filename)

    sub_id = get_sub_id_by_adb(ad)
    iccid = get_iccid_by_adb(ad)
    ad.log.info("iccid = %s", iccid)
    if sim_data.get(iccid) and sim_data[iccid].get("phone_num"):
        phone_number = phone_number_formatter(sim_data[iccid]["phone_num"])
    else:
        phone_number = get_phone_number_by_adb(ad)
        if not phone_number and hasattr(ad, phone_number):
            phone_number = ad.phone_number
    if not phone_number:
        ad.log.error("Failed to find valid phone number for %s", iccid)
        abort_all_tests(ad.log, "Failed to find valid phone number for %s")
    sub_record = {
        'phone_num': phone_number,
        'iccid': get_iccid_by_adb(ad),
        'sim_operator_name': get_operator_by_adb(ad),
        'operator': operator_name_from_plmn_id(get_plmn_by_adb(ad))
    }
    device_props = {'subscription': {sub_id: sub_record}}
    ad.log.info("subId %s SIM record: %s", sub_id, sub_record)
    setattr(ad, 'telephony', device_props)


def setup_droid_properties(log, ad, sim_filename=None):

    if ad.skip_sl4a:
        return setup_droid_properties_by_adb(
            log, ad, sim_filename=sim_filename)
    refresh_droid_config(log, ad)
    device_props = {}
    device_props['subscription'] = {}

    sim_data = {}
    if sim_filename:
        try:
            sim_data = load_config(sim_filename)
        except Exception:
            log.warning("Failed to load %s!", sim_filename)
    if not ad.telephony["subscription"]:
        abort_all_tests(ad.log, "No valid subscription")
    ad.log.debug("Subscription DB %s", ad.telephony["subscription"])
    result = True
    active_sub_id = get_outgoing_voice_sub_id(ad)
    for sub_id, sub_info in ad.telephony["subscription"].items():
        ad.log.debug("Loop for Subid %s", sub_id)
        sub_info["operator"] = get_operator_name(log, ad, sub_id)
        iccid = sub_info["iccid"]
        if not iccid:
            ad.log.warning("Unable to find ICC-ID for subscriber %s", sub_id)
            continue
        if sub_info.get("phone_num"):
            if iccid in sim_data and sim_data[iccid].get("phone_num"):
                if not check_phone_number_match(sim_data[iccid]["phone_num"],
                                                sub_info["phone_num"]):
                    ad.log.warning(
                        "phone_num %s in sim card data file for iccid %s"
                        "  do not match phone_num %s from subscription",
                        sim_data[iccid]["phone_num"], iccid,
                        sub_info["phone_num"])
                sub_info["phone_num"] = sim_data[iccid]["phone_num"]
        else:
            if iccid in sim_data and sim_data[iccid].get("phone_num"):
                sub_info["phone_num"] = sim_data[iccid]["phone_num"]
            elif sub_id == active_sub_id:
                phone_number = get_phone_number_by_secret_code(
                    ad, sub_info["sim_operator_name"])
                if phone_number:
                    sub_info["phone_num"] = phone_number
                elif getattr(ad, "phone_num", None):
                    sub_info["phone_num"] = ad.phone_number
        if (not sub_info.get("phone_num")) and sub_id == active_sub_id:
            ad.log.info("sub_id %s sub_info = %s", sub_id, sub_info)
            ad.log.error(
                "Unable to retrieve phone number for sub %s with iccid"
                " %s from device or testbed config or sim card file %s",
                sub_id, iccid, sim_filename)
            result = False
        if not hasattr(
                ad, 'roaming'
        ) and sub_info["sim_plmn"] != sub_info["network_plmn"] and sub_info["sim_operator_name"].strip(
        ) not in sub_info["network_operator_name"].strip():
            ad.log.info("roaming is not enabled, enable it")
            setattr(ad, 'roaming', True)
        ad.log.info("SubId %s info: %s", sub_id, sorted(sub_info.items()))
    get_phone_capability(ad)
    data_roaming = getattr(ad, 'roaming', False)
    if get_cell_data_roaming_state_by_adb(ad) != data_roaming:
        set_cell_data_roaming_state_by_adb(ad, data_roaming)
        # Setup VoWiFi MDN for Verizon. b/33187374
    if not result:
        abort_all_tests(ad.log, "Failed to find valid phone number")

    ad.log.debug("telephony = %s", ad.telephony)


def refresh_droid_config(log, ad):
    """ Update Android Device telephony records for each sub_id.

    Args:
        log: log object
        ad: android device object

    Returns:
        None
    """
    if not getattr(ad, 'telephony', {}):
        setattr(ad, 'telephony', {"subscription": {}})
    droid = ad.droid
    sub_info_list = droid.subscriptionGetAllSubInfoList()
    ad.log.info("SubInfoList is %s", sub_info_list)
    active_sub_id = get_outgoing_voice_sub_id(ad)
    for sub_info in sub_info_list:
        sub_id = sub_info["subscriptionId"]
        sim_slot = sub_info["simSlotIndex"]
        if sub_info.get("carrierId"):
            carrier_id = sub_info["carrierId"]
        else:
            carrier_id = -1

        if sim_slot != INVALID_SIM_SLOT_INDEX:
            if sub_id not in ad.telephony["subscription"]:
                ad.telephony["subscription"][sub_id] = {}
            sub_record = ad.telephony["subscription"][sub_id]
            if sub_info.get("iccId"):
                sub_record["iccid"] = sub_info["iccId"]
            else:
                sub_record[
                    "iccid"] = droid.telephonyGetSimSerialNumberForSubscription(
                        sub_id)
            sub_record["sim_slot"] = sim_slot
            if sub_info.get("mcc"):
                sub_record["mcc"] = sub_info["mcc"]
            if sub_info.get("mnc"):
                sub_record["mnc"] = sub_info["mnc"]
            if sub_info.get("displayName"):
                sub_record["display_name"] = sub_info["displayName"]
            try:
                sub_record[
                    "phone_type"] = droid.telephonyGetPhoneTypeForSubscription(
                        sub_id)
            except:
                if not sub_record.get("phone_type"):
                    sub_record["phone_type"] = droid.telephonyGetPhoneType()
            sub_record[
                "sim_plmn"] = droid.telephonyGetSimOperatorForSubscription(
                    sub_id)
            sub_record[
                "sim_operator_name"] = droid.telephonyGetSimOperatorNameForSubscription(
                    sub_id)
            sub_record[
                "network_plmn"] = droid.telephonyGetNetworkOperatorForSubscription(
                    sub_id)
            sub_record[
                "network_operator_name"] = droid.telephonyGetNetworkOperatorNameForSubscription(
                    sub_id)
            sub_record[
                "sim_country"] = droid.telephonyGetSimCountryIsoForSubscription(
                    sub_id)
            if active_sub_id == sub_id:
                try:
                    sub_record[
                        "carrier_id"] = ad.droid.telephonyGetSimCarrierId()
                    sub_record[
                        "carrier_id_name"] = ad.droid.telephonyGetSimCarrierIdName(
                        )
                except:
                    ad.log.info("Carrier ID is not supported")
            if carrier_id == 2340:
                ad.log.info("SubId %s info: %s", sub_id, sorted(
                    sub_record.items()))
                continue
            if not sub_info.get("number"):
                sub_info[
                    "number"] = droid.telephonyGetLine1NumberForSubscription(
                        sub_id)
            if sub_info.get("number"):
                if sub_record.get("phone_num"):
                    # Use the phone number provided in sim info file by default
                    # as the sub_info["number"] may not be formatted in a
                    # dialable number
                    if not check_phone_number_match(sub_info["number"],
                                                    sub_record["phone_num"]):
                        ad.log.info(
                            "Subscriber phone number changed from %s to %s",
                            sub_record["phone_num"], sub_info["number"])
                        sub_record["phone_num"] = sub_info["number"]
                else:
                    sub_record["phone_num"] = phone_number_formatter(
                        sub_info["number"])
            #ad.telephony['subscription'][sub_id] = sub_record
            ad.log.info("SubId %s info: %s", sub_id, sorted(
                sub_record.items()))


def get_phone_number_by_secret_code(ad, operator):
    if "T-Mobile" in operator:
        ad.droid.telecomDialNumber("#686#")
        ad.send_keycode("ENTER")
        for _ in range(12):
            output = ad.search_logcat("mobile number")
            if output:
                result = re.findall(r"mobile number is (\S+)",
                                    output[-1]["log_message"])
                ad.send_keycode("BACK")
                return result[0]
            else:
                time.sleep(5)
    return ""


def get_user_config_profile(ad):
    return {
        "Airplane Mode":
        ad.droid.connectivityCheckAirplaneMode(),
        "IMS Registered":
        ad.droid.telephonyIsImsRegistered(),
        "Preferred Network Type":
        ad.droid.telephonyGetPreferredNetworkTypes(),
        "VoLTE Platform Enabled":
        ad.droid.imsIsEnhanced4gLteModeSettingEnabledByPlatform(),
        "VoLTE Enabled":
        ad.droid.imsIsEnhanced4gLteModeSettingEnabledByUser(),
        "VoLTE Available":
        ad.droid.telephonyIsVolteAvailable(),
        "VT Available":
        ad.droid.telephonyIsVideoCallingAvailable(),
        "VT Enabled":
        ad.droid.imsIsVtEnabledByUser(),
        "VT Platform Enabled":
        ad.droid.imsIsVtEnabledByPlatform(),
        "WiFi State":
        ad.droid.wifiCheckState(),
        "WFC Available":
        ad.droid.telephonyIsWifiCallingAvailable(),
        "WFC Enabled":
        ad.droid.imsIsWfcEnabledByUser(),
        "WFC Platform Enabled":
        ad.droid.imsIsWfcEnabledByPlatform(),
        "WFC Mode":
        ad.droid.imsGetWfcMode()
    }


def get_slot_index_from_subid(log, ad, sub_id):
    try:
        info = ad.droid.subscriptionGetSubInfoForSubscriber(sub_id)
        return info['simSlotIndex']
    except KeyError:
        return INVALID_SIM_SLOT_INDEX


def get_num_active_sims(log, ad):
    """ Get the number of active SIM cards by counting slots

    Args:
        ad: android_device object.

    Returns:
        result: The number of loaded (physical) SIM cards
    """
    # using a dictionary as a cheap way to prevent double counting
    # in the situation where multiple subscriptions are on the same SIM.
    # yes, this is a corner corner case.
    valid_sims = {}
    subInfo = ad.droid.subscriptionGetAllSubInfoList()
    for info in subInfo:
        ssidx = info['simSlotIndex']
        if ssidx == INVALID_SIM_SLOT_INDEX:
            continue
        valid_sims[ssidx] = True
    return len(valid_sims.keys())


def toggle_airplane_mode_by_adb(log, ad, new_state=None):
    """ Toggle the state of airplane mode.

    Args:
        log: log handler.
        ad: android_device object.
        new_state: Airplane mode state to set to.
            If None, opposite of the current state.
        strict_checking: Whether to turn on strict checking that checks all features.

    Returns:
        result: True if operation succeed. False if error happens.
    """
    cur_state = bool(int(ad.adb.shell("settings get global airplane_mode_on")))
    if new_state == cur_state:
        ad.log.info("Airplane mode already in %s", new_state)
        return True
    elif new_state is None:
        new_state = not cur_state
    ad.log.info("Change airplane mode from %s to %s", cur_state, new_state)
    try:
        ad.adb.shell("settings put global airplane_mode_on %s" % int(new_state))
        ad.adb.shell("am broadcast -a android.intent.action.AIRPLANE_MODE")
    except Exception as e:
        ad.log.error(e)
        return False
    changed_state = bool(int(ad.adb.shell("settings get global airplane_mode_on")))
    return changed_state == new_state


def toggle_airplane_mode(log, ad, new_state=None, strict_checking=True):
    """ Toggle the state of airplane mode.

    Args:
        log: log handler.
        ad: android_device object.
        new_state: Airplane mode state to set to.
            If None, opposite of the current state.
        strict_checking: Whether to turn on strict checking that checks all features.

    Returns:
        result: True if operation succeed. False if error happens.
    """
    if ad.skip_sl4a:
        return toggle_airplane_mode_by_adb(log, ad, new_state)
    else:
        return toggle_airplane_mode_msim(
            log, ad, new_state, strict_checking=strict_checking)


def get_telephony_signal_strength(ad):
    #{'evdoEcio': -1, 'asuLevel': 28, 'lteSignalStrength': 14, 'gsmLevel': 0,
    # 'cdmaAsuLevel': 99, 'evdoDbm': -120, 'gsmDbm': -1, 'cdmaEcio': -160,
    # 'level': 2, 'lteLevel': 2, 'cdmaDbm': -120, 'dbm': -112, 'cdmaLevel': 0,
    # 'lteAsuLevel': 28, 'gsmAsuLevel': 99, 'gsmBitErrorRate': 0,
    # 'lteDbm': -112, 'gsmSignalStrength': 99}
    try:
        signal_strength = ad.droid.telephonyGetSignalStrength()
        if not signal_strength:
            signal_strength = {}
    except Exception as e:
        ad.log.error(e)
        signal_strength = {}
    return signal_strength


def get_wifi_signal_strength(ad):
    signal_strength = ad.droid.wifiGetConnectionInfo()['rssi']
    ad.log.info("WiFi Signal Strength is %s" % signal_strength)
    return signal_strength


def get_lte_rsrp(ad):
    try:
        if ad.adb.getprop("ro.build.version.release")[0] in ("9", "P"):
            out = ad.adb.shell(
                "dumpsys telephony.registry | grep -i signalstrength")
            if out:
                lte_rsrp = out.split()[9]
                if lte_rsrp:
                    ad.log.info("lte_rsrp: %s ", lte_rsrp)
                    return lte_rsrp
        else:
            out = ad.adb.shell(
            "dumpsys telephony.registry |grep -i primary=CellSignalStrengthLte")
            if out:
                lte_cell_info = out.split('mLte=')[1]
                lte_rsrp = re.match(r'.*rsrp=(\S+).*', lte_cell_info).group(1)
                if lte_rsrp:
                    ad.log.info("lte_rsrp: %s ", lte_rsrp)
                    return lte_rsrp
    except Exception as e:
        ad.log.error(e)
    return None


def check_data_stall_detection(ad, wait_time=WAIT_TIME_FOR_DATA_STALL):
    data_stall_detected = False
    time_var = 1
    try:
        while (time_var < wait_time):
            out = ad.adb.shell("dumpsys network_stack " \
                              "| grep \"Suspecting data stall\"",
                            ignore_status=True)
            ad.log.debug("Output is %s", out)
            if out:
                ad.log.info("NetworkMonitor detected - %s", out)
                data_stall_detected = True
                break
            time.sleep(30)
            time_var += 30
    except Exception as e:
        ad.log.error(e)
    return data_stall_detected


def check_network_validation_fail(ad, begin_time=None,
                                  wait_time=WAIT_TIME_FOR_NW_VALID_FAIL):
    network_validation_fail = False
    time_var = 1
    try:
        while (time_var < wait_time):
            time_var += 30
            nw_valid = ad.search_logcat("validation failed",
                                         begin_time)
            if nw_valid:
                ad.log.info("Validation Failed received here - %s",
                            nw_valid[0]["log_message"])
                network_validation_fail = True
                break
            time.sleep(30)
    except Exception as e:
        ad.log.error(e)
    return network_validation_fail


def check_data_stall_recovery(ad, begin_time=None,
                              wait_time=WAIT_TIME_FOR_DATA_STALL_RECOVERY):
    data_stall_recovery = False
    time_var = 1
    try:
        while (time_var < wait_time):
            time_var += 30
            recovery = ad.search_logcat("doRecovery() cleanup all connections",
                                         begin_time)
            if recovery:
                ad.log.info("Recovery Performed here - %s",
                            recovery[-1]["log_message"])
                data_stall_recovery = True
                break
            time.sleep(30)
    except Exception as e:
        ad.log.error(e)
    return data_stall_recovery


def break_internet_except_sl4a_port(ad, sl4a_port):
    ad.log.info("Breaking internet using iptables rules")
    ad.adb.shell("iptables -I INPUT 1 -p tcp --dport %s -j ACCEPT" % sl4a_port,
                 ignore_status=True)
    ad.adb.shell("iptables -I INPUT 2 -p tcp --sport %s -j ACCEPT" % sl4a_port,
                 ignore_status=True)
    ad.adb.shell("iptables -I INPUT 3 -j DROP", ignore_status=True)
    ad.adb.shell("ip6tables -I INPUT -j DROP", ignore_status=True)
    return True


def resume_internet_with_sl4a_port(ad, sl4a_port):
    ad.log.info("Bring internet back using iptables rules")
    ad.adb.shell("iptables -D INPUT -p tcp --dport %s -j ACCEPT" % sl4a_port,
                 ignore_status=True)
    ad.adb.shell("iptables -D INPUT -p tcp --sport %s -j ACCEPT" % sl4a_port,
                 ignore_status=True)
    ad.adb.shell("iptables -D INPUT -j DROP", ignore_status=True)
    ad.adb.shell("ip6tables -D INPUT -j DROP", ignore_status=True)
    return True


def test_data_browsing_success_using_sl4a(log, ad):
    result = True
    web_page_list = ['https://www.google.com', 'https://www.yahoo.com',
                     'https://www.amazon.com', 'https://www.nike.com',
                     'https://www.facebook.com']
    for website in web_page_list:
        if not verify_http_connection(log, ad, website, retry=0):
            ad.log.error("Failed to browse %s successfully!", website)
            result = False
    return result


def test_data_browsing_failure_using_sl4a(log, ad):
    result = True
    web_page_list = ['https://www.youtube.com', 'https://www.cnn.com',
                     'https://www.att.com', 'https://www.nbc.com',
                     'https://www.verizonwireless.com']
    for website in web_page_list:
        if not verify_http_connection(log, ad, website, retry=0,
                                      expected_state=False):
            ad.log.error("Browsing to %s worked!", website)
            result = False
    return result


def is_expected_event(event_to_check, events_list):
    """ check whether event is present in the event list

    Args:
        event_to_check: event to be checked.
        events_list: list of events
    Returns:
        result: True if event present in the list. False if not.
    """
    for event in events_list:
        if event in event_to_check['name']:
            return True
    return False


def is_sim_ready(log, ad, sim_slot_id=None):
    """ check whether SIM is ready.

    Args:
        ad: android_device object.
        sim_slot_id: check the SIM status for sim_slot_id
            This is optional. If this is None, check default SIM.

    Returns:
        result: True if all SIMs are ready. False if not.
    """
    if sim_slot_id is None:
        status = ad.droid.telephonyGetSimState()
    else:
        status = ad.droid.telephonyGetSimStateForSlotId(sim_slot_id)
    if status != SIM_STATE_READY:
        ad.log.info("Sim state is %s, not ready", status)
        return False
    return True


def is_sim_ready_by_adb(log, ad):
    state = ad.adb.getprop("gsm.sim.state")
    ad.log.info("gsm.sim.state = %s", state)
    return state == SIM_STATE_READY or state == SIM_STATE_LOADED


def wait_for_sim_ready_by_adb(log, ad, wait_time=90):
    return _wait_for_droid_in_state(log, ad, wait_time, is_sim_ready_by_adb)


def is_sims_ready_by_adb(log, ad):
    states = list(ad.adb.getprop("gsm.sim.state").split(","))
    ad.log.info("gsm.sim.state = %s", states)
    for state in states:
        if state != SIM_STATE_READY and state != SIM_STATE_LOADED:
            return False
    return True


def wait_for_sims_ready_by_adb(log, ad, wait_time=90):
    return _wait_for_droid_in_state(log, ad, wait_time, is_sims_ready_by_adb)


def get_service_state_by_adb(log, ad):
    output = ad.adb.shell("dumpsys telephony.registry | grep mServiceState")
    if "mVoiceRegState" in output:
        result = re.search(r"mVoiceRegState=(\S+)\((\S+)\)", output)
        if result:
            ad.log.info("mVoiceRegState is %s %s", result.group(1),
                        result.group(2))
            return result.group(2)
        else:
            if getattr(ad, "sdm_log", False):
                #look for all occurrence in string
                result2 = re.findall(r"mVoiceRegState=(\S+)\((\S+)\)", output)
                for voice_state in result2:
                    if voice_state[0] == 0:
                        ad.log.info("mVoiceRegState is 0 %s", voice_state[1])
                        return voice_state[1]
                return result2[1][1]
    else:
        result = re.search(r"mServiceState=(\S+)", output)
        if result:
            ad.log.info("mServiceState=%s %s", result.group(1),
                        SERVICE_STATE_MAPPING[result.group(1)])
            return SERVICE_STATE_MAPPING[result.group(1)]


def _is_expecting_event(event_recv_list):
    """ check for more event is expected in event list

    Args:
        event_recv_list: list of events
    Returns:
        result: True if more events are expected. False if not.
    """
    for state in event_recv_list:
        if state is False:
            return True
    return False


def _set_event_list(event_recv_list, sub_id_list, sub_id, value):
    """ set received event in expected event list

    Args:
        event_recv_list: list of received events
        sub_id_list: subscription ID list
        sub_id: subscription id of current event
        value: True or False
    Returns:
        None.
    """
    for i in range(len(sub_id_list)):
        if sub_id_list[i] == sub_id:
            event_recv_list[i] = value


def _wait_for_bluetooth_in_state(log, ad, state, max_wait):
    # FIXME: These event names should be defined in a common location
    _BLUETOOTH_STATE_ON_EVENT = 'BluetoothStateChangedOn'
    _BLUETOOTH_STATE_OFF_EVENT = 'BluetoothStateChangedOff'
    ad.ed.clear_events(_BLUETOOTH_STATE_ON_EVENT)
    ad.ed.clear_events(_BLUETOOTH_STATE_OFF_EVENT)

    ad.droid.bluetoothStartListeningForAdapterStateChange()
    try:
        bt_state = ad.droid.bluetoothCheckState()
        if bt_state == state:
            return True
        if max_wait <= 0:
            ad.log.error("Time out: bluetooth state still %s, expecting %s",
                         bt_state, state)
            return False

        event = {
            False: _BLUETOOTH_STATE_OFF_EVENT,
            True: _BLUETOOTH_STATE_ON_EVENT
        }[state]
        event = ad.ed.pop_event(event, max_wait)
        ad.log.info("Got event %s", event['name'])
        return True
    except Empty:
        ad.log.error("Time out: bluetooth state still in %s, expecting %s",
                     bt_state, state)
        return False
    finally:
        ad.droid.bluetoothStopListeningForAdapterStateChange()


# TODO: replace this with an event-based function
def _wait_for_wifi_in_state(log, ad, state, max_wait):
    return _wait_for_droid_in_state(log, ad, max_wait,
        lambda log, ad, state: \
                (True if ad.droid.wifiCheckState() == state else False),
                state)


def toggle_airplane_mode_msim(log, ad, new_state=None, strict_checking=True):
    """ Toggle the state of airplane mode.

    Args:
        log: log handler.
        ad: android_device object.
        new_state: Airplane mode state to set to.
            If None, opposite of the current state.
        strict_checking: Whether to turn on strict checking that checks all features.

    Returns:
        result: True if operation succeed. False if error happens.
    """

    cur_state = ad.droid.connectivityCheckAirplaneMode()
    if cur_state == new_state:
        ad.log.info("Airplane mode already in %s", new_state)
        return True
    elif new_state is None:
        new_state = not cur_state
        ad.log.info("Toggle APM mode, from current tate %s to %s", cur_state,
                    new_state)
    sub_id_list = []
    active_sub_info = ad.droid.subscriptionGetAllSubInfoList()
    if active_sub_info:
        for info in active_sub_info:
            sub_id_list.append(info['subscriptionId'])

    ad.ed.clear_all_events()
    time.sleep(0.1)
    service_state_list = []
    if new_state:
        service_state_list.append(SERVICE_STATE_POWER_OFF)
        ad.log.info("Turn on airplane mode")

    else:
        # If either one of these 3 events show up, it should be OK.
        # Normal SIM, phone in service
        service_state_list.append(SERVICE_STATE_IN_SERVICE)
        # NO SIM, or Dead SIM, or no Roaming coverage.
        service_state_list.append(SERVICE_STATE_OUT_OF_SERVICE)
        service_state_list.append(SERVICE_STATE_EMERGENCY_ONLY)
        ad.log.info("Turn off airplane mode")

    for sub_id in sub_id_list:
        ad.droid.telephonyStartTrackingServiceStateChangeForSubscription(
            sub_id)

    timeout_time = time.time() + MAX_WAIT_TIME_AIRPLANEMODE_EVENT
    ad.droid.connectivityToggleAirplaneMode(new_state)

    try:
        try:
            event = ad.ed.wait_for_event(
                EventServiceStateChanged,
                is_event_match_for_list,
                timeout=MAX_WAIT_TIME_AIRPLANEMODE_EVENT,
                field=ServiceStateContainer.SERVICE_STATE,
                value_list=service_state_list)
            ad.log.info("Got event %s", event)
        except Empty:
            ad.log.warning("Did not get expected service state change to %s",
                           service_state_list)
        finally:
            for sub_id in sub_id_list:
                ad.droid.telephonyStopTrackingServiceStateChangeForSubscription(
                    sub_id)
    except Exception as e:
        ad.log.error(e)

    # APM on (new_state=True) will turn off bluetooth but may not turn it on
    try:
        if new_state and not _wait_for_bluetooth_in_state(
                log, ad, False, timeout_time - time.time()):
            ad.log.error(
                "Failed waiting for bluetooth during airplane mode toggle")
            if strict_checking: return False
    except Exception as e:
        ad.log.error("Failed to check bluetooth state due to %s", e)
        if strict_checking:
            raise

    # APM on (new_state=True) will turn off wifi but may not turn it on
    if new_state and not _wait_for_wifi_in_state(log, ad, False,
                                                 timeout_time - time.time()):
        ad.log.error("Failed waiting for wifi during airplane mode toggle on")
        if strict_checking: return False

    if ad.droid.connectivityCheckAirplaneMode() != new_state:
        ad.log.error("Set airplane mode to %s failed", new_state)
        return False
    return True


def wait_and_answer_call(log,
                         ad,
                         incoming_number=None,
                         incall_ui_display=INCALL_UI_DISPLAY_FOREGROUND,
                         caller=None,
                         video_state=None):
    """Wait for an incoming call on default voice subscription and
       accepts the call.

    Args:
        ad: android device object.
        incoming_number: Expected incoming number.
            Optional. Default is None
        incall_ui_display: after answer the call, bring in-call UI to foreground or
            background. Optional, default value is INCALL_UI_DISPLAY_FOREGROUND.
            if = INCALL_UI_DISPLAY_FOREGROUND, bring in-call UI to foreground.
            if = INCALL_UI_DISPLAY_BACKGROUND, bring in-call UI to background.
            else, do nothing.

    Returns:
        True: if incoming call is received and answered successfully.
        False: for errors
        """
    return wait_and_answer_call_for_subscription(
        log,
        ad,
        get_incoming_voice_sub_id(ad),
        incoming_number,
        incall_ui_display=incall_ui_display,
        caller=caller,
        video_state=video_state)


def _wait_for_ringing_event(log, ad, wait_time):
    """Wait for ringing event.

    Args:
        log: log object.
        ad: android device object.
        wait_time: max time to wait for ringing event.

    Returns:
        event_ringing if received ringing event.
        otherwise return None.
    """
    event_ringing = None

    try:
        event_ringing = ad.ed.wait_for_event(
            EventCallStateChanged,
            is_event_match,
            timeout=wait_time,
            field=CallStateContainer.CALL_STATE,
            value=TELEPHONY_STATE_RINGING)
        ad.log.info("Receive ringing event")
    except Empty:
        ad.log.info("No Ringing Event")
    finally:
        return event_ringing


def wait_for_ringing_call(log, ad, incoming_number=None):
    """Wait for an incoming call on default voice subscription and
       accepts the call.

    Args:
        log: log object.
        ad: android device object.
        incoming_number: Expected incoming number.
            Optional. Default is None

    Returns:
        True: if incoming call is received and answered successfully.
        False: for errors
        """
    return wait_for_ringing_call_for_subscription(
        log, ad, get_incoming_voice_sub_id(ad), incoming_number)


def wait_for_ringing_call_for_subscription(
        log,
        ad,
        sub_id,
        incoming_number=None,
        caller=None,
        event_tracking_started=False,
        timeout=MAX_WAIT_TIME_CALLEE_RINGING,
        interval=WAIT_TIME_BETWEEN_STATE_CHECK):
    """Wait for an incoming call on specified subscription.

    Args:
        log: log object.
        ad: android device object.
        sub_id: subscription ID
        incoming_number: Expected incoming number. Default is None
        event_tracking_started: True if event tracking already state outside
        timeout: time to wait for ring
        interval: checking interval

    Returns:
        True: if incoming call is received and answered successfully.
        False: for errors
    """
    if not event_tracking_started:
        ad.ed.clear_events(EventCallStateChanged)
        ad.droid.telephonyStartTrackingCallStateForSubscription(sub_id)
    ring_event_received = False
    end_time = time.time() + timeout
    try:
        while time.time() < end_time:
            if not ring_event_received:
                event_ringing = _wait_for_ringing_event(log, ad, interval)
                if event_ringing:
                    if incoming_number and not check_phone_number_match(
                            event_ringing['data']
                        [CallStateContainer.INCOMING_NUMBER], incoming_number):
                        ad.log.error(
                            "Incoming Number not match. Expected number:%s, actual number:%s",
                            incoming_number, event_ringing['data'][
                                CallStateContainer.INCOMING_NUMBER])
                        return False
                    ring_event_received = True
            telephony_state = ad.droid.telephonyGetCallStateForSubscription(
                sub_id)
            telecom_state = ad.droid.telecomGetCallState()
            if telephony_state == TELEPHONY_STATE_RINGING and (
                    telecom_state == TELEPHONY_STATE_RINGING):
                ad.log.info("callee is in telephony and telecom RINGING state")
                if caller:
                    if caller.droid.telecomIsInCall():
                        caller.log.info("Caller telecom is in call state")
                        return True
                    else:
                        caller.log.info("Caller telecom is NOT in call state")
                else:
                    return True
            else:
                ad.log.info(
                    "telephony in %s, telecom in %s, expecting RINGING state",
                    telephony_state, telecom_state)
            time.sleep(interval)
    finally:
        if not event_tracking_started:
            ad.droid.telephonyStopTrackingCallStateChangeForSubscription(
                sub_id)


def wait_for_call_offhook_for_subscription(
        log,
        ad,
        sub_id,
        event_tracking_started=False,
        timeout=MAX_WAIT_TIME_ACCEPT_CALL_TO_OFFHOOK_EVENT,
        interval=WAIT_TIME_BETWEEN_STATE_CHECK):
    """Wait for an incoming call on specified subscription.

    Args:
        log: log object.
        ad: android device object.
        sub_id: subscription ID
        timeout: time to wait for ring
        interval: checking interval

    Returns:
        True: if incoming call is received and answered successfully.
        False: for errors
    """
    if not event_tracking_started:
        ad.ed.clear_events(EventCallStateChanged)
        ad.droid.telephonyStartTrackingCallStateForSubscription(sub_id)
    offhook_event_received = False
    end_time = time.time() + timeout
    try:
        while time.time() < end_time:
            if not offhook_event_received:
                if wait_for_call_offhook_event(log, ad, sub_id, True,
                                               interval):
                    offhook_event_received = True
            telephony_state = ad.droid.telephonyGetCallStateForSubscription(
                sub_id)
            telecom_state = ad.droid.telecomGetCallState()
            if telephony_state == TELEPHONY_STATE_OFFHOOK and (
                    telecom_state == TELEPHONY_STATE_OFFHOOK):
                ad.log.info("telephony and telecom are in OFFHOOK state")
                return True
            else:
                ad.log.info(
                    "telephony in %s, telecom in %s, expecting OFFHOOK state",
                    telephony_state, telecom_state)
            if offhook_event_received:
                time.sleep(interval)
    finally:
        if not event_tracking_started:
            ad.droid.telephonyStopTrackingCallStateChangeForSubscription(
                sub_id)


def wait_for_call_offhook_event(
        log,
        ad,
        sub_id,
        event_tracking_started=False,
        timeout=MAX_WAIT_TIME_ACCEPT_CALL_TO_OFFHOOK_EVENT):
    """Wait for an incoming call on specified subscription.

    Args:
        log: log object.
        ad: android device object.
        event_tracking_started: True if event tracking already state outside
        timeout: time to wait for event

    Returns:
        True: if call offhook event is received.
        False: if call offhook event is not received.
    """
    if not event_tracking_started:
        ad.ed.clear_events(EventCallStateChanged)
        ad.droid.telephonyStartTrackingCallStateForSubscription(sub_id)
    try:
        ad.ed.wait_for_event(
            EventCallStateChanged,
            is_event_match,
            timeout=timeout,
            field=CallStateContainer.CALL_STATE,
            value=TELEPHONY_STATE_OFFHOOK)
        ad.log.info("Got event %s", TELEPHONY_STATE_OFFHOOK)
    except Empty:
        ad.log.info("No event for call state change to OFFHOOK")
        return False
    finally:
        if not event_tracking_started:
            ad.droid.telephonyStopTrackingCallStateChangeForSubscription(
                sub_id)
    return True


def wait_and_answer_call_for_subscription(
        log,
        ad,
        sub_id,
        incoming_number=None,
        incall_ui_display=INCALL_UI_DISPLAY_FOREGROUND,
        timeout=MAX_WAIT_TIME_CALLEE_RINGING,
        caller=None,
        video_state=None):
    """Wait for an incoming call on specified subscription and
       accepts the call.

    Args:
        log: log object.
        ad: android device object.
        sub_id: subscription ID
        incoming_number: Expected incoming number.
            Optional. Default is None
        incall_ui_display: after answer the call, bring in-call UI to foreground or
            background. Optional, default value is INCALL_UI_DISPLAY_FOREGROUND.
            if = INCALL_UI_DISPLAY_FOREGROUND, bring in-call UI to foreground.
            if = INCALL_UI_DISPLAY_BACKGROUND, bring in-call UI to background.
            else, do nothing.

    Returns:
        True: if incoming call is received and answered successfully.
        False: for errors
    """
    ad.ed.clear_events(EventCallStateChanged)
    ad.droid.telephonyStartTrackingCallStateForSubscription(sub_id)
    try:
        if not wait_for_ringing_call_for_subscription(
                log,
                ad,
                sub_id,
                incoming_number=incoming_number,
                caller=caller,
                event_tracking_started=True,
                timeout=timeout):
            ad.log.info("Incoming call ringing check failed.")
            return False
        ad.log.info("Accept the ring call")
        ad.droid.telecomAcceptRingingCall(video_state)

        if wait_for_call_offhook_for_subscription(
                log, ad, sub_id, event_tracking_started=True):
            return True
        else:
            ad.log.error("Could not answer the call.")
            return False
    except Exception as e:
        log.error(e)
        return False
    finally:
        ad.droid.telephonyStopTrackingCallStateChangeForSubscription(sub_id)
        if incall_ui_display == INCALL_UI_DISPLAY_FOREGROUND:
            ad.droid.telecomShowInCallScreen()
        elif incall_ui_display == INCALL_UI_DISPLAY_BACKGROUND:
            ad.droid.showHomeScreen()


def wait_and_reject_call(log,
                         ad,
                         incoming_number=None,
                         delay_reject=WAIT_TIME_REJECT_CALL,
                         reject=True):
    """Wait for an incoming call on default voice subscription and
       reject the call.

    Args:
        log: log object.
        ad: android device object.
        incoming_number: Expected incoming number.
            Optional. Default is None
        delay_reject: time to wait before rejecting the call
            Optional. Default is WAIT_TIME_REJECT_CALL

    Returns:
        True: if incoming call is received and reject successfully.
        False: for errors
    """
    return wait_and_reject_call_for_subscription(log, ad,
                                                 get_incoming_voice_sub_id(ad),
                                                 incoming_number, delay_reject,
                                                 reject)


def wait_and_reject_call_for_subscription(log,
                                          ad,
                                          sub_id,
                                          incoming_number=None,
                                          delay_reject=WAIT_TIME_REJECT_CALL,
                                          reject=True):
    """Wait for an incoming call on specific subscription and
       reject the call.

    Args:
        log: log object.
        ad: android device object.
        sub_id: subscription ID
        incoming_number: Expected incoming number.
            Optional. Default is None
        delay_reject: time to wait before rejecting the call
            Optional. Default is WAIT_TIME_REJECT_CALL

    Returns:
        True: if incoming call is received and reject successfully.
        False: for errors
    """

    if not wait_for_ringing_call_for_subscription(log, ad, sub_id,
                                                  incoming_number):
        ad.log.error(
            "Could not reject a call: incoming call in ringing check failed.")
        return False

    ad.ed.clear_events(EventCallStateChanged)
    ad.droid.telephonyStartTrackingCallStateForSubscription(sub_id)
    if reject is True:
        # Delay between ringing and reject.
        time.sleep(delay_reject)
        is_find = False
        # Loop the call list and find the matched one to disconnect.
        for call in ad.droid.telecomCallGetCallIds():
            if check_phone_number_match(
                    get_number_from_tel_uri(get_call_uri(ad, call)),
                    incoming_number):
                ad.droid.telecomCallDisconnect(call)
                ad.log.info("Callee reject the call")
                is_find = True
        if is_find is False:
            ad.log.error("Callee did not find matching call to reject.")
            return False
    else:
        # don't reject on callee. Just ignore the incoming call.
        ad.log.info("Callee received incoming call. Ignore it.")
    try:
        ad.ed.wait_for_event(
            EventCallStateChanged,
            is_event_match_for_list,
            timeout=MAX_WAIT_TIME_CALL_IDLE_EVENT,
            field=CallStateContainer.CALL_STATE,
            value_list=[TELEPHONY_STATE_IDLE, TELEPHONY_STATE_OFFHOOK])
    except Empty:
        ad.log.error("No onCallStateChangedIdle event received.")
        return False
    finally:
        ad.droid.telephonyStopTrackingCallStateChangeForSubscription(sub_id)
    return True


def hangup_call(log, ad, is_emergency=False):
    """Hang up ongoing active call.

    Args:
        log: log object.
        ad: android device object.

    Returns:
        True: if all calls are cleared
        False: for errors
    """
    # short circuit in case no calls are active
    if not ad.droid.telecomIsInCall():
        return True
    ad.ed.clear_events(EventCallStateChanged)
    ad.droid.telephonyStartTrackingCallState()
    ad.log.info("Hangup call.")
    if is_emergency:
        for call in ad.droid.telecomCallGetCallIds():
            ad.droid.telecomCallDisconnect(call)
    else:
        ad.droid.telecomEndCall()

    try:
        ad.ed.wait_for_event(
            EventCallStateChanged,
            is_event_match,
            timeout=MAX_WAIT_TIME_CALL_IDLE_EVENT,
            field=CallStateContainer.CALL_STATE,
            value=TELEPHONY_STATE_IDLE)
    except Empty:
        ad.log.warning("Call state IDLE event is not received after hang up.")
    finally:
        ad.droid.telephonyStopTrackingCallStateChange()
    if not wait_for_state(ad.droid.telecomIsInCall, False, 15, 1):
        ad.log.error("Telecom is in call, hangup call failed.")
        return False
    return True


def wait_for_cbrs_data_active_sub_change_event(
        ad,
        event_tracking_started=False,
        timeout=120):
    """Wait for an data change event on specified subscription.

    Args:
        ad: android device object.
        event_tracking_started: True if event tracking already state outside
        timeout: time to wait for event

    Returns:
        True: if data change event is received.
        False: if data change event is not received.
    """
    if not event_tracking_started:
        ad.ed.clear_events(EventActiveDataSubIdChanged)
        ad.droid.telephonyStartTrackingActiveDataChange()
    try:
        ad.ed.wait_for_event(
            EventActiveDataSubIdChanged,
            is_event_match,
            timeout=timeout)
        ad.log.info("Got event activedatasubidchanged")
    except Empty:
        ad.log.info("No event for data subid change")
        return False
    finally:
        if not event_tracking_started:
            ad.droid.telephonyStopTrackingActiveDataChange()
    return True


def is_current_data_on_cbrs(ad, cbrs_subid):
    """Verifies if current data sub is on CBRS

    Args:
        ad: android device object.
        cbrs_subid: sub_id against which we need to check

    Returns:
        True: if data is on cbrs
        False: if data is not on cbrs
    """
    if cbrs_subid is None:
        return False
    current_data = ad.droid.subscriptionGetActiveDataSubscriptionId()
    ad.log.info("Current Data subid %s cbrs_subid %s", current_data, cbrs_subid)
    if current_data == cbrs_subid:
        return True
    else:
        return False


def disconnect_call_by_id(log, ad, call_id):
    """Disconnect call by call id.
    """
    ad.droid.telecomCallDisconnect(call_id)
    return True


def _phone_number_remove_prefix(number):
    """Remove the country code and other prefix from the input phone number.
    Currently only handle phone number with the following formats:
        (US phone number format)
        +1abcxxxyyyy
        1abcxxxyyyy
        abcxxxyyyy
        abc xxx yyyy
        abc.xxx.yyyy
        abc-xxx-yyyy
        (EEUK phone number format)
        +44abcxxxyyyy
        0abcxxxyyyy

    Args:
        number: input phone number

    Returns:
        Phone number without country code or prefix
    """
    if number is None:
        return None, None
    for country_code in COUNTRY_CODE_LIST:
        if number.startswith(country_code):
            return number[len(country_code):], country_code
    if number[0] == "1" or number[0] == "0":
        return number[1:], None
    return number, None


def check_phone_number_match(number1, number2):
    """Check whether two input phone numbers match or not.

    Compare the two input phone numbers.
    If they match, return True; otherwise, return False.
    Currently only handle phone number with the following formats:
        (US phone number format)
        +1abcxxxyyyy
        1abcxxxyyyy
        abcxxxyyyy
        abc xxx yyyy
        abc.xxx.yyyy
        abc-xxx-yyyy
        (EEUK phone number format)
        +44abcxxxyyyy
        0abcxxxyyyy

        There are some scenarios we can not verify, one example is:
            number1 = +15555555555, number2 = 5555555555
            (number2 have no country code)

    Args:
        number1: 1st phone number to be compared.
        number2: 2nd phone number to be compared.

    Returns:
        True if two phone numbers match. Otherwise False.
    """
    number1 = phone_number_formatter(number1)
    number2 = phone_number_formatter(number2)
    # Handle extra country code attachment when matching phone number
    if number1[-7:] in number2 or number2[-7:] in number1:
        return True
    else:
        logging.info("phone number1 %s and number2 %s does not match" %
                     (number1, number2))
        return False


def initiate_call(log,
                  ad,
                  callee_number,
                  emergency=False,
                  timeout=MAX_WAIT_TIME_CALL_INITIATION,
                  checking_interval=5,
                  incall_ui_display=INCALL_UI_DISPLAY_FOREGROUND,
                  video=False):
    """Make phone call from caller to callee.

    Args:
        ad_caller: Caller android device object.
        callee_number: Callee phone number.
        emergency : specify the call is emergency.
            Optional. Default value is False.
        incall_ui_display: show the dialer UI foreground or backgroud
        video: whether to initiate as video call

    Returns:
        result: if phone call is placed successfully.
    """
    ad.ed.clear_events(EventCallStateChanged)
    sub_id = get_outgoing_voice_sub_id(ad)
    begin_time = get_device_epoch_time(ad)
    ad.droid.telephonyStartTrackingCallStateForSubscription(sub_id)
    try:
        # Make a Call
        ad.log.info("Make a phone call to %s", callee_number)
        if emergency:
            ad.droid.telecomCallEmergencyNumber(callee_number)
        else:
            ad.droid.telecomCallNumber(callee_number, video)

        # Verify OFFHOOK state
        if not wait_for_call_offhook_for_subscription(
                log, ad, sub_id, event_tracking_started=True):
            ad.log.info("sub_id %s not in call offhook state", sub_id)
            last_call_drop_reason(ad, begin_time=begin_time)
            return False
        else:
            return True
    finally:
        if hasattr(ad, "sdm_log") and getattr(ad, "sdm_log"):
            ad.adb.shell("i2cset -fy 3 64 6 1 b", ignore_status=True)
            ad.adb.shell("i2cset -fy 3 65 6 1 b", ignore_status=True)
        ad.droid.telephonyStopTrackingCallStateChangeForSubscription(sub_id)
        if incall_ui_display == INCALL_UI_DISPLAY_FOREGROUND:
            ad.droid.telecomShowInCallScreen()
        elif incall_ui_display == INCALL_UI_DISPLAY_BACKGROUND:
            ad.droid.showHomeScreen()


def dial_phone_number(ad, callee_number):
    for number in str(callee_number):
        if number == "#":
            ad.send_keycode("POUND")
        elif number == "*":
            ad.send_keycode("STAR")
        elif number in ["1", "2", "3", "4", "5", "6", "7", "8", "9", "0"]:
            ad.send_keycode("%s" % number)


def get_call_state_by_adb(ad):
    slot_index_of_default_voice_subid = get_slot_index_from_subid(ad.log, ad,
        get_incoming_voice_sub_id(ad))
    output = ad.adb.shell("dumpsys telephony.registry | grep mCallState")
    if "mCallState" in output:
        call_state_list = re.findall("mCallState=(\d)", output)
        if call_state_list:
            return call_state_list[slot_index_of_default_voice_subid]


def check_call_state_connected_by_adb(ad):
    return "2" in get_call_state_by_adb(ad)


def check_call_state_idle_by_adb(ad):
    return "0" in get_call_state_by_adb(ad)


def check_call_state_ring_by_adb(ad):
    return "1" in get_call_state_by_adb(ad)


def get_incoming_call_number_by_adb(ad):
    output = ad.adb.shell(
        "dumpsys telephony.registry | grep mCallIncomingNumber")
    return re.search(r"mCallIncomingNumber=(.*)", output).group(1)


def emergency_dialer_call_by_keyevent(ad, callee_number):
    for i in range(3):
        if "EmergencyDialer" in ad.get_my_current_focus_window():
            ad.log.info("EmergencyDialer is the current focus window")
            break
        elif i <= 2:
            ad.adb.shell("am start -a com.android.phone.EmergencyDialer.DIAL")
            time.sleep(1)
        else:
            ad.log.error("Unable to bring up EmergencyDialer")
            return False
    ad.log.info("Make a phone call to %s", callee_number)
    dial_phone_number(ad, callee_number)
    ad.send_keycode("CALL")


def initiate_emergency_dialer_call_by_adb(
        log,
        ad,
        callee_number,
        timeout=MAX_WAIT_TIME_CALL_INITIATION,
        checking_interval=5):
    """Make emergency call by EmergencyDialer.

    Args:
        ad: Caller android device object.
        callee_number: Callee phone number.
        emergency : specify the call is emergency.
        Optional. Default value is False.

    Returns:
        result: if phone call is placed successfully.
    """
    try:
        # Make a Call
        ad.wakeup_screen()
        ad.send_keycode("MENU")
        ad.log.info("Call %s", callee_number)
        ad.adb.shell("am start -a com.android.phone.EmergencyDialer.DIAL")
        ad.adb.shell(
            "am start -a android.intent.action.CALL_EMERGENCY -d tel:%s" %
            callee_number)
        if not timeout: return True
        ad.log.info("Check call state")
        # Verify Call State
        elapsed_time = 0
        while elapsed_time < timeout:
            time.sleep(checking_interval)
            elapsed_time += checking_interval
            if check_call_state_connected_by_adb(ad):
                ad.log.info("Call to %s is connected", callee_number)
                return True
            if check_call_state_idle_by_adb(ad):
                ad.log.info("Call to %s failed", callee_number)
                return False
        ad.log.info("Make call to %s failed", callee_number)
        return False
    except Exception as e:
        ad.log.error("initiate emergency call failed with error %s", e)


def hangup_call_by_adb(ad):
    """Make emergency call by EmergencyDialer.

    Args:
        ad: Caller android device object.
        callee_number: Callee phone number.
    """
    ad.log.info("End call by adb")
    ad.send_keycode("ENDCALL")


def dumpsys_all_call_info(ad):
    """ Get call information by dumpsys telecom. """
    output = ad.adb.shell("dumpsys telecom")
    calls = re.findall("Call TC@\d+: {(.*?)}", output, re.DOTALL)
    calls_info = []
    for call in calls:
        call_info = {}
        for attr in ("startTime", "endTime", "direction", "isInterrupted",
                     "callTechnologies", "callTerminationsReason",
                     "connectionService", "isVideoCall", "callProperties"):
            match = re.search(r"%s: (.*)" % attr, call)
            if match:
                if attr in ("startTime", "endTime"):
                    call_info[attr] = epoch_to_log_line_timestamp(
                        int(match.group(1)))
                else:
                    call_info[attr] = match.group(1)
        call_info["inCallServices"] = re.findall(r"name: (.*)", call)
        calls_info.append(call_info)
    ad.log.debug("calls_info = %s", calls_info)
    return calls_info


def dumpsys_last_call_info(ad):
    """ Get call information by dumpsys telecom. """
    num = dumpsys_last_call_number(ad)
    output = ad.adb.shell("dumpsys telecom")
    result = re.search(r"Call TC@%s: {(.*?)}" % num, output, re.DOTALL)
    call_info = {"TC": num}
    if result:
        result = result.group(1)
        for attr in ("startTime", "endTime", "direction", "isInterrupted",
                     "callTechnologies", "callTerminationsReason",
                     "isVideoCall", "callProperties"):
            match = re.search(r"%s: (.*)" % attr, result)
            if match:
                if attr in ("startTime", "endTime"):
                    call_info[attr] = epoch_to_log_line_timestamp(
                        int(match.group(1)))
                else:
                    call_info[attr] = match.group(1)
    ad.log.debug("call_info = %s", call_info)
    return call_info


def dumpsys_last_call_number(ad):
    output = ad.adb.shell("dumpsys telecom")
    call_nums = re.findall("Call TC@(\d+):", output)
    if not call_nums:
        return 0
    else:
        return int(call_nums[-1])


def dumpsys_new_call_info(ad, last_tc_number, retries=3, interval=5):
    for i in range(retries):
        if dumpsys_last_call_number(ad) > last_tc_number:
            call_info = dumpsys_last_call_info(ad)
            ad.log.info("New call info = %s", sorted(call_info.items()))
            return call_info
        else:
            time.sleep(interval)
    ad.log.error("New call is not in sysdump telecom")
    return {}


def dumpsys_carrier_config(ad):
    output = ad.adb.shell("dumpsys carrier_config").split("\n")
    output_phone_id_0 = []
    output_phone_id_1 = []
    current_output = []
    for line in output:
        if "Phone Id = 0" in line:
            current_output = output_phone_id_0
        elif "Phone Id = 1" in line:
            current_output = output_phone_id_1
        current_output.append(line.strip())

    configs = {}
    if ad.adb.getprop("ro.build.version.release")[0] in ("9", "P"):
        phone_count = 1
        if "," in ad.adb.getprop("gsm.network.type"):
            phone_count = 2
    else:
        phone_count = ad.droid.telephonyGetPhoneCount()

    slot_0_subid = get_subid_from_slot_index(ad.log, ad, 0)
    if slot_0_subid != INVALID_SUB_ID:
        configs[slot_0_subid] = {}

    if phone_count == 2:
        slot_1_subid = get_subid_from_slot_index(ad.log, ad, 1)
        if slot_1_subid != INVALID_SUB_ID:
            configs[slot_1_subid] = {}

    attrs = [attr for attr in dir(CarrierConfigs) if not attr.startswith("__")]
    for attr in attrs:
        attr_string = getattr(CarrierConfigs, attr)
        values = re.findall(
            r"%s = (\S+)" % attr_string, "\n".join(output_phone_id_0))

        if slot_0_subid != INVALID_SUB_ID:
            if values:
                value = values[-1]
                if value == "true":
                    configs[slot_0_subid][attr_string] = True
                elif value == "false":
                    configs[slot_0_subid][attr_string] = False
                elif attr_string == CarrierConfigs.DEFAULT_WFC_IMS_MODE_INT:
                    if value == "0":
                        configs[slot_0_subid][attr_string] = WFC_MODE_WIFI_ONLY
                    elif value == "1":
                        configs[slot_0_subid][attr_string] = \
                            WFC_MODE_CELLULAR_PREFERRED
                    elif value == "2":
                        configs[slot_0_subid][attr_string] = \
                            WFC_MODE_WIFI_PREFERRED
                else:
                    try:
                        configs[slot_0_subid][attr_string] = int(value)
                    except Exception:
                        configs[slot_0_subid][attr_string] = value
            else:
                configs[slot_0_subid][attr_string] = None

        if phone_count == 2:
            if slot_1_subid != INVALID_SUB_ID:
                values = re.findall(
                    r"%s = (\S+)" % attr_string, "\n".join(output_phone_id_1))
                if values:
                    value = values[-1]
                    if value == "true":
                        configs[slot_1_subid][attr_string] = True
                    elif value == "false":
                        configs[slot_1_subid][attr_string] = False
                    elif attr_string == CarrierConfigs.DEFAULT_WFC_IMS_MODE_INT:
                        if value == "0":
                            configs[slot_1_subid][attr_string] = \
                                WFC_MODE_WIFI_ONLY
                        elif value == "1":
                            configs[slot_1_subid][attr_string] = \
                                WFC_MODE_CELLULAR_PREFERRED
                        elif value == "2":
                            configs[slot_1_subid][attr_string] = \
                                WFC_MODE_WIFI_PREFERRED
                    else:
                        try:
                            configs[slot_1_subid][attr_string] = int(value)
                        except Exception:
                            configs[slot_1_subid][attr_string] = value
                else:
                    configs[slot_1_subid][attr_string] = None
    return configs


def get_phone_capability(ad):
    carrier_configs = dumpsys_carrier_config(ad)
    for sub_id in carrier_configs:
        capabilities = []
        if carrier_configs[sub_id][CarrierConfigs.VOLTE_AVAILABLE_BOOL]:
            capabilities.append(CAPABILITY_VOLTE)
        if carrier_configs[sub_id][CarrierConfigs.WFC_IMS_AVAILABLE_BOOL]:
            capabilities.append(CAPABILITY_WFC)
        if carrier_configs[sub_id][CarrierConfigs.EDITABLE_WFC_MODE_BOOL]:
            capabilities.append(CAPABILITY_WFC_MODE_CHANGE)
        if carrier_configs[sub_id][CarrierConfigs.SUPPORT_CONFERENCE_CALL_BOOL]:
            capabilities.append(CAPABILITY_CONFERENCE)
        if carrier_configs[sub_id][CarrierConfigs.VT_AVAILABLE_BOOL]:
            capabilities.append(CAPABILITY_VT)
        if carrier_configs[sub_id][CarrierConfigs.VOLTE_PROVISIONED_BOOL]:
            capabilities.append(CAPABILITY_VOLTE_PROVISIONING)
        if carrier_configs[sub_id][CarrierConfigs.VOLTE_OVERRIDE_WFC_BOOL]:
            capabilities.append(CAPABILITY_VOLTE_OVERRIDE_WFC_PROVISIONING)
        ad.log.info("Capabilities of sub ID %s: %s", sub_id, capabilities)
        if not getattr(ad, 'telephony', {}):
            ad.telephony["subscription"] = {}
            ad.telephony["subscription"][sub_id] = {}
            setattr(
                ad.telephony["subscription"][sub_id],
                'capabilities', capabilities)

        else:
            ad.telephony["subscription"][sub_id]["capabilities"] = capabilities
        if CAPABILITY_WFC not in capabilities:
            wfc_modes = []
        else:
            if carrier_configs[sub_id].get(
                CarrierConfigs.EDITABLE_WFC_MODE_BOOL, False):
                wfc_modes = [
                    WFC_MODE_CELLULAR_PREFERRED,
                    WFC_MODE_WIFI_PREFERRED]
            else:
                wfc_modes = [
                    carrier_configs[sub_id].get(
                        CarrierConfigs.DEFAULT_WFC_IMS_MODE_INT,
                        WFC_MODE_CELLULAR_PREFERRED)
                ]
        if carrier_configs[sub_id].get(
            CarrierConfigs.WFC_SUPPORTS_WIFI_ONLY_BOOL,
            False) and WFC_MODE_WIFI_ONLY not in wfc_modes:
            wfc_modes.append(WFC_MODE_WIFI_ONLY)
        ad.telephony["subscription"][sub_id]["wfc_modes"] = wfc_modes
        if wfc_modes:
            ad.log.info("Supported WFC modes for sub ID %s: %s", sub_id,
                wfc_modes)


def get_capability_for_subscription(ad, capability, subid):
    if capability in ad.telephony["subscription"][subid].get(
        "capabilities", []):
        ad.log.info('Capability "%s" is available for sub ID %s.',
            capability, subid)
        return True
    else:
        ad.log.info('Capability "%s" is NOT available for sub ID %s.',
            capability, subid)
        return False


def call_reject(log, ad_caller, ad_callee, reject=True):
    """Caller call Callee, then reject on callee.


    """
    subid_caller = ad_caller.droid.subscriptionGetDefaultVoiceSubId()
    subid_callee = ad_callee.incoming_voice_sub_id
    ad_caller.log.info("Sub-ID Caller %s, Sub-ID Callee %s", subid_caller,
                       subid_callee)
    return call_reject_for_subscription(log, ad_caller, ad_callee,
                                        subid_caller, subid_callee, reject)


def call_reject_for_subscription(log,
                                 ad_caller,
                                 ad_callee,
                                 subid_caller,
                                 subid_callee,
                                 reject=True):
    """
    """

    caller_number = ad_caller.telephony['subscription'][subid_caller][
        'phone_num']
    callee_number = ad_callee.telephony['subscription'][subid_callee][
        'phone_num']

    ad_caller.log.info("Call from %s to %s", caller_number, callee_number)
    if not initiate_call(log, ad_caller, callee_number):
        ad_caller.log.error("Initiate call failed")
        return False

    if not wait_and_reject_call_for_subscription(
            log, ad_callee, subid_callee, caller_number, WAIT_TIME_REJECT_CALL,
            reject):
        ad_callee.log.error("Reject call fail.")
        return False
    # Check if incoming call is cleared on callee or not.
    if ad_callee.droid.telephonyGetCallStateForSubscription(
            subid_callee) == TELEPHONY_STATE_RINGING:
        ad_callee.log.error("Incoming call is not cleared")
        return False
    # Hangup on caller
    hangup_call(log, ad_caller)
    return True


def call_reject_leave_message(log,
                              ad_caller,
                              ad_callee,
                              verify_caller_func=None,
                              wait_time_in_call=WAIT_TIME_LEAVE_VOICE_MAIL):
    """On default voice subscription, Call from caller to callee,
    reject on callee, caller leave a voice mail.

    1. Caller call Callee.
    2. Callee reject incoming call.
    3. Caller leave a voice mail.
    4. Verify callee received the voice mail notification.

    Args:
        ad_caller: caller android device object.
        ad_callee: callee android device object.
        verify_caller_func: function to verify caller is in correct state while in-call.
            This is optional, default is None.
        wait_time_in_call: time to wait when leaving a voice mail.
            This is optional, default is WAIT_TIME_LEAVE_VOICE_MAIL

    Returns:
        True: if voice message is received on callee successfully.
        False: for errors
    """
    subid_caller = get_outgoing_voice_sub_id(ad_caller)
    subid_callee = get_incoming_voice_sub_id(ad_callee)
    return call_reject_leave_message_for_subscription(
        log, ad_caller, ad_callee, subid_caller, subid_callee,
        verify_caller_func, wait_time_in_call)


def call_reject_leave_message_for_subscription(
        log,
        ad_caller,
        ad_callee,
        subid_caller,
        subid_callee,
        verify_caller_func=None,
        wait_time_in_call=WAIT_TIME_LEAVE_VOICE_MAIL):
    """On specific voice subscription, Call from caller to callee,
    reject on callee, caller leave a voice mail.

    1. Caller call Callee.
    2. Callee reject incoming call.
    3. Caller leave a voice mail.
    4. Verify callee received the voice mail notification.

    Args:
        ad_caller: caller android device object.
        ad_callee: callee android device object.
        subid_caller: caller's subscription id.
        subid_callee: callee's subscription id.
        verify_caller_func: function to verify caller is in correct state while in-call.
            This is optional, default is None.
        wait_time_in_call: time to wait when leaving a voice mail.
            This is optional, default is WAIT_TIME_LEAVE_VOICE_MAIL

    Returns:
        True: if voice message is received on callee successfully.
        False: for errors
    """

    # Currently this test utility only works for TMO and ATT and SPT.
    # It does not work for VZW (see b/21559800)
    # "with VVM TelephonyManager APIs won't work for vm"

    caller_number = ad_caller.telephony['subscription'][subid_caller][
        'phone_num']
    callee_number = ad_callee.telephony['subscription'][subid_callee][
        'phone_num']

    ad_caller.log.info("Call from %s to %s", caller_number, callee_number)

    try:
        voice_mail_count_before = ad_callee.droid.telephonyGetVoiceMailCountForSubscription(
            subid_callee)
        ad_callee.log.info("voice mail count is %s", voice_mail_count_before)
        # -1 means there are unread voice mail, but the count is unknown
        # 0 means either this API not working (VZW) or no unread voice mail.
        if voice_mail_count_before != 0:
            log.warning("--Pending new Voice Mail, please clear on phone.--")

        if not initiate_call(log, ad_caller, callee_number):
            ad_caller.log.error("Initiate call failed.")
            return False

        if not wait_and_reject_call_for_subscription(
                log, ad_callee, subid_callee, incoming_number=caller_number):
            ad_callee.log.error("Reject call fail.")
            return False

        ad_callee.droid.telephonyStartTrackingVoiceMailStateChangeForSubscription(
            subid_callee)

        # ensure that all internal states are updated in telecom
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)
        ad_callee.ed.clear_events(EventCallStateChanged)

        if verify_caller_func and not verify_caller_func(log, ad_caller):
            ad_caller.log.error("Caller not in correct state!")
            return False

        # TODO: b/26293512 Need to play some sound to leave message.
        # Otherwise carrier voice mail server may drop this voice mail.
        time.sleep(wait_time_in_call)

        if not verify_caller_func:
            caller_state_result = ad_caller.droid.telecomIsInCall()
        else:
            caller_state_result = verify_caller_func(log, ad_caller)
        if not caller_state_result:
            ad_caller.log.error("Caller not in correct state after %s seconds",
                                wait_time_in_call)

        if not hangup_call(log, ad_caller):
            ad_caller.log.error("Error in Hanging-Up Call")
            return False

        ad_callee.log.info("Wait for voice mail indicator on callee.")
        try:
            event = ad_callee.ed.wait_for_event(
                EventMessageWaitingIndicatorChanged,
                _is_on_message_waiting_event_true)
            ad_callee.log.info("Got event %s", event)
        except Empty:
            ad_callee.log.warning("No expected event %s",
                                  EventMessageWaitingIndicatorChanged)
            return False
        voice_mail_count_after = ad_callee.droid.telephonyGetVoiceMailCountForSubscription(
            subid_callee)
        ad_callee.log.info(
            "telephonyGetVoiceMailCount output - before: %s, after: %s",
            voice_mail_count_before, voice_mail_count_after)

        # voice_mail_count_after should:
        # either equals to (voice_mail_count_before + 1) [For ATT and SPT]
        # or equals to -1 [For TMO]
        # -1 means there are unread voice mail, but the count is unknown
        if not check_voice_mail_count(log, ad_callee, voice_mail_count_before,
                                      voice_mail_count_after):
            log.error("before and after voice mail count is not incorrect.")
            return False
    finally:
        ad_callee.droid.telephonyStopTrackingVoiceMailStateChangeForSubscription(
            subid_callee)
    return True


def call_voicemail_erase_all_pending_voicemail(log, ad):
    """Script for phone to erase all pending voice mail.
    This script only works for TMO and ATT and SPT currently.
    This script only works if phone have already set up voice mail options,
    and phone should disable password protection for voice mail.

    1. If phone don't have pending voice message, return True.
    2. Dial voice mail number.
        For TMO, the number is '123'
        For ATT, the number is phone's number
        For SPT, the number is phone's number
    3. Wait for voice mail connection setup.
    4. Wait for voice mail play pending voice message.
    5. Send DTMF to delete one message.
        The digit is '7'.
    6. Repeat steps 4 and 5 until voice mail server drop this call.
        (No pending message)
    6. Check telephonyGetVoiceMailCount result. it should be 0.

    Args:
        log: log object
        ad: android device object
    Returns:
        False if error happens. True is succeed.
    """
    log.info("Erase all pending voice mail.")
    count = ad.droid.telephonyGetVoiceMailCount()
    if count == 0:
        ad.log.info("No Pending voice mail.")
        return True
    if count == -1:
        ad.log.info("There is pending voice mail, but the count is unknown")
        count = MAX_SAVED_VOICE_MAIL
    else:
        ad.log.info("There are %s voicemails", count)

    voice_mail_number = get_voice_mail_number(log, ad)
    delete_digit = get_voice_mail_delete_digit(get_operator_name(log, ad))
    if not initiate_call(log, ad, voice_mail_number):
        log.error("Initiate call to voice mail failed.")
        return False
    time.sleep(WAIT_TIME_VOICE_MAIL_SERVER_RESPONSE)
    callId = ad.droid.telecomCallGetCallIds()[0]
    time.sleep(WAIT_TIME_VOICE_MAIL_SERVER_RESPONSE)
    while (is_phone_in_call(log, ad) and (count > 0)):
        ad.log.info("Press %s to delete voice mail.", delete_digit)
        ad.droid.telecomCallPlayDtmfTone(callId, delete_digit)
        ad.droid.telecomCallStopDtmfTone(callId)
        time.sleep(WAIT_TIME_VOICE_MAIL_SERVER_RESPONSE)
        count -= 1
    if is_phone_in_call(log, ad):
        hangup_call(log, ad)

    # wait for telephonyGetVoiceMailCount to update correct result
    remaining_time = MAX_WAIT_TIME_VOICE_MAIL_COUNT
    while ((remaining_time > 0)
           and (ad.droid.telephonyGetVoiceMailCount() != 0)):
        time.sleep(1)
        remaining_time -= 1
    current_voice_mail_count = ad.droid.telephonyGetVoiceMailCount()
    ad.log.info("telephonyGetVoiceMailCount: %s", current_voice_mail_count)
    return (current_voice_mail_count == 0)


def _is_on_message_waiting_event_true(event):
    """Private function to return if the received EventMessageWaitingIndicatorChanged
    event MessageWaitingIndicatorContainer.IS_MESSAGE_WAITING field is True.
    """
    return event['data'][MessageWaitingIndicatorContainer.IS_MESSAGE_WAITING]


def call_setup_teardown(log,
                        ad_caller,
                        ad_callee,
                        ad_hangup=None,
                        verify_caller_func=None,
                        verify_callee_func=None,
                        wait_time_in_call=WAIT_TIME_IN_CALL,
                        incall_ui_display=INCALL_UI_DISPLAY_FOREGROUND,
                        dialing_number_length=None,
                        video_state=None,
                        slot_id_callee=None):
    """ Call process, including make a phone call from caller,
    accept from callee, and hang up. The call is on default voice subscription

    In call process, call from <droid_caller> to <droid_callee>,
    accept the call, (optional)then hang up from <droid_hangup>.

    Args:
        ad_caller: Caller Android Device Object.
        ad_callee: Callee Android Device Object.
        ad_hangup: Android Device Object end the phone call.
            Optional. Default value is None, and phone call will continue.
        verify_call_mode_caller: func_ptr to verify caller in correct mode
            Optional. Default is None
        verify_call_mode_caller: func_ptr to verify caller in correct mode
            Optional. Default is None
        incall_ui_display: after answer the call, bring in-call UI to foreground or
            background. Optional, default value is INCALL_UI_DISPLAY_FOREGROUND.
            if = INCALL_UI_DISPLAY_FOREGROUND, bring in-call UI to foreground.
            if = INCALL_UI_DISPLAY_BACKGROUND, bring in-call UI to background.
            else, do nothing.
        dialing_number_length: the number of digits used for dialing
        slot_id_callee : the slot if of the callee to call to

    Returns:
        True if call process without any error.
        False if error happened.

    """
    subid_caller = get_outgoing_voice_sub_id(ad_caller)
    if slot_id_callee is None:
        subid_callee = get_incoming_voice_sub_id(ad_callee)
    else:
        subid_callee = get_subid_from_slot_index(log, ad_callee, slot_id_callee)

    return call_setup_teardown_for_subscription(
        log, ad_caller, ad_callee, subid_caller, subid_callee, ad_hangup,
        verify_caller_func, verify_callee_func, wait_time_in_call,
        incall_ui_display, dialing_number_length, video_state)


def call_setup_teardown_for_subscription(
        log,
        ad_caller,
        ad_callee,
        subid_caller,
        subid_callee,
        ad_hangup=None,
        verify_caller_func=None,
        verify_callee_func=None,
        wait_time_in_call=WAIT_TIME_IN_CALL,
        incall_ui_display=INCALL_UI_DISPLAY_FOREGROUND,
        dialing_number_length=None,
        video_state=None):
    """ Call process, including make a phone call from caller,
    accept from callee, and hang up. The call is on specified subscription

    In call process, call from <droid_caller> to <droid_callee>,
    accept the call, (optional)then hang up from <droid_hangup>.

    Args:
        ad_caller: Caller Android Device Object.
        ad_callee: Callee Android Device Object.
        subid_caller: Caller subscription ID
        subid_callee: Callee subscription ID
        ad_hangup: Android Device Object end the phone call.
            Optional. Default value is None, and phone call will continue.
        verify_call_mode_caller: func_ptr to verify caller in correct mode
            Optional. Default is None
        verify_call_mode_caller: func_ptr to verify caller in correct mode
            Optional. Default is None
        incall_ui_display: after answer the call, bring in-call UI to foreground or
            background. Optional, default value is INCALL_UI_DISPLAY_FOREGROUND.
            if = INCALL_UI_DISPLAY_FOREGROUND, bring in-call UI to foreground.
            if = INCALL_UI_DISPLAY_BACKGROUND, bring in-call UI to background.
            else, do nothing.

    Returns:
        TelResultWrapper which will evaluate as False if error.

    """
    CHECK_INTERVAL = 5
    begin_time = get_current_epoch_time()
    if not verify_caller_func:
        verify_caller_func = is_phone_in_call
    if not verify_callee_func:
        verify_callee_func = is_phone_in_call

    caller_number = ad_caller.telephony['subscription'][subid_caller][
        'phone_num']
    callee_number = ad_callee.telephony['subscription'][subid_callee][
        'phone_num']
    if dialing_number_length:
        skip_test = False
        trunc_position = 0 - int(dialing_number_length)
        try:
            caller_area_code = caller_number[:trunc_position]
            callee_area_code = callee_number[:trunc_position]
            callee_dial_number = callee_number[trunc_position:]
        except:
            skip_test = True
        if caller_area_code != callee_area_code:
            skip_test = True
        if skip_test:
            msg = "Cannot make call from %s to %s by %s digits" % (
                caller_number, callee_number, dialing_number_length)
            ad_caller.log.info(msg)
            raise signals.TestSkip(msg)
        else:
            callee_number = callee_dial_number

    tel_result_wrapper = TelResultWrapper(CallResult('SUCCESS'))
    msg = "Call from %s to %s" % (caller_number, callee_number)
    if video_state:
        msg = "Video %s" % msg
        video = True
    else:
        video = False
    if ad_hangup:
        msg = "%s for duration of %s seconds" % (msg, wait_time_in_call)
    ad_caller.log.info(msg)

    for ad in (ad_caller, ad_callee):
        call_ids = ad.droid.telecomCallGetCallIds()
        setattr(ad, "call_ids", call_ids)
        if call_ids:
            ad.log.info("Pre-exist CallId %s before making call", call_ids)
    try:
        if not initiate_call(
                log,
                ad_caller,
                callee_number,
                incall_ui_display=incall_ui_display,
                video=video):
            ad_caller.log.error("Initiate call failed.")
            tel_result_wrapper.result_value = CallResult('INITIATE_FAILED')
            return tel_result_wrapper
        else:
            ad_caller.log.info("Caller initate call successfully")
        if not wait_and_answer_call_for_subscription(
                log,
                ad_callee,
                subid_callee,
                incoming_number=caller_number,
                caller=ad_caller,
                incall_ui_display=incall_ui_display,
                video_state=video_state):
            ad_callee.log.error("Answer call fail.")
            tel_result_wrapper.result_value = CallResult(
                'NO_RING_EVENT_OR_ANSWER_FAILED')
            return tel_result_wrapper
        else:
            ad_callee.log.info("Callee answered the call successfully")

        for ad, call_func in zip([ad_caller, ad_callee],
                                 [verify_caller_func, verify_callee_func]):
            call_ids = ad.droid.telecomCallGetCallIds()
            new_call_ids = set(call_ids) - set(ad.call_ids)
            if not new_call_ids:
                ad.log.error(
                    "No new call ids are found after call establishment")
                ad.log.error("telecomCallGetCallIds returns %s",
                             ad.droid.telecomCallGetCallIds())
                tel_result_wrapper.result_value = CallResult('NO_CALL_ID_FOUND')
            for new_call_id in new_call_ids:
                if not wait_for_in_call_active(ad, call_id=new_call_id):
                    tel_result_wrapper.result_value = CallResult(
                        'CALL_STATE_NOT_ACTIVE_DURING_ESTABLISHMENT')
                else:
                    ad.log.info("callProperties = %s",
                                ad.droid.telecomCallGetProperties(new_call_id))

            if not ad.droid.telecomCallGetAudioState():
                ad.log.error("Audio is not in call state")
                tel_result_wrapper.result_value = CallResult(
                    'AUDIO_STATE_NOT_INCALL_DURING_ESTABLISHMENT')

            if call_func(log, ad):
                ad.log.info("Call is in %s state", call_func.__name__)
            else:
                ad.log.error("Call is not in %s state, voice in RAT %s",
                             call_func.__name__,
                             ad.droid.telephonyGetCurrentVoiceNetworkType())
                tel_result_wrapper.result_value = CallResult(
                    'CALL_DROP_OR_WRONG_STATE_DURING_ESTABLISHMENT')
        if not tel_result_wrapper:
            return tel_result_wrapper
        elapsed_time = 0
        while (elapsed_time < wait_time_in_call):
            CHECK_INTERVAL = min(CHECK_INTERVAL,
                                 wait_time_in_call - elapsed_time)
            time.sleep(CHECK_INTERVAL)
            elapsed_time += CHECK_INTERVAL
            time_message = "at <%s>/<%s> second." % (elapsed_time,
                                                     wait_time_in_call)
            for ad, call_func in [(ad_caller, verify_caller_func),
                                  (ad_callee, verify_callee_func)]:
                if not call_func(log, ad):
                    ad.log.error(
                        "NOT in correct %s state at %s, voice in RAT %s",
                        call_func.__name__, time_message,
                        ad.droid.telephonyGetCurrentVoiceNetworkType())
                    tel_result_wrapper.result_value = CallResult(
                        'CALL_DROP_OR_WRONG_STATE_AFTER_CONNECTED')
                else:
                    ad.log.info("In correct %s state at %s",
                                call_func.__name__, time_message)
                if not ad.droid.telecomCallGetAudioState():
                    ad.log.error("Audio is not in call state at %s",
                                 time_message)
                    tel_result_wrapper.result_value = CallResult(
                        'AUDIO_STATE_NOT_INCALL_AFTER_CONNECTED')
            if not tel_result_wrapper:
                return tel_result_wrapper

        if ad_hangup:
            if not hangup_call(log, ad_hangup):
                ad_hangup.log.info("Failed to hang up the call")
                tel_result_wrapper.result_value = CallResult('CALL_HANGUP_FAIL')
                return tel_result_wrapper
    finally:
        if not tel_result_wrapper:
            for ad in (ad_caller, ad_callee):
                last_call_drop_reason(ad, begin_time)
                try:
                    if ad.droid.telecomIsInCall():
                        ad.log.info("In call. End now.")
                        ad.droid.telecomEndCall()
                except Exception as e:
                    log.error(str(e))
        if ad_hangup or not tel_result_wrapper:
            for ad in (ad_caller, ad_callee):
                if not wait_for_call_id_clearing(
                        ad, getattr(ad, "caller_ids", [])):
                    tel_result_wrapper.result_value = CallResult(
                        'CALL_ID_CLEANUP_FAIL')
    return tel_result_wrapper

def call_setup_teardown_for_call_forwarding(
    log,
    ad_caller,
    ad_callee,
    forwarded_callee,
    ad_hangup=None,
    verify_callee_func=None,
    verify_after_cf_disabled=None,
    wait_time_in_call=WAIT_TIME_IN_CALL,
    incall_ui_display=INCALL_UI_DISPLAY_FOREGROUND,
    dialing_number_length=None,
    video_state=None,
    call_forwarding_type="unconditional"):
    """ Call process for call forwarding, including make a phone call from
    caller, forward from callee, accept from the forwarded callee and hang up.
    The call is on default voice subscription

    In call process, call from <ad_caller> to <ad_callee>, forwarded to
    <forwarded_callee>, accept the call, (optional) and then hang up from
    <ad_hangup>.

    Args:
        ad_caller: Caller Android Device Object.
        ad_callee: Callee Android Device Object which forwards the call.
        forwarded_callee: Callee Android Device Object which answers the call.
        ad_hangup: Android Device Object end the phone call.
            Optional. Default value is None, and phone call will continue.
        verify_callee_func: func_ptr to verify callee in correct mode
            Optional. Default is None
        verify_after_cf_disabled: If True the test of disabling call forwarding
        will be appended.
        wait_time_in_call: the call duration of a connected call
        incall_ui_display: after answer the call, bring in-call UI to foreground
        or background.
            Optional, default value is INCALL_UI_DISPLAY_FOREGROUND.
            if = INCALL_UI_DISPLAY_FOREGROUND, bring in-call UI to foreground.
            if = INCALL_UI_DISPLAY_BACKGROUND, bring in-call UI to background.
            else, do nothing.
        dialing_number_length: the number of digits used for dialing
        video_state: video call or voice call. Default is voice call.
        call_forwarding_type: type of call forwarding listed below:
            - unconditional
            - busy
            - not_answered
            - not_reachable

    Returns:
        True if call process without any error.
        False if error happened.

    """
    subid_caller = get_outgoing_voice_sub_id(ad_caller)
    subid_callee = get_incoming_voice_sub_id(ad_callee)
    subid_forwarded_callee = get_incoming_voice_sub_id(forwarded_callee)
    return call_setup_teardown_for_call_forwarding_for_subscription(
        log,
        ad_caller,
        ad_callee,
        forwarded_callee,
        subid_caller,
        subid_callee,
        subid_forwarded_callee,
        ad_hangup,
        verify_callee_func,
        wait_time_in_call,
        incall_ui_display,
        dialing_number_length,
        video_state,
        call_forwarding_type,
        verify_after_cf_disabled)

def call_setup_teardown_for_call_forwarding_for_subscription(
        log,
        ad_caller,
        ad_callee,
        forwarded_callee,
        subid_caller,
        subid_callee,
        subid_forwarded_callee,
        ad_hangup=None,
        verify_callee_func=None,
        wait_time_in_call=WAIT_TIME_IN_CALL,
        incall_ui_display=INCALL_UI_DISPLAY_FOREGROUND,
        dialing_number_length=None,
        video_state=None,
        call_forwarding_type="unconditional",
        verify_after_cf_disabled=None):
    """ Call process for call forwarding, including make a phone call from caller,
    forward from callee, accept from the forwarded callee and hang up.
    The call is on specified subscription

    In call process, call from <ad_caller> to <ad_callee>, forwarded to
    <forwarded_callee>, accept the call, (optional) and then hang up from
    <ad_hangup>.

    Args:
        ad_caller: Caller Android Device Object.
        ad_callee: Callee Android Device Object which forwards the call.
        forwarded_callee: Callee Android Device Object which answers the call.
        subid_caller: Caller subscription ID
        subid_callee: Callee subscription ID
        subid_forwarded_callee: Forwarded callee subscription ID
        ad_hangup: Android Device Object end the phone call.
            Optional. Default value is None, and phone call will continue.
        verify_callee_func: func_ptr to verify callee in correct mode
            Optional. Default is None
        wait_time_in_call: the call duration of a connected call
        incall_ui_display: after answer the call, bring in-call UI to foreground
        or background. Optional, default value is INCALL_UI_DISPLAY_FOREGROUND.
            if = INCALL_UI_DISPLAY_FOREGROUND, bring in-call UI to foreground.
            if = INCALL_UI_DISPLAY_BACKGROUND, bring in-call UI to background.
            else, do nothing.
        dialing_number_length: the number of digits used for dialing
        video_state: video call or voice call. Default is voice call.
        call_forwarding_type: type of call forwarding listed below:
            - unconditional
            - busy
            - not_answered
            - not_reachable
        verify_after_cf_disabled: If True the call forwarding will not be
        enabled. This argument is used to verify if the call can be received
        successfully after call forwarding was disabled.

    Returns:
        True if call process without any error.
        False if error happened.

    """
    CHECK_INTERVAL = 5
    begin_time = get_current_epoch_time()
    verify_caller_func = is_phone_in_call
    if not verify_callee_func:
        verify_callee_func = is_phone_in_call
    verify_forwarded_callee_func = is_phone_in_call

    caller_number = ad_caller.telephony['subscription'][subid_caller][
        'phone_num']
    callee_number = ad_callee.telephony['subscription'][subid_callee][
        'phone_num']
    forwarded_callee_number = forwarded_callee.telephony['subscription'][
        subid_forwarded_callee]['phone_num']

    if dialing_number_length:
        skip_test = False
        trunc_position = 0 - int(dialing_number_length)
        try:
            caller_area_code = caller_number[:trunc_position]
            callee_area_code = callee_number[:trunc_position]
            callee_dial_number = callee_number[trunc_position:]
        except:
            skip_test = True
        if caller_area_code != callee_area_code:
            skip_test = True
        if skip_test:
            msg = "Cannot make call from %s to %s by %s digits" % (
                caller_number, callee_number, dialing_number_length)
            ad_caller.log.info(msg)
            raise signals.TestSkip(msg)
        else:
            callee_number = callee_dial_number

    result = True
    msg = "Call from %s to %s (forwarded to %s)" % (
        caller_number, callee_number, forwarded_callee_number)
    if video_state:
        msg = "Video %s" % msg
        video = True
    else:
        video = False
    if ad_hangup:
        msg = "%s for duration of %s seconds" % (msg, wait_time_in_call)
    ad_caller.log.info(msg)

    for ad in (ad_caller, forwarded_callee):
        call_ids = ad.droid.telecomCallGetCallIds()
        setattr(ad, "call_ids", call_ids)
        if call_ids:
            ad.log.info("Pre-exist CallId %s before making call", call_ids)

    if not verify_after_cf_disabled:
        if not set_call_forwarding_by_mmi(
            log,
            ad_callee,
            forwarded_callee,
            call_forwarding_type=call_forwarding_type):
            raise signals.TestFailure(
                    "Failed to register or activate call forwarding.",
                    extras={"fail_reason": "Failed to register or activate call"
                    " forwarding."})

    if call_forwarding_type == "not_reachable":
        if not toggle_airplane_mode_msim(
            log,
            ad_callee,
            new_state=True,
            strict_checking=True):
            return False

    if call_forwarding_type == "busy":
        ad_callee.log.info("Callee is making a phone call to 0000000000 to make"
            " itself busy.")
        ad_callee.droid.telecomCallNumber("0000000000", False)
        time.sleep(2)

        if check_call_state_idle_by_adb(ad_callee):
            ad_callee.log.error("Call state of the callee is idle.")
            if not verify_after_cf_disabled:
                erase_call_forwarding_by_mmi(
                    log,
                    ad_callee,
                    call_forwarding_type=call_forwarding_type)
            return False

    try:
        if not initiate_call(
                log,
                ad_caller,
                callee_number,
                incall_ui_display=incall_ui_display,
                video=video):

            ad_caller.log.error("Caller failed to initiate the call.")
            result = False

            if call_forwarding_type == "not_reachable":
                if toggle_airplane_mode_msim(
                    log,
                    ad_callee,
                    new_state=False,
                    strict_checking=True):
                    time.sleep(10)
            elif call_forwarding_type == "busy":
                hangup_call(log, ad_callee)

            if not verify_after_cf_disabled:
                erase_call_forwarding_by_mmi(
                    log,
                    ad_callee,
                    call_forwarding_type=call_forwarding_type)
            return False
        else:
            ad_caller.log.info("Caller initated the call successfully.")

        if call_forwarding_type == "not_answered":
            if not wait_for_ringing_call_for_subscription(
                    log,
                    ad_callee,
                    subid_callee,
                    incoming_number=caller_number,
                    caller=ad_caller,
                    event_tracking_started=True):
                ad.log.info("Incoming call ringing check failed.")
                return False

            _timeout = 30
            while check_call_state_ring_by_adb(ad_callee) == 1 and _timeout >= 0:
                time.sleep(1)
                _timeout = _timeout - 1

        if not wait_and_answer_call_for_subscription(
                log,
                forwarded_callee,
                subid_forwarded_callee,
                incoming_number=caller_number,
                caller=ad_caller,
                incall_ui_display=incall_ui_display,
                video_state=video_state):

            if not verify_after_cf_disabled:
                forwarded_callee.log.error("Forwarded callee failed to receive"
                    "or answer the call.")
                result = False
            else:
                forwarded_callee.log.info("Forwarded callee did not receive or"
                    " answer the call.")

            if call_forwarding_type == "not_reachable":
                if toggle_airplane_mode_msim(
                    log,
                    ad_callee,
                    new_state=False,
                    strict_checking=True):
                    time.sleep(10)
            elif call_forwarding_type == "busy":
                hangup_call(log, ad_callee)

            if not verify_after_cf_disabled:
                erase_call_forwarding_by_mmi(
                    log,
                    ad_callee,
                    call_forwarding_type=call_forwarding_type)
                return False

        else:
            if not verify_after_cf_disabled:
                forwarded_callee.log.info("Forwarded callee answered the call"
                    " successfully.")
            else:
                forwarded_callee.log.error("Forwarded callee should not be able"
                    " to answer the call.")
                hangup_call(log, ad_caller)
                result = False

        for ad, subid, call_func in zip(
                [ad_caller, forwarded_callee],
                [subid_caller, subid_forwarded_callee],
                [verify_caller_func, verify_forwarded_callee_func]):
            call_ids = ad.droid.telecomCallGetCallIds()
            new_call_ids = set(call_ids) - set(ad.call_ids)
            if not new_call_ids:
                if not verify_after_cf_disabled:
                    ad.log.error(
                        "No new call ids are found after call establishment")
                    ad.log.error("telecomCallGetCallIds returns %s",
                                 ad.droid.telecomCallGetCallIds())
                result = False
            for new_call_id in new_call_ids:
                if not verify_after_cf_disabled:
                    if not wait_for_in_call_active(ad, call_id=new_call_id):
                        result = False
                    else:
                        ad.log.info("callProperties = %s",
                            ad.droid.telecomCallGetProperties(new_call_id))
                else:
                    ad.log.error("No new call id should be found.")

            if not ad.droid.telecomCallGetAudioState():
                if not verify_after_cf_disabled:
                    ad.log.error("Audio is not in call state")
                    result = False

            if call_func(log, ad):
                if not verify_after_cf_disabled:
                    ad.log.info("Call is in %s state", call_func.__name__)
                else:
                    ad.log.error("Call is in %s state", call_func.__name__)
            else:
                if not verify_after_cf_disabled:
                    ad.log.error(
                        "Call is not in %s state, voice in RAT %s",
                        call_func.__name__,
                        ad.droid.telephonyGetCurrentVoiceNetworkTypeForSubscription(subid))
                    result = False

        if not result:
            if call_forwarding_type == "not_reachable":
                if toggle_airplane_mode_msim(
                    log,
                    ad_callee,
                    new_state=False,
                    strict_checking=True):
                    time.sleep(10)
            elif call_forwarding_type == "busy":
                hangup_call(log, ad_callee)

            if not verify_after_cf_disabled:
                erase_call_forwarding_by_mmi(
                    log,
                    ad_callee,
                    call_forwarding_type=call_forwarding_type)
                return False

        elapsed_time = 0
        while (elapsed_time < wait_time_in_call):
            CHECK_INTERVAL = min(CHECK_INTERVAL,
                                 wait_time_in_call - elapsed_time)
            time.sleep(CHECK_INTERVAL)
            elapsed_time += CHECK_INTERVAL
            time_message = "at <%s>/<%s> second." % (elapsed_time,
                                                     wait_time_in_call)
            for ad, subid, call_func in [
                (ad_caller, subid_caller, verify_caller_func),
                (forwarded_callee, subid_forwarded_callee,
                    verify_forwarded_callee_func)]:
                if not call_func(log, ad):
                    if not verify_after_cf_disabled:
                        ad.log.error(
                            "NOT in correct %s state at %s, voice in RAT %s",
                            call_func.__name__, time_message,
                            ad.droid.telephonyGetCurrentVoiceNetworkTypeForSubscription(subid))
                    result = False
                else:
                    if not verify_after_cf_disabled:
                        ad.log.info("In correct %s state at %s",
                                    call_func.__name__, time_message)
                    else:
                        ad.log.error("In correct %s state at %s",
                                    call_func.__name__, time_message)

                if not ad.droid.telecomCallGetAudioState():
                    if not verify_after_cf_disabled:
                        ad.log.error("Audio is not in call state at %s",
                                     time_message)
                    result = False

            if not result:
                if call_forwarding_type == "not_reachable":
                    if toggle_airplane_mode_msim(
                        log,
                        ad_callee,
                        new_state=False,
                        strict_checking=True):
                        time.sleep(10)
                elif call_forwarding_type == "busy":
                    hangup_call(log, ad_callee)

                if not verify_after_cf_disabled:
                    erase_call_forwarding_by_mmi(
                        log,
                        ad_callee,
                        call_forwarding_type=call_forwarding_type)
                    return False

        if ad_hangup:
            if not hangup_call(log, ad_hangup):
                ad_hangup.log.info("Failed to hang up the call")
                result = False
                if call_forwarding_type == "not_reachable":
                    if toggle_airplane_mode_msim(
                        log,
                        ad_callee,
                        new_state=False,
                        strict_checking=True):
                        time.sleep(10)
                elif call_forwarding_type == "busy":
                    hangup_call(log, ad_callee)

                if not verify_after_cf_disabled:
                    erase_call_forwarding_by_mmi(
                        log,
                        ad_callee,
                        call_forwarding_type=call_forwarding_type)
                return False
    finally:
        if not result:
            if verify_after_cf_disabled:
                result = True
            else:
                for ad in (ad_caller, forwarded_callee):
                    last_call_drop_reason(ad, begin_time)
                    try:
                        if ad.droid.telecomIsInCall():
                            ad.log.info("In call. End now.")
                            ad.droid.telecomEndCall()
                    except Exception as e:
                        log.error(str(e))

        if ad_hangup or not result:
            for ad in (ad_caller, forwarded_callee):
                if not wait_for_call_id_clearing(
                        ad, getattr(ad, "caller_ids", [])):
                    result = False

    if call_forwarding_type == "not_reachable":
        if toggle_airplane_mode_msim(
            log,
            ad_callee,
            new_state=False,
            strict_checking=True):
            time.sleep(10)
    elif call_forwarding_type == "busy":
        hangup_call(log, ad_callee)

    if not verify_after_cf_disabled:
        erase_call_forwarding_by_mmi(
            log,
            ad_callee,
            call_forwarding_type=call_forwarding_type)

    if not result:
        return result

    ad_caller.log.info(
        "Make a normal call to callee to ensure the call can be connected after"
        " call forwarding was disabled")
    return call_setup_teardown_for_subscription(
        log, ad_caller, ad_callee, subid_caller, subid_callee, ad_caller,
        verify_caller_func, verify_callee_func, wait_time_in_call,
        incall_ui_display, dialing_number_length, video_state)

def wait_for_call_id_clearing(ad,
                              previous_ids,
                              timeout=MAX_WAIT_TIME_CALL_DROP):
    while timeout > 0:
        new_call_ids = ad.droid.telecomCallGetCallIds()
        if len(new_call_ids) <= len(previous_ids):
            return True
        time.sleep(5)
        timeout = timeout - 5
    ad.log.error("Call id clearing failed. Before: %s; After: %s",
                 previous_ids, new_call_ids)
    return False


def last_call_drop_reason(ad, begin_time=None):
    reasons = ad.search_logcat(
        "qcril_qmi_voice_map_qmi_to_ril_last_call_failure_cause", begin_time)
    reason_string = ""
    if reasons:
        log_msg = "Logcat call drop reasons:"
        for reason in reasons:
            log_msg = "%s\n\t%s" % (log_msg, reason["log_message"])
            if "ril reason str" in reason["log_message"]:
                reason_string = reason["log_message"].split(":")[-1].strip()
        ad.log.info(log_msg)
    reasons = ad.search_logcat("ACTION_FORBIDDEN_NO_SERVICE_AUTHORIZATION",
                               begin_time)
    if reasons:
        ad.log.warning("ACTION_FORBIDDEN_NO_SERVICE_AUTHORIZATION is seen")
    ad.log.info("last call dumpsys: %s",
                sorted(dumpsys_last_call_info(ad).items()))
    return reason_string


def phone_number_formatter(input_string, formatter=None):
    """Get expected format of input phone number string.

    Args:
        input_string: (string) input phone number.
            The input could be 10/11/12 digital, with or without " "/"-"/"."
        formatter: (int) expected format, this could be 7/10/11/12
            if formatter is 7: output string would be 7 digital number.
            if formatter is 10: output string would be 10 digital (standard) number.
            if formatter is 11: output string would be "1" + 10 digital number.
            if formatter is 12: output string would be "+1" + 10 digital number.

    Returns:
        If no error happen, return phone number in expected format.
        Else, return None.
    """
    if not input_string:
        return ""
    # make sure input_string is 10 digital
    # Remove white spaces, dashes, dots
    input_string = input_string.replace(" ", "").replace("-", "").replace(
        ".", "").lstrip("0")
    if not formatter:
        return input_string
    # Remove +81 and add 0 for Japan Carriers only.
    if (len(input_string) == 13 and input_string[0:3] == "+81"):
        input_string = "0" + input_string[3:]
        return input_string
    # Remove "1"  or "+1"from front
    if (len(input_string) == PHONE_NUMBER_STRING_FORMAT_11_DIGIT
            and input_string[0] == "1"):
        input_string = input_string[1:]
    elif (len(input_string) == PHONE_NUMBER_STRING_FORMAT_12_DIGIT
          and input_string[0:2] == "+1"):
        input_string = input_string[2:]
    elif (len(input_string) == PHONE_NUMBER_STRING_FORMAT_7_DIGIT
          and formatter == PHONE_NUMBER_STRING_FORMAT_7_DIGIT):
        return input_string
    elif len(input_string) != PHONE_NUMBER_STRING_FORMAT_10_DIGIT:
        return None
    # change input_string according to format
    if formatter == PHONE_NUMBER_STRING_FORMAT_12_DIGIT:
        input_string = "+1" + input_string
    elif formatter == PHONE_NUMBER_STRING_FORMAT_11_DIGIT:
        input_string = "1" + input_string
    elif formatter == PHONE_NUMBER_STRING_FORMAT_10_DIGIT:
        input_string = input_string
    elif formatter == PHONE_NUMBER_STRING_FORMAT_7_DIGIT:
        input_string = input_string[3:]
    else:
        return None
    return input_string


def get_internet_connection_type(log, ad):
    """Get current active connection type name.

    Args:
        log: Log object.
        ad: Android Device Object.
    Returns:
        current active connection type name.
    """
    if not ad.droid.connectivityNetworkIsConnected():
        return 'none'
    return connection_type_from_type_string(
        ad.droid.connectivityNetworkGetActiveConnectionTypeName())


def verify_http_connection(log,
                           ad,
                           url="https://www.google.com",
                           retry=5,
                           retry_interval=15,
                           expected_state=True):
    """Make ping request and return status.

    Args:
        log: log object
        ad: Android Device Object.
        url: Optional. The ping request will be made to this URL.
            Default Value is "http://www.google.com/".

    """
    if not getattr(ad, "data_droid", None):
        ad.data_droid, ad.data_ed = ad.get_droid()
        ad.data_ed.start()
    else:
        try:
            if not ad.data_droid.is_live:
                ad.data_droid, ad.data_ed = ad.get_droid()
                ad.data_ed.start()
        except Exception:
            ad.log.info("Start new sl4a session for file download")
            ad.data_droid, ad.data_ed = ad.get_droid()
            ad.data_ed.start()
    for i in range(0, retry + 1):
        try:
            http_response = ad.data_droid.httpPing(url)
        except Exception as e:
            ad.log.info("httpPing with %s", e)
            http_response = None
        if (expected_state and http_response) or (not expected_state
                                                  and not http_response):
            ad.log.info("Http ping response for %s meet expected %s", url,
                        expected_state)
            return True
        if i < retry:
            time.sleep(retry_interval)
    ad.log.error("Http ping to %s is %s after %s second, expecting %s", url,
                 http_response, i * retry_interval, expected_state)
    return False


def _generate_file_directory_and_file_name(url, out_path):
    file_name = url.split("/")[-1]
    if not out_path:
        file_directory = "/sdcard/Download/"
    elif not out_path.endswith("/"):
        file_directory, file_name = os.path.split(out_path)
    else:
        file_directory = out_path
    return file_directory, file_name


def _check_file_existance(ad, file_path, expected_file_size=None):
    """Check file existance by file_path. If expected_file_size
       is provided, then also check if the file meet the file size requirement.
    """
    out = None
    try:
        out = ad.adb.shell('stat -c "%%s" %s' % file_path)
    except AdbError:
        pass
    # Handle some old version adb returns error message "No such" into std_out
    if out and "No such" not in out:
        if expected_file_size:
            file_size = int(out)
            if file_size >= expected_file_size:
                ad.log.info("File %s of size %s exists", file_path, file_size)
                return True
            else:
                ad.log.info("File %s is of size %s, does not meet expected %s",
                            file_path, file_size, expected_file_size)
                return False
        else:
            ad.log.info("File %s exists", file_path)
            return True
    else:
        ad.log.info("File %s does not exist.", file_path)
        return False


def check_curl_availability(ad):
    if not hasattr(ad, "curl_capable"):
        try:
            out = ad.adb.shell("/data/curl --version")
            if not out or "not found" in out:
                setattr(ad, "curl_capable", False)
                ad.log.info("curl is unavailable, use chrome to download file")
            else:
                setattr(ad, "curl_capable", True)
        except Exception:
            setattr(ad, "curl_capable", False)
            ad.log.info("curl is unavailable, use chrome to download file")
    return ad.curl_capable


def start_youtube_video(ad, url="https://www.youtube.com/watch?v=pSJoP0LR8CQ"):
    ad.log.info("Open an youtube video")
    ad.ensure_screen_on()
    ad.adb.shell('am start -a android.intent.action.VIEW -d "%s"' % url)
    if wait_for_state(ad.droid.audioIsMusicActive, True, 15, 1):
        ad.log.info("Started a video in youtube, audio is in MUSIC state")
        return True
    else:
        ad.unlock_screen()
        ad.adb.shell('am start -a android.intent.action.VIEW -d "%s"' % url)
        if wait_for_state(ad.droid.audioIsMusicActive, True, 15, 1):
            ad.log.info("Started a video in youtube, audio is in MUSIC state")
            return True
        else:
            ad.log.warning(
                "Started a video in youtube, but audio is not in MUSIC state")
            return False


def active_file_download_task(log, ad, file_name="5MB", method="curl"):
    # files available for download on the same website:
    # 1GB.zip, 512MB.zip, 200MB.zip, 50MB.zip, 20MB.zip, 10MB.zip, 5MB.zip
    # download file by adb command, as phone call will use sl4a
    file_size_map = {
        '5MB': 5000000,
        '10MB': 10000000,
        '20MB': 20000000,
        '50MB': 50000000,
        '100MB': 100000000,
        '200MB': 200000000,
        '512MB': 512000000
    }
    url_map = {
        "5MB": [
            "http://146.148.91.8/download/5MB.zip",
            "http://212.183.159.230/5MB.zip",
            "http://ipv4.download.thinkbroadband.com/5MB.zip"
        ],
        "10MB": [
            "http://146.148.91.8/download/10MB.zip",
            "http://212.183.159.230/10MB.zip",
            "http://ipv4.download.thinkbroadband.com/10MB.zip",
            "http://lax.futurehosting.com/test.zip",
            "http://ovh.net/files/10Mio.dat"
        ],
        "20MB": [
            "http://146.148.91.8/download/20MB.zip",
            "http://212.183.159.230/20MB.zip",
            "http://ipv4.download.thinkbroadband.com/20MB.zip"
        ],
        "50MB": [
            "http://146.148.91.8/download/50MB.zip",
            "http://212.183.159.230/50MB.zip",
            "http://ipv4.download.thinkbroadband.com/50MB.zip"
        ],
        "100MB": [
            "http://146.148.91.8/download/100MB.zip",
            "http://212.183.159.230/100MB.zip",
            "http://ipv4.download.thinkbroadband.com/100MB.zip",
            "http://speedtest-ca.turnkeyinternet.net/100mb.bin",
            "http://ovh.net/files/100Mio.dat",
            "http://lax.futurehosting.com/test100.zip"
        ],
        "200MB": [
            "http://146.148.91.8/download/200MB.zip",
            "http://212.183.159.230/200MB.zip",
            "http://ipv4.download.thinkbroadband.com/200MB.zip"
        ],
        "512MB": [
            "http://146.148.91.8/download/512MB.zip",
            "http://212.183.159.230/512MB.zip",
            "http://ipv4.download.thinkbroadband.com/512MB.zip"
        ]
    }

    file_size = file_size_map.get(file_name)
    file_urls = url_map.get(file_name)
    file_url = None
    for url in file_urls:
        url_splits = url.split("/")
        if verify_http_connection(log, ad, url=url, retry=1):
            output_path = "/sdcard/Download/%s" % url_splits[-1]
            file_url = url
            break
    if not file_url:
        ad.log.error("No url is available to download %s", file_name)
        return False
    timeout = min(max(file_size / 100000, 600), 3600)
    if method == "sl4a":
        return (http_file_download_by_sl4a, (ad, file_url, output_path,
                                             file_size, True, timeout))
    if method == "curl" and check_curl_availability(ad):
        return (http_file_download_by_curl, (ad, file_url, output_path,
                                             file_size, True, timeout))
    elif method == "sl4a" or method == "curl":
        return (http_file_download_by_sl4a, (ad, file_url, output_path,
                                             file_size, True, timeout))
    else:
        return (http_file_download_by_chrome, (ad, file_url, file_size, True,
                                               timeout))


def active_file_download_test(log, ad, file_name="5MB", method="sl4a"):
    task = active_file_download_task(log, ad, file_name, method=method)
    if not task:
        return False
    return task[0](*task[1])


def verify_internet_connection_by_ping(log,
                                       ad,
                                       retries=1,
                                       expected_state=True,
                                       timeout=60):
    """Verify internet connection by ping test.

    Args:
        log: log object
        ad: Android Device Object.

    """
    begin_time = get_current_epoch_time()
    ip_addr = "54.230.144.105"
    for dest in ("www.google.com", "www.amazon.com", ip_addr):
        for i in range(retries):
            ad.log.info("Ping %s - attempt %d", dest, i + 1)
            result = adb_shell_ping(
                ad, count=5, timeout=timeout, loss_tolerance=40, dest_ip=dest)
            if result == expected_state:
                ad.log.info(
                    "Internet connection by pinging to %s is %s as expected",
                    dest, expected_state)
                if dest == ip_addr:
                    ad.log.warning("Suspect dns failure")
                    ad.log.info("DNS config: %s",
                                ad.adb.shell("getprop | grep dns").replace(
                                    "\n", " "))
                    return False
                return True
            else:
                ad.log.warning(
                    "Internet connection test by pinging %s is %s, expecting %s",
                    dest, result, expected_state)
                if get_current_epoch_time() - begin_time < timeout * 1000:
                    time.sleep(5)
    ad.log.error("Ping test doesn't meet expected %s", expected_state)
    return False


def verify_internet_connection(log, ad, retries=3, expected_state=True):
    """Verify internet connection by ping test and http connection.

    Args:
        log: log object
        ad: Android Device Object.

    """
    if ad.droid.connectivityNetworkIsConnected() != expected_state:
        ad.log.info("NetworkIsConnected = %s, expecting %s",
                    not expected_state, expected_state)
    if verify_internet_connection_by_ping(
            log, ad, retries=retries, expected_state=expected_state):
        return True
    for url in ("https://www.google.com", "https://www.amazon.com"):
        if verify_http_connection(
                log, ad, url=url, retry=retries,
                expected_state=expected_state):
            return True
    ad.log.info("DNS config: %s", " ".join(
        ad.adb.shell("getprop | grep dns").split()))
    ad.log.info("Interface info:\n%s", ad.adb.shell("ifconfig"))
    ad.log.info("NetworkAgentInfo: %s",
                ad.adb.shell("dumpsys connectivity | grep NetworkAgentInfo"))
    return False


def iperf_test_with_options(log,
                            ad,
                            iperf_server,
                            iperf_option,
                            timeout=180,
                            rate_dict=None,
                            blocking=True,
                            log_file_path=None):
    """Iperf adb run helper.

    Args:
        log: log object
        ad: Android Device Object.
        iperf_server: The iperf host url".
        iperf_option: The options to pass to iperf client
        timeout: timeout for file download to complete.
        rate_dict: dictionary that can be passed in to save data
        blocking: run iperf in blocking mode if True
        log_file_path: location to save logs
    Returns:
        True if IPerf runs without throwing an exception
    """
    try:
        if log_file_path:
            ad.adb.shell("rm %s" % log_file_path, ignore_status=True)
        ad.log.info("Running adb iperf test with server %s", iperf_server)
        ad.log.info("IPerf options are %s", iperf_option)
        if not blocking:
            ad.run_iperf_client_nb(
                iperf_server,
                iperf_option,
                timeout=timeout + 60,
                log_file_path=log_file_path)
            return True
        result, data = ad.run_iperf_client(
            iperf_server, iperf_option, timeout=timeout + 60)
        ad.log.info("IPerf test result with server %s is %s", iperf_server,
                    result)
        if result:
            iperf_str = ''.join(data)
            iperf_result = ipf.IPerfResult(iperf_str)
            if "-u" in iperf_option:
                udp_rate = iperf_result.avg_rate
                if udp_rate is None:
                    ad.log.warning(
                        "UDP rate is none, IPerf server returned error: %s",
                        iperf_result.error)
                ad.log.info("IPerf3 udp speed is %sbps", udp_rate)
            else:
                tx_rate = iperf_result.avg_send_rate
                rx_rate = iperf_result.avg_receive_rate
                if (tx_rate or rx_rate) is None:
                    ad.log.warning(
                        "A TCP rate is none, IPerf server returned error: %s",
                        iperf_result.error)
                ad.log.info(
                    "IPerf3 upload speed is %sbps, download speed is %sbps",
                    tx_rate, rx_rate)
            if rate_dict is not None:
                rate_dict["Uplink"] = tx_rate
                rate_dict["Downlink"] = rx_rate
        return result
    except AdbError as e:
        ad.log.warning("Fail to run iperf test with exception %s", e)
        raise


def iperf_udp_test_by_adb(log,
                          ad,
                          iperf_server,
                          port_num=None,
                          reverse=False,
                          timeout=180,
                          limit_rate=None,
                          omit=10,
                          ipv6=False,
                          rate_dict=None,
                          blocking=True,
                          log_file_path=None):
    """Iperf test by adb using UDP.

    Args:
        log: log object
        ad: Android Device Object.
        iperf_Server: The iperf host url".
        port_num: TCP/UDP server port
        reverse: whether to test download instead of upload
        timeout: timeout for file download to complete.
        limit_rate: iperf bandwidth option. None by default
        omit: the omit option provided in iperf command.
        ipv6: whether to run the test as ipv6
        rate_dict: dictionary that can be passed in to save data
        blocking: run iperf in blocking mode if True
        log_file_path: location to save logs
    """
    iperf_option = "-u -i 1 -t %s -O %s -J" % (timeout, omit)
    if limit_rate:
        iperf_option += " -b %s" % limit_rate
    if port_num:
        iperf_option += " -p %s" % port_num
    if ipv6:
        iperf_option += " -6"
    if reverse:
        iperf_option += " -R"
    try:
        return iperf_test_with_options(log,
                                        ad,
                                        iperf_server,
                                        iperf_option,
                                        timeout,
                                        rate_dict,
                                        blocking,
                                        log_file_path)
    except AdbError:
        return False

def iperf_test_by_adb(log,
                      ad,
                      iperf_server,
                      port_num=None,
                      reverse=False,
                      timeout=180,
                      limit_rate=None,
                      omit=10,
                      ipv6=False,
                      rate_dict=None,
                      blocking=True,
                      log_file_path=None):
    """Iperf test by adb using TCP.

    Args:
        log: log object
        ad: Android Device Object.
        iperf_server: The iperf host url".
        port_num: TCP/UDP server port
        reverse: whether to test download instead of upload
        timeout: timeout for file download to complete.
        limit_rate: iperf bandwidth option. None by default
        omit: the omit option provided in iperf command.
        ipv6: whether to run the test as ipv6
        rate_dict: dictionary that can be passed in to save data
        blocking: run iperf in blocking mode if True
        log_file_path: location to save logs
    """
    iperf_option = "-t %s -O %s -J" % (timeout, omit)
    if limit_rate:
        iperf_option += " -b %s" % limit_rate
    if port_num:
        iperf_option += " -p %s" % port_num
    if ipv6:
        iperf_option += " -6"
    if reverse:
        iperf_option += " -R"
    try:
        return iperf_test_with_options(log,
                                        ad,
                                        iperf_server,
                                        iperf_option,
                                        timeout,
                                        rate_dict,
                                        blocking,
                                        log_file_path)
    except AdbError:
        return False


def http_file_download_by_curl(ad,
                               url,
                               out_path=None,
                               expected_file_size=None,
                               remove_file_after_check=True,
                               timeout=3600,
                               limit_rate=None,
                               retry=3):
    """Download http file by adb curl.

    Args:
        ad: Android Device Object.
        url: The url that file to be downloaded from".
        out_path: Optional. Where to download file to.
                  out_path is /sdcard/Download/ by default.
        expected_file_size: Optional. Provided if checking the download file meet
                            expected file size in unit of byte.
        remove_file_after_check: Whether to remove the downloaded file after
                                 check.
        timeout: timeout for file download to complete.
        limit_rate: download rate in bps. None, if do not apply rate limit.
        retry: the retry request times provided in curl command.
    """
    file_directory, file_name = _generate_file_directory_and_file_name(
        url, out_path)
    file_path = os.path.join(file_directory, file_name)
    curl_cmd = "/data/curl"
    if limit_rate:
        curl_cmd += " --limit-rate %s" % limit_rate
    if retry:
        curl_cmd += " --retry %s" % retry
    curl_cmd += " --url %s > %s" % (url, file_path)
    try:
        ad.log.info("Download %s to %s by adb shell command %s", url,
                    file_path, curl_cmd)

        ad.adb.shell(curl_cmd, timeout=timeout)
        if _check_file_existance(ad, file_path, expected_file_size):
            ad.log.info("%s is downloaded to %s successfully", url, file_path)
            return True
        else:
            ad.log.warning("Fail to download %s", url)
            return False
    except Exception as e:
        ad.log.warning("Download %s failed with exception %s", url, e)
        for cmd in ("ls -lh /data/local/tmp/tcpdump/",
                    "ls -lh /sdcard/Download/",
                    "ls -lh /data/vendor/radio/diag_logs/logs/",
                    "df -h",
                    "du -d 4 -h /data"):
            out = ad.adb.shell(cmd)
            ad.log.debug("%s", out)
        return False
    finally:
        if remove_file_after_check:
            ad.log.info("Remove the downloaded file %s", file_path)
            ad.adb.shell("rm %s" % file_path, ignore_status=True)


def open_url_by_adb(ad, url):
    ad.adb.shell('am start -a android.intent.action.VIEW -d "%s"' % url)


def http_file_download_by_chrome(ad,
                                 url,
                                 expected_file_size=None,
                                 remove_file_after_check=True,
                                 timeout=3600):
    """Download http file by chrome.

    Args:
        ad: Android Device Object.
        url: The url that file to be downloaded from".
        expected_file_size: Optional. Provided if checking the download file meet
                            expected file size in unit of byte.
        remove_file_after_check: Whether to remove the downloaded file after
                                 check.
        timeout: timeout for file download to complete.
    """
    chrome_apk = "com.android.chrome"
    file_directory, file_name = _generate_file_directory_and_file_name(
        url, "/sdcard/Download/")
    file_path = os.path.join(file_directory, file_name)
    # Remove pre-existing file
    ad.force_stop_apk(chrome_apk)
    file_to_be_delete = os.path.join(file_directory, "*%s*" % file_name)
    ad.adb.shell("rm -f %s" % file_to_be_delete)
    ad.adb.shell("rm -rf /sdcard/Download/.*")
    ad.adb.shell("rm -f /sdcard/Download/.*")
    data_accounting = {
        "total_rx_bytes": ad.droid.getTotalRxBytes(),
        "mobile_rx_bytes": ad.droid.getMobileRxBytes(),
        "subscriber_mobile_data_usage": get_mobile_data_usage(ad, None, None),
        "chrome_mobile_data_usage": get_mobile_data_usage(
            ad, None, chrome_apk)
    }
    ad.log.debug("Before downloading: %s", data_accounting)
    ad.log.info("Download %s with timeout %s", url, timeout)
    ad.ensure_screen_on()
    open_url_by_adb(ad, url)
    elapse_time = 0
    result = True
    while elapse_time < timeout:
        time.sleep(30)
        if _check_file_existance(ad, file_path, expected_file_size):
            ad.log.info("%s is downloaded successfully", url)
            if remove_file_after_check:
                ad.log.info("Remove the downloaded file %s", file_path)
                ad.adb.shell("rm -f %s" % file_to_be_delete)
                ad.adb.shell("rm -rf /sdcard/Download/.*")
                ad.adb.shell("rm -f /sdcard/Download/.*")
            #time.sleep(30)
            new_data_accounting = {
                "mobile_rx_bytes":
                ad.droid.getMobileRxBytes(),
                "subscriber_mobile_data_usage":
                get_mobile_data_usage(ad, None, None),
                "chrome_mobile_data_usage":
                get_mobile_data_usage(ad, None, chrome_apk)
            }
            ad.log.info("After downloading: %s", new_data_accounting)
            accounting_diff = {
                key: value - data_accounting[key]
                for key, value in new_data_accounting.items()
            }
            ad.log.debug("Data accounting difference: %s", accounting_diff)
            if getattr(ad, "on_mobile_data", False):
                for key, value in accounting_diff.items():
                    if value < expected_file_size:
                        ad.log.warning("%s diff is %s less than %s", key,
                                       value, expected_file_size)
                        ad.data_accounting["%s_failure" % key] += 1
            else:
                for key, value in accounting_diff.items():
                    if value >= expected_file_size:
                        ad.log.error("%s diff is %s. File download is "
                                     "consuming mobile data", key, value)
                        result = False
            return result
        elif _check_file_existance(ad, "%s.crdownload" % file_path):
            ad.log.info("Chrome is downloading %s", url)
        elif elapse_time < 60:
            # download not started, retry download wit chrome again
            open_url_by_adb(ad, url)
        else:
            ad.log.error("Unable to download file from %s", url)
            break
        elapse_time += 30
    ad.log.warning("Fail to download file from %s", url)
    ad.force_stop_apk("com.android.chrome")
    ad.adb.shell("rm -f %s" % file_to_be_delete)
    ad.adb.shell("rm -rf /sdcard/Download/.*")
    ad.adb.shell("rm -f /sdcard/Download/.*")
    return False


def http_file_download_by_sl4a(ad,
                               url,
                               out_path=None,
                               expected_file_size=None,
                               remove_file_after_check=True,
                               timeout=300):
    """Download http file by sl4a RPC call.

    Args:
        ad: Android Device Object.
        url: The url that file to be downloaded from".
        out_path: Optional. Where to download file to.
                  out_path is /sdcard/Download/ by default.
        expected_file_size: Optional. Provided if checking the download file meet
                            expected file size in unit of byte.
        remove_file_after_check: Whether to remove the downloaded file after
                                 check.
        timeout: timeout for file download to complete.
    """
    file_folder, file_name = _generate_file_directory_and_file_name(
        url, out_path)
    file_path = os.path.join(file_folder, file_name)
    ad.adb.shell("rm -f %s" % file_path)
    accounting_apk = SL4A_APK_NAME
    result = True
    try:
        if not getattr(ad, "data_droid", None):
            ad.data_droid, ad.data_ed = ad.get_droid()
            ad.data_ed.start()
        else:
            try:
                if not ad.data_droid.is_live:
                    ad.data_droid, ad.data_ed = ad.get_droid()
                    ad.data_ed.start()
            except Exception:
                ad.log.info("Start new sl4a session for file download")
                ad.data_droid, ad.data_ed = ad.get_droid()
                ad.data_ed.start()
        data_accounting = {
            "mobile_rx_bytes":
            ad.droid.getMobileRxBytes(),
            "subscriber_mobile_data_usage":
            get_mobile_data_usage(ad, None, None),
            "sl4a_mobile_data_usage":
            get_mobile_data_usage(ad, None, accounting_apk)
        }
        ad.log.debug("Before downloading: %s", data_accounting)
        ad.log.info("Download file from %s to %s by sl4a RPC call", url,
                    file_path)
        try:
            ad.data_droid.httpDownloadFile(url, file_path, timeout=timeout)
        except Exception as e:
            ad.log.warning("SL4A file download error: %s", e)
            for cmd in ("ls -lh /data/local/tmp/tcpdump/",
                        "ls -lh /sdcard/Download/",
                        "ls -lh /data/vendor/radio/diag_logs/logs/",
                        "df -h",
                        "du -d 4 -h /data"):
                out = ad.adb.shell(cmd)
                ad.log.debug("%s", out)
            ad.data_droid.terminate()
            return False
        if _check_file_existance(ad, file_path, expected_file_size):
            ad.log.info("%s is downloaded successfully", url)
            new_data_accounting = {
                "mobile_rx_bytes":
                ad.droid.getMobileRxBytes(),
                "subscriber_mobile_data_usage":
                get_mobile_data_usage(ad, None, None),
                "sl4a_mobile_data_usage":
                get_mobile_data_usage(ad, None, accounting_apk)
            }
            ad.log.debug("After downloading: %s", new_data_accounting)
            accounting_diff = {
                key: value - data_accounting[key]
                for key, value in new_data_accounting.items()
            }
            ad.log.debug("Data accounting difference: %s", accounting_diff)
            if getattr(ad, "on_mobile_data", False):
                for key, value in accounting_diff.items():
                    if value < expected_file_size:
                        ad.log.debug("%s diff is %s less than %s", key,
                                       value, expected_file_size)
                        ad.data_accounting["%s_failure"] += 1
            else:
                for key, value in accounting_diff.items():
                    if value >= expected_file_size:
                        ad.log.error("%s diff is %s. File download is "
                                     "consuming mobile data", key, value)
                        result = False
            return result
        else:
            ad.log.warning("Fail to download %s", url)
            return False
    except Exception as e:
        ad.log.error("Download %s failed with exception %s", url, e)
        raise
    finally:
        if remove_file_after_check:
            ad.log.info("Remove the downloaded file %s", file_path)
            ad.adb.shell("rm %s" % file_path, ignore_status=True)


def get_wifi_usage(ad, sid=None, apk=None):
    if not sid:
        sid = ad.droid.subscriptionGetDefaultDataSubId()
    current_time = int(time.time() * 1000)
    begin_time = current_time - 10 * 24 * 60 * 60 * 1000
    end_time = current_time + 10 * 24 * 60 * 60 * 1000

    if apk:
        uid = ad.get_apk_uid(apk)
        ad.log.debug("apk %s uid = %s", apk, uid)
        try:
            return ad.droid.connectivityQueryDetailsForUid(
                TYPE_WIFI,
                ad.droid.telephonyGetSubscriberIdForSubscription(sid),
                begin_time, end_time, uid)
        except:
            return ad.droid.connectivityQueryDetailsForUid(
                ad.droid.telephonyGetSubscriberIdForSubscription(sid),
                begin_time, end_time, uid)
    else:
        try:
            return ad.droid.connectivityQuerySummaryForDevice(
                TYPE_WIFI,
                ad.droid.telephonyGetSubscriberIdForSubscription(sid),
                begin_time, end_time)
        except:
            return ad.droid.connectivityQuerySummaryForDevice(
                ad.droid.telephonyGetSubscriberIdForSubscription(sid),
                begin_time, end_time)


def get_mobile_data_usage(ad, sid=None, apk=None):
    if not sid:
        sid = ad.droid.subscriptionGetDefaultDataSubId()
    current_time = int(time.time() * 1000)
    begin_time = current_time - 10 * 24 * 60 * 60 * 1000
    end_time = current_time + 10 * 24 * 60 * 60 * 1000

    if apk:
        uid = ad.get_apk_uid(apk)
        ad.log.debug("apk %s uid = %s", apk, uid)
        try:
            usage_info = ad.droid.getMobileDataUsageInfoForUid(uid, sid)
            ad.log.debug("Mobile data usage info for uid %s = %s", uid,
                        usage_info)
            return usage_info["UsageLevel"]
        except:
            try:
                return ad.droid.connectivityQueryDetailsForUid(
                    TYPE_MOBILE,
                    ad.droid.telephonyGetSubscriberIdForSubscription(sid),
                    begin_time, end_time, uid)
            except:
                return ad.droid.connectivityQueryDetailsForUid(
                    ad.droid.telephonyGetSubscriberIdForSubscription(sid),
                    begin_time, end_time, uid)
    else:
        try:
            usage_info = ad.droid.getMobileDataUsageInfo(sid)
            ad.log.debug("Mobile data usage info = %s", usage_info)
            return usage_info["UsageLevel"]
        except:
            try:
                return ad.droid.connectivityQuerySummaryForDevice(
                    TYPE_MOBILE,
                    ad.droid.telephonyGetSubscriberIdForSubscription(sid),
                    begin_time, end_time)
            except:
                return ad.droid.connectivityQuerySummaryForDevice(
                    ad.droid.telephonyGetSubscriberIdForSubscription(sid),
                    begin_time, end_time)


def set_mobile_data_usage_limit(ad, limit, subscriber_id=None):
    if not subscriber_id:
        subscriber_id = ad.droid.telephonyGetSubscriberId()
    ad.log.debug("Set subscriber mobile data usage limit to %s", limit)
    ad.droid.logV("Setting subscriber mobile data usage limit to %s" % limit)
    try:
        ad.droid.connectivitySetDataUsageLimit(subscriber_id, str(limit))
    except:
        ad.droid.connectivitySetDataUsageLimit(subscriber_id, limit)


def remove_mobile_data_usage_limit(ad, subscriber_id=None):
    if not subscriber_id:
        subscriber_id = ad.droid.telephonyGetSubscriberId()
    ad.log.debug("Remove subscriber mobile data usage limit")
    ad.droid.logV(
        "Setting subscriber mobile data usage limit to -1, unlimited")
    try:
        ad.droid.connectivitySetDataUsageLimit(subscriber_id, "-1")
    except:
        ad.droid.connectivitySetDataUsageLimit(subscriber_id, -1)


def trigger_modem_crash(ad, timeout=120):
    cmd = "echo restart > /sys/kernel/debug/msm_subsys/modem"
    ad.log.info("Triggering Modem Crash from kernel using adb command %s", cmd)
    ad.adb.shell(cmd)
    time.sleep(timeout)
    return True


def trigger_modem_crash_by_modem(ad, timeout=120):
    begin_time = get_device_epoch_time(ad)
    ad.adb.shell(
        "setprop persist.vendor.sys.modem.diag.mdlog false",
        ignore_status=True)
    # Legacy pixels use persist.sys.modem.diag.mdlog.
    ad.adb.shell(
        "setprop persist.sys.modem.diag.mdlog false", ignore_status=True)
    disable_qxdm_logger(ad)
    cmd = ('am instrument -w -e request "4b 25 03 00" '
           '"com.google.mdstest/com.google.mdstest.instrument.'
           'ModemCommandInstrumentation"')
    ad.log.info("Crash modem by %s", cmd)
    ad.adb.shell(cmd, ignore_status=True)
    time.sleep(timeout)  # sleep time for sl4a stability
    reasons = ad.search_logcat("modem subsystem failure reason", begin_time)
    if reasons:
        ad.log.info("Modem crash is triggered successfully")
        ad.log.info(reasons[-1]["log_message"])
        return True
    else:
        ad.log.warning("There is no modem subsystem failure reason logcat")
        return False


def phone_switch_to_msim_mode(ad, retries=3, timeout=60):
    result = False
    if not ad.is_apk_installed("com.google.mdstest"):
        raise signals.TestAbortClass("mdstest is not installed")
    mode = ad.droid.telephonyGetPhoneCount()
    if mode == 2:
        ad.log.info("Device already in MSIM mode")
        return True
    for i in range(retries):
        ad.adb.shell(
        "setprop persist.vendor.sys.modem.diag.mdlog false", ignore_status=True)
        ad.adb.shell(
        "setprop persist.sys.modem.diag.mdlog false", ignore_status=True)
        disable_qxdm_logger(ad)
        cmd = ('am instrument -w -e request "WriteEFS" -e item '
               '"/google/pixel_multisim_config" -e data  "02 00 00 00" '
               '"com.google.mdstest/com.google.mdstest.instrument.'
               'ModemConfigInstrumentation"')
        ad.log.info("Switch to MSIM mode by using %s", cmd)
        ad.adb.shell(cmd, ignore_status=True)
        time.sleep(timeout)
        ad.adb.shell("setprop persist.radio.multisim.config dsds")
        reboot_device(ad)
        # Verify if device is really in msim mode
        mode = ad.droid.telephonyGetPhoneCount()
        if mode == 2:
            ad.log.info("Device correctly switched to MSIM mode")
            result = True
            if "Sprint" in ad.adb.getprop("gsm.sim.operator.alpha"):
                cmd = ('am instrument -w -e request "WriteEFS" -e item '
                       '"/google/pixel_dsds_imei_mapping_slot_record" -e data "03"'
                       ' "com.google.mdstest/com.google.mdstest.instrument.'
                       'ModemConfigInstrumentation"')
                ad.log.info("Switch Sprint to IMEI1 slot using %s", cmd)
                ad.adb.shell(cmd, ignore_status=True)
                time.sleep(timeout)
                reboot_device(ad)
            break
        else:
            ad.log.warning("Attempt %d - failed to switch to MSIM", (i + 1))
    return result


def phone_switch_to_ssim_mode(ad, retries=3, timeout=30):
    result = False
    if not ad.is_apk_installed("com.google.mdstest"):
        raise signals.TestAbortClass("mdstest is not installed")
    mode = ad.droid.telephonyGetPhoneCount()
    if mode == 1:
        ad.log.info("Device already in SSIM mode")
        return True
    for i in range(retries):
        ad.adb.shell(
        "setprop persist.vendor.sys.modem.diag.mdlog false", ignore_status=True)
        ad.adb.shell(
        "setprop persist.sys.modem.diag.mdlog false", ignore_status=True)
        disable_qxdm_logger(ad)
        cmds = ('am instrument -w -e request "WriteEFS" -e item '
                '"/google/pixel_multisim_config" -e data  "01 00 00 00" '
                '"com.google.mdstest/com.google.mdstest.instrument.'
                'ModemConfigInstrumentation"',
                'am instrument -w -e request "WriteEFS" -e item "/nv/item_files'
                '/modem/uim/uimdrv/uim_extended_slot_mapping_config" -e data '
                '"00 01 02 01" "com.google.mdstest/com.google.mdstest.'
                'instrument.ModemConfigInstrumentation"')
        for cmd in cmds:
            ad.log.info("Switch to SSIM mode by using %s", cmd)
            ad.adb.shell(cmd, ignore_status=True)
            time.sleep(timeout)
        ad.adb.shell("setprop persist.radio.multisim.config ssss")
        reboot_device(ad)
        # Verify if device is really in ssim mode
        mode = ad.droid.telephonyGetPhoneCount()
        if mode == 1:
            ad.log.info("Device correctly switched to SSIM mode")
            result = True
            break
        else:
            ad.log.warning("Attempt %d - failed to switch to SSIM", (i + 1))
    return result


def lock_lte_band_by_mds(ad, band):
    disable_qxdm_logger(ad)
    ad.log.info("Write band %s locking to efs file", band)
    if band == "4":
        item_string = (
            "4B 13 26 00 08 00 00 00 40 00 08 00 0B 00 08 00 00 00 00 00 00 00 "
            "2F 6E 76 2F 69 74 65 6D 5F 66 69 6C 65 73 2F 6D 6F 64 65 6D 2F 6D "
            "6D 6F 64 65 2F 6C 74 65 5F 62 61 6E 64 70 72 65 66 00")
    elif band == "13":
        item_string = (
            "4B 13 26 00 08 00 00 00 40 00 08 00 0A 00 00 10 00 00 00 00 00 00 "
            "2F 6E 76 2F 69 74 65 6D 5F 66 69 6C 65 73 2F 6D 6F 64 65 6D 2F 6D "
            "6D 6F 64 65 2F 6C 74 65 5F 62 61 6E 64 70 72 65 66 00")
    else:
        ad.log.error("Band %s is not supported", band)
        return False
    cmd = ('am instrument -w -e request "%s" com.google.mdstest/com.google.'
           'mdstest.instrument.ModemCommandInstrumentation')
    for _ in range(3):
        if "SUCCESS" in ad.adb.shell(cmd % item_string, ignore_status=True):
            break
    else:
        ad.log.error("Fail to write band by %s" % (cmd % item_string))
        return False

    # EFS Sync
    item_string = "4B 13 30 00 2A 00 2F 00"

    for _ in range(3):
        if "SUCCESS" in ad.adb.shell(cmd % item_string, ignore_status=True):
            break
    else:
        ad.log.error("Fail to sync efs by %s" % (cmd % item_string))
        return False
    time.sleep(5)
    reboot_device(ad)


def _connection_state_change(_event, target_state, connection_type):
    if connection_type:
        if 'TypeName' not in _event['data']:
            return False
        connection_type_string_in_event = _event['data']['TypeName']
        cur_type = connection_type_from_type_string(
            connection_type_string_in_event)
        if cur_type != connection_type:
            log.info(
                "_connection_state_change expect: %s, received: %s <type %s>",
                connection_type, connection_type_string_in_event, cur_type)
            return False

    if 'isConnected' in _event['data'] and _event['data']['isConnected'] == target_state:
        return True
    return False


def wait_for_cell_data_connection(
        log, ad, state, timeout_value=MAX_WAIT_TIME_CONNECTION_STATE_UPDATE):
    """Wait for data connection status to be expected value for default
       data subscription.

    Wait for the data connection status to be DATA_STATE_CONNECTED
        or DATA_STATE_DISCONNECTED.

    Args:
        log: Log object.
        ad: Android Device Object.
        state: Expected status: True or False.
            If True, it will wait for status to be DATA_STATE_CONNECTED.
            If False, it will wait for status ti be DATA_STATE_DISCONNECTED.
        timeout_value: wait for cell data timeout value.
            This is optional, default value is MAX_WAIT_TIME_CONNECTION_STATE_UPDATE

    Returns:
        True if success.
        False if failed.
    """
    sub_id = get_default_data_sub_id(ad)
    return wait_for_cell_data_connection_for_subscription(
        log, ad, sub_id, state, timeout_value)


def _is_data_connection_state_match(log, ad, expected_data_connection_state):
    return (expected_data_connection_state ==
            ad.droid.telephonyGetDataConnectionState())


def _is_network_connected_state_match(log, ad,
                                      expected_network_connected_state):
    return (expected_network_connected_state ==
            ad.droid.connectivityNetworkIsConnected())


def wait_for_cell_data_connection_for_subscription(
        log,
        ad,
        sub_id,
        state,
        timeout_value=MAX_WAIT_TIME_CONNECTION_STATE_UPDATE):
    """Wait for data connection status to be expected value for specified
       subscrption id.

    Wait for the data connection status to be DATA_STATE_CONNECTED
        or DATA_STATE_DISCONNECTED.

    Args:
        log: Log object.
        ad: Android Device Object.
        sub_id: subscription Id
        state: Expected status: True or False.
            If True, it will wait for status to be DATA_STATE_CONNECTED.
            If False, it will wait for status ti be DATA_STATE_DISCONNECTED.
        timeout_value: wait for cell data timeout value.
            This is optional, default value is MAX_WAIT_TIME_CONNECTION_STATE_UPDATE

    Returns:
        True if success.
        False if failed.
    """
    state_str = {
        True: DATA_STATE_CONNECTED,
        False: DATA_STATE_DISCONNECTED
    }[state]

    data_state = ad.droid.telephonyGetDataConnectionState()
    if not state and ad.droid.telephonyGetDataConnectionState() == state_str:
        return True

    ad.ed.clear_events(EventDataConnectionStateChanged)
    ad.droid.telephonyStartTrackingDataConnectionStateChangeForSubscription(
        sub_id)
    ad.droid.connectivityStartTrackingConnectivityStateChange()
    try:
        ad.log.info("User data enabled for sub_id %s: %s", sub_id,
                    ad.droid.telephonyIsDataEnabledForSubscription(sub_id))
        data_state = ad.droid.telephonyGetDataConnectionState()
        ad.log.info("Data connection state is %s", data_state)
        ad.log.info("Network is connected: %s",
                    ad.droid.connectivityNetworkIsConnected())
        if data_state == state_str:
            return _wait_for_nw_data_connection(
                log, ad, state, NETWORK_CONNECTION_TYPE_CELL, timeout_value)

        try:
            ad.ed.wait_for_event(
                EventDataConnectionStateChanged,
                is_event_match,
                timeout=timeout_value,
                field=DataConnectionStateContainer.DATA_CONNECTION_STATE,
                value=state_str)
        except Empty:
            ad.log.info("No expected event EventDataConnectionStateChanged %s",
                        state_str)

        # TODO: Wait for <MAX_WAIT_TIME_CONNECTION_STATE_UPDATE> seconds for
        # data connection state.
        # Otherwise, the network state will not be correct.
        # The bug is tracked here: b/20921915

        # Previously we use _is_data_connection_state_match,
        # but telephonyGetDataConnectionState sometimes return wrong value.
        # The bug is tracked here: b/22612607
        # So we use _is_network_connected_state_match.

        if _wait_for_droid_in_state(log, ad, timeout_value,
                                    _is_network_connected_state_match, state):
            return _wait_for_nw_data_connection(
                log, ad, state, NETWORK_CONNECTION_TYPE_CELL, timeout_value)
        else:
            return False

    finally:
        ad.droid.telephonyStopTrackingDataConnectionStateChangeForSubscription(
            sub_id)


def wait_for_wifi_data_connection(
        log, ad, state, timeout_value=MAX_WAIT_TIME_CONNECTION_STATE_UPDATE):
    """Wait for data connection status to be expected value and connection is by WiFi.

    Args:
        log: Log object.
        ad: Android Device Object.
        state: Expected status: True or False.
            If True, it will wait for status to be DATA_STATE_CONNECTED.
            If False, it will wait for status ti be DATA_STATE_DISCONNECTED.
        timeout_value: wait for network data timeout value.
            This is optional, default value is MAX_WAIT_TIME_NW_SELECTION

    Returns:
        True if success.
        False if failed.
    """
    ad.log.info("wait_for_wifi_data_connection")
    return _wait_for_nw_data_connection(
        log, ad, state, NETWORK_CONNECTION_TYPE_WIFI, timeout_value)


def wait_for_data_connection(
        log, ad, state, timeout_value=MAX_WAIT_TIME_CONNECTION_STATE_UPDATE):
    """Wait for data connection status to be expected value.

    Wait for the data connection status to be DATA_STATE_CONNECTED
        or DATA_STATE_DISCONNECTED.

    Args:
        log: Log object.
        ad: Android Device Object.
        state: Expected status: True or False.
            If True, it will wait for status to be DATA_STATE_CONNECTED.
            If False, it will wait for status ti be DATA_STATE_DISCONNECTED.
        timeout_value: wait for network data timeout value.
            This is optional, default value is MAX_WAIT_TIME_CONNECTION_STATE_UPDATE

    Returns:
        True if success.
        False if failed.
    """
    return _wait_for_nw_data_connection(log, ad, state, None, timeout_value)


def _wait_for_nw_data_connection(
        log,
        ad,
        is_connected,
        connection_type=None,
        timeout_value=MAX_WAIT_TIME_CONNECTION_STATE_UPDATE):
    """Wait for data connection status to be expected value.

    Wait for the data connection status to be DATA_STATE_CONNECTED
        or DATA_STATE_DISCONNECTED.

    Args:
        log: Log object.
        ad: Android Device Object.
        is_connected: Expected connection status: True or False.
            If True, it will wait for status to be DATA_STATE_CONNECTED.
            If False, it will wait for status ti be DATA_STATE_DISCONNECTED.
        connection_type: expected connection type.
            This is optional, if it is None, then any connection type will return True.
        timeout_value: wait for network data timeout value.
            This is optional, default value is MAX_WAIT_TIME_CONNECTION_STATE_UPDATE

    Returns:
        True if success.
        False if failed.
    """
    ad.ed.clear_events(EventConnectivityChanged)
    ad.droid.connectivityStartTrackingConnectivityStateChange()
    try:
        cur_data_connection_state = ad.droid.connectivityNetworkIsConnected()
        if is_connected == cur_data_connection_state:
            current_type = get_internet_connection_type(log, ad)
            ad.log.info("current data connection type: %s", current_type)
            if not connection_type:
                return True
            else:
                if not is_connected and current_type != connection_type:
                    ad.log.info("data connection not on %s!", connection_type)
                    return True
                elif is_connected and current_type == connection_type:
                    ad.log.info("data connection on %s as expected",
                                connection_type)
                    return True
        else:
            ad.log.info("current data connection state: %s target: %s",
                        cur_data_connection_state, is_connected)

        try:
            event = ad.ed.wait_for_event(
                EventConnectivityChanged, _connection_state_change,
                timeout_value, is_connected, connection_type)
            ad.log.info("Got event: %s", event)
        except Empty:
            pass

        log.info(
            "_wait_for_nw_data_connection: check connection after wait event.")
        # TODO: Wait for <MAX_WAIT_TIME_CONNECTION_STATE_UPDATE> seconds for
        # data connection state.
        # Otherwise, the network state will not be correct.
        # The bug is tracked here: b/20921915
        if _wait_for_droid_in_state(log, ad, timeout_value,
                                    _is_network_connected_state_match,
                                    is_connected):
            current_type = get_internet_connection_type(log, ad)
            ad.log.info("current data connection type: %s", current_type)
            if not connection_type:
                return True
            else:
                if not is_connected and current_type != connection_type:
                    ad.log.info("data connection not on %s", connection_type)
                    return True
                elif is_connected and current_type == connection_type:
                    ad.log.info("after event wait, data connection on %s",
                                connection_type)
                    return True
                else:
                    return False
        else:
            return False
    except Exception as e:
        ad.log.error("Exception error %s", str(e))
        return False
    finally:
        ad.droid.connectivityStopTrackingConnectivityStateChange()


def get_cell_data_roaming_state_by_adb(ad):
    """Get Cell Data Roaming state. True for enabled, False for disabled"""
    adb_str = {"1": True, "0": False}
    out = ad.adb.shell("settings get global data_roaming")
    return adb_str[out]


def get_cell_data_roaming_state_by_adb(ad):
    """Get Cell Data Roaming state. True for enabled, False for disabled"""
    state_mapping = {"1": True, "0": False}
    return state_mapping[ad.adb.shell("settings get global data_roaming")]


def set_cell_data_roaming_state_by_adb(ad, state):
    """Set Cell Data Roaming state."""
    state_mapping = {True: "1", False: "0"}
    ad.log.info("Set data roaming to %s", state)
    ad.adb.shell("settings put global data_roaming %s" % state_mapping[state])


def toggle_cell_data_roaming(ad, state):
    """Enable cell data roaming for default data subscription.

    Wait for the data roaming status to be DATA_STATE_CONNECTED
        or DATA_STATE_DISCONNECTED.

    Args:
        log: Log object.
        ad: Android Device Object.
        state: True or False for enable or disable cell data roaming.

    Returns:
        True if success.
        False if failed.
    """
    state_int = {True: DATA_ROAMING_ENABLE, False: DATA_ROAMING_DISABLE}[state]
    action_str = {True: "Enable", False: "Disable"}[state]
    if ad.droid.connectivityCheckDataRoamingMode() == state:
        ad.log.info("Data roaming is already in state %s", state)
        return True
    if not ad.droid.connectivitySetDataRoaming(state_int):
        ad.error.info("Fail to config data roaming into state %s", state)
        return False
    if ad.droid.connectivityCheckDataRoamingMode() == state:
        ad.log.info("Data roaming is configured into state %s", state)
        return True
    else:
        ad.log.error("Data roaming is not configured into state %s", state)
        return False


def verify_incall_state(log, ads, expected_status):
    """Verify phones in incall state or not.

    Verify if all phones in the array <ads> are in <expected_status>.

    Args:
        log: Log object.
        ads: Array of Android Device Object. All droid in this array will be tested.
        expected_status: If True, verify all Phones in incall state.
            If False, verify all Phones not in incall state.

    """
    result = True
    for ad in ads:
        if ad.droid.telecomIsInCall() is not expected_status:
            ad.log.error("InCall status:%s, expected:%s",
                         ad.droid.telecomIsInCall(), expected_status)
            result = False
    return result


def verify_active_call_number(log, ad, expected_number):
    """Verify the number of current active call.

    Verify if the number of current active call in <ad> is
        equal to <expected_number>.

    Args:
        ad: Android Device Object.
        expected_number: Expected active call number.
    """
    calls = ad.droid.telecomCallGetCallIds()
    if calls is None:
        actual_number = 0
    else:
        actual_number = len(calls)
    if actual_number != expected_number:
        ad.log.error("Active Call number is %s, expecting", actual_number,
                     expected_number)
        return False
    return True


def num_active_calls(log, ad):
    """Get the count of current active calls.

    Args:
        log: Log object.
        ad: Android Device Object.

    Returns:
        Count of current active calls.
    """
    calls = ad.droid.telecomCallGetCallIds()
    return len(calls) if calls else 0


def toggle_volte(log, ad, new_state=None):
    """Toggle enable/disable VoLTE for default voice subscription.

    Args:
        ad: Android device object.
        new_state: VoLTE mode state to set to.
            True for enable, False for disable.
            If None, opposite of the current state.

    Raises:
        TelTestUtilsError if platform does not support VoLTE.
    """
    return toggle_volte_for_subscription(
        log, ad, get_outgoing_voice_sub_id(ad), new_state)


def toggle_volte_for_subscription(log, ad, sub_id, new_state=None):
    """Toggle enable/disable VoLTE for specified voice subscription.

    Args:
        ad: Android device object.
        sub_id: subscription ID
        new_state: VoLTE mode state to set to.
            True for enable, False for disable.
            If None, opposite of the current state.

    """
    current_state = ad.droid.imsMmTelIsAdvancedCallingEnabled(sub_id)
    if new_state is None:
        new_state = not current_state
    if new_state != current_state:
        ad.log.info("Toggle Enhanced 4G LTE Mode from %s to %s on sub_id %s", current_state,
                    new_state, sub_id)
        ad.droid.imsMmTelSetAdvancedCallingEnabled(sub_id, new_state)
    check_state = ad.droid.imsMmTelIsAdvancedCallingEnabled(sub_id)
    if check_state != new_state:
        ad.log.error("Failed to toggle Enhanced 4G LTE Mode to %s, still set to %s on sub_id %s",
                     new_state, check_state, sub_id)
        return False
    return True


def toggle_wfc(log, ad, new_state=None):
    """ Toggle WFC enable/disable

    Args:
        log: Log object
        ad: Android device object.
        new_state: True or False
    """
    if not ad.droid.imsIsWfcEnabledByPlatform():
        ad.log.info("WFC is not enabled by platform")
        return False
    current_state = ad.droid.imsIsWfcEnabledByUser()
    if current_state is None:
        new_state = not current_state
    if new_state != current_state:
        ad.log.info("Toggle WFC user enabled from %s to %s", current_state,
                    new_state)
        ad.droid.imsSetWfcSetting(new_state)
    return True


def toggle_wfc_for_subscription(ad, new_state=None, sub_id=None):
    """ Toggle WFC enable/disable

    Args:
        ad: Android device object.
        sub_id: subscription Id
        new_state: True or False
    """
    if sub_id is None:
        sub_id = ad.droid.subscriptionGetDefaultVoiceSubId()
    current_state = ad.droid.imsMmTelIsVoWiFiSettingEnabled(sub_id)
    if current_state is None:
        new_state = not current_state
    if new_state != current_state:
        ad.log.info("SubId %s - Toggle WFC from %s to %s", sub_id,
                    current_state, new_state)
        ad.droid.imsMmTelSetVoWiFiSettingEnabled(sub_id, new_state)
    return True


def wait_for_enhanced_4g_lte_setting(log,
                                     ad,
                                     max_time=MAX_WAIT_TIME_FOR_STATE_CHANGE):
    """Wait for android device to enable enhance 4G LTE setting.

    Args:
        log: log object.
        ad:  android device.
        max_time: maximal wait time.

    Returns:
        Return True if device report VoLTE enabled bit true within max_time.
        Return False if timeout.
    """
    return wait_for_state(
        ad.droid.imsIsEnhanced4gLteModeSettingEnabledByPlatform,
        True,
        max_wait_time=max_time)


def set_wfc_mode(log, ad, wfc_mode):
    """Set WFC enable/disable and mode.

    Args:
        log: Log object
        ad: Android device object.
        wfc_mode: WFC mode to set to.
            Valid mode includes: WFC_MODE_WIFI_ONLY, WFC_MODE_CELLULAR_PREFERRED,
            WFC_MODE_WIFI_PREFERRED, WFC_MODE_DISABLED.

    Returns:
        True if success. False if ad does not support WFC or error happened.
    """
    if wfc_mode != WFC_MODE_DISABLED and wfc_mode not in ad.telephony[
        "subscription"][get_outgoing_voice_sub_id(ad)].get("wfc_modes", []):
        ad.log.error("WFC mode %s is not supported", wfc_mode)
        raise signals.TestSkip("WFC mode %s is not supported" % wfc_mode)
    try:
        ad.log.info("Set wfc mode to %s", wfc_mode)
        if wfc_mode != WFC_MODE_DISABLED:
            start_adb_tcpdump(ad, interface="wlan0", mask="all")
        if not ad.droid.imsIsWfcEnabledByPlatform():
            if wfc_mode == WFC_MODE_DISABLED:
                return True
            else:
                ad.log.error("WFC not supported by platform.")
                return False
        ad.droid.imsSetWfcMode(wfc_mode)
        mode = ad.droid.imsGetWfcMode()
        if mode != wfc_mode:
            ad.log.error("WFC mode is %s, not in %s", mode, wfc_mode)
            return False
    except Exception as e:
        log.error(e)
        return False
    return True


def set_wfc_mode_for_subscription(ad, wfc_mode, sub_id=None):
    """Set WFC enable/disable and mode subscription based

    Args:
        ad: Android device object.
        wfc_mode: WFC mode to set to.
            Valid mode includes: WFC_MODE_WIFI_ONLY, WFC_MODE_CELLULAR_PREFERRED,
            WFC_MODE_WIFI_PREFERRED.
        sub_id: subscription Id

    Returns:
        True if success. False if ad does not support WFC or error happened.
    """
    try:
        if sub_id is None:
            sub_id = ad.droid.subscriptionGetDefaultVoiceSubId()
        if not ad.droid.imsMmTelIsVoWiFiSettingEnabled(sub_id):
            ad.log.info("SubId %s - Enabling WiFi Calling", sub_id)
            ad.droid.imsMmTelSetVoWiFiSettingEnabled(sub_id, True)
        ad.log.info("SubId %s - setwfcmode to %s", sub_id, wfc_mode)
        ad.droid.imsMmTelSetVoWiFiModeSetting(sub_id, wfc_mode)
        mode = ad.droid.imsMmTelGetVoWiFiModeSetting(sub_id)
        if mode != wfc_mode:
            ad.log.error("SubId %s - getwfcmode shows %s", sub_id, mode)
            return False
    except Exception as e:
        ad.log.error(e)
        return False
    return True


def set_ims_provisioning_for_subscription(ad, feature_flag, value, sub_id=None):
    """ Sets Provisioning Values for Subscription Id

    Args:
        ad: Android device object.
        sub_id: Subscription Id
        feature_flag: voice or video
        value: enable or disable

    """
    try:
        if sub_id is None:
            sub_id = ad.droid.subscriptionGetDefaultVoiceSubId()
        ad.log.info("SubId %s - setprovisioning for %s to %s",
                    sub_id, feature_flag, value)
        result = ad.droid.provisioningSetProvisioningIntValue(sub_id,
                    feature_flag, value)
        if result == 0:
            return True
        return False
    except Exception as e:
        ad.log.error(e)
        return False


def get_ims_provisioning_for_subscription(ad, feature_flag, tech, sub_id=None):
    """ Gets Provisioning Values for Subscription Id

    Args:
        ad: Android device object.
        sub_id: Subscription Id
        feature_flag: voice, video, ut, sms
        tech: lte, iwlan

    """
    try:
        if sub_id is None:
            sub_id = ad.droid.subscriptionGetDefaultVoiceSubId()
        result = ad.droid.provisioningGetProvisioningStatusForCapability(
                    sub_id, feature_flag, tech)
        ad.log.info("SubId %s - getprovisioning for %s on %s - %s",
                    sub_id, feature_flag, tech, result)
        return result
    except Exception as e:
        ad.log.error(e)
        return False


def get_carrier_provisioning_for_subscription(ad, feature_flag,
                                              tech, sub_id=None):
    """ Gets Provisioning Values for Subscription Id

    Args:
        ad: Android device object.
        sub_id: Subscription Id
        feature_flag: voice, video, ut, sms
        tech: wlan, wwan

    """
    try:
        if sub_id is None:
            sub_id = ad.droid.subscriptionGetDefaultVoiceSubId()
        result = ad.droid.imsMmTelIsSupported(sub_id, feature_flag, tech)
        ad.log.info("SubId %s - imsMmTelIsSupported for %s on %s - %s",
                    sub_id, feature_flag, tech, result)
        return result
    except Exception as e:
        ad.log.error(e)
        return False

def activate_wfc_on_device(log, ad):
    """ Activates WiFi calling on device.

        Required for certain network operators.

    Args:
        log: Log object
        ad: Android device object

    """
    activate_wfc_on_device_for_subscription(log, ad,
                                            ad.droid.subscriptionGetDefaultSubId())


def activate_wfc_on_device_for_subscription(log, ad, sub_id):
    """ Activates WiFi calling on device for a subscription.

    Args:
        log: Log object
        ad: Android device object
        sub_id: Subscription id (integer)

    """
    if not sub_id or INVALID_SUB_ID == sub_id:
        ad.log.error("Subscription id invalid")
        return
    operator_name = get_operator_name(log, ad, sub_id)
    if operator_name in (CARRIER_VZW, CARRIER_ATT, CARRIER_BELL, CARRIER_ROGERS,
                         CARRIER_TELUS, CARRIER_KOODO, CARRIER_VIDEOTRON, CARRIER_FRE):
        ad.log.info("Activating WFC on operator : %s", operator_name)
        if not ad.is_apk_installed("com.google.android.wfcactivation"):
            ad.log.error("WFC Activation Failed, wfc activation apk not installed")
            return
        wfc_activate_cmd ="am start --ei EXTRA_LAUNCH_CARRIER_APP 0 --ei " \
                    "android.telephony.extra.SUBSCRIPTION_INDEX {} -n ".format(sub_id)
        if CARRIER_ATT == operator_name:
            ad.adb.shell("setprop dbg.att.force_wfc_nv_enabled true")
            wfc_activate_cmd = wfc_activate_cmd+\
                               "\"com.google.android.wfcactivation/" \
                               ".WfcActivationActivity\""
        elif CARRIER_VZW == operator_name:
            ad.adb.shell("setprop dbg.vzw.force_wfc_nv_enabled true")
            wfc_activate_cmd = wfc_activate_cmd + \
                               "\"com.google.android.wfcactivation/" \
                               ".VzwEmergencyAddressActivity\""
        else:
            wfc_activate_cmd = wfc_activate_cmd+ \
                               "\"com.google.android.wfcactivation/" \
                               ".can.WfcActivationCanadaActivity\""
        ad.adb.shell(wfc_activate_cmd)


def toggle_video_calling(log, ad, new_state=None):
    """Toggle enable/disable Video calling for default voice subscription.

    Args:
        ad: Android device object.
        new_state: Video mode state to set to.
            True for enable, False for disable.
            If None, opposite of the current state.

    Raises:
        TelTestUtilsError if platform does not support Video calling.
    """
    if not ad.droid.imsIsVtEnabledByPlatform():
        if new_state is not False:
            raise TelTestUtilsError("VT not supported by platform.")
        # if the user sets VT false and it's unavailable we just let it go
        return False

    current_state = ad.droid.imsIsVtEnabledByUser()
    if new_state is None:
        new_state = not current_state
    if new_state != current_state:
        ad.droid.imsSetVtSetting(new_state)
    return True


def toggle_video_calling_for_subscription(ad, new_state=None, sub_id=None):
    """Toggle enable/disable Video calling for subscription.

    Args:
        ad: Android device object.
        new_state: Video mode state to set to.
            True for enable, False for disable.
            If None, opposite of the current state.
        sub_id: subscription Id

    """
    try:
        if sub_id is None:
            sub_id = ad.droid.subscriptionGetDefaultVoiceSubId()
        current_state = ad.droid.imsMmTelIsVtSettingEnabled(sub_id)
        if new_state is None:
            new_state = not current_state
        if new_state != current_state:
            ad.log.info("SubId %s - Toggle VT from %s to %s", sub_id,
                        current_state, new_state)
            ad.droid.imsMmTelSetVtSettingEnabled(sub_id, new_state)
    except Exception as e:
        ad.log.error(e)
        return False
    return True


def _wait_for_droid_in_state(log, ad, max_time, state_check_func, *args,
                             **kwargs):
    while max_time >= 0:
        if state_check_func(log, ad, *args, **kwargs):
            return True

        time.sleep(WAIT_TIME_BETWEEN_STATE_CHECK)
        max_time -= WAIT_TIME_BETWEEN_STATE_CHECK

    return False


def _wait_for_droid_in_state_for_subscription(
        log, ad, sub_id, max_time, state_check_func, *args, **kwargs):
    while max_time >= 0:
        if state_check_func(log, ad, sub_id, *args, **kwargs):
            return True

        time.sleep(WAIT_TIME_BETWEEN_STATE_CHECK)
        max_time -= WAIT_TIME_BETWEEN_STATE_CHECK

    return False


def _wait_for_droids_in_state(log, ads, max_time, state_check_func, *args,
                              **kwargs):
    while max_time > 0:
        success = True
        for ad in ads:
            if not state_check_func(log, ad, *args, **kwargs):
                success = False
                break
        if success:
            return True

        time.sleep(WAIT_TIME_BETWEEN_STATE_CHECK)
        max_time -= WAIT_TIME_BETWEEN_STATE_CHECK

    return False


def is_phone_in_call(log, ad):
    """Return True if phone in call.

    Args:
        log: log object.
        ad:  android device.
    """
    try:
        return ad.droid.telecomIsInCall()
    except:
        return "mCallState=2" in ad.adb.shell(
            "dumpsys telephony.registry | grep mCallState")


def is_phone_not_in_call(log, ad):
    """Return True if phone not in call.

    Args:
        log: log object.
        ad:  android device.
    """
    in_call = ad.droid.telecomIsInCall()
    call_state = ad.droid.telephonyGetCallState()
    if in_call:
        ad.log.info("Device is In Call")
    if call_state != TELEPHONY_STATE_IDLE:
        ad.log.info("Call_state is %s, not %s", call_state,
                    TELEPHONY_STATE_IDLE)
    return ((not in_call) and (call_state == TELEPHONY_STATE_IDLE))


def wait_for_droid_in_call(log, ad, max_time):
    """Wait for android to be in call state.

    Args:
        log: log object.
        ad:  android device.
        max_time: maximal wait time.

    Returns:
        If phone become in call state within max_time, return True.
        Return False if timeout.
    """
    return _wait_for_droid_in_state(log, ad, max_time, is_phone_in_call)


def is_phone_in_call_active(ad, call_id=None):
    """Return True if phone in active call.

    Args:
        log: log object.
        ad:  android device.
        call_id: the call id
    """
    if ad.droid.telecomIsInCall():
        if not call_id:
            call_id = ad.droid.telecomCallGetCallIds()[0]
        call_state = ad.droid.telecomCallGetCallState(call_id)
        ad.log.info("%s state is %s", call_id, call_state)
        return call_state == "ACTIVE"
    else:
        ad.log.info("Not in telecomIsInCall")
        return False


def wait_for_in_call_active(ad,
                            timeout=MAX_WAIT_TIME_ACCEPT_CALL_TO_OFFHOOK_EVENT,
                            interval=WAIT_TIME_BETWEEN_STATE_CHECK,
                            call_id=None):
    """Wait for call reach active state.

    Args:
        log: log object.
        ad:  android device.
        call_id: the call id
    """
    if not call_id:
        call_id = ad.droid.telecomCallGetCallIds()[0]
    args = [ad, call_id]
    if not wait_for_state(is_phone_in_call_active, True, timeout, interval,
                          *args):
        ad.log.error("Call did not reach ACTIVE state")
        return False
    else:
        return True


def wait_for_telecom_ringing(log, ad, max_time=MAX_WAIT_TIME_TELECOM_RINGING):
    """Wait for android to be in telecom ringing state.

    Args:
        log: log object.
        ad:  android device.
        max_time: maximal wait time. This is optional.
            Default Value is MAX_WAIT_TIME_TELECOM_RINGING.

    Returns:
        If phone become in telecom ringing state within max_time, return True.
        Return False if timeout.
    """
    return _wait_for_droid_in_state(
        log, ad, max_time, lambda log, ad: ad.droid.telecomIsRinging())


def wait_for_droid_not_in_call(log, ad, max_time=MAX_WAIT_TIME_CALL_DROP):
    """Wait for android to be not in call state.

    Args:
        log: log object.
        ad:  android device.
        max_time: maximal wait time.

    Returns:
        If phone become not in call state within max_time, return True.
        Return False if timeout.
    """
    return _wait_for_droid_in_state(log, ad, max_time, is_phone_not_in_call)


def _is_attached(log, ad, voice_or_data):
    return _is_attached_for_subscription(
        log, ad, ad.droid.subscriptionGetDefaultSubId(), voice_or_data)


def _is_attached_for_subscription(log, ad, sub_id, voice_or_data):
    rat = get_network_rat_for_subscription(log, ad, sub_id, voice_or_data)
    ad.log.info("Sub_id %s network RAT is %s for %s", sub_id, rat,
                voice_or_data)
    return rat != RAT_UNKNOWN


def is_voice_attached(log, ad):
    return _is_attached_for_subscription(
        log, ad, ad.droid.subscriptionGetDefaultSubId(), NETWORK_SERVICE_VOICE)


def wait_for_voice_attach(log, ad, max_time=MAX_WAIT_TIME_NW_SELECTION):
    """Wait for android device to attach on voice.

    Args:
        log: log object.
        ad:  android device.
        max_time: maximal wait time.

    Returns:
        Return True if device attach voice within max_time.
        Return False if timeout.
    """
    return _wait_for_droid_in_state(log, ad, max_time, _is_attached,
                                    NETWORK_SERVICE_VOICE)


def wait_for_voice_attach_for_subscription(
        log, ad, sub_id, max_time=MAX_WAIT_TIME_NW_SELECTION):
    """Wait for android device to attach on voice in subscription id.

    Args:
        log: log object.
        ad:  android device.
        sub_id: subscription id.
        max_time: maximal wait time.

    Returns:
        Return True if device attach voice within max_time.
        Return False if timeout.
    """
    if not _wait_for_droid_in_state_for_subscription(
            log, ad, sub_id, max_time, _is_attached_for_subscription,
            NETWORK_SERVICE_VOICE):
        return False

    # TODO: b/26295983 if pone attach to 1xrtt from unknown, phone may not
    # receive incoming call immediately.
    if ad.droid.telephonyGetCurrentVoiceNetworkType() == RAT_1XRTT:
        time.sleep(WAIT_TIME_1XRTT_VOICE_ATTACH)
    return True


def wait_for_data_attach(log, ad, max_time):
    """Wait for android device to attach on data.

    Args:
        log: log object.
        ad:  android device.
        max_time: maximal wait time.

    Returns:
        Return True if device attach data within max_time.
        Return False if timeout.
    """
    return _wait_for_droid_in_state(log, ad, max_time, _is_attached,
                                    NETWORK_SERVICE_DATA)


def wait_for_data_attach_for_subscription(log, ad, sub_id, max_time):
    """Wait for android device to attach on data in subscription id.

    Args:
        log: log object.
        ad:  android device.
        sub_id: subscription id.
        max_time: maximal wait time.

    Returns:
        Return True if device attach data within max_time.
        Return False if timeout.
    """
    return _wait_for_droid_in_state_for_subscription(
        log, ad, sub_id, max_time, _is_attached_for_subscription,
        NETWORK_SERVICE_DATA)


def is_ims_registered(log, ad):
    """Return True if IMS registered.

    Args:
        log: log object.
        ad: android device.

    Returns:
        Return True if IMS registered.
        Return False if IMS not registered.
    """
    return ad.droid.telephonyIsImsRegistered()


def wait_for_ims_registered(log, ad, max_time=MAX_WAIT_TIME_WFC_ENABLED):
    """Wait for android device to register on ims.

    Args:
        log: log object.
        ad:  android device.
        max_time: maximal wait time.

    Returns:
        Return True if device register ims successfully within max_time.
        Return False if timeout.
    """
    return _wait_for_droid_in_state(log, ad, max_time, is_ims_registered)


def is_volte_enabled(log, ad):
    """Return True if VoLTE feature bit is True.

    Args:
        log: log object.
        ad: android device.

    Returns:
        Return True if VoLTE feature bit is True and IMS registered.
        Return False if VoLTE feature bit is False or IMS not registered.
    """
    if not is_ims_registered(log, ad):
        ad.log.info("IMS is not registered.")
        return False
    if not ad.droid.telephonyIsVolteAvailable():
        ad.log.info("IMS is registered, IsVolteCallingAvailble is False")
        return False
    else:
        ad.log.info("IMS is registered, IsVolteCallingAvailble is True")
        return True


def is_video_enabled(log, ad):
    """Return True if Video Calling feature bit is True.

    Args:
        log: log object.
        ad: android device.

    Returns:
        Return True if Video Calling feature bit is True and IMS registered.
        Return False if Video Calling feature bit is False or IMS not registered.
    """
    video_status = ad.droid.telephonyIsVideoCallingAvailable()
    if video_status is True and is_ims_registered(log, ad) is False:
        ad.log.error(
            "Error! Video Call is Available, but IMS is not registered.")
        return False
    return video_status


def wait_for_volte_enabled(log, ad, max_time=MAX_WAIT_TIME_VOLTE_ENABLED):
    """Wait for android device to report VoLTE enabled bit true.

    Args:
        log: log object.
        ad:  android device.
        max_time: maximal wait time.

    Returns:
        Return True if device report VoLTE enabled bit true within max_time.
        Return False if timeout.
    """
    return _wait_for_droid_in_state(log, ad, max_time, is_volte_enabled)


def wait_for_video_enabled(log, ad, max_time=MAX_WAIT_TIME_VOLTE_ENABLED):
    """Wait for android device to report Video Telephony enabled bit true.

    Args:
        log: log object.
        ad:  android device.
        max_time: maximal wait time.

    Returns:
        Return True if device report Video Telephony enabled bit true within max_time.
        Return False if timeout.
    """
    return _wait_for_droid_in_state(log, ad, max_time, is_video_enabled)


def is_wfc_enabled(log, ad):
    """Return True if WiFi Calling feature bit is True.

    Args:
        log: log object.
        ad: android device.

    Returns:
        Return True if WiFi Calling feature bit is True and IMS registered.
        Return False if WiFi Calling feature bit is False or IMS not registered.
    """
    if not is_ims_registered(log, ad):
        ad.log.info("IMS is not registered.")
        return False
    if not ad.droid.telephonyIsWifiCallingAvailable():
        ad.log.info("IMS is registered, IsWifiCallingAvailble is False")
        return False
    else:
        ad.log.info("IMS is registered, IsWifiCallingAvailble is True")
        return True


def wait_for_wfc_enabled(log, ad, max_time=MAX_WAIT_TIME_WFC_ENABLED):
    """Wait for android device to report WiFi Calling enabled bit true.

    Args:
        log: log object.
        ad:  android device.
        max_time: maximal wait time.
            Default value is MAX_WAIT_TIME_WFC_ENABLED.

    Returns:
        Return True if device report WiFi Calling enabled bit true within max_time.
        Return False if timeout.
    """
    return _wait_for_droid_in_state(log, ad, max_time, is_wfc_enabled)


def wait_for_wfc_disabled(log, ad, max_time=MAX_WAIT_TIME_WFC_DISABLED):
    """Wait for android device to report WiFi Calling enabled bit false.

    Args:
        log: log object.
        ad:  android device.
        max_time: maximal wait time.
            Default value is MAX_WAIT_TIME_WFC_DISABLED.

    Returns:
        Return True if device report WiFi Calling enabled bit false within max_time.
        Return False if timeout.
    """
    return _wait_for_droid_in_state(
        log, ad, max_time, lambda log, ad: not is_wfc_enabled(log, ad))


def get_phone_number(log, ad):
    """Get phone number for default subscription

    Args:
        log: log object.
        ad: Android device object.

    Returns:
        Phone number.
    """
    return get_phone_number_for_subscription(log, ad,
                                             get_outgoing_voice_sub_id(ad))


def get_phone_number_for_subscription(log, ad, subid):
    """Get phone number for subscription

    Args:
        log: log object.
        ad: Android device object.
        subid: subscription id.

    Returns:
        Phone number.
    """
    number = None
    try:
        number = ad.telephony['subscription'][subid]['phone_num']
    except KeyError:
        number = ad.droid.telephonyGetLine1NumberForSubscription(subid)
    return number


def set_phone_number(log, ad, phone_num):
    """Set phone number for default subscription

    Args:
        log: log object.
        ad: Android device object.
        phone_num: phone number string.

    Returns:
        True if success.
    """
    return set_phone_number_for_subscription(log, ad,
                                             get_outgoing_voice_sub_id(ad),
                                             phone_num)


def set_phone_number_for_subscription(log, ad, subid, phone_num):
    """Set phone number for subscription

    Args:
        log: log object.
        ad: Android device object.
        subid: subscription id.
        phone_num: phone number string.

    Returns:
        True if success.
    """
    try:
        ad.telephony['subscription'][subid]['phone_num'] = phone_num
    except Exception:
        return False
    return True


def get_operator_name(log, ad, subId=None):
    """Get operator name (e.g. vzw, tmo) of droid.

    Args:
        ad: Android device object.
        sub_id: subscription ID
            Optional, default is None

    Returns:
        Operator name.
    """
    try:
        if subId is not None:
            result = operator_name_from_plmn_id(
                ad.droid.telephonyGetNetworkOperatorForSubscription(subId))
        else:
            result = operator_name_from_plmn_id(
                ad.droid.telephonyGetNetworkOperator())
    except KeyError:
        try:
            if subId is not None:
                result = ad.droid.telephonyGetNetworkOperatorNameForSubscription(
                    subId)
            else:
                result = ad.droid.telephonyGetNetworkOperatorName()
            result = operator_name_from_network_name(result)
        except Exception:
            result = CARRIER_UNKNOWN
    ad.log.info("Operator Name is %s", result)
    return result


def get_model_name(ad):
    """Get android device model name

    Args:
        ad: Android device object

    Returns:
        model name string
    """
    # TODO: Create translate table.
    model = ad.model
    if (model.startswith(AOSP_PREFIX)):
        model = model[len(AOSP_PREFIX):]
    return model


def is_sms_match(event, phonenumber_tx, text):
    """Return True if 'text' equals to event['data']['Text']
        and phone number match.

    Args:
        event: Event object to verify.
        phonenumber_tx: phone number for sender.
        text: text string to verify.

    Returns:
        Return True if 'text' equals to event['data']['Text']
            and phone number match.
    """
    return (check_phone_number_match(event['data']['Sender'], phonenumber_tx)
            and event['data']['Text'].strip() == text)


def is_sms_partial_match(event, phonenumber_tx, text):
    """Return True if 'text' starts with event['data']['Text']
        and phone number match.

    Args:
        event: Event object to verify.
        phonenumber_tx: phone number for sender.
        text: text string to verify.

    Returns:
        Return True if 'text' starts with event['data']['Text']
            and phone number match.
    """
    event_text = event['data']['Text'].strip()
    if event_text.startswith("("):
        event_text = event_text.split(")")[-1]
    return (check_phone_number_match(event['data']['Sender'], phonenumber_tx)
            and text.startswith(event_text))


def sms_send_receive_verify(log,
                            ad_tx,
                            ad_rx,
                            array_message,
                            max_wait_time=MAX_WAIT_TIME_SMS_RECEIVE,
                            expected_result=True,
                            slot_id_rx=None):
    """Send SMS, receive SMS, and verify content and sender's number.

        Send (several) SMS from droid_tx to droid_rx.
        Verify SMS is sent, delivered and received.
        Verify received content and sender's number are correct.

    Args:
        log: Log object.
        ad_tx: Sender's Android Device Object
        ad_rx: Receiver's Android Device Object
        array_message: the array of message to send/receive
        slot_id_rx: the slot on the Receiver's android device (0/1)
    """
    subid_tx = get_outgoing_message_sub_id(ad_tx)
    if slot_id_rx is None:
        subid_rx = get_incoming_message_sub_id(ad_rx)
    else:
        subid_rx = get_subid_from_slot_index(log, ad_rx, slot_id_rx)

    result = sms_send_receive_verify_for_subscription(
        log, ad_tx, ad_rx, subid_tx, subid_rx, array_message, max_wait_time)
    if result != expected_result:
        log_messaging_screen_shot(ad_tx, test_name="sms_tx")
        log_messaging_screen_shot(ad_rx, test_name="sms_rx")
    return result == expected_result


def wait_for_matching_sms(log,
                          ad_rx,
                          phonenumber_tx,
                          text,
                          allow_multi_part_long_sms=True,
                          max_wait_time=MAX_WAIT_TIME_SMS_RECEIVE):
    """Wait for matching incoming SMS.

    Args:
        log: Log object.
        ad_rx: Receiver's Android Device Object
        phonenumber_tx: Sender's phone number.
        text: SMS content string.
        allow_multi_part_long_sms: is long SMS allowed to be received as
            multiple short SMS. This is optional, default value is True.

    Returns:
        True if matching incoming SMS is received.
    """
    if not allow_multi_part_long_sms:
        try:
            ad_rx.messaging_ed.wait_for_event(EventSmsReceived, is_sms_match,
                                              max_wait_time, phonenumber_tx,
                                              text)
            ad_rx.log.info("Got event %s", EventSmsReceived)
            return True
        except Empty:
            ad_rx.log.error("No matched SMS received event.")
            return False
    else:
        try:
            received_sms = ''
            remaining_text = text
            while (remaining_text != ''):
                event = ad_rx.messaging_ed.wait_for_event(
                    EventSmsReceived, is_sms_partial_match, max_wait_time,
                    phonenumber_tx, remaining_text)
                event_text = event['data']['Text'].split(")")[-1].strip()
                event_text_length = len(event_text)
                ad_rx.log.info("Got event %s of text length %s from %s",
                               EventSmsReceived, event_text_length,
                               phonenumber_tx)
                remaining_text = remaining_text[event_text_length:]
                received_sms += event_text
            ad_rx.log.info("Received SMS of length %s", len(received_sms))
            return True
        except Empty:
            ad_rx.log.error(
                "Missing SMS received event of text length %s from %s",
                len(remaining_text), phonenumber_tx)
            if received_sms != '':
                ad_rx.log.error(
                    "Only received partial matched SMS of length %s",
                    len(received_sms))
            return False


def is_mms_match(event, phonenumber_tx, text):
    """Return True if 'text' equals to event['data']['Text']
        and phone number match.

    Args:
        event: Event object to verify.
        phonenumber_tx: phone number for sender.
        text: text string to verify.

    Returns:
        Return True if 'text' equals to event['data']['Text']
            and phone number match.
    """
    #TODO:  add mms matching after mms message parser is added in sl4a. b/34276948
    return True


def wait_for_matching_mms(log,
                          ad_rx,
                          phonenumber_tx,
                          text,
                          max_wait_time=MAX_WAIT_TIME_SMS_RECEIVE):
    """Wait for matching incoming SMS.

    Args:
        log: Log object.
        ad_rx: Receiver's Android Device Object
        phonenumber_tx: Sender's phone number.
        text: SMS content string.
        allow_multi_part_long_sms: is long SMS allowed to be received as
            multiple short SMS. This is optional, default value is True.

    Returns:
        True if matching incoming SMS is received.
    """
    try:
        #TODO: add mms matching after mms message parser is added in sl4a. b/34276948
        ad_rx.messaging_ed.wait_for_event(EventMmsDownloaded, is_mms_match,
                                          max_wait_time, phonenumber_tx, text)
        ad_rx.log.info("Got event %s", EventMmsDownloaded)
        return True
    except Empty:
        ad_rx.log.warning("No matched MMS downloaded event.")
        return False


def sms_send_receive_verify_for_subscription(
        log,
        ad_tx,
        ad_rx,
        subid_tx,
        subid_rx,
        array_message,
        max_wait_time=MAX_WAIT_TIME_SMS_RECEIVE):
    """Send SMS, receive SMS, and verify content and sender's number.

        Send (several) SMS from droid_tx to droid_rx.
        Verify SMS is sent, delivered and received.
        Verify received content and sender's number are correct.

    Args:
        log: Log object.
        ad_tx: Sender's Android Device Object..
        ad_rx: Receiver's Android Device Object.
        subid_tx: Sender's subsciption ID to be used for SMS
        subid_rx: Receiver's subsciption ID to be used for SMS
        array_message: the array of message to send/receive
    """
    phonenumber_tx = ad_tx.telephony['subscription'][subid_tx]['phone_num']
    phonenumber_rx = ad_rx.telephony['subscription'][subid_rx]['phone_num']

    for ad in (ad_tx, ad_rx):
        ad.send_keycode("BACK")
        if not getattr(ad, "messaging_droid", None):
            ad.messaging_droid, ad.messaging_ed = ad.get_droid()
            ad.messaging_ed.start()
        else:
            try:
                if not ad.messaging_droid.is_live:
                    ad.messaging_droid, ad.messaging_ed = ad.get_droid()
                    ad.messaging_ed.start()
                else:
                    ad.messaging_ed.clear_all_events()
                ad.messaging_droid.logI(
                    "Start sms_send_receive_verify_for_subscription test")
            except Exception:
                ad.log.info("Create new sl4a session for messaging")
                ad.messaging_droid, ad.messaging_ed = ad.get_droid()
                ad.messaging_ed.start()

    for text in array_message:
        length = len(text)
        ad_tx.log.info("Sending SMS from %s to %s, len: %s, content: %s.",
                       phonenumber_tx, phonenumber_rx, length, text)
        try:
            ad_rx.messaging_ed.clear_events(EventSmsReceived)
            ad_tx.messaging_ed.clear_events(EventSmsSentSuccess)
            ad_tx.messaging_ed.clear_events(EventSmsSentFailure)
            ad_rx.messaging_droid.smsStartTrackingIncomingSmsMessage()
            time.sleep(1)  #sleep 100ms after starting event tracking
            ad_tx.messaging_droid.logI("Sending SMS of length %s" % length)
            ad_rx.messaging_droid.logI("Expecting SMS of length %s" % length)
            ad_tx.messaging_droid.smsSendTextMessage(phonenumber_rx, text,
                                                     True)
            try:
                events = ad_tx.messaging_ed.pop_events(
                    "(%s|%s|%s|%s)" %
                    (EventSmsSentSuccess, EventSmsSentFailure,
                     EventSmsDeliverSuccess,
                     EventSmsDeliverFailure), max_wait_time)
                for event in events:
                    ad_tx.log.info("Got event %s", event["name"])
                    if event["name"] == EventSmsSentFailure or event["name"] == EventSmsDeliverFailure:
                        if event.get("data") and event["data"].get("Reason"):
                            ad_tx.log.error("%s with reason: %s",
                                            event["name"],
                                            event["data"]["Reason"])
                        return False
                    elif event["name"] == EventSmsSentSuccess or event["name"] == EventSmsDeliverSuccess:
                        break
            except Empty:
                ad_tx.log.error("No %s or %s event for SMS of length %s.",
                                EventSmsSentSuccess, EventSmsSentFailure,
                                length)
                return False

            if not wait_for_matching_sms(
                    log,
                    ad_rx,
                    phonenumber_tx,
                    text,
                    allow_multi_part_long_sms=True):
                ad_rx.log.error("No matching received SMS of length %s.",
                                length)
                return False
        except Exception as e:
            log.error("Exception error %s", e)
            raise
        finally:
            ad_rx.messaging_droid.smsStopTrackingIncomingSmsMessage()
    return True


def mms_send_receive_verify(log,
                            ad_tx,
                            ad_rx,
                            array_message,
                            max_wait_time=MAX_WAIT_TIME_SMS_RECEIVE,
                            expected_result=True,
                            slot_id_rx=None):
    """Send MMS, receive MMS, and verify content and sender's number.

        Send (several) MMS from droid_tx to droid_rx.
        Verify MMS is sent, delivered and received.
        Verify received content and sender's number are correct.

    Args:
        log: Log object.
        ad_tx: Sender's Android Device Object
        ad_rx: Receiver's Android Device Object
        array_message: the array of message to send/receive
    """
    subid_tx = get_outgoing_message_sub_id(ad_tx)
    if slot_id_rx is None:
        subid_rx = get_incoming_message_sub_id(ad_rx)
    else:
        subid_rx = get_subid_from_slot_index(log, ad_rx, slot_id_rx)

    result = mms_send_receive_verify_for_subscription(
        log, ad_tx, ad_rx, subid_tx, subid_rx, array_message, max_wait_time)
    if result != expected_result:
        log_messaging_screen_shot(ad_tx, test_name="mms_tx")
        log_messaging_screen_shot(ad_rx, test_name="mms_rx")
    return result == expected_result


def sms_mms_send_logcat_check(ad, type, begin_time):
    type = type.upper()
    log_results = ad.search_logcat(
        "%s Message sent successfully" % type, begin_time=begin_time)
    if log_results:
        ad.log.info("Found %s sent successful log message: %s", type,
                    log_results[-1]["log_message"])
        return True
    else:
        log_results = ad.search_logcat(
            "ProcessSentMessageAction: Done sending %s message" % type,
            begin_time=begin_time)
        if log_results:
            for log_result in log_results:
                if "status is SUCCEEDED" in log_result["log_message"]:
                    ad.log.info(
                        "Found BugleDataModel %s send succeed log message: %s",
                        type, log_result["log_message"])
                    return True
    return False


def sms_mms_receive_logcat_check(ad, type, begin_time):
    type = type.upper()
    smshandle_logs = ad.search_logcat(
        "InboundSmsHandler: No broadcast sent on processing EVENT_BROADCAST_SMS",
        begin_time=begin_time)
    if smshandle_logs:
        ad.log.warning("Found %s", smshandle_logs[-1]["log_message"])
    log_results = ad.search_logcat(
        "New %s Received" % type, begin_time=begin_time) or \
        ad.search_logcat("New %s Downloaded" % type, begin_time=begin_time)
    if log_results:
        ad.log.info("Found SL4A %s received log message: %s", type,
                    log_results[-1]["log_message"])
        return True
    else:
        log_results = ad.search_logcat(
            "Received %s message" % type, begin_time=begin_time)
        if log_results:
            ad.log.info("Found %s received log message: %s", type,
                        log_results[-1]["log_message"])
        log_results = ad.search_logcat(
            "ProcessDownloadedMmsAction", begin_time=begin_time)
        for log_result in log_results:
            ad.log.info("Found %s", log_result["log_message"])
            if "status is SUCCEEDED" in log_result["log_message"]:
                ad.log.info("Download succeed with ProcessDownloadedMmsAction")
                return True
    return False


#TODO: add mms matching after mms message parser is added in sl4a. b/34276948
def mms_send_receive_verify_for_subscription(
        log,
        ad_tx,
        ad_rx,
        subid_tx,
        subid_rx,
        array_payload,
        max_wait_time=MAX_WAIT_TIME_SMS_RECEIVE):
    """Send MMS, receive MMS, and verify content and sender's number.

        Send (several) MMS from droid_tx to droid_rx.
        Verify MMS is sent, delivered and received.
        Verify received content and sender's number are correct.

    Args:
        log: Log object.
        ad_tx: Sender's Android Device Object..
        ad_rx: Receiver's Android Device Object.
        subid_tx: Sender's subsciption ID to be used for SMS
        subid_rx: Receiver's subsciption ID to be used for SMS
        array_message: the array of message to send/receive
    """

    phonenumber_tx = ad_tx.telephony['subscription'][subid_tx]['phone_num']
    phonenumber_rx = ad_rx.telephony['subscription'][subid_rx]['phone_num']
    toggle_enforce = False

    for ad in (ad_tx, ad_rx):
        ad.send_keycode("BACK")
        if "Permissive" not in ad.adb.shell("su root getenforce"):
            ad.adb.shell("su root setenforce 0")
            toggle_enforce = True
        if not getattr(ad, "messaging_droid", None):
            ad.messaging_droid, ad.messaging_ed = ad.get_droid()
            ad.messaging_ed.start()
        else:
            try:
                if not ad.messaging_droid.is_live:
                    ad.messaging_droid, ad.messaging_ed = ad.get_droid()
                    ad.messaging_ed.start()
                else:
                    ad.messaging_ed.clear_all_events()
                ad.messaging_droid.logI(
                    "Start mms_send_receive_verify_for_subscription test")
            except Exception:
                ad.log.info("Create new sl4a session for messaging")
                ad.messaging_droid, ad.messaging_ed = ad.get_droid()
                ad.messaging_ed.start()

    for subject, message, filename in array_payload:
        ad_tx.messaging_ed.clear_events(EventMmsSentSuccess)
        ad_tx.messaging_ed.clear_events(EventMmsSentFailure)
        ad_rx.messaging_ed.clear_events(EventMmsDownloaded)
        ad_rx.messaging_droid.smsStartTrackingIncomingMmsMessage()
        ad_tx.log.info(
            "Sending MMS from %s to %s, subject: %s, message: %s, file: %s.",
            phonenumber_tx, phonenumber_rx, subject, message, filename)
        try:
            ad_tx.messaging_droid.smsSendMultimediaMessage(
                phonenumber_rx, subject, message, phonenumber_tx, filename)
            try:
                events = ad_tx.messaging_ed.pop_events(
                    "(%s|%s)" % (EventMmsSentSuccess,
                                 EventMmsSentFailure), max_wait_time)
                for event in events:
                    ad_tx.log.info("Got event %s", event["name"])
                    if event["name"] == EventMmsSentFailure:
                        if event.get("data") and event["data"].get("Reason"):
                            ad_tx.log.error("%s with reason: %s",
                                            event["name"],
                                            event["data"]["Reason"])
                        return False
                    elif event["name"] == EventMmsSentSuccess:
                        break
            except Empty:
                ad_tx.log.warning("No %s or %s event.", EventMmsSentSuccess,
                                  EventMmsSentFailure)

            if not wait_for_matching_mms(log, ad_rx, phonenumber_tx,
                                         message, max_wait_time):
                return False
        except Exception as e:
            log.error("Exception error %s", e)
            raise
        finally:
            ad_rx.droid.smsStopTrackingIncomingMmsMessage()
            for ad in (ad_tx, ad_rx):
                if toggle_enforce:
                    ad.send_keycode("BACK")
                    ad.adb.shell("su root setenforce 1")
    return True


def mms_receive_verify_after_call_hangup(
        log, ad_tx, ad_rx, array_message,
        max_wait_time=MAX_WAIT_TIME_SMS_RECEIVE):
    """Verify the suspanded MMS during call will send out after call release.

        Hangup call from droid_tx to droid_rx.
        Verify MMS is sent, delivered and received.
        Verify received content and sender's number are correct.

    Args:
        log: Log object.
        ad_tx: Sender's Android Device Object
        ad_rx: Receiver's Android Device Object
        array_message: the array of message to send/receive
    """
    return mms_receive_verify_after_call_hangup_for_subscription(
        log, ad_tx, ad_rx, get_outgoing_message_sub_id(ad_tx),
        get_incoming_message_sub_id(ad_rx), array_message, max_wait_time)


#TODO: add mms matching after mms message parser is added in sl4a. b/34276948
def mms_receive_verify_after_call_hangup_for_subscription(
        log,
        ad_tx,
        ad_rx,
        subid_tx,
        subid_rx,
        array_payload,
        max_wait_time=MAX_WAIT_TIME_SMS_RECEIVE):
    """Verify the suspanded MMS during call will send out after call release.

        Hangup call from droid_tx to droid_rx.
        Verify MMS is sent, delivered and received.
        Verify received content and sender's number are correct.

    Args:
        log: Log object.
        ad_tx: Sender's Android Device Object..
        ad_rx: Receiver's Android Device Object.
        subid_tx: Sender's subsciption ID to be used for SMS
        subid_rx: Receiver's subsciption ID to be used for SMS
        array_message: the array of message to send/receive
    """

    phonenumber_tx = ad_tx.telephony['subscription'][subid_tx]['phone_num']
    phonenumber_rx = ad_rx.telephony['subscription'][subid_rx]['phone_num']
    for ad in (ad_tx, ad_rx):
        if not getattr(ad, "messaging_droid", None):
            ad.messaging_droid, ad.messaging_ed = ad.get_droid()
            ad.messaging_ed.start()
    for subject, message, filename in array_payload:
        ad_rx.log.info(
            "Waiting MMS from %s to %s, subject: %s, message: %s, file: %s.",
            phonenumber_tx, phonenumber_rx, subject, message, filename)
        ad_rx.messaging_droid.smsStartTrackingIncomingMmsMessage()
        time.sleep(5)
        try:
            hangup_call(log, ad_tx)
            hangup_call(log, ad_rx)
            try:
                ad_tx.messaging_ed.pop_event(EventMmsSentSuccess,
                                             max_wait_time)
                ad_tx.log.info("Got event %s", EventMmsSentSuccess)
            except Empty:
                log.warning("No sent_success event.")
            if not wait_for_matching_mms(log, ad_rx, phonenumber_tx, message):
                return False
        finally:
            ad_rx.droid.smsStopTrackingIncomingMmsMessage()
    return True


def ensure_preferred_network_type_for_subscription(
        ad,
        network_preference
        ):
    sub_id = ad.droid.subscriptionGetDefaultSubId()
    if not ad.droid.telephonySetPreferredNetworkTypesForSubscription(
            network_preference, sub_id):
        ad.log.error("Set sub_id %s Preferred Networks Type %s failed.",
                     sub_id, network_preference)
    return True


def ensure_network_rat(log,
                       ad,
                       network_preference,
                       rat_family,
                       voice_or_data=None,
                       max_wait_time=MAX_WAIT_TIME_NW_SELECTION,
                       toggle_apm_after_setting=False):
    """Ensure ad's current network is in expected rat_family.
    """
    return ensure_network_rat_for_subscription(
        log, ad, ad.droid.subscriptionGetDefaultSubId(), network_preference,
        rat_family, voice_or_data, max_wait_time, toggle_apm_after_setting)


def ensure_network_rat_for_subscription(
        log,
        ad,
        sub_id,
        network_preference,
        rat_family,
        voice_or_data=None,
        max_wait_time=MAX_WAIT_TIME_NW_SELECTION,
        toggle_apm_after_setting=False):
    """Ensure ad's current network is in expected rat_family.
    """
    if not ad.droid.telephonySetPreferredNetworkTypesForSubscription(
            network_preference, sub_id):
        ad.log.error("Set sub_id %s Preferred Networks Type %s failed.",
                     sub_id, network_preference)
        return False
    if is_droid_in_rat_family_for_subscription(log, ad, sub_id, rat_family,
                                               voice_or_data):
        ad.log.info("Sub_id %s in RAT %s for %s", sub_id, rat_family,
                    voice_or_data)
        return True

    if toggle_apm_after_setting:
        toggle_airplane_mode(log, ad, new_state=True, strict_checking=False)
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)
        toggle_airplane_mode(log, ad, new_state=None, strict_checking=False)

    result = wait_for_network_rat_for_subscription(
        log, ad, sub_id, rat_family, max_wait_time, voice_or_data)

    log.info(
        "End of ensure_network_rat_for_subscription for %s. "
        "Setting to %s, Expecting %s %s. Current: voice: %s(family: %s), "
        "data: %s(family: %s)", ad.serial, network_preference, rat_family,
        voice_or_data,
        ad.droid.telephonyGetCurrentVoiceNetworkTypeForSubscription(sub_id),
        rat_family_from_rat(
            ad.droid.telephonyGetCurrentVoiceNetworkTypeForSubscription(
                sub_id)),
        ad.droid.telephonyGetCurrentDataNetworkTypeForSubscription(sub_id),
        rat_family_from_rat(
            ad.droid.telephonyGetCurrentDataNetworkTypeForSubscription(
                sub_id)))
    return result


def ensure_network_preference(log,
                              ad,
                              network_preference,
                              voice_or_data=None,
                              max_wait_time=MAX_WAIT_TIME_NW_SELECTION,
                              toggle_apm_after_setting=False):
    """Ensure that current rat is within the device's preferred network rats.
    """
    return ensure_network_preference_for_subscription(
        log, ad, ad.droid.subscriptionGetDefaultSubId(), network_preference,
        voice_or_data, max_wait_time, toggle_apm_after_setting)


def ensure_network_preference_for_subscription(
        log,
        ad,
        sub_id,
        network_preference,
        voice_or_data=None,
        max_wait_time=MAX_WAIT_TIME_NW_SELECTION,
        toggle_apm_after_setting=False):
    """Ensure ad's network preference is <network_preference> for sub_id.
    """
    rat_family_list = rat_families_for_network_preference(network_preference)
    if not ad.droid.telephonySetPreferredNetworkTypesForSubscription(
            network_preference, sub_id):
        log.error("Set Preferred Networks failed.")
        return False
    if is_droid_in_rat_family_list_for_subscription(
            log, ad, sub_id, rat_family_list, voice_or_data):
        return True

    if toggle_apm_after_setting:
        toggle_airplane_mode(log, ad, new_state=True, strict_checking=False)
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)
        toggle_airplane_mode(log, ad, new_state=False, strict_checking=False)

    result = wait_for_preferred_network_for_subscription(
        log, ad, sub_id, network_preference, max_wait_time, voice_or_data)

    ad.log.info(
        "End of ensure_network_preference_for_subscription. "
        "Setting to %s, Expecting %s %s. Current: voice: %s(family: %s), "
        "data: %s(family: %s)", network_preference, rat_family_list,
        voice_or_data,
        ad.droid.telephonyGetCurrentVoiceNetworkTypeForSubscription(sub_id),
        rat_family_from_rat(
            ad.droid.telephonyGetCurrentVoiceNetworkTypeForSubscription(
                sub_id)),
        ad.droid.telephonyGetCurrentDataNetworkTypeForSubscription(sub_id),
        rat_family_from_rat(
            ad.droid.telephonyGetCurrentDataNetworkTypeForSubscription(
                sub_id)))
    return result


def ensure_network_generation(log,
                              ad,
                              generation,
                              max_wait_time=MAX_WAIT_TIME_NW_SELECTION,
                              voice_or_data=None,
                              toggle_apm_after_setting=False):
    """Ensure ad's network is <network generation> for default subscription ID.

    Set preferred network generation to <generation>.
    Toggle ON/OFF airplane mode if necessary.
    Wait for ad in expected network type.
    """
    return ensure_network_generation_for_subscription(
        log, ad, ad.droid.subscriptionGetDefaultSubId(), generation,
        max_wait_time, voice_or_data, toggle_apm_after_setting)


def ensure_network_generation_for_subscription(
        log,
        ad,
        sub_id,
        generation,
        max_wait_time=MAX_WAIT_TIME_NW_SELECTION,
        voice_or_data=None,
        toggle_apm_after_setting=False):
    """Ensure ad's network is <network generation> for specified subscription ID.

    Set preferred network generation to <generation>.
    Toggle ON/OFF airplane mode if necessary.
    Wait for ad in expected network type.
    """
    ad.log.info(
        "RAT network type voice: %s, data: %s",
        ad.droid.telephonyGetCurrentVoiceNetworkTypeForSubscription(sub_id),
        ad.droid.telephonyGetCurrentDataNetworkTypeForSubscription(sub_id))

    try:
        ad.log.info("Finding the network preference for generation %s for "
                    "operator %s phone type %s", generation,
                    ad.telephony["subscription"][sub_id]["operator"],
                    ad.telephony["subscription"][sub_id]["phone_type"])
        network_preference = network_preference_for_generation(
            generation, ad.telephony["subscription"][sub_id]["operator"],
            ad.telephony["subscription"][sub_id]["phone_type"])
        if ad.telephony["subscription"][sub_id]["operator"] == CARRIER_FRE \
            and generation == GEN_4G:
            network_preference = NETWORK_MODE_LTE_ONLY
        ad.log.info("Network preference for %s is %s", generation,
                    network_preference)
        rat_family = rat_family_for_generation(
            generation, ad.telephony["subscription"][sub_id]["operator"],
            ad.telephony["subscription"][sub_id]["phone_type"])
    except KeyError as e:
        ad.log.error("Failed to find a rat_family entry for generation %s"
                     " for subscriber id %s with error %s", generation,
                     sub_id, e)
        return False

    if not set_preferred_network_mode_pref(log, ad, sub_id,
                                           network_preference):
        return False

    if hasattr(ad, "dsds") and voice_or_data == "data" and sub_id != get_default_data_sub_id(ad):
        ad.log.info("MSIM - Non DDS, ignore data RAT")
        return True

    if is_droid_in_network_generation_for_subscription(
            log, ad, sub_id, generation, voice_or_data):
        return True

    if toggle_apm_after_setting:
        toggle_airplane_mode(log, ad, new_state=True, strict_checking=False)
        time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)
        toggle_airplane_mode(log, ad, new_state=False, strict_checking=False)

    result = wait_for_network_generation_for_subscription(
        log, ad, sub_id, generation, max_wait_time, voice_or_data)

    ad.log.info(
        "Ensure network %s %s %s. With network preference %s, "
        "current: voice: %s(family: %s), data: %s(family: %s)", generation,
        voice_or_data, result, network_preference,
        ad.droid.telephonyGetCurrentVoiceNetworkTypeForSubscription(sub_id),
        rat_generation_from_rat(
            ad.droid.telephonyGetCurrentVoiceNetworkTypeForSubscription(
                sub_id)),
        ad.droid.telephonyGetCurrentDataNetworkTypeForSubscription(sub_id),
        rat_generation_from_rat(
            ad.droid.telephonyGetCurrentDataNetworkTypeForSubscription(
                sub_id)))
    if not result:
        get_telephony_signal_strength(ad)
    return result


def wait_for_network_rat(log,
                         ad,
                         rat_family,
                         max_wait_time=MAX_WAIT_TIME_NW_SELECTION,
                         voice_or_data=None):
    return wait_for_network_rat_for_subscription(
        log, ad, ad.droid.subscriptionGetDefaultSubId(), rat_family,
        max_wait_time, voice_or_data)


def wait_for_network_rat_for_subscription(
        log,
        ad,
        sub_id,
        rat_family,
        max_wait_time=MAX_WAIT_TIME_NW_SELECTION,
        voice_or_data=None):
    return _wait_for_droid_in_state_for_subscription(
        log, ad, sub_id, max_wait_time,
        is_droid_in_rat_family_for_subscription, rat_family, voice_or_data)


def wait_for_not_network_rat(log,
                             ad,
                             rat_family,
                             max_wait_time=MAX_WAIT_TIME_NW_SELECTION,
                             voice_or_data=None):
    return wait_for_not_network_rat_for_subscription(
        log, ad, ad.droid.subscriptionGetDefaultSubId(), rat_family,
        max_wait_time, voice_or_data)


def wait_for_not_network_rat_for_subscription(
        log,
        ad,
        sub_id,
        rat_family,
        max_wait_time=MAX_WAIT_TIME_NW_SELECTION,
        voice_or_data=None):
    return _wait_for_droid_in_state_for_subscription(
        log, ad, sub_id, max_wait_time,
        lambda log, ad, sub_id, *args, **kwargs: not is_droid_in_rat_family_for_subscription(log, ad, sub_id, rat_family, voice_or_data)
    )


def wait_for_preferred_network(log,
                               ad,
                               network_preference,
                               max_wait_time=MAX_WAIT_TIME_NW_SELECTION,
                               voice_or_data=None):
    return wait_for_preferred_network_for_subscription(
        log, ad, ad.droid.subscriptionGetDefaultSubId(), network_preference,
        max_wait_time, voice_or_data)


def wait_for_preferred_network_for_subscription(
        log,
        ad,
        sub_id,
        network_preference,
        max_wait_time=MAX_WAIT_TIME_NW_SELECTION,
        voice_or_data=None):
    rat_family_list = rat_families_for_network_preference(network_preference)
    return _wait_for_droid_in_state_for_subscription(
        log, ad, sub_id, max_wait_time,
        is_droid_in_rat_family_list_for_subscription, rat_family_list,
        voice_or_data)


def wait_for_network_generation(log,
                                ad,
                                generation,
                                max_wait_time=MAX_WAIT_TIME_NW_SELECTION,
                                voice_or_data=None):
    return wait_for_network_generation_for_subscription(
        log, ad, ad.droid.subscriptionGetDefaultSubId(), generation,
        max_wait_time, voice_or_data)


def wait_for_network_generation_for_subscription(
        log,
        ad,
        sub_id,
        generation,
        max_wait_time=MAX_WAIT_TIME_NW_SELECTION,
        voice_or_data=None):
    return _wait_for_droid_in_state_for_subscription(
        log, ad, sub_id, max_wait_time,
        is_droid_in_network_generation_for_subscription, generation,
        voice_or_data)


def is_droid_in_rat_family(log, ad, rat_family, voice_or_data=None):
    return is_droid_in_rat_family_for_subscription(
        log, ad, ad.droid.subscriptionGetDefaultSubId(), rat_family,
        voice_or_data)


def is_droid_in_rat_family_for_subscription(log,
                                            ad,
                                            sub_id,
                                            rat_family,
                                            voice_or_data=None):
    return is_droid_in_rat_family_list_for_subscription(
        log, ad, sub_id, [rat_family], voice_or_data)


def is_droid_in_rat_familiy_list(log, ad, rat_family_list, voice_or_data=None):
    return is_droid_in_rat_family_list_for_subscription(
        log, ad, ad.droid.subscriptionGetDefaultSubId(), rat_family_list,
        voice_or_data)


def is_droid_in_rat_family_list_for_subscription(log,
                                                 ad,
                                                 sub_id,
                                                 rat_family_list,
                                                 voice_or_data=None):
    service_list = [NETWORK_SERVICE_DATA, NETWORK_SERVICE_VOICE]
    if voice_or_data:
        service_list = [voice_or_data]

    for service in service_list:
        nw_rat = get_network_rat_for_subscription(log, ad, sub_id, service)
        if nw_rat == RAT_UNKNOWN or not is_valid_rat(nw_rat):
            continue
        if rat_family_from_rat(nw_rat) in rat_family_list:
            return True
    return False


def is_droid_in_network_generation(log, ad, nw_gen, voice_or_data):
    """Checks if a droid in expected network generation ("2g", "3g" or "4g").

    Args:
        log: log object.
        ad: android device.
        nw_gen: expected generation "4g", "3g", "2g".
        voice_or_data: check voice network generation or data network generation
            This parameter is optional. If voice_or_data is None, then if
            either voice or data in expected generation, function will return True.

    Returns:
        True if droid in expected network generation. Otherwise False.
    """
    return is_droid_in_network_generation_for_subscription(
        log, ad, ad.droid.subscriptionGetDefaultSubId(), nw_gen, voice_or_data)


def is_droid_in_network_generation_for_subscription(log, ad, sub_id, nw_gen,
                                                    voice_or_data):
    """Checks if a droid in expected network generation ("2g", "3g" or "4g").

    Args:
        log: log object.
        ad: android device.
        nw_gen: expected generation "4g", "3g", "2g".
        voice_or_data: check voice network generation or data network generation
            This parameter is optional. If voice_or_data is None, then if
            either voice or data in expected generation, function will return True.

    Returns:
        True if droid in expected network generation. Otherwise False.
    """
    service_list = [NETWORK_SERVICE_DATA, NETWORK_SERVICE_VOICE]

    if voice_or_data:
        service_list = [voice_or_data]

    for service in service_list:
        nw_rat = get_network_rat_for_subscription(log, ad, sub_id, service)
        ad.log.info("%s network rat is %s", service, nw_rat)
        if nw_rat == RAT_UNKNOWN or not is_valid_rat(nw_rat):
            continue

        if rat_generation_from_rat(nw_rat) == nw_gen:
            ad.log.info("%s network rat %s is expected %s", service, nw_rat,
                        nw_gen)
            return True
        else:
            ad.log.info("%s network rat %s is %s, does not meet expected %s",
                        service, nw_rat, rat_generation_from_rat(nw_rat),
                        nw_gen)
            return False

    return False


def get_network_rat(log, ad, voice_or_data):
    """Get current network type (Voice network type, or data network type)
       for default subscription id

    Args:
        ad: Android Device Object
        voice_or_data: Input parameter indicating to get voice network type or
            data network type.

    Returns:
        Current voice/data network type.
    """
    return get_network_rat_for_subscription(
        log, ad, ad.droid.subscriptionGetDefaultSubId(), voice_or_data)


def get_network_rat_for_subscription(log, ad, sub_id, voice_or_data):
    """Get current network type (Voice network type, or data network type)
       for specified subscription id

    Args:
        ad: Android Device Object
        sub_id: subscription ID
        voice_or_data: Input parameter indicating to get voice network type or
            data network type.

    Returns:
        Current voice/data network type.
    """
    if voice_or_data == NETWORK_SERVICE_VOICE:
        ret_val = ad.droid.telephonyGetCurrentVoiceNetworkTypeForSubscription(
            sub_id)
    elif voice_or_data == NETWORK_SERVICE_DATA:
        ret_val = ad.droid.telephonyGetCurrentDataNetworkTypeForSubscription(
            sub_id)
    else:
        ret_val = ad.droid.telephonyGetNetworkTypeForSubscription(sub_id)

    if ret_val is None:
        log.error("get_network_rat(): Unexpected null return value")
        return RAT_UNKNOWN
    else:
        return ret_val


def get_network_gen(log, ad, voice_or_data):
    """Get current network generation string (Voice network type, or data network type)

    Args:
        ad: Android Device Object
        voice_or_data: Input parameter indicating to get voice network generation
            or data network generation.

    Returns:
        Current voice/data network generation.
    """
    return get_network_gen_for_subscription(
        log, ad, ad.droid.subscriptionGetDefaultSubId(), voice_or_data)


def get_network_gen_for_subscription(log, ad, sub_id, voice_or_data):
    """Get current network generation string (Voice network type, or data network type)

    Args:
        ad: Android Device Object
        voice_or_data: Input parameter indicating to get voice network generation
            or data network generation.

    Returns:
        Current voice/data network generation.
    """
    try:
        return rat_generation_from_rat(
            get_network_rat_for_subscription(log, ad, sub_id, voice_or_data))
    except KeyError as e:
        ad.log.error("KeyError %s", e)
        return GEN_UNKNOWN


def check_voice_mail_count(log, ad, voice_mail_count_before,
                           voice_mail_count_after):
    """function to check if voice mail count is correct after leaving a new voice message.
    """
    return get_voice_mail_count_check_function(get_operator_name(log, ad))(
        voice_mail_count_before, voice_mail_count_after)


def get_voice_mail_number(log, ad):
    """function to get the voice mail number
    """
    voice_mail_number = get_voice_mail_check_number(get_operator_name(log, ad))
    if voice_mail_number is None:
        return get_phone_number(log, ad)
    return voice_mail_number


def ensure_phones_idle(log, ads, max_time=MAX_WAIT_TIME_CALL_DROP):
    """Ensure ads idle (not in call).
    """
    result = True
    for ad in ads:
        if not ensure_phone_idle(log, ad, max_time=max_time):
            result = False
    return result


def ensure_phone_idle(log, ad, max_time=MAX_WAIT_TIME_CALL_DROP, retry=2):
    """Ensure ad idle (not in call).
    """
    while ad.droid.telecomIsInCall() and retry > 0:
        ad.droid.telecomEndCall()
        time.sleep(3)
        retry -= 1
    if not wait_for_droid_not_in_call(log, ad, max_time=max_time):
        ad.log.error("Failed to end call")
        return False
    return True


def ensure_phone_subscription(log, ad):
    """Ensure Phone Subscription.
    """
    #check for sim and service
    duration = 0
    while duration < MAX_WAIT_TIME_NW_SELECTION:
        subInfo = ad.droid.subscriptionGetAllSubInfoList()
        if subInfo and len(subInfo) >= 1:
            ad.log.debug("Find valid subcription %s", subInfo)
            break
        else:
            ad.log.info("Did not find any subscription")
            time.sleep(5)
            duration += 5
    else:
        ad.log.error("Unable to find a valid subscription!")
        return False
    while duration < MAX_WAIT_TIME_NW_SELECTION:
        data_sub_id = ad.droid.subscriptionGetDefaultDataSubId()
        voice_sub_id = ad.droid.subscriptionGetDefaultVoiceSubId()
        if data_sub_id > INVALID_SUB_ID or voice_sub_id > INVALID_SUB_ID:
            ad.log.debug("Find valid voice or data sub id")
            break
        else:
            ad.log.info("Did not find valid data or voice sub id")
            time.sleep(5)
            duration += 5
    else:
        ad.log.error("Unable to find valid data or voice sub id")
        return False
    while duration < MAX_WAIT_TIME_NW_SELECTION:
        data_sub_id = ad.droid.subscriptionGetDefaultVoiceSubId()
        if data_sub_id > INVALID_SUB_ID:
            data_rat = get_network_rat_for_subscription(
                log, ad, data_sub_id, NETWORK_SERVICE_DATA)
        else:
            data_rat = RAT_UNKNOWN
        if voice_sub_id > INVALID_SUB_ID:
            voice_rat = get_network_rat_for_subscription(
                log, ad, voice_sub_id, NETWORK_SERVICE_VOICE)
        else:
            voice_rat = RAT_UNKNOWN
        if data_rat != RAT_UNKNOWN or voice_rat != RAT_UNKNOWN:
            ad.log.info("Data sub_id %s in %s, voice sub_id %s in %s",
                        data_sub_id, data_rat, voice_sub_id, voice_rat)
            return True
        else:
            ad.log.info("Did not attach for data or voice service")
            time.sleep(5)
            duration += 5
    else:
        ad.log.error("Did not attach for voice or data service")
        return False


def ensure_phone_default_state(log, ad, check_subscription=True, retry=2):
    """Ensure ad in default state.
    Phone not in call.
    Phone have no stored WiFi network and WiFi disconnected.
    Phone not in airplane mode.
    """
    result = True
    if not toggle_airplane_mode(log, ad, False, False):
        ad.log.error("Fail to turn off airplane mode")
        result = False
    try:
        set_wifi_to_default(log, ad)
        while ad.droid.telecomIsInCall() and retry > 0:
            ad.droid.telecomEndCall()
            time.sleep(3)
            retry -= 1
        if not wait_for_droid_not_in_call(log, ad):
            ad.log.error("Failed to end call")
        ad.droid.telephonyFactoryReset()
        data_roaming = getattr(ad, 'roaming', False)
        if get_cell_data_roaming_state_by_adb(ad) != data_roaming:
            set_cell_data_roaming_state_by_adb(ad, data_roaming)
        remove_mobile_data_usage_limit(ad)
        if not wait_for_not_network_rat(
                log, ad, RAT_FAMILY_WLAN, voice_or_data=NETWORK_SERVICE_DATA):
            ad.log.error("%s still in %s", NETWORK_SERVICE_DATA,
                         RAT_FAMILY_WLAN)
            result = False

        if check_subscription and not ensure_phone_subscription(log, ad):
            ad.log.error("Unable to find a valid subscription!")
            result = False
    except Exception as e:
        ad.log.error("%s failure, toggle APM instead", e)
        toggle_airplane_mode_by_adb(log, ad, True)
        toggle_airplane_mode_by_adb(log, ad, False)
        ad.send_keycode("ENDCALL")
        ad.adb.shell("settings put global wfc_ims_enabled 0")
        ad.adb.shell("settings put global mobile_data 1")

    return result


def ensure_phones_default_state(log, ads, check_subscription=True):
    """Ensure ads in default state.
    Phone not in call.
    Phone have no stored WiFi network and WiFi disconnected.
    Phone not in airplane mode.

    Returns:
        True if all steps of restoring default state succeed.
        False if any of the steps to restore default state fails.
    """
    tasks = []
    for ad in ads:
        tasks.append((ensure_phone_default_state, (log, ad,
                                                   check_subscription)))
    if not multithread_func(log, tasks):
        log.error("Ensure_phones_default_state Fail.")
        return False
    return True


def check_is_wifi_connected(log, ad, wifi_ssid):
    """Check if ad is connected to wifi wifi_ssid.

    Args:
        log: Log object.
        ad: Android device object.
        wifi_ssid: WiFi network SSID.

    Returns:
        True if wifi is connected to wifi_ssid
        False if wifi is not connected to wifi_ssid
    """
    wifi_info = ad.droid.wifiGetConnectionInfo()
    if wifi_info["supplicant_state"] == "completed" and wifi_info["SSID"] == wifi_ssid:
        ad.log.info("Wifi is connected to %s", wifi_ssid)
        ad.on_mobile_data = False
        return True
    else:
        ad.log.info("Wifi is not connected to %s", wifi_ssid)
        ad.log.debug("Wifi connection_info=%s", wifi_info)
        ad.on_mobile_data = True
        return False


def ensure_wifi_connected(log, ad, wifi_ssid, wifi_pwd=None, retries=3):
    """Ensure ad connected to wifi on network wifi_ssid.

    Args:
        log: Log object.
        ad: Android device object.
        wifi_ssid: WiFi network SSID.
        wifi_pwd: optional secure network password.
        retries: the number of retries.

    Returns:
        True if wifi is connected to wifi_ssid
        False if wifi is not connected to wifi_ssid
    """
    network = {WIFI_SSID_KEY: wifi_ssid}
    if wifi_pwd:
        network[WIFI_PWD_KEY] = wifi_pwd
    for i in range(retries):
        if not ad.droid.wifiCheckState():
            ad.log.info("Wifi state is down. Turn on Wifi")
            ad.droid.wifiToggleState(True)
        if check_is_wifi_connected(log, ad, wifi_ssid):
            ad.log.info("Wifi is connected to %s", wifi_ssid)
            return verify_internet_connection(log, ad, retries=3)
        else:
            ad.log.info("Connecting to wifi %s", wifi_ssid)
            try:
                ad.droid.wifiConnectByConfig(network)
            except Exception:
                ad.log.info("Connecting to wifi by wifiConnect instead")
                ad.droid.wifiConnect(network)
            time.sleep(20)
            if check_is_wifi_connected(log, ad, wifi_ssid):
                ad.log.info("Connected to Wifi %s", wifi_ssid)
                return verify_internet_connection(log, ad, retries=3)
    ad.log.info("Fail to connected to wifi %s", wifi_ssid)
    return False


def forget_all_wifi_networks(log, ad):
    """Forget all stored wifi network information

    Args:
        log: log object
        ad: AndroidDevice object

    Returns:
        boolean success (True) or failure (False)
    """
    if not ad.droid.wifiGetConfiguredNetworks():
        ad.on_mobile_data = True
        return True
    try:
        old_state = ad.droid.wifiCheckState()
        wifi_test_utils.reset_wifi(ad)
        wifi_toggle_state(log, ad, old_state)
    except Exception as e:
        log.error("forget_all_wifi_networks with exception: %s", e)
        return False
    ad.on_mobile_data = True
    return True


def wifi_reset(log, ad, disable_wifi=True):
    """Forget all stored wifi networks and (optionally) disable WiFi

    Args:
        log: log object
        ad: AndroidDevice object
        disable_wifi: boolean to disable wifi, defaults to True
    Returns:
        boolean success (True) or failure (False)
    """
    if not forget_all_wifi_networks(log, ad):
        ad.log.error("Unable to forget all networks")
        return False
    if not wifi_toggle_state(log, ad, not disable_wifi):
        ad.log.error("Failed to toggle WiFi state to %s!", not disable_wifi)
        return False
    return True


def set_wifi_to_default(log, ad):
    """Set wifi to default state (Wifi disabled and no configured network)

    Args:
        log: log object
        ad: AndroidDevice object

    Returns:
        boolean success (True) or failure (False)
    """
    ad.droid.wifiFactoryReset()
    ad.droid.wifiToggleState(False)
    ad.on_mobile_data = True


def wifi_toggle_state(log, ad, state, retries=3):
    """Toggle the WiFi State

    Args:
        log: log object
        ad: AndroidDevice object
        state: True, False, or None

    Returns:
        boolean success (True) or failure (False)
    """
    for i in range(retries):
        if wifi_test_utils.wifi_toggle_state(ad, state, assert_on_fail=False):
            ad.on_mobile_data = not state
            return True
        time.sleep(WAIT_TIME_BETWEEN_STATE_CHECK)
    return False


def start_wifi_tethering(log, ad, ssid, password, ap_band=None):
    """Start a Tethering Session

    Args:
        log: log object
        ad: AndroidDevice object
        ssid: the name of the WiFi network
        password: optional password, used for secure networks.
        ap_band=DEPRECATED specification of 2G or 5G tethering
    Returns:
        boolean success (True) or failure (False)
    """
    return wifi_test_utils._assert_on_fail_handler(
        wifi_test_utils.start_wifi_tethering,
        False,
        ad,
        ssid,
        password,
        band=ap_band)


def stop_wifi_tethering(log, ad):
    """Stop a Tethering Session

    Args:
        log: log object
        ad: AndroidDevice object
    Returns:
        boolean success (True) or failure (False)
    """
    return wifi_test_utils._assert_on_fail_handler(
        wifi_test_utils.stop_wifi_tethering, False, ad)


def reset_preferred_network_type_to_allowable_range(log, ad):
    """If preferred network type is not in allowable range, reset to GEN_4G
    preferred network type.

    Args:
        log: log object
        ad: android device object

    Returns:
        None
    """
    for sub_id, sub_info in ad.telephony["subscription"].items():
        current_preference = \
            ad.droid.telephonyGetPreferredNetworkTypesForSubscription(sub_id)
        ad.log.debug("sub_id network preference is %s", current_preference)
        try:
            if current_preference not in get_allowable_network_preference(
                    sub_info["operator"], sub_info["phone_type"]):
                network_preference = network_preference_for_generation(
                    GEN_4G, sub_info["operator"], sub_info["phone_type"])
                ad.droid.telephonySetPreferredNetworkTypesForSubscription(
                    network_preference, sub_id)
        except KeyError:
            pass


def task_wrapper(task):
    """Task wrapper for multithread_func

    Args:
        task[0]: function to be wrapped.
        task[1]: function args.

    Returns:
        Return value of wrapped function call.
    """
    func = task[0]
    params = task[1]
    return func(*params)


def run_multithread_func_async(log, task):
    """Starts a multi-threaded function asynchronously.

    Args:
        log: log object.
        task: a task to be executed in parallel.

    Returns:
        Future object representing the execution of the task.
    """
    executor = concurrent.futures.ThreadPoolExecutor(max_workers=1)
    try:
        future_object = executor.submit(task_wrapper, task)
    except Exception as e:
        log.error("Exception error %s", e)
        raise
    return future_object


def run_multithread_func(log, tasks):
    """Run multi-thread functions and return results.

    Args:
        log: log object.
        tasks: a list of tasks to be executed in parallel.

    Returns:
        results for tasks.
    """
    MAX_NUMBER_OF_WORKERS = 10
    number_of_workers = min(MAX_NUMBER_OF_WORKERS, len(tasks))
    executor = concurrent.futures.ThreadPoolExecutor(
        max_workers=number_of_workers)
    if not log: log = logging
    try:
        results = list(executor.map(task_wrapper, tasks))
    except Exception as e:
        log.error("Exception error %s", e)
        raise
    executor.shutdown()
    if log:
        log.info("multithread_func %s result: %s",
                 [task[0].__name__ for task in tasks], results)
    return results


def multithread_func(log, tasks):
    """Multi-thread function wrapper.

    Args:
        log: log object.
        tasks: tasks to be executed in parallel.

    Returns:
        True if all tasks return True.
        False if any task return False.
    """
    results = run_multithread_func(log, tasks)
    for r in results:
        if not r:
            return False
    return True


def multithread_func_and_check_results(log, tasks, expected_results):
    """Multi-thread function wrapper.

    Args:
        log: log object.
        tasks: tasks to be executed in parallel.
        expected_results: check if the results from tasks match expected_results.

    Returns:
        True if expected_results are met.
        False if expected_results are not met.
    """
    return_value = True
    results = run_multithread_func(log, tasks)
    log.info("multithread_func result: %s, expecting %s", results,
             expected_results)
    for task, result, expected_result in zip(tasks, results, expected_results):
        if result != expected_result:
            logging.info("Result for task %s is %s, expecting %s", task[0],
                         result, expected_result)
            return_value = False
    return return_value


def set_phone_screen_on(log, ad, screen_on_time=MAX_SCREEN_ON_TIME):
    """Set phone screen on time.

    Args:
        log: Log object.
        ad: Android device object.
        screen_on_time: screen on time.
            This is optional, default value is MAX_SCREEN_ON_TIME.
    Returns:
        True if set successfully.
    """
    ad.droid.setScreenTimeout(screen_on_time)
    return screen_on_time == ad.droid.getScreenTimeout()


def set_phone_silent_mode(log, ad, silent_mode=True):
    """Set phone silent mode.

    Args:
        log: Log object.
        ad: Android device object.
        silent_mode: set phone silent or not.
            This is optional, default value is True (silent mode on).
    Returns:
        True if set successfully.
    """
    ad.droid.toggleRingerSilentMode(silent_mode)
    ad.droid.setMediaVolume(0)
    ad.droid.setVoiceCallVolume(0)
    ad.droid.setAlarmVolume(0)
    ad.adb.ensure_root()
    ad.adb.shell("setprop ro.audio.silent 1", ignore_status=True)
    ad.adb.shell("cmd notification set_dnd on", ignore_status=True)
    return silent_mode == ad.droid.checkRingerSilentMode()


def set_preferred_network_mode_pref(log,
                                    ad,
                                    sub_id,
                                    network_preference,
                                    timeout=WAIT_TIME_ANDROID_STATE_SETTLING):
    """Set Preferred Network Mode for Sub_id
    Args:
        log: Log object.
        ad: Android device object.
        sub_id: Subscription ID.
        network_preference: Network Mode Type
    """
    begin_time = get_device_epoch_time(ad)
    if ad.droid.telephonyGetPreferredNetworkTypesForSubscription(
            sub_id) == network_preference:
        ad.log.info("Current ModePref for Sub %s is in %s", sub_id,
                    network_preference)
        return True
    ad.log.info("Setting ModePref to %s for Sub %s", network_preference,
                sub_id)
    while timeout >= 0:
        if ad.droid.telephonySetPreferredNetworkTypesForSubscription(
                network_preference, sub_id):
            return True
        time.sleep(WAIT_TIME_BETWEEN_STATE_CHECK)
        timeout = timeout - WAIT_TIME_BETWEEN_STATE_CHECK
    error_msg = "Failed to set sub_id %s PreferredNetworkType to %s" % (
        sub_id, network_preference)
    search_results = ad.search_logcat(
        "REQUEST_SET_PREFERRED_NETWORK_TYPE error", begin_time=begin_time)
    if search_results:
        log_message = search_results[-1]["log_message"]
        if "DEVICE_IN_USE" in log_message:
            error_msg = "%s due to DEVICE_IN_USE" % error_msg
        else:
            error_msg = "%s due to %s" % (error_msg, log_message)
    ad.log.error(error_msg)
    return False


def set_preferred_subid_for_sms(log, ad, sub_id):
    """set subscription id for SMS

    Args:
        log: Log object.
        ad: Android device object.
        sub_id :Subscription ID.

    """
    ad.log.info("Setting subscription %s as preferred SMS SIM", sub_id)
    ad.droid.subscriptionSetDefaultSmsSubId(sub_id)
    # Wait to make sure settings take effect
    time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)
    return sub_id == ad.droid.subscriptionGetDefaultSmsSubId()


def set_preferred_subid_for_data(log, ad, sub_id):
    """set subscription id for data

    Args:
        log: Log object.
        ad: Android device object.
        sub_id :Subscription ID.

    """
    ad.log.info("Setting subscription %s as preferred Data SIM", sub_id)
    ad.droid.subscriptionSetDefaultDataSubId(sub_id)
    time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)
    # Wait to make sure settings take effect
    # Data SIM change takes around 1 min
    # Check whether data has changed to selected sim
    if not wait_for_data_connection(log, ad, True,
                                    MAX_WAIT_TIME_DATA_SUB_CHANGE):
        log.error("Data Connection failed - Not able to switch Data SIM")
        return False
    return True


def set_preferred_subid_for_voice(log, ad, sub_id):
    """set subscription id for voice

    Args:
        log: Log object.
        ad: Android device object.
        sub_id :Subscription ID.

    """
    ad.log.info("Setting subscription %s as Voice SIM", sub_id)
    ad.droid.subscriptionSetDefaultVoiceSubId(sub_id)
    ad.droid.telecomSetUserSelectedOutgoingPhoneAccountBySubId(sub_id)
    # Wait to make sure settings take effect
    time.sleep(WAIT_TIME_ANDROID_STATE_SETTLING)
    return True


def set_call_state_listen_level(log, ad, value, sub_id):
    """Set call state listen level for subscription id.

    Args:
        log: Log object.
        ad: Android device object.
        value: True or False
        sub_id :Subscription ID.

    Returns:
        True or False
    """
    if sub_id == INVALID_SUB_ID:
        log.error("Invalid Subscription ID")
        return False
    ad.droid.telephonyAdjustPreciseCallStateListenLevelForSubscription(
        "Foreground", value, sub_id)
    ad.droid.telephonyAdjustPreciseCallStateListenLevelForSubscription(
        "Ringing", value, sub_id)
    ad.droid.telephonyAdjustPreciseCallStateListenLevelForSubscription(
        "Background", value, sub_id)
    return True


def setup_sim(log, ad, sub_id, voice=False, sms=False, data=False):
    """set subscription id for voice, sms and data

    Args:
        log: Log object.
        ad: Android device object.
        sub_id :Subscription ID.
        voice: True if to set subscription as default voice subscription
        sms: True if to set subscription as default sms subscription
        data: True if to set subscription as default data subscription

    """
    if sub_id == INVALID_SUB_ID:
        log.error("Invalid Subscription ID")
        return False
    else:
        if voice:
            if not set_preferred_subid_for_voice(log, ad, sub_id):
                return False
        if sms:
            if not set_preferred_subid_for_sms(log, ad, sub_id):
                return False
        if data:
            if not set_preferred_subid_for_data(log, ad, sub_id):
                return False
    return True


def is_event_match(event, field, value):
    """Return if <field> in "event" match <value> or not.

    Args:
        event: event to test. This event need to have <field>.
        field: field to match.
        value: value to match.

    Returns:
        True if <field> in "event" match <value>.
        False otherwise.
    """
    return is_event_match_for_list(event, field, [value])


def is_event_match_for_list(event, field, value_list):
    """Return if <field> in "event" match any one of the value
        in "value_list" or not.

    Args:
        event: event to test. This event need to have <field>.
        field: field to match.
        value_list: a list of value to match.

    Returns:
        True if <field> in "event" match one of the value in "value_list".
        False otherwise.
    """
    try:
        value_in_event = event['data'][field]
    except KeyError:
        return False
    for value in value_list:
        if value_in_event == value:
            return True
    return False


def is_network_call_back_event_match(event, network_callback_id,
                                     network_callback_event):
    try:
        return (
            (network_callback_id == event['data'][NetworkCallbackContainer.ID])
            and (network_callback_event == event['data']
                 [NetworkCallbackContainer.NETWORK_CALLBACK_EVENT]))
    except KeyError:
        return False


def is_build_id(log, ad, build_id):
    """Return if ad's build id is the same as input parameter build_id.

    Args:
        log: log object.
        ad: android device object.
        build_id: android build id.

    Returns:
        True if ad's build id is the same as input parameter build_id.
        False otherwise.
    """
    actual_bid = ad.droid.getBuildID()

    ad.log.info("BUILD DISPLAY: %s", ad.droid.getBuildDisplay())
    #In case we want to log more stuff/more granularity...
    #log.info("{} BUILD ID:{} ".format(ad.serial, ad.droid.getBuildID()))
    #log.info("{} BUILD FINGERPRINT: {} "
    # .format(ad.serial), ad.droid.getBuildFingerprint())
    #log.info("{} BUILD TYPE: {} "
    # .format(ad.serial), ad.droid.getBuildType())
    #log.info("{} BUILD NUMBER: {} "
    # .format(ad.serial), ad.droid.getBuildNumber())
    if actual_bid.upper() != build_id.upper():
        ad.log.error("%s: Incorrect Build ID", ad.model)
        return False
    return True


def is_uri_equivalent(uri1, uri2):
    """Check whether two input uris match or not.

    Compare Uris.
        If Uris are tel URI, it will only take the digit part
        and compare as phone number.
        Else, it will just do string compare.

    Args:
        uri1: 1st uri to be compared.
        uri2: 2nd uri to be compared.

    Returns:
        True if two uris match. Otherwise False.
    """

    #If either is None/empty we return false
    if not uri1 or not uri2:
        return False

    try:
        if uri1.startswith('tel:') and uri2.startswith('tel:'):
            uri1_number = get_number_from_tel_uri(uri1)
            uri2_number = get_number_from_tel_uri(uri2)
            return check_phone_number_match(uri1_number, uri2_number)
        else:
            return uri1 == uri2
    except AttributeError as e:
        return False


def get_call_uri(ad, call_id):
    """Get call's uri field.

    Get Uri for call_id in ad.

    Args:
        ad: android device object.
        call_id: the call id to get Uri from.

    Returns:
        call's Uri if call is active and have uri field. None otherwise.
    """
    try:
        call_detail = ad.droid.telecomCallGetDetails(call_id)
        return call_detail["Handle"]["Uri"]
    except:
        return None


def get_number_from_tel_uri(uri):
    """Get Uri number from tel uri

    Args:
        uri: input uri

    Returns:
        If input uri is tel uri, return the number part.
        else return None.
    """
    if uri.startswith('tel:'):
        uri_number = ''.join(
            i for i in urllib.parse.unquote(uri) if i.isdigit())
        return uri_number
    else:
        return None


def find_qxdm_log_mask(ad, mask="default.cfg"):
    """Find QXDM logger mask."""
    if "/" not in mask:
        # Call nexuslogger to generate log mask
        start_nexuslogger(ad)
        # Find the log mask path
        for path in (DEFAULT_QXDM_LOG_PATH, "/data/diag_logs",
                     "/vendor/etc/mdlog/"):
            out = ad.adb.shell(
                "find %s -type f -iname %s" % (path, mask), ignore_status=True)
            if out and "No such" not in out and "Permission denied" not in out:
                if path.startswith("/vendor/"):
                    setattr(ad, "qxdm_log_path", DEFAULT_QXDM_LOG_PATH)
                else:
                    setattr(ad, "qxdm_log_path", path)
                return out.split("\n")[0]
        if mask in ad.adb.shell("ls /vendor/etc/mdlog/"):
            setattr(ad, "qxdm_log_path", DEFAULT_QXDM_LOG_PATH)
            return "%s/%s" % ("/vendor/etc/mdlog/", mask)
    else:
        out = ad.adb.shell("ls %s" % mask, ignore_status=True)
        if out and "No such" not in out:
            qxdm_log_path, cfg_name = os.path.split(mask)
            setattr(ad, "qxdm_log_path", qxdm_log_path)
            return mask
    ad.log.warning("Could NOT find QXDM logger mask path for %s", mask)


def set_qxdm_logger_command(ad, mask=None):
    """Set QXDM logger always on.

    Args:
        ad: android device object.

    """
    ## Neet to check if log mask will be generated without starting nexus logger
    masks = []
    mask_path = None
    if mask:
        masks = [mask]
    masks.extend(["QC_Default.cfg", "default.cfg"])
    for mask in masks:
        mask_path = find_qxdm_log_mask(ad, mask)
        if mask_path: break
    if not mask_path:
        ad.log.error("Cannot find QXDM mask %s", mask)
        ad.qxdm_logger_command = None
        return False
    else:
        ad.log.info("Use QXDM log mask %s", mask_path)
        ad.log.debug("qxdm_log_path = %s", ad.qxdm_log_path)
        output_path = os.path.join(ad.qxdm_log_path, "logs")
        ad.qxdm_logger_command = ("diag_mdlog -f %s -o %s -s 90 -c" %
                                  (mask_path, output_path))
        for prop in ("persist.sys.modem.diag.mdlog",
                     "persist.vendor.sys.modem.diag.mdlog"):
            if ad.adb.getprop(prop):
                # Enable qxdm always on if supported
                for conf_path in ("/data/vendor/radio/diag_logs",
                                  "/vendor/etc/mdlog"):
                    if "diag.conf" in ad.adb.shell(
                            "ls %s" % conf_path, ignore_status=True):
                        conf_path = "%s/diag.conf" % conf_path
                        ad.adb.shell('echo "%s" > %s' %
                                     (ad.qxdm_logger_command, conf_path))
                        break
                ad.adb.shell("setprop %s true" % prop, ignore_status=True)
                break
        return True


def start_sdm_logger(ad):
    """Start SDM logger."""
    if not getattr(ad, "sdm_log", True): return
    # Delete existing SDM logs which were created 15 mins prior
    ad.sdm_log_path = DEFAULT_SDM_LOG_PATH
    file_count = ad.adb.shell(
        "find %s -type f -iname sbuff_[0-9]*.sdm* | wc -l" % ad.sdm_log_path)
    if int(file_count) > 3:
        seconds = 15 * 60
        # Remove sdm logs modified more than specified seconds ago
        ad.adb.shell(
            "find %s -type f -iname sbuff_[0-9]*.sdm* -not -mtime -%ss -delete" %
            (ad.sdm_log_path, seconds))
    # start logging
    cmd = "setprop vendor.sys.modem.logging.enable true"
    ad.log.debug("start sdm logging")
    ad.adb.shell(cmd, ignore_status=True)
    time.sleep(5)


def stop_sdm_logger(ad):
    """Stop SDM logger."""
    cmd = "setprop vendor.sys.modem.logging.enable false"
    ad.log.debug("stop sdm logging")
    ad.adb.shell(cmd, ignore_status=True)
    time.sleep(5)


def stop_qxdm_logger(ad):
    """Stop QXDM logger."""
    for cmd in ("diag_mdlog -k", "killall diag_mdlog"):
        output = ad.adb.shell("ps -ef | grep mdlog") or ""
        if "diag_mdlog" not in output:
            break
        ad.log.debug("Kill the existing qxdm process")
        ad.adb.shell(cmd, ignore_status=True)
        time.sleep(5)


def start_qxdm_logger(ad, begin_time=None):
    """Start QXDM logger."""
    if not getattr(ad, "qxdm_log", True): return
    # Delete existing QXDM logs 5 minutes earlier than the begin_time
    current_time = get_current_epoch_time()
    if getattr(ad, "qxdm_log_path", None):
        seconds = None
        file_count = ad.adb.shell(
            "find %s -type f -iname *.qmdl | wc -l" % ad.qxdm_log_path)
        if int(file_count) > 50:
            if begin_time:
                # if begin_time specified, delete old qxdm logs modified
                # 10 minutes before begin time
                seconds = int((current_time - begin_time) / 1000.0) + 10 * 60
            else:
                # if begin_time is not specified, delete old qxdm logs modified
                # 15 minutes before current time
                seconds = 15 * 60
        if seconds:
            # Remove qxdm logs modified more than specified seconds ago
            ad.adb.shell(
                "find %s -type f -iname *.qmdl -not -mtime -%ss -delete" %
                (ad.qxdm_log_path, seconds))
            ad.adb.shell(
                "find %s -type f -iname *.xml -not -mtime -%ss -delete" %
                (ad.qxdm_log_path, seconds))
    if getattr(ad, "qxdm_logger_command", None):
        output = ad.adb.shell("ps -ef | grep mdlog") or ""
        if ad.qxdm_logger_command not in output:
            ad.log.debug("QXDM logging command %s is not running",
                         ad.qxdm_logger_command)
            if "diag_mdlog" in output:
                # Kill the existing non-matching diag_mdlog process
                # Only one diag_mdlog process can be run
                stop_qxdm_logger(ad)
            ad.log.info("Start QXDM logger")
            ad.adb.shell_nb(ad.qxdm_logger_command)
            time.sleep(10)
        else:
            run_time = check_qxdm_logger_run_time(ad)
            if run_time < 600:
                # the last diag_mdlog started within 10 minutes ago
                # no need to restart
                return True
            if ad.search_logcat(
                    "Diag_Lib: diag: In delete_log",
                    begin_time=current_time -
                    run_time) or not ad.get_file_names(
                        ad.qxdm_log_path,
                        begin_time=current_time - 600000,
                        match_string="*.qmdl"):
                # diag_mdlog starts deleting files or no qmdl logs were
                # modified in the past 10 minutes
                ad.log.debug("Quit existing diag_mdlog and start a new one")
                stop_qxdm_logger(ad)
                ad.adb.shell_nb(ad.qxdm_logger_command)
                time.sleep(10)
        return True


def disable_qxdm_logger(ad):
    for prop in ("persist.sys.modem.diag.mdlog",
                 "persist.vendor.sys.modem.diag.mdlog",
                 "vendor.sys.modem.diag.mdlog_on"):
        if ad.adb.getprop(prop):
            ad.adb.shell("setprop %s false" % prop, ignore_status=True)
    for apk in ("com.android.nexuslogger", "com.android.pixellogger"):
        if ad.is_apk_installed(apk) and ad.is_apk_running(apk):
            ad.force_stop_apk(apk)
    stop_qxdm_logger(ad)
    return True


def check_qxdm_logger_run_time(ad):
    output = ad.adb.shell("ps -eo etime,cmd | grep diag_mdlog")
    result = re.search(r"(\d+):(\d+):(\d+) diag_mdlog", output)
    if result:
        return int(result.group(1)) * 60 * 60 + int(
            result.group(2)) * 60 + int(result.group(3))
    else:
        result = re.search(r"(\d+):(\d+) diag_mdlog", output)
        if result:
            return int(result.group(1)) * 60 + int(result.group(2))
        else:
            return 0


def start_qxdm_loggers(log, ads, begin_time=None):
    tasks = [(start_qxdm_logger, [ad, begin_time]) for ad in ads
             if getattr(ad, "qxdm_log", True)]
    if tasks: run_multithread_func(log, tasks)


def stop_qxdm_loggers(log, ads):
    tasks = [(stop_qxdm_logger, [ad]) for ad in ads]
    run_multithread_func(log, tasks)


def start_sdm_loggers(log, ads):
    tasks = [(start_sdm_logger, [ad]) for ad in ads
             if getattr(ad, "sdm_log", True)]
    if tasks: run_multithread_func(log, tasks)


def stop_sdm_loggers(log, ads):
    tasks = [(stop_sdm_logger, [ad]) for ad in ads]
    run_multithread_func(log, tasks)


def start_nexuslogger(ad):
    """Start Nexus/Pixel Logger Apk."""
    qxdm_logger_apk = None
    for apk, activity in (("com.android.nexuslogger", ".MainActivity"),
                          ("com.android.pixellogger",
                           ".ui.main.MainActivity")):
        if ad.is_apk_installed(apk):
            qxdm_logger_apk = apk
            break
    if not qxdm_logger_apk: return
    if ad.is_apk_running(qxdm_logger_apk):
        if "granted=true" in ad.adb.shell(
                "dumpsys package %s | grep WRITE_EXTERN" % qxdm_logger_apk):
            return True
        else:
            ad.log.info("Kill %s" % qxdm_logger_apk)
            ad.force_stop_apk(qxdm_logger_apk)
            time.sleep(5)
    for perm in ("READ", "WRITE"):
        ad.adb.shell("pm grant %s android.permission.%s_EXTERNAL_STORAGE" %
                     (qxdm_logger_apk, perm))
    time.sleep(2)
    for i in range(3):
        ad.unlock_screen()
        ad.log.info("Start %s Attempt %d" % (qxdm_logger_apk, i + 1))
        ad.adb.shell("am start -n %s/%s" % (qxdm_logger_apk, activity))
        time.sleep(5)
        if ad.is_apk_running(qxdm_logger_apk):
            ad.send_keycode("HOME")
            return True
    return False


def check_qxdm_logger_mask(ad, mask_file="QC_Default.cfg"):
    """Check if QXDM logger always on is set.

    Args:
        ad: android device object.

    """
    output = ad.adb.shell(
        "ls /data/vendor/radio/diag_logs/", ignore_status=True)
    if not output or "No such" in output:
        return True
    if mask_file not in ad.adb.shell(
            "cat /data/vendor/radio/diag_logs/diag.conf", ignore_status=True):
        return False
    return True


def start_tcpdumps(ads,
                   test_name="",
                   begin_time=None,
                   interface="any",
                   mask="all"):
    for ad in ads:
        try:
            start_adb_tcpdump(
                ad,
                test_name=test_name,
                begin_time=begin_time,
                interface=interface,
                mask=mask)
        except Exception as e:
            ad.log.warning("Fail to start tcpdump due to %s", e)


def start_adb_tcpdump(ad,
                      test_name="",
                      begin_time=None,
                      interface="any",
                      mask="all"):
    """Start tcpdump on any iface

    Args:
        ad: android device object.
        test_name: tcpdump file name will have this

    """
    out = ad.adb.shell("ls -l /data/local/tmp/tcpdump/")
    if "No such file" in out or not out:
        ad.adb.shell("mkdir /data/local/tmp/tcpdump")
    else:
        ad.adb.shell(
            "find /data/local/tmp/tcpdump -type f -not -mtime -1800s -delete")
        ad.adb.shell(
            "find /data/local/tmp/tcpdump -type f -size +5G -delete")

    if not begin_time:
        begin_time = get_current_epoch_time()

    out = ad.adb.shell(
        'ifconfig | grep -v -E "r_|-rmnet" | grep -E "lan|data"',
        ignore_status=True,
        timeout=180)
    intfs = re.findall(r"(\S+).*", out)
    if interface and interface not in ("any", "all"):
        if interface not in intfs: return
        intfs = [interface]

    out = ad.adb.shell("ps -ef | grep tcpdump")
    cmds = []
    for intf in intfs:
        if intf in out:
            ad.log.info("tcpdump on interface %s is already running", intf)
            continue
        else:
            log_file_name = "/data/local/tmp/tcpdump/tcpdump_%s_%s_%s_%s.pcap" \
                            % (ad.serial, intf, test_name, begin_time)
            if mask == "ims":
                cmds.append(
                    "adb -s %s shell tcpdump -i %s -s0 -n -p udp port 500 or "
                    "udp port 4500 -w %s" % (ad.serial, intf, log_file_name))
            else:
                cmds.append("adb -s %s shell tcpdump -i %s -s0 -w %s" %
                            (ad.serial, intf, log_file_name))
    for cmd in cmds:
        ad.log.info(cmd)
        try:
            start_standing_subprocess(cmd, 10)
        except Exception as e:
            ad.log.error(e)
    if cmds:
        time.sleep(5)


def stop_tcpdumps(ads):
    for ad in ads:
        stop_adb_tcpdump(ad)


def stop_adb_tcpdump(ad, interface="any"):
    """Stops tcpdump on any iface
       Pulls the tcpdump file in the tcpdump dir

    Args:
        ad: android device object.

    """
    if interface == "any":
        try:
            ad.adb.shell("killall -9 tcpdump")
        except Exception as e:
            ad.log.error("Killing tcpdump with exception %s", e)
    else:
        out = ad.adb.shell("ps -ef | grep tcpdump | grep %s" % interface)
        if "tcpdump -i" in out:
            pids = re.findall(r"\S+\s+(\d+).*tcpdump -i", out)
            for pid in pids:
                ad.adb.shell("kill -9 %s" % pid)
    ad.adb.shell(
      "find /data/local/tmp/tcpdump -type f -not -mtime -1800s -delete")


def get_tcpdump_log(ad, test_name="", begin_time=None):
    """Stops tcpdump on any iface
       Pulls the tcpdump file in the tcpdump dir
       Zips all tcpdump files

    Args:
        ad: android device object.
        test_name: test case name
        begin_time: test begin time
    """
    logs = ad.get_file_names("/data/local/tmp/tcpdump", begin_time=begin_time)
    if logs:
        ad.log.info("Pulling tcpdumps %s", logs)
        log_path = os.path.join(
            ad.device_log_path, "TCPDUMP_%s_%s" % (ad.model, ad.serial))
        os.makedirs(log_path, exist_ok=True)
        ad.pull_files(logs, log_path)
        shutil.make_archive(log_path, "zip", log_path)
        shutil.rmtree(log_path)
    return True


def fastboot_wipe(ad, skip_setup_wizard=True):
    """Wipe the device in fastboot mode.

    Pull sl4a apk from device. Terminate all sl4a sessions,
    Reboot the device to bootloader, wipe the device by fastboot.
    Reboot the device. wait for device to complete booting
    Re-intall and start an sl4a session.
    """
    status = True
    # Pull sl4a apk from device
    out = ad.adb.shell("pm path %s" % SL4A_APK_NAME)
    result = re.search(r"package:(.*)", out)
    if not result:
        ad.log.error("Couldn't find sl4a apk")
    else:
        sl4a_apk = result.group(1)
        ad.log.info("Get sl4a apk from %s", sl4a_apk)
        ad.pull_files([sl4a_apk], "/tmp/")
    ad.stop_services()
    attemps = 3
    for i in range(1, attemps + 1):
        try:
            if ad.serial in list_adb_devices():
                ad.log.info("Reboot to bootloader")
                ad.adb.reboot("bootloader", ignore_status=True)
                time.sleep(10)
            if ad.serial in list_fastboot_devices():
                ad.log.info("Wipe in fastboot")
                ad.fastboot._w(timeout=300, ignore_status=True)
                time.sleep(30)
                ad.log.info("Reboot in fastboot")
                ad.fastboot.reboot()
            ad.wait_for_boot_completion()
            ad.root_adb()
            if ad.skip_sl4a:
                break
            if ad.is_sl4a_installed():
                break
            ad.log.info("Re-install sl4a")
            ad.adb.shell("settings put global verifier_verify_adb_installs 0")
            ad.adb.install("-r /tmp/base.apk")
            time.sleep(10)
            break
        except Exception as e:
            ad.log.warning(e)
            if i == attemps:
                abort_all_tests(log, str(e))
            time.sleep(5)
    try:
        ad.start_adb_logcat()
    except:
        ad.log.error("Failed to start adb logcat!")
    if skip_setup_wizard:
        ad.exit_setup_wizard()
    if getattr(ad, "qxdm_log", True):
        set_qxdm_logger_command(ad, mask=getattr(ad, "qxdm_log_mask", None))
        start_qxdm_logger(ad)
    if ad.skip_sl4a: return status
    bring_up_sl4a(ad)
    synchronize_device_time(ad)
    set_phone_silent_mode(ad.log, ad)
    # Activate WFC on Verizon, AT&T and Canada operators as per # b/33187374 &
    # b/122327716
    activate_wfc_on_device(ad.log, ad)
    return status

def install_carriersettings_apk(ad, carriersettingsapk, skip_setup_wizard=True):
    """ Carrier Setting Installation Steps

    Pull sl4a apk from device. Terminate all sl4a sessions,
    Reboot the device to bootloader, wipe the device by fastboot.
    Reboot the device. wait for device to complete booting
    """
    status = True
    if carriersettingsapk is None:
        ad.log.warning("CarrierSettingsApk is not provided, aborting")
        return False
    ad.log.info("Push carriersettings apk to the Android device.")
    android_apk_path = "/product/priv-app/CarrierSettings/CarrierSettings.apk"
    ad.adb.push("%s %s" % (carriersettingsapk, android_apk_path))
    ad.stop_services()

    attempts = 3
    for i in range(1, attempts + 1):
        try:
            if ad.serial in list_adb_devices():
                ad.log.info("Reboot to bootloader")
                ad.adb.reboot("bootloader", ignore_status=True)
                time.sleep(30)
            if ad.serial in list_fastboot_devices():
                ad.log.info("Reboot in fastboot")
                ad.fastboot.reboot()
            ad.wait_for_boot_completion()
            ad.root_adb()
            if ad.is_sl4a_installed():
                break
            time.sleep(10)
            break
        except Exception as e:
            ad.log.warning(e)
            if i == attempts:
                abort_all_tests(log, str(e))
            time.sleep(5)
    try:
        ad.start_adb_logcat()
    except:
        ad.log.error("Failed to start adb logcat!")
    if skip_setup_wizard:
        ad.exit_setup_wizard()
    return status


def bring_up_sl4a(ad, attemps=3):
    for i in range(attemps):
        try:
            droid, ed = ad.get_droid()
            ed.start()
            ad.log.info("Brought up new sl4a session")
            break
        except Exception as e:
            if i < attemps - 1:
                ad.log.info(e)
                time.sleep(10)
            else:
                ad.log.error(e)
                raise


def reboot_device(ad, recover_sim_state=True):
    sim_state = is_sim_ready(ad.log, ad)
    ad.reboot()
    if ad.qxdm_log:
        start_qxdm_logger(ad)
    ad.unlock_screen()
    if recover_sim_state:
        if not unlock_sim(ad):
            ad.log.error("Unable to unlock SIM")
            return False
        if sim_state and not _wait_for_droid_in_state(
                log, ad, MAX_WAIT_TIME_FOR_STATE_CHANGE, is_sim_ready):
            ad.log.error("Sim state didn't reach pre-reboot ready state")
            return False
    return True


def unlocking_device(ad, device_password=None):
    """First unlock device attempt, required after reboot"""
    ad.unlock_screen(device_password)
    time.sleep(2)
    ad.adb.wait_for_device(timeout=180)
    if not ad.is_waiting_for_unlock_pin():
        return True
    else:
        ad.unlock_screen(device_password)
        time.sleep(2)
        ad.adb.wait_for_device(timeout=180)
        if ad.wait_for_window_ready():
            return True
    ad.log.error("Unable to unlock to user window")
    return False


def refresh_sl4a_session(ad):
    try:
        ad.droid.logI("Checking SL4A connection")
        ad.log.debug("Existing sl4a session is active")
        return True
    except Exception as e:
        ad.log.warning("Existing sl4a session is NOT active: %s", e)
    try:
        ad.terminate_all_sessions()
    except Exception as e:
        ad.log.info("terminate_all_sessions with error %s", e)
    ad.ensure_screen_on()
    ad.log.info("Open new sl4a connection")
    bring_up_sl4a(ad)


def reset_device_password(ad, device_password=None):
    # Enable or Disable Device Password per test bed config
    unlock_sim(ad)
    screen_lock = ad.is_screen_lock_enabled()
    if device_password:
        try:
            refresh_sl4a_session(ad)
            ad.droid.setDevicePassword(device_password)
        except Exception as e:
            ad.log.warning("setDevicePassword failed with %s", e)
            try:
                ad.droid.setDevicePassword(device_password, "1111")
            except Exception as e:
                ad.log.warning(
                    "setDevicePassword providing previous password error: %s",
                    e)
        time.sleep(2)
        if screen_lock:
            # existing password changed
            return
        else:
            # enable device password and log in for the first time
            ad.log.info("Enable device password")
            ad.adb.wait_for_device(timeout=180)
    else:
        if not screen_lock:
            # no existing password, do not set password
            return
        else:
            # password is enabled on the device
            # need to disable the password and log in on the first time
            # with unlocking with a swipe
            ad.log.info("Disable device password")
            ad.unlock_screen(password="1111")
            refresh_sl4a_session(ad)
            ad.ensure_screen_on()
            try:
                ad.droid.disableDevicePassword()
            except Exception as e:
                ad.log.warning("disableDevicePassword failed with %s", e)
                fastboot_wipe(ad)
            time.sleep(2)
            ad.adb.wait_for_device(timeout=180)
    refresh_sl4a_session(ad)
    if not ad.is_adb_logcat_on:
        ad.start_adb_logcat()


def get_sim_state(ad):
    try:
        state = ad.droid.telephonyGetSimState()
    except Exception as e:
        ad.log.error(e)
        state = ad.adb.getprop("gsm.sim.state")
    return state


def is_sim_locked(ad):
    return get_sim_state(ad) == SIM_STATE_PIN_REQUIRED


def is_sim_lock_enabled(ad):
    # TODO: add sl4a fascade to check if sim is locked
    return getattr(ad, "is_sim_locked", False)


def unlock_sim(ad):
    #The puk and pin can be provided in testbed config file.
    #"AndroidDevice": [{"serial": "84B5T15A29018214",
    #                   "adb_logcat_param": "-b all",
    #                   "puk": "12345678",
    #                   "puk_pin": "1234"}]
    if not is_sim_locked(ad):
        return True
    else:
        ad.is_sim_locked = True
    puk_pin = getattr(ad, "puk_pin", "1111")
    try:
        if not hasattr(ad, 'puk'):
            ad.log.info("Enter SIM pin code")
            ad.droid.telephonySupplyPin(puk_pin)
        else:
            ad.log.info("Enter PUK code and pin")
            ad.droid.telephonySupplyPuk(ad.puk, puk_pin)
    except:
        # if sl4a is not available, use adb command
        ad.unlock_screen(puk_pin)
        if is_sim_locked(ad):
            ad.unlock_screen(puk_pin)
    time.sleep(30)
    return not is_sim_locked(ad)


def send_dialer_secret_code(ad, secret_code):
    """Send dialer secret code.

    ad: android device controller
    secret_code: the secret code to be sent to dialer. the string between
                 code prefix *#*# and code postfix #*#*. *#*#<xxx>#*#*
    """
    action = 'android.provider.Telephony.SECRET_CODE'
    uri = 'android_secret_code://%s' % secret_code
    intent = ad.droid.makeIntent(
        action,
        uri,
        None,  # type
        None,  # extras
        None,  # categories,
        None,  # packagename,
        None,  # classname,
        0x01000000)  # flags
    ad.log.info('Issuing dialer secret dialer code: %s', secret_code)
    ad.droid.sendBroadcastIntent(intent)


def enable_radio_log_on(ad):
    if ad.adb.getprop("persist.vendor.radio.adb_log_on") != "1":
        ad.log.info("Enable radio adb_log_on and reboot")
        adb_disable_verity(ad)
        ad.adb.shell("setprop persist.vendor.radio.adb_log_on 1")
        reboot_device(ad)


def adb_disable_verity(ad):
    if ad.adb.getprop("ro.boot.veritymode") == "enforcing":
        ad.adb.disable_verity()
        reboot_device(ad)
        ad.adb.remount()


def recover_build_id(ad):
    build_fingerprint = ad.adb.getprop(
        "ro.vendor.build.fingerprint") or ad.adb.getprop(
            "ro.build.fingerprint")
    if not build_fingerprint:
        return
    build_id = build_fingerprint.split("/")[3]
    if ad.adb.getprop("ro.build.id") != build_id:
        build_id_override(ad, build_id)


def build_id_override(ad, new_build_id=None, postfix=None):
    build_fingerprint = ad.adb.getprop(
        "ro.build.fingerprint") or ad.adb.getprop(
            "ro.vendor.build.fingerprint")
    if build_fingerprint:
        build_id = build_fingerprint.split("/")[3]
    else:
        build_id = None
    existing_build_id = ad.adb.getprop("ro.build.id")
    if postfix is not None and postfix in build_id:
        ad.log.info("Build id already contains %s", postfix)
        return
    if not new_build_id:
        if postfix and build_id:
            new_build_id = "%s.%s" % (build_id, postfix)
    if not new_build_id or existing_build_id == new_build_id:
        return
    ad.log.info("Override build id %s with %s", existing_build_id,
                new_build_id)
    adb_disable_verity(ad)
    ad.adb.remount()
    if "backup.prop" not in ad.adb.shell("ls /sdcard/"):
        ad.adb.shell("cp /system/build.prop /sdcard/backup.prop")
    ad.adb.shell("cat /system/build.prop | grep -v ro.build.id > /sdcard/test.prop")
    ad.adb.shell("echo ro.build.id=%s >> /sdcard/test.prop" % new_build_id)
    ad.adb.shell("cp /sdcard/test.prop /system/build.prop")
    reboot_device(ad)
    ad.log.info("ro.build.id = %s", ad.adb.getprop("ro.build.id"))


def enable_connectivity_metrics(ad):
    cmds = [
        "pm enable com.android.connectivity.metrics",
        "am startservice -a com.google.android.gms.usagereporting.OPTIN_UR",
        "am broadcast -a com.google.gservices.intent.action.GSERVICES_OVERRIDE"
        " -e usagestats:connectivity_metrics:enable_data_collection 1",
        "am broadcast -a com.google.gservices.intent.action.GSERVICES_OVERRIDE"
        " -e usagestats:connectivity_metrics:telephony_snapshot_period_millis 180000"
        # By default it turn on all modules
        #"am broadcast -a com.google.gservices.intent.action.GSERVICES_OVERRIDE"
        #" -e usagestats:connectivity_metrics:data_collection_bitmap 62"
    ]
    for cmd in cmds:
        ad.adb.shell(cmd, ignore_status=True)


def force_connectivity_metrics_upload(ad):
    cmd = "cmd jobscheduler run --force com.android.connectivity.metrics %s"
    for job_id in [2, 3, 5, 4, 1, 6]:
        ad.adb.shell(cmd % job_id, ignore_status=True)


def system_file_push(ad, src_file_path, dst_file_path):
    """Push system file on a device.

    Push system file need to change some system setting and remount.
    """
    cmd = "%s %s" % (src_file_path, dst_file_path)
    out = ad.adb.push(cmd, timeout=300, ignore_status=True)
    skip_sl4a = True if "sl4a.apk" in src_file_path else False
    if "Read-only file system" in out:
        ad.log.info("Change read-only file system")
        adb_disable_verity(ad)
        out = ad.adb.push(cmd, timeout=300, ignore_status=True)
        if "Read-only file system" in out:
            ad.reboot(skip_sl4a)
            out = ad.adb.push(cmd, timeout=300, ignore_status=True)
            if "error" in out:
                ad.log.error("%s failed with %s", cmd, out)
                return False
            else:
                ad.log.info("push %s succeed")
                if skip_sl4a: ad.reboot(skip_sl4a)
                return True
        else:
            return True
    elif "error" in out:
        return False
    else:
        return True


def flash_radio(ad, file_path, skip_setup_wizard=True):
    """Flash radio image."""
    ad.stop_services()
    ad.log.info("Reboot to bootloader")
    ad.adb.reboot_bootloader(ignore_status=True)
    ad.log.info("Flash radio in fastboot")
    try:
        ad.fastboot.flash("radio %s" % file_path, timeout=300)
    except Exception as e:
        ad.log.error(e)
    ad.fastboot.reboot("bootloader")
    time.sleep(5)
    output = ad.fastboot.getvar("version-baseband")
    result = re.search(r"version-baseband: (\S+)", output)
    if not result:
        ad.log.error("fastboot getvar version-baseband output = %s", output)
        abort_all_tests(ad.log, "Radio version-baseband is not provided")
    fastboot_radio_version_output = result.group(1)
    for _ in range(2):
        try:
            ad.log.info("Reboot in fastboot")
            ad.fastboot.reboot()
            ad.wait_for_boot_completion()
            break
        except Exception as e:
            ad.log.error("Exception error %s", e)
    ad.root_adb()
    adb_radio_version_output = ad.adb.getprop("gsm.version.baseband")
    ad.log.info("adb getprop gsm.version.baseband = %s",
                adb_radio_version_output)
    if adb_radio_version_output != fastboot_radio_version_output:
        msg = ("fastboot radio version output %s does not match with adb"
               " radio version output %s" % (fastboot_radio_version_output,
                                             adb_radio_version_output))
        abort_all_tests(ad.log, msg)
    if not ad.ensure_screen_on():
        ad.log.error("User window cannot come up")
    ad.start_services(skip_setup_wizard=skip_setup_wizard)
    unlock_sim(ad)


def set_preferred_apn_by_adb(ad, pref_apn_name):
    """Select Pref APN
       Set Preferred APN on UI using content query/insert
       It needs apn name as arg, and it will match with plmn id
    """
    try:
        plmn_id = get_plmn_by_adb(ad)
        out = ad.adb.shell("content query --uri content://telephony/carriers "
                           "--where \"apn='%s' and numeric='%s'\"" %
                           (pref_apn_name, plmn_id))
        if "No result found" in out:
            ad.log.warning("Cannot find APN %s on device", pref_apn_name)
            return False
        else:
            apn_id = re.search(r'_id=(\d+)', out).group(1)
            ad.log.info("APN ID is %s", apn_id)
            ad.adb.shell("content insert --uri content:"
                         "//telephony/carriers/preferapn --bind apn_id:i:%s" %
                         (apn_id))
            out = ad.adb.shell("content query --uri "
                               "content://telephony/carriers/preferapn")
            if "No result found" in out:
                ad.log.error("Failed to set prefer APN %s", pref_apn_name)
                return False
            elif apn_id == re.search(r'_id=(\d+)', out).group(1):
                ad.log.info("Preferred APN set to %s", pref_apn_name)
                return True
    except Exception as e:
        ad.log.error("Exception while setting pref apn %s", e)
        return True


def check_apm_mode_on_by_serial(ad, serial_id):
    try:
        apm_check_cmd = "|".join(("adb -s %s shell dumpsys wifi" % serial_id,
                                  "grep -i airplanemodeon", "cut -f2 -d ' '"))
        output = exe_cmd(apm_check_cmd)
        if output.decode("utf-8").split("\n")[0] == "true":
            return True
        else:
            return False
    except Exception as e:
        ad.log.warning("Exception during check apm mode on %s", e)
        return True


def set_apm_mode_on_by_serial(ad, serial_id):
    try:
        cmd1 = "adb -s %s shell settings put global airplane_mode_on 1" % serial_id
        cmd2 = "adb -s %s shell am broadcast -a android.intent.action.AIRPLANE_MODE" % serial_id
        exe_cmd(cmd1)
        exe_cmd(cmd2)
    except Exception as e:
        ad.log.warning("Exception during set apm mode on %s", e)
        return True


def print_radio_info(ad, extra_msg=""):
    for prop in ("gsm.version.baseband", "persist.radio.ver_info",
                 "persist.radio.cnv.ver_info"):
        output = ad.adb.getprop(prop)
        ad.log.info("%s%s = %s", extra_msg, prop, output)


def wait_for_state(state_check_func,
                   state,
                   max_wait_time=MAX_WAIT_TIME_FOR_STATE_CHANGE,
                   checking_interval=WAIT_TIME_BETWEEN_STATE_CHECK,
                   *args,
                   **kwargs):
    while max_wait_time >= 0:
        if state_check_func(*args, **kwargs) == state:
            return True
        time.sleep(checking_interval)
        max_wait_time -= checking_interval
    return False


def power_off_sim(ad, sim_slot_id=None,
                  timeout=MAX_WAIT_TIME_FOR_STATE_CHANGE):
    try:
        if sim_slot_id is None:
            ad.droid.telephonySetSimPowerState(CARD_POWER_DOWN)
            verify_func = ad.droid.telephonyGetSimState
            verify_args = []
        else:
            ad.droid.telephonySetSimStateForSlotId(sim_slot_id,
                                                   CARD_POWER_DOWN)
            verify_func = ad.droid.telephonyGetSimStateForSlotId
            verify_args = [sim_slot_id]
    except Exception as e:
        ad.log.error(e)
        return False
    while timeout > 0:
        sim_state = verify_func(*verify_args)
        if sim_state in (SIM_STATE_UNKNOWN, SIM_STATE_ABSENT):
            ad.log.info("SIM slot is powered off, SIM state is %s", sim_state)
            return True
        timeout = timeout - WAIT_TIME_BETWEEN_STATE_CHECK
        time.sleep(WAIT_TIME_BETWEEN_STATE_CHECK)
    ad.log.warning("Fail to power off SIM slot, sim_state=%s",
                   verify_func(*verify_args))
    return False


def power_on_sim(ad, sim_slot_id=None):
    try:
        if sim_slot_id is None:
            ad.droid.telephonySetSimPowerState(CARD_POWER_UP)
            verify_func = ad.droid.telephonyGetSimState
            verify_args = []
        else:
            ad.droid.telephonySetSimStateForSlotId(sim_slot_id, CARD_POWER_UP)
            verify_func = ad.droid.telephonyGetSimStateForSlotId
            verify_args = [sim_slot_id]
    except Exception as e:
        ad.log.error(e)
        return False
    if wait_for_state(verify_func, SIM_STATE_READY,
                      MAX_WAIT_TIME_FOR_STATE_CHANGE,
                      WAIT_TIME_BETWEEN_STATE_CHECK, *verify_args):
        ad.log.info("SIM slot is powered on, SIM state is READY")
        return True
    elif verify_func(*verify_args) == SIM_STATE_PIN_REQUIRED:
        ad.log.info("SIM is pin locked")
        return True
    else:
        ad.log.error("Fail to power on SIM slot")
        return False


def extract_test_log(log, src_file, dst_file, test_tag):
    os.makedirs(os.path.dirname(dst_file), exist_ok=True)
    cmd = "grep -n '%s' %s" % (test_tag, src_file)
    result = job.run(cmd, ignore_status=True)
    if not result.stdout or result.exit_status == 1:
        log.warning("Command %s returns %s", cmd, result)
        return
    line_nums = re.findall(r"(\d+).*", result.stdout)
    if line_nums:
        begin_line = int(line_nums[0])
        end_line = int(line_nums[-1])
        if end_line - begin_line <= 5:
            result = job.run("wc -l < %s" % src_file)
            if result.stdout:
                end_line = int(result.stdout)
        log.info("Extract %s from line %s to line %s to %s", src_file,
                 begin_line, end_line, dst_file)
        job.run("awk 'NR >= %s && NR <= %s' %s > %s" % (begin_line, end_line,
                                                        src_file, dst_file))


def get_device_epoch_time(ad):
    return int(1000 * float(ad.adb.shell("date +%s.%N")))


def synchronize_device_time(ad):
    ad.adb.shell("put global auto_time 0", ignore_status=True)
    try:
        ad.adb.droid.setTime(get_current_epoch_time())
    except Exception:
        try:
            ad.adb.shell("date `date +%m%d%H%M%G.%S`")
        except Exception:
            pass
    try:
        ad.adb.shell(
            "am broadcast -a android.intent.action.TIME_SET",
            ignore_status=True)
    except Exception:
        pass


def revert_default_telephony_setting(ad):
    toggle_airplane_mode_by_adb(ad.log, ad, True)
    default_data_roaming = int(
        ad.adb.getprop("ro.com.android.dataroaming") == 'true')
    default_network_preference = int(
        ad.adb.getprop("ro.telephony.default_network"))
    ad.log.info("Default data roaming %s, network preference %s",
                default_data_roaming, default_network_preference)
    new_data_roaming = abs(default_data_roaming - 1)
    new_network_preference = abs(default_network_preference - 1)
    ad.log.info(
        "Set data roaming = %s, mobile data = 0, network preference = %s",
        new_data_roaming, new_network_preference)
    ad.adb.shell("settings put global mobile_data 0")
    ad.adb.shell("settings put global data_roaming %s" % new_data_roaming)
    ad.adb.shell("settings put global preferred_network_mode %s" %
                 new_network_preference)


def verify_default_telephony_setting(ad):
    ad.log.info("carrier_config: %s", dumpsys_carrier_config(ad))
    default_data_roaming = int(
        ad.adb.getprop("ro.com.android.dataroaming") == 'true')
    default_network_preference = int(
        ad.adb.getprop("ro.telephony.default_network"))
    ad.log.info("Default data roaming %s, network preference %s",
                default_data_roaming, default_network_preference)
    data_roaming = int(ad.adb.shell("settings get global data_roaming"))
    mobile_data = int(ad.adb.shell("settings get global mobile_data"))
    network_preference = int(
        ad.adb.shell("settings get global preferred_network_mode"))
    airplane_mode = int(ad.adb.shell("settings get global airplane_mode_on"))
    result = True
    ad.log.info("data_roaming = %s, mobile_data = %s, "
                "network_perference = %s, airplane_mode = %s", data_roaming,
                mobile_data, network_preference, airplane_mode)
    if airplane_mode:
        ad.log.error("Airplane mode is on")
        result = False
    if data_roaming != default_data_roaming:
        ad.log.error("Data roaming is %s, expecting %s", data_roaming,
                     default_data_roaming)
        result = False
    if not mobile_data:
        ad.log.error("Mobile data is off")
        result = False
    if network_preference != default_network_preference:
        ad.log.error("preferred_network_mode is %s, expecting %s",
                     network_preference, default_network_preference)
        result = False
    return result


def log_messaging_screen_shot(ad, test_name=""):
    ad.ensure_screen_on()
    ad.send_keycode("HOME")
    ad.adb.shell("am start -n com.google.android.apps.messaging/.ui."
                 "ConversationListActivity")
    log_screen_shot(ad, test_name)
    ad.adb.shell("am start -n com.google.android.apps.messaging/com.google."
                 "android.apps.messaging.ui.conversation.ConversationActivity"
                 " -e conversation_id 1")
    log_screen_shot(ad, test_name)
    ad.send_keycode("HOME")


def log_screen_shot(ad, test_name=""):
    file_name = "/sdcard/Pictures/screencap"
    if test_name:
        file_name = "%s_%s" % (file_name, test_name)
    file_name = "%s_%s.png" % (file_name, utils.get_current_epoch_time())
    try:
        ad.adb.shell("screencap -p %s" % file_name)
    except:
        ad.log.error("Fail to log screen shot to %s", file_name)


def get_screen_shot_log(ad, test_name="", begin_time=None):
    logs = ad.get_file_names("/sdcard/Pictures", begin_time=begin_time)
    if logs:
        ad.log.info("Pulling %s", logs)
        log_path = os.path.join(ad.device_log_path, "Screenshot_%s" % ad.serial)
        os.makedirs(log_path, exist_ok=True)
        ad.pull_files(logs, log_path)
    ad.adb.shell("rm -rf /sdcard/Pictures/screencap_*", ignore_status=True)


def get_screen_shot_logs(ads, test_name="", begin_time=None):
    for ad in ads:
        get_screen_shot_log(ad, test_name=test_name, begin_time=begin_time)


def get_carrier_id_version(ad):
    out = ad.adb.shell("dumpsys activity service TelephonyDebugService | " \
                       "grep -i carrier_list_version")
    if out and ":" in out:
        version = out.split(':')[1].lstrip()
    else:
        version = "0"
    ad.log.debug("Carrier Config Version is %s", version)
    return version


def get_carrier_config_version(ad):
    out = ad.adb.shell("dumpsys carrier_config | grep version_string")
    if out and "-" in out:
        version = out.split('-')[1]
    else:
        version = "0"
    ad.log.debug("Carrier Config Version is %s", version)
    return version


def install_googleaccountutil_apk(ad, account_util):
    ad.log.info("Install account_util %s", account_util)
    ad.ensure_screen_on()
    ad.adb.install("-r %s" % account_util, timeout=300, ignore_status=True)
    time.sleep(3)
    if not ad.is_apk_installed("com.google.android.tradefed.account"):
        ad.log.info("com.google.android.tradefed.account is not installed")
        return False
    return True


def install_googlefi_apk(ad, fi_util):
    ad.log.info("Install fi_util %s", fi_util)
    ad.ensure_screen_on()
    ad.adb.install("-r -g --user 0 %s" % fi_util,
                   timeout=300, ignore_status=True)
    time.sleep(3)
    if not check_fi_apk_installed(ad):
        return False
    return True


def check_fi_apk_installed(ad):
    if not ad.is_apk_installed("com.google.android.apps.tycho"):
        ad.log.warning("com.google.android.apps.tycho is not installed")
        return False
    return True


def add_google_account(ad, retries=3):
    if not ad.is_apk_installed("com.google.android.tradefed.account"):
        ad.log.error("GoogleAccountUtil is not installed")
        return False
    for _ in range(retries):
        ad.ensure_screen_on()
        output = ad.adb.shell(
            'am instrument -w -e account "%s@gmail.com" -e password '
            '"%s" -e sync true -e wait-for-checkin false '
            'com.google.android.tradefed.account/.AddAccount' %
            (ad.user_account, ad.user_password))
        if "result=SUCCESS" in output:
            ad.log.info("Google account is added successfully")
            return True
    ad.log.error("Failed to add google account - %s", output)
    return False


def remove_google_account(ad, retries=3):
    if not ad.is_apk_installed("com.google.android.tradefed.account"):
        ad.log.error("GoogleAccountUtil is not installed")
        return False
    for _ in range(retries):
        ad.ensure_screen_on()
        output = ad.adb.shell(
            'am instrument -w '
            'com.google.android.tradefed.account/.RemoveAccounts')
        if "result=SUCCESS" in output:
            ad.log.info("google account is removed successfully")
            return True
    ad.log.error("Fail to remove google account due to %s", output)
    return False


def my_current_screen_content(ad, content):
    ad.adb.shell("uiautomator dump --window=WINDOW")
    out = ad.adb.shell("cat /sdcard/window_dump.xml | grep -E '%s'" % content)
    if not out:
        ad.log.warning("NOT FOUND - %s", content)
        return False
    return True


def activate_esim_using_suw(ad):
    _START_SUW = ('am start -a android.intent.action.MAIN -n '
                  'com.google.android.setupwizard/.SetupWizardTestActivity')
    _STOP_SUW = ('am start -a com.android.setupwizard.EXIT')

    toggle_airplane_mode(ad.log, ad, new_state=False, strict_checking=False)
    ad.adb.shell("settings put system screen_off_timeout 1800000")
    ad.ensure_screen_on()
    ad.send_keycode("MENU")
    ad.send_keycode("HOME")
    for _ in range(3):
        ad.log.info("Attempt %d - activating eSIM", (_ + 1))
        ad.adb.shell(_START_SUW)
        time.sleep(10)
        log_screen_shot(ad, "start_suw")
        for _ in range(4):
            ad.send_keycode("TAB")
            time.sleep(0.5)
        ad.send_keycode("ENTER")
        time.sleep(15)
        log_screen_shot(ad, "activate_esim")
        get_screen_shot_log(ad)
        ad.adb.shell(_STOP_SUW)
        time.sleep(5)
        current_sim = get_sim_state(ad)
        ad.log.info("Current SIM status is %s", current_sim)
        if current_sim not in (SIM_STATE_ABSENT, SIM_STATE_UNKNOWN):
            break
    return True

def activate_google_fi_account(ad, retries=10):
    _FI_APK = "com.google.android.apps.tycho"
    _FI_ACTIVATE_CMD = ('am start -c android.intent.category.DEFAULT -n '
                        'com.google.android.apps.tycho/.AccountDetailsActivity --ez '
                        'in_setup_wizard false --ez force_show_account_chooser '
                        'false')
    toggle_airplane_mode(ad.log, ad, new_state=False, strict_checking=False)
    ad.adb.shell("settings put system screen_off_timeout 1800000")
    page_match_dict = {
       "SelectAccount" : "Choose an account to use",
       "Setup" : "Activate Google Fi to use your device for calls",
       "Switch" : "Switch to the Google Fi mobile network",
       "WiFi" : "Fi to download your SIM",
       "Connect" : "Connect to the Google Fi mobile network",
       "Move" : "Move number",
       "Data" : "first turn on mobile data",
       "Activate" : "This takes a minute or two, sometimes longer",
       "Welcome" : "Welcome to Google Fi",
       "Account" : "Your current cycle ends in"
    }
    page_list = ["Account", "Setup", "WiFi", "Switch", "Connect",
                 "Activate", "Move", "Welcome", "Data"]
    for _ in range(retries):
        ad.force_stop_apk(_FI_APK)
        ad.ensure_screen_on()
        ad.send_keycode("MENU")
        ad.send_keycode("HOME")
        ad.adb.shell(_FI_ACTIVATE_CMD)
        time.sleep(15)
        for page in page_list:
            if my_current_screen_content(ad, page_match_dict[page]):
                ad.log.info("Ready for Step %s", page)
                log_screen_shot(ad, "fi_activation_step_%s" % page)
                if page in ("Setup", "Switch", "Connect", "WiFi"):
                    ad.send_keycode("TAB")
                    ad.send_keycode("TAB")
                    ad.send_keycode("ENTER")
                    time.sleep(30)
                elif page == "Move" or page == "SelectAccount":
                    ad.send_keycode("TAB")
                    ad.send_keycode("ENTER")
                    time.sleep(5)
                elif page == "Welcome":
                    ad.send_keycode("TAB")
                    ad.send_keycode("TAB")
                    ad.send_keycode("TAB")
                    ad.send_keycode("ENTER")
                    ad.log.info("Activation SUCCESS using Fi App")
                    time.sleep(5)
                    ad.send_keycode("TAB")
                    ad.send_keycode("TAB")
                    ad.send_keycode("ENTER")
                    return True
                elif page == "Activate":
                    time.sleep(60)
                    if my_current_screen_content(ad, page_match_dict[page]):
                        time.sleep(60)
                elif page == "Account":
                    return True
                elif page == "Data":
                    ad.log.error("Mobile Data is turned OFF by default")
                    ad.send_keycode("TAB")
                    ad.send_keycode("TAB")
                    ad.send_keycode("ENTER")
            else:
                ad.log.info("NOT FOUND - Page %s", page)
                log_screen_shot(ad, "fi_activation_step_%s_failure" % page)
                get_screen_shot_log(ad)
    return False


def check_google_fi_activated(ad, retries=20):
    if check_fi_apk_installed(ad):
        _FI_APK = "com.google.android.apps.tycho"
        _FI_LAUNCH_CMD = ("am start -n %s/%s.AccountDetailsActivity" \
                          % (_FI_APK, _FI_APK))
        toggle_airplane_mode(ad.log, ad, new_state=False, strict_checking=False)
        ad.adb.shell("settings put system screen_off_timeout 1800000")
        ad.force_stop_apk(_FI_APK)
        ad.ensure_screen_on()
        ad.send_keycode("HOME")
        ad.adb.shell(_FI_LAUNCH_CMD)
        time.sleep(10)
        if not my_current_screen_content(ad, "Your current cycle ends in"):
            ad.log.warning("Fi is not activated")
            return False
        ad.send_keycode("HOME")
        return True
    else:
        ad.log.info("Fi Apk is not yet installed")
        return False


def cleanup_configupdater(ad):
    cmds = ('rm -rf /data/data/com.google.android.configupdater/shared_prefs',
            'rm /data/misc/carrierid/carrier_list.pb',
            'setprop persist.telephony.test.carrierid.ota true',
            'rm /data/user_de/0/com.android.providers.telephony/shared_prefs'
            '/CarrierIdProvider.xml')
    for cmd in cmds:
        ad.log.info("Cleanup ConfigUpdater - %s", cmd)
        ad.adb.shell(cmd, ignore_status=True)


def pull_carrier_id_files(ad, carrier_id_path):
    os.makedirs(carrier_id_path, exist_ok=True)
    ad.log.info("Pull CarrierId Files")
    cmds = ('/data/data/com.google.android.configupdater/shared_prefs/',
            '/data/misc/carrierid/',
            '/data/user_de/0/com.android.providers.telephony/shared_prefs/',
            '/data/data/com.android.providers.downloads/databases/downloads.db')
    for cmd in cmds:
        cmd = cmd + " %s" % carrier_id_path
        ad.adb.pull(cmd, timeout=30, ignore_status=True)


def bring_up_connectivity_monitor(ad):
    monitor_apk = None
    for apk in ("com.google.telephonymonitor",
                "com.google.android.connectivitymonitor"):
        if ad.is_apk_installed(apk):
            ad.log.info("apk %s is installed", apk)
            monitor_apk = apk
            break
    if not monitor_apk:
        ad.log.info("ConnectivityMonitor|TelephonyMonitor is not installed")
        return False
    toggle_connectivity_monitor_setting(ad, True)

    if not ad.is_apk_running(monitor_apk):
        ad.log.info("%s is not running", monitor_apk)
        # Reboot
        ad.log.info("reboot to bring up %s", monitor_apk)
        reboot_device(ad)
        for i in range(30):
            if ad.is_apk_running(monitor_apk):
                ad.log.info("%s is running after reboot", monitor_apk)
                return True
            else:
                ad.log.info(
                    "%s is not running after reboot. Wait and check again",
                    monitor_apk)
                time.sleep(30)
        ad.log.error("%s is not running after reboot", monitor_apk)
        return False
    else:
        ad.log.info("%s is running", monitor_apk)
        return True


def get_host_ip_address(ad):
    cmd = "|".join(("ifconfig", "grep eno1 -A1", "grep inet", "awk '{$1=$1};1'", "cut -d ' ' -f 2"))
    destination_ip = exe_cmd(cmd)
    destination_ip = (destination_ip.decode("utf-8")).split("\n")[0]
    ad.log.info("Host IP is %s", destination_ip)
    return destination_ip


def load_scone_cat_simulate_data(ad, simulate_data, sub_id=None):
    """ Load radio simulate data
    ad: android device controller
    simulate_data: JSON object of simulate data
    sub_id: RIL sub id, should be 0 or 1
    """
    ad.log.info("load_scone_cat_simulate_data")

    #Check RIL sub id
    if sub_id is None or sub_id > 1:
        ad.log.error("The value of RIL sub_id should be 0 or 1")
        return False

    action = "com.google.android.apps.scone.cat.action.SetSimulateData"

    #add sub id
    simulate_data["SubId"] = sub_id
    try:
        #dump json
        extra = json.dumps(simulate_data)
        ad.log.info("send simulate_data=[%s]" % extra)
        #send data
        ad.adb.shell("am broadcast -a " + action + " --es simulate_data '" + extra + "'")
    except Exception as e:
        ad.log.error("Exception error to send CAT: %s", e)
        return False

    return True


def load_scone_cat_data_from_file(ad, simulate_file_path, sub_id=None):
    """ Load radio simulate data
    ad: android device controller
    simulate_file_path: JSON file of simulate data
    sub_id: RIL sub id, should be 0 or 1
    """
    ad.log.info("load_radio_simulate_data_from_file from %s" % simulate_file_path)
    radio_simulate_data = {}

    #Check RIL sub id
    if sub_id is None or sub_id > 1:
        ad.log.error("The value of RIL sub_id should be 0 or 1")
        raise ValueError

    with open(simulate_file_path, 'r') as f:
        try:
            radio_simulate_data = json.load(f)
        except Exception as e:
            self.log.error("Exception error to load %s: %s", f, e)
            return False

    for item in radio_simulate_data:
        result = load_scone_cat_simulate_data(ad, item, sub_id)
        if result == False:
            ad.log.error("Load CAT command fail")
            return False
        time.sleep(0.1)

    return True


def toggle_connectivity_monitor_setting(ad, state=True):
    monitor_setting = ad.adb.getprop("persist.radio.enable_tel_mon")
    ad.log.info("radio.enable_tel_mon setting is %s", monitor_setting)
    current_state = True if monitor_setting == "user_enabled" else False
    if current_state == state:
        return True
    elif state is None:
        state = not current_state
    expected_monitor_setting = "user_enabled" if state else "disabled"
    cmd = "setprop persist.radio.enable_tel_mon %s" % expected_monitor_setting
    ad.log.info("Toggle connectivity monitor by %s", cmd)
    ad.adb.shell(
        "am start -n com.android.settings/.DevelopmentSettings",
        ignore_status=True)
    ad.adb.shell(cmd)
    monitor_setting = ad.adb.getprop("persist.radio.enable_tel_mon")
    ad.log.info("radio.enable_tel_mon setting is %s", monitor_setting)
    return monitor_setting == expected_monitor_setting

def get_call_forwarding_by_adb(log, ad, call_forwarding_type="unconditional"):
    """ Get call forwarding status by adb shell command
        'dumpsys telephony.registry'.

        Args:
            log: log object
            ad: android object
            call_forwarding_type:
                - "unconditional"
                - "busy" (todo)
                - "not_answered" (todo)
                - "not_reachable" (todo)
        Returns:
            - "true": if call forwarding unconditional is enabled.
            - "false": if call forwarding unconditional is disabled.
            - "unknown": if the type is other than 'unconditional'.
            - False: any case other than above 3 cases.
    """
    if call_forwarding_type != "unconditional":
        return "unknown"

    slot_index_of_default_voice_subid = get_slot_index_from_subid(log, ad,
        get_incoming_voice_sub_id(ad))
    output = ad.adb.shell("dumpsys telephony.registry | grep mCallForwarding")
    if "mCallForwarding" in output:
        result_list = re.findall(r"mCallForwarding=(true|false)", output)
        if result_list:
            result = result_list[slot_index_of_default_voice_subid]
            ad.log.info("mCallForwarding is %s", result)

            if re.search("false", result, re.I):
                return "false"
            elif re.search("true", result, re.I):
                return "true"
            else:
                return False
        else:
            return False
    else:
        ad.log.error("'mCallForwarding' cannot be found in dumpsys.")
        return False

def erase_call_forwarding_by_mmi(
        log,
        ad,
        retry=2,
        call_forwarding_type="unconditional"):
    """ Erase setting of call forwarding (erase the number and disable call
    forwarding) by MMI code.

    Args:
        log: log object
        ad: android object
        retry: times of retry if the erasure failed.
        call_forwarding_type:
            - "unconditional"
            - "busy"
            - "not_answered"
            - "not_reachable"
    Returns:
        True by successful erasure. Otherwise False.
    """
    res = get_call_forwarding_by_adb(log, ad,
        call_forwarding_type=call_forwarding_type)
    if res == "false":
        return True

    user_config_profile = get_user_config_profile(ad)
    is_airplane_mode = user_config_profile["Airplane Mode"]
    is_wfc_enabled = user_config_profile["WFC Enabled"]
    wfc_mode = user_config_profile["WFC Mode"]
    is_wifi_on = user_config_profile["WiFi State"]

    if is_airplane_mode:
        if not toggle_airplane_mode(log, ad, False):
            ad.log.error("Failed to disable airplane mode.")
            return False

    operator_name = get_operator_name(log, ad)

    code_dict = {
        "Verizon": {
            "unconditional": "73",
            "busy": "73",
            "not_answered": "73",
            "not_reachable": "73",
            "mmi": "*%s"
        },
        "Sprint": {
            "unconditional": "720",
            "busy": "740",
            "not_answered": "730",
            "not_reachable": "720",
            "mmi": "*%s"
        },
        'Generic': {
            "unconditional": "21",
            "busy": "67",
            "not_answered": "61",
            "not_reachable": "62",
            "mmi": "##%s#"
        }
    }

    if operator_name in code_dict:
        code = code_dict[operator_name][call_forwarding_type]
        mmi = code_dict[operator_name]["mmi"]
    else:
        code = code_dict['Generic'][call_forwarding_type]
        mmi = code_dict['Generic']["mmi"]

    result = False
    while retry >= 0:
        res = get_call_forwarding_by_adb(
            log, ad, call_forwarding_type=call_forwarding_type)
        if res == "false":
            ad.log.info("Call forwarding is already disabled.")
            result = True
            break

        ad.log.info("Erasing and deactivating call forwarding %s..." %
            call_forwarding_type)

        ad.droid.telecomDialNumber(mmi % code)

        time.sleep(3)
        ad.send_keycode("ENTER")
        time.sleep(15)

        # To dismiss the pop-out dialog
        ad.send_keycode("BACK")
        time.sleep(5)
        ad.send_keycode("BACK")

        res = get_call_forwarding_by_adb(
            log, ad, call_forwarding_type=call_forwarding_type)
        if res == "false" or res == "unknown":
            result = True
            break
        else:
            ad.log.error("Failed to erase and deactivate call forwarding by "
                "MMI code ##%s#." % code)
            retry = retry - 1
            time.sleep(30)

    if is_airplane_mode:
        if not toggle_airplane_mode(log, ad, True):
            ad.log.error("Failed to enable airplane mode again.")
        else:
            if is_wifi_on:
                ad.droid.wifiToggleState(True)
                if is_wfc_enabled:
                    if not wait_for_wfc_enabled(
                        log, ad,max_time=MAX_WAIT_TIME_WFC_ENABLED):
                        ad.log.error("WFC is not enabled")

    return result

def set_call_forwarding_by_mmi(
        log,
        ad,
        ad_forwarded,
        call_forwarding_type="unconditional",
        retry=2):
    """ Set up the forwarded number and enable call forwarding by MMI code.

    Args:
        log: log object
        ad: android object of the device forwarding the call (primary device)
        ad_forwarded: android object of the device receiving forwarded call.
        retry: times of retry if the erasure failed.
        call_forwarding_type:
            - "unconditional"
            - "busy"
            - "not_answered"
            - "not_reachable"
    Returns:
        True by successful erasure. Otherwise False.
    """

    res = get_call_forwarding_by_adb(log, ad,
        call_forwarding_type=call_forwarding_type)
    if res == "true":
        return True

    if ad.droid.connectivityCheckAirplaneMode():
        ad.log.warning("%s is now in airplane mode.", ad.serial)
        return False

    operator_name = get_operator_name(log, ad)

    code_dict = {
        "Verizon": {
            "unconditional": "72",
            "busy": "71",
            "not_answered": "71",
            "not_reachable": "72",
            "mmi": "*%s%s"
        },
        "Sprint": {
            "unconditional": "72",
            "busy": "74",
            "not_answered": "73",
            "not_reachable": "72",
            "mmi": "*%s%s"
        },
        'Generic': {
            "unconditional": "21",
            "busy": "67",
            "not_answered": "61",
            "not_reachable": "62",
            "mmi": "*%s*%s#",
            "mmi_for_plus_sign": "*%s*"
        }
    }

    if operator_name in code_dict:
        code = code_dict[operator_name][call_forwarding_type]
        mmi = code_dict[operator_name]["mmi"]
    else:
        code = code_dict['Generic'][call_forwarding_type]
        mmi = code_dict['Generic']["mmi"]
        mmi_for_plus_sign = code_dict['Generic']["mmi_for_plus_sign"]

    while retry >= 0:
        if not erase_call_forwarding_by_mmi(
            log, ad, call_forwarding_type=call_forwarding_type):
            retry = retry - 1
            continue

        forwarded_number = ad_forwarded.telephony['subscription'][
            ad_forwarded.droid.subscriptionGetDefaultVoiceSubId()][
            'phone_num']
        ad.log.info("Registering and activating call forwarding %s to %s..." %
            (call_forwarding_type, forwarded_number))

        (forwarded_number_no_prefix, _) = _phone_number_remove_prefix(
            forwarded_number)

        _found_plus_sign = 0
        if re.search("^\+", forwarded_number):
            _found_plus_sign = 1
            forwarded_number.replace("+", "")

        if operator_name in code_dict:
            ad.droid.telecomDialNumber(mmi % (code, forwarded_number_no_prefix))
        else:
            if _found_plus_sign == 0:
                ad.droid.telecomDialNumber(mmi % (code, forwarded_number))
            else:
                ad.droid.telecomDialNumber(mmi_for_plus_sign % code)
                ad.send_keycode("PLUS")
                dial_phone_number(ad, forwarded_number + "#")

        time.sleep(3)
        ad.send_keycode("ENTER")
        time.sleep(15)

        # To dismiss the pop-out dialog
        ad.send_keycode("BACK")
        time.sleep(5)
        ad.send_keycode("BACK")

        result = get_call_forwarding_by_adb(
            log, ad, call_forwarding_type=call_forwarding_type)
        if result == "false":
            retry = retry - 1
        elif result == "true":
            return True
        elif result == "unknown":
            return True
        else:
            retry = retry - 1

        if retry >= 0:
            ad.log.warning("Failed to register or activate call forwarding %s "
                "to %s. Retry after 15 seconds." % (call_forwarding_type,
                    forwarded_number))
            time.sleep(15)

    ad.log.error("Failed to register or activate call forwarding %s to %s." %
        (call_forwarding_type, forwarded_number))
    return False


def get_rx_tx_power_levels(log, ad):
    """ Obtains Rx and Tx power levels from the MDS application.

    The method requires the MDS app to be installed in the DUT.

    Args:
        log: logger object
        ad: an android device

    Return:
        A tuple where the first element is an array array with the RSRP value
        in Rx chain, and the second element is the transmitted power in dBm.
        Values for invalid Rx / Tx chains are set to None.
    """
    cmd = ('am instrument -w -e request "80 00 e8 03 00 08 00 00 00" -e '
           'response wait "com.google.mdstest/com.google.mdstest.instrument.'
           'ModemCommandInstrumentation"')
    output = ad.adb.shell(cmd)

    if 'result=SUCCESS' not in output:
        raise RuntimeError('Could not obtain Tx/Rx power levels from MDS. Is '
                           'the MDS app installed?')

    response = re.search(r"(?<=response=).+", output)

    if not response:
        raise RuntimeError('Invalid response from the MDS app:\n' + output)

    # Obtain a list of bytes in hex format from the response string
    response_hex = response.group(0).split(' ')

    def get_bool(pos):
        """ Obtain a boolean variable from the byte array. """
        return response_hex[pos] == '01'

    def get_int32(pos):
        """ Obtain an int from the byte array. Bytes are printed in
        little endian format."""
        return struct.unpack(
            '<i', bytearray.fromhex(''.join(response_hex[pos:pos + 4])))[0]

    rx_power = []
    RX_CHAINS = 4

    for i in range(RX_CHAINS):
        # Calculate starting position for the Rx chain data structure
        start = 12 + i * 22

        # The first byte in the data structure indicates if the rx chain is
        # valid.
        if get_bool(start):
            rx_power.append(get_int32(start + 2) / 10)
        else:
            rx_power.append(None)

    # Calculate the position for the tx chain data structure
    tx_pos = 12 + RX_CHAINS * 22

    tx_valid = get_bool(tx_pos)
    if tx_valid:
        tx_power = get_int32(tx_pos + 2) / -10
    else:
        tx_power = None

    return rx_power, tx_power
