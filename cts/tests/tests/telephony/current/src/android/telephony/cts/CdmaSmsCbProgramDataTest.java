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

import static android.telephony.cdma.CdmaSmsCbProgramData.ALERT_OPTION_NO_ALERT;
import static android.telephony.cdma.CdmaSmsCbProgramData.CATEGORY_CMAS_PRESIDENTIAL_LEVEL_ALERT;
import static android.telephony.cdma.CdmaSmsCbProgramData.OPERATION_CLEAR_CATEGORIES;

import static org.junit.Assert.assertEquals;

import android.telephony.cdma.CdmaSmsCbProgramData;

import com.android.internal.telephony.cdma.sms.BearerData;

import org.junit.Test;

public class CdmaSmsCbProgramDataTest {

    private static final int OPERATION = OPERATION_CLEAR_CATEGORIES;
    private static final int CATEGORY = CATEGORY_CMAS_PRESIDENTIAL_LEVEL_ALERT;
    private static final int LANGUAGE = BearerData.LANGUAGE_ENGLISH;
    private static final int MAX_MESSAGES = 10;
    private static final int ALERT_OPTION = ALERT_OPTION_NO_ALERT;
    private static final String CATEGORY_NAME = "category_name";

    @Test
    public void testCdmaSmsCbProgramDataConstructorAndGetters() {
        CdmaSmsCbProgramData data = new CdmaSmsCbProgramData(
                OPERATION,
                CATEGORY,
                LANGUAGE,
                MAX_MESSAGES,
                ALERT_OPTION,
                CATEGORY_NAME
        );

        assertEquals(OPERATION, data.getOperation());
        assertEquals(CATEGORY, data.getCategory());
        assertEquals(LANGUAGE, data.getLanguage());
        assertEquals(MAX_MESSAGES, data.getMaxMessages());
        assertEquals(ALERT_OPTION, data.getAlertOption());
        assertEquals(CATEGORY_NAME, data.getCategoryName());
    }
}
