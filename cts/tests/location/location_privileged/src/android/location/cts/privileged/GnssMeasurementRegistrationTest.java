/*
 * Copyright (C) 2020 The Android Open Source Project
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

package android.location.cts.privileged;

import android.Manifest;
import android.location.GnssMeasurementsEvent;
import android.location.GnssRequest;
import android.location.Location;
import android.location.cts.common.GnssTestCase;
import android.location.cts.common.SoftAssert;
import android.location.cts.common.TestGnssMeasurementListener;
import android.location.cts.common.TestLocationListener;
import android.location.cts.common.TestLocationManager;
import android.location.cts.common.TestMeasurementUtil;
import android.os.Build;
import android.util.Log;

import androidx.test.InstrumentationRegistry;

import java.util.List;

/**
 * Test for {@link GnssMeasurementsEvent.Callback} registration.
 *
 * Test steps:
 * 1. Register a listener for {@link GnssMeasurementsEvent}s.
 * 2. Check {@link GnssMeasurementsEvent} status: if the status is not
 *    {@link GnssMeasurementsEvent.Callback#STATUS_READY}, the test will fail and because one of
 *    the following reasons:
 *          2.1 the device does not support the feature,
 *          2.2 Location or GPS is disabled in the device.
 * 3. If at least one {@link GnssMeasurementsEvent} is received, the test will pass.
 * 4. If no {@link GnssMeasurementsEvent} are received, then check if the device can receive
 *    measurements only when {@link Location} is requested. This is done by performing the following
 *    steps:
 *          4.1 Register for location updates.
 *          4.2 Wait for {@link TestLocationListener#onLocationChanged(Location)}}.
 *          4.3 If at least one {@link GnssMeasurementsEvent} is received, the test will pass.
 *          4.4 If no {@link Location} is received this will mean that the device is located
 *              indoor. If we receive a {@link Location}, it mean that
 *              {@link GnssMeasurementsEvent}s are provided only if the application registers for
 *              location updates as well.
 */
public class GnssMeasurementRegistrationTest extends GnssTestCase {

    private static final String TAG = "GnssMeasRegTest";
    private static final int EVENTS_COUNT = 5;
    private static final int GPS_EVENTS_COUNT = 1;
    private TestLocationListener mLocationListener;
    private TestGnssMeasurementListener mMeasurementListener;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        InstrumentationRegistry.getInstrumentation().getUiAutomation()
                .adoptShellPermissionIdentity(Manifest.permission.LOCATION_HARDWARE);
        mTestLocationManager = new TestLocationManager(getContext());
    }

    @Override
    protected void tearDown() throws Exception {
        // Unregister listeners
        if (mLocationListener != null) {
            mTestLocationManager.removeLocationUpdates(mLocationListener);
        }
        if (mMeasurementListener != null) {
            mTestLocationManager.unregisterGnssMeasurementCallback(mMeasurementListener);
        }
        InstrumentationRegistry.getInstrumentation().getUiAutomation()
                .dropShellPermissionIdentity();
        super.tearDown();
    }

    /**
     * Test GPS measurements registration with full tracking enabled.
     */
    public void testGnssMeasurementRegistration_enableFullTracking() throws Exception {
        // Checks if GPS hardware feature is present, skips test (pass) if not,
        // and hard asserts that Location/GPS (Provider) is turned on if is Cts Verifier.
        if (!TestMeasurementUtil.canTestRunOnCurrentDevice(Build.VERSION_CODES.R,
                mTestLocationManager,
                TAG)) {
            return;
        }

        // Register for GPS measurements.
        mMeasurementListener = new TestGnssMeasurementListener(TAG, GPS_EVENTS_COUNT);
        mTestLocationManager.registerGnssMeasurementCallback(mMeasurementListener,
                new GnssRequest.Builder().setFullTracking(true).build());

        mMeasurementListener.await();
        if (!mMeasurementListener.verifyStatus()) {
            // If test is strict verifyStatus will assert conditions are good for further testing.
            // Else this returns false and, we arrive here, and then return from here (pass.)
            return;
        }

        List<GnssMeasurementsEvent> events = mMeasurementListener.getEvents();
        Log.i(TAG, "Number of GnssMeasurement events received = " + events.size());

        if (!events.isEmpty()) {
            // Test passes if we get at least 1 pseudorange.
            Log.i(TAG, "Received GPS measurements. Test Pass.");
            return;
        }

        SoftAssert.failAsWarning(
                TAG,
                "GPS measurements were not received without registering for location updates. "
                        + "Trying again with Location request.");

        // Register for location updates.
        mLocationListener = new TestLocationListener(EVENTS_COUNT);
        mTestLocationManager.requestLocationUpdates(mLocationListener);

        // Wait for location updates
        mLocationListener.await();
        Log.i(TAG, "Location received = " + mLocationListener.isLocationReceived());

        events = mMeasurementListener.getEvents();
        Log.i(TAG, "Number of GnssMeasurement events received = " + events.size());

        SoftAssert softAssert = new SoftAssert(TAG);
        softAssert.assertTrue(
                "Did not receive any GnssMeasurement events.  Retry outdoors?",
                !events.isEmpty());
        softAssert.assertAll();
    }
}
