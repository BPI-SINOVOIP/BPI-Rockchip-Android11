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

import static com.android.os.AtomsProto.IntegrityCheckResultReported.Response.ALLOWED;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import android.app.AppOpEnum;
import android.net.wifi.WifiModeEnum;
import android.os.WakeLockLevelEnum;
import android.server.ErrorSource;
import android.telephony.NetworkTypeEnum;

import com.android.compatibility.common.tradefed.build.CompatibilityBuildHelper;
import com.android.compatibility.common.util.PropertyUtil;
import com.android.internal.os.StatsdConfigProto.StatsdConfig;
import com.android.os.AtomsProto;
import com.android.os.AtomsProto.ANROccurred;
import com.android.os.AtomsProto.AppBreadcrumbReported;
import com.android.os.AtomsProto.AppCrashOccurred;
import com.android.os.AtomsProto.AppOps;
import com.android.os.AtomsProto.AppStartOccurred;
import com.android.os.AtomsProto.AppUsageEventOccurred;
import com.android.os.AtomsProto.Atom;
import com.android.os.AtomsProto.AttributedAppOps;
import com.android.os.AtomsProto.AttributionNode;
import com.android.os.AtomsProto.AudioStateChanged;
import com.android.os.AtomsProto.BinderCalls;
import com.android.os.AtomsProto.BleScanResultReceived;
import com.android.os.AtomsProto.BleScanStateChanged;
import com.android.os.AtomsProto.BlobCommitted;
import com.android.os.AtomsProto.BlobLeased;
import com.android.os.AtomsProto.BlobOpened;
import com.android.os.AtomsProto.CameraStateChanged;
import com.android.os.AtomsProto.DangerousPermissionState;
import com.android.os.AtomsProto.DangerousPermissionStateSampled;
import com.android.os.AtomsProto.DeviceCalculatedPowerBlameUid;
import com.android.os.AtomsProto.FlashlightStateChanged;
import com.android.os.AtomsProto.ForegroundServiceAppOpSessionEnded;
import com.android.os.AtomsProto.ForegroundServiceStateChanged;
import com.android.os.AtomsProto.GpsScanStateChanged;
import com.android.os.AtomsProto.HiddenApiUsed;
import com.android.os.AtomsProto.IntegrityCheckResultReported;
import com.android.os.AtomsProto.IonHeapSize;
import com.android.os.AtomsProto.LmkKillOccurred;
import com.android.os.AtomsProto.LooperStats;
import com.android.os.AtomsProto.MediaCodecStateChanged;
import com.android.os.AtomsProto.NotificationReported;
import com.android.os.AtomsProto.OverlayStateChanged;
import com.android.os.AtomsProto.PackageNotificationChannelGroupPreferences;
import com.android.os.AtomsProto.PackageNotificationChannelPreferences;
import com.android.os.AtomsProto.PackageNotificationPreferences;
import com.android.os.AtomsProto.PictureInPictureStateChanged;
import com.android.os.AtomsProto.ProcessMemoryHighWaterMark;
import com.android.os.AtomsProto.ProcessMemorySnapshot;
import com.android.os.AtomsProto.ProcessMemoryState;
import com.android.os.AtomsProto.ScheduledJobStateChanged;
import com.android.os.AtomsProto.SettingSnapshot;
import com.android.os.AtomsProto.SyncStateChanged;
import com.android.os.AtomsProto.TestAtomReported;
import com.android.os.AtomsProto.VibratorStateChanged;
import com.android.os.AtomsProto.WakelockStateChanged;
import com.android.os.AtomsProto.WakeupAlarmOccurred;
import com.android.os.AtomsProto.WifiLockStateChanged;
import com.android.os.AtomsProto.WifiMulticastLockStateChanged;
import com.android.os.AtomsProto.WifiScanStateChanged;
import com.android.os.StatsLog.EventMetricData;
import com.android.server.notification.SmallHash;
import com.android.tradefed.log.LogUtil;

import com.google.common.collect.Range;
import com.google.protobuf.Descriptors;

import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.function.Function;

/**
 * Statsd atom tests that are done via app, for atoms that report a uid.
 */
public class UidAtomTests extends DeviceAtomTestCase {

    private static final String TAG = "Statsd.UidAtomTests";

    private static final String TEST_PACKAGE_NAME = "com.android.server.cts.device.statsd";

    private static final boolean DAVEY_ENABLED = false;

    private static final int NUM_APP_OPS = AttributedAppOps.getDefaultInstance().getOp().
            getDescriptorForType().getValues().size() - 1;

    private static final String TEST_INSTALL_APK = "CtsStatsdEmptyApp.apk";
    private static final String TEST_INSTALL_APK_BASE = "CtsStatsdEmptySplitApp.apk";
    private static final String TEST_INSTALL_APK_SPLIT = "CtsStatsdEmptySplitApp_pl.apk";
    private static final String TEST_INSTALL_PACKAGE =
            "com.android.cts.device.statsd.emptyapp";
    private static final String TEST_REMOTE_DIR = "/data/local/tmp/statsd";
    private static final String ACTION_SHOW_APPLICATION_OVERLAY = "action.show_application_overlay";
    private static final String ACTION_LONG_SLEEP_WHILE_TOP = "action.long_sleep_top";

    private static final int WAIT_TIME_FOR_CONFIG_UPDATE_MS = 200;
    private static final int EXTRA_WAIT_TIME_MS = 5_000; // as buffer when app starting/stopping.
    private static final int STATSD_REPORT_WAIT_TIME_MS = 500; // make sure statsd finishes log.

    @Override
    protected void setUp() throws Exception {
        super.setUp();
    }

    @Override
    protected void tearDown() throws Exception {
        resetBatteryStatus();
        super.tearDown();
    }

    public void testLmkKillOccurred() throws Exception {
        if (!"true".equals(getProperty("ro.lmk.log_stats"))) {
            return;
        }

        final int atomTag = Atom.LMK_KILL_OCCURRED_FIELD_NUMBER;
        createAndUploadConfig(atomTag, false);

        Thread.sleep(WAIT_TIME_SHORT);

        executeBackgroundService(ACTION_LMK);
        Thread.sleep(15_000);

        // Sorted list of events in order in which they occurred.
        List<EventMetricData> data = getEventMetricDataList();

        assertThat(data).hasSize(1);
        assertThat(data.get(0).getAtom().hasLmkKillOccurred()).isTrue();
        LmkKillOccurred atom = data.get(0).getAtom().getLmkKillOccurred();
        assertThat(atom.getUid()).isEqualTo(getUid());
        assertThat(atom.getProcessName()).isEqualTo(DEVICE_SIDE_TEST_PACKAGE);
        assertThat(atom.getOomAdjScore()).isAtLeast(500);
    }

    public void testAppCrashOccurred() throws Exception {
        final int atomTag = Atom.APP_CRASH_OCCURRED_FIELD_NUMBER;
        createAndUploadConfig(atomTag, false);
        Thread.sleep(WAIT_TIME_SHORT);

        runActivity("StatsdCtsForegroundActivity", "action", "action.crash");

        Thread.sleep(WAIT_TIME_SHORT);
        // Sorted list of events in order in which they occurred.
        List<EventMetricData> data = getEventMetricDataList();

        AppCrashOccurred atom = data.get(0).getAtom().getAppCrashOccurred();
        assertThat(atom.getEventType()).isEqualTo("crash");
        assertThat(atom.getIsInstantApp().getNumber())
            .isEqualTo(AppCrashOccurred.InstantApp.FALSE_VALUE);
        assertThat(atom.getForegroundState().getNumber())
            .isEqualTo(AppCrashOccurred.ForegroundState.FOREGROUND_VALUE);
        assertThat(atom.getPackageName()).isEqualTo(TEST_PACKAGE_NAME);
    }

    public void testAppStartOccurred() throws Exception {
        final int atomTag = Atom.APP_START_OCCURRED_FIELD_NUMBER;

        createAndUploadConfig(atomTag, false);
        Thread.sleep(WAIT_TIME_SHORT);

        runActivity("StatsdCtsForegroundActivity", "action", "action.sleep_top", 3_500);

        // Sorted list of events in order in which they occurred.
        List<EventMetricData> data = getEventMetricDataList();

        assertThat(data).hasSize(1);
        AppStartOccurred atom = data.get(0).getAtom().getAppStartOccurred();
        assertThat(atom.getPkgName()).isEqualTo(TEST_PACKAGE_NAME);
        assertThat(atom.getActivityName())
            .isEqualTo("com.android.server.cts.device.statsd.StatsdCtsForegroundActivity");
        assertThat(atom.getIsInstantApp()).isFalse();
        assertThat(atom.getActivityStartMillis()).isGreaterThan(0L);
        assertThat(atom.getTransitionDelayMillis()).isGreaterThan(0);
    }

    public void testAudioState() throws Exception {
        if (!hasFeature(FEATURE_AUDIO_OUTPUT, true)) return;

        final int atomTag = Atom.AUDIO_STATE_CHANGED_FIELD_NUMBER;
        final String name = "testAudioState";

        Set<Integer> onState = new HashSet<>(
                Arrays.asList(AudioStateChanged.State.ON_VALUE));
        Set<Integer> offState = new HashSet<>(
                Arrays.asList(AudioStateChanged.State.OFF_VALUE));

        // Add state sets to the list in order.
        List<Set<Integer>> stateSet = Arrays.asList(onState, offState);

        createAndUploadConfig(atomTag, true);  // True: uses attribution.
        Thread.sleep(WAIT_TIME_SHORT);

        runDeviceTests(DEVICE_SIDE_TEST_PACKAGE, ".AtomTests", name);

        Thread.sleep(WAIT_TIME_SHORT);
        // Sorted list of events in order in which they occurred.
        List<EventMetricData> data = getEventMetricDataList();

        // Because the timestamp is truncated, we skip checking time differences between state
        // changes.
        assertStatesOccurred(stateSet, data, 0,
                atom -> atom.getAudioStateChanged().getState().getNumber());

        // Check that timestamp is truncated
        for (EventMetricData metric : data) {
            long elapsedTimestampNs = metric.getElapsedTimestampNanos();
            assertTimestampIsTruncated(elapsedTimestampNs);
        }
    }

    public void testBleScan() throws Exception {
        if (!hasFeature(FEATURE_BLUETOOTH_LE, true)) return;

        final int atom = Atom.BLE_SCAN_STATE_CHANGED_FIELD_NUMBER;
        final int field = BleScanStateChanged.STATE_FIELD_NUMBER;
        final int stateOn = BleScanStateChanged.State.ON_VALUE;
        final int stateOff = BleScanStateChanged.State.OFF_VALUE;
        final int minTimeDiffMillis = 1_500;
        final int maxTimeDiffMillis = 3_000;

        List<EventMetricData> data = doDeviceMethodOnOff("testBleScanUnoptimized", atom, field,
                stateOn, stateOff, minTimeDiffMillis, maxTimeDiffMillis, true);

        BleScanStateChanged a0 = data.get(0).getAtom().getBleScanStateChanged();
        BleScanStateChanged a1 = data.get(1).getAtom().getBleScanStateChanged();
        assertThat(a0.getState().getNumber()).isEqualTo(stateOn);
        assertThat(a1.getState().getNumber()).isEqualTo(stateOff);
    }

    public void testBleUnoptimizedScan() throws Exception {
        if (!hasFeature(FEATURE_BLUETOOTH_LE, true)) return;

        final int atom = Atom.BLE_SCAN_STATE_CHANGED_FIELD_NUMBER;
        final int field = BleScanStateChanged.STATE_FIELD_NUMBER;
        final int stateOn = BleScanStateChanged.State.ON_VALUE;
        final int stateOff = BleScanStateChanged.State.OFF_VALUE;
        final int minTimeDiffMillis = 1_500;
        final int maxTimeDiffMillis = 3_000;

        List<EventMetricData> data = doDeviceMethodOnOff("testBleScanUnoptimized", atom, field,
                stateOn, stateOff, minTimeDiffMillis, maxTimeDiffMillis, true);

        BleScanStateChanged a0 = data.get(0).getAtom().getBleScanStateChanged();
        assertThat(a0.getState().getNumber()).isEqualTo(stateOn);
        assertThat(a0.getIsFiltered()).isFalse();
        assertThat(a0.getIsFirstMatch()).isFalse();
        assertThat(a0.getIsOpportunistic()).isFalse();
        BleScanStateChanged a1 = data.get(1).getAtom().getBleScanStateChanged();
        assertThat(a1.getState().getNumber()).isEqualTo(stateOff);
        assertThat(a1.getIsFiltered()).isFalse();
        assertThat(a1.getIsFirstMatch()).isFalse();
        assertThat(a1.getIsOpportunistic()).isFalse();


        // Now repeat the test for opportunistic scanning and make sure it is reported correctly.
        data = doDeviceMethodOnOff("testBleScanOpportunistic", atom, field,
                stateOn, stateOff, minTimeDiffMillis, maxTimeDiffMillis, true);

        a0 = data.get(0).getAtom().getBleScanStateChanged();
        assertThat(a0.getState().getNumber()).isEqualTo(stateOn);
        assertThat(a0.getIsFiltered()).isFalse();
        assertThat(a0.getIsFirstMatch()).isFalse();
        assertThat(a0.getIsOpportunistic()).isTrue();  // This scan is opportunistic.
        a1 = data.get(1).getAtom().getBleScanStateChanged();
        assertThat(a1.getState().getNumber()).isEqualTo(stateOff);
        assertThat(a1.getIsFiltered()).isFalse();
        assertThat(a1.getIsFirstMatch()).isFalse();
        assertThat(a1.getIsOpportunistic()).isTrue();
    }

