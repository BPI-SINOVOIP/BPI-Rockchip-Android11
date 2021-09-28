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
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.os.Bundle;
import android.os.Parcel;
import android.telecom.VideoProfile;
import android.telephony.TelephonyManager;
import android.telephony.emergency.EmergencyNumber;
import android.telephony.ims.ImsCallProfile;
import android.telephony.ims.ImsStreamMediaProfile;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.List;

@RunWith(AndroidJUnit4.class)
public class ImsCallProfileTest {

    @Test
    public void testParcelUnparcel() {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }
        Bundle testBundle = new Bundle();
        testBundle.putString("testString", "testResult");
        Bundle testProprietaryBundle = new Bundle();
        testProprietaryBundle.putString("proprietaryString", "proprietaryValue");
        testBundle.putBundle(ImsCallProfile.EXTRA_OEM_EXTRAS, testProprietaryBundle);
        ImsStreamMediaProfile testProfile = new ImsStreamMediaProfile(1, 1, 1, 1, 1);
        ImsCallProfile data = new ImsCallProfile(ImsCallProfile.SERVICE_TYPE_NORMAL,
                ImsCallProfile.CALL_TYPE_VOICE_N_VIDEO, testBundle, testProfile);
        data.setCallerNumberVerificationStatus(ImsCallProfile.VERIFICATION_STATUS_PASSED);
        data.setEmergencyServiceCategories(EmergencyNumber.EMERGENCY_SERVICE_CATEGORY_POLICE
                | EmergencyNumber.EMERGENCY_SERVICE_CATEGORY_AMBULANCE);
        List<String> testUrns = new ArrayList<>();
        testUrns.add("sos.ambulance");
        data.setEmergencyUrns(testUrns);
        data.setEmergencyCallRouting(EmergencyNumber.EMERGENCY_CALL_ROUTING_NORMAL);
        data.setEmergencyCallTesting(true);
        data.setHasKnownUserIntentEmergency(true);

        Parcel dataParceled = Parcel.obtain();
        data.writeToParcel(dataParceled, 0);
        dataParceled.setDataPosition(0);
        ImsCallProfile unparceledData =
                ImsCallProfile.CREATOR.createFromParcel(dataParceled);
        dataParceled.recycle();

        assertEquals(data.getServiceType(), unparceledData.getServiceType());
        assertEquals(data.getCallType(), unparceledData.getCallType());
        assertEquals(data.getEmergencyServiceCategories(),
                unparceledData.getEmergencyServiceCategories());
        assertEquals(data.getEmergencyUrns(),
                unparceledData.getEmergencyUrns());
        assertEquals(EmergencyNumber.EMERGENCY_SERVICE_CATEGORY_POLICE
                        | EmergencyNumber.EMERGENCY_SERVICE_CATEGORY_AMBULANCE,
                unparceledData.getEmergencyServiceCategories());
        assertEquals(testUrns, unparceledData.getEmergencyUrns());
        assertEquals(data.getEmergencyCallRouting(),
                unparceledData.getEmergencyCallRouting());
        assertEquals(EmergencyNumber.EMERGENCY_CALL_ROUTING_NORMAL,
                unparceledData.getEmergencyCallRouting());
        assertEquals(true, unparceledData.isEmergencyCallTesting());
        assertEquals(true, unparceledData.hasKnownUserIntentEmergency());

