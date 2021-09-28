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

package com.android.internal.telephony;

import static com.google.common.truth.Truth.assertThat;

import android.hardware.radio.V1_4.NrSignalStrength;
import android.os.Parcel;
import android.telephony.CellInfo;
import android.telephony.CellSignalStrength;
import android.telephony.CellSignalStrengthNr;
import android.test.AndroidTestCase;

import com.google.common.collect.BoundType;
import com.google.common.collect.Range;

import org.junit.Test;

public class CellSignalStrengthNrTest extends AndroidTestCase {
    private static final int CSIRSRP = -123;
    private static final int CSIRSRQ = -11;
    private static final int ANOTHER_CSIRSRP = -111;
    private static final int ANOTHER_CSIRSRQ = -12;
    private static final int INVALID_CSIRSRP = Integer.MAX_VALUE;
    private static final int INVALID_SSRSRP = Integer.MAX_VALUE;
    private static final int CSISINR = 18;
    private static final int SSRSRP = -112;
    private static final int SSRSRQ = -13;
    private static final int SSSINR = 32;

    @Test
    public void testGetMethod() {
        // GIVEN an instance of CellSignalStrengthNr
        CellSignalStrengthNr css = new CellSignalStrengthNr(
                CSIRSRP, CSIRSRQ, CSISINR, SSRSRP, SSRSRQ, SSSINR);

        // THEN the get method should return correct value
        assertThat(css.getCsiRsrp()).isEqualTo(CSIRSRP);
        assertThat(css.getCsiRsrq()).isEqualTo(CSIRSRQ);
        assertThat(css.getCsiSinr()).isEqualTo(CSISINR);
        assertThat(css.getSsRsrp()).isEqualTo(SSRSRP);
        assertThat(css.getSsRsrq()).isEqualTo(SSRSRQ);
        assertThat(css.getSsSinr()).isEqualTo(SSSINR);
        assertThat(css.getDbm()).isEqualTo(SSRSRP);
    }

    @Test
    public void testGetMethodWithHal() {
        // GIVEN an instance of NrSignalStrength with some positive values
        NrSignalStrength nrSignalStrength = new NrSignalStrength();
        nrSignalStrength.csiRsrp = -CSIRSRP;
        nrSignalStrength.csiRsrq = -CSIRSRQ;
        nrSignalStrength.csiSinr = CSISINR;
        nrSignalStrength.ssRsrp = -SSRSRP;
        nrSignalStrength.ssRsrq = -SSRSRQ;
        nrSignalStrength.ssSinr = SSSINR;

        // THEN the get method should return the correct value
        CellSignalStrengthNr css = new CellSignalStrengthNr(nrSignalStrength);
        assertThat(css.getCsiRsrp()).isEqualTo(CSIRSRP);
        assertThat(css.getCsiRsrq()).isEqualTo(CSIRSRQ);
        assertThat(css.getCsiSinr()).isEqualTo(CSISINR);
        assertThat(css.getSsRsrp()).isEqualTo(SSRSRP);
        assertThat(css.getSsRsrq()).isEqualTo(SSRSRQ);
        assertThat(css.getSsSinr()).isEqualTo(SSSINR);
        assertThat(css.getDbm()).isEqualTo(SSRSRP);
    }

    @Test
    public void testUnavailableValueWithHal() {
        // GIVEN an instance of NrSignalStrength
        NrSignalStrength nrSignalStrength = new NrSignalStrength();
        nrSignalStrength.csiRsrp = CellInfo.UNAVAILABLE;
        nrSignalStrength.csiRsrq = CellInfo.UNAVAILABLE;
        nrSignalStrength.csiSinr = CellInfo.UNAVAILABLE;
        nrSignalStrength.ssRsrp = CellInfo.UNAVAILABLE;
        nrSignalStrength.ssRsrq = CellInfo.UNAVAILABLE;
        nrSignalStrength.ssSinr = CellInfo.UNAVAILABLE;

        // THEN the get method should return unavailable value
        CellSignalStrengthNr css = new CellSignalStrengthNr(nrSignalStrength);
        assertThat(css.getCsiRsrp()).isEqualTo(CellInfo.UNAVAILABLE);
        assertThat(css.getCsiRsrq()).isEqualTo(CellInfo.UNAVAILABLE);
        assertThat(css.getCsiSinr()).isEqualTo(CellInfo.UNAVAILABLE);
        assertThat(css.getSsRsrp()).isEqualTo(CellInfo.UNAVAILABLE);
        assertThat(css.getSsRsrq()).isEqualTo(CellInfo.UNAVAILABLE);
        assertThat(css.getSsSinr()).isEqualTo(CellInfo.UNAVAILABLE);
        assertThat(css.getDbm()).isEqualTo(CellInfo.UNAVAILABLE);
    }