    public void testBleScanResult() throws Exception {
        if (!hasFeature(FEATURE_BLUETOOTH_LE, true)) return;

        final int atom = Atom.BLE_SCAN_RESULT_RECEIVED_FIELD_NUMBER;
        final int field = BleScanResultReceived.NUM_RESULTS_FIELD_NUMBER;

        StatsdConfig.Builder conf = createConfigBuilder();
        addAtomEvent(conf, atom, createFvm(field).setGteInt(0));
        List<EventMetricData> data = doDeviceMethod("testBleScanResult", conf);

        assertThat(data.size()).isAtLeast(1);
        BleScanResultReceived a0 = data.get(0).getAtom().getBleScanResultReceived();
        assertThat(a0.getNumResults()).isAtLeast(1);
    }

    public void testHiddenApiUsed() throws Exception {
        String oldRate = getDevice().executeShellCommand(
                "device_config get app_compat hidden_api_access_statslog_sampling_rate").trim();

        getDevice().executeShellCommand(
                "device_config put app_compat hidden_api_access_statslog_sampling_rate 65536");

        Thread.sleep(WAIT_TIME_SHORT);

        try {
            final int atomTag = Atom.HIDDEN_API_USED_FIELD_NUMBER;

            createAndUploadConfig(atomTag, false);

            runActivity("HiddenApiUsedActivity", null, null, 2_500);

            List<EventMetricData> data = getEventMetricDataList();
            assertThat(data).hasSize(1);

            HiddenApiUsed atom = data.get(0).getAtom().getHiddenApiUsed();

            int uid = getUid();
            assertThat(atom.getUid()).isEqualTo(uid);
            assertThat(atom.getAccessDenied()).isFalse();
            assertThat(atom.getSignature())
                .isEqualTo("Landroid/app/Activity;->mWindow:Landroid/view/Window;");
        } finally {
            if (!oldRate.equals("null")) {
                getDevice().executeShellCommand(
                        "device_config put app_compat hidden_api_access_statslog_sampling_rate "
                        + oldRate);
            } else {
                getDevice().executeShellCommand(
                        "device_config delete hidden_api_access_statslog_sampling_rate");
            }
        }
    }

    public void testCameraState() throws Exception {
        if (!hasFeature(FEATURE_CAMERA, true) && !hasFeature(FEATURE_CAMERA_FRONT, true)) return;

        final int atomTag = Atom.CAMERA_STATE_CHANGED_FIELD_NUMBER;
        Set<Integer> cameraOn = new HashSet<>(Arrays.asList(CameraStateChanged.State.ON_VALUE));
        Set<Integer> cameraOff = new HashSet<>(Arrays.asList(CameraStateChanged.State.OFF_VALUE));

        // Add state sets to the list in order.
        List<Set<Integer>> stateSet = Arrays.asList(cameraOn, cameraOff);

        createAndUploadConfig(atomTag, true);  // True: uses attribution.
        runDeviceTests(DEVICE_SIDE_TEST_PACKAGE, ".AtomTests", "testCameraState");

        // Sorted list of events in order in which they occurred.
        List<EventMetricData> data = getEventMetricDataList();

        // Assert that the events happened in the expected order.
        assertStatesOccurred(stateSet, data, WAIT_TIME_LONG,
                atom -> atom.getCameraStateChanged().getState().getNumber());
    }

    public void testCpuTimePerUid() throws Exception {
        if (!hasFeature(FEATURE_WATCH, false)) return;
        StatsdConfig.Builder config = createConfigBuilder();
        addGaugeAtomWithDimensions(config, Atom.CPU_TIME_PER_UID_FIELD_NUMBER, null);

        uploadConfig(config);

        runDeviceTests(DEVICE_SIDE_TEST_PACKAGE, ".AtomTests", "testSimpleCpu");

        Thread.sleep(WAIT_TIME_SHORT);
        setAppBreadcrumbPredicate();
        Thread.sleep(WAIT_TIME_LONG);

        List<Atom> atomList = getGaugeMetricDataList();

        // TODO: We don't have atom matching on gauge yet. Let's refactor this after that feature is
        // implemented.
        boolean found = false;
        int uid = getUid();
        for (Atom atom : atomList) {
            if (atom.getCpuTimePerUid().getUid() == uid) {
                found = true;
                assertThat(atom.getCpuTimePerUid().getUserTimeMicros()).isGreaterThan(0L);
                assertThat(atom.getCpuTimePerUid().getSysTimeMicros()).isGreaterThan(0L);
            }
        }
        assertWithMessage(String.format("did not find uid %d", uid)).that(found).isTrue();
    }

    public void testDeviceCalculatedPowerUse() throws Exception {
        if (!hasFeature(FEATURE_LEANBACK_ONLY, false)) return;

        StatsdConfig.Builder config = createConfigBuilder();
        addGaugeAtomWithDimensions(config, Atom.DEVICE_CALCULATED_POWER_USE_FIELD_NUMBER, null);
        uploadConfig(config);
        unplugDevice();

        Thread.sleep(WAIT_TIME_LONG);
        runDeviceTests(DEVICE_SIDE_TEST_PACKAGE, ".AtomTests", "testSimpleCpu");
        Thread.sleep(WAIT_TIME_SHORT);
        setAppBreadcrumbPredicate();
        Thread.sleep(WAIT_TIME_LONG);

        Atom atom = getGaugeMetricDataList().get(0);
        assertThat(atom.getDeviceCalculatedPowerUse().getComputedPowerNanoAmpSecs())
            .isGreaterThan(0L);
    }


    public void testDeviceCalculatedPowerBlameUid() throws Exception {
        if (!hasFeature(FEATURE_LEANBACK_ONLY, false)) return;
        if (!hasBattery()) {
            return;
        }

        StatsdConfig.Builder config = createConfigBuilder();
        addGaugeAtomWithDimensions(config,
                Atom.DEVICE_CALCULATED_POWER_BLAME_UID_FIELD_NUMBER, null);
        uploadConfig(config);
        unplugDevice();

        Thread.sleep(WAIT_TIME_LONG);
        runDeviceTests(DEVICE_SIDE_TEST_PACKAGE, ".AtomTests", "testSimpleCpu");
        Thread.sleep(WAIT_TIME_SHORT);
        setAppBreadcrumbPredicate();
        Thread.sleep(WAIT_TIME_LONG);

        List<Atom> atomList = getGaugeMetricDataList();
        boolean uidFound = false;
        int uid = getUid();
        long uidPower = 0;
        for (Atom atom : atomList) {
            DeviceCalculatedPowerBlameUid item = atom.getDeviceCalculatedPowerBlameUid();
                if (item.getUid() == uid) {
                assertWithMessage(String.format("Found multiple power values for uid %d", uid))
                    .that(uidFound).isFalse();
                uidFound = true;
                uidPower = item.getPowerNanoAmpSecs();
            }
        }
        assertWithMessage(String.format("No power value for uid %d", uid)).that(uidFound).isTrue();
        assertWithMessage(String.format("Non-positive power value for uid %d", uid))
            .that(uidPower).isGreaterThan(0L);
    }

    public void testDavey() throws Exception {
        if (!DAVEY_ENABLED ) return;
        long MAX_DURATION = 2000;
        long MIN_DURATION = 750;
        final int atomTag = Atom.DAVEY_OCCURRED_FIELD_NUMBER;
        createAndUploadConfig(atomTag, false); // UID is logged without attribution node

        runActivity("DaveyActivity", null, null);

        List<EventMetricData> data = getEventMetricDataList();
        assertThat(data).hasSize(1);
        long duration = data.get(0).getAtom().getDaveyOccurred().getJankDurationMillis();
        assertWithMessage("Incorrect jank duration")
            .that(duration).isIn(Range.closed(MIN_DURATION, MAX_DURATION));
    }

    public void testFlashlightState() throws Exception {
        if (!hasFeature(FEATURE_CAMERA_FLASH, true)) return;

        final int atomTag = Atom.FLASHLIGHT_STATE_CHANGED_FIELD_NUMBER;
        final String name = "testFlashlight";

        Set<Integer> flashlightOn = new HashSet<>(
            Arrays.asList(FlashlightStateChanged.State.ON_VALUE));
        Set<Integer> flashlightOff = new HashSet<>(
            Arrays.asList(FlashlightStateChanged.State.OFF_VALUE));

        // Add state sets to the list in order.
        List<Set<Integer>> stateSet = Arrays.asList(flashlightOn, flashlightOff);

        createAndUploadConfig(atomTag, true);  // True: uses attribution.
        Thread.sleep(WAIT_TIME_SHORT);

        runDeviceTests(DEVICE_SIDE_TEST_PACKAGE, ".AtomTests", name);

        // Sorted list of events in order in which they occurred.
        List<EventMetricData> data = getEventMetricDataList();

        // Assert that the events happened in the expected order.
        assertStatesOccurred(stateSet, data, WAIT_TIME_SHORT,
                atom -> atom.getFlashlightStateChanged().getState().getNumber());
    }

    public void testForegroundServiceState() throws Exception {
        final int atomTag = Atom.FOREGROUND_SERVICE_STATE_CHANGED_FIELD_NUMBER;
        final String name = "testForegroundService";

        Set<Integer> enterForeground = new HashSet<>(
                Arrays.asList(ForegroundServiceStateChanged.State.ENTER_VALUE));
        Set<Integer> exitForeground = new HashSet<>(
                Arrays.asList(ForegroundServiceStateChanged.State.EXIT_VALUE));

        // Add state sets to the list in order.
        List<Set<Integer>> stateSet = Arrays.asList(enterForeground, exitForeground);

        createAndUploadConfig(atomTag, false);
        Thread.sleep(WAIT_TIME_SHORT);

        runDeviceTests(DEVICE_SIDE_TEST_PACKAGE, ".AtomTests", name);

        // Sorted list of events in order in which they occurred.
        List<EventMetricData> data = getEventMetricDataList();

        // Assert that the events happened in the expected order.
        assertStatesOccurred(stateSet, data, WAIT_TIME_SHORT,
                atom -> atom.getForegroundServiceStateChanged().getState().getNumber());
    }


    public void testForegroundServiceAccessAppOp() throws Exception {
        final int atomTag = Atom.FOREGROUND_SERVICE_APP_OP_SESSION_ENDED_FIELD_NUMBER;
        final String name = "testForegroundServiceAccessAppOp";

        createAndUploadConfig(atomTag, false);
        Thread.sleep(WAIT_TIME_SHORT);

        runDeviceTests(DEVICE_SIDE_TEST_PACKAGE, ".AtomTests", name);

        // Sorted list of events in order in which they occurred.
        List<EventMetricData> data = getEventMetricDataList();

        assertWithMessage("Wrong atom size").that(data.size()).isEqualTo(3);
        for (int i = 0; i < data.size(); i++) {
            ForegroundServiceAppOpSessionEnded atom
                    = data.get(i).getAtom().getForegroundServiceAppOpSessionEnded();
            final int opName = atom.getAppOpName().getNumber();
            final int acceptances = atom.getCountOpsAccepted();
            final int rejections = atom.getCountOpsRejected();
            final int count = acceptances + rejections;
            int expectedCount = 0;
            switch (opName) {
                case AppOpEnum.APP_OP_CAMERA_VALUE:
                    expectedCount = 3;
                    break;
                case AppOpEnum.APP_OP_FINE_LOCATION_VALUE:
                    expectedCount = 1;
                    break;
                case AppOpEnum.APP_OP_RECORD_AUDIO_VALUE:
                    expectedCount = 2;
                    break;
                case AppOpEnum.APP_OP_COARSE_LOCATION_VALUE:
                    // fall-through
                default:
                    fail("Unexpected opName " + opName);
            }
            assertWithMessage("Wrong count for " + opName).that(count).isEqualTo(expectedCount);
        }
    }

