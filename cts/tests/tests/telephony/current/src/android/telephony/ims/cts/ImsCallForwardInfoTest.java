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

import android.os.Parcel;
import android.telephony.ims.ImsCallForwardInfo;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class ImsCallForwardInfoTest {

    @Test
    public void createParcelUnparcel() {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }
        int condition = 1; // ImsUtInterface#CDIV_CF_BUSY
        int status = 1; //enabled
        int toA = 0x91; // International
        int serviceClass = 1; // CommandsInterface#SERVICE_CLASS_VOICE
        String number = "5555551212";
        int timeSeconds = 1; // no reply timer
        ImsCallForwardInfo info = new ImsCallForwardInfo(condition, status, toA, serviceClass,
                number, timeSeconds);

        Parcel infoParceled = Parcel.obtain();
        info.writeToParcel(infoParceled, 0);
        infoParceled.setDataPosition(0);
        ImsCallForwardInfo unparceledInfo =
                ImsCallForwardInfo.CREATOR.createFromParcel(infoParceled);
        infoParceled.recycle();

        assertEquals(condition, unparceledInfo.getCondition());
        assertEquals(status, unparceledInfo.getStatus());
        assertEquals(toA, unparceledInfo.getToA());
        assertEquals(serviceClass, unparceledInfo.getServiceClass());
        assertEquals(number, unparceledInfo.getNumber());
        assertEquals(timeSeconds, unparceledInfo.getTimeSeconds());
    }
}
