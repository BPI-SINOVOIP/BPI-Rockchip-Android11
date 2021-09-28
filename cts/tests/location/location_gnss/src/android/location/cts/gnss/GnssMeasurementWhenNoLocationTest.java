/*
 * Copyright (C) 2015 Google Inc.
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

package android.location.cts.gnss;

import android.location.GnssMeasurement;
import android.location.GnssMeasurementsEvent;
import android.location.GnssStatus;
import android.location.cts.common.GnssTestCase;
import android.location.cts.common.TestGnssMeasurementListener;
import android.location.cts.common.TestLocationListener;
import android.os.Build;
import android.platform.test.annotations.AppModeFull;
import android.util.Log;
import android.location.cts.common.TestUtils;
import android.location.cts.common.TestMeasurementUtil;
import android.location.cts.common.SoftAssert;
import android.location.cts.common.TestLocationManager;

import java.util.Arrays;
import java.util.Collection;
import java.util.List;

/**
 * Test for {@link GnssMeasurement} without a location fix.
 *
 * Test steps:
 * 1. Clear A-GPS: this ensures that the device is not in a warm mode and it has 4+ satellites
 *    acquired already.
 * 2. Register a listener for:
 *      - {@link GnssMeasurementsEvent}s,
 *      - location updates and
 *      - {@link GnssStatus} events.
 * 3. Wait for {@link GnssMeasurementsEvent}s to provide {@link EVENTS_COUNT} measurements
 * 4. Ensure that zero locations have been received
 * 5. Check {@link GnssMeasurementsEvent} status: if the status is not
 *    {@link GnssMeasurementsEvent#STATUS_READY}, the test will be skipped because one of the
 *    following reasons:
 *          4.1 the device does not support the feature,
 *          4.2 GPS Locaiton is disabled in the device && the test is CTS non-verifier
 * 6. Check whether the device is deep indoor. This is done by performing the following steps:
 *          4.1 If no {@link GnssStatus} is received this will mean that the device is located
 *              indoor. The test will be skipped if not strict (CTS or pre-2016.)
 * 7. When the device is not indoor, verify that we receive {@link GnssMeasurementsEvent}s before
 *    a GPS location is calculated, and reported by GPS HAL. If {@link GnssMeasurementsEvent}s are
 *    only received after a location update is received:
 *          4.1.1 The test will pass with a warning for the M release.
 *          4.1.2 The test will fail on N with CTS-Verifier & newer (2016+) GPS hardware.
 * 8. If {@link GnssMeasurementsEvent}s are received: verify all mandatory fields, the test will
 *    fail if any of the mandatory fields is not populated or in the expected range.
 */
public class GnssMeasurementWhenNoLocationTest extends GnssTestCase {

    private static final String TAG = "GnssMeasBeforeLocTest";
    private TestGnssMeasurementListener mMeasurementListener;
    private TestLocationListener mLocationListener;
    private TestGnssStatusCallback mGnssStatusCallback;
    private static final int EVENTS_COUNT = 2;
    private static final int LOCATIONS_COUNT = 1;

    // Command to delete cached A-GPS data to get a truer GPS fix.
    private static final String AGPS_DELETE_COMMAND = "delete_aiding_data";

    @Override
    protected void setUp() throws Exception {
        super.setUp();

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
        if (mGnssStatusCallback != null) {
            mTestLocationManager.unregisterGnssStatusCallback(mGnssStatusCallback);
        }
        super.tearDown();
    }

