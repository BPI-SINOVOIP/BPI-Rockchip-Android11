#!/usr/bin/env python3
#
# Copyright (C) 2016 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.

import logging
import os
import random
import re
import string
import threading
import time
from queue import Empty
from subprocess import call

from acts import asserts
from acts.test_utils.bt.bt_constants import adv_fail
from acts.test_utils.bt.bt_constants import adv_succ
from acts.test_utils.bt.bt_constants import batch_scan_not_supported_list
from acts.test_utils.bt.bt_constants import bits_per_samples
from acts.test_utils.bt.bt_constants import ble_advertise_settings_modes
from acts.test_utils.bt.bt_constants import ble_advertise_settings_tx_powers
from acts.test_utils.bt.bt_constants import bluetooth_a2dp_codec_config_changed
from acts.test_utils.bt.bt_constants import bluetooth_off
from acts.test_utils.bt.bt_constants import bluetooth_on
from acts.test_utils.bt.bt_constants import \
    bluetooth_profile_connection_state_changed
from acts.test_utils.bt.bt_constants import bluetooth_socket_conn_test_uuid
from acts.test_utils.bt.bt_constants import bt_default_timeout
from acts.test_utils.bt.bt_constants import bt_profile_constants
from acts.test_utils.bt.bt_constants import bt_profile_states
from acts.test_utils.bt.bt_constants import bt_rfcomm_uuids
from acts.test_utils.bt.bt_constants import bt_scan_mode_types
from acts.test_utils.bt.bt_constants import btsnoop_last_log_path_on_device
from acts.test_utils.bt.bt_constants import btsnoop_log_path_on_device
from acts.test_utils.bt.bt_constants import channel_modes
from acts.test_utils.bt.bt_constants import codec_types
from acts.test_utils.bt.bt_constants import default_bluetooth_socket_timeout_ms
from acts.test_utils.bt.bt_constants import default_rfcomm_timeout_ms
from acts.test_utils.bt.bt_constants import hid_id_keyboard
from acts.test_utils.bt.bt_constants import pairing_variant_passkey_confirmation
from acts.test_utils.bt.bt_constants import pan_connect_timeout
from acts.test_utils.bt.bt_constants import sample_rates
from acts.test_utils.bt.bt_constants import scan_result
from acts.test_utils.bt.bt_constants import sig_uuid_constants
from acts.test_utils.bt.bt_constants import small_timeout
from acts.test_utils.tel.tel_test_utils import toggle_airplane_mode_by_adb
from acts.test_utils.tel.tel_test_utils import verify_http_connection
from acts.utils import exe_cmd

from acts import utils

log = logging

advertisements_to_devices = {}


class BtTestUtilsError(Exception):
    pass


def _add_android_device_to_dictionary(android_device, profile_list,
                                      selector_dict):
    """Adds the AndroidDevice and supported features to the selector dictionary

    Args:
        android_device: The Android device.
        profile_list: The list of profiles the Android device supports.
    """
    for profile in profile_list:
        if profile in selector_dict and android_device not in selector_dict[
                profile]:
            selector_dict[profile].append(android_device)
        else:
            selector_dict[profile] = [android_device]


def bluetooth_enabled_check(ad, timeout_sec=5):
    """Checks if the Bluetooth state is enabled, if not it will attempt to
    enable it.

    Args:
        ad: The Android device list to enable Bluetooth on.
        timeout_sec: number of seconds to wait for toggle to take effect.

    Returns:
        True if successful, false if unsuccessful.
    """
    if not ad.droid.bluetoothCheckState():
        ad.droid.bluetoothToggleState(True)
        expected_bluetooth_on_event_name = bluetooth_on
        try:
            ad.ed.pop_event(expected_bluetooth_on_event_name,
                            bt_default_timeout)
        except Empty:
            ad.log.info(
                "Failed to toggle Bluetooth on(no broadcast received).")
            # Try one more time to poke at the actual state.
            if ad.droid.bluetoothCheckState():
                ad.log.info(".. actual state is ON")
                return True
            ad.log.error(".. actual state is OFF")
            return False
    end_time = time.time() + timeout_sec
    while not ad.droid.bluetoothCheckState() and time.time() < end_time:
        time.sleep(1)
    return ad.droid.bluetoothCheckState()


def check_device_supported_profiles(droid):
    """Checks for Android device supported profiles.

    Args:
        droid: The droid object to query.

    Returns:
        A dictionary of supported profiles.
    """
    profile_dict = {}
    profile_dict['hid'] = droid.bluetoothHidIsReady()
    profile_dict['hsp'] = droid.bluetoothHspIsReady()
    profile_dict['a2dp'] = droid.bluetoothA2dpIsReady()
    profile_dict['avrcp'] = droid.bluetoothAvrcpIsReady()
    profile_dict['a2dp_sink'] = droid.bluetoothA2dpSinkIsReady()
    profile_dict['hfp_client'] = droid.bluetoothHfpClientIsReady()
    profile_dict['pbap_client'] = droid.bluetoothPbapClientIsReady()
    return profile_dict


def cleanup_scanners_and_advertisers(scn_android_device, scn_callback_list,
                                     adv_android_device, adv_callback_list):
    """Try to gracefully stop all scanning and advertising instances.

    Args:
        scn_android_device: The Android device that is actively scanning.
        scn_callback_list: The scan callback id list that needs to be stopped.
        adv_android_device: The Android device that is actively advertising.
        adv_callback_list: The advertise callback id list that needs to be
            stopped.
    """
    scan_droid, scan_ed = scn_android_device.droid, scn_android_device.ed
    adv_droid = adv_android_device.droid
    try:
        for scan_callback in scn_callback_list:
            scan_droid.bleStopBleScan(scan_callback)
    except Exception as err:
        scn_android_device.log.debug(
            "Failed to stop LE scan... reseting Bluetooth. Error {}".format(
                err))
        reset_bluetooth([scn_android_device])
    try:
        for adv_callback in adv_callback_list:
            adv_droid.bleStopBleAdvertising(adv_callback)
    except Exception as err:
        adv_android_device.log.debug(
            "Failed to stop LE advertisement... reseting Bluetooth. Error {}".
            format(err))
        reset_bluetooth([adv_android_device])


def clear_bonded_devices(ad):
    """Clear bonded devices from the input Android device.

    Args:
        ad: the Android device performing the connection.
    Returns:
        True if clearing bonded devices was successful, false if unsuccessful.
    """
    bonded_device_list = ad.droid.bluetoothGetBondedDevices()
    while bonded_device_list:
        device_address = bonded_device_list[0]['address']
        if not ad.droid.bluetoothUnbond(device_address):
            log.error("Failed to unbond {} from {}".format(
                device_address, ad.serial))
            return False
        log.info("Successfully unbonded {} from {}".format(
            device_address, ad.serial))
        #TODO: wait for BOND_STATE_CHANGED intent instead of waiting
        time.sleep(1)

        # If device was first connected using LE transport, after bonding it is
        # accessible through it's LE address, and through it classic address.
        # Unbonding it will unbond two devices representing different
        # "addresses". Attempt to unbond such already unbonded devices will
        # result in bluetoothUnbond returning false.
        bonded_device_list = ad.droid.bluetoothGetBondedDevices()
    return True