    public void testGpsScan() throws Exception {
        if (!hasFeature(FEATURE_LOCATION_GPS, true)) return;
        // Whitelist this app against background location request throttling
        String origWhitelist = getDevice().executeShellCommand(
                "settings get global location_background_throttle_package_whitelist").trim();
        getDevice().executeShellCommand(String.format(
                "settings put global location_background_throttle_package_whitelist %s",
                DEVICE_SIDE_TEST_PACKAGE));

        try {
            final int atom = Atom.GPS_SCAN_STATE_CHANGED_FIELD_NUMBER;
            final int key = GpsScanStateChanged.STATE_FIELD_NUMBER;
            final int stateOn = GpsScanStateChanged.State.ON_VALUE;
            final int stateOff = GpsScanStateChanged.State.OFF_VALUE;
            final int minTimeDiffMillis = 500;
            final int maxTimeDiffMillis = 60_000;

            List<EventMetricData> data = doDeviceMethodOnOff("testGpsScan", atom, key,
                    stateOn, stateOff, minTimeDiffMillis, maxTimeDiffMillis, true);

            GpsScanStateChanged a0 = data.get(0).getAtom().getGpsScanStateChanged();
            GpsScanStateChanged a1 = data.get(1).getAtom().getGpsScanStateChanged();
            assertThat(a0.getState().getNumber()).isEqualTo(stateOn);
            assertThat(a1.getState().getNumber()).isEqualTo(stateOff);
        } finally {
            if ("null".equals(origWhitelist) || "".equals(origWhitelist)) {
                getDevice().executeShellCommand(
                        "settings delete global location_background_throttle_package_whitelist");
            } else {
                getDevice().executeShellCommand(String.format(
                        "settings put global location_background_throttle_package_whitelist %s",
                        origWhitelist));
            }
        }
    }

    public void testGnssStats() throws Exception {
        // Get GnssMetrics as a simple gauge metric.
        StatsdConfig.Builder config = createConfigBuilder();
        addGaugeAtomWithDimensions(config, Atom.GNSS_STATS_FIELD_NUMBER, null);
        uploadConfig(config);
        Thread.sleep(WAIT_TIME_SHORT);

        if (!hasFeature(FEATURE_LOCATION_GPS, true)) return;
        // Whitelist this app against background location request throttling
        String origWhitelist = getDevice().executeShellCommand(
                "settings get global location_background_throttle_package_whitelist").trim();
        getDevice().executeShellCommand(String.format(
                "settings put global location_background_throttle_package_whitelist %s",
                DEVICE_SIDE_TEST_PACKAGE));

        try {
            runDeviceTests(DEVICE_SIDE_TEST_PACKAGE, ".AtomTests", "testGpsStatus");

            Thread.sleep(WAIT_TIME_LONG);
            // Trigger a pull and wait for new pull before killing the process.
            setAppBreadcrumbPredicate();
            Thread.sleep(WAIT_TIME_LONG);

            // Assert about GnssMetrics for the test app.
            List<Atom> atoms = getGaugeMetricDataList();

            boolean found = false;
            for (Atom atom : atoms) {
                AtomsProto.GnssStats state = atom.getGnssStats();
                found = true;
                if ((state.getSvStatusReports() > 0 || state.getL5SvStatusReports() > 0)
                        && state.getLocationReports() == 0) {
                    // Device is detected to be indoors and not able to acquire location.
                    // flaky test device
                    break;
                }
                assertThat(state.getLocationReports()).isGreaterThan((long) 0);
                assertThat(state.getLocationFailureReports()).isAtLeast((long) 0);
                assertThat(state.getTimeToFirstFixReports()).isGreaterThan((long) 0);
                assertThat(state.getTimeToFirstFixMillis()).isGreaterThan((long) 0);
                assertThat(state.getPositionAccuracyReports()).isGreaterThan((long) 0);
                assertThat(state.getPositionAccuracyMeters()).isGreaterThan((long) 0);
                assertThat(state.getTopFourAverageCn0Reports()).isGreaterThan((long) 0);
                assertThat(state.getTopFourAverageCn0DbMhz()).isGreaterThan((long) 0);
                assertThat(state.getL5TopFourAverageCn0Reports()).isAtLeast((long) 0);
                assertThat(state.getL5TopFourAverageCn0DbMhz()).isAtLeast((long) 0);
                assertThat(state.getSvStatusReports()).isAtLeast((long) 0);
                assertThat(state.getSvStatusReportsUsedInFix()).isAtLeast((long) 0);
                assertThat(state.getL5SvStatusReports()).isAtLeast((long) 0);
                assertThat(state.getL5SvStatusReportsUsedInFix()).isAtLeast((long) 0);
            }
            assertWithMessage(String.format("Did not find a matching atom"))
                    .that(found).isTrue();
        } finally {
            if ("null".equals(origWhitelist) || "".equals(origWhitelist)) {
                getDevice().executeShellCommand(
                        "settings delete global location_background_throttle_package_whitelist");
            } else {
                getDevice().executeShellCommand(String.format(
                        "settings put global location_background_throttle_package_whitelist %s",
                        origWhitelist));
            }
        }
    }

    public void testMediaCodecActivity() throws Exception {
        if (!hasFeature(FEATURE_WATCH, false)) return;
        final int atomTag = Atom.MEDIA_CODEC_STATE_CHANGED_FIELD_NUMBER;

        // 5 seconds. Starting video tends to be much slower than most other
        // tests on slow devices. This is unfortunate, because it leaves a
        // really big slop in assertStatesOccurred.  It would be better if
        // assertStatesOccurred had a tighter range on large timeouts.
        final int waitTime = 5000;

        // From {@link VideoPlayerActivity#DELAY_MILLIS}
        final int videoDuration = 2000;

        Set<Integer> onState = new HashSet<>(
                Arrays.asList(MediaCodecStateChanged.State.ON_VALUE));
        Set<Integer> offState = new HashSet<>(
                Arrays.asList(MediaCodecStateChanged.State.OFF_VALUE));

        // Add state sets to the list in order.
        List<Set<Integer>> stateSet = Arrays.asList(onState, offState);

        createAndUploadConfig(atomTag, true);  // True: uses attribution.
        Thread.sleep(WAIT_TIME_SHORT);

        runActivity("VideoPlayerActivity", "action", "action.play_video",
            waitTime);

        // Sorted list of events in order in which they occurred.
        List<EventMetricData> data = getEventMetricDataList();

        // Assert that the events happened in the expected order.
        assertStatesOccurred(stateSet, data, videoDuration,
                atom -> atom.getMediaCodecStateChanged().getState().getNumber());
    }

    public void testOverlayState() throws Exception {
        if (!hasFeature(FEATURE_WATCH, false)) return;
        final int atomTag = Atom.OVERLAY_STATE_CHANGED_FIELD_NUMBER;

        Set<Integer> entered = new HashSet<>(
                Arrays.asList(OverlayStateChanged.State.ENTERED_VALUE));
        Set<Integer> exited = new HashSet<>(
                Arrays.asList(OverlayStateChanged.State.EXITED_VALUE));

        // Add state sets to the list in order.
        List<Set<Integer>> stateSet = Arrays.asList(entered, exited);

        createAndUploadConfig(atomTag, false);

        runActivity("StatsdCtsForegroundActivity", "action", "action.show_application_overlay",
                5_000);

        // Sorted list of events in order in which they occurred.
        List<EventMetricData> data = getEventMetricDataList();

        // Assert that the events happened in the expected order.
        // The overlay box should appear about 2sec after the app start
        assertStatesOccurred(stateSet, data, 0,
                atom -> atom.getOverlayStateChanged().getState().getNumber());
    }

    public void testPictureInPictureState() throws Exception {
        String supported = getDevice().executeShellCommand("am supports-multiwindow");
        if (!hasFeature(FEATURE_WATCH, false) ||
                !hasFeature(FEATURE_PICTURE_IN_PICTURE, true) ||
                !supported.contains("true")) {
            LogUtil.CLog.d("Skipping picture in picture atom test.");
            return;
        }

        final int atomTag = Atom.PICTURE_IN_PICTURE_STATE_CHANGED_FIELD_NUMBER;

        Set<Integer> entered = new HashSet<>(
                Arrays.asList(PictureInPictureStateChanged.State.ENTERED_VALUE));

        // Add state sets to the list in order.
        List<Set<Integer>> stateSet = Arrays.asList(entered);

        createAndUploadConfig(atomTag, false);

        LogUtil.CLog.d("Playing video in Picture-in-Picture mode");
        runActivity("VideoPlayerActivity", "action", "action.play_video_picture_in_picture_mode");

        // Sorted list of events in order in which they occurred.
        List<EventMetricData> data = getEventMetricDataList();

        // Assert that the events happened in the expected order.
        assertStatesOccurred(stateSet, data, WAIT_TIME_LONG,
                atom -> atom.getPictureInPictureStateChanged().getState().getNumber());
    }

    public void testScheduledJobState() throws Exception {
        String expectedName = "com.android.server.cts.device.statsd/.StatsdJobService";
        final int atomTag = Atom.SCHEDULED_JOB_STATE_CHANGED_FIELD_NUMBER;
        Set<Integer> jobSchedule = new HashSet<>(
                Arrays.asList(ScheduledJobStateChanged.State.SCHEDULED_VALUE));
        Set<Integer> jobOn = new HashSet<>(
                Arrays.asList(ScheduledJobStateChanged.State.STARTED_VALUE));
        Set<Integer> jobOff = new HashSet<>(
                Arrays.asList(ScheduledJobStateChanged.State.FINISHED_VALUE));

        // Add state sets to the list in order.
        List<Set<Integer>> stateSet = Arrays.asList(jobSchedule, jobOn, jobOff);

        createAndUploadConfig(atomTag, true);  // True: uses attribution.
        allowImmediateSyncs();
        runDeviceTests(DEVICE_SIDE_TEST_PACKAGE, ".AtomTests", "testScheduledJob");

        // Sorted list of events in order in which they occurred.
        List<EventMetricData> data = getEventMetricDataList();

        assertStatesOccurred(stateSet, data, 0,
                atom -> atom.getScheduledJobStateChanged().getState().getNumber());

        for (EventMetricData e : data) {
            assertThat(e.getAtom().getScheduledJobStateChanged().getJobName())
                .isEqualTo(expectedName);
        }
    }

    //Note: this test does not have uid, but must run on the device
    public void testScreenBrightness() throws Exception {
        int initialBrightness = getScreenBrightness();
        boolean isInitialManual = isScreenBrightnessModeManual();
        setScreenBrightnessMode(true);
        setScreenBrightness(200);
        Thread.sleep(WAIT_TIME_LONG);

        final int atomTag = Atom.SCREEN_BRIGHTNESS_CHANGED_FIELD_NUMBER;

        Set<Integer> screenMin = new HashSet<>(Arrays.asList(47));
        Set<Integer> screen100 = new HashSet<>(Arrays.asList(100));
        Set<Integer> screen200 = new HashSet<>(Arrays.asList(198));
        // Set<Integer> screenMax = new HashSet<>(Arrays.asList(255));

        // Add state sets to the list in order.
        List<Set<Integer>> stateSet = Arrays.asList(screenMin, screen100, screen200);

        createAndUploadConfig(atomTag);
        Thread.sleep(WAIT_TIME_SHORT);
        runDeviceTests(DEVICE_SIDE_TEST_PACKAGE, ".AtomTests", "testScreenBrightness");

        // Sorted list of events in order in which they occurred.
        List<EventMetricData> data = getEventMetricDataList();

        // Restore initial screen brightness
        setScreenBrightness(initialBrightness);
        setScreenBrightnessMode(isInitialManual);

        popUntilFind(data, screenMin, atom->atom.getScreenBrightnessChanged().getLevel());
        popUntilFindFromEnd(data, screen200, atom->atom.getScreenBrightnessChanged().getLevel());
        // Assert that the events happened in the expected order.
        assertStatesOccurred(stateSet, data, WAIT_TIME_SHORT,
            atom -> atom.getScreenBrightnessChanged().getLevel());
    }
    public void testSyncState() throws Exception {
        final int atomTag = Atom.SYNC_STATE_CHANGED_FIELD_NUMBER;
        Set<Integer> syncOn = new HashSet<>(Arrays.asList(SyncStateChanged.State.ON_VALUE));
        Set<Integer> syncOff = new HashSet<>(Arrays.asList(SyncStateChanged.State.OFF_VALUE));

        // Add state sets to the list in order.
        List<Set<Integer>> stateSet = Arrays.asList(syncOn, syncOff, syncOn, syncOff);

        createAndUploadConfig(atomTag, true);
        allowImmediateSyncs();
        runDeviceTests(DEVICE_SIDE_TEST_PACKAGE, ".AtomTests", "testSyncState");

        // Sorted list of events in order in which they occurred.
        List<EventMetricData> data = getEventMetricDataList();

        // Assert that the events happened in the expected order.
        assertStatesOccurred(stateSet, data,
            /* wait = */ 0 /* don't verify time differences between state changes */,
            atom -> atom.getSyncStateChanged().getState().getNumber());
    }

    public void testVibratorState() throws Exception {
        if (!checkDeviceFor("checkVibratorSupported")) return;

        final int atomTag = Atom.VIBRATOR_STATE_CHANGED_FIELD_NUMBER;
        final String name = "testVibratorState";

        Set<Integer> onState = new HashSet<>(
                Arrays.asList(VibratorStateChanged.State.ON_VALUE));
        Set<Integer> offState = new HashSet<>(
                Arrays.asList(VibratorStateChanged.State.OFF_VALUE));

        // Add state sets to the list in order.
        List<Set<Integer>> stateSet = Arrays.asList(onState, offState);

        createAndUploadConfig(atomTag, true);  // True: uses attribution.
        Thread.sleep(WAIT_TIME_SHORT);

        runDeviceTests(DEVICE_SIDE_TEST_PACKAGE, ".AtomTests", name);

        Thread.sleep(WAIT_TIME_LONG);
        // Sorted list of events in order in which they occurred.
        List<EventMetricData> data = getEventMetricDataList();

        assertStatesOccurred(stateSet, data, 300,
                atom -> atom.getVibratorStateChanged().getState().getNumber());
    }

