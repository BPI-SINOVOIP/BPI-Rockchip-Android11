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
package android.os.cts.batterysaving;

import static android.provider.Settings.Global.BATTERY_SAVER_CONSTANTS;
import static android.provider.Settings.Secure.LOCATION_MODE_OFF;

import static androidx.test.InstrumentationRegistry.getInstrumentation;

import static com.android.compatibility.common.util.BatteryUtils.enableBatterySaver;
import static com.android.compatibility.common.util.BatteryUtils.runDumpsysBatteryUnplug;
import static com.android.compatibility.common.util.BatteryUtils.turnOnScreen;
import static com.android.compatibility.common.util.TestUtils.waitUntil;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.app.UiModeManager;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.location.Criteria;
import android.location.Location;
import android.location.LocationListener;
import android.location.LocationManager;
import android.location.LocationProvider;
import android.location.LocationRequest;
import android.os.Bundle;
import android.os.Looper;
import android.os.PowerManager;
import android.os.Process;
import android.provider.Settings;
import android.provider.Settings.Global;
import android.provider.Settings.Secure;

import androidx.test.filters.MediumTest;
import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.CallbackAsserter;
import com.android.compatibility.common.util.LocationUtils;
import com.android.compatibility.common.util.RequiredFeatureRule;
import com.android.compatibility.common.util.SettingsUtils;
import com.android.compatibility.common.util.TestUtils.RunnableWithThrow;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.List;

/**
 * Tests related to battery saver:
 * atest android.os.cts.batterysaving.BatterySaverLocationTest
 */
@MediumTest
@RunWith(AndroidJUnit4.class)
public class BatterySaverLocationTest extends BatterySavingTestBase {
    private static final String TAG = "BatterySaverLocationTest";

    private static final String TEST_PROVIDER_NAME = "test_provider";

    @Rule
    public final RequiredFeatureRule mRequireLocationRule =
            new RequiredFeatureRule(PackageManager.FEATURE_LOCATION);

    @Rule
    public final RequiredFeatureRule mRequireLocationGpsRule =
            new RequiredFeatureRule(PackageManager.FEATURE_LOCATION_GPS);

    private LocationManager mLocationManager;

    private static class TestLocationListener implements LocationListener {
        @Override
        public void onLocationChanged(Location location) {

        }

        @Override
        public void onStatusChanged(String provider, int status, Bundle extras) {

        }

        @Override
        public void onProviderEnabled(String provider) {

        }

        @Override
        public void onProviderDisabled(String provider) {
            fail("Provider disabled");
        }
    }

    @Before
    public void setUp() throws Exception {
        LocationUtils.registerMockLocationProvider(getInstrumentation(), true);
        mLocationManager = getLocationManager();

        // remove test provider if left over from an aborted run
        LocationProvider lp = mLocationManager.getProvider(TEST_PROVIDER_NAME);
        if (lp != null) {
            mLocationManager.removeTestProvider(TEST_PROVIDER_NAME);
        }

        SettingsUtils.set(SettingsUtils.NAMESPACE_GLOBAL,
                Settings.Global.LOCATION_IGNORE_SETTINGS_PACKAGE_WHITELIST,
                "android.os.cts.batterysaving");

        getContext().getSystemService(UiModeManager.class).disableCarMode(0);
    }

    @After
    public void tearDown() throws Exception {
        LocationUtils.registerMockLocationProvider(getInstrumentation(), false);
        SettingsUtils.set(SettingsUtils.NAMESPACE_GLOBAL,
                Settings.Global.LOCATION_IGNORE_SETTINGS_PACKAGE_WHITELIST,
                "");
    }

    private boolean areOnlyIgnoreSettingsRequestsSentToProvider() {
        List<LocationRequest> requests =
                mLocationManager.getTestProviderCurrentRequests(TEST_PROVIDER_NAME);
        for (LocationRequest request : requests) {
            if (!request.isLocationSettingsIgnored()) {
                return false;
            }
        }
        return true;
    }

