/*
 * Copyright (C) 2009 The Android Open Source Project
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

import android.telephony.SmsCbEtwsInfo;

import com.android.internal.telephony.gsm.SmsCbConstants;

import org.junit.Test;

public class SmsCbEtwsInfoTest {

    private static final int TEST_ETWS_WARNING_TYPE =
            SmsCbConstants.MESSAGE_ID_ETWS_OTHER_EMERGENCY_TYPE;

    @Test
    public void testIsPrimary() {
        SmsCbEtwsInfo info = new SmsCbEtwsInfo(TEST_ETWS_WARNING_TYPE,
                false, false, false, null);
        assertFalse(info.isPrimary());

        SmsCbEtwsInfo info2 = new SmsCbEtwsInfo(TEST_ETWS_WARNING_TYPE,
                false, false, true, null);
        assertTrue(info2.isPrimary());
    }

    @Test
    public void testIsPopupAlert() {
        SmsCbEtwsInfo info = new SmsCbEtwsInfo(TEST_ETWS_WARNING_TYPE,
                false, false, false, null);
        assertFalse(info.isPopupAlert());

        SmsCbEtwsInfo info2 = new SmsCbEtwsInfo(TEST_ETWS_WARNING_TYPE,
                false, true, false, null);
        assertTrue(info2.isPopupAlert());
    }

    @Test
    public void testIsEmergencyUserAlert() {
        SmsCbEtwsInfo info = new SmsCbEtwsInfo(TEST_ETWS_WARNING_TYPE,
                false, false, false, null);
        assertFalse(info.isEmergencyUserAlert());

        SmsCbEtwsInfo info2 = new SmsCbEtwsInfo(TEST_ETWS_WARNING_TYPE,
                true, false, false, null);
        assertTrue(info2.isEmergencyUserAlert());
    }

    @Test
    public void testGetWarningType() {
        SmsCbEtwsInfo info = new SmsCbEtwsInfo(TEST_ETWS_WARNING_TYPE,
                false, false, false, null);
        assertEquals(TEST_ETWS_WARNING_TYPE, info.getWarningType());
    }
}
