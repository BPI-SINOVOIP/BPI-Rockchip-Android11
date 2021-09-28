/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package android.cts.statsd.atom;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import android.os.BatteryPluggedStateEnum;
import android.os.BatteryStatusEnum;
import android.platform.test.annotations.RestrictedBuildTest;
import android.server.DeviceIdleModeEnum;
import android.view.DisplayStateEnum;
import android.telephony.NetworkTypeEnum;

import com.android.internal.os.StatsdConfigProto.StatsdConfig;
import com.android.os.AtomsProto.AppBreadcrumbReported;
import com.android.os.AtomsProto.Atom;
import com.android.os.AtomsProto.BatterySaverModeStateChanged;
import com.android.os.AtomsProto.BuildInformation;
import com.android.os.AtomsProto.ConnectivityStateChanged;
import com.android.os.AtomsProto.SimSlotState;
import com.android.os.AtomsProto.SupportedRadioAccessFamily;
import com.android.os.StatsLog.ConfigMetricsReportList;
import com.android.os.StatsLog.EventMetricData;

import com.google.common.collect.Range;

import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Statsd atom tests that are done via adb (hostside).
 */
public class HostAtomTests extends AtomTestCase {

    private static final String TAG = "Statsd.HostAtomTests";

    // Either file must exist to read kernel wake lock stats.
    private static final String WAKE_LOCK_FILE = "/proc/wakelocks";
    private static final String WAKE_SOURCES_FILE = "/d/wakeup_sources";

    // Bitmask of radio access technologies that all GSM phones should at least partially support
    protected static final long NETWORK_TYPE_BITMASK_GSM_ALL =
            (1 << (NetworkTypeEnum.NETWORK_TYPE_GSM_VALUE - 1))
            | (1 << (NetworkTypeEnum.NETWORK_TYPE_GPRS_VALUE - 1))
            | (1 << (NetworkTypeEnum.NETWORK_TYPE_EDGE_VALUE - 1))
            | (1 << (NetworkTypeEnum.NETWORK_TYPE_UMTS_VALUE - 1))
            | (1 << (NetworkTypeEnum.NETWORK_TYPE_HSDPA_VALUE - 1))
            | (1 << (NetworkTypeEnum.NETWORK_TYPE_HSUPA_VALUE - 1))
            | (1 << (NetworkTypeEnum.NETWORK_TYPE_HSPA_VALUE - 1))
            | (1 << (NetworkTypeEnum.NETWORK_TYPE_HSPAP_VALUE - 1))
            | (1 << (NetworkTypeEnum.NETWORK_TYPE_TD_SCDMA_VALUE - 1))
            | (1 << (NetworkTypeEnum.NETWORK_TYPE_LTE_VALUE - 1))
            | (1 << (NetworkTypeEnum.NETWORK_TYPE_LTE_CA_VALUE - 1))
            | (1 << (NetworkTypeEnum.NETWORK_TYPE_NR_VALUE - 1));
    // Bitmask of radio access technologies that all CDMA phones should at least partially support
    protected static final long NETWORK_TYPE_BITMASK_CDMA_ALL =
            (1 << (NetworkTypeEnum.NETWORK_TYPE_CDMA_VALUE - 1))
            | (1 << (NetworkTypeEnum.NETWORK_TYPE_1XRTT_VALUE - 1))
            | (1 << (NetworkTypeEnum.NETWORK_TYPE_EVDO_0_VALUE - 1))
            | (1 << (NetworkTypeEnum.NETWORK_TYPE_EVDO_A_VALUE - 1))
            | (1 << (NetworkTypeEnum.NETWORK_TYPE_EHRPD_VALUE - 1));

    @Override
    protected void setUp() throws Exception {
        super.setUp();
    }