    /**
     * Test for the {@link PowerManager#LOCATION_MODE_ALL_DISABLED_WHEN_SCREEN_OFF} mode.
     */
    @Test
    public void testLocationAllDisabled() throws Exception {
        assertTrue("Screen is off", getPowerManager().isInteractive());

        assertFalse(getPowerManager().isPowerSaveMode());
        assertEquals(PowerManager.LOCATION_MODE_NO_CHANGE,
                getPowerManager().getLocationPowerSaveMode());

        assertEquals(0, getLocationGlobalKillSwitch());

        SettingsUtils.set(SettingsUtils.NAMESPACE_GLOBAL, BATTERY_SAVER_CONSTANTS, "gps_mode=2");
        assertNotEquals(LOCATION_MODE_OFF, getLocationMode());
        assertTrue(getLocationManager().isLocationEnabled());

        // Unplug the charger and activate battery saver.
        runDumpsysBatteryUnplug();
        enableBatterySaver(true);

        // Skip if the location mode is not what's expected.
        final int mode = getPowerManager().getLocationPowerSaveMode();
        if (mode != PowerManager.LOCATION_MODE_ALL_DISABLED_WHEN_SCREEN_OFF) {
            fail("Unexpected location power save mode (" + mode + ").");
        }

        // Make sure screen is on.
        assertTrue(getPowerManager().isInteractive());

        // Make sure the kill switch is still off.
        assertEquals(0, getLocationGlobalKillSwitch());

        // Make sure location is still enabled.
        assertNotEquals(LOCATION_MODE_OFF, getLocationMode());
        assertTrue(getLocationManager().isLocationEnabled());

        // Turn screen off.
        runWithExpectingLocationCallback(() -> {
            turnOnScreen(false);
            waitUntil("Kill switch still off", () -> getLocationGlobalKillSwitch() == 1);
            assertEquals(LOCATION_MODE_OFF, getLocationMode());
            assertFalse(getLocationManager().isLocationEnabled());
        });

        // On again.
        runWithExpectingLocationCallback(() -> {
            turnOnScreen(true);
            waitUntil("Kill switch still off", () -> getLocationGlobalKillSwitch() == 0);
            assertNotEquals(LOCATION_MODE_OFF, getLocationMode());
            assertTrue(getLocationManager().isLocationEnabled());
        });

        // Off again.
        runWithExpectingLocationCallback(() -> {
            turnOnScreen(false);
            waitUntil("Kill switch still off", () -> getLocationGlobalKillSwitch() == 1);
            assertEquals(LOCATION_MODE_OFF, getLocationMode());
            assertFalse(getLocationManager().isLocationEnabled());
        });

        // Disable battery saver and make sure the kill swtich is off.
        runWithExpectingLocationCallback(() -> {
            enableBatterySaver(false);
            waitUntil("Kill switch still on", () -> getLocationGlobalKillSwitch() == 0);
            assertNotEquals(LOCATION_MODE_OFF, getLocationMode());
            assertTrue(getLocationManager().isLocationEnabled());
        });
    }

    private int getLocationGlobalKillSwitch() {
        return Global.getInt(getContext().getContentResolver(),
                Global.LOCATION_GLOBAL_KILL_SWITCH, 0);
    }

    private int getLocationMode() {
        return Secure.getInt(getContext().getContentResolver(), Secure.LOCATION_MODE, 0);
    }

    private void runWithExpectingLocationCallback(RunnableWithThrow r) throws Exception {
        CallbackAsserter locationModeBroadcastAsserter = CallbackAsserter.forBroadcast(
                new IntentFilter(LocationManager.MODE_CHANGED_ACTION));
        CallbackAsserter locationModeObserverAsserter = CallbackAsserter.forContentUri(
                Settings.Secure.getUriFor(Settings.Secure.LOCATION_MODE));

        r.run();

        locationModeBroadcastAsserter.assertCalled("Broadcast not received",
                DEFAULT_TIMEOUT_SECONDS);
        locationModeObserverAsserter.assertCalled("Observer not notified",
                DEFAULT_TIMEOUT_SECONDS);
    }