def connect_phone_to_headset(android,
                             headset,
                             timeout=bt_default_timeout,
                             connection_check_period=10):
    """Connects android phone to bluetooth headset.
    Headset object must have methods power_on and enter_pairing_mode,
    and attribute mac_address.

    Args:
        android: AndroidDevice object with SL4A installed.
        headset: Object with attribute mac_address and methods power_on and
            enter_pairing_mode.
        timeout: Seconds to wait for devices to connect.
        connection_check_period: how often to check for connection once the
            SL4A connect RPC has been sent.
    Returns:
        connected (bool): True if devices are paired and connected by end of
        method. False otherwise.
    """
    headset_mac_address = headset.mac_address
    connected = is_a2dp_src_device_connected(android, headset_mac_address)
    log.info('Devices connected before pair attempt: %s' % connected)
    if not connected:
        # Turn on headset and initiate pairing mode.
        headset.enter_pairing_mode()
        android.droid.bluetoothStartPairingHelper()
    start_time = time.time()
    # If already connected, skip pair and connect attempt.
    while not connected and (time.time() - start_time < timeout):
        bonded_info = android.droid.bluetoothGetBondedDevices()
        if headset.mac_address not in [
                info["address"] for info in bonded_info
        ]:
            # Use SL4A to pair and connect with headset.
            headset.enter_pairing_mode()
            android.droid.bluetoothDiscoverAndBond(headset_mac_address)
        else:  # Device is bonded but not connected
            android.droid.bluetoothConnectBonded(headset_mac_address)
        log.info('Waiting for connection...')
        time.sleep(connection_check_period)
        # Check for connection.
        connected = is_a2dp_src_device_connected(android, headset_mac_address)
    log.info('Devices connected after pair attempt: %s' % connected)
    return connected


def connect_pri_to_sec(pri_ad, sec_ad, profiles_set, attempts=2):
    """Connects pri droid to secondary droid.

    Args:
        pri_ad: AndroidDroid initiating connection
        sec_ad: AndroidDroid accepting connection
        profiles_set: Set of profiles to be connected
        attempts: Number of attempts to try until failure.

    Returns:
        Pass if True
        Fail if False
    """
    device_addr = sec_ad.droid.bluetoothGetLocalAddress()
    # Allows extra time for the SDP records to be updated.
    time.sleep(2)
    curr_attempts = 0
    while curr_attempts < attempts:
        log.info("connect_pri_to_sec curr attempt {} total {}".format(
            curr_attempts, attempts))
        if _connect_pri_to_sec(pri_ad, sec_ad, profiles_set):
            return True
        curr_attempts += 1
    log.error("connect_pri_to_sec failed to connect after {} attempts".format(
        attempts))
    return False


def _connect_pri_to_sec(pri_ad, sec_ad, profiles_set):
    """Connects pri droid to secondary droid.

    Args:
        pri_ad: AndroidDroid initiating connection.
        sec_ad: AndroidDroid accepting connection.
        profiles_set: Set of profiles to be connected.

    Returns:
        True of connection is successful, false if unsuccessful.
    """
    # Check if we support all profiles.
    supported_profiles = bt_profile_constants.values()
    for profile in profiles_set:
        if profile not in supported_profiles:
            pri_ad.log.info("Profile {} is not supported list {}".format(
                profile, supported_profiles))
            return False

    # First check that devices are bonded.
    paired = False
    for paired_device in pri_ad.droid.bluetoothGetBondedDevices():
        if paired_device['address'] == \
                sec_ad.droid.bluetoothGetLocalAddress():
            paired = True
            break

    if not paired:
        pri_ad.log.error("Not paired to {}".format(sec_ad.serial))
        return False

    # Now try to connect them, the following call will try to initiate all
    # connections.
    pri_ad.droid.bluetoothConnectBonded(
        sec_ad.droid.bluetoothGetLocalAddress())

    end_time = time.time() + 10
    profile_connected = set()
    sec_addr = sec_ad.droid.bluetoothGetLocalAddress()
    pri_ad.log.info("Profiles to be connected {}".format(profiles_set))
    # First use APIs to check profile connection state
    while (time.time() < end_time
           and not profile_connected.issuperset(profiles_set)):
        if (bt_profile_constants['headset_client'] not in profile_connected
                and bt_profile_constants['headset_client'] in profiles_set):
            if is_hfp_client_device_connected(pri_ad, sec_addr):
                profile_connected.add(bt_profile_constants['headset_client'])
        if (bt_profile_constants['a2dp'] not in profile_connected
                and bt_profile_constants['a2dp'] in profiles_set):
            if is_a2dp_src_device_connected(pri_ad, sec_addr):
                profile_connected.add(bt_profile_constants['a2dp'])
        if (bt_profile_constants['a2dp_sink'] not in profile_connected
                and bt_profile_constants['a2dp_sink'] in profiles_set):
            if is_a2dp_snk_device_connected(pri_ad, sec_addr):
                profile_connected.add(bt_profile_constants['a2dp_sink'])
        if (bt_profile_constants['map_mce'] not in profile_connected
                and bt_profile_constants['map_mce'] in profiles_set):
            if is_map_mce_device_connected(pri_ad, sec_addr):
                profile_connected.add(bt_profile_constants['map_mce'])
        if (bt_profile_constants['map'] not in profile_connected
                and bt_profile_constants['map'] in profiles_set):
            if is_map_mse_device_connected(pri_ad, sec_addr):
                profile_connected.add(bt_profile_constants['map'])
        time.sleep(0.1)
    # If APIs fail, try to find the connection broadcast receiver.
    while not profile_connected.issuperset(profiles_set):
        try:
            profile_event = pri_ad.ed.pop_event(
                bluetooth_profile_connection_state_changed,
                bt_default_timeout + 10)
            pri_ad.log.info("Got event {}".format(profile_event))
        except Exception:
            pri_ad.log.error("Did not get {} profiles left {}".format(
                bluetooth_profile_connection_state_changed, profile_connected))
            return False

        profile = profile_event['data']['profile']
        state = profile_event['data']['state']
        device_addr = profile_event['data']['addr']
        if state == bt_profile_states['connected'] and \
                device_addr == sec_ad.droid.bluetoothGetLocalAddress():
            profile_connected.add(profile)
        pri_ad.log.info(
            "Profiles connected until now {}".format(profile_connected))
    # Failure happens inside the while loop. If we came here then we already
    # connected.
    return True


def determine_max_advertisements(android_device):
    """Determines programatically how many advertisements the Android device
    supports.

    Args:
        android_device: The Android device to determine max advertisements of.

    Returns:
        The maximum advertisement count.
    """
    android_device.log.info(
        "Determining number of maximum concurrent advertisements...")
    advertisement_count = 0
    bt_enabled = False
    expected_bluetooth_on_event_name = bluetooth_on
    if not android_device.droid.bluetoothCheckState():
        android_device.droid.bluetoothToggleState(True)
    try:
        android_device.ed.pop_event(expected_bluetooth_on_event_name,
                                    bt_default_timeout)
    except Exception:
        android_device.log.info(
            "Failed to toggle Bluetooth on(no broadcast received).")
        # Try one more time to poke at the actual state.
        if android_device.droid.bluetoothCheckState() is True:
            android_device.log.info(".. actual state is ON")
        else:
            android_device.log.error(
                "Failed to turn Bluetooth on. Setting default advertisements to 1"
            )
            advertisement_count = -1
            return advertisement_count
    advertise_callback_list = []
    advertise_data = android_device.droid.bleBuildAdvertiseData()
    advertise_settings = android_device.droid.bleBuildAdvertiseSettings()
    while (True):
        advertise_callback = android_device.droid.bleGenBleAdvertiseCallback()
        advertise_callback_list.append(advertise_callback)

        android_device.droid.bleStartBleAdvertising(advertise_callback,
                                                    advertise_data,
                                                    advertise_settings)

        regex = "(" + adv_succ.format(
            advertise_callback) + "|" + adv_fail.format(
                advertise_callback) + ")"
        # wait for either success or failure event
        evt = android_device.ed.pop_events(regex, bt_default_timeout,
                                           small_timeout)
        if evt[0]["name"] == adv_succ.format(advertise_callback):
            advertisement_count += 1
            android_device.log.info(
                "Advertisement {} started.".format(advertisement_count))
        else:
            error = evt[0]["data"]["Error"]
            if error == "ADVERTISE_FAILED_TOO_MANY_ADVERTISERS":
                android_device.log.info(
                    "Advertisement failed to start. Reached max " +
                    "advertisements at {}".format(advertisement_count))
                break
            else:
                raise BtTestUtilsError(
                    "Expected ADVERTISE_FAILED_TOO_MANY_ADVERTISERS," +
                    " but received bad error code {}".format(error))
    try:
        for adv in advertise_callback_list:
            android_device.droid.bleStopBleAdvertising(adv)
    except Exception:
        android_device.log.error(
            "Failed to stop advertisingment, resetting Bluetooth.")
        reset_bluetooth([android_device])
    return advertisement_count