    public void testScreenStateChangedAtom() throws Exception {
        // Setup, make sure the screen is off and turn off AoD if it is on.
        // AoD needs to be turned off because the screen should go into an off state. But, if AoD is
        // on and the device doesn't support STATE_DOZE, the screen sadly goes back to STATE_ON.
        String aodState = getAodState();
        setAodState("0");
        turnScreenOn();
        Thread.sleep(WAIT_TIME_SHORT);
        turnScreenOff();
        Thread.sleep(WAIT_TIME_SHORT);

        final int atomTag = Atom.SCREEN_STATE_CHANGED_FIELD_NUMBER;

        Set<Integer> screenOnStates = new HashSet<>(
                Arrays.asList(DisplayStateEnum.DISPLAY_STATE_ON_VALUE,
                        DisplayStateEnum.DISPLAY_STATE_ON_SUSPEND_VALUE,
                        DisplayStateEnum.DISPLAY_STATE_VR_VALUE));
        Set<Integer> screenOffStates = new HashSet<>(
                Arrays.asList(DisplayStateEnum.DISPLAY_STATE_OFF_VALUE,
                        DisplayStateEnum.DISPLAY_STATE_DOZE_VALUE,
                        DisplayStateEnum.DISPLAY_STATE_DOZE_SUSPEND_VALUE,
                        DisplayStateEnum.DISPLAY_STATE_UNKNOWN_VALUE));

        // Add state sets to the list in order.
        List<Set<Integer>> stateSet = Arrays.asList(screenOnStates, screenOffStates);

        createAndUploadConfig(atomTag);
        Thread.sleep(WAIT_TIME_SHORT);

        // Trigger events in same order.
        turnScreenOn();
        Thread.sleep(WAIT_TIME_LONG);
        turnScreenOff();
        Thread.sleep(WAIT_TIME_LONG);

        // Sorted list of events in order in which they occurred.
        List<EventMetricData> data = getEventMetricDataList();
        // reset screen to on
        turnScreenOn();
        // Restores AoD to initial state.
        setAodState(aodState);
        // Assert that the events happened in the expected order.
        assertStatesOccurred(stateSet, data, WAIT_TIME_LONG,
                atom -> atom.getScreenStateChanged().getState().getNumber());
    }

    public void testChargingStateChangedAtom() throws Exception {
        if (!hasFeature(FEATURE_AUTOMOTIVE, false)) return;
        // Setup, set charging state to full.
        setChargingState(5);
        Thread.sleep(WAIT_TIME_SHORT);

        final int atomTag = Atom.CHARGING_STATE_CHANGED_FIELD_NUMBER;

        Set<Integer> batteryUnknownStates = new HashSet<>(
                Arrays.asList(BatteryStatusEnum.BATTERY_STATUS_UNKNOWN_VALUE));
        Set<Integer> batteryChargingStates = new HashSet<>(
                Arrays.asList(BatteryStatusEnum.BATTERY_STATUS_CHARGING_VALUE));
        Set<Integer> batteryDischargingStates = new HashSet<>(
                Arrays.asList(BatteryStatusEnum.BATTERY_STATUS_DISCHARGING_VALUE));
        Set<Integer> batteryNotChargingStates = new HashSet<>(
                Arrays.asList(BatteryStatusEnum.BATTERY_STATUS_NOT_CHARGING_VALUE));
        Set<Integer> batteryFullStates = new HashSet<>(
                Arrays.asList(BatteryStatusEnum.BATTERY_STATUS_FULL_VALUE));

        // Add state sets to the list in order.
        List<Set<Integer>> stateSet = Arrays.asList(batteryUnknownStates, batteryChargingStates,
                batteryDischargingStates, batteryNotChargingStates, batteryFullStates);

        createAndUploadConfig(atomTag);
        Thread.sleep(WAIT_TIME_SHORT);

        // Trigger events in same order.
        setChargingState(1);
        Thread.sleep(WAIT_TIME_SHORT);
        setChargingState(2);
        Thread.sleep(WAIT_TIME_SHORT);
        setChargingState(3);
        Thread.sleep(WAIT_TIME_SHORT);
        setChargingState(4);
        Thread.sleep(WAIT_TIME_SHORT);
        setChargingState(5);
        Thread.sleep(WAIT_TIME_SHORT);

        // Sorted list of events in order in which they occurred.
        List<EventMetricData> data = getEventMetricDataList();

        // Unfreeze battery state after test
        resetBatteryStatus();
        Thread.sleep(WAIT_TIME_SHORT);

        // Assert that the events happened in the expected order.
        assertStatesOccurred(stateSet, data, WAIT_TIME_SHORT,
                atom -> atom.getChargingStateChanged().getState().getNumber());
    }

