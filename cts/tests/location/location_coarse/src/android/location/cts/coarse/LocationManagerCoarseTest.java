/*
 * Copyright (C) 2019 The Android Open Source Project
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

package android.location.cts.coarse;

import static android.location.LocationManager.GPS_PROVIDER;
import static android.location.LocationManager.NETWORK_PROVIDER;
import static android.location.LocationManager.PASSIVE_PROVIDER;

import static androidx.test.ext.truth.location.LocationSubject.assertThat;

import static com.android.compatibility.common.util.LocationUtils.createLocation;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import android.content.Context;
import android.content.pm.PackageManager;
import android.location.Criteria;
import android.location.Location;
import android.location.LocationManager;
import android.location.cts.common.LocationListenerCapture;
import android.location.cts.common.LocationPendingIntentCapture;
import android.os.Bundle;
import android.os.SystemClock;
import android.platform.test.annotations.AppModeFull;
import android.util.Log;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;

import com.android.compatibility.common.util.LocationUtils;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.List;
import java.util.Random;
import java.util.concurrent.Executor;

@RunWith(AndroidJUnit4.class)
public class LocationManagerCoarseTest {

    private static final String TAG = "LocationManagerCoarseTest";

    private static final long TIMEOUT_MS = 5000;

    // 2000m is the default grid size used by location fudger
    private static final float MAX_COARSE_FUDGE_DISTANCE_M = 2500f;

    private static final String TEST_PROVIDER = "test_provider";

    private Random mRandom;
    private Context mContext;
    private LocationManager mManager;

    @Before
    public void setUp() throws Exception {
        LocationUtils.registerMockLocationProvider(InstrumentationRegistry.getInstrumentation(),
                true);

        long seed = System.currentTimeMillis();
        Log.i(TAG, "location random seed: " + seed);

        mRandom = new Random(seed);
        mContext = ApplicationProvider.getApplicationContext();
        mManager = mContext.getSystemService(LocationManager.class);

        assertNotNull(mManager);

        for (String provider : mManager.getAllProviders()) {
            mManager.removeTestProvider(provider);
        }

        mManager.addTestProvider(TEST_PROVIDER,
                true,
                false,
                true,
                false,
                false,
                false,
                false,
                Criteria.POWER_MEDIUM,
                Criteria.ACCURACY_COARSE);
        mManager.setTestProviderEnabled(TEST_PROVIDER, true);
    }

    @After
    public void tearDown() throws Exception {
        for (String provider : mManager.getAllProviders()) {
            mManager.removeTestProvider(provider);
        }

        LocationUtils.registerMockLocationProvider(InstrumentationRegistry.getInstrumentation(),
                false);
    }

    @Test
    public void testGetLastKnownLocation() {
        Location loc = createLocation(TEST_PROVIDER, mRandom);

        mManager.setTestProviderLocation(TEST_PROVIDER, loc);
        assertThat(mManager.getLastKnownLocation(TEST_PROVIDER)).isNearby(loc, MAX_COARSE_FUDGE_DISTANCE_M);
    }

    @Test
    public void testGetLastKnownLocation_FastInterval() {
        Location loc1 = createLocation(TEST_PROVIDER, mRandom);
        Location loc2 = createLocation(TEST_PROVIDER, mRandom);

        mManager.setTestProviderLocation(TEST_PROVIDER, loc1);
        Location coarseLocation = mManager.getLastKnownLocation(TEST_PROVIDER);
        mManager.setTestProviderLocation(TEST_PROVIDER, loc2);
        assertThat(mManager.getLastKnownLocation(TEST_PROVIDER)).isEqualTo(coarseLocation);
    }

    @Test
    public void testRequestLocationUpdates() throws Exception {
        Location loc = createLocation(TEST_PROVIDER, mRandom);
        Bundle extras = new Bundle();
        extras.putParcelable(Location.EXTRA_NO_GPS_LOCATION, new Location(loc));
        loc.setExtras(extras);

        try (LocationListenerCapture capture = new LocationListenerCapture(mContext)) {
            mManager.requestLocationUpdates(TEST_PROVIDER, 0, 0, directExecutor(), capture);
            mManager.setTestProviderLocation(TEST_PROVIDER, loc);
            assertThat(capture.getNextLocation(TIMEOUT_MS)).isNearby(loc, MAX_COARSE_FUDGE_DISTANCE_M);
        }
    }

    @Test
    public void testRequestLocationUpdates_PendingIntent() throws Exception {
        Location loc = createLocation(TEST_PROVIDER, mRandom);
        Bundle extras = new Bundle();
        extras.putParcelable(Location.EXTRA_NO_GPS_LOCATION, new Location(loc));
        loc.setExtras(extras);

        try (LocationPendingIntentCapture capture = new LocationPendingIntentCapture(mContext)) {
            mManager.requestLocationUpdates(TEST_PROVIDER, 0, 0, capture.getPendingIntent());
            mManager.setTestProviderLocation(TEST_PROVIDER, loc);
            assertThat(capture.getNextLocation(TIMEOUT_MS)).isNearby(loc, MAX_COARSE_FUDGE_DISTANCE_M);
        }
    }

    @Test
    public void testGetProviders() {
        List<String> providers = mManager.getProviders(false);
        assertTrue(providers.contains(TEST_PROVIDER));
        if (hasGpsFeature()) {
            assertTrue(providers.contains(GPS_PROVIDER));
        } else {
            assertFalse(providers.contains(GPS_PROVIDER));
        }
        assertTrue(providers.contains(PASSIVE_PROVIDER));
    }

    @Test
    public void testGetBestProvider() {
        // prevent network provider from matching
        mManager.addTestProvider(NETWORK_PROVIDER,
                true,
                false,
                true,
                false,
                false,
                false,
                false,
                Criteria.POWER_HIGH,
                Criteria.ACCURACY_COARSE);

        Criteria criteria = new Criteria();
        criteria.setAccuracy(Criteria.ACCURACY_COARSE);
        criteria.setPowerRequirement(Criteria.POWER_MEDIUM);

        String bestProvider = mManager.getBestProvider(criteria, false);
        assertEquals(TEST_PROVIDER, bestProvider);

        criteria.setAccuracy(Criteria.ACCURACY_FINE);
        criteria.setPowerRequirement(Criteria.POWER_LOW);
        assertNotEquals(TEST_PROVIDER, mManager.getBestProvider(criteria, false));
    }

    @Test
    @AppModeFull(reason = "Instant apps can't hold ACCESS_LOCATION_EXTRA_COMMANDS")
    public void testSendExtraCommand() {
        mManager.sendExtraCommand(TEST_PROVIDER, "command", null);
    }

    // TODO: this test should probably not be in the location module
    @Test
    public void testGnssProvidedClock() throws Exception {
        mManager.addTestProvider(GPS_PROVIDER,
                false,
                true,
                false,
                false,
                true,
                true,
                true,
                Criteria.POWER_MEDIUM,
                Criteria.ACCURACY_COARSE);
        mManager.setTestProviderEnabled(GPS_PROVIDER, true);

        Location location = new Location(GPS_PROVIDER);
        long elapsed = SystemClock.elapsedRealtimeNanos();
        location.setLatitude(0);
        location.setLongitude(0);
        location.setAccuracy(0);
        location.setElapsedRealtimeNanos(elapsed);
        location.setTime(1);

        mManager.setTestProviderLocation(GPS_PROVIDER, location);
        assertTrue(SystemClock.currentGnssTimeClock().millis() < 1000);

        location.setTime(java.lang.System.currentTimeMillis());
        location.setElapsedRealtimeNanos(SystemClock.elapsedRealtimeNanos());
        mManager.setTestProviderLocation(GPS_PROVIDER, location);
        Thread.sleep(200);
        long clockms = SystemClock.currentGnssTimeClock().millis();
        assertTrue(System.currentTimeMillis() - clockms < 1000);
    }

    private static Executor directExecutor() {
        return Runnable::run;
    }

    private boolean hasGpsFeature() {
        return mContext.getPackageManager().hasSystemFeature(PackageManager.FEATURE_LOCATION_GPS);
    }
}