    public void testWakelockState() throws Exception {
        final int atomTag = Atom.WAKELOCK_STATE_CHANGED_FIELD_NUMBER;
        Set<Integer> wakelockOn = new HashSet<>(Arrays.asList(
                WakelockStateChanged.State.ACQUIRE_VALUE,
                WakelockStateChanged.State.CHANGE_ACQUIRE_VALUE));
        Set<Integer> wakelockOff = new HashSet<>(Arrays.asList(
                WakelockStateChanged.State.RELEASE_VALUE,
                WakelockStateChanged.State.CHANGE_RELEASE_VALUE));

        final String EXPECTED_TAG = "StatsdPartialWakelock";
        final WakeLockLevelEnum EXPECTED_LEVEL = WakeLockLevelEnum.PARTIAL_WAKE_LOCK;

        // Add state sets to the list in order.
        List<Set<Integer>> stateSet = Arrays.asList(wakelockOn, wakelockOff);

        createAndUploadConfig(atomTag, true);  // True: uses attribution.
        runDeviceTests(DEVICE_SIDE_TEST_PACKAGE, ".AtomTests", "testWakelockState");

        // Sorted list of events in order in which they occurred.
        List<EventMetricData> data = getEventMetricDataList();

        // Assert that the events happened in the expected order.
        assertStatesOccurred(stateSet, data, WAIT_TIME_SHORT,
            atom -> atom.getWakelockStateChanged().getState().getNumber());

        for (EventMetricData event: data) {
            String tag = event.getAtom().getWakelockStateChanged().getTag();
            WakeLockLevelEnum type = event.getAtom().getWakelockStateChanged().getType();
            assertThat(tag).isEqualTo(EXPECTED_TAG);
            assertThat(type).isEqualTo(EXPECTED_LEVEL);
        }
    }

    public void testWakeupAlarm() throws Exception {
        // For automotive, all wakeup alarm becomes normal alarm. So this
        // test does not work.
        if (!hasFeature(FEATURE_AUTOMOTIVE, false)) return;
        final int atomTag = Atom.WAKEUP_ALARM_OCCURRED_FIELD_NUMBER;

        StatsdConfig.Builder config = createConfigBuilder();
        addAtomEvent(config, atomTag, true);  // True: uses attribution.

        List<EventMetricData> data = doDeviceMethod("testWakeupAlarm", config);
        assertThat(data.size()).isAtLeast(1);
        for (int i = 0; i < data.size(); i++) {
            WakeupAlarmOccurred wao = data.get(i).getAtom().getWakeupAlarmOccurred();
            assertThat(wao.getTag()).isEqualTo("*walarm*:android.cts.statsd.testWakeupAlarm");
            assertThat(wao.getPackageName()).isEqualTo(DEVICE_SIDE_TEST_PACKAGE);
        }
    }

    public void testWifiLockHighPerf() throws Exception {
        if (!hasFeature(FEATURE_WIFI, true)) return;
        if (!hasFeature(FEATURE_PC, false)) return;

        final int atomTag = Atom.WIFI_LOCK_STATE_CHANGED_FIELD_NUMBER;
        Set<Integer> lockOn = new HashSet<>(Arrays.asList(WifiLockStateChanged.State.ON_VALUE));
        Set<Integer> lockOff = new HashSet<>(Arrays.asList(WifiLockStateChanged.State.OFF_VALUE));

        // Add state sets to the list in order.
        List<Set<Integer>> stateSet = Arrays.asList(lockOn, lockOff);

        createAndUploadConfig(atomTag, true);  // True: uses attribution.
        runDeviceTests(DEVICE_SIDE_TEST_PACKAGE, ".AtomTests", "testWifiLockHighPerf");

        // Sorted list of events in order in which they occurred.
        List<EventMetricData> data = getEventMetricDataList();

        // Assert that the events happened in the expected order.
        assertStatesOccurred(stateSet, data, WAIT_TIME_SHORT,
                atom -> atom.getWifiLockStateChanged().getState().getNumber());

        for (EventMetricData event : data) {
            assertThat(event.getAtom().getWifiLockStateChanged().getMode())
                .isEqualTo(WifiModeEnum.WIFI_MODE_FULL_HIGH_PERF);
        }
    }

    public void testWifiLockLowLatency() throws Exception {
        if (!hasFeature(FEATURE_WIFI, true)) return;
        if (!hasFeature(FEATURE_PC, false)) return;

        final int atomTag = Atom.WIFI_LOCK_STATE_CHANGED_FIELD_NUMBER;
        Set<Integer> lockOn = new HashSet<>(Arrays.asList(WifiLockStateChanged.State.ON_VALUE));
        Set<Integer> lockOff = new HashSet<>(Arrays.asList(WifiLockStateChanged.State.OFF_VALUE));

        // Add state sets to the list in order.
        List<Set<Integer>> stateSet = Arrays.asList(lockOn, lockOff);

        createAndUploadConfig(atomTag, true);  // True: uses attribution.
        runDeviceTests(DEVICE_SIDE_TEST_PACKAGE, ".AtomTests", "testWifiLockLowLatency");

        // Sorted list of events in order in which they occurred.
        List<EventMetricData> data = getEventMetricDataList();

        // Assert that the events happened in the expected order.
        assertStatesOccurred(stateSet, data, WAIT_TIME_SHORT,
                atom -> atom.getWifiLockStateChanged().getState().getNumber());

        for (EventMetricData event : data) {
            assertThat(event.getAtom().getWifiLockStateChanged().getMode())
                .isEqualTo(WifiModeEnum.WIFI_MODE_FULL_LOW_LATENCY);
        }
    }

    public void testWifiMulticastLock() throws Exception {
        if (!hasFeature(FEATURE_WIFI, true)) return;
        if (!hasFeature(FEATURE_PC, false)) return;

        final int atomTag = Atom.WIFI_MULTICAST_LOCK_STATE_CHANGED_FIELD_NUMBER;
        Set<Integer> lockOn = new HashSet<>(
                Arrays.asList(WifiMulticastLockStateChanged.State.ON_VALUE));
        Set<Integer> lockOff = new HashSet<>(
                Arrays.asList(WifiMulticastLockStateChanged.State.OFF_VALUE));

        final String EXPECTED_TAG = "StatsdCTSMulticastLock";

        // Add state sets to the list in order.
        List<Set<Integer>> stateSet = Arrays.asList(lockOn, lockOff);

        createAndUploadConfig(atomTag, true);
        runDeviceTests(DEVICE_SIDE_TEST_PACKAGE, ".AtomTests", "testWifiMulticastLock");

        // Sorted list of events in order in which they occurred.
        List<EventMetricData> data = getEventMetricDataList();

        // Assert that the events happened in the expected order.
        assertStatesOccurred(stateSet, data, WAIT_TIME_SHORT,
                atom -> atom.getWifiMulticastLockStateChanged().getState().getNumber());

        for (EventMetricData event: data) {
            String tag = event.getAtom().getWifiMulticastLockStateChanged().getTag();
            assertThat(tag).isEqualTo(EXPECTED_TAG);
        }
    }

    public void testWifiScan() throws Exception {
        if (!hasFeature(FEATURE_WIFI, true)) return;

        final int atom = Atom.WIFI_SCAN_STATE_CHANGED_FIELD_NUMBER;
        final int key = WifiScanStateChanged.STATE_FIELD_NUMBER;
        final int stateOn = WifiScanStateChanged.State.ON_VALUE;
        final int stateOff = WifiScanStateChanged.State.OFF_VALUE;
        final int minTimeDiffMillis = 250;
        final int maxTimeDiffMillis = 60_000;
        final boolean demandExactlyTwo = false; // Two scans are performed, so up to 4 atoms logged.

        List<EventMetricData> data = doDeviceMethodOnOff("testWifiScan", atom, key,
                stateOn, stateOff, minTimeDiffMillis, maxTimeDiffMillis, demandExactlyTwo);

        assertThat(data.size()).isIn(Range.closed(2, 4));
        WifiScanStateChanged a0 = data.get(0).getAtom().getWifiScanStateChanged();
        WifiScanStateChanged a1 = data.get(1).getAtom().getWifiScanStateChanged();
        assertThat(a0.getState().getNumber()).isEqualTo(stateOn);
        assertThat(a1.getState().getNumber()).isEqualTo(stateOff);
    }

    public void testBinderStats() throws Exception {
        try {
            unplugDevice();
            Thread.sleep(WAIT_TIME_SHORT);
            enableBinderStats();
            binderStatsNoSampling();
            resetBinderStats();
            StatsdConfig.Builder config = createConfigBuilder();
            addGaugeAtomWithDimensions(config, Atom.BINDER_CALLS_FIELD_NUMBER, null);

            uploadConfig(config);
            Thread.sleep(WAIT_TIME_SHORT);

            runActivity("StatsdCtsForegroundActivity", "action", "action.show_notification",3_000);

            setAppBreadcrumbPredicate();
            Thread.sleep(WAIT_TIME_SHORT);

            boolean found = false;
            int uid = getUid();
            List<Atom> atomList = getGaugeMetricDataList();
            for (Atom atom : atomList) {
                BinderCalls calls = atom.getBinderCalls();
                boolean classMatches = calls.getServiceClassName().contains(
                        "com.android.server.notification.NotificationManagerService");
                boolean methodMatches = calls.getServiceMethodName()
                        .equals("createNotificationChannels");

                if (calls.getUid() == uid && classMatches && methodMatches) {
                    found = true;
                    assertThat(calls.getRecordedCallCount()).isGreaterThan(0L);
                    assertThat(calls.getCallCount()).isGreaterThan(0L);
                    assertThat(calls.getRecordedTotalLatencyMicros())
                        .isIn(Range.open(0L, 1000000L));
                    assertThat(calls.getRecordedTotalCpuMicros()).isIn(Range.open(0L, 1000000L));
                }
            }

            assertWithMessage(String.format("Did not find a matching atom for uid %d", uid))
                .that(found).isTrue();

        } finally {
            disableBinderStats();
            plugInAc();
        }
    }

    public void testLooperStats() throws Exception {
        try {
            unplugDevice();
            setUpLooperStats();
            StatsdConfig.Builder config = createConfigBuilder();
            addGaugeAtomWithDimensions(config, Atom.LOOPER_STATS_FIELD_NUMBER, null);
            uploadConfig(config);
            Thread.sleep(WAIT_TIME_SHORT);

            runActivity("StatsdCtsForegroundActivity", "action", "action.show_notification", 3_000);

            setAppBreadcrumbPredicate();
            Thread.sleep(WAIT_TIME_SHORT);

            List<Atom> atomList = getGaugeMetricDataList();

            boolean found = false;
            int uid = getUid();
            for (Atom atom : atomList) {
                LooperStats stats = atom.getLooperStats();
                String notificationServiceFullName =
                        "com.android.server.notification.NotificationManagerService";
                boolean handlerMatches =
                        stats.getHandlerClassName().equals(
                                notificationServiceFullName + "$WorkerHandler");
                boolean messageMatches =
                        stats.getMessageName().equals(
                                notificationServiceFullName + "$EnqueueNotificationRunnable");
                if (atom.getLooperStats().getUid() == uid && handlerMatches && messageMatches) {
                    found = true;
                    assertThat(stats.getMessageCount()).isGreaterThan(0L);
                    assertThat(stats.getRecordedMessageCount()).isGreaterThan(0L);
                    assertThat(stats.getRecordedTotalLatencyMicros())
                        .isIn(Range.open(0L, 1000000L));
                    assertThat(stats.getRecordedTotalCpuMicros()).isIn(Range.open(0L, 1000000L));
                    assertThat(stats.getRecordedMaxLatencyMicros()).isIn(Range.open(0L, 1000000L));
                    assertThat(stats.getRecordedMaxCpuMicros()).isIn(Range.open(0L, 1000000L));
                    assertThat(stats.getRecordedDelayMessageCount()).isGreaterThan(0L);
                    assertThat(stats.getRecordedTotalDelayMillis())
                        .isIn(Range.closedOpen(0L, 5000L));
                    assertThat(stats.getRecordedMaxDelayMillis()).isIn(Range.closedOpen(0L, 5000L));
                }
            }
            assertWithMessage(String.format("Did not find a matching atom for uid %d", uid))
                .that(found).isTrue();
        } finally {
            cleanUpLooperStats();
            plugInAc();
        }
    }

