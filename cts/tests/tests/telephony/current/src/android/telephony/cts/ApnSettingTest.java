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

package android.telephony.cts;

import static org.junit.Assert.assertEquals;

import android.telephony.data.ApnSetting;

import org.junit.Test;

public class ApnSettingTest {
    @Test
    public void testGetApnTypesStringFromBitmaskWithDefault() {
        String value = ApnSetting.getApnTypesStringFromBitmask(ApnSetting.TYPE_DEFAULT);
        assertEquals(value, "hipri,default");
    }

    @Test
    public void testGetApnTypesStringFromBitmaskWithSingleValue() {
        String value = ApnSetting.getApnTypesStringFromBitmask(ApnSetting.TYPE_DUN);
        assertEquals(value, "dun");
    }

    @Test
    public void testGetApnTypesStringFromBitmaskWithSeveralValues() {
        String value = ApnSetting.getApnTypesStringFromBitmask(ApnSetting.TYPE_MCX
                | ApnSetting.TYPE_DUN | ApnSetting.TYPE_EMERGENCY);
        assertEquals(value, "dun,emergency,mcx");
    }
}
