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

package android.location.cts.fine;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import android.location.GnssAntennaInfo;
import android.location.GnssAntennaInfo.PhaseCenterOffset;
import android.location.GnssAntennaInfo.SphericalCorrections;
import android.os.Parcel;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Tests fundamental functionality of GnssAntennaInfo class. This includes writing and reading from
 * parcel, and verifying computed values and getters.
 */
@RunWith(AndroidJUnit4.class)
public class GnssAntennaInfoTest {

    private static final double PRECISION = 0.0001;
    private static final double[][] PHASE_CENTER_VARIATION_CORRECTIONS = new double[][]{
        {5.29, 0.20, 7.15, 10.18, 9.47, 8.05},
        {11.93, 3.98, 2.68, 2.66, 8.15, 13.54},
        {14.69, 7.63, 13.46, 8.70, 4.36, 1.21},
        {4.19, 12.43, 12.40, 0.90, 1.96, 1.99},
        {7.30, 0.49, 7.43, 8.71, 3.70, 7.24},
        {4.79, 1.88, 13.88, 3.52, 13.40, 11.81}
    };
    private static final double[][] PHASE_CENTER_VARIATION_CORRECTION_UNCERTAINTIES = new double[][]{
            {1.77, 0.81, 0.72, 1.65, 2.35, 1.22},
            {0.77, 3.43, 2.77, 0.97, 4.55, 1.38},
            {1.51, 2.50, 2.23, 2.43, 1.94, 0.90},
            {0.34, 4.72, 4.14, 4.78, 4.57, 1.69},
            {4.49, 0.05, 2.78, 1.33, 3.20, 2.75},
            {1.09, 0.31, 3.79, 4.32, 0.65, 1.23}
    };
    private static final double[][] SIGNAL_GAIN_CORRECTIONS = new double[][]{
            {0.19, 7.04, 1.65, 14.84, 2.95, 9.21},
            {0.45, 6.27, 14.57, 8.95, 3.92, 12.68},
            {6.80, 13.04, 7.92, 2.23, 14.22, 7.36},
            {4.81, 11.78, 5.04, 5.13, 12.09, 12.85},
            {0.88, 4.04, 5.71, 3.72, 12.62, 0.40},
            {14.26, 9.50, 4.21, 11.14, 6.54, 14.63}
    };
    private static final double[][] SIGNAL_GAIN_CORRECTION_UNCERTAINTIES = new double[][]{
            {4.74, 1.54, 1.59, 4.05, 1.65, 2.46},
            {0.10, 0.33, 0.84, 0.83, 0.57, 2.66},
            {2.08, 1.46, 2.10, 3.25, 1.48, 0.65},
            {4.02, 2.90, 2.51, 2.13, 1.67, 1.23},
            {2.13, 4.30, 1.36, 3.86, 1.02, 2.96},
            {3.22, 3.95, 3.75, 1.73, 1.91, 4.93}

    };

    @Test
    public void testFullAntennaInfoDescribeContents() {
        GnssAntennaInfo gnssAntennaInfo = createFullTestGnssAntennaInfo();
        assertEquals(0, gnssAntennaInfo.describeContents());
    }

    @Test
    public void testPartialAntennaInfoDescribeContents() {
        GnssAntennaInfo gnssAntennaInfo = createPartialTestGnssAntennaInfo();
        assertEquals(0, gnssAntennaInfo.describeContents());
    }

    @Test
    public void testFullAntennaInfoWriteToParcel() {
        GnssAntennaInfo gnssAntennaInfo = createFullTestGnssAntennaInfo();
        Parcel parcel = Parcel.obtain();
        gnssAntennaInfo.writeToParcel(parcel, 0);
        parcel.setDataPosition(0);
        GnssAntennaInfo newGnssAntennaInfo = GnssAntennaInfo.CREATOR.createFromParcel(parcel);
        verifyFullGnssAntennaInfoValuesAndGetters(newGnssAntennaInfo);
        parcel.recycle();
    }

    @Test
    public void testPartialAntennaInfoWriteToParcel() {
        GnssAntennaInfo gnssAntennaInfo = createPartialTestGnssAntennaInfo();
        Parcel parcel = Parcel.obtain();
        gnssAntennaInfo.writeToParcel(parcel, 0);
        parcel.setDataPosition(0);
        GnssAntennaInfo newGnssAntennaInfo = GnssAntennaInfo.CREATOR.createFromParcel(parcel);
        verifyPartialGnssAntennaInfoValuesAndGetters(newGnssAntennaInfo);
        parcel.recycle();
    }

    @Test
    public void testCreateFullGnssAntennaInfoAndGetValues() {
        GnssAntennaInfo gnssAntennaInfo = createFullTestGnssAntennaInfo();
        verifyFullGnssAntennaInfoValuesAndGetters(gnssAntennaInfo);
    }

    @Test
    public void testCreatePartialGnssAntennaInfoAndGetValues() {
        GnssAntennaInfo gnssAntennaInfo = createPartialTestGnssAntennaInfo();
        verifyPartialGnssAntennaInfoValuesAndGetters(gnssAntennaInfo);
    }