def disable_bluetooth(droid):
    """Disable Bluetooth on input Droid object.

    Args:
        droid: The droid object to disable Bluetooth on.

    Returns:
        True if successful, false if unsuccessful.
    """
    if droid.bluetoothCheckState() is True:
        droid.bluetoothToggleState(False)
        if droid.bluetoothCheckState() is True:
            log.error("Failed to toggle Bluetooth off.")
            return False
    return True


def disconnect_pri_from_sec(pri_ad, sec_ad, profiles_list):
    """
    Disconnect primary from secondary on a specific set of profiles
    Args:
        pri_ad - Primary android_device initiating disconnection
        sec_ad - Secondary android droid (sl4a interface to keep the
          method signature the same connect_pri_to_sec above)
        profiles_list - List of profiles we want to disconnect from

    Returns:
        True on Success
        False on Failure
    """
    # Sanity check to see if all the profiles in the given set is supported
    supported_profiles = bt_profile_constants.values()
    for profile in profiles_list:
        if profile not in supported_profiles:
            pri_ad.log.info("Profile {} is not in supported list {}".format(
                profile, supported_profiles))
            return False

    pri_ad.log.info(pri_ad.droid.bluetoothGetBondedDevices())
    # Disconnecting on a already disconnected profile is a nop,
    # so not checking for the connection state
    try:
        pri_ad.droid.bluetoothDisconnectConnectedProfile(
            sec_ad.droid.bluetoothGetLocalAddress(), profiles_list)
    except Exception as err:
        pri_ad.log.error(
            "Exception while trying to disconnect profile(s) {}: {}".format(
                profiles_list, err))
        return False

    profile_disconnected = set()
    pri_ad.log.info("Disconnecting from profiles: {}".format(profiles_list))

    while not profile_disconnected.issuperset(profiles_list):
        try:
            profile_event = pri_ad.ed.pop_event(
                bluetooth_profile_connection_state_changed, bt_default_timeout)
            pri_ad.log.info("Got event {}".format(profile_event))
        except Exception as e:
            pri_ad.log.error(
                "Did not disconnect from Profiles. Reason {}".format(e))
            return False

        profile = profile_event['data']['profile']
        state = profile_event['data']['state']
        device_addr = profile_event['data']['addr']

        if state == bt_profile_states['disconnected'] and \
                device_addr == sec_ad.droid.bluetoothGetLocalAddress():
            profile_disconnected.add(profile)
        pri_ad.log.info(
            "Profiles disconnected so far {}".format(profile_disconnected))

    return True


def enable_bluetooth(droid, ed):
    if droid.bluetoothCheckState() is True:
        return True

    droid.bluetoothToggleState(True)
    expected_bluetooth_on_event_name = bluetooth_on
    try:
        ed.pop_event(expected_bluetooth_on_event_name, bt_default_timeout)
    except Exception:
        log.info("Failed to toggle Bluetooth on (no broadcast received)")
        if droid.bluetoothCheckState() is True:
            log.info(".. actual state is ON")
            return True
        log.info(".. actual state is OFF")
        return False

    return True


def factory_reset_bluetooth(android_devices):
    """Clears Bluetooth stack of input Android device list.

        Args:
            android_devices: The Android device list to reset Bluetooth

        Returns:
            True if successful, false if unsuccessful.
        """
    for a in android_devices:
        droid, ed = a.droid, a.ed
        a.log.info("Reset state of bluetooth on device.")
        if not bluetooth_enabled_check(a):
            return False
        # TODO: remove device unbond b/79418045
        # Temporary solution to ensure all devices are unbonded
        bonded_devices = droid.bluetoothGetBondedDevices()
        for b in bonded_devices:
            a.log.info("Removing bond for device {}".format(b['address']))
            droid.bluetoothUnbond(b['address'])

        droid.bluetoothFactoryReset()
        wait_for_bluetooth_manager_state(droid)
        if not enable_bluetooth(droid, ed):
            return False
    return True


def generate_ble_advertise_objects(droid):
    """Generate generic LE advertise objects.

    Args:
        droid: The droid object to generate advertise LE objects from.

    Returns:
        advertise_callback: The generated advertise callback id.
        advertise_data: The generated advertise data id.
        advertise_settings: The generated advertise settings id.
    """
    advertise_callback = droid.bleGenBleAdvertiseCallback()
    advertise_data = droid.bleBuildAdvertiseData()
    advertise_settings = droid.bleBuildAdvertiseSettings()
    return advertise_callback, advertise_data, advertise_settings


def generate_ble_scan_objects(droid):
    """Generate generic LE scan objects.

    Args:
        droid: The droid object to generate LE scan objects from.

    Returns:
        filter_list: The generated scan filter list id.
        scan_settings: The generated scan settings id.
        scan_callback: The generated scan callback id.
    """
    filter_list = droid.bleGenFilterList()
    scan_settings = droid.bleBuildScanSetting()
    scan_callback = droid.bleGenScanCallback()
    return filter_list, scan_settings, scan_callback


def generate_id_by_size(size,
                        chars=(string.ascii_lowercase +
                               string.ascii_uppercase + string.digits)):
    """Generate random ascii characters of input size and input char types

    Args:
        size: Input size of string.
        chars: (Optional) Chars to use in generating a random string.

    Returns:
        String of random input chars at the input size.
    """
    return ''.join(random.choice(chars) for _ in range(size))


def get_advanced_droid_list(android_devices):
    """Add max_advertisement and batch_scan_supported attributes to input
    Android devices

    This will programatically determine maximum LE advertisements of each
    input Android device.

    Args:
        android_devices: The Android devices to setup.

    Returns:
        List of Android devices with new attribtues.
    """
    droid_list = []
    for a in android_devices:
        d, e = a.droid, a.ed
        model = d.getBuildModel()
        max_advertisements = 1
        batch_scan_supported = True
        if model in advertisements_to_devices.keys():
            max_advertisements = advertisements_to_devices[model]
        else:
            max_advertisements = determine_max_advertisements(a)
            max_tries = 3
            # Retry to calculate max advertisements
            while max_advertisements == -1 and max_tries > 0:
                a.log.info(
                    "Attempts left to determine max advertisements: {}".format(
                        max_tries))
                max_advertisements = determine_max_advertisements(a)
                max_tries -= 1
            advertisements_to_devices[model] = max_advertisements
        if model in batch_scan_not_supported_list:
            batch_scan_supported = False
        role = {
            'droid': d,
            'ed': e,
            'max_advertisements': max_advertisements,
            'batch_scan_supported': batch_scan_supported
        }
        droid_list.append(role)
    return droid_list


def get_bluetooth_crash_count(android_device):
    out = android_device.adb.shell("dumpsys bluetooth_manager")
    return int(re.search("crashed(.*\d)", out).group(1))