    public void testPluggedStateChangedAtom() throws Exception {
        if (!hasFeature(FEATURE_AUTOMOTIVE, false)) return;
        // Setup, unplug device.
        unplugDevice();
        Thread.sleep(WAIT_TIME_SHORT);

        final int atomTag = Atom.PLUGGED_STATE_CHANGED_FIELD_NUMBER;

        Set<Integer> unpluggedStates = new HashSet<>(
                Arrays.asList(BatteryPluggedStateEnum.BATTERY_PLUGGED_NONE_VALUE));
        Set<Integer> acStates = new HashSet<>(
                Arrays.asList(BatteryPluggedStateEnum.BATTERY_PLUGGED_AC_VALUE));
        Set<Integer> usbStates = new HashSet<>(
                Arrays.asList(BatteryPluggedStateEnum.BATTERY_PLUGGED_USB_VALUE));
        Set<Integer> wirelessStates = new HashSet<>(
                Arrays.asList(BatteryPluggedStateEnum.BATTERY_PLUGGED_WIRELESS_VALUE));

        // Add state sets to the list in order.
        List<Set<Integer>> stateSet = Arrays.asList(acStates, unpluggedStates, usbStates,
                unpluggedStates, wirelessStates, unpluggedStates);

        createAndUploadConfig(atomTag);
        Thread.sleep(WAIT_TIME_SHORT);

        // Trigger events in same order.
        plugInAc();
        Thread.sleep(WAIT_TIME_SHORT);
        unplugDevice();
        Thread.sleep(WAIT_TIME_SHORT);
        plugInUsb();
        Thread.sleep(WAIT_TIME_SHORT);
        unplugDevice();
        Thread.sleep(WAIT_TIME_SHORT);
        plugInWireless();
        Thread.sleep(WAIT_TIME_SHORT);
        unplugDevice();
        Thread.sleep(WAIT_TIME_SHORT);

        // Sorted list of events in order in which they occurred.
        List<EventMetricData> data = getEventMetricDataList();

        // Unfreeze battery state after test
        resetBatteryStatus();
        Thread.sleep(WAIT_TIME_SHORT);

        // Assert that the events happened in the expected order.
        assertStatesOccurred(stateSet, data, WAIT_TIME_SHORT,
                atom -> atom.getPluggedStateChanged().getState().getNumber());
    }

    public void testBatteryLevelChangedAtom() throws Exception {
        if (!hasFeature(FEATURE_AUTOMOTIVE, false)) return;
        // Setup, set battery level to full.
        setBatteryLevel(100);
        Thread.sleep(WAIT_TIME_SHORT);

        final int atomTag = Atom.BATTERY_LEVEL_CHANGED_FIELD_NUMBER;

        Set<Integer> batteryLow = new HashSet<>(Arrays.asList(2));
        Set<Integer> battery25p = new HashSet<>(Arrays.asList(25));
        Set<Integer> battery50p = new HashSet<>(Arrays.asList(50));
        Set<Integer> battery75p = new HashSet<>(Arrays.asList(75));
        Set<Integer> batteryFull = new HashSet<>(Arrays.asList(100));

        // Add state sets to the list in order.
        List<Set<Integer>> stateSet = Arrays.asList(batteryLow, battery25p, battery50p,
                battery75p, batteryFull);

        createAndUploadConfig(atomTag);
        Thread.sleep(WAIT_TIME_SHORT);

        // Trigger events in same order.
        setBatteryLevel(2);
        Thread.sleep(WAIT_TIME_SHORT);
        setBatteryLevel(25);
        Thread.sleep(WAIT_TIME_SHORT);
        setBatteryLevel(50);
        Thread.sleep(WAIT_TIME_SHORT);
        setBatteryLevel(75);
        Thread.sleep(WAIT_TIME_SHORT);
        setBatteryLevel(100);
        Thread.sleep(WAIT_TIME_SHORT);

        // Sorted list of events in order in which they occurred.
        List<EventMetricData> data = getEventMetricDataList();

        // Unfreeze battery state after test
        resetBatteryStatus();
        Thread.sleep(WAIT_TIME_SHORT);

        // Assert that the events happened in the expected order.
        assertStatesOccurred(stateSet, data, WAIT_TIME_SHORT,
                atom -> atom.getBatteryLevelChanged().getBatteryLevel());
    }

