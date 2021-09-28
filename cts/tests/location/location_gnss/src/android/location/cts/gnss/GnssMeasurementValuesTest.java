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
import android.location.cts.common.GnssTestCase;
import android.location.cts.common.SoftAssert;
import android.location.cts.common.TestGnssMeasurementListener;
import android.location.cts.common.TestLocationListener;
import android.location.cts.common.TestLocationManager;
import android.location.cts.common.TestMeasurementUtil;
import android.os.Build;
import android.util.Log;

import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Test the {@link GnssMeasurement} values.
 *
 * Test steps:
 * 1. Register for location updates.
 * 2. Register a listener for {@link GnssMeasurementsEvent}s.
 * 3. Wait for {@link #LOCATION_TO_COLLECT_COUNT} locations.
 *          3.1 Confirm locations have been found.
 * 4. Check {@link GnssMeasurementsEvent} status: if the status is not
 *    {@link GnssMeasurementsEvent.Callback#STATUS_READY}, the test will be skipped because
 *    one of the following reasons:
 *          4.1 the device does not support the GPS feature,
 *          4.2 GPS Location is disabled in the device and this is CTS (non-verifier)
 *  5. Verify {@link GnssMeasurement}s (all mandatory fields), the test will fail if any of the
 *     mandatory fields is not populated or in the expected range.
 */
public class GnssMeasurementValuesTest extends GnssTestCase {

    private static final String TAG = "GnssMeasValuesTest";
    private static final int LOCATION_TO_COLLECT_COUNT = 20;

    private TestGnssMeasurementListener mMeasurementListener;
    private TestLocationListener mLocationListener;
    // Store list of Satellites including Gnss Band, constellation & SvId
    private Set<String> mGnssMeasSvStringIds;

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        mTestLocationManager = new TestLocationManager(getContext());
        mGnssMeasSvStringIds = new HashSet<>();
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
        super.tearDown();
    }

    /**
     * Tests that one can listen for {@link GnssMeasurementsEvent} for collection purposes.
     * It only performs sanity checks for the measurements received.
     * This tests uses actual data retrieved from GPS HAL.
     */
    public void testListenForGnssMeasurements() throws Exception {
        // Checks if GPS hardware feature is present, skips test (pass) if not
        if (!TestMeasurementUtil.canTestRunOnCurrentDevice(Build.VERSION_CODES.N,
                mTestLocationManager,
                TAG)) {
            return;
        }

        mLocationListener = new TestLocationListener(LOCATION_TO_COLLECT_COUNT);
        mTestLocationManager.requestLocationUpdates(mLocationListener);

        mMeasurementListener = new TestGnssMeasurementListener(TAG);
        mTestLocationManager.registerGnssMeasurementCallback(mMeasurementListener);

        // Register a status callback to enable checking Svs across status & measurements.
        // Count in status callback is not important as this test is pass/fail based on
        // measurement count, not status count.
        TestGnssStatusCallback gnssStatusCallback = new TestGnssStatusCallback(TAG, 0);
        mTestLocationManager.registerGnssStatusCallback(gnssStatusCallback);

        SoftAssert softAssert = new SoftAssert(TAG);
        boolean success = mLocationListener.await();
        softAssert.assertTrue(
                "Time elapsed without getting enough location fixes."
                        + " Possibly, the test has been run deep indoors."
                        + " Consider retrying test outdoors.",
                success);
        mTestLocationManager.unregisterGnssStatusCallback(gnssStatusCallback);

        Log.i(TAG, "Location status received = " + mLocationListener.isLocationReceived());

        if (!mMeasurementListener.verifyStatus()) {
            // If test is strict and verifyStatus returns false, an assert exception happens and
            // test fails.   If test is not strict, we arrive here, and:
            return; // exit (with pass)
        }

        List<GnssMeasurementsEvent> events = mMeasurementListener.getEvents();
        int eventCount = events.size();
        Log.i(TAG, "Number of Gps Event received = " + eventCount);

        softAssert.assertTrue("GnssMeasurementEvent count", "X > 0",
                String.valueOf(eventCount), eventCount > 0);

        boolean carrierPhaseQualityPrrFound = false;
        // we received events, so perform a quick sanity check on mandatory fields
        for (GnssMeasurementsEvent event : events) {
            // Verify Gps Event mandatory fields are in required ranges
            assertNotNull("GnssMeasurementEvent cannot be null.", event);

            // TODO(sumitk): To validate the timestamp if we receive GPS clock.
            long timeInNs = event.getClock().getTimeNanos();
            TestMeasurementUtil.assertGnssClockFields(event.getClock(), softAssert, timeInNs);

            for (GnssMeasurement measurement : event.getMeasurements()) {
                TestMeasurementUtil.assertAllGnssMeasurementMandatoryFields(mTestLocationManager,
                        measurement, softAssert, timeInNs);
                carrierPhaseQualityPrrFound |=
                        TestMeasurementUtil.gnssMeasurementHasCarrierPhasePrr(measurement);
                if (measurement.hasCarrierFrequencyHz()) {
                    mGnssMeasSvStringIds.add(
                        TestMeasurementUtil.getUniqueSvStringId(measurement.getConstellationType(),
                            measurement.getSvid(), measurement.getCarrierFrequencyHz()));
                } else {
                    mGnssMeasSvStringIds.add(
                        TestMeasurementUtil.getUniqueSvStringId(measurement.getConstellationType(),
                            measurement.getSvid()));
                }
            }
        }
        TestMeasurementUtil.assertGnssClockHasConsistentFullBiasNanos(softAssert, events);

        softAssert.assertTrue(
                "GNSS Measurements PRRs with Carrier Phase "
                        + "level uncertainties.  If failed, retry near window or outdoors?",
                carrierPhaseQualityPrrFound);
        Log.i(TAG, "Meas received for:" + mGnssMeasSvStringIds);
        Log.i(TAG, "Status Received for:" + gnssStatusCallback.getGnssUsedSvStringIds());

        // Logging YEAR_2017 Capability.
        // Get all SVs marked as USED in GnssStatus. Remove any SV for which measurement
        // is received. The resulting list should ideally be empty (How can you use a SV
        // with no measurement). To allow for race condition where the last GNSS Status
        // has 1 SV with used flag set, but the corresponding measurement has not yet been
        // received, the check is done as <= 1
        Set<String> svDiff = gnssStatusCallback.getGnssUsedSvStringIds();
        svDiff.removeAll(mGnssMeasSvStringIds);
        softAssert.assertOrWarnTrue(/* strict= */ YEAR_2017_CAPABILITY_ENFORCED,
                "Used Svs with no Meas: " + (svDiff.isEmpty() ? "None" : svDiff),
                svDiff.size() <= 1);

        softAssert.assertAll();
    }
}