def get_bt_metric(ad_list, duration=1, tag="bt_metric", processed=True):
    """ Function to get the bt metric from logcat.

    Captures logcat for the specified duration and returns the bqr results.
    Takes list of android objects as input. If a single android object is given,
    converts it into a list.

    Args:
        ad_list: list of android_device objects
        duration: time duration (seconds) for which the logcat is parsed.
        tag: tag to be appended to the logcat dump.
        processed: flag to process bqr output.

    Returns:
        metrics_dict: dict of metrics for each android device.
    """

    # Defining bqr quantitites and their regex to extract
    regex_dict = {"pwlv": "PwLv:\s(\S+)", "rssi": "RSSI:\s[-](\d+)"}
    metrics_dict = {"rssi": {}, "pwlv": {}}

    # Converting a single android device object to list
    if not isinstance(ad_list, list):
        ad_list = [ad_list]

    #Time sync with the test machine
    for ad in ad_list:
        ad.droid.setTime(int(round(time.time() * 1000)))
        time.sleep(0.5)

    begin_time = utils.get_current_epoch_time()
    time.sleep(duration)
    end_time = utils.get_current_epoch_time()

    for ad in ad_list:
        bt_rssi_log = ad.cat_adb_log(tag, begin_time, end_time)
        bqr_tag = "Monitoring , Handle:"

        # Extracting supporting bqr quantities
        for metric, regex in regex_dict.items():
            bqr_metric = []
            file_bt_log = open(bt_rssi_log, "r")
            for line in file_bt_log:
                if bqr_tag in line:
                    if re.findall(regex, line):
                        m = re.findall(regex, line)[0].strip(",")
                        bqr_metric.append(m)
            metrics_dict[metric][ad.serial] = bqr_metric

        # Formatting the raw data
        metrics_dict["rssi"][ad.serial] = [
            (-1) * int(x) for x in metrics_dict["rssi"][ad.serial]
        ]
        metrics_dict["pwlv"][ad.serial] = [
            int(x, 16) for x in metrics_dict["pwlv"][ad.serial]
        ]

        # Processing formatted data if processing is required
        if processed:
            # Computes the average RSSI
            metrics_dict["rssi"][ad.serial] = round(
                sum(metrics_dict["rssi"][ad.serial]) /
                len(metrics_dict["rssi"][ad.serial]), 2)
            # Returns last noted value for power level
            metrics_dict["pwlv"][ad.serial] = metrics_dict["pwlv"][
                ad.serial][-1]

    return metrics_dict


def get_bt_rssi(ad, duration=1, processed=True):
    """Function to get average bt rssi from logcat.

    This function returns the average RSSI for the given duration. RSSI values are
    extracted from BQR.

    Args:
        ad: (list of) android_device object.
        duration: time duration(seconds) for which logcat is parsed.

    Returns:
        avg_rssi: average RSSI on each android device for the given duration.
    """
    function_tag = "get_bt_rssi"
    bqr_results = get_bt_metric(ad,
                                duration,
                                tag=function_tag,
                                processed=processed)
    return bqr_results["rssi"]


def enable_bqr(
        ad_list,
        bqr_interval=10,
        bqr_event_mask=15,
):
    """Sets up BQR reporting.

       Sets up BQR to report BT metrics at the requested frequency and toggles
       airplane mode for the bqr settings to take effect.

    Args:
        ad_list: an android_device or list of android devices.
    """
    # Converting a single android device object to list
    if not isinstance(ad_list, list):
        ad_list = [ad_list]

    for ad in ad_list:
        #Setting BQR parameters
        ad.adb.shell("setprop persist.bluetooth.bqr.event_mask {}".format(
            bqr_event_mask))
        ad.adb.shell("setprop persist.bluetooth.bqr.min_interval_ms {}".format(
            bqr_interval))

        ## Toggle airplane mode
        ad.droid.connectivityToggleAirplaneMode(True)
        ad.droid.connectivityToggleAirplaneMode(False)


def disable_bqr(ad_list):
    """Disables BQR reporting.

    Args:
        ad_list: an android_device or list of android devices.
    """
    # Converting a single android device object to list
    if not isinstance(ad_list, list):
        ad_list = [ad_list]

    DISABLE_BQR_MASK = 0

    for ad in ad_list:
        #Disabling BQR
        ad.adb.shell("setprop persist.bluetooth.bqr.event_mask {}".format(
            DISABLE_BQR_MASK))

        ## Toggle airplane mode
        ad.droid.connectivityToggleAirplaneMode(True)
        ad.droid.connectivityToggleAirplaneMode(False)


def get_device_selector_dictionary(android_device_list):
    """Create a dictionary of Bluetooth features vs Android devices.

    Args:
        android_device_list: The list of Android devices.
    Returns:
        A dictionary of profiles/features to Android devices.
    """
    selector_dict = {}
    for ad in android_device_list:
        uuids = ad.droid.bluetoothGetLocalUuids()

        for profile, uuid_const in sig_uuid_constants.items():
            uuid_check = sig_uuid_constants['BASE_UUID'].format(
                uuid_const).lower()
            if uuids and uuid_check in uuids:
                if profile in selector_dict:
                    selector_dict[profile].append(ad)
                else:
                    selector_dict[profile] = [ad]

        # Various services may not be active during BT startup.
        # If the device can be identified through adb shell pm list features
        # then try to add them to the appropriate profiles / features.

        # Android TV.
        if "feature:com.google.android.tv.installed" in ad.features:
            ad.log.info("Android TV device found.")
            supported_profiles = ['AudioSink']
            _add_android_device_to_dictionary(ad, supported_profiles,
                                              selector_dict)

        # Android Auto
        elif "feature:android.hardware.type.automotive" in ad.features:
            ad.log.info("Android Auto device found.")
            # Add: AudioSink , A/V_RemoteControl,
            supported_profiles = [
                'AudioSink', 'A/V_RemoteControl', 'Message Notification Server'
            ]
            _add_android_device_to_dictionary(ad, supported_profiles,
                                              selector_dict)
        # Android Wear
        elif "feature:android.hardware.type.watch" in ad.features:
            ad.log.info("Android Wear device found.")
            supported_profiles = []
            _add_android_device_to_dictionary(ad, supported_profiles,
                                              selector_dict)
        # Android Phone
        elif "feature:android.hardware.telephony" in ad.features:
            ad.log.info("Android Phone device found.")
            # Add: AudioSink
            supported_profiles = [
                'AudioSource', 'A/V_RemoteControlTarget',
                'Message Access Server'
            ]
            _add_android_device_to_dictionary(ad, supported_profiles,
                                              selector_dict)
    return selector_dict


def get_mac_address_of_generic_advertisement(scan_ad, adv_ad):
    """Start generic advertisement and get it's mac address by LE scanning.

    Args:
        scan_ad: The Android device to use as the scanner.
        adv_ad: The Android device to use as the advertiser.

    Returns:
        mac_address: The mac address of the advertisement.
        advertise_callback: The advertise callback id of the active
            advertisement.
    """
    adv_ad.droid.bleSetAdvertiseDataIncludeDeviceName(True)
    adv_ad.droid.bleSetAdvertiseSettingsAdvertiseMode(
        ble_advertise_settings_modes['low_latency'])
    adv_ad.droid.bleSetAdvertiseSettingsIsConnectable(True)
    adv_ad.droid.bleSetAdvertiseSettingsTxPowerLevel(
        ble_advertise_settings_tx_powers['high'])
    advertise_callback, advertise_data, advertise_settings = (
        generate_ble_advertise_objects(adv_ad.droid))
    adv_ad.droid.bleStartBleAdvertising(advertise_callback, advertise_data,
                                        advertise_settings)
    try:
        adv_ad.ed.pop_event(adv_succ.format(advertise_callback),
                            bt_default_timeout)
    except Empty as err:
        raise BtTestUtilsError(
            "Advertiser did not start successfully {}".format(err))
    filter_list = scan_ad.droid.bleGenFilterList()
    scan_settings = scan_ad.droid.bleBuildScanSetting()
    scan_callback = scan_ad.droid.bleGenScanCallback()
    scan_ad.droid.bleSetScanFilterDeviceName(
        adv_ad.droid.bluetoothGetLocalName())
    scan_ad.droid.bleBuildScanFilter(filter_list)
    scan_ad.droid.bleStartBleScan(filter_list, scan_settings, scan_callback)
    try:
        event = scan_ad.ed.pop_event(
            "BleScan{}onScanResults".format(scan_callback), bt_default_timeout)
    except Empty as err:
        raise BtTestUtilsError(
            "Scanner did not find advertisement {}".format(err))
    mac_address = event['data']['Result']['deviceInfo']['address']
    return mac_address, advertise_callback, scan_callback