    public void testDeviceIdleModeStateChangedAtom() throws Exception {
        // Setup, leave doze mode.
        leaveDozeMode();
        Thread.sleep(WAIT_TIME_SHORT);

        final int atomTag = Atom.DEVICE_IDLE_MODE_STATE_CHANGED_FIELD_NUMBER;

        Set<Integer> dozeOff = new HashSet<>(
                Arrays.asList(DeviceIdleModeEnum.DEVICE_IDLE_MODE_OFF_VALUE));
        Set<Integer> dozeLight = new HashSet<>(
                Arrays.asList(DeviceIdleModeEnum.DEVICE_IDLE_MODE_LIGHT_VALUE));
        Set<Integer> dozeDeep = new HashSet<>(
                Arrays.asList(DeviceIdleModeEnum.DEVICE_IDLE_MODE_DEEP_VALUE));

        // Add state sets to the list in order.
        List<Set<Integer>> stateSet = Arrays.asList(dozeLight, dozeDeep, dozeOff);

        createAndUploadConfig(atomTag);
        Thread.sleep(WAIT_TIME_SHORT);

        // Trigger events in same order.
        enterDozeModeLight();
        Thread.sleep(WAIT_TIME_SHORT);
        enterDozeModeDeep();
        Thread.sleep(WAIT_TIME_SHORT);
        leaveDozeMode();
        Thread.sleep(WAIT_TIME_SHORT);

        // Sorted list of events in order in which they occurred.
        List<EventMetricData> data = getEventMetricDataList();;

        // Assert that the events happened in the expected order.
        assertStatesOccurred(stateSet, data, WAIT_TIME_SHORT,
                atom -> atom.getDeviceIdleModeStateChanged().getState().getNumber());
    }

    public void testBatterySaverModeStateChangedAtom() throws Exception {
        if (!hasFeature(FEATURE_AUTOMOTIVE, false)) return;
        // Setup, turn off battery saver.
        turnBatterySaverOff();
        Thread.sleep(WAIT_TIME_SHORT);

        final int atomTag = Atom.BATTERY_SAVER_MODE_STATE_CHANGED_FIELD_NUMBER;

        Set<Integer> batterySaverOn = new HashSet<>(
                Arrays.asList(BatterySaverModeStateChanged.State.ON_VALUE));
        Set<Integer> batterySaverOff = new HashSet<>(
                Arrays.asList(BatterySaverModeStateChanged.State.OFF_VALUE));

        // Add state sets to the list in order.
        List<Set<Integer>> stateSet = Arrays.asList(batterySaverOn, batterySaverOff);

        createAndUploadConfig(atomTag);
        Thread.sleep(WAIT_TIME_SHORT);

        // Trigger events in same order.
        turnBatterySaverOn();
        Thread.sleep(WAIT_TIME_LONG);
        turnBatterySaverOff();
        Thread.sleep(WAIT_TIME_LONG);

        // Sorted list of events in order in which they occurred.
        List<EventMetricData> data = getEventMetricDataList();

        // Assert that the events happened in the expected order.
        assertStatesOccurred(stateSet, data, WAIT_TIME_LONG,
                atom -> atom.getBatterySaverModeStateChanged().getState().getNumber());
    }

