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
import static org.junit.Assert.assertTrue;

import android.os.Parcel;
import android.telephony.AvailableNetworkInfo;

import org.junit.Test;

import java.util.ArrayList;
import java.util.List;

public class AvailableNetworkInfoTest {
    private static final String OPERATOR_MCCMNC_1 = "123456";
    private static final String OPERATOR_MCCMNC_2 = "246135";
    private static final int SUB_ID = 123;

    @Test
    public void testAvailableNetworkInfo() {
        List<String> mccMncs = new ArrayList<String>();
        mccMncs.add(OPERATOR_MCCMNC_1);
        mccMncs.add(OPERATOR_MCCMNC_2);
        List<Integer> bands = new ArrayList<Integer>();

        AvailableNetworkInfo availableNetworkInfo = new AvailableNetworkInfo(SUB_ID,
                AvailableNetworkInfo.PRIORITY_HIGH, mccMncs, bands);
        assertEquals(0, availableNetworkInfo.describeContents());
        assertEquals(SUB_ID, availableNetworkInfo.getSubId());
        assertEquals(AvailableNetworkInfo.PRIORITY_HIGH, availableNetworkInfo.getPriority());
        assertEquals(mccMncs, availableNetworkInfo.getMccMncs());
        assertEquals(bands, availableNetworkInfo.getBands());

        Parcel availableNetworkInfoParcel = Parcel.obtain();
        availableNetworkInfo.writeToParcel(availableNetworkInfoParcel, 0);
        availableNetworkInfoParcel.setDataPosition(0);
        AvailableNetworkInfo tempAvailableNetworkInfo =
                AvailableNetworkInfo.CREATOR.createFromParcel(availableNetworkInfoParcel);
        assertTrue(tempAvailableNetworkInfo.equals(availableNetworkInfo));
    }
}