def hid_device_send_key_data_report(host_id, device_ad, key, interval=1):
    """Send a HID report simulating a 1-second keyboard press from host_ad to
    device_ad

    Args:
        host_id: the Bluetooth MAC address or name of the HID host
        device_ad: HID device
        key: the key we want to send
        interval: the interval between key press and key release
    """
    device_ad.droid.bluetoothHidDeviceSendReport(host_id, hid_id_keyboard,
                                                 hid_keyboard_report(key))
    time.sleep(interval)
    device_ad.droid.bluetoothHidDeviceSendReport(host_id, hid_id_keyboard,
                                                 hid_keyboard_report("00"))


def hid_keyboard_report(key, modifier="00"):
    """Get the HID keyboard report for the given key

    Args:
        key: the key we want
        modifier: HID keyboard modifier bytes
    Returns:
        The byte array for the HID report.
    """
    return str(
        bytearray.fromhex(" ".join(
            [modifier, "00", key, "00", "00", "00", "00", "00"])), "utf-8")


def is_a2dp_connected(sink, source):
    """
    Convenience Function to see if the 2 devices are connected on
    A2dp.
    Args:
        sink:       Audio Sink
        source:     Audio Source
    Returns:
        True if Connected
        False if Not connected
    """

    devices = sink.droid.bluetoothA2dpSinkGetConnectedDevices()
    for device in devices:
        sink.log.info("A2dp Connected device {}".format(device["name"]))
        if (device["address"] == source.droid.bluetoothGetLocalAddress()):
            return True
    return False


def is_a2dp_snk_device_connected(ad, addr):
    """Determines if an AndroidDevice has A2DP snk connectivity to input address

    Args:
        ad: the Android device
        addr: the address that's expected
    Returns:
        True if connection was successful, false if unsuccessful.
    """
    devices = ad.droid.bluetoothA2dpSinkGetConnectedDevices()
    ad.log.info("Connected A2DP Sink devices: {}".format(devices))
    if addr in {d['address'] for d in devices}:
        return True
    return False


def is_a2dp_src_device_connected(ad, addr):
    """Determines if an AndroidDevice has A2DP connectivity to input address

    Args:
        ad: the Android device
        addr: the address that's expected
    Returns:
        True if connection was successful, false if unsuccessful.
    """
    devices = ad.droid.bluetoothA2dpGetConnectedDevices()
    ad.log.info("Connected A2DP Source devices: {}".format(devices))
    if addr in {d['address'] for d in devices}:
        return True
    return False


def is_hfp_client_device_connected(ad, addr):
    """Determines if an AndroidDevice has HFP connectivity to input address

    Args:
        ad: the Android device
        addr: the address that's expected
    Returns:
        True if connection was successful, false if unsuccessful.
    """
    devices = ad.droid.bluetoothHfpClientGetConnectedDevices()
    ad.log.info("Connected HFP Client devices: {}".format(devices))
    if addr in {d['address'] for d in devices}:
        return True
    return False


def is_map_mce_device_connected(ad, addr):
    """Determines if an AndroidDevice has MAP MCE connectivity to input address

    Args:
        ad: the Android device
        addr: the address that's expected
    Returns:
        True if connection was successful, false if unsuccessful.
    """
    devices = ad.droid.bluetoothMapClientGetConnectedDevices()
    ad.log.info("Connected MAP MCE devices: {}".format(devices))
    if addr in {d['address'] for d in devices}:
        return True
    return False


def is_map_mse_device_connected(ad, addr):
    """Determines if an AndroidDevice has MAP MSE connectivity to input address

    Args:
        ad: the Android device
        addr: the address that's expected
    Returns:
        True if connection was successful, false if unsuccessful.
    """
    devices = ad.droid.bluetoothMapGetConnectedDevices()
    ad.log.info("Connected MAP MSE devices: {}".format(devices))
    if addr in {d['address'] for d in devices}:
        return True
    return False


def kill_bluetooth_process(ad):
    """Kill Bluetooth process on Android device.

    Args:
        ad: Android device to kill BT process on.
    """
    ad.log.info("Killing Bluetooth process.")
    pid = ad.adb.shell(
        "ps | grep com.android.bluetooth | awk '{print $2}'").decode('ascii')
    call(["adb -s " + ad.serial + " shell kill " + pid], shell=True)


def log_energy_info(android_devices, state):
    """Logs energy info of input Android devices.

    Args:
        android_devices: input Android device list to log energy info from.
        state: the input state to log. Usually 'Start' or 'Stop' for logging.

    Returns:
        A logging string of the Bluetooth energy info reported.
    """
    return_string = "{} Energy info collection:\n".format(state)
    # Bug: b/31966929
    return return_string


def orchestrate_and_verify_pan_connection(pan_dut, panu_dut):
    """Setups up a PAN conenction between two android devices.

    Args:
        pan_dut: the Android device providing tethering services
        panu_dut: the Android device using the internet connection from the
            pan_dut
    Returns:
        True if PAN connection and verification is successful,
        false if unsuccessful.
    """
    if not toggle_airplane_mode_by_adb(log, panu_dut, True):
        panu_dut.log.error("Failed to toggle airplane mode on")
        return False
    if not toggle_airplane_mode_by_adb(log, panu_dut, False):
        pan_dut.log.error("Failed to toggle airplane mode off")
        return False
    pan_dut.droid.bluetoothStartConnectionStateChangeMonitor("")
    panu_dut.droid.bluetoothStartConnectionStateChangeMonitor("")
    if not bluetooth_enabled_check(panu_dut):
        return False
    if not bluetooth_enabled_check(pan_dut):
        return False
    pan_dut.droid.bluetoothPanSetBluetoothTethering(True)
    if not (pair_pri_to_sec(pan_dut, panu_dut)):
        return False
    if not pan_dut.droid.bluetoothPanIsTetheringOn():
        pan_dut.log.error("Failed to enable Bluetooth tethering.")
        return False
    # Magic sleep needed to give the stack time in between bonding and
    # connecting the PAN profile.
    time.sleep(pan_connect_timeout)
    panu_dut.droid.bluetoothConnectBonded(
        pan_dut.droid.bluetoothGetLocalAddress())
    if not verify_http_connection(log, panu_dut):
        panu_dut.log.error("Can't verify http connection on PANU device.")
        if not verify_http_connection(log, pan_dut):
            pan_dut.log.info(
                "Can't verify http connection on PAN service device")
        return False
    return True


def orchestrate_bluetooth_socket_connection(
        client_ad,
        server_ad,
        accept_timeout_ms=default_bluetooth_socket_timeout_ms,
        uuid=None):
    """Sets up the Bluetooth Socket connection between two Android devices.

    Args:
        client_ad: the Android device performing the connection.
        server_ad: the Android device accepting the connection.
    Returns:
        True if connection was successful, false if unsuccessful.
    """
    server_ad.droid.bluetoothStartPairingHelper()
    client_ad.droid.bluetoothStartPairingHelper()

    server_ad.droid.bluetoothSocketConnBeginAcceptThreadUuid(
        (bluetooth_socket_conn_test_uuid if uuid is None else uuid),
        accept_timeout_ms)
    client_ad.droid.bluetoothSocketConnBeginConnectThreadUuid(
        server_ad.droid.bluetoothGetLocalAddress(),
        (bluetooth_socket_conn_test_uuid if uuid is None else uuid))

    end_time = time.time() + bt_default_timeout
    result = False
    test_result = True
    while time.time() < end_time:
        if len(client_ad.droid.bluetoothSocketConnActiveConnections()) > 0:
            test_result = True
            client_ad.log.info("Bluetooth socket Client Connection Active")
            break
        else:
            test_result = False
        time.sleep(1)
    if not test_result:
        client_ad.log.error(
            "Failed to establish a Bluetooth socket connection")
        return False
    return True