    @RestrictedBuildTest
    public void testRemainingBatteryCapacity() throws Exception {
        if (!hasFeature(FEATURE_WATCH, false)) return;
        if (!hasFeature(FEATURE_AUTOMOTIVE, false)) return;
        StatsdConfig.Builder config = createConfigBuilder();
        addGaugeAtomWithDimensions(config, Atom.REMAINING_BATTERY_CAPACITY_FIELD_NUMBER, null);

        uploadConfig(config);

        Thread.sleep(WAIT_TIME_LONG);
        setAppBreadcrumbPredicate();
        Thread.sleep(WAIT_TIME_LONG);

        List<Atom> data = getGaugeMetricDataList();

        assertThat(data).isNotEmpty();
        Atom atom = data.get(0);
        assertThat(atom.getRemainingBatteryCapacity().hasChargeMicroAmpereHour()).isTrue();
        if (hasBattery()) {
            assertThat(atom.getRemainingBatteryCapacity().getChargeMicroAmpereHour())
                .isGreaterThan(0);
        }
    }

    @RestrictedBuildTest
    public void testFullBatteryCapacity() throws Exception {
        if (!hasFeature(FEATURE_WATCH, false)) return;
        if (!hasFeature(FEATURE_AUTOMOTIVE, false)) return;
        StatsdConfig.Builder config = createConfigBuilder();
        addGaugeAtomWithDimensions(config, Atom.FULL_BATTERY_CAPACITY_FIELD_NUMBER, null);

        uploadConfig(config);

        Thread.sleep(WAIT_TIME_LONG);
        setAppBreadcrumbPredicate();
        Thread.sleep(WAIT_TIME_LONG);

        List<Atom> data = getGaugeMetricDataList();

        assertThat(data).isNotEmpty();
        Atom atom = data.get(0);
        assertThat(atom.getFullBatteryCapacity().hasCapacityMicroAmpereHour()).isTrue();
        if (hasBattery()) {
            assertThat(atom.getFullBatteryCapacity().getCapacityMicroAmpereHour()).isGreaterThan(0);
        }
    }

    public void testBatteryVoltage() throws Exception {
        if (!hasFeature(FEATURE_WATCH, false)) return;
        StatsdConfig.Builder config = createConfigBuilder();
        addGaugeAtomWithDimensions(config, Atom.BATTERY_VOLTAGE_FIELD_NUMBER, null);

        uploadConfig(config);

        Thread.sleep(WAIT_TIME_LONG);
        setAppBreadcrumbPredicate();
        Thread.sleep(WAIT_TIME_LONG);

        List<Atom> data = getGaugeMetricDataList();

        assertThat(data).isNotEmpty();
        Atom atom = data.get(0);
        assertThat(atom.getBatteryVoltage().hasVoltageMillivolt()).isTrue();
        if (hasBattery()) {
            assertThat(atom.getBatteryVoltage().getVoltageMillivolt()).isGreaterThan(0);
        }
    }

    // This test is for the pulled battery level atom.
    public void testBatteryLevel() throws Exception {
        if (!hasFeature(FEATURE_WATCH, false)) return;
        StatsdConfig.Builder config = createConfigBuilder();
        addGaugeAtomWithDimensions(config, Atom.BATTERY_LEVEL_FIELD_NUMBER, null);

        uploadConfig(config);

        Thread.sleep(WAIT_TIME_LONG);
        setAppBreadcrumbPredicate();
        Thread.sleep(WAIT_TIME_LONG);

        List<Atom> data = getGaugeMetricDataList();

        assertThat(data).isNotEmpty();
        Atom atom = data.get(0);
        assertThat(atom.getBatteryLevel().hasBatteryLevel()).isTrue();
        if (hasBattery()) {
            assertThat(atom.getBatteryLevel().getBatteryLevel()).isIn(Range.openClosed(0, 100));
        }
    }

