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

package android.telephony.ims.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.os.Parcel;
import android.telephony.ims.ImsStreamMediaProfile;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class ImsStreamMediaProfileTest {

    @Test
    public void testParcelUnparcel() {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }
        ImsStreamMediaProfile data = new ImsStreamMediaProfile(
                ImsStreamMediaProfile.AUDIO_QUALITY_AMR_WB,
                ImsStreamMediaProfile.DIRECTION_SEND_RECEIVE,
                ImsStreamMediaProfile.VIDEO_QUALITY_QCIF,
                ImsStreamMediaProfile.DIRECTION_RECEIVE,
                ImsStreamMediaProfile.RTT_MODE_FULL);

        ImsStreamMediaProfile unparceledData = (ImsStreamMediaProfile) parcelUnparcel(data);

        assertEquals(data.getAudioDirection(), unparceledData.getAudioDirection());
        assertEquals(data.getAudioQuality(), unparceledData.getAudioQuality());
        assertEquals(data.getRttMode(), unparceledData.getRttMode());
        assertEquals(data.getVideoDirection(), unparceledData.getVideoDirection());
        assertEquals(data.getVideoQuality(), unparceledData.getVideoQuality());
    }

    @Test
    public void testCopyFrom() {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }
        ImsStreamMediaProfile data = new ImsStreamMediaProfile(
                ImsStreamMediaProfile.AUDIO_QUALITY_AMR_WB,
                ImsStreamMediaProfile.DIRECTION_SEND_RECEIVE,
                ImsStreamMediaProfile.VIDEO_QUALITY_QCIF,
                ImsStreamMediaProfile.DIRECTION_RECEIVE,
                ImsStreamMediaProfile.RTT_MODE_FULL);

        ImsStreamMediaProfile copiedData = new ImsStreamMediaProfile(0, 0, 0, 0, 0);
        copiedData.copyFrom(data);

        assertEquals(data.getAudioDirection(), copiedData.getAudioDirection());
        assertEquals(data.getAudioQuality(), copiedData.getAudioQuality());
        assertEquals(data.getRttMode(), copiedData.getRttMode());
        assertEquals(data.getVideoDirection(), copiedData.getVideoDirection());
        assertEquals(data.getVideoQuality(), copiedData.getVideoQuality());
    }

    @Test
    public void testSetRttMode() {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }
        ImsStreamMediaProfile data = new ImsStreamMediaProfile(
                ImsStreamMediaProfile.AUDIO_QUALITY_AMR_WB,
                ImsStreamMediaProfile.DIRECTION_SEND_RECEIVE,
                ImsStreamMediaProfile.VIDEO_QUALITY_QCIF,
                ImsStreamMediaProfile.DIRECTION_RECEIVE,
                ImsStreamMediaProfile.RTT_MODE_FULL);
        assertTrue(data.isRttCall());

        data.setRttMode(ImsStreamMediaProfile.RTT_MODE_DISABLED);
        assertFalse(data.isRttCall());
    }

    @Test
    public void testReceivingRttAudio() {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }
        ImsStreamMediaProfile data = new ImsStreamMediaProfile(
                ImsStreamMediaProfile.AUDIO_QUALITY_AMR_WB,
                ImsStreamMediaProfile.DIRECTION_SEND_RECEIVE,
                ImsStreamMediaProfile.VIDEO_QUALITY_QCIF,
                ImsStreamMediaProfile.DIRECTION_RECEIVE,
                ImsStreamMediaProfile.RTT_MODE_FULL);

        data.setReceivingRttAudio(true);

        ImsStreamMediaProfile unparceled = (ImsStreamMediaProfile) parcelUnparcel(data);

        assertTrue(unparceled.isReceivingRttAudio());
    }

    public Object parcelUnparcel(Object dataObj) {
        ImsStreamMediaProfile data = (ImsStreamMediaProfile) dataObj;
        Parcel parcel = Parcel.obtain();
        data.writeToParcel(parcel, 0);
        parcel.setDataPosition(0);
        ImsStreamMediaProfile unparceledData =
                ImsStreamMediaProfile.CREATOR.createFromParcel(parcel);
        parcel.recycle();
        return unparceledData;
    }
}
