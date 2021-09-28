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
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.telephony.SmsCbLocation;

import org.junit.Test;

public class SmsCbLocationTest {
    private static final String PLMN = "TEST_PLMN";
    private static final String PLMN2 = "TEST_PLMN 2";
    private static final int LAC = -1;
    private static final int CID = -1;

    @Test
    public void testSmsCbLocation() throws Throwable {
        SmsCbLocation cbLocation = new SmsCbLocation("94040", 1234, 5678);
        assertEquals("94040", cbLocation.getPlmn());
        assertEquals(1234, cbLocation.getLac());
        assertEquals(5678, cbLocation.getCid());
    }

    @Test
    public void testIsInLocationArea() {
        SmsCbLocation cbLocation = new SmsCbLocation(PLMN, LAC, CID);

        SmsCbLocation area = new SmsCbLocation(PLMN, LAC, CID);
        assertTrue(cbLocation.isInLocationArea(area));

        SmsCbLocation area2 = new SmsCbLocation(PLMN2, LAC, CID);
        assertFalse(cbLocation.isInLocationArea(area2));
    }
}
