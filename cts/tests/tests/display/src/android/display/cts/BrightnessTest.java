/*
 * Copyright (C) 2018 The Android Open Source Project
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

package android.display.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.junit.Assume.assumeTrue;

import android.Manifest;
import android.app.UiAutomation;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.hardware.display.BrightnessChangeEvent;
import android.hardware.display.BrightnessConfiguration;
import android.hardware.display.BrightnessCorrection;
import android.hardware.display.DisplayManager;
import android.os.ParcelFileDescriptor;
import android.os.PowerManager;
import android.provider.Settings;
import android.util.Pair;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.MediumTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.FileDescriptor;
import java.io.FileInputStream;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Scanner;

@MediumTest
@RunWith(AndroidJUnit4.class)
public class BrightnessTest {

    private Map<Long, BrightnessChangeEvent> mLastReadEvents = new HashMap<>();
    private DisplayManager mDisplayManager;
    private PowerManager.WakeLock mWakeLock;
    private Context mContext;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getContext();
        mDisplayManager = mContext.getSystemService(DisplayManager.class);
        PowerManager pm = mContext.getSystemService(PowerManager.class);

        mWakeLock = pm.newWakeLock(
                PowerManager.SCREEN_BRIGHT_WAKE_LOCK | PowerManager.ACQUIRE_CAUSES_WAKEUP,
                "BrightnessTest");
        mWakeLock.acquire();
    }

    @After
    public void tearDown() {
        if (mWakeLock != null) {
            mWakeLock.release();
        }

        revokePermission(Manifest.permission.CONFIGURE_DISPLAY_BRIGHTNESS);
        revokePermission(Manifest.permission.BRIGHTNESS_SLIDER_USAGE);
    }

    @Test
    public void testBrightnessSliderTracking() throws InterruptedException {
        // Don't run as there is no app that has permission to access slider usage.
        assumeTrue(
                numberOfSystemAppsWithPermission(Manifest.permission.BRIGHTNESS_SLIDER_USAGE) > 0);

        int previousBrightness = getSystemSetting(Settings.System.SCREEN_BRIGHTNESS);
        int previousBrightnessMode =
                getSystemSetting(Settings.System.SCREEN_BRIGHTNESS_MODE);
        try {
            setSystemSetting(Settings.System.SCREEN_BRIGHTNESS_MODE,
                    Settings.System.SCREEN_BRIGHTNESS_MODE_AUTOMATIC);
            int mode = getSystemSetting(Settings.System.SCREEN_BRIGHTNESS_MODE);
            assertEquals(Settings.System.SCREEN_BRIGHTNESS_MODE_AUTOMATIC, mode);

            grantPermission(Manifest.permission.BRIGHTNESS_SLIDER_USAGE);

            // Setup and remember some initial state.
            recordSliderEvents();
            waitForFirstSliderEvent();
            setSystemSetting(Settings.System.SCREEN_BRIGHTNESS, 20);
            getNewEvents(1);

            // Update brightness
            setSystemSetting(Settings.System.SCREEN_BRIGHTNESS, 60);

            // Check we got a slider event for the change.
            List<BrightnessChangeEvent> newEvents = getNewEvents(1);
            assertEquals(1, newEvents.size());
            BrightnessChangeEvent firstEvent = newEvents.get(0);
            assertValidLuxData(firstEvent);

            // Update brightness again
            setSystemSetting(Settings.System.SCREEN_BRIGHTNESS, 200);

            // Check we get a second slider event.
            newEvents = getNewEvents(1);
            assertEquals(1, newEvents.size());
            BrightnessChangeEvent secondEvent = newEvents.get(0);
            assertValidLuxData(secondEvent);
            assertEquals(secondEvent.lastBrightness, firstEvent.brightness, 1.0f);
            assertTrue(secondEvent.isUserSetBrightness);
            assertTrue("failed " + secondEvent.brightness + " not greater than " +
                    firstEvent.brightness, secondEvent.brightness > firstEvent.brightness);
        } finally {
            setSystemSetting(Settings.System.SCREEN_BRIGHTNESS, previousBrightness);
            setSystemSetting(Settings.System.SCREEN_BRIGHTNESS_MODE, previousBrightnessMode);
        }
    }

    @Test
    public void testNoTrackingForManualBrightness() throws InterruptedException {
        // Don't run as there is no app that has permission to access slider usage.
        assumeTrue(
                numberOfSystemAppsWithPermission(Manifest.permission.BRIGHTNESS_SLIDER_USAGE) > 0);

        int previousBrightness = getSystemSetting(Settings.System.SCREEN_BRIGHTNESS);
        int previousBrightnessMode =
                getSystemSetting(Settings.System.SCREEN_BRIGHTNESS_MODE);
        try {
            setSystemSetting(Settings.System.SCREEN_BRIGHTNESS_MODE,
                    Settings.System.SCREEN_BRIGHTNESS_MODE_MANUAL);
            int mode = getSystemSetting(Settings.System.SCREEN_BRIGHTNESS_MODE);
            assertEquals(Settings.System.SCREEN_BRIGHTNESS_MODE_MANUAL, mode);

            grantPermission(Manifest.permission.BRIGHTNESS_SLIDER_USAGE);

            // Setup and remember some initial state.
            recordSliderEvents();
            setSystemSetting(Settings.System.SCREEN_BRIGHTNESS, 20);
            assertTrue(getNewEvents().isEmpty());

            // Then change the brightness
            setSystemSetting(Settings.System.SCREEN_BRIGHTNESS, 80);
            Thread.sleep(200);
            // There shouldn't be any events.
            assertTrue(getNewEvents().isEmpty());
        } finally {
            setSystemSetting(Settings.System.SCREEN_BRIGHTNESS, previousBrightness);
            setSystemSetting(Settings.System.SCREEN_BRIGHTNESS_MODE, previousBrightnessMode);
        }
    }

    @Test
    public void testNoColorSampleData() throws InterruptedException {
          // Don't run as there is no app that has permission to access slider usage.
        assumeTrue(
                numberOfSystemAppsWithPermission(Manifest.permission.BRIGHTNESS_SLIDER_USAGE) > 0);

        // Don't run as there is no app that has permission to push curves.
        assumeTrue(numberOfSystemAppsWithPermission(
                Manifest.permission.CONFIGURE_DISPLAY_BRIGHTNESS) > 0);

        int previousBrightness = getSystemSetting(Settings.System.SCREEN_BRIGHTNESS);
        int previousBrightnessMode =
                getSystemSetting(Settings.System.SCREEN_BRIGHTNESS_MODE);
        try {
            setSystemSetting(Settings.System.SCREEN_BRIGHTNESS_MODE,
                    Settings.System.SCREEN_BRIGHTNESS_MODE_AUTOMATIC);
            int mode = getSystemSetting(Settings.System.SCREEN_BRIGHTNESS_MODE);
            assertEquals(Settings.System.SCREEN_BRIGHTNESS_MODE_AUTOMATIC, mode);

            grantPermission(Manifest.permission.BRIGHTNESS_SLIDER_USAGE);
            grantPermission(Manifest.permission.CONFIGURE_DISPLAY_BRIGHTNESS);

            // Set brightness config to not sample color.
            BrightnessConfiguration config =
                    new BrightnessConfiguration.Builder(
                            new float[]{0.0f, 1000.0f},new float[]{20.0f, 500.0f})
                            .setShouldCollectColorSamples(false).build();
            mDisplayManager.setBrightnessConfiguration(config);

            // Setup and generate one slider event.
            recordSliderEvents();
            waitForFirstSliderEvent();
            setSystemSetting(Settings.System.SCREEN_BRIGHTNESS, 20);
            List<BrightnessChangeEvent> newEvents = getNewEvents(1);

            // No color samples.
            assertEquals(0, newEvents.get(0).colorSampleDuration);
            assertNull(newEvents.get(0).colorValueBuckets);

            // No test for sampling color as support is optional.
        } finally {
            setSystemSetting(Settings.System.SCREEN_BRIGHTNESS, previousBrightness);
            setSystemSetting(Settings.System.SCREEN_BRIGHTNESS_MODE, previousBrightnessMode);
        }
    }

    @Test
    public void testSliderUsagePermission() {
        revokePermission(Manifest.permission.BRIGHTNESS_SLIDER_USAGE);

        try {
            mDisplayManager.getBrightnessEvents();
        } catch (SecurityException e) {
            // Expected
            return;
        }
        fail();
    }

    @Test
    public void testConfigureBrightnessPermission() {
        revokePermission(Manifest.permission.CONFIGURE_DISPLAY_BRIGHTNESS);

        BrightnessConfiguration config =
            new BrightnessConfiguration.Builder(
                    new float[]{0.0f, 1000.0f},new float[]{20.0f, 500.0f})
                .setDescription("some test").build();

        try {
            mDisplayManager.setBrightnessConfiguration(config);
        } catch (SecurityException e) {
            // Expected
            return;
        }
        fail();
    }

    @Test
    public void testSetGetSimpleCurve() {
        // Don't run as there is no app that has permission to push curves.
        assumeTrue(numberOfSystemAppsWithPermission(
                Manifest.permission.CONFIGURE_DISPLAY_BRIGHTNESS) > 0);

        grantPermission(Manifest.permission.CONFIGURE_DISPLAY_BRIGHTNESS);

        BrightnessConfiguration defaultConfig = mDisplayManager.getDefaultBrightnessConfiguration();

        BrightnessConfiguration config =
                new BrightnessConfiguration.Builder(
                        new float[]{0.0f, 1000.0f},new float[]{20.0f, 500.0f})
                        .addCorrectionByCategory(ApplicationInfo.CATEGORY_IMAGE,
                                BrightnessCorrection.createScaleAndTranslateLog(0.80f, 0.2f))
                        .addCorrectionByPackageName("some.package.name",
                                BrightnessCorrection.createScaleAndTranslateLog(0.70f, 0.1f))
                        .setShortTermModelTimeoutMillis(
                                defaultConfig.getShortTermModelTimeoutMillis() + 1000L)
                        .setShortTermModelLowerLuxMultiplier(
                                defaultConfig.getShortTermModelLowerLuxMultiplier() + 0.2f)
                        .setShortTermModelUpperLuxMultiplier(
                                defaultConfig.getShortTermModelUpperLuxMultiplier() + 0.3f)
                        .setDescription("some test").build();
        mDisplayManager.setBrightnessConfiguration(config);
        BrightnessConfiguration returnedConfig = mDisplayManager.getBrightnessConfiguration();
        assertEquals(config, returnedConfig);
        assertEquals(returnedConfig.getCorrectionByCategory(ApplicationInfo.CATEGORY_IMAGE),
                BrightnessCorrection.createScaleAndTranslateLog(0.80f, 0.2f));
        assertEquals(returnedConfig.getCorrectionByPackageName("some.package.name"),
                BrightnessCorrection.createScaleAndTranslateLog(0.70f, 0.1f));
        assertNull(returnedConfig.getCorrectionByCategory(ApplicationInfo.CATEGORY_GAME));
        assertNull(returnedConfig.getCorrectionByPackageName("someother.package.name"));
        assertEquals(defaultConfig.getShortTermModelTimeoutMillis() + 1000L,
                returnedConfig.getShortTermModelTimeoutMillis());
        assertEquals(defaultConfig.getShortTermModelLowerLuxMultiplier() + 0.2f,
                returnedConfig.getShortTermModelLowerLuxMultiplier(), 0.001f);
        assertEquals(defaultConfig.getShortTermModelUpperLuxMultiplier() + 0.3f,
                returnedConfig.getShortTermModelUpperLuxMultiplier(), 0.001f);

        // After clearing the curve we should get back the default curve.
        mDisplayManager.setBrightnessConfiguration(null);
        returnedConfig = mDisplayManager.getBrightnessConfiguration();
        assertEquals(mDisplayManager.getDefaultBrightnessConfiguration(), returnedConfig);
    }

    @Test
    public void testGetDefaultCurve()  {
        // Don't run as there is no app that has permission to push curves.
        assumeTrue(numberOfSystemAppsWithPermission(
                Manifest.permission.CONFIGURE_DISPLAY_BRIGHTNESS) > 0);

        grantPermission(Manifest.permission.CONFIGURE_DISPLAY_BRIGHTNESS);

        BrightnessConfiguration defaultConfig = mDisplayManager.getDefaultBrightnessConfiguration();
        // Must provide a default config if an app with CONFIGURE_DISPLAY_BRIGHTNESS exists.
        assertNotNull(defaultConfig);

        Pair<float[], float[]> curve = defaultConfig.getCurve();
        assertTrue(curve.first.length > 0);
        assertEquals(curve.first.length, curve.second.length);
        assertInRange(curve.first, 0, Float.MAX_VALUE);
        assertInRange(curve.second, 0, Float.MAX_VALUE);
        assertEquals(0.0, curve.first[0], 0.1);
        assertMonotonic(curve.first, true /*strictly increasing*/, "lux");
        assertMonotonic(curve.second, false /*strictly increasing*/, "nits");
        assertTrue(defaultConfig.getShortTermModelLowerLuxMultiplier() > 0.0f);
        assertTrue(defaultConfig.getShortTermModelLowerLuxMultiplier() < 10.0f);
        assertTrue(defaultConfig.getShortTermModelUpperLuxMultiplier() > 0.0f);
        assertTrue(defaultConfig.getShortTermModelUpperLuxMultiplier() < 10.0f);
        assertTrue(defaultConfig.getShortTermModelTimeoutMillis() > 0L);
        assertTrue(defaultConfig.getShortTermModelTimeoutMillis() < 24 * 60 * 60 * 1000L);
        assertFalse(defaultConfig.shouldCollectColorSamples());
    }


    @Test
    public void testSliderEventsReflectCurves() throws InterruptedException {
        // Don't run as there is no app that has permission to access slider usage.
        assumeTrue(
                numberOfSystemAppsWithPermission(Manifest.permission.BRIGHTNESS_SLIDER_USAGE) > 0);
        // Don't run as there is no app that has permission to push curves.
        assumeTrue(numberOfSystemAppsWithPermission(
                Manifest.permission.CONFIGURE_DISPLAY_BRIGHTNESS) > 0);

        BrightnessConfiguration config =
                new BrightnessConfiguration.Builder(
                        new float[]{0.0f, 10000.0f},new float[]{15.0f, 400.0f})
                        .setDescription("model:8").build();

        int previousBrightness = getSystemSetting(Settings.System.SCREEN_BRIGHTNESS);
        int previousBrightnessMode =
                getSystemSetting(Settings.System.SCREEN_BRIGHTNESS_MODE);
        try {
            setSystemSetting(Settings.System.SCREEN_BRIGHTNESS_MODE,
                    Settings.System.SCREEN_BRIGHTNESS_MODE_AUTOMATIC);
            int mode = getSystemSetting(Settings.System.SCREEN_BRIGHTNESS_MODE);
            assertEquals(Settings.System.SCREEN_BRIGHTNESS_MODE_AUTOMATIC, mode);

            grantPermission(Manifest.permission.BRIGHTNESS_SLIDER_USAGE);
            grantPermission(Manifest.permission.CONFIGURE_DISPLAY_BRIGHTNESS);

            // Setup and remember some initial state.
            recordSliderEvents();
            waitForFirstSliderEvent();
            setSystemSetting(Settings.System.SCREEN_BRIGHTNESS, 20);
            getNewEvents(1);

            // Update brightness while we have a custom curve.
            mDisplayManager.setBrightnessConfiguration(config);
            setSystemSetting(Settings.System.SCREEN_BRIGHTNESS, 60);

            // Check we got a slider event for the change.
            List<BrightnessChangeEvent> newEvents = getNewEvents(1);
            assertEquals(1, newEvents.size());
            BrightnessChangeEvent firstEvent = newEvents.get(0);
            assertValidLuxData(firstEvent);
            assertFalse(firstEvent.isDefaultBrightnessConfig);

            // Update brightness again now with default curve.
            mDisplayManager.setBrightnessConfiguration(null);
            setSystemSetting(Settings.System.SCREEN_BRIGHTNESS, 200);

            // Check we get a second slider event.
            newEvents = getNewEvents(1);
            assertEquals(1, newEvents.size());
            BrightnessChangeEvent secondEvent = newEvents.get(0);
            assertValidLuxData(secondEvent);
            assertTrue(secondEvent.isDefaultBrightnessConfig);
        } finally {
            setSystemSetting(Settings.System.SCREEN_BRIGHTNESS, previousBrightness);
            setSystemSetting(Settings.System.SCREEN_BRIGHTNESS_MODE, previousBrightnessMode);
        }
    }

    @Test
    public void testAtMostOneAppHoldsBrightnessConfigurationPermission() {
        assertTrue(numberOfSystemAppsWithPermission(
                    Manifest.permission.CONFIGURE_DISPLAY_BRIGHTNESS) < 2);
    }

    private void assertValidLuxData(BrightnessChangeEvent event) {
        assertNotNull(event.luxTimestamps);
        assertNotNull(event.luxValues);
        assertTrue(event.luxTimestamps.length > 0);
        assertEquals(event.luxValues.length, event.luxTimestamps.length);
        for (int i = 1; i < event.luxTimestamps.length; ++i) {
            assertTrue(event.luxTimestamps[i - 1] <= event.luxTimestamps[i]);
        }
        for (int i = 0; i < event.luxValues.length; ++i) {
            assertTrue(event.luxValues[i] >= 0.0f);
            assertTrue(event.luxValues[i] <= Float.MAX_VALUE);
            assertFalse(Float.isNaN(event.luxValues[i]));
        }
    }

    /**
     * Returns the number of system apps with the given permission.
     */
    private int numberOfSystemAppsWithPermission(String permission) {
        List<PackageInfo> packages = mContext.getPackageManager().getPackagesHoldingPermissions(
                new String[] {permission}, PackageManager.MATCH_SYSTEM_ONLY);
        return packages.size();
    }

    private List<BrightnessChangeEvent> getNewEvents(int expected)
            throws InterruptedException {
        List<BrightnessChangeEvent> newEvents = new ArrayList<>();
        for (int i = 0; newEvents.size() < expected && i < 20; ++i) {
            if (i != 0) {
                Thread.sleep(100);
            }
            newEvents.addAll(getNewEvents());
        }
        return newEvents;
    }

    private List<BrightnessChangeEvent> getNewEvents() {
        List<BrightnessChangeEvent> newEvents = new ArrayList<>();
        List<BrightnessChangeEvent> events = mDisplayManager.getBrightnessEvents();
        for (BrightnessChangeEvent event : events) {
            if (!mLastReadEvents.containsKey(event.timeStamp)) {
                newEvents.add(event);
            }
        }
        mLastReadEvents = new HashMap<>();
        for (BrightnessChangeEvent event : events) {
            mLastReadEvents.put(event.timeStamp, event);
        }
        return newEvents;
    }

    private void recordSliderEvents() {
        mLastReadEvents = new HashMap<>();
        List<BrightnessChangeEvent> eventsBefore = mDisplayManager.getBrightnessEvents();
        for (BrightnessChangeEvent event : eventsBefore) {
            mLastReadEvents.put(event.timeStamp, event);
        }
    }

    private void waitForFirstSliderEvent() throws  InterruptedException {
        // Keep changing brightness until we get an event to handle devices with sensors
        // that take a while to warm up.
        int brightness = 25;
        for (int i = 0; i < 20; ++i) {
            setSystemSetting(Settings.System.SCREEN_BRIGHTNESS, brightness);
            brightness = brightness == 25 ? 80 : 25;
            Thread.sleep(100);
            if (!getNewEvents().isEmpty()) {
                return;
            }
        }
        fail("Failed to fetch first slider event. Is the ambient brightness sensor working?");
    }

    private int getSystemSetting(String setting) {
        return Integer.parseInt(runShellCommand("settings get system " + setting));
    }

    private void setSystemSetting(String setting, int value) {
        runShellCommand("settings put system " + setting + " " + Integer.toString(value));
    }

    private void grantPermission(String permission) {
        InstrumentationRegistry.getInstrumentation().getUiAutomation()
                .grantRuntimePermission(mContext.getPackageName(), permission);
    }

    private void revokePermission(String permission) {
        InstrumentationRegistry.getInstrumentation().getUiAutomation()
                .revokeRuntimePermission(mContext.getPackageName(), permission);
    }

    private String runShellCommand(String cmd) {
        UiAutomation automation = InstrumentationRegistry.getInstrumentation().getUiAutomation();
        ParcelFileDescriptor output = automation.executeShellCommand(cmd);
        String result = convertFileDescriptorToString(output.getFileDescriptor());
        return result.trim();
    }

    private String convertFileDescriptorToString(FileDescriptor desc) {
        try (Scanner s = new Scanner(new FileInputStream(desc)).useDelimiter("\\Z")) {
            return s.hasNext() ? s.next() : "";
        }
    }

    private static void assertInRange(float[] values, float min, float max) {
        for (int i = 0; i < values.length; i++) {
            assertFalse(Float.isNaN(values[i]));
            assertTrue(values[i] >= min);
            assertTrue(values[i] <= max);
        }
    }

    private static void assertMonotonic(float[] values, boolean strictlyIncreasing, String name) {
        if (values.length <= 1) {
            return;
        }
        float prev = values[0];
        for (int i = 1; i < values.length; i++) {
            if (prev > values[i] || (prev == values[i] && strictlyIncreasing)) {
                String condition = strictlyIncreasing ? "strictly increasing" : "monotonic";
                fail(name + " values must be " + condition);
            }
            prev = values[i];
        }
    }
}
