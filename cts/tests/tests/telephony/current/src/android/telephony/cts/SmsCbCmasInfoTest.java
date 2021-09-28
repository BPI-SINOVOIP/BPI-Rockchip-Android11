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

import static android.telephony.SmsCbCmasInfo.CMAS_CATEGORY_SAFETY;
import static android.telephony.SmsCbCmasInfo.CMAS_CERTAINTY_LIKELY;
import static android.telephony.SmsCbCmasInfo.CMAS_CLASS_PRESIDENTIAL_LEVEL_ALERT;
import static android.telephony.SmsCbCmasInfo.CMAS_RESPONSE_TYPE_EVACUATE;
import static android.telephony.SmsCbCmasInfo.CMAS_SEVERITY_SEVERE;
import static android.telephony.SmsCbCmasInfo.CMAS_URGENCY_IMMEDIATE;

import android.telephony.SmsCbCmasInfo;

import org.junit.Assert;
import org.junit.Test;


public class SmsCbCmasInfoTest {

    private static final int MESSAGE_CLASS = CMAS_CLASS_PRESIDENTIAL_LEVEL_ALERT;
    private static final int CATEGORY = CMAS_CATEGORY_SAFETY;
    private static final int RESPONSE_TYPE = CMAS_RESPONSE_TYPE_EVACUATE;
    private static final int SEVERITY = CMAS_SEVERITY_SEVERE;
    private static final int URGENCY = CMAS_URGENCY_IMMEDIATE;
    private static final int CERTAINTY = CMAS_CERTAINTY_LIKELY;

    @Test
    public void testSmsCbCmasInfo() {
        SmsCbCmasInfo info = new SmsCbCmasInfo(
                MESSAGE_CLASS,
                CATEGORY,
                RESPONSE_TYPE,
                SEVERITY,
                URGENCY,
                CERTAINTY
        );

        Assert.assertEquals(MESSAGE_CLASS, info.getMessageClass());
        Assert.assertEquals(CATEGORY, info.getCategory());
        Assert.assertEquals(RESPONSE_TYPE, info.getResponseType());
        Assert.assertEquals(SEVERITY, info.getSeverity());
        Assert.assertEquals(URGENCY, info.getUrgency());
        Assert.assertEquals(CERTAINTY, info.getCertainty());
    }
}