    private static GnssAntennaInfo createFullTestGnssAntennaInfo() {
        double carrierFrequencyMHz = 13758.0;

        GnssAntennaInfo.PhaseCenterOffset phaseCenterOffset = new
                GnssAntennaInfo.PhaseCenterOffset(
                        4.3d,
                    1.4d,
                    2.10d,
                    2.1d,
                    3.12d,
                    0.5d);

        double[][] phaseCenterVariationCorrectionsMillimeters = PHASE_CENTER_VARIATION_CORRECTIONS;
        double[][] phaseCenterVariationCorrectionsUncertaintyMillimeters =
                PHASE_CENTER_VARIATION_CORRECTION_UNCERTAINTIES;
        SphericalCorrections
                phaseCenterVariationCorrections =
                new SphericalCorrections(
                        phaseCenterVariationCorrectionsMillimeters,
                        phaseCenterVariationCorrectionsUncertaintyMillimeters);

        double[][] signalGainCorrectionsDbi = SIGNAL_GAIN_CORRECTIONS;
        double[][] signalGainCorrectionsUncertaintyDbi = SIGNAL_GAIN_CORRECTION_UNCERTAINTIES;
        SphericalCorrections signalGainCorrections = new
                SphericalCorrections(
                signalGainCorrectionsDbi,
                signalGainCorrectionsUncertaintyDbi);

        return new GnssAntennaInfo.Builder()
                .setCarrierFrequencyMHz(carrierFrequencyMHz)
                .setPhaseCenterOffset(phaseCenterOffset)
                .setPhaseCenterVariationCorrections(phaseCenterVariationCorrections)
                .setSignalGainCorrections(signalGainCorrections)
                .build();
    }

    private static GnssAntennaInfo createPartialTestGnssAntennaInfo() {
        double carrierFrequencyMHz = 13758.0;

        GnssAntennaInfo.PhaseCenterOffset phaseCenterOffset = new
                GnssAntennaInfo.PhaseCenterOffset(
                4.3d,
                1.4d,
                2.10d,
                2.1d,
                3.12d,
                0.5d);

        return new GnssAntennaInfo.Builder()
                .setCarrierFrequencyMHz(carrierFrequencyMHz)
                .setPhaseCenterOffset(phaseCenterOffset)
                .build();
    }

    private static void verifyPartialGnssAntennaInfoValuesAndGetters(GnssAntennaInfo gnssAntennaInfo) {
        assertEquals(13758.0d, gnssAntennaInfo.getCarrierFrequencyMHz(), PRECISION);

        // Phase Center Offset Tests --------------------------------------------------------
        PhaseCenterOffset phaseCenterOffset =
                gnssAntennaInfo.getPhaseCenterOffset();
        assertEquals(4.3d, phaseCenterOffset.getXOffsetMm(),
                PRECISION);
        assertEquals(1.4d, phaseCenterOffset.getXOffsetUncertaintyMm(),
                PRECISION);
        assertEquals(2.10d, phaseCenterOffset.getYOffsetMm(),
                PRECISION);
        assertEquals(2.1d, phaseCenterOffset.getYOffsetUncertaintyMm(),
                PRECISION);
        assertEquals(3.12d, phaseCenterOffset.getZOffsetMm(),
                PRECISION);
        assertEquals(0.5d, phaseCenterOffset.getZOffsetUncertaintyMm(),
                PRECISION);

        // Phase Center Variation Corrections Tests -----------------------------------------
        assertNull(gnssAntennaInfo.getPhaseCenterVariationCorrections());

        // Signal Gain Corrections Tests -----------------------------------------------------
        assertNull(gnssAntennaInfo.getSignalGainCorrections());
    }

    private static void verifyFullGnssAntennaInfoValuesAndGetters(GnssAntennaInfo gnssAntennaInfo) {
        assertEquals(13758.0d, gnssAntennaInfo.getCarrierFrequencyMHz(), PRECISION);

        // Phase Center Offset Tests --------------------------------------------------------
        PhaseCenterOffset phaseCenterOffset =
                gnssAntennaInfo.getPhaseCenterOffset();
        assertEquals(4.3d, phaseCenterOffset.getXOffsetMm(),
                PRECISION);
        assertEquals(1.4d, phaseCenterOffset.getXOffsetUncertaintyMm(),
                PRECISION);
        assertEquals(2.10d, phaseCenterOffset.getYOffsetMm(),
                PRECISION);
        assertEquals(2.1d, phaseCenterOffset.getYOffsetUncertaintyMm(),
                PRECISION);
        assertEquals(3.12d, phaseCenterOffset.getZOffsetMm(),
                PRECISION);
        assertEquals(0.5d, phaseCenterOffset.getZOffsetUncertaintyMm(),
                PRECISION);

        // Phase Center Variation Corrections Tests -----------------------------------------
        SphericalCorrections phaseCenterVariationCorrections =
                gnssAntennaInfo.getPhaseCenterVariationCorrections();

        assertEquals(60.0d, phaseCenterVariationCorrections.getDeltaTheta(), PRECISION);
        assertEquals(36.0d, phaseCenterVariationCorrections.getDeltaPhi(), PRECISION);
        assertArrayEquals(PHASE_CENTER_VARIATION_CORRECTIONS, phaseCenterVariationCorrections
                .getCorrectionsArray());
        assertArrayEquals(PHASE_CENTER_VARIATION_CORRECTION_UNCERTAINTIES,
                phaseCenterVariationCorrections.getCorrectionUncertaintiesArray());

        // Signal Gain Corrections Tests -----------------------------------------------------
        SphericalCorrections signalGainCorrections = gnssAntennaInfo.getSignalGainCorrections();

        assertEquals(60.0d, signalGainCorrections.getDeltaTheta(), PRECISION);
        assertEquals(36.0d, signalGainCorrections.getDeltaPhi(), PRECISION);
        assertArrayEquals(SIGNAL_GAIN_CORRECTIONS, signalGainCorrections
                .getCorrectionsArray());
        assertArrayEquals(SIGNAL_GAIN_CORRECTION_UNCERTAINTIES,
                signalGainCorrections.getCorrectionUncertaintiesArray());
    }
}