    // This test is for the pulled battery charge count atom.
    public void testBatteryCycleCount() throws Exception {
        if (!hasFeature(FEATURE_WATCH, false)) return;
        StatsdConfig.Builder config = createConfigBuilder();
        addGaugeAtomWithDimensions(config, Atom.BATTERY_CYCLE_COUNT_FIELD_NUMBER, null);

        uploadConfig(config);

        Thread.sleep(WAIT_TIME_LONG);
        setAppBreadcrumbPredicate();
        Thread.sleep(WAIT_TIME_LONG);

        List<Atom> data = getGaugeMetricDataList();

        assertThat(data).isNotEmpty();
        Atom atom = data.get(0);
        assertThat(atom.getBatteryCycleCount().hasCycleCount()).isTrue();
        if (hasBattery()) {
            assertThat(atom.getBatteryCycleCount().getCycleCount()).isAtLeast(0);
        }
    }

    public void testKernelWakelock() throws Exception {
        if (!kernelWakelockStatsExist()) {
            return;
        }
        StatsdConfig.Builder config = createConfigBuilder();
        addGaugeAtomWithDimensions(config, Atom.KERNEL_WAKELOCK_FIELD_NUMBER, null);

        uploadConfig(config);

        Thread.sleep(WAIT_TIME_LONG);
        setAppBreadcrumbPredicate();
        Thread.sleep(WAIT_TIME_LONG);

        List<Atom> data = getGaugeMetricDataList();

        assertThat(data).isNotEmpty();
        for (Atom atom : data) {
            assertThat(atom.getKernelWakelock().hasName()).isTrue();
            assertThat(atom.getKernelWakelock().hasCount()).isTrue();
            assertThat(atom.getKernelWakelock().hasVersion()).isTrue();
            assertThat(atom.getKernelWakelock().getVersion()).isGreaterThan(0);
            assertThat(atom.getKernelWakelock().hasTimeMicros()).isTrue();
        }
    }

    // Returns true iff either |WAKE_LOCK_FILE| or |WAKE_SOURCES_FILE| exists.
    private boolean kernelWakelockStatsExist() {
      try {
        return doesFileExist(WAKE_LOCK_FILE) || doesFileExist(WAKE_SOURCES_FILE);
      } catch(Exception e) {
        return false;
      }
    }

    public void testWifiActivityInfo() throws Exception {
        if (!hasFeature(FEATURE_WIFI, true)) return;
        if (!hasFeature(FEATURE_WATCH, false)) return;
        if (!checkDeviceFor("checkWifiEnhancedPowerReportingSupported")) return;

        StatsdConfig.Builder config = createConfigBuilder();
        addGaugeAtomWithDimensions(config, Atom.WIFI_ACTIVITY_INFO_FIELD_NUMBER, null);

        uploadConfig(config);

        Thread.sleep(WAIT_TIME_LONG);
        setAppBreadcrumbPredicate();
        Thread.sleep(WAIT_TIME_LONG);

        List<Atom> dataList = getGaugeMetricDataList();

        for (Atom atom: dataList) {
            assertThat(atom.getWifiActivityInfo().getTimestampMillis()).isGreaterThan(0L);
            assertThat(atom.getWifiActivityInfo().getStackState()).isAtLeast(0);
            assertThat(atom.getWifiActivityInfo().getControllerIdleTimeMillis()).isGreaterThan(0L);
            assertThat(atom.getWifiActivityInfo().getControllerTxTimeMillis()).isAtLeast(0L);
            assertThat(atom.getWifiActivityInfo().getControllerRxTimeMillis()).isAtLeast(0L);
            assertThat(atom.getWifiActivityInfo().getControllerEnergyUsed()).isAtLeast(0L);
        }
    }

    public void testBuildInformation() throws Exception {
        StatsdConfig.Builder config = createConfigBuilder();
        addGaugeAtomWithDimensions(config, Atom.BUILD_INFORMATION_FIELD_NUMBER, null);
        uploadConfig(config);

        Thread.sleep(WAIT_TIME_LONG);
        setAppBreadcrumbPredicate();
        Thread.sleep(WAIT_TIME_LONG);

        List<Atom> data = getGaugeMetricDataList();
        assertThat(data).isNotEmpty();
        BuildInformation atom = data.get(0).getBuildInformation();
        assertThat(getProperty("ro.product.brand")).isEqualTo(atom.getBrand());
        assertThat(getProperty("ro.product.name")).isEqualTo(atom.getProduct());
        assertThat(getProperty("ro.product.device")).isEqualTo(atom.getDevice());
        assertThat(getProperty("ro.build.version.release_or_codename")).isEqualTo(atom.getVersionRelease());
        assertThat(getProperty("ro.build.id")).isEqualTo(atom.getId());
        assertThat(getProperty("ro.build.version.incremental"))
            .isEqualTo(atom.getVersionIncremental());
        assertThat(getProperty("ro.build.type")).isEqualTo(atom.getType());
        assertThat(getProperty("ro.build.tags")).isEqualTo(atom.getTags());
    }