    public void testProcessMemoryState() throws Exception {
        // Get ProcessMemoryState as a simple gauge metric.
        StatsdConfig.Builder config = createConfigBuilder();
        addGaugeAtomWithDimensions(config, Atom.PROCESS_MEMORY_STATE_FIELD_NUMBER, null);
        uploadConfig(config);
        Thread.sleep(WAIT_TIME_SHORT);

        // Start test app.
        try (AutoCloseable a = withActivity("StatsdCtsForegroundActivity", "action",
                "action.show_notification")) {
            Thread.sleep(WAIT_TIME_LONG);
            // Trigger a pull and wait for new pull before killing the process.
            setAppBreadcrumbPredicate();
            Thread.sleep(WAIT_TIME_LONG);
        }

        // Assert about ProcessMemoryState for the test app.
        List<Atom> atoms = getGaugeMetricDataList();
        int uid = getUid();
        boolean found = false;
        for (Atom atom : atoms) {
            ProcessMemoryState state = atom.getProcessMemoryState();
            if (state.getUid() != uid) {
                continue;
            }
            found = true;
            assertThat(state.getProcessName()).isEqualTo(DEVICE_SIDE_TEST_PACKAGE);
            assertThat(state.getOomAdjScore()).isAtLeast(0);
            assertThat(state.getPageFault()).isAtLeast(0L);
            assertThat(state.getPageMajorFault()).isAtLeast(0L);
            assertThat(state.getRssInBytes()).isGreaterThan(0L);
            assertThat(state.getCacheInBytes()).isAtLeast(0L);
            assertThat(state.getSwapInBytes()).isAtLeast(0L);
        }
        assertWithMessage(String.format("Did not find a matching atom for uid %d", uid))
            .that(found).isTrue();
    }

    public void testProcessMemoryHighWaterMark() throws Exception {
        // Get ProcessMemoryHighWaterMark as a simple gauge metric.
        StatsdConfig.Builder config = createConfigBuilder();
        addGaugeAtomWithDimensions(config, Atom.PROCESS_MEMORY_HIGH_WATER_MARK_FIELD_NUMBER, null);
        uploadConfig(config);
        Thread.sleep(WAIT_TIME_SHORT);

        // Start test app and trigger a pull while it is running.
        try (AutoCloseable a = withActivity("StatsdCtsForegroundActivity", "action",
                "action.show_notification")) {
            Thread.sleep(WAIT_TIME_SHORT);
            // Trigger a pull and wait for new pull before killing the process.
            setAppBreadcrumbPredicate();
            Thread.sleep(WAIT_TIME_LONG);
        }

        // Assert about ProcessMemoryHighWaterMark for the test app, statsd and system server.
        List<Atom> atoms = getGaugeMetricDataList();
        int uid = getUid();
        boolean foundTestApp = false;
        boolean foundStatsd = false;
        boolean foundSystemServer = false;
        for (Atom atom : atoms) {
            ProcessMemoryHighWaterMark state = atom.getProcessMemoryHighWaterMark();
            if (state.getUid() == uid) {
                foundTestApp = true;
                assertThat(state.getProcessName()).isEqualTo(DEVICE_SIDE_TEST_PACKAGE);
                assertThat(state.getRssHighWaterMarkInBytes()).isGreaterThan(0L);
            } else if (state.getProcessName().contains("/statsd")) {
                foundStatsd = true;
                assertThat(state.getRssHighWaterMarkInBytes()).isGreaterThan(0L);
            } else if (state.getProcessName().equals("system")) {
                foundSystemServer = true;
                assertThat(state.getRssHighWaterMarkInBytes()).isGreaterThan(0L);
            }
        }
        assertWithMessage(String.format("Did not find a matching atom for test app uid=%d",uid))
            .that(foundTestApp).isTrue();
        assertWithMessage("Did not find a matching atom for statsd").that(foundStatsd).isTrue();
        assertWithMessage("Did not find a matching atom for system server")
            .that(foundSystemServer).isTrue();
    }

    public void testProcessMemorySnapshot() throws Exception {
        // Get ProcessMemorySnapshot as a simple gauge metric.
        StatsdConfig.Builder config = createConfigBuilder();
        addGaugeAtomWithDimensions(config, Atom.PROCESS_MEMORY_SNAPSHOT_FIELD_NUMBER, null);
        uploadConfig(config);
        Thread.sleep(WAIT_TIME_SHORT);

        // Start test app and trigger a pull while it is running.
        try (AutoCloseable a = withActivity("StatsdCtsForegroundActivity", "action",
                "action.show_notification")) {
            Thread.sleep(WAIT_TIME_LONG);
            setAppBreadcrumbPredicate();
        }

        // Assert about ProcessMemorySnapshot for the test app, statsd and system server.
        List<Atom> atoms = getGaugeMetricDataList();
        int uid = getUid();
        boolean foundTestApp = false;
        boolean foundStatsd = false;
        boolean foundSystemServer = false;
        for (Atom atom : atoms) {
          ProcessMemorySnapshot snapshot = atom.getProcessMemorySnapshot();
          if (snapshot.getUid() == uid) {
              foundTestApp = true;
              assertThat(snapshot.getProcessName()).isEqualTo(DEVICE_SIDE_TEST_PACKAGE);
          } else if (snapshot.getProcessName().contains("/statsd")) {
              foundStatsd = true;
          } else if (snapshot.getProcessName().equals("system")) {
              foundSystemServer = true;
          }

          assertThat(snapshot.getPid()).isGreaterThan(0);
          assertThat(snapshot.getAnonRssAndSwapInKilobytes()).isAtLeast(0);
          assertThat(snapshot.getAnonRssAndSwapInKilobytes()).isEqualTo(
                  snapshot.getAnonRssInKilobytes() + snapshot.getSwapInKilobytes());
          assertThat(snapshot.getRssInKilobytes()).isAtLeast(0);
          assertThat(snapshot.getAnonRssInKilobytes()).isAtLeast(0);
          assertThat(snapshot.getSwapInKilobytes()).isAtLeast(0);
        }
        assertWithMessage(String.format("Did not find a matching atom for test app uid=%d",uid))
            .that(foundTestApp).isTrue();
        assertWithMessage("Did not find a matching atom for statsd").that(foundStatsd).isTrue();
        assertWithMessage("Did not find a matching atom for system server")
            .that(foundSystemServer).isTrue();
    }

    public void testIonHeapSize_optional() throws Exception {
        if (isIonHeapSizeMandatory()) {
            return;
        }

        List<Atom> atoms = pullIonHeapSizeAsGaugeMetric();
        if (atoms.isEmpty()) {
            // No support.
            return;
        }
        assertIonHeapSize(atoms);
    }

    public void testIonHeapSize_mandatory() throws Exception {
        if (!isIonHeapSizeMandatory()) {
            return;
        }

        List<Atom> atoms = pullIonHeapSizeAsGaugeMetric();
        assertIonHeapSize(atoms);
    }

    /** Returns whether IonHeapSize atom is supported. */
    private boolean isIonHeapSizeMandatory() throws Exception {
        // Support is guaranteed by libmeminfo VTS.
        return PropertyUtil.getFirstApiLevel(getDevice()) >= 30;
    }

    /** Returns IonHeapSize atoms pulled as a simple gauge metric while test app is running. */
    private List<Atom> pullIonHeapSizeAsGaugeMetric() throws Exception {
        // Get IonHeapSize as a simple gauge metric.
        StatsdConfig.Builder config = createConfigBuilder();
        addGaugeAtomWithDimensions(config, Atom.ION_HEAP_SIZE_FIELD_NUMBER, null);
        uploadConfig(config);
        Thread.sleep(WAIT_TIME_SHORT);

        // Start test app and trigger a pull while it is running.
        try (AutoCloseable a = withActivity("StatsdCtsForegroundActivity", "action",
                "action.show_notification")) {
            setAppBreadcrumbPredicate();
            Thread.sleep(WAIT_TIME_LONG);
        }

        return getGaugeMetricDataList();
    }

    private static void assertIonHeapSize(List<Atom> atoms) {
        assertThat(atoms).hasSize(1);
        IonHeapSize ionHeapSize = atoms.get(0).getIonHeapSize();
        assertThat(ionHeapSize.getTotalSizeKb()).isAtLeast(0);
    }

    /**
     * The the app id from a uid.
     *
     * @param uid The uid of the app
     *
     * @return The app id of the app
     *
     * @see android.os.UserHandle#getAppId
     */
    private static int getAppId(int uid) {
        return uid % 100000;
    }

    public void testRoleHolder() throws Exception {
        // Make device side test package a role holder
        String callScreenAppRole = "android.app.role.CALL_SCREENING";
        getDevice().executeShellCommand(
                "cmd role add-role-holder " + callScreenAppRole + " " + DEVICE_SIDE_TEST_PACKAGE);

        // Set up what to collect
        StatsdConfig.Builder config = createConfigBuilder();
        addGaugeAtomWithDimensions(config, Atom.ROLE_HOLDER_FIELD_NUMBER, null);
        uploadConfig(config);
        Thread.sleep(WAIT_TIME_SHORT);

        boolean verifiedKnowRoleState = false;

        // Pull a report
        setAppBreadcrumbPredicate();
        Thread.sleep(WAIT_TIME_SHORT);

        int testAppId = getAppId(getUid());

        for (Atom atom : getGaugeMetricDataList()) {
            AtomsProto.RoleHolder roleHolder = atom.getRoleHolder();

            assertThat(roleHolder.getPackageName()).isNotNull();
            assertThat(roleHolder.getUid()).isAtLeast(0);
            assertThat(roleHolder.getRole()).isNotNull();

            if (roleHolder.getPackageName().equals(DEVICE_SIDE_TEST_PACKAGE)) {
                assertThat(getAppId(roleHolder.getUid())).isEqualTo(testAppId);
                assertThat(roleHolder.getPackageName()).isEqualTo(DEVICE_SIDE_TEST_PACKAGE);
                assertThat(roleHolder.getRole()).isEqualTo(callScreenAppRole);

                verifiedKnowRoleState = true;
            }
        }

        assertThat(verifiedKnowRoleState).isTrue();
    }

    public void testDangerousPermissionState() throws Exception {
        final int FLAG_PERMISSION_USER_SENSITIVE_WHEN_GRANTED =  1 << 8;
        final int FLAG_PERMISSION_USER_SENSITIVE_WHEN_DENIED =  1 << 9;

        // Set up what to collect
        StatsdConfig.Builder config = createConfigBuilder();
        addGaugeAtomWithDimensions(config, Atom.DANGEROUS_PERMISSION_STATE_FIELD_NUMBER, null);
        uploadConfig(config);
        Thread.sleep(WAIT_TIME_SHORT);

        boolean verifiedKnowPermissionState = false;

        // Pull a report
        setAppBreadcrumbPredicate();
        Thread.sleep(WAIT_TIME_SHORT);

        int testAppId = getAppId(getUid());

        for (Atom atom : getGaugeMetricDataList()) {
            DangerousPermissionState permissionState = atom.getDangerousPermissionState();

            assertThat(permissionState.getPermissionName()).isNotNull();
            assertThat(permissionState.getUid()).isAtLeast(0);
            assertThat(permissionState.getPackageName()).isNotNull();

            if (getAppId(permissionState.getUid()) == testAppId) {

                if (permissionState.getPermissionName().contains(
                        "ACCESS_FINE_LOCATION")) {
                    assertThat(permissionState.getIsGranted()).isTrue();
                    assertThat(permissionState.getPermissionFlags() & ~(
                            FLAG_PERMISSION_USER_SENSITIVE_WHEN_DENIED
                            | FLAG_PERMISSION_USER_SENSITIVE_WHEN_GRANTED))
                        .isEqualTo(0);

                    verifiedKnowPermissionState = true;
                }
            }
        }

        assertThat(verifiedKnowPermissionState).isTrue();
    }

    public void testDangerousPermissionStateSampled() throws Exception {
        // get full atom for reference
        StatsdConfig.Builder config = createConfigBuilder();
        addGaugeAtomWithDimensions(config, Atom.DANGEROUS_PERMISSION_STATE_FIELD_NUMBER, null);
        uploadConfig(config);
        Thread.sleep(WAIT_TIME_SHORT);

        setAppBreadcrumbPredicate();
        Thread.sleep(WAIT_TIME_SHORT);

        List<DangerousPermissionState> fullDangerousPermissionState = new ArrayList<>();
        for (Atom atom : getGaugeMetricDataList()) {
            fullDangerousPermissionState.add(atom.getDangerousPermissionState());
        }

        removeConfig(CONFIG_ID);
        getReportList(); // Clears data.
        List<Atom> gaugeMetricDataList = null;

        // retries in case sampling returns full list or empty list - which should be extremely rare
        for (int attempt = 0; attempt < 10; attempt++) {
            // Set up what to collect
            config = createConfigBuilder();
            addGaugeAtomWithDimensions(config, Atom.DANGEROUS_PERMISSION_STATE_SAMPLED_FIELD_NUMBER,
                    null);
            uploadConfig(config);
            Thread.sleep(WAIT_TIME_SHORT);

            // Pull a report
            setAppBreadcrumbPredicate();
            Thread.sleep(WAIT_TIME_SHORT);

            gaugeMetricDataList = getGaugeMetricDataList();
            if (gaugeMetricDataList.size() > 0
                    && gaugeMetricDataList.size() < fullDangerousPermissionState.size()) {
                break;
            }
            removeConfig(CONFIG_ID);
            getReportList(); // Clears data.
        }
        assertThat(gaugeMetricDataList.size()).isGreaterThan(0);
        assertThat(gaugeMetricDataList.size()).isLessThan(fullDangerousPermissionState.size());

        long lastUid = -1;
        int fullIndex = 0;

        for (Atom atom : getGaugeMetricDataList()) {
            DangerousPermissionStateSampled permissionState =
                    atom.getDangerousPermissionStateSampled();

            DangerousPermissionState referenceState = fullDangerousPermissionState.get(fullIndex);

            if (referenceState.getUid() != permissionState.getUid()) {
                // atoms are sampled on uid basis if uid is present, all related permissions must
                // be logged.
                assertThat(permissionState.getUid()).isNotEqualTo(lastUid);
                continue;
            }

            lastUid = permissionState.getUid();

            assertThat(permissionState.getPermissionFlags()).isEqualTo(
                    referenceState.getPermissionFlags());
            assertThat(permissionState.getIsGranted()).isEqualTo(referenceState.getIsGranted());
            assertThat(permissionState.getPermissionName()).isEqualTo(
                    referenceState.getPermissionName());

            fullIndex++;
        }
    }