def orchestrate_rfcomm_connection(client_ad,
                                  server_ad,
                                  accept_timeout_ms=default_rfcomm_timeout_ms,
                                  uuid=None):
    """Sets up the RFCOMM connection between two Android devices.

    Args:
        client_ad: the Android device performing the connection.
        server_ad: the Android device accepting the connection.
    Returns:
        True if connection was successful, false if unsuccessful.
    """
    result = orchestrate_bluetooth_socket_connection(
        client_ad, server_ad, accept_timeout_ms,
        (bt_rfcomm_uuids['default_uuid'] if uuid is None else uuid))

    return result


def pair_pri_to_sec(pri_ad, sec_ad, attempts=2, auto_confirm=True):
    """Pairs pri droid to secondary droid.

    Args:
        pri_ad: Android device initiating connection
        sec_ad: Android device accepting connection
        attempts: Number of attempts to try until failure.
        auto_confirm: Auto confirm passkey match for both devices

    Returns:
        Pass if True
        Fail if False
    """
    pri_ad.droid.bluetoothStartConnectionStateChangeMonitor(
        sec_ad.droid.bluetoothGetLocalAddress())
    curr_attempts = 0
    while curr_attempts < attempts:
        if _pair_pri_to_sec(pri_ad, sec_ad, auto_confirm):
            return True
        # Wait 2 seconds before unbound
        time.sleep(2)
        if not clear_bonded_devices(pri_ad):
            log.error(
                "Failed to clear bond for primary device at attempt {}".format(
                    str(curr_attempts)))
            return False
        if not clear_bonded_devices(sec_ad):
            log.error(
                "Failed to clear bond for secondary device at attempt {}".
                format(str(curr_attempts)))
            return False
        # Wait 2 seconds after unbound
        time.sleep(2)
        curr_attempts += 1
    log.error("pair_pri_to_sec failed to connect after {} attempts".format(
        str(attempts)))
    return False


def _pair_pri_to_sec(pri_ad, sec_ad, auto_confirm):
    # Enable discovery on sec_ad so that pri_ad can find it.
    # The timeout here is based on how much time it would take for two devices
    # to pair with each other once pri_ad starts seeing devices.
    pri_droid = pri_ad.droid
    sec_droid = sec_ad.droid
    pri_ad.ed.clear_all_events()
    sec_ad.ed.clear_all_events()
    log.info("Bonding device {} to {}".format(
        pri_droid.bluetoothGetLocalAddress(),
        sec_droid.bluetoothGetLocalAddress()))
    sec_droid.bluetoothMakeDiscoverable(bt_default_timeout)
    target_address = sec_droid.bluetoothGetLocalAddress()
    log.debug("Starting paring helper on each device")
    pri_droid.bluetoothStartPairingHelper(auto_confirm)
    sec_droid.bluetoothStartPairingHelper(auto_confirm)
    pri_ad.log.info("Primary device starting discovery and executing bond")
    result = pri_droid.bluetoothDiscoverAndBond(target_address)
    if not auto_confirm:
        if not _wait_for_passkey_match(pri_ad, sec_ad):
            return False
    # Loop until we have bonded successfully or timeout.
    end_time = time.time() + bt_default_timeout
    pri_ad.log.info("Verifying devices are bonded")
    while time.time() < end_time:
        bonded_devices = pri_droid.bluetoothGetBondedDevices()
        bonded = False
        for d in bonded_devices:
            if d['address'] == target_address:
                pri_ad.log.info("Successfully bonded to device")
                return True
        time.sleep(0.1)
    # Timed out trying to bond.
    pri_ad.log.info("Failed to bond devices.")
    return False


def reset_bluetooth(android_devices):
    """Resets Bluetooth state of input Android device list.

    Args:
        android_devices: The Android device list to reset Bluetooth state on.

    Returns:
        True if successful, false if unsuccessful.
    """
    for a in android_devices:
        droid, ed = a.droid, a.ed
        a.log.info("Reset state of bluetooth on device.")
        if droid.bluetoothCheckState() is True:
            droid.bluetoothToggleState(False)
            expected_bluetooth_off_event_name = bluetooth_off
            try:
                ed.pop_event(expected_bluetooth_off_event_name,
                             bt_default_timeout)
            except Exception:
                a.log.error("Failed to toggle Bluetooth off.")
                return False
        # temp sleep for b/17723234
        time.sleep(3)
        if not bluetooth_enabled_check(a):
            return False
    return True


def scan_and_verify_n_advertisements(scn_ad, max_advertisements):
    """Verify that input number of advertisements can be found from the scanning
    Android device.

    Args:
        scn_ad: The Android device to start LE scanning on.
        max_advertisements: The number of advertisements the scanner expects to
        find.

    Returns:
        True if successful, false if unsuccessful.
    """
    test_result = False
    address_list = []
    filter_list = scn_ad.droid.bleGenFilterList()
    scn_ad.droid.bleBuildScanFilter(filter_list)
    scan_settings = scn_ad.droid.bleBuildScanSetting()
    scan_callback = scn_ad.droid.bleGenScanCallback()
    scn_ad.droid.bleStartBleScan(filter_list, scan_settings, scan_callback)
    start_time = time.time()
    while (start_time + bt_default_timeout) > time.time():
        event = None
        try:
            event = scn_ad.ed.pop_event(scan_result.format(scan_callback),
                                        bt_default_timeout)
        except Empty as error:
            raise BtTestUtilsError(
                "Failed to find scan event: {}".format(error))
        address = event['data']['Result']['deviceInfo']['address']
        if address not in address_list:
            address_list.append(address)
        if len(address_list) == max_advertisements:
            test_result = True
            break
    scn_ad.droid.bleStopBleScan(scan_callback)
    return test_result


def set_bluetooth_codec(android_device,
                        codec_type,
                        sample_rate,
                        bits_per_sample,
                        channel_mode,
                        codec_specific_1=0):
    """Sets the A2DP codec configuration on the AndroidDevice.

    Args:
        android_device (acts.controllers.android_device.AndroidDevice): the
            android device for which to switch the codec.
        codec_type (str): the desired codec type. Must be a key in
            bt_constants.codec_types.
        sample_rate (str): the desired sample rate. Must be a key in
            bt_constants.sample_rates.
        bits_per_sample (str): the desired bits per sample. Must be a key in
            bt_constants.bits_per_samples.
        channel_mode (str): the desired channel mode. Must be a key in
            bt_constants.channel_modes.
        codec_specific_1 (int): the desired bit rate (quality) for LDAC codec.
    Returns:
        bool: True if the codec config was successfully changed to the desired
            values. Else False.
    """
    message = ("Set Android Device A2DP Bluetooth codec configuration:\n"
               "\tCodec: {codec_type}\n"
               "\tSample Rate: {sample_rate}\n"
               "\tBits per Sample: {bits_per_sample}\n"
               "\tChannel Mode: {channel_mode}".format(
                   codec_type=codec_type,
                   sample_rate=sample_rate,
                   bits_per_sample=bits_per_sample,
                   channel_mode=channel_mode))
    android_device.log.info(message)

    # Send SL4A command
    droid, ed = android_device.droid, android_device.ed
    if not droid.bluetoothA2dpSetCodecConfigPreference(
            codec_types[codec_type], sample_rates[str(sample_rate)],
            bits_per_samples[str(bits_per_sample)],
            channel_modes[channel_mode], codec_specific_1):
        android_device.log.warning(
            "SL4A command returned False. Codec was not "
            "changed.")
    else:
        try:
            ed.pop_event(bluetooth_a2dp_codec_config_changed,
                         bt_default_timeout)
        except Exception:
            android_device.log.warning("SL4A event not registered. Codec "
                                       "may not have been changed.")

    # Validate codec value through ADB
    # TODO (aidanhb): validate codec more robustly using SL4A
    command = "dumpsys bluetooth_manager | grep -i 'current codec'"
    out = android_device.adb.shell(command)
    split_out = out.split(": ")
    if len(split_out) != 2:
        android_device.log.warning("Could not verify codec config change "
                                   "through ADB.")
    elif split_out[1].strip().upper() != codec_type:
        android_device.log.error("Codec config was not changed.\n"
                                 "\tExpected codec: {exp}\n"
                                 "\tActual codec: {act}".format(
                                     exp=codec_type, act=split_out[1].strip()))
        return False
    android_device.log.info("Bluetooth codec successfully changed.")
    return True