        //stream media profile parceling
        ImsStreamMediaProfile resultProfile = unparceledData.getMediaProfile();
        assertEquals(testProfile.getAudioDirection(), resultProfile.getAudioDirection());
        assertEquals(testProfile.getAudioQuality(), resultProfile.getAudioQuality());
        assertEquals(testProfile.getRttMode(), resultProfile.getRttMode());
        assertEquals(testProfile.getVideoDirection(), resultProfile.getVideoDirection());
        assertEquals(testProfile.getVideoQuality(), resultProfile.getVideoQuality());
        // bundle
        assertEquals(testBundle.getString("testString"), unparceledData.getCallExtra("testString"));
        Bundle unparceledProprietaryBundle = unparceledData.getProprietaryCallExtras();
        assertNotNull(unparceledProprietaryBundle);
        assertEquals(testProprietaryBundle.getString("proprietaryString"),
                unparceledProprietaryBundle.getString("proprietaryString"));
        // number verification
        assertEquals(ImsCallProfile.VERIFICATION_STATUS_PASSED,
                unparceledData.getCallerNumberVerificationStatus());
    }

    @Test
    public void testProprietaryExtrasNullCallExtras() {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }
        ImsStreamMediaProfile testProfile = new ImsStreamMediaProfile(1, 1, 1, 1, 1);
        // pass in null for bundle
        ImsCallProfile data = new ImsCallProfile(ImsCallProfile.SERVICE_TYPE_NORMAL,
                ImsCallProfile.CALL_TYPE_VOICE_N_VIDEO, null /*bundle*/, testProfile);

        Parcel dataParceled = Parcel.obtain();
        data.writeToParcel(dataParceled, 0);
        dataParceled.setDataPosition(0);
        ImsCallProfile unparceledData =
                ImsCallProfile.CREATOR.createFromParcel(dataParceled);
        dataParceled.recycle();

        assertNotNull(unparceledData.getProprietaryCallExtras());
    }

    @Test
    public void testProprietaryExtrasEmptyExtras() {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }
        // Empty bundle
        Bundle testBundle = new Bundle();
        ImsStreamMediaProfile testProfile = new ImsStreamMediaProfile(1, 1, 1, 1, 1);
        ImsCallProfile data = new ImsCallProfile(ImsCallProfile.SERVICE_TYPE_NORMAL,
                ImsCallProfile.CALL_TYPE_VOICE_N_VIDEO, testBundle, testProfile);

        Parcel dataParceled = Parcel.obtain();
        data.writeToParcel(dataParceled, 0);
        dataParceled.setDataPosition(0);
        ImsCallProfile unparceledData =
                ImsCallProfile.CREATOR.createFromParcel(dataParceled);
        dataParceled.recycle();

        assertNotNull(unparceledData.getProprietaryCallExtras());
    }

    @Test
    public void testCallExtras() {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }
        ImsCallProfile data = new ImsCallProfile();
        // check defaults
        assertEquals("", data.getCallExtra("invalid"));
        assertEquals(-1, data.getCallExtraInt("invalid"));
        assertEquals(false, data.getCallExtraBoolean("invalid"));
        // check returning defaults
        assertEquals("abc", data.getCallExtra("invalid", "abc"));
        assertEquals(2, data.getCallExtraInt("invalid", 2));
        assertEquals(true, data.getCallExtraBoolean("invalid", true));
        // insert/retrieve
        data.setCallExtra("testData", "testResult");
        assertEquals("testResult", data.getCallExtra("testData"));
        data.setCallExtraInt("testData", 1);
        assertEquals(1, data.getCallExtraInt("testData"));
        data.setCallExtraBoolean("testData", true);
        assertEquals(true, data.getCallExtraBoolean("testData"));
    }

    @Test
    public void testUpdateCallType() {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }
        ImsCallProfile data = new ImsCallProfile(ImsCallProfile.SERVICE_TYPE_NORMAL,
                ImsCallProfile.CALL_TYPE_VOICE);
        ImsCallProfile data2 = new ImsCallProfile(ImsCallProfile.SERVICE_TYPE_NORMAL,
                ImsCallProfile.CALL_TYPE_VOICE_N_VIDEO);
        data.updateCallType(data2);
        assertEquals(data.getCallType(), data2.getCallType());
    }

    @Test
    public void testUpdateCallExtras() {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }
        ImsCallProfile data = new ImsCallProfile(ImsCallProfile.SERVICE_TYPE_NORMAL,
                ImsCallProfile.CALL_TYPE_VOICE);
        data.setCallExtra("testData", "testResult");
        ImsCallProfile data2 = new ImsCallProfile(ImsCallProfile.SERVICE_TYPE_NORMAL,
                ImsCallProfile.CALL_TYPE_VOICE_N_VIDEO);
        data2.setCallExtra("testData2", "testResult2");
        data.updateCallExtras(data2);
        assertEquals("testResult2", data.getCallExtra("testData2"));
        assertEquals("", data.getCallExtra("testData"));
    }

    @Test
    public void testUpdateMediaProfile() {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }
        ImsCallProfile data = new ImsCallProfile(ImsCallProfile.SERVICE_TYPE_NORMAL,
                ImsCallProfile.CALL_TYPE_VOICE);
        ImsStreamMediaProfile profile2 = new ImsStreamMediaProfile(1, 1, 1, 1, 1);
        ImsCallProfile data2 = new ImsCallProfile(ImsCallProfile.SERVICE_TYPE_NORMAL,
                ImsCallProfile.CALL_TYPE_VOICE_N_VIDEO, new Bundle(), profile2);
        data.updateMediaProfile(data2);

        assertEquals(profile2.getAudioDirection(), data.getMediaProfile().getAudioDirection());
        assertEquals(profile2.getAudioQuality(), data.getMediaProfile().getAudioQuality());
        assertEquals(profile2.getRttMode(), data.getMediaProfile().getRttMode());
        assertEquals(profile2.getVideoDirection(), data.getMediaProfile().getVideoDirection());
        assertEquals(profile2.getVideoQuality(), data.getMediaProfile().getVideoQuality());
    }

    @Test
    public void testGetVideoStatefromProfile() {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }
        ImsCallProfile profile = new ImsCallProfile(ImsCallProfile.SERVICE_TYPE_NORMAL,
                ImsCallProfile.CALL_TYPE_VT);
        int result = ImsCallProfile.getVideoStateFromImsCallProfile(profile);
        assertTrue(VideoProfile.isVideo(result));
        assertFalse(VideoProfile.isPaused(result));
    }

    @Test
    public void testGetVideoStateFromCallType() {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }
        ImsCallProfile profile = new ImsCallProfile(ImsCallProfile.SERVICE_TYPE_NORMAL,
                ImsCallProfile.CALL_TYPE_VOICE);
        int result = ImsCallProfile.getVideoStateFromCallType(profile.getCallType());
        assertTrue(VideoProfile.isAudioOnly(result));

        profile = new ImsCallProfile(ImsCallProfile.SERVICE_TYPE_NORMAL,
                ImsCallProfile.CALL_TYPE_VT_RX);
        result = ImsCallProfile.getVideoStateFromCallType(profile.getCallType());
        assertTrue(VideoProfile.isReceptionEnabled(result));

        profile = new ImsCallProfile(ImsCallProfile.SERVICE_TYPE_NORMAL,
                ImsCallProfile.CALL_TYPE_VT_TX);
        result = ImsCallProfile.getVideoStateFromCallType(profile.getCallType());
        assertTrue(VideoProfile.isTransmissionEnabled(result));

        profile = new ImsCallProfile(ImsCallProfile.SERVICE_TYPE_NORMAL,
                ImsCallProfile.CALL_TYPE_VT);
        result = ImsCallProfile.getVideoStateFromCallType(profile.getCallType());
        assertTrue(VideoProfile.isBidirectional(result));
    }

    @Test
    public void testGetCallTypeFromVideoState() {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }
        VideoProfile videoProfile = new VideoProfile(VideoProfile.STATE_PAUSED);
        assertEquals(ImsCallProfile.CALL_TYPE_VT_NODIR,
                ImsCallProfile.getCallTypeFromVideoState(videoProfile.getVideoState()));

        videoProfile = new VideoProfile(VideoProfile.STATE_TX_ENABLED);
        assertEquals(ImsCallProfile.CALL_TYPE_VT_TX,
                ImsCallProfile.getCallTypeFromVideoState(videoProfile.getVideoState()));

        videoProfile = new VideoProfile(VideoProfile.STATE_RX_ENABLED);
        assertEquals(ImsCallProfile.CALL_TYPE_VT_RX,
                ImsCallProfile.getCallTypeFromVideoState(videoProfile.getVideoState()));

        videoProfile = new VideoProfile(VideoProfile.STATE_RX_ENABLED
                | VideoProfile.STATE_TX_ENABLED);
        assertEquals(ImsCallProfile.CALL_TYPE_VT,
                ImsCallProfile.getCallTypeFromVideoState(videoProfile.getVideoState()));
    }

    @Test
    public void testIsVideoPaused() {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }
        ImsStreamMediaProfile testProfile = new ImsStreamMediaProfile(1, 1, 1,
                ImsStreamMediaProfile.DIRECTION_INACTIVE, 1);
        ImsCallProfile data = new ImsCallProfile(ImsCallProfile.SERVICE_TYPE_NORMAL,
                ImsCallProfile.CALL_TYPE_VOICE_N_VIDEO, new Bundle(), testProfile);

        assertTrue(data.isVideoPaused());

        ImsStreamMediaProfile testProfile2 = new ImsStreamMediaProfile(1, 1, 1,
                ImsStreamMediaProfile.DIRECTION_SEND_RECEIVE, 1);
        ImsCallProfile data2 = new ImsCallProfile(ImsCallProfile.SERVICE_TYPE_NORMAL,
                ImsCallProfile.CALL_TYPE_VOICE_N_VIDEO, new Bundle(), testProfile2);

        data.updateMediaProfile(data2);

        assertFalse(data.isVideoPaused());

    }

    @Test
    public void testIsVideoCall() {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }
        ImsCallProfile data = new ImsCallProfile(ImsCallProfile.SERVICE_TYPE_NORMAL,
                ImsCallProfile.CALL_TYPE_VT);

        assertTrue(data.isVideoCall());

        ImsCallProfile data2 = new ImsCallProfile(ImsCallProfile.SERVICE_TYPE_NORMAL,
                ImsCallProfile.CALL_TYPE_VOICE);
        data.updateCallType(data2);

        assertFalse(data.isVideoCall());
    }

    @Test
    public void testParcelUnparcelExtraNetworkType() {
        Bundle testBundle = new Bundle();
        ImsStreamMediaProfile testProfile = new ImsStreamMediaProfile(1, 1, 1, 1, 1);
        ImsCallProfile data = new ImsCallProfile(ImsCallProfile.SERVICE_TYPE_NORMAL,
                ImsCallProfile.CALL_TYPE_VOICE_N_VIDEO, testBundle, testProfile);
        data.setCallExtraInt(ImsCallProfile.EXTRA_CALL_NETWORK_TYPE,
                TelephonyManager.NETWORK_TYPE_LTE);

        Parcel dataParceled = Parcel.obtain();
        data.writeToParcel(dataParceled, 0);
        dataParceled.setDataPosition(0);
        ImsCallProfile unparceledData =
                ImsCallProfile.CREATOR.createFromParcel(dataParceled);
        dataParceled.recycle();

        assertEquals("unparceled data for EXTRA_CALL_NETWORK_TYPE is not valid!",
                data.getCallExtraInt(ImsCallProfile.EXTRA_CALL_NETWORK_TYPE),
                unparceledData.getCallExtraInt(ImsCallProfile.EXTRA_CALL_NETWORK_TYPE));
    }
}
