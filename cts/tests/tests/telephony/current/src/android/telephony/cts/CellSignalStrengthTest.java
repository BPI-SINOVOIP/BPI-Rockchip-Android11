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

package android.telephony.cts;

import static org.junit.Assert.assertEquals;

import android.telephony.CellSignalStrength;
import android.telephony.SignalStrength;

import org.junit.Test;

/**
 * Test CellSignalStrength to ensure that valid data is being reported and that invalid data is
 * not reported.
 */
public class CellSignalStrengthTest {
    private static final String TAG = "CellSignalStrengthTest";

    /** Check whether NUM_SIGNAL_STRENGTH_BINS holds value 5 as required by
     * {@link SignalStrength#getLevel)} which returns value between 0 and 4. */
    @Test
    public void testGetNumSignalStrengthLevels() {
        assertEquals(5, CellSignalStrength.getNumSignalStrengthLevels());
    }

}