def set_bt_scan_mode(ad, scan_mode_value):
    """Set Android device's Bluetooth scan mode.

    Args:
        ad: The Android device to set the scan mode on.
        scan_mode_value: The value to set the scan mode to.

    Returns:
        True if successful, false if unsuccessful.
    """
    droid, ed = ad.droid, ad.ed
    if scan_mode_value == bt_scan_mode_types['state_off']:
        disable_bluetooth(droid)
        scan_mode = droid.bluetoothGetScanMode()
        reset_bluetooth([ad])
        if scan_mode != scan_mode_value:
            return False
    elif scan_mode_value == bt_scan_mode_types['none']:
        droid.bluetoothMakeUndiscoverable()
        scan_mode = droid.bluetoothGetScanMode()
        if scan_mode != scan_mode_value:
            return False
    elif scan_mode_value == bt_scan_mode_types['connectable']:
        droid.bluetoothMakeUndiscoverable()
        droid.bluetoothMakeConnectable()
        scan_mode = droid.bluetoothGetScanMode()
        if scan_mode != scan_mode_value:
            return False
    elif (scan_mode_value == bt_scan_mode_types['connectable_discoverable']):
        droid.bluetoothMakeDiscoverable()
        scan_mode = droid.bluetoothGetScanMode()
        if scan_mode != scan_mode_value:
            return False
    else:
        # invalid scan mode
        return False
    return True


def set_device_name(droid, name):
    """Set and check Bluetooth local name on input droid object.

    Args:
        droid: Droid object to set local name on.
        name: the Bluetooth local name to set.

    Returns:
        True if successful, false if unsuccessful.
    """
    droid.bluetoothSetLocalName(name)
    time.sleep(2)
    droid_name = droid.bluetoothGetLocalName()
    if droid_name != name:
        return False
    return True


def set_profile_priority(host_ad, client_ad, profiles, priority):
    """Sets the priority of said profile(s) on host_ad for client_ad"""
    for profile in profiles:
        host_ad.log.info("Profile {} on {} for {} set to priority {}".format(
            profile, host_ad.droid.bluetoothGetLocalName(),
            client_ad.droid.bluetoothGetLocalAddress(), priority.value))
        if bt_profile_constants['a2dp_sink'] == profile:
            host_ad.droid.bluetoothA2dpSinkSetPriority(
                client_ad.droid.bluetoothGetLocalAddress(), priority.value)
        elif bt_profile_constants['headset_client'] == profile:
            host_ad.droid.bluetoothHfpClientSetPriority(
                client_ad.droid.bluetoothGetLocalAddress(), priority.value)
        elif bt_profile_constants['pbap_client'] == profile:
            host_ad.droid.bluetoothPbapClientSetPriority(
                client_ad.droid.bluetoothGetLocalAddress(), priority.value)
        else:
            host_ad.log.error(
                "Profile {} not yet supported for priority settings".format(
                    profile))


def setup_multiple_devices_for_bt_test(android_devices):
    """A common setup routine for Bluetooth on input Android device list.

    Things this function sets up:
    1. Resets Bluetooth
    2. Set Bluetooth local name to random string of size 4
    3. Disable BLE background scanning.
    4. Enable Bluetooth snoop logging.

    Args:
        android_devices: Android device list to setup Bluetooth on.

    Returns:
        True if successful, false if unsuccessful.
    """
    log.info("Setting up Android Devices")
    # TODO: Temp fix for an selinux error.
    for ad in android_devices:
        ad.adb.shell("setenforce 0")
    threads = []
    try:
        for a in android_devices:
            thread = threading.Thread(target=factory_reset_bluetooth,
                                      args=([[a]]))
            threads.append(thread)
            thread.start()
        for t in threads:
            t.join()

        for a in android_devices:
            d = a.droid
            # TODO: Create specific RPC command to instantiate
            # BluetoothConnectionFacade. This is just a workaround.
            d.bluetoothStartConnectionStateChangeMonitor("")
            setup_result = d.bluetoothSetLocalName(generate_id_by_size(4))
            if not setup_result:
                a.log.error("Failed to set device name.")
                return setup_result
            d.bluetoothDisableBLE()
            utils.set_location_service(a, True)
            bonded_devices = d.bluetoothGetBondedDevices()
            for b in bonded_devices:
                a.log.info("Removing bond for device {}".format(b['address']))
                d.bluetoothUnbond(b['address'])
        for a in android_devices:
            a.adb.shell("setprop persist.bluetooth.btsnooplogmode full")
            getprop_result = a.adb.shell(
                "getprop persist.bluetooth.btsnooplogmode") == "full"
            if not getprop_result:
                a.log.warning("Failed to enable Bluetooth Hci Snoop Logging.")
    except Exception as err:
        log.error("Something went wrong in multi device setup: {}".format(err))
        return False
    return setup_result


def setup_n_advertisements(adv_ad, num_advertisements):
    """Setup input number of advertisements on input Android device.

    Args:
        adv_ad: The Android device to start LE advertisements on.
        num_advertisements: The number of advertisements to start.

    Returns:
        advertise_callback_list: List of advertisement callback ids.
    """
    adv_ad.droid.bleSetAdvertiseSettingsAdvertiseMode(
        ble_advertise_settings_modes['low_latency'])
    advertise_data = adv_ad.droid.bleBuildAdvertiseData()
    advertise_settings = adv_ad.droid.bleBuildAdvertiseSettings()
    advertise_callback_list = []
    for i in range(num_advertisements):
        advertise_callback = adv_ad.droid.bleGenBleAdvertiseCallback()
        advertise_callback_list.append(advertise_callback)
        adv_ad.droid.bleStartBleAdvertising(advertise_callback, advertise_data,
                                            advertise_settings)
        try:
            adv_ad.ed.pop_event(adv_succ.format(advertise_callback),
                                bt_default_timeout)
            adv_ad.log.info("Advertisement {} started.".format(i + 1))
        except Empty as error:
            adv_ad.log.error("Advertisement {} failed to start.".format(i + 1))
            raise BtTestUtilsError(
                "Test failed with Empty error: {}".format(error))
    return advertise_callback_list


def take_btsnoop_log(ad, testcase, testname):
    """Grabs the btsnoop_hci log on a device and stores it in the log directory
    of the test class.

    If you want grab the btsnoop_hci log, call this function with android_device
    objects in on_fail. Bug report takes a relative long time to take, so use
    this cautiously.

    Args:
        ad: The android_device instance to take bugreport on.
        testcase: Name of the test calss that triggered this snoop log.
        testname: Name of the test case that triggered this bug report.
    """
    testname = "".join(x for x in testname if x.isalnum())
    serial = ad.serial
    device_model = ad.droid.getBuildModel()
    device_model = device_model.replace(" ", "")
    out_name = ','.join((testname, device_model, serial))
    snoop_path = os.path.join(ad.device_log_path, 'BluetoothSnoopLogs')
    os.makedirs(snoop_path, exist_ok=True)
    cmd = ''.join(("adb -s ", serial, " pull ", btsnoop_log_path_on_device,
                   " ", snoop_path + '/' + out_name, ".btsnoop_hci.log"))
    exe_cmd(cmd)
    try:
        cmd = ''.join(
            ("adb -s ", serial, " pull ", btsnoop_last_log_path_on_device, " ",
             snoop_path + '/' + out_name, ".btsnoop_hci.log.last"))
        exe_cmd(cmd)
    except Exception as err:
        testcase.log.info(
            "File does not exist {}".format(btsnoop_last_log_path_on_device))