    public void testOnDevicePowerMeasurement() throws Exception {
        if (!OPTIONAL_TESTS_ENABLED) return;

        StatsdConfig.Builder config = createConfigBuilder();
        addGaugeAtomWithDimensions(config, Atom.ON_DEVICE_POWER_MEASUREMENT_FIELD_NUMBER, null);

        uploadConfig(config);

        Thread.sleep(WAIT_TIME_LONG);
        setAppBreadcrumbPredicate();
        Thread.sleep(WAIT_TIME_LONG);

        List<Atom> dataList = getGaugeMetricDataList();

        for (Atom atom: dataList) {
            assertThat(atom.getOnDevicePowerMeasurement().getMeasurementTimestampMillis())
                .isAtLeast(0L);
            assertThat(atom.getOnDevicePowerMeasurement().getEnergyMicrowattSecs()).isAtLeast(0L);
        }
    }

    // Explicitly tests if the adb command to log a breadcrumb is working.
    public void testBreadcrumbAdb() throws Exception {
        final int atomTag = Atom.APP_BREADCRUMB_REPORTED_FIELD_NUMBER;
        createAndUploadConfig(atomTag);
        Thread.sleep(WAIT_TIME_SHORT);

        doAppBreadcrumbReportedStart(1);
        Thread.sleep(WAIT_TIME_SHORT);

        List<EventMetricData> data = getEventMetricDataList();
        AppBreadcrumbReported atom = data.get(0).getAtom().getAppBreadcrumbReported();
        assertThat(atom.getLabel()).isEqualTo(1);
        assertThat(atom.getState().getNumber()).isEqualTo(AppBreadcrumbReported.State.START_VALUE);
    }

    // Test dumpsys stats --proto.
    public void testDumpsysStats() throws Exception {
        final int atomTag = Atom.APP_BREADCRUMB_REPORTED_FIELD_NUMBER;
        createAndUploadConfig(atomTag);
        Thread.sleep(WAIT_TIME_SHORT);

        doAppBreadcrumbReportedStart(1);
        Thread.sleep(WAIT_TIME_SHORT);

        // Get the stats incident section.
        List<ConfigMetricsReportList> listList = getReportsFromStatsDataDumpProto();
        assertThat(listList).isNotEmpty();

        // Extract the relevent report from the incident section.
        ConfigMetricsReportList ourList = null;
        int hostUid = getHostUid();
        for (ConfigMetricsReportList list : listList) {
            ConfigMetricsReportList.ConfigKey configKey = list.getConfigKey();
            if (configKey.getUid() == hostUid && configKey.getId() == CONFIG_ID) {
                ourList = list;
                break;
            }
        }
        assertWithMessage(String.format("Could not find list for uid=%d id=%d", hostUid, CONFIG_ID))
            .that(ourList).isNotNull();

        // Make sure that the report is correct.
        List<EventMetricData> data = getEventMetricDataList(ourList);
        AppBreadcrumbReported atom = data.get(0).getAtom().getAppBreadcrumbReported();
        assertThat(atom.getLabel()).isEqualTo(1);
        assertThat(atom.getState().getNumber()).isEqualTo(AppBreadcrumbReported.State.START_VALUE);
    }

