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

import static org.junit.Assert.fail;
import static org.junit.Assume.assumeTrue;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;

import androidx.test.InstrumentationRegistry;

import org.junit.Before;
import org.junit.Test;


public class TelephonyProtectedBroadcastsTest {
    private static final String[] BACKGROUND_BROADCASTS = new String[] {
            "android.intent.action.ACTION_DEFAULT_DATA_SUBSCRIPTION_CHANGED",
            "android.intent.action.ACTION_DEFAULT_VOICE_SUBSCRIPTION_CHANGED",
            "android.intent.action.DATA_SMS_RECEIVED",
            "android.provider.Telephony.SECRET_CODE",
            "android.provider.Telephony.SMS_CB_RECEIVED",
            "android.provider.Telephony.SMS_DELIVER",
            "android.provider.Telephony.SMS_RECEIVED",
            "android.provider.Telephony.SMS_REJECTED",
            "android.provider.Telephony.WAP_PUSH_DELIVER",
            "android.provider.Telephony.WAP_PUSH_RECEIVED",
            "android.telephony.action.CARRIER_CONFIG_CHANGED",
            "android.telephony.action.DEFAULT_SMS_SUBSCRIPTION_CHANGED",
            "android.telephony.action.DEFAULT_SUBSCRIPTION_CHANGED",
            "android.telephony.action.SECRET_CODE",
            "android.telephony.action.SIM_APPLICATION_STATE_CHANGED",
            "android.telephony.action.SIM_CARD_STATE_CHANGED",
            "android.telephony.action.SIM_SLOT_STATUS_CHANGED",
    };

    @Before
    public void setUp() throws Exception {
        assumeTrue(getContext().getPackageManager()
                .hasSystemFeature(PackageManager.FEATURE_TELEPHONY));
    }

    @Test
    public void testBroadcasts() {
        StringBuilder errorMessage = new StringBuilder(); //Fail on all missing broadcasts
        for (String action : BACKGROUND_BROADCASTS) {
            try {
                Intent intent = new Intent(action);
                getContext().sendBroadcast(intent);

                //Add error message because no security exception was thrown.
                if (errorMessage.length() == 0) {
                    errorMessage.append("--- Expected security exceptions when broadcasting on the "
                            + "following actions:");
                    errorMessage.append(System.lineSeparator());
                }
                errorMessage.append(action);
                errorMessage.append(System.lineSeparator());

            } catch (SecurityException expected) {
            }
        }

        if (errorMessage.length() > 0) {
            errorMessage.append("------------------------------------------------------------");
            errorMessage.append(System.lineSeparator());
            fail(errorMessage.toString());
        }
    }

    private static Context getContext() {
        return InstrumentationRegistry.getContext();
    }
}