def take_btsnoop_logs(android_devices, testcase, testname):
    """Pull btsnoop logs from an input list of android devices.

    Args:
        android_devices: the list of Android devices to pull btsnoop logs from.
        testcase: Name of the test calss that triggered this snoop log.
        testname: Name of the test case that triggered this bug report.
    """
    for a in android_devices:
        take_btsnoop_log(a, testcase, testname)


def teardown_n_advertisements(adv_ad, num_advertisements,
                              advertise_callback_list):
    """Stop input number of advertisements on input Android device.

    Args:
        adv_ad: The Android device to stop LE advertisements on.
        num_advertisements: The number of advertisements to stop.
        advertise_callback_list: The list of advertisement callbacks to stop.

    Returns:
        True if successful, false if unsuccessful.
    """
    for n in range(num_advertisements):
        adv_ad.droid.bleStopBleAdvertising(advertise_callback_list[n])
    return True


def verify_server_and_client_connected(client_ad, server_ad, log=True):
    """Verify that input server and client Android devices are connected.

    This code is under the assumption that there will only be
    a single connection.

    Args:
        client_ad: the Android device to check number of active connections.
        server_ad: the Android device to check number of active connections.

    Returns:
        True both server and client have at least 1 active connection,
        false if unsuccessful.
    """
    test_result = True
    if len(server_ad.droid.bluetoothSocketConnActiveConnections()) == 0:
        if log:
            server_ad.log.error("No socket connections found on server.")
        test_result = False
    if len(client_ad.droid.bluetoothSocketConnActiveConnections()) == 0:
        if log:
            client_ad.log.error("No socket connections found on client.")
        test_result = False
    return test_result


def wait_for_bluetooth_manager_state(droid,
                                     state=None,
                                     timeout=10,
                                     threshold=5):
    """ Waits for BlueTooth normalized state or normalized explicit state
    args:
        droid: droid device object
        state: expected BlueTooth state
        timeout: max timeout threshold
        threshold: list len of bt state
    Returns:
        True if successful, false if unsuccessful.
    """
    all_states = []
    get_state = lambda: droid.bluetoothGetLeState()
    start_time = time.time()
    while time.time() < start_time + timeout:
        all_states.append(get_state())
        if len(all_states) >= threshold:
            # for any normalized state
            if state is None:
                if len(set(all_states[-threshold:])) == 1:
                    log.info("State normalized {}".format(
                        set(all_states[-threshold:])))
                    return True
            else:
                # explicit check against normalized state
                if set([state]).issubset(all_states[-threshold:]):
                    return True
        time.sleep(0.5)
    log.error(
        "Bluetooth state fails to normalize" if state is None else
        "Failed to match bluetooth state, current state {} expected state {}".
        format(get_state(), state))
    return False


def _wait_for_passkey_match(pri_ad, sec_ad):
    pri_pin, sec_pin = -1, 1
    pri_variant, sec_variant = -1, 1
    pri_pairing_req, sec_pairing_req = None, None
    try:
        pri_pairing_req = pri_ad.ed.pop_event(
            event_name="BluetoothActionPairingRequest",
            timeout=bt_default_timeout)
        pri_variant = pri_pairing_req["data"]["PairingVariant"]
        pri_pin = pri_pairing_req["data"]["Pin"]
        pri_ad.log.info("Primary device received Pin: {}, Variant: {}".format(
            pri_pin, pri_variant))
        sec_pairing_req = sec_ad.ed.pop_event(
            event_name="BluetoothActionPairingRequest",
            timeout=bt_default_timeout)
        sec_variant = sec_pairing_req["data"]["PairingVariant"]
        sec_pin = sec_pairing_req["data"]["Pin"]
        sec_ad.log.info(
            "Secondary device received Pin: {}, Variant: {}".format(
                sec_pin, sec_variant))
    except Empty as err:
        log.error("Wait for pin error: {}".format(err))
        log.error("Pairing request state, Primary: {}, Secondary: {}".format(
            pri_pairing_req, sec_pairing_req))
        return False
    if pri_variant == sec_variant == pairing_variant_passkey_confirmation:
        confirmation = pri_pin == sec_pin
        if confirmation:
            log.info("Pairing code matched, accepting connection")
        else:
            log.info("Pairing code mismatched, rejecting connection")
        pri_ad.droid.eventPost("BluetoothActionPairingRequestUserConfirm",
                               str(confirmation))
        sec_ad.droid.eventPost("BluetoothActionPairingRequestUserConfirm",
                               str(confirmation))
        if not confirmation:
            return False
    elif pri_variant != sec_variant:
        log.error("Pairing variant mismatched, abort connection")
        return False
    return True


def write_read_verify_data(client_ad, server_ad, msg, binary=False):
    """Verify that the client wrote data to the server Android device correctly.

    Args:
        client_ad: the Android device to perform the write.
        server_ad: the Android device to read the data written.
        msg: the message to write.
        binary: if the msg arg is binary or not.

    Returns:
        True if the data written matches the data read, false if not.
    """
    client_ad.log.info("Write message.")
    try:
        if binary:
            client_ad.droid.bluetoothSocketConnWriteBinary(msg)
        else:
            client_ad.droid.bluetoothSocketConnWrite(msg)
    except Exception as err:
        client_ad.log.error("Failed to write data: {}".format(err))
        return False
    server_ad.log.info("Read message.")
    try:
        if binary:
            read_msg = server_ad.droid.bluetoothSocketConnReadBinary().rstrip(
                "\r\n")
        else:
            read_msg = server_ad.droid.bluetoothSocketConnRead()
    except Exception as err:
        server_ad.log.error("Failed to read data: {}".format(err))
        return False
    log.info("Verify message.")
    if msg != read_msg:
        log.error("Mismatch! Read: {}, Expected: {}".format(read_msg, msg))
        return False
    return True


class MediaControlOverSl4a(object):
    """Media control using sl4a facade for general purpose.

    """
    def __init__(self, android_device, music_file):
        """Initialize the media_control class.

        Args:
            android_dut: android_device object
            music_file: location of the music file
        """
        self.android_device = android_device
        self.music_file = music_file

    def play(self):
        """Play media.

        """
        self.android_device.droid.mediaPlayOpen('file://%s' % self.music_file,
                                                'default', True)
        playing = self.android_device.droid.mediaIsPlaying()
        asserts.assert_true(playing,
                            'Failed to play music %s' % self.music_file)

    def pause(self):
        """Pause media.

        """
        self.android_device.droid.mediaPlayPause('default')
        paused = not self.android_device.droid.mediaIsPlaying()
        asserts.assert_true(paused,
                            'Failed to pause music %s' % self.music_file)

    def resume(self):
        """Resume media.

        """
        self.android_device.droid.mediaPlayStart('default')
        playing = self.android_device.droid.mediaIsPlaying()
        asserts.assert_true(playing,
                            'Failed to play music %s' % self.music_file)

    def stop(self):
        """Stop media.

        """
        self.android_device.droid.mediaPlayStop('default')
        stopped = not self.android_device.droid.mediaIsPlaying()
        asserts.assert_true(stopped,
                            'Failed to stop music %s' % self.music_file)