    public void testConnectivityStateChange() throws Exception {
        if (!hasFeature(FEATURE_WIFI, true)) return;
        if (!hasFeature(FEATURE_WATCH, false)) return;
        if (!hasFeature(FEATURE_LEANBACK_ONLY, false)) return;

        final int atomTag = Atom.CONNECTIVITY_STATE_CHANGED_FIELD_NUMBER;
        createAndUploadConfig(atomTag);
        Thread.sleep(WAIT_TIME_SHORT);

        turnOnAirplaneMode();
        // wait long enough for airplane mode events to propagate.
        Thread.sleep(1_200);
        turnOffAirplaneMode();
        // wait long enough for the device to restore connection
        Thread.sleep(13_000);

        List<EventMetricData> data = getEventMetricDataList();
        // at least 1 disconnect and 1 connect
        assertThat(data.size()).isAtLeast(2);
        boolean foundDisconnectEvent = false;
        boolean foundConnectEvent = false;
        for (EventMetricData d : data) {
            ConnectivityStateChanged atom = d.getAtom().getConnectivityStateChanged();
            if(atom.getState().getNumber()
                    == ConnectivityStateChanged.State.DISCONNECTED_VALUE) {
                foundDisconnectEvent = true;
            }
            if(atom.getState().getNumber()
                    == ConnectivityStateChanged.State.CONNECTED_VALUE) {
                foundConnectEvent = true;
            }
        }
        assertThat(foundConnectEvent).isTrue();
        assertThat(foundDisconnectEvent).isTrue();
    }

    public void testSimSlotState() throws Exception {
        if (!hasFeature(FEATURE_TELEPHONY, true)) {
            return;
        }

        StatsdConfig.Builder config = createConfigBuilder();
        addGaugeAtomWithDimensions(config, Atom.SIM_SLOT_STATE_FIELD_NUMBER, null);
        uploadConfig(config);

        Thread.sleep(WAIT_TIME_LONG);
        setAppBreadcrumbPredicate();
        Thread.sleep(WAIT_TIME_LONG);

        List<Atom> data = getGaugeMetricDataList();
        assertThat(data).isNotEmpty();
        SimSlotState atom = data.get(0).getSimSlotState();
        // NOTE: it is possible for devices with telephony support to have no SIM at all
        assertThat(atom.getActiveSlotCount()).isEqualTo(getActiveSimSlotCount());
        assertThat(atom.getSimCount()).isAtMost(getActiveSimCountUpperBound());
        assertThat(atom.getEsimCount()).isAtMost(getActiveEsimCountUpperBound());
        // Above assertions do no necessarily enforce the following, since some are upper bounds
        assertThat(atom.getActiveSlotCount()).isAtLeast(atom.getSimCount());
        assertThat(atom.getSimCount()).isAtLeast(atom.getEsimCount());
        assertThat(atom.getEsimCount()).isAtLeast(0);
        // For GSM phones, at least one slot should be active even if there is no card
        if (hasGsmPhone()) {
            assertThat(atom.getActiveSlotCount()).isAtLeast(1);
        }
    }

    public void testSupportedRadioAccessFamily() throws Exception {
        if (!hasFeature(FEATURE_TELEPHONY, true)) {
            return;
        }

        StatsdConfig.Builder config = createConfigBuilder();
        addGaugeAtomWithDimensions(config, Atom.SUPPORTED_RADIO_ACCESS_FAMILY_FIELD_NUMBER, null);
        uploadConfig(config);

        Thread.sleep(WAIT_TIME_LONG);
        setAppBreadcrumbPredicate();
        Thread.sleep(WAIT_TIME_LONG);

        List<Atom> data = getGaugeMetricDataList();
        assertThat(data).isNotEmpty();
        SupportedRadioAccessFamily atom = data.get(0).getSupportedRadioAccessFamily();
        if (hasGsmPhone()) {
            assertThat(atom.getNetworkTypeBitmask() & NETWORK_TYPE_BITMASK_GSM_ALL)
                    .isNotEqualTo(0L);
        }
        if (hasCdmaPhone()) {
            assertThat(atom.getNetworkTypeBitmask() & NETWORK_TYPE_BITMASK_CDMA_ALL)
                    .isNotEqualTo(0L);
        }
    }
}
