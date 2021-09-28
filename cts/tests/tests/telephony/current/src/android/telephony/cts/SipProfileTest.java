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

import static androidx.test.InstrumentationRegistry.getContext;

import static org.junit.Assert.assertEquals;

import android.content.pm.PackageManager;
import android.net.sip.SipProfile;

import org.junit.Before;
import org.junit.Test;

public class SipProfileTest {
    private static final String SIP_URI = "test";

    private PackageManager mPackageManager;

    @Before
    public void setUp() {
        mPackageManager = getContext().getPackageManager();
    }

    @Test
    public void testSetCallingUid() throws Exception {
        if (!mPackageManager.hasSystemFeature(PackageManager.FEATURE_SIP)
                || !mPackageManager.hasSystemFeature(PackageManager.FEATURE_SIP_VOIP)) {
            return;
        }
        SipProfile sipProfile = new SipProfile.Builder(SIP_URI).build();
        int uid = 0;
        sipProfile.setCallingUid(uid);
        assertEquals(uid, sipProfile.getCallingUid());
        uid = 1;
        sipProfile.setCallingUid(uid);
        assertEquals(uid, sipProfile.getCallingUid());
    }
}
