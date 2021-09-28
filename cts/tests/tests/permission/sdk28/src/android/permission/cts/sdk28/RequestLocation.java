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

package android.permission.cts.sdk28;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.fail;

import android.content.Context;
import android.content.pm.PackageManager;
import android.telephony.NeighboringCellInfo;
import android.telephony.TelephonyManager;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.List;

@RunWith(AndroidJUnit4.class)
public class RequestLocation {

    private TelephonyManager mTelephonyManager;
    private boolean mHasTelephony;

    @Before
    public void setUp() throws Exception {
        mHasTelephony = getContext().getPackageManager().hasSystemFeature(
                PackageManager.FEATURE_TELEPHONY);
        mTelephonyManager = (TelephonyManager) getContext().getSystemService(
                Context.TELEPHONY_SERVICE);
        assertNotNull(mTelephonyManager);
    }

    /**
     * Verify that a SecurityException is thrown when an app targeting SDK 28
     * lacks the coarse location permission.
     */
    @Test
    public void testGetNeighboringCellInfo() {
        if (!mHasTelephony) return;
        try {
            List<NeighboringCellInfo> cellInfos = mTelephonyManager.getNeighboringCellInfo();
            if (cellInfos != null && !cellInfos.isEmpty()) {
                fail("Meaningful information returned from getNeighboringCellInfo!");
            }
        } catch (SecurityException expected) {
        }
    }

    private static Context getContext() {
        return InstrumentationRegistry.getContext();
    }
}
