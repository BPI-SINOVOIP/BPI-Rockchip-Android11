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

package com.android.cellbroadcastreceiver.unit;

import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doReturn;

import android.content.Context;
import android.content.res.Resources;
import android.telephony.SmsCbCmasInfo;
import android.telephony.SmsCbLocation;
import android.telephony.SmsCbMessage;

import com.android.cellbroadcastreceiver.CellBroadcastResources;
import com.android.internal.telephony.gsm.SmsCbConstants;

import org.junit.Before;
import org.junit.Test;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

public class CellBroadcastResourcesTest {

    @Mock
    private Context mContext;

    @Mock
    private Resources mResources;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        doReturn(mResources).when(mContext).getResources();
        String stringResultToReturn = "";
        doReturn(stringResultToReturn).when(mResources).getString(anyInt());
    }

    @Test
    public void testGetMessageDetails() {
        SmsCbMessage smsCbMessage = new SmsCbMessage(1, 2, 0, new SmsCbLocation(),
                SmsCbConstants.MESSAGE_ID_ETWS_EARTHQUAKE_AND_TSUNAMI_WARNING, "language", "body",
                SmsCbMessage.MESSAGE_PRIORITY_EMERGENCY, null, new SmsCbCmasInfo(0, 2, 3, 4, 5, 6),
                0, 1);
        CharSequence details = CellBroadcastResources.getMessageDetails(mContext, true,
                smsCbMessage, -1, false,
                null);
        assertNotNull(details);
    }

    @Test
    public void testGetMessageDetailsCmasMessage() {
        SmsCbMessage smsCbMessage = new SmsCbMessage(1, 2, 0, new SmsCbLocation(),
                SmsCbConstants.MESSAGE_ID_CMAS_ALERT_PRESIDENTIAL_LEVEL, "language", "body",
                SmsCbMessage.MESSAGE_PRIORITY_EMERGENCY, null, new SmsCbCmasInfo(0, 2, 3, 4, 5, 6),
                0, 1);
        CharSequence details = CellBroadcastResources.getMessageDetails(mContext, true,
                smsCbMessage, -1, false,
                null);
        assertNotNull(details);
    }
}