    /**
     * Test for the {@link PowerManager#LOCATION_MODE_THROTTLE_REQUESTS_WHEN_SCREEN_OFF} mode.
     */
    @Test
    public void testLocationRequestThrottling() throws Exception {
        mLocationManager.addTestProvider(TEST_PROVIDER_NAME,
                true, //requiresNetwork,
                false, // requiresSatellite,
                false,  // requiresCell,
                false, // hasMonetaryCost,
                true, // supportsAltitude,
                false, // supportsSpeed,
                true, // supportsBearing,
                Criteria.POWER_MEDIUM, // powerRequirement
                Criteria.ACCURACY_FINE); // accuracy
        mLocationManager.setTestProviderEnabled(TEST_PROVIDER_NAME, true);

        LocationRequest normalLocationRequest = LocationRequest.create()
                .setExpireIn(300_000)
                .setFastestInterval(0)
                .setInterval(0)
                .setLocationSettingsIgnored(false)
                .setProvider(TEST_PROVIDER_NAME)
                .setQuality(LocationRequest.ACCURACY_FINE);
        LocationRequest ignoreSettingsLocationRequest = LocationRequest.create()
                .setExpireIn(300_000)
                .setFastestInterval(0)
                .setInterval(0)
                .setLocationSettingsIgnored(true)
                .setProvider(TEST_PROVIDER_NAME)
                .setQuality(LocationRequest.ACCURACY_FINE);
        mLocationManager.requestLocationUpdates(
                normalLocationRequest, new TestLocationListener(), Looper.getMainLooper());
        mLocationManager.requestLocationUpdates(
                ignoreSettingsLocationRequest, new TestLocationListener(), Looper.getMainLooper());
        assertTrue("Not enough requests sent to provider",
                mLocationManager.getTestProviderCurrentRequests(TEST_PROVIDER_NAME).size() >= 2);
        assertFalse("Normal priority requests not sent to provider",
                areOnlyIgnoreSettingsRequestsSentToProvider());

        assertTrue("Screen is off", getPowerManager().isInteractive());

        SettingsUtils.set(SettingsUtils.NAMESPACE_GLOBAL, BATTERY_SAVER_CONSTANTS, "gps_mode=4");
        assertFalse(getPowerManager().isPowerSaveMode());
        assertEquals(PowerManager.LOCATION_MODE_NO_CHANGE,
                getPowerManager().getLocationPowerSaveMode());

        // Make sure location is enabled.
        getLocationManager().setLocationEnabledForUser(true, Process.myUserHandle());
        assertTrue(getLocationManager().isLocationEnabled());

        // Unplug the charger and activate battery saver.
        runDumpsysBatteryUnplug();
        enableBatterySaver(true);

        // Skip if the location mode is not what's expected.
        final int mode = getPowerManager().getLocationPowerSaveMode();
        if (mode != PowerManager.LOCATION_MODE_THROTTLE_REQUESTS_WHEN_SCREEN_OFF) {
            fail("Unexpected location power save mode (" + mode + "), skipping.");
        }

        // Make sure screen is on.
        assertTrue(getPowerManager().isInteractive());

        // Make sure location is still enabled.
        assertTrue(getLocationManager().isLocationEnabled());

        // Turn screen off.
        turnOnScreen(false);
        waitUntil("Normal location request still sent to provider",
                this::areOnlyIgnoreSettingsRequestsSentToProvider);
        assertTrue("Not enough requests sent to provider",
                mLocationManager.getTestProviderCurrentRequests(TEST_PROVIDER_NAME).size() >= 1);

        // On again.
        turnOnScreen(true);
        waitUntil("Normal location request not sent to provider",
                () -> !areOnlyIgnoreSettingsRequestsSentToProvider());
        assertTrue("Not enough requests sent to provider",
                mLocationManager.getTestProviderCurrentRequests(TEST_PROVIDER_NAME).size() >= 2);

        // Off again.
        turnOnScreen(false);
        waitUntil("Normal location request still sent to provider",
                this::areOnlyIgnoreSettingsRequestsSentToProvider);
        assertTrue("Not enough requests sent to provider",
                mLocationManager.getTestProviderCurrentRequests(TEST_PROVIDER_NAME).size() >= 1);


        // Disable battery saver and make sure the kill switch is off.
        enableBatterySaver(false);
        waitUntil("Normal location request not sent to provider",
                () -> !areOnlyIgnoreSettingsRequestsSentToProvider());
        assertTrue("Not enough requests sent to provider",
                mLocationManager.getTestProviderCurrentRequests(TEST_PROVIDER_NAME).size() >= 2);
    }
}