    public void testAppOps() throws Exception {
        // Set up what to collect
        StatsdConfig.Builder config = createConfigBuilder();
        addGaugeAtomWithDimensions(config, Atom.APP_OPS_FIELD_NUMBER, null);
        uploadConfig(config);

        runDeviceTests(DEVICE_SIDE_TEST_PACKAGE, ".AtomTests", "testAppOps");
        Thread.sleep(WAIT_TIME_SHORT);

        // Pull a report
        setAppBreadcrumbPredicate();
        Thread.sleep(WAIT_TIME_SHORT);

        ArrayList<Integer> expectedOps = new ArrayList<>();
        for (int i = 0; i < NUM_APP_OPS; i++) {
            expectedOps.add(i);
        }

        for (Descriptors.EnumValueDescriptor valueDescriptor :
                AttributedAppOps.getDefaultInstance().getOp().getDescriptorForType().getValues()) {
            if (valueDescriptor.getOptions().hasDeprecated()) {
                // Deprecated app op, remove from list of expected ones.
                expectedOps.remove(expectedOps.indexOf(valueDescriptor.getNumber()));
            }
        }
        for (Atom atom : getGaugeMetricDataList()) {

            AppOps appOps = atom.getAppOps();
            if (appOps.getPackageName().equals(TEST_PACKAGE_NAME)) {
                if (appOps.getOpId().getNumber() == -1) {
                    continue;
                }
                long totalNoted = appOps.getTrustedForegroundGrantedCount()
                        + appOps.getTrustedBackgroundGrantedCount()
                        + appOps.getTrustedForegroundRejectedCount()
                        + appOps.getTrustedBackgroundRejectedCount();
                assertWithMessage("Operation in APP_OPS_ENUM_MAP: " + appOps.getOpId().getNumber())
                        .that(totalNoted - 1).isEqualTo(appOps.getOpId().getNumber());
                assertWithMessage("Unexpected Op reported").that(expectedOps).contains(
                        appOps.getOpId().getNumber());
                expectedOps.remove(expectedOps.indexOf(appOps.getOpId().getNumber()));
            }
        }
        assertWithMessage("Logging app op ids are missing in report.").that(expectedOps).isEmpty();
    }

    public void testANROccurred() throws Exception {
        final int atomTag = Atom.ANR_OCCURRED_FIELD_NUMBER;
        createAndUploadConfig(atomTag, false);
        Thread.sleep(WAIT_TIME_SHORT);

        try (AutoCloseable a = withActivity("ANRActivity", null, null)) {
            Thread.sleep(WAIT_TIME_SHORT);
            getDevice().executeShellCommand(
                    "am broadcast -a action_anr -p " + DEVICE_SIDE_TEST_PACKAGE);
            Thread.sleep(20_000);
        }

        // Sorted list of events in order in which they occurred.
        List<EventMetricData> data = getEventMetricDataList();

        assertThat(data).hasSize(1);
        assertThat(data.get(0).getAtom().hasAnrOccurred()).isTrue();
        ANROccurred atom = data.get(0).getAtom().getAnrOccurred();
        assertThat(atom.getIsInstantApp().getNumber())
            .isEqualTo(ANROccurred.InstantApp.FALSE_VALUE);
        assertThat(atom.getForegroundState().getNumber())
            .isEqualTo(ANROccurred.ForegroundState.FOREGROUND_VALUE);
        assertThat(atom.getErrorSource()).isEqualTo(ErrorSource.DATA_APP);
        assertThat(atom.getPackageName()).isEqualTo(DEVICE_SIDE_TEST_PACKAGE);
    }

    public void testWriteRawTestAtom() throws Exception {
        final int atomTag = Atom.TEST_ATOM_REPORTED_FIELD_NUMBER;
        createAndUploadConfig(atomTag, true);
        Thread.sleep(WAIT_TIME_SHORT);

        runDeviceTests(DEVICE_SIDE_TEST_PACKAGE, ".AtomTests", "testWriteRawTestAtom");

        Thread.sleep(WAIT_TIME_SHORT);
        // Sorted list of events in order in which they occurred.
        List<EventMetricData> data = getEventMetricDataList();
        assertThat(data).hasSize(4);

        TestAtomReported atom = data.get(0).getAtom().getTestAtomReported();
        List<AttributionNode> attrChain = atom.getAttributionNodeList();
        assertThat(attrChain).hasSize(2);
        assertThat(attrChain.get(0).getUid()).isEqualTo(1234);
        assertThat(attrChain.get(0).getTag()).isEqualTo("tag1");
        assertThat(attrChain.get(1).getUid()).isEqualTo(getUid());
        assertThat(attrChain.get(1).getTag()).isEqualTo("tag2");

        assertThat(atom.getIntField()).isEqualTo(42);
        assertThat(atom.getLongField()).isEqualTo(Long.MAX_VALUE);
        assertThat(atom.getFloatField()).isEqualTo(3.14f);
        assertThat(atom.getStringField()).isEqualTo("This is a basic test!");
        assertThat(atom.getBooleanField()).isFalse();
        assertThat(atom.getState().getNumber()).isEqualTo(TestAtomReported.State.ON_VALUE);
        assertThat(atom.getBytesField().getExperimentIdList())
            .containsExactly(1L, 2L, 3L).inOrder();


        atom = data.get(1).getAtom().getTestAtomReported();
        attrChain = atom.getAttributionNodeList();
        assertThat(attrChain).hasSize(2);
        assertThat(attrChain.get(0).getUid()).isEqualTo(9999);
        assertThat(attrChain.get(0).getTag()).isEqualTo("tag9999");
        assertThat(attrChain.get(1).getUid()).isEqualTo(getUid());
        assertThat(attrChain.get(1).getTag()).isEmpty();

        assertThat(atom.getIntField()).isEqualTo(100);
        assertThat(atom.getLongField()).isEqualTo(Long.MIN_VALUE);
        assertThat(atom.getFloatField()).isEqualTo(-2.5f);
        assertThat(atom.getStringField()).isEqualTo("Test null uid");
        assertThat(atom.getBooleanField()).isTrue();
        assertThat(atom.getState().getNumber()).isEqualTo(TestAtomReported.State.UNKNOWN_VALUE);
        assertThat(atom.getBytesField().getExperimentIdList())
            .containsExactly(1L, 2L, 3L).inOrder();

        atom = data.get(2).getAtom().getTestAtomReported();
        attrChain = atom.getAttributionNodeList();
        assertThat(attrChain).hasSize(1);
        assertThat(attrChain.get(0).getUid()).isEqualTo(getUid());
        assertThat(attrChain.get(0).getTag()).isEqualTo("tag1");

        assertThat(atom.getIntField()).isEqualTo(-256);
        assertThat(atom.getLongField()).isEqualTo(-1234567890L);
        assertThat(atom.getFloatField()).isEqualTo(42.01f);
        assertThat(atom.getStringField()).isEqualTo("Test non chained");
        assertThat(atom.getBooleanField()).isTrue();
        assertThat(atom.getState().getNumber()).isEqualTo(TestAtomReported.State.OFF_VALUE);
        assertThat(atom.getBytesField().getExperimentIdList())
            .containsExactly(1L, 2L, 3L).inOrder();

        atom = data.get(3).getAtom().getTestAtomReported();
        attrChain = atom.getAttributionNodeList();
        assertThat(attrChain).hasSize(1);
        assertThat(attrChain.get(0).getUid()).isEqualTo(getUid());
        assertThat(attrChain.get(0).getTag()).isEmpty();

        assertThat(atom.getIntField()).isEqualTo(0);
        assertThat(atom.getLongField()).isEqualTo(0L);
        assertThat(atom.getFloatField()).isEqualTo(0f);
        assertThat(atom.getStringField()).isEmpty();
        assertThat(atom.getBooleanField()).isTrue();
        assertThat(atom.getState().getNumber()).isEqualTo(TestAtomReported.State.OFF_VALUE);
        assertThat(atom.getBytesField().getExperimentIdList()).isEmpty();
    }

    public void testNotificationPackagePreferenceExtraction() throws Exception {
        StatsdConfig.Builder config = createConfigBuilder();
        addGaugeAtomWithDimensions(config,
                    Atom.PACKAGE_NOTIFICATION_PREFERENCES_FIELD_NUMBER,
                    null);
        uploadConfig(config);
        Thread.sleep(WAIT_TIME_SHORT);
        runActivity("StatsdCtsForegroundActivity", "action", "action.show_notification");
        Thread.sleep(WAIT_TIME_SHORT);
        setAppBreadcrumbPredicate();
        Thread.sleep(WAIT_TIME_SHORT);

        List<PackageNotificationPreferences> allPreferences = new ArrayList<>();
        for (Atom atom : getGaugeMetricDataList()){
            if(atom.hasPackageNotificationPreferences()) {
                allPreferences.add(atom.getPackageNotificationPreferences());
            }
        }
        assertThat(allPreferences.size()).isGreaterThan(0);

        boolean foundTestPackagePreferences = false;
        int uid = getUid();
        for (PackageNotificationPreferences pref : allPreferences) {
            assertThat(pref.getUid()).isGreaterThan(0);
            assertTrue(pref.hasImportance());
            assertTrue(pref.hasVisibility());
            assertTrue(pref.hasUserLockedFields());
            if(pref.getUid() == uid){
                assertThat(pref.getImportance()).isEqualTo(-1000);  //UNSPECIFIED_IMPORTANCE
                assertThat(pref.getVisibility()).isEqualTo(-1000);  //UNSPECIFIED_VISIBILITY
                foundTestPackagePreferences = true;
            }
        }
        assertTrue(foundTestPackagePreferences);
    }

    public void testNotificationChannelPreferencesExtraction() throws Exception {
        StatsdConfig.Builder config = createConfigBuilder();
        addGaugeAtomWithDimensions(config,
                    Atom.PACKAGE_NOTIFICATION_CHANNEL_PREFERENCES_FIELD_NUMBER,
                    null);
        uploadConfig(config);
        Thread.sleep(WAIT_TIME_SHORT);
        runActivity("StatsdCtsForegroundActivity", "action", "action.show_notification");
        Thread.sleep(WAIT_TIME_SHORT);
        setAppBreadcrumbPredicate();
        Thread.sleep(WAIT_TIME_SHORT);

        List<PackageNotificationChannelPreferences> allChannelPreferences = new ArrayList<>();
        for(Atom atom : getGaugeMetricDataList()) {
            if (atom.hasPackageNotificationChannelPreferences()) {
               allChannelPreferences.add(atom.getPackageNotificationChannelPreferences());
            }
        }
        assertThat(allChannelPreferences.size()).isGreaterThan(0);

        boolean foundTestPackagePreferences = false;
        int uid = getUid();
        for (PackageNotificationChannelPreferences pref : allChannelPreferences) {
            assertThat(pref.getUid()).isGreaterThan(0);
            assertTrue(pref.hasChannelId());
            assertTrue(pref.hasChannelName());
            assertTrue(pref.hasDescription());
            assertTrue(pref.hasImportance());
            assertTrue(pref.hasUserLockedFields());
            assertTrue(pref.hasIsDeleted());
            if(uid == pref.getUid() && pref.getChannelId().equals("StatsdCtsChannel")) {
                assertThat(pref.getChannelName()).isEqualTo("Statsd Cts");
                assertThat(pref.getDescription()).isEqualTo("Statsd Cts Channel");
                assertThat(pref.getImportance()).isEqualTo(3);  // IMPORTANCE_DEFAULT
                foundTestPackagePreferences = true;
            }
        }
        assertTrue(foundTestPackagePreferences);
    }

