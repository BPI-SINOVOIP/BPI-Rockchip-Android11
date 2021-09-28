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
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.net.sip.SipException;
import android.net.sip.SipManager;
import android.net.sip.SipProfile;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import java.text.ParseException;
import java.util.List;

public class SipManagerTest {
    private Context mContext;
    private SipManager mSipManager;
    private static final String SIP_URI1 = "test1";
    private static final String SIP_URI2 = "test2";

    @Before
    public void setUp() {
        mContext = getContext();
        mSipManager = SipManager.newInstance(mContext);
    }

    @After
    public void tearDown() throws SipException {
        if (mSipManager != null) {
            for (SipProfile profile : mSipManager.getProfiles()) {
                mSipManager.close(profile.getUriString());
            }
        }
    }

    @Test
    public void testGetProfiles() throws SipException, ParseException {
        if (!SipManager.isApiSupported(mContext)) {
            return;
        }
        SipProfile sipProfile1 = new SipProfile.Builder(SIP_URI1).build();
        SipProfile sipProfile2 = new SipProfile.Builder(SIP_URI2).build();
        mSipManager.open(sipProfile1);
        mSipManager.open(sipProfile2);
        List<SipProfile> profiles = mSipManager.getProfiles();
        assertEquals(2, profiles.size());
        assertTrue(profiles.get(0).getUriString().contains(SIP_URI2));
        assertTrue(profiles.get(1).getUriString().contains(SIP_URI1));
    }
}
