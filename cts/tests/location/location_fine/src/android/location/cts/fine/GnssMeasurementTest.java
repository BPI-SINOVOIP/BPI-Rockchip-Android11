/*
 * Copyright (C) 2016 The Android Open Source Project
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

package android.location.cts.fine;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.location.GnssMeasurement;
import android.location.GnssStatus;
import android.os.Parcel;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class GnssMeasurementTest {

    private static final double DELTA = 0.001;

    @Test
    public void testDescribeContents() {
        GnssMeasurement measurement = new GnssMeasurement();
        assertEquals(0, measurement.describeContents());
    }

    @Test
    public void testReset() {
        GnssMeasurement measurement = new GnssMeasurement();
        measurement.reset();
    }

    @Test
    public void testWriteToParcel() {
        GnssMeasurement measurement = new GnssMeasurement();
        setTestValues(measurement);
        Parcel parcel = Parcel.obtain();
        measurement.writeToParcel(parcel, 0);
        parcel.setDataPosition(0);
        GnssMeasurement newMeasurement = GnssMeasurement.CREATOR.createFromParcel(parcel);
        verifyTestValues(newMeasurement);
        parcel.recycle();
    }

    @Test
    public void testSet() {
        GnssMeasurement measurement = new GnssMeasurement();
        setTestValues(measurement);
        GnssMeasurement newMeasurement = new GnssMeasurement();
        newMeasurement.set(measurement);
        verifyTestValues(newMeasurement);
    }

    @Test
    public void testSetReset() {
        GnssMeasurement measurement = new GnssMeasurement();
        setTestValues(measurement);

        assertTrue(measurement.hasCarrierCycles());
        measurement.resetCarrierCycles();
        assertFalse(measurement.hasCarrierCycles());

        assertTrue(measurement.hasCarrierFrequencyHz());
        measurement.resetCarrierFrequencyHz();
        assertFalse(measurement.hasCarrierFrequencyHz());

        assertTrue(measurement.hasCarrierPhase());
        measurement.resetCarrierPhase();
        assertFalse(measurement.hasCarrierPhase());

        assertTrue(measurement.hasCarrierPhaseUncertainty());
        measurement.resetCarrierPhaseUncertainty();
        assertFalse(measurement.hasCarrierPhaseUncertainty());

        assertTrue(measurement.hasSnrInDb());
        measurement.resetSnrInDb();
        assertFalse(measurement.hasSnrInDb());

        assertTrue(measurement.hasCodeType());
        measurement.resetCodeType();
        assertFalse(measurement.hasCodeType());

        assertTrue(measurement.hasBasebandCn0DbHz());
        measurement.resetBasebandCn0DbHz();
        assertFalse(measurement.hasBasebandCn0DbHz());

        assertTrue(measurement.hasFullInterSignalBiasNanos());
        measurement.resetFullInterSignalBiasNanos();
        assertFalse(measurement.hasFullInterSignalBiasNanos());

        assertTrue(measurement.hasFullInterSignalBiasUncertaintyNanos());
        measurement.resetFullInterSignalBiasUncertaintyNanos();
        assertFalse(measurement.hasFullInterSignalBiasUncertaintyNanos());

        assertTrue(measurement.hasSatelliteInterSignalBiasNanos());
        measurement.resetSatelliteInterSignalBiasNanos();
        assertFalse(measurement.hasSatelliteInterSignalBiasNanos());

        assertTrue(measurement.hasSatelliteInterSignalBiasUncertaintyNanos());
        measurement.resetSatelliteInterSignalBiasUncertaintyNanos();
        assertFalse(measurement.hasSatelliteInterSignalBiasUncertaintyNanos());
    }

    private static void setTestValues(GnssMeasurement measurement) {
        measurement.setAccumulatedDeltaRangeMeters(1.0);
        measurement.setAccumulatedDeltaRangeState(2);
        measurement.setAccumulatedDeltaRangeUncertaintyMeters(3.0);
        measurement.setBasebandCn0DbHz(3.0);
        measurement.setCarrierCycles(4);
        measurement.setCarrierFrequencyHz(5.0f);
        measurement.setCarrierPhase(6.0);
        measurement.setCarrierPhaseUncertainty(7.0);
        measurement.setCn0DbHz(8.0);
        measurement.setCodeType("C");
        measurement.setConstellationType(GnssStatus.CONSTELLATION_GALILEO);
        measurement.setMultipathIndicator(GnssMeasurement.MULTIPATH_INDICATOR_DETECTED);
        measurement.setPseudorangeRateMetersPerSecond(9.0);
        measurement.setPseudorangeRateUncertaintyMetersPerSecond(10.0);
        measurement.setReceivedSvTimeNanos(11);
        measurement.setReceivedSvTimeUncertaintyNanos(12);
        measurement.setFullInterSignalBiasNanos(1.3);
        measurement.setFullInterSignalBiasUncertaintyNanos(2.5);
        measurement.setSatelliteInterSignalBiasNanos(5.4);
        measurement.setSatelliteInterSignalBiasUncertaintyNanos(10.0);
        measurement.setSnrInDb(13.0);
        measurement.setState(14);
        measurement.setSvid(15);
        measurement.setTimeOffsetNanos(16.0);
    }

    private static void verifyTestValues(GnssMeasurement measurement) {
        assertEquals(1.0, measurement.getAccumulatedDeltaRangeMeters(), DELTA);
        assertEquals(2, measurement.getAccumulatedDeltaRangeState());
        assertEquals(3.0, measurement.getAccumulatedDeltaRangeUncertaintyMeters(), DELTA);
        assertEquals(3.0, measurement.getBasebandCn0DbHz(), DELTA);
        assertEquals(4, measurement.getCarrierCycles());
        assertEquals(5.0f, measurement.getCarrierFrequencyHz(), DELTA);
        assertEquals(6.0, measurement.getCarrierPhase(), DELTA);
        assertEquals(7.0, measurement.getCarrierPhaseUncertainty(), DELTA);
        assertEquals(8.0, measurement.getCn0DbHz(), DELTA);
        assertEquals(GnssStatus.CONSTELLATION_GALILEO, measurement.getConstellationType());
        assertEquals(GnssMeasurement.MULTIPATH_INDICATOR_DETECTED,
                measurement.getMultipathIndicator());
        assertEquals("C", measurement.getCodeType());
        assertEquals(9.0, measurement.getPseudorangeRateMetersPerSecond(), DELTA);
        assertEquals(10.0, measurement.getPseudorangeRateUncertaintyMetersPerSecond(), DELTA);
        assertEquals(11, measurement.getReceivedSvTimeNanos());
        assertEquals(12, measurement.getReceivedSvTimeUncertaintyNanos());
        assertEquals(1.3, measurement.getFullInterSignalBiasNanos(), DELTA);
        assertEquals(2.5, measurement.getFullInterSignalBiasUncertaintyNanos(), DELTA);
        assertEquals(5.4, measurement.getSatelliteInterSignalBiasNanos(), DELTA);
        assertEquals(10.0, measurement.getSatelliteInterSignalBiasUncertaintyNanos(), DELTA);
        assertEquals(13.0, measurement.getSnrInDb(), DELTA);
        assertEquals(14, measurement.getState());
        assertEquals(15, measurement.getSvid());
        assertEquals(16.0, measurement.getTimeOffsetNanos(), DELTA);
    }
}