    public void testNotificationChannelGroupPreferencesExtraction() throws Exception {
        StatsdConfig.Builder config = createConfigBuilder();
        addGaugeAtomWithDimensions(config,
                    Atom.PACKAGE_NOTIFICATION_CHANNEL_GROUP_PREFERENCES_FIELD_NUMBER,
                    null);
        uploadConfig(config);
        Thread.sleep(WAIT_TIME_SHORT);
        runActivity("StatsdCtsForegroundActivity", "action", "action.create_channel_group");
        Thread.sleep(WAIT_TIME_SHORT);
        setAppBreadcrumbPredicate();
        Thread.sleep(WAIT_TIME_SHORT);

        List<PackageNotificationChannelGroupPreferences> allGroupPreferences = new ArrayList<>();
        for(Atom atom : getGaugeMetricDataList()) {
            if (atom.hasPackageNotificationChannelGroupPreferences()) {
                allGroupPreferences.add(atom.getPackageNotificationChannelGroupPreferences());
            }
        }
        assertThat(allGroupPreferences.size()).isGreaterThan(0);

        boolean foundTestPackagePreferences = false;
        int uid = getUid();
        for(PackageNotificationChannelGroupPreferences pref : allGroupPreferences) {
            assertThat(pref.getUid()).isGreaterThan(0);
            assertTrue(pref.hasGroupId());
            assertTrue(pref.hasGroupName());
            assertTrue(pref.hasDescription());
            assertTrue(pref.hasIsBlocked());
            assertTrue(pref.hasUserLockedFields());
            if(uid == pref.getUid() && pref.getGroupId().equals("StatsdCtsGroup")) {
                assertThat(pref.getGroupName()).isEqualTo("Statsd Cts Group");
                assertThat(pref.getDescription()).isEqualTo("StatsdCtsGroup Description");
                assertThat(pref.getIsBlocked()).isFalse();
                foundTestPackagePreferences = true;
            }
        }
        assertTrue(foundTestPackagePreferences);
    }

    public void testNotificationReported() throws Exception {
        StatsdConfig.Builder config = getPulledConfig();
        addAtomEvent(config, Atom.NOTIFICATION_REPORTED_FIELD_NUMBER,
            Arrays.asList(createFvm(NotificationReported.PACKAGE_NAME_FIELD_NUMBER)
                              .setEqString(DEVICE_SIDE_TEST_PACKAGE)));
        uploadConfig(config);
        Thread.sleep(WAIT_TIME_SHORT);
        runActivity("StatsdCtsForegroundActivity", "action", "action.show_notification");
        Thread.sleep(WAIT_TIME_SHORT);

        // Sorted list of events in order in which they occurred.
        List<EventMetricData> data = getEventMetricDataList();
        assertThat(data).hasSize(1);
        assertThat(data.get(0).getAtom().hasNotificationReported()).isTrue();
        AtomsProto.NotificationReported n = data.get(0).getAtom().getNotificationReported();
        assertThat(n.getPackageName()).isEqualTo(DEVICE_SIDE_TEST_PACKAGE);
        assertThat(n.getUid()).isEqualTo(getUid());
        assertThat(n.getNotificationIdHash()).isEqualTo(1);  // smallHash(0x7f080001)
        assertThat(n.getChannelIdHash()).isEqualTo(SmallHash.hash("StatsdCtsChannel"));
        assertThat(n.getGroupIdHash()).isEqualTo(0);
        assertFalse(n.getIsGroupSummary());
        assertThat(n.getCategory()).isEmpty();
        assertThat(n.getStyle()).isEqualTo(0);
        assertThat(n.getNumPeople()).isEqualTo(0);
    }

    public void testSettingsStatsReported() throws Exception {
        // Base64 encoded proto com.android.service.nano.StringListParamProto,
        // which contains two strings "font_scale" and "screen_auto_brightness_adj".
        final String encoded = "ChpzY3JlZW5fYXV0b19icmlnaHRuZXNzX2FkagoKZm9udF9zY2FsZQ";
        final String font_scale = "font_scale";
        SettingSnapshot snapshot = null;

        // Set whitelist through device config.
        Thread.sleep(WAIT_TIME_SHORT);
        getDevice().executeShellCommand(
                "device_config put settings_stats SystemFeature__float_whitelist " + encoded);
        Thread.sleep(WAIT_TIME_SHORT);
        // Set font_scale value
        getDevice().executeShellCommand("settings put system font_scale 1.5");

        // Get SettingSnapshot as a simple gauge metric.
        StatsdConfig.Builder config = createConfigBuilder();
        addGaugeAtomWithDimensions(config, Atom.SETTING_SNAPSHOT_FIELD_NUMBER, null);
        uploadConfig(config);
        Thread.sleep(WAIT_TIME_SHORT);

        // Start test app and trigger a pull while it is running.
        try (AutoCloseable a = withActivity("StatsdCtsForegroundActivity", "action",
                "action.show_notification")) {
            Thread.sleep(WAIT_TIME_SHORT);
            // Trigger a pull and wait for new pull before killing the process.
            setAppBreadcrumbPredicate();
            Thread.sleep(WAIT_TIME_LONG);
        }

        // Test the size of atoms. It should contain at least "font_scale" and
        // "screen_auto_brightness_adj" two setting values.
        List<Atom> atoms = getGaugeMetricDataList();
        assertThat(atoms.size()).isAtLeast(2);
        for (Atom atom : atoms) {
            SettingSnapshot settingSnapshot = atom.getSettingSnapshot();
            if (font_scale.equals(settingSnapshot.getName())) {
                snapshot = settingSnapshot;
                break;
            }
        }

        Thread.sleep(WAIT_TIME_SHORT);
        // Test the data of atom.
        assertNotNull(snapshot);
        // Get font_scale value and test value type.
        final float fontScale = Float.parseFloat(
                getDevice().executeShellCommand("settings get system font_scale"));
        assertThat(snapshot.getType()).isEqualTo(
                SettingSnapshot.SettingsValueType.ASSIGNED_FLOAT_TYPE);
        assertThat(snapshot.getBoolValue()).isEqualTo(false);
        assertThat(snapshot.getIntValue()).isEqualTo(0);
        assertThat(snapshot.getFloatValue()).isEqualTo(fontScale);
        assertThat(snapshot.getStrValue()).isEqualTo("");
        assertThat(snapshot.getUserId()).isEqualTo(0);
    }

    public void testIntegrityCheckAtomReportedDuringInstall() throws Exception {
        createAndUploadConfig(AtomsProto.Atom.INTEGRITY_CHECK_RESULT_REPORTED_FIELD_NUMBER);

        getDevice().uninstallPackage(DEVICE_SIDE_TEST_PACKAGE);
        installTestApp();

        List<EventMetricData> data = getEventMetricDataList();

        assertThat(data.size()).isEqualTo(1);
        assertThat(data.get(0).getAtom().hasIntegrityCheckResultReported()).isTrue();
        IntegrityCheckResultReported result = data.get(0)
                .getAtom().getIntegrityCheckResultReported();
        assertThat(result.getPackageName()).isEqualTo(DEVICE_SIDE_TEST_PACKAGE);
        // we do not assert on certificates since it seem to differ by device.
        assertThat(result.getInstallerPackageName()).isEqualTo("adb");
        assertThat(result.getVersionCode()).isEqualTo(DEVICE_SIDE_TEST_PACKAGE_VERSION);
        assertThat(result.getResponse()).isEqualTo(ALLOWED);
        assertThat(result.getCausedByAppCertRule()).isFalse();
        assertThat(result.getCausedByInstallerRule()).isFalse();
    }

    public void testMobileBytesTransfer() throws Throwable {
        final int appUid = getUid();

        // Verify MobileBytesTransfer, passing a ThrowingPredicate that verifies contents of
        // corresponding atom type to prevent code duplication. The passed predicate returns
        // true if the atom of appUid is found, false otherwise, and throws an exception if
        // contents are not expected.
        doTestMobileBytesTransferThat(Atom.MOBILE_BYTES_TRANSFER_FIELD_NUMBER, (atom) -> {
            final AtomsProto.MobileBytesTransfer data = ((Atom) atom).getMobileBytesTransfer();
            if (data.getUid() == appUid) {
                assertDataUsageAtomDataExpected(data.getRxBytes(), data.getTxBytes(),
                        data.getRxPackets(), data.getTxPackets());
                return true; // found
            }
            return false;
        });
    }

    public void testMobileBytesTransferByFgBg() throws Throwable {
        final int appUid = getUid();

        doTestMobileBytesTransferThat(Atom.MOBILE_BYTES_TRANSFER_BY_FG_BG_FIELD_NUMBER, (atom) -> {
            final AtomsProto.MobileBytesTransferByFgBg data =
                    ((Atom) atom).getMobileBytesTransferByFgBg();
            if (data.getUid() == appUid && data.getIsForeground()) {
                assertDataUsageAtomDataExpected(data.getRxBytes(), data.getTxBytes(),
                        data.getRxPackets(), data.getTxPackets());
                return true; // found
            }
            return false;
        });
    }

    public void testDataUsageBytesTransfer() throws Throwable {
        final boolean subtypeCombined = getNetworkStatsCombinedSubTypeEnabled();

        doTestMobileBytesTransferThat(Atom.DATA_USAGE_BYTES_TRANSFER_FIELD_NUMBER, (atom) -> {
            final AtomsProto.DataUsageBytesTransfer data =
                    ((Atom) atom).getDataUsageBytesTransfer();
            if (data.getState() == 1 /*NetworkStats.SET_FOREGROUND*/) {
                assertDataUsageAtomDataExpected(data.getRxBytes(), data.getTxBytes(),
                        data.getRxPackets(), data.getTxPackets());
                // TODO: verify the RAT type field with the value gotten from device.
                if (subtypeCombined) {
                    assertThat(data.getRatType()).isEqualTo(
                            NetworkTypeEnum.NETWORK_TYPE_UNKNOWN_VALUE);
                } else {
                    assertThat(data.getRatType()).isGreaterThan(
                            NetworkTypeEnum.NETWORK_TYPE_UNKNOWN_VALUE);
                }

                // Assert that subscription info is valid.
                assertThat(data.getSimMcc()).matches("^\\d{3}$");
                assertThat(data.getSimMnc()).matches("^\\d{2,3}$");
                assertThat(data.getCarrierId()).isNotEqualTo(
                        -1); // TelephonyManager#UNKNOWN_CARRIER_ID

                return true; // found
            }
            return false;
        });
    }

    // TODO(b/157651730): Determine how to test tag and metered state within atom.
    public void testBytesTransferByTagAndMetered() throws Throwable {
        final int appUid = getUid();
        final int atomId = Atom.BYTES_TRANSFER_BY_TAG_AND_METERED_FIELD_NUMBER;

        doTestMobileBytesTransferThat(atomId, (atom) -> {
            final AtomsProto.BytesTransferByTagAndMetered data =
                    ((Atom) atom).getBytesTransferByTagAndMetered();
            if (data.getUid() == appUid && data.getTag() == 0 /*app traffic generated on tag 0*/) {
                assertDataUsageAtomDataExpected(data.getRxBytes(), data.getTxBytes(),
                        data.getRxPackets(), data.getTxPackets());
                return true; // found
            }
            return false;
        });
    }

    public void testIsolatedToHostUidMapping() throws Exception {
        createAndUploadConfig(Atom.APP_BREADCRUMB_REPORTED_FIELD_NUMBER, /*useAttribution=*/false);
        Thread.sleep(WAIT_TIME_SHORT);

        // Create an isolated service from which An AppBreadcrumbReported atom is written.
        runDeviceTests(DEVICE_SIDE_TEST_PACKAGE, ".AtomTests", "testIsolatedProcessService");

        List<EventMetricData> data = getEventMetricDataList();
        assertThat(data).hasSize(1);
        AppBreadcrumbReported atom = data.get(0).getAtom().getAppBreadcrumbReported();
        assertThat(atom.getUid()).isEqualTo(getUid());
        assertThat(atom.getLabel()).isEqualTo(0);
        assertThat(atom.getState()).isEqualTo(AppBreadcrumbReported.State.START);
    }

    public void testPushedBlobStoreStats() throws Exception {
        StatsdConfig.Builder conf = createConfigBuilder();
        addAtomEvent(conf, Atom.BLOB_COMMITTED_FIELD_NUMBER, false);
        addAtomEvent(conf, Atom.BLOB_LEASED_FIELD_NUMBER, false);
        addAtomEvent(conf, Atom.BLOB_OPENED_FIELD_NUMBER, false);
        uploadConfig(conf);

        Thread.sleep(WAIT_TIME_SHORT);

        runDeviceTests(DEVICE_SIDE_TEST_PACKAGE, ".AtomTests", "testBlobStore");

        List<EventMetricData> data = getEventMetricDataList();
        assertThat(data).hasSize(3);

        BlobCommitted blobCommitted = data.get(0).getAtom().getBlobCommitted();
        final long blobId = blobCommitted.getBlobId();
        final long blobSize = blobCommitted.getSize();
        assertThat(blobCommitted.getUid()).isEqualTo(getUid());
        assertThat(blobId).isNotEqualTo(0);
        assertThat(blobSize).isNotEqualTo(0);
        assertThat(blobCommitted.getResult()).isEqualTo(BlobCommitted.Result.SUCCESS);

        BlobLeased blobLeased = data.get(1).getAtom().getBlobLeased();
        assertThat(blobLeased.getUid()).isEqualTo(getUid());
        assertThat(blobLeased.getBlobId()).isEqualTo(blobId);
        assertThat(blobLeased.getSize()).isEqualTo(blobSize);
        assertThat(blobLeased.getResult()).isEqualTo(BlobLeased.Result.SUCCESS);

        BlobOpened blobOpened = data.get(2).getAtom().getBlobOpened();
        assertThat(blobOpened.getUid()).isEqualTo(getUid());
        assertThat(blobOpened.getBlobId()).isEqualTo(blobId);
        assertThat(blobOpened.getSize()).isEqualTo(blobSize);
        assertThat(blobOpened.getResult()).isEqualTo(BlobOpened.Result.SUCCESS);
    }

