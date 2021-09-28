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

import android.content.Context;
import android.net.rtp.AudioGroup;
import android.net.sip.SipAudioCall;
import android.net.sip.SipProfile;

import org.junit.Before;
import org.junit.Test;

import java.text.ParseException;

public class SipAudioCallTest {
    private static final String SIP_URI = "test";
    private Context mContext;

    @Before
    public void setUp() {
        mContext = getContext();
    }

    @Test
    public void testSetAndGetAudioGroup() throws ParseException {
        SipProfile sipProfile = new SipProfile.Builder(SIP_URI).build();
        SipAudioCall call = new SipAudioCall(mContext, sipProfile);
        AudioGroup audioGroup1 = new AudioGroup(mContext);
        audioGroup1.setMode(AudioGroup.MODE_NORMAL);
        call.setAudioGroup(audioGroup1);
        assertEquals(AudioGroup.MODE_NORMAL, call.getAudioGroup().getMode());

        AudioGroup audioGroup2 = new AudioGroup(mContext);
        audioGroup2.setMode(AudioGroup.MODE_MUTED);
        call.setAudioGroup(audioGroup2);
        assertEquals(AudioGroup.MODE_MUTED, call.getAudioGroup().getMode());
    }
}