    /**
     * Test for GPS measurements before a location fix.
     */
    @AppModeFull(reason = "Requires use of extra LocationManager commands")
    public void testGnssMeasurementWhenNoLocation() throws Exception {
        // Checks if GPS hardware feature is present, skips test (pass) if not
        if (!TestMeasurementUtil.canTestRunOnCurrentDevice(Build.VERSION_CODES.N,
                mTestLocationManager,
                TAG)) {
            return;
        }

        // Set the device in airplane mode so that the GPS assistance data cannot be downloaded.
        // This results in GNSS measurements being reported before a location is reported.
        // NOTE: Changing global setting airplane_mode_on is not allowed in CtsVerifier application.
        //       Hence, airplane mode is turned on only when this test is run as a regular CTS test
        //       and not when it is invoked through CtsVerifier.
        boolean isAirplaneModeOffBeforeTest = true;
        // Record the state of the airplane mode before the test so that we can restore it
        // after the test.
        isAirplaneModeOffBeforeTest = !TestUtils.isAirplaneModeOn();
        if (isAirplaneModeOffBeforeTest) {
            TestUtils.setAirplaneModeOn(getContext(), true);
        }

        try {
            // Clear A-GPS and skip the test if the operation fails.
            if (!mTestLocationManager.sendExtraCommand(AGPS_DELETE_COMMAND)) {
                Log.i(TAG, "A-GPS failed to clear. Skip test.");
                return;
            }

            // Register for GPS measurements.
            mMeasurementListener = new TestGnssMeasurementListener(TAG, EVENTS_COUNT);
            mTestLocationManager.registerGnssMeasurementCallback(mMeasurementListener);

            // Register for Gps Status updates.
            mGnssStatusCallback = new TestGnssStatusCallback(TAG, EVENTS_COUNT);
            mTestLocationManager.registerGnssStatusCallback(mGnssStatusCallback);

            // Register for location updates.
            mLocationListener = new TestLocationListener(LOCATIONS_COUNT);
            mTestLocationManager.requestLocationUpdates(mLocationListener);

            mMeasurementListener.awaitStatus();
            if (!mMeasurementListener.verifyStatus()) {
                return; // exit peacefully (if not already asserted out inside verifyStatus)
            }

            // Wait for two measurement events - this is better than waiting for a location
            // calculation because the test generally completes much faster.
            mMeasurementListener.await();

            Log.i(TAG, "mLocationListener.isLocationReceived(): "
                    + mLocationListener.isLocationReceived());

            SoftAssert softAssert = new SoftAssert(TAG);
            softAssert.assertTrue(
                    "No Satellites are visible. Device may be indoors.  Retry outdoors?",
                    mGnssStatusCallback.getGnssStatus() != null);

            List<GnssMeasurementsEvent> events = mMeasurementListener.getEvents();
            Log.i(TAG, "Number of GPS measurement events received = " + events.size());

            if (events.isEmpty()) {
                softAssert.assertTrue(
                        "No measurement events received",
                        false);
                return;  // All of the following checks rely on there being measurements
            }

            // Ensure that after getting a few (at least 2) measurement events, that we still
            // don't have location (i.e. that we got measurements before location.)  Fail, if
            // strict, warn, if not.
            softAssert.assertTrue(
                    "Location was received before " + events.size() +
                            " GnssMeasurementEvents with measurements were reported. " +
                            "Test expects at least " + EVENTS_COUNT +
                            " GnssMeasurementEvents before a location, given the cold start" +
                            " start. Ensure no other active GPS apps (so the cold start" +
                            " command works) and retry?",
                    !mLocationListener.isLocationReceived());

            // If device has received measurements also verify
            // that mandatory fields of GnssMeasurement are in expected ranges.
            GnssMeasurementsEvent firstEvent = events.get(0);
            Collection<GnssMeasurement> gpsMeasurements = firstEvent.getMeasurements();
            int satelliteCount = gpsMeasurements.size();
            int[] gpsPrns = new int[satelliteCount];
            int i = 0;
            for (GnssMeasurement measurement : gpsMeasurements) {
                gpsPrns[i] = measurement.getSvid();
                ++i;
            }
            Log.i(TAG, "First GnssMeasurementsEvent with PRNs=" + Arrays.toString(gpsPrns));

            long timeInNs = firstEvent.getClock().getTimeNanos();
            softAssert.assertTrue("GPS measurement satellite count check: ",
                    timeInNs, // event time in ns
                    "satelliteCount > 0", // expected value
                    Integer.toString(satelliteCount), // actual value
                    satelliteCount > 0); // condition

            TestMeasurementUtil.assertGnssClockFields(firstEvent.getClock(), softAssert, timeInNs);

            // Verify mandatory fields of GnssMeasurement
            for (GnssMeasurement measurement : gpsMeasurements) {
                TestMeasurementUtil.assertAllGnssMeasurementMandatoryFields(mTestLocationManager,
                        measurement, softAssert, timeInNs);
            }
            softAssert.assertAll();
        } finally {
            // Set the airplane mode back to off if it was off before this test.
            if (isAirplaneModeOffBeforeTest) {
                TestUtils.setAirplaneModeOn(getContext(), false);
            }
        }
    }
}
