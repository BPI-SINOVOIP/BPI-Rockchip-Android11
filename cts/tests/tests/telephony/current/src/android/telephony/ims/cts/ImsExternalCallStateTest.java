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

import android.net.Uri;
import android.os.Parcel;
import android.telephony.ims.ImsCallProfile;
import android.telephony.ims.ImsExternalCallState;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class ImsExternalCallStateTest {

    @Test
    public void parcelUnparcel() {
        String callId = "1";
        Uri address = Uri.fromParts("tel", "5551212", null);
        Uri localAddress = Uri.fromParts("tel", "5551213", null);
        boolean isPullable = false;
        int callState = ImsExternalCallState.CALL_STATE_CONFIRMED;
        int callType = ImsCallProfile.CALL_TYPE_VOICE;
        boolean isHeld = false;
        ImsExternalCallState testState = new ImsExternalCallState(callId, address, localAddress,
                isPullable, callState, callType, isHeld);

        Parcel infoParceled = Parcel.obtain();
        testState.writeToParcel(infoParceled, 0);
        infoParceled.setDataPosition(0);
        ImsExternalCallState unparceledInfo =
                ImsExternalCallState.CREATOR.createFromParcel(infoParceled);
        infoParceled.recycle();

        // Test that the string is parsed to an int correctly
        assertEquals(1 /**callId*/, unparceledInfo.getCallId());
        assertEquals(address, unparceledInfo.getAddress());
        assertEquals(localAddress, unparceledInfo.getLocalAddress());
        assertEquals(isPullable, unparceledInfo.isCallPullable());
        assertEquals(callState, unparceledInfo.getCallState());
        assertEquals(callType, unparceledInfo.getCallType());
        assertEquals(isHeld, unparceledInfo.isCallHeld());
    }
}
