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

import android.location.GnssClock;
import android.location.GnssStatus;
import android.os.Parcel;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class GnssClockTest {

    private static final double DELTA = 0.001;

    @Test
    public void testWriteToParcel() {
        GnssClock clock = new GnssClock();
        setTestValues(clock);
        Parcel parcel = Parcel.obtain();
        clock.writeToParcel(parcel, 0);
        parcel.setDataPosition(0);
        GnssClock newClock = GnssClock.CREATOR.createFromParcel(parcel);
        verifyTestValues(newClock);
        parcel.recycle();
    }

    @Test
    public void testReset() {
        GnssClock clock = new GnssClock();
        clock.reset();
    }

    @Test
    public void testSet() {
        GnssClock clock = new GnssClock();
        setTestValues(clock);
        GnssClock newClock = new GnssClock();
        newClock.set(clock);
        verifyTestValues(newClock);
    }

    @Test
    public void testHasAndReset() {
        GnssClock clock = new GnssClock();
        setTestValues(clock);

        assertTrue(clock.hasBiasNanos());
        clock.resetBiasNanos();
        assertFalse(clock.hasBiasNanos());

        assertTrue(clock.hasBiasUncertaintyNanos());
        clock.resetBiasUncertaintyNanos();
        assertFalse(clock.hasBiasUncertaintyNanos());

        assertTrue(clock.hasDriftNanosPerSecond());
        clock.resetDriftNanosPerSecond();
        assertFalse(clock.hasDriftNanosPerSecond());

        assertTrue(clock.hasDriftUncertaintyNanosPerSecond());
        clock.resetDriftUncertaintyNanosPerSecond();
        assertFalse(clock.hasDriftUncertaintyNanosPerSecond());

        assertTrue(clock.hasElapsedRealtimeNanos());
        clock.resetElapsedRealtimeNanos();
        assertFalse(clock.hasElapsedRealtimeNanos());

        assertTrue(clock.hasElapsedRealtimeUncertaintyNanos());
        clock.resetElapsedRealtimeUncertaintyNanos();
        assertFalse(clock.hasElapsedRealtimeUncertaintyNanos());

        assertTrue(clock.hasFullBiasNanos());
        clock.resetFullBiasNanos();
        assertFalse(clock.hasFullBiasNanos());

        assertTrue(clock.hasLeapSecond());
        clock.resetLeapSecond();
        assertFalse(clock.hasLeapSecond());

        assertTrue(clock.hasReferenceConstellationTypeForIsb());
        clock.resetReferenceConstellationTypeForIsb();
        assertFalse(clock.hasReferenceConstellationTypeForIsb());

        assertTrue(clock.hasReferenceCarrierFrequencyHzForIsb());
        clock.resetReferenceCarrierFrequencyHzForIsb();
        assertFalse(clock.hasReferenceCarrierFrequencyHzForIsb());

        assertTrue(clock.hasReferenceCodeTypeForIsb());
        clock.resetReferenceCodeTypeForIsb();
        assertFalse(clock.hasReferenceCodeTypeForIsb());

        assertTrue(clock.hasTimeUncertaintyNanos());
        clock.resetTimeUncertaintyNanos();
        assertFalse(clock.hasTimeUncertaintyNanos());
    }

    private static void setTestValues(GnssClock clock) {
        clock.setBiasNanos(1.0);
        clock.setBiasUncertaintyNanos(2.0);
        clock.setDriftNanosPerSecond(3.0);
        clock.setDriftUncertaintyNanosPerSecond(4.0);
        clock.setElapsedRealtimeNanos(10987732253L);
        clock.setElapsedRealtimeUncertaintyNanos(3943523.0);
        clock.setFullBiasNanos(5);
        clock.setHardwareClockDiscontinuityCount(6);
        clock.setLeapSecond(7);
        clock.setReferenceConstellationTypeForIsb(GnssStatus.CONSTELLATION_GPS);
        clock.setReferenceCarrierFrequencyHzForIsb(1.59975e+09);
        clock.setReferenceCodeTypeForIsb("C");
        clock.setTimeNanos(8);
        clock.setTimeUncertaintyNanos(9.0);
    }

    private static void verifyTestValues(GnssClock clock) {
        assertEquals(1.0, clock.getBiasNanos(), DELTA);
        assertEquals(2.0, clock.getBiasUncertaintyNanos(), DELTA);
        assertEquals(3.0, clock.getDriftNanosPerSecond(), DELTA);
        assertEquals(4.0, clock.getDriftUncertaintyNanosPerSecond(), DELTA);
        assertEquals(10987732253L, clock.getElapsedRealtimeNanos());
        assertEquals(3943523.0, clock.getElapsedRealtimeUncertaintyNanos(), DELTA);
        assertEquals(5, clock.getFullBiasNanos());
        assertEquals(6, clock.getHardwareClockDiscontinuityCount());
        assertEquals(7, clock.getLeapSecond());
        assertEquals(GnssStatus.CONSTELLATION_GPS, clock.getReferenceConstellationTypeForIsb());
        assertEquals(1.59975e+09, clock.getReferenceCarrierFrequencyHzForIsb(), DELTA);
        assertEquals("C", clock.getReferenceCodeTypeForIsb());
        assertEquals(8, clock.getTimeNanos());
        assertEquals(9.0, clock.getTimeUncertaintyNanos(), DELTA);
    }
}
