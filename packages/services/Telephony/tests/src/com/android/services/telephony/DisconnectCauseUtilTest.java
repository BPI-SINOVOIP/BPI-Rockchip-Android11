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

package com.android.services.telephony;

import static android.media.ToneGenerator.TONE_PROP_PROMPT;

import static junit.framework.Assert.assertNotNull;
import static junit.framework.TestCase.assertEquals;

import android.telephony.DisconnectCause;

import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class DisconnectCauseUtilTest {
    /**
     * Verifies that a call drop due to loss of WIFI results in a disconnect cause of error and that
     * the label, description and tone are all present.
     */
    @Test
    public void testDropDueToWifiLoss() {
        android.telecom.DisconnectCause tcCause = DisconnectCauseUtil.toTelecomDisconnectCause(
                DisconnectCause.WIFI_LOST);
        assertEquals(android.telecom.DisconnectCause.ERROR, tcCause.getCode());
        assertEquals(TONE_PROP_PROMPT, tcCause.getTone());
        assertNotNull(tcCause.getDescription());
        assertNotNull(tcCause.getReason());
    }
}