    @Test
    public void testEquals_sameParameters() {
        // GIVEN an instance of CellSignalStrengthNr and another object with the same parameters
        CellSignalStrengthNr css = new CellSignalStrengthNr(
                CSIRSRP, CSIRSRQ, CSISINR, SSRSRP, SSRSRQ, SSSINR);
        CellSignalStrengthNr anotherCss = new CellSignalStrengthNr(
                CSIRSRP, CSIRSRQ, CSISINR, SSRSRP, SSRSRQ, SSSINR);

        // THEN this two objects are equivalent
        assertThat(css).isEqualTo(anotherCss);
    }

    @Test
    public void testEquals_differentParameters() {
        // GIVEN an instance of CellSignalStrengthNr and another object with some different
        // parameters
        CellSignalStrengthNr css = new CellSignalStrengthNr(
                CSIRSRP, CSIRSRQ, CSISINR, SSRSRP, SSRSRQ, SSSINR);
        CellSignalStrengthNr anotherCss = new CellSignalStrengthNr(
                ANOTHER_CSIRSRP, ANOTHER_CSIRSRQ, CSISINR, SSRSRP, SSRSRQ, SSSINR);

        // THEN this two objects are different
        assertThat(css).isNotEqualTo(anotherCss);
    }

    @Test
    public void testAusLevel_validValue() {
        // GIVEN an instance of CellSignalStrengthNr with valid csirsrp
        CellSignalStrengthNr css = new CellSignalStrengthNr(
                CSIRSRP, CSIRSRQ, CSISINR, SSRSRP, SSRSRQ, SSSINR);

        // THEN the asu level is in range [0, 97]
        assertThat(css.getAsuLevel()).isIn(Range.range(0, BoundType.CLOSED, 97, BoundType.CLOSED));
    }

    @Test
    public void testAsuLevel_invalidValue() {
        // GIVEN an instance of CellSignalStrengthNr with invalid csirsrp
        CellSignalStrengthNr css = new CellSignalStrengthNr(
                CSIRSRP, CSIRSRQ, CSISINR, INVALID_SSRSRP, SSRSRQ, SSSINR);

        // THEN the asu level is unknown
        assertThat(css.getAsuLevel()).isEqualTo(CellSignalStrengthNr.UNKNOWN_ASU_LEVEL);
    }

    @Test
    public void testSignalLevel_validValue() {
        for (int ssRsrp = -140; ssRsrp <= -44; ssRsrp++) {
            // GIVEN an instance of CellSignalStrengthNr with valid csirsrp
            CellSignalStrengthNr css = new CellSignalStrengthNr(
                    CSIRSRP, CSIRSRQ, CSISINR, ssRsrp, SSRSRQ, SSSINR);

            // THEN the signal level is valid
            assertThat(css.getLevel()).isIn(Range.range(
                    CellSignalStrength.SIGNAL_STRENGTH_NONE_OR_UNKNOWN, BoundType.CLOSED,
                    CellSignalStrength.SIGNAL_STRENGTH_GREAT, BoundType.CLOSED));
        }
    }

    @Test
    public void testSignalLevel_invalidValue() {
        // GIVEN an instance of CellSignalStrengthNr with invalid csirsrp
        CellSignalStrengthNr css = new CellSignalStrengthNr(
                CSIRSRP, CSIRSRQ, CSISINR, INVALID_SSRSRP, SSRSRQ, SSSINR);

        // THEN the signal level is unknown
        assertThat(css.getLevel()).isEqualTo(CellSignalStrength.SIGNAL_STRENGTH_NONE_OR_UNKNOWN);
    }

    @Test
    public void testParcel() {
        // GIVEN an instance of CellSignalStrengthNr
        CellSignalStrengthNr css = new CellSignalStrengthNr(
                CSIRSRP, CSIRSRQ, CSISINR, SSRSRP, SSRSRQ, SSSINR);

        // WHEN write the object to parcel and create another object with that parcel
        Parcel parcel = Parcel.obtain();
        css.writeToParcel(parcel, 0 /* type */);
        parcel.setDataPosition(0);
        CellSignalStrengthNr anotherCss = CellSignalStrengthNr.CREATOR.createFromParcel(parcel);

        // THEN the new object is equal to the old one
        assertThat(anotherCss).isEqualTo(css);
        assertThat(anotherCss.getCsiRsrp()).isEqualTo(CSIRSRP);
        assertThat(anotherCss.getCsiRsrq()).isEqualTo(CSIRSRQ);
        assertThat(anotherCss.getCsiSinr()).isEqualTo(CSISINR);
        assertThat(anotherCss.getSsRsrp()).isEqualTo(SSRSRP);
        assertThat(anotherCss.getSsRsrq()).isEqualTo(SSRSRQ);
        assertThat(anotherCss.getSsSinr()).isEqualTo(SSSINR);
    }
}