    // Constants that match the constants for AtomTests#testBlobStore
    private static final long BLOB_COMMIT_CALLBACK_TIMEOUT_SEC = 5;
    private static final long BLOB_EXPIRY_DURATION_MS = 24 * 60 * 60 * 1000;
    private static final long BLOB_FILE_SIZE_BYTES = 23 * 1024L;
    private static final long BLOB_LEASE_EXPIRY_DURATION_MS = 60 * 60 * 1000;

    public void testPulledBlobStoreStats() throws Exception {
        StatsdConfig.Builder config = createConfigBuilder();
        addGaugeAtomWithDimensions(config,
                Atom.BLOB_INFO_FIELD_NUMBER,
                null);
        uploadConfig(config);

        final long testStartTimeMs = System.currentTimeMillis();
        Thread.sleep(WAIT_TIME_SHORT);
        runDeviceTests(DEVICE_SIDE_TEST_PACKAGE, ".AtomTests", "testBlobStore");
        Thread.sleep(WAIT_TIME_LONG);
        setAppBreadcrumbPredicate();
        Thread.sleep(WAIT_TIME_SHORT);

        // Add commit callback time to test end time to account for async execution
        final long testEndTimeMs =
                System.currentTimeMillis() + BLOB_COMMIT_CALLBACK_TIMEOUT_SEC * 1000;

        // Find the BlobInfo for the blob created in the test run
        AtomsProto.BlobInfo blobInfo = null;
        for (Atom atom : getGaugeMetricDataList()) {
            if (atom.hasBlobInfo()) {
                final AtomsProto.BlobInfo temp = atom.getBlobInfo();
                if (temp.getCommitters().getCommitter(0).getUid() == getUid()) {
                    blobInfo = temp;
                    break;
                }
            }
        }
        assertThat(blobInfo).isNotNull();

        assertThat(blobInfo.getSize()).isEqualTo(BLOB_FILE_SIZE_BYTES);

        // Check that expiry time is reasonable
        assertThat(blobInfo.getExpiryTimestampMillis()).isGreaterThan(
                testStartTimeMs + BLOB_EXPIRY_DURATION_MS);
        assertThat(blobInfo.getExpiryTimestampMillis()).isLessThan(
                testEndTimeMs + BLOB_EXPIRY_DURATION_MS);

        // Check that commit time is reasonable
        final long commitTimeMs = blobInfo.getCommitters().getCommitter(
                0).getCommitTimestampMillis();
        assertThat(commitTimeMs).isGreaterThan(testStartTimeMs);
        assertThat(commitTimeMs).isLessThan(testEndTimeMs);

        // Check that WHITELIST and PRIVATE access mode flags are set
        assertThat(blobInfo.getCommitters().getCommitter(0).getAccessMode()).isEqualTo(0b1001);
        assertThat(blobInfo.getCommitters().getCommitter(0).getNumWhitelistedPackage()).isEqualTo(
                1);

        assertThat(blobInfo.getLeasees().getLeaseeCount()).isGreaterThan(0);
        assertThat(blobInfo.getLeasees().getLeasee(0).getUid()).isEqualTo(getUid());

        // Check that lease expiry time is reasonable
        final long leaseExpiryMs = blobInfo.getLeasees().getLeasee(
                0).getLeaseExpiryTimestampMillis();
        assertThat(leaseExpiryMs).isGreaterThan(testStartTimeMs + BLOB_LEASE_EXPIRY_DURATION_MS);
        assertThat(leaseExpiryMs).isLessThan(testEndTimeMs + BLOB_LEASE_EXPIRY_DURATION_MS);
    }

    private void assertDataUsageAtomDataExpected(long rxb, long txb, long rxp, long txp) {
        assertThat(rxb).isGreaterThan(0L);
        assertThat(txb).isGreaterThan(0L);
        assertThat(rxp).isGreaterThan(0L);
        assertThat(txp).isGreaterThan(0L);
    }

    private void doTestMobileBytesTransferThat(int atomTag, ThrowingPredicate p)
            throws Throwable {
        if (!hasFeature(FEATURE_TELEPHONY, true)) return;

        // Get MobileBytesTransfer as a simple gauge metric.
        final StatsdConfig.Builder config = getPulledConfig();
        addGaugeAtomWithDimensions(config, atomTag, null);
        uploadConfig(config);
        Thread.sleep(WAIT_TIME_SHORT);

        // Generate some traffic on mobile network.
        runDeviceTests(DEVICE_SIDE_TEST_PACKAGE, ".AtomTests", "testGenerateMobileTraffic");
        Thread.sleep(WAIT_TIME_SHORT);

        // Force polling NetworkStatsService to get most updated network stats from lower layer.
        runActivity("StatsdCtsForegroundActivity", "action", "action.poll_network_stats");
        Thread.sleep(WAIT_TIME_SHORT);

        // Pull a report
        setAppBreadcrumbPredicate();
        Thread.sleep(WAIT_TIME_SHORT);

        final List<Atom> atoms = getGaugeMetricDataList(/*checkTimestampTruncated=*/true);
        assertThat(atoms.size()).isAtLeast(1);

        boolean foundAppStats = false;
        for (final Atom atom : atoms) {
            if (p.accept(atom)) {
                foundAppStats = true;
            }
        }
        assertWithMessage("uid " + getUid() + " is not found in " + atoms.size() + " atoms")
                .that(foundAppStats).isTrue();
    }

    @FunctionalInterface
    private interface ThrowingPredicate<S, T extends Throwable> {
        boolean accept(S s) throws T;
    }

    public void testPackageInstallerV2MetricsReported() throws Throwable {
        if (!hasFeature(FEATURE_INCREMENTAL_DELIVERY, true)) return;
        final AtomsProto.PackageInstallerV2Reported report = installPackageUsingV2AndGetReport(
                new String[]{TEST_INSTALL_APK});
        assertTrue(report.getIsIncremental());
        // tests are ran using SHELL_UID and installation will be treated as adb install
        assertEquals("", report.getPackageName());
        assertEquals(1, report.getReturnCode());
        assertTrue(report.getDurationMillis() > 0);
        assertEquals(getTestFileSize(TEST_INSTALL_APK), report.getApksSizeBytes());

        getDevice().uninstallPackage(TEST_INSTALL_PACKAGE);
    }

    public void testPackageInstallerV2MetricsReportedForSplits() throws Throwable {
        if (!hasFeature(FEATURE_INCREMENTAL_DELIVERY, true)) return;

        final AtomsProto.PackageInstallerV2Reported report = installPackageUsingV2AndGetReport(
                new String[]{TEST_INSTALL_APK_BASE, TEST_INSTALL_APK_SPLIT});
        assertTrue(report.getIsIncremental());
        // tests are ran using SHELL_UID and installation will be treated as adb install
        assertEquals("", report.getPackageName());
        assertEquals(1, report.getReturnCode());
        assertTrue(report.getDurationMillis() > 0);
        assertEquals(
                getTestFileSize(TEST_INSTALL_APK_BASE) + getTestFileSize(TEST_INSTALL_APK_SPLIT),
                report.getApksSizeBytes());

        getDevice().uninstallPackage(TEST_INSTALL_PACKAGE);
    }

    public void testAppForegroundBackground() throws Exception {
        Set<Integer> onStates = new HashSet<>(Arrays.asList(
                AppUsageEventOccurred.EventType.MOVE_TO_FOREGROUND_VALUE));
        Set<Integer> offStates = new HashSet<>(Arrays.asList(
                AppUsageEventOccurred.EventType.MOVE_TO_BACKGROUND_VALUE));

        List<Set<Integer>> stateSet = Arrays.asList(onStates, offStates); // state sets, in order
        createAndUploadConfig(Atom.APP_USAGE_EVENT_OCCURRED_FIELD_NUMBER, false);
        Thread.sleep(WAIT_TIME_FOR_CONFIG_UPDATE_MS);

        getDevice().executeShellCommand(String.format(
                "am start -n '%s' -e %s %s",
                "com.android.server.cts.device.statsd/.StatsdCtsForegroundActivity",
                "action", ACTION_SHOW_APPLICATION_OVERLAY));
        final int waitTime = EXTRA_WAIT_TIME_MS + 5_000; // Overlay may need to sit there a while.
        Thread.sleep(waitTime + STATSD_REPORT_WAIT_TIME_MS);

        List<EventMetricData> data = getEventMetricDataList();
        Function<Atom, Integer> appUsageStateFunction =
                atom -> atom.getAppUsageEventOccurred().getEventType().getNumber();
        popUntilFind(data, onStates, appUsageStateFunction); // clear out initial appusage states.s
        assertStatesOccurred(stateSet, data, 0, appUsageStateFunction);
    }

    public void testAppForceStopUsageEvent() throws Exception {
        Set<Integer> onStates = new HashSet<>(Arrays.asList(
                AppUsageEventOccurred.EventType.MOVE_TO_FOREGROUND_VALUE));
        Set<Integer> offStates = new HashSet<>(Arrays.asList(
                AppUsageEventOccurred.EventType.MOVE_TO_BACKGROUND_VALUE));

        List<Set<Integer>> stateSet = Arrays.asList(onStates, offStates); // state sets, in order
        createAndUploadConfig(Atom.APP_USAGE_EVENT_OCCURRED_FIELD_NUMBER, false);
        Thread.sleep(WAIT_TIME_FOR_CONFIG_UPDATE_MS);

        getDevice().executeShellCommand(String.format(
                "am start -n '%s' -e %s %s",
                "com.android.server.cts.device.statsd/.StatsdCtsForegroundActivity",
                "action", ACTION_LONG_SLEEP_WHILE_TOP));
        final int waitTime = EXTRA_WAIT_TIME_MS + 5_000;
        Thread.sleep(waitTime);

        getDevice().executeShellCommand(String.format(
                "am force-stop %s",
                "com.android.server.cts.device.statsd/.StatsdCtsForegroundActivity"));
        Thread.sleep(waitTime + STATSD_REPORT_WAIT_TIME_MS);

        List<EventMetricData> data = getEventMetricDataList();
        Function<Atom, Integer> appUsageStateFunction =
                atom -> atom.getAppUsageEventOccurred().getEventType().getNumber();
        popUntilFind(data, onStates, appUsageStateFunction); // clear out initial appusage states.
        assertStatesOccurred(stateSet, data, 0, appUsageStateFunction);
    }

    private AtomsProto.PackageInstallerV2Reported installPackageUsingV2AndGetReport(
            String[] apkNames) throws Exception {
        createAndUploadConfig(Atom.PACKAGE_INSTALLER_V2_REPORTED_FIELD_NUMBER);
        Thread.sleep(WAIT_TIME_SHORT);
        installPackageUsingIncremental(apkNames, TEST_REMOTE_DIR);
        assertTrue(getDevice().isPackageInstalled(TEST_INSTALL_PACKAGE));
        Thread.sleep(WAIT_TIME_SHORT);

        List<AtomsProto.PackageInstallerV2Reported> reports = new ArrayList<>();
        for(EventMetricData data : getEventMetricDataList()) {
            if (data.getAtom().hasPackageInstallerV2Reported()) {
                reports.add(data.getAtom().getPackageInstallerV2Reported());
            }
        }
        assertEquals(1, reports.size());
        return reports.get(0);
    }

    private void installPackageUsingIncremental(String[] apkNames, String remoteDirPath)
            throws Exception {
        getDevice().executeShellCommand("mkdir " + remoteDirPath);
        String[] remoteApkPaths = new String[apkNames.length];
        for (int i = 0; i < remoteApkPaths.length; i++) {
            remoteApkPaths[i] = pushApkToRemote(apkNames[i], remoteDirPath);
        }
        getDevice().executeShellCommand(
                "pm install-incremental -t -g " + String.join(" ", remoteApkPaths));
    }

    private String pushApkToRemote(String apkName, String remoteDirPath)
            throws Exception {
        CompatibilityBuildHelper buildHelper = new CompatibilityBuildHelper(getBuild());
        final File apk = buildHelper.getTestFile(apkName);
        final String remoteApkPath = remoteDirPath + "/" + apk.getName();
        assertTrue(getDevice().pushFile(apk, remoteApkPath));
        assertNotNull(apk);
        return remoteApkPath;
    }

    private long getTestFileSize(String fileName) throws Exception {
        CompatibilityBuildHelper buildHelper = new CompatibilityBuildHelper(getBuild());
        final File file = buildHelper.getTestFile(fileName);
        return file.length();
    }
}
