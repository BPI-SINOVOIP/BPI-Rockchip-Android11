/*
 * Copyright (C) 2016 The Android Open Source Project
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
 * limitations under the License
 */

package com.android.cellbroadcastreceiver.unit;

import static com.android.internal.telephony.cdma.sms.SmsEnvelope.SERVICE_CATEGORY_CMAS_CHILD_ABDUCTION_EMERGENCY;
import static com.android.internal.telephony.cdma.sms.SmsEnvelope.SERVICE_CATEGORY_CMAS_EXTREME_THREAT;
import static com.android.internal.telephony.cdma.sms.SmsEnvelope.SERVICE_CATEGORY_CMAS_PRESIDENTIAL_LEVEL_ALERT;
import static com.android.internal.telephony.cdma.sms.SmsEnvelope.SERVICE_CATEGORY_CMAS_SEVERE_THREAT;
import static com.android.internal.telephony.gsm.SmsCbConstants.MESSAGE_ID_CMAS_ALERT_CHILD_ABDUCTION_EMERGENCY;
import static com.android.internal.telephony.gsm.SmsCbConstants.MESSAGE_ID_CMAS_ALERT_CHILD_ABDUCTION_EMERGENCY_LANGUAGE;
import static com.android.internal.telephony.gsm.SmsCbConstants.MESSAGE_ID_CMAS_ALERT_EXTREME_EXPECTED_OBSERVED;
import static com.android.internal.telephony.gsm.SmsCbConstants.MESSAGE_ID_CMAS_ALERT_EXTREME_EXPECTED_OBSERVED_LANGUAGE;
import static com.android.internal.telephony.gsm.SmsCbConstants.MESSAGE_ID_CMAS_ALERT_EXTREME_IMMEDIATE_LIKELY;
import static com.android.internal.telephony.gsm.SmsCbConstants.MESSAGE_ID_CMAS_ALERT_EXTREME_IMMEDIATE_LIKELY_LANGUAGE;
import static com.android.internal.telephony.gsm.SmsCbConstants.MESSAGE_ID_CMAS_ALERT_EXTREME_IMMEDIATE_OBSERVED;
import static com.android.internal.telephony.gsm.SmsCbConstants.MESSAGE_ID_CMAS_ALERT_EXTREME_IMMEDIATE_OBSERVED_LANGUAGE;
import static com.android.internal.telephony.gsm.SmsCbConstants.MESSAGE_ID_CMAS_ALERT_PRESIDENTIAL_LEVEL;
import static com.android.internal.telephony.gsm.SmsCbConstants.MESSAGE_ID_CMAS_ALERT_PRESIDENTIAL_LEVEL_LANGUAGE;
import static com.android.internal.telephony.gsm.SmsCbConstants.MESSAGE_ID_CMAS_ALERT_SEVERE_EXPECTED_LIKELY;
import static com.android.internal.telephony.gsm.SmsCbConstants.MESSAGE_ID_CMAS_ALERT_SEVERE_EXPECTED_LIKELY_LANGUAGE;
import static com.android.internal.telephony.gsm.SmsCbConstants.MESSAGE_ID_ETWS_EARTHQUAKE_AND_TSUNAMI_WARNING;
import static com.android.internal.telephony.gsm.SmsCbConstants.MESSAGE_ID_ETWS_EARTHQUAKE_WARNING;
import static com.android.internal.telephony.gsm.SmsCbConstants.MESSAGE_ID_ETWS_OTHER_EMERGENCY_TYPE;

import static org.mockito.Matchers.anyInt;
import static org.mockito.Matchers.anyString;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Intent;
import android.os.IBinder;
import android.telephony.SmsCbMessage;

import com.android.cellbroadcastreceiver.CellBroadcastConfigService;
import com.android.internal.telephony.ISms;

import org.junit.After;
import org.junit.Before;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;

public class CellBroadcastBootupConfigTest extends
        CellBroadcastServiceTestCase<CellBroadcastConfigService> {

    @Mock
    ISms.Stub mSmsService;
    @Mock
    IBinder mIBinder;

    @Captor
    private ArgumentCaptor<Integer> mStartIds;

    @Captor
    private ArgumentCaptor<Integer> mEndIds;

    @Captor
    private ArgumentCaptor<Integer> mTypes;

    public CellBroadcastBootupConfigTest() {
        super(CellBroadcastConfigService.class);
    }

    @Before
    public void setUp() throws Exception {
        super.setUp();

        doReturn(mSmsService).when(mSmsService).queryLocalInterface(anyString());
        doReturn(mIBinder).when(mSmsService).asBinder();
        mMockedServiceManager.replaceService("isms", mSmsService);
        putResources(com.android.cellbroadcastreceiver.R.array
                .cmas_presidential_alerts_channels_range_strings, new String[]{
                "0x1112-0x1112:rat=gsm",
                "0x1000-0x1000:rat=cdma",
                "0x111F-0x111F:rat=gsm",
        });
        putResources(com.android.cellbroadcastreceiver.R.array
                .cmas_alert_extreme_channels_range_strings, new String[]{
                "0x1113-0x1114:rat=gsm",
                "0x1001-0x1001:rat=cdma",
                "0x1120-0x1121:rat=gsm",
        });
        putResources(com.android.cellbroadcastreceiver.R.array
                .cmas_alerts_severe_range_strings, new String[]{
                "0x1115-0x111A:rat=gsm",
                "0x1002-0x1002:rat=cdma",
                "0x1122-0x1127:rat=gsm",
        });
        putResources(com.android.cellbroadcastreceiver.R.array
                .cmas_amber_alerts_channels_range_strings, new String[]{
                "0x111B-0x111B:rat=gsm",
                "0x1003-0x1003:rat=cdma",
                "0x1128-0x1128:rat=gsm",
        });
        putResources(com.android.cellbroadcastreceiver.R.array
                .etws_alerts_range_strings, new String[]{
                "0x1100-0x1102:rat=gsm",
                "0x1104-0x1104:rat=gsm",
        });
    }

    @After
    public void tearDown() throws Exception {
        super.tearDown();
    }

    private static class CbConfig {
        final int startId;
        final int endId;
        final int type;

        CbConfig(int startId, int endId, int type) {
            this.startId = startId;
            this.endId = endId;
            this.type = type;
        }
    }

    // Test if CellbroadcastConfigService properly configure all the required channels.
    public void testConfiguration() throws Exception {

        Intent intent = new Intent(mContext, CellBroadcastConfigService.class);
        intent.setAction(CellBroadcastConfigService.ACTION_ENABLE_CHANNELS);

        startService(intent);
        waitForMs(200);

        CbConfig[] configs = new CbConfig[] {
                new CbConfig(MESSAGE_ID_CMAS_ALERT_PRESIDENTIAL_LEVEL,
                        MESSAGE_ID_CMAS_ALERT_PRESIDENTIAL_LEVEL,
                        SmsCbMessage.MESSAGE_FORMAT_3GPP),
                new CbConfig(SERVICE_CATEGORY_CMAS_PRESIDENTIAL_LEVEL_ALERT,
                        SERVICE_CATEGORY_CMAS_PRESIDENTIAL_LEVEL_ALERT,
                        SmsCbMessage.MESSAGE_FORMAT_3GPP2),
                new CbConfig(MESSAGE_ID_CMAS_ALERT_PRESIDENTIAL_LEVEL_LANGUAGE,
                        MESSAGE_ID_CMAS_ALERT_PRESIDENTIAL_LEVEL_LANGUAGE,
                        SmsCbMessage.MESSAGE_FORMAT_3GPP),
                new CbConfig(MESSAGE_ID_CMAS_ALERT_EXTREME_IMMEDIATE_OBSERVED,
                        MESSAGE_ID_CMAS_ALERT_EXTREME_IMMEDIATE_LIKELY,
                        SmsCbMessage.MESSAGE_FORMAT_3GPP),
                new CbConfig(SERVICE_CATEGORY_CMAS_EXTREME_THREAT,
                        SERVICE_CATEGORY_CMAS_EXTREME_THREAT,
                        SmsCbMessage.MESSAGE_FORMAT_3GPP2),
                new CbConfig(MESSAGE_ID_CMAS_ALERT_EXTREME_IMMEDIATE_OBSERVED_LANGUAGE,
                        MESSAGE_ID_CMAS_ALERT_EXTREME_IMMEDIATE_LIKELY_LANGUAGE,
                        SmsCbMessage.MESSAGE_FORMAT_3GPP),
                new CbConfig(MESSAGE_ID_CMAS_ALERT_EXTREME_EXPECTED_OBSERVED,
                        MESSAGE_ID_CMAS_ALERT_SEVERE_EXPECTED_LIKELY,
                        SmsCbMessage.MESSAGE_FORMAT_3GPP),
                new CbConfig(SERVICE_CATEGORY_CMAS_SEVERE_THREAT,
                        SERVICE_CATEGORY_CMAS_SEVERE_THREAT,
                        SmsCbMessage.MESSAGE_FORMAT_3GPP2),
                new CbConfig(MESSAGE_ID_CMAS_ALERT_EXTREME_EXPECTED_OBSERVED_LANGUAGE,
                        MESSAGE_ID_CMAS_ALERT_SEVERE_EXPECTED_LIKELY_LANGUAGE,
                        SmsCbMessage.MESSAGE_FORMAT_3GPP),
                new CbConfig(MESSAGE_ID_CMAS_ALERT_CHILD_ABDUCTION_EMERGENCY,
                        MESSAGE_ID_CMAS_ALERT_CHILD_ABDUCTION_EMERGENCY,
                        SmsCbMessage.MESSAGE_FORMAT_3GPP),
                new CbConfig(SERVICE_CATEGORY_CMAS_CHILD_ABDUCTION_EMERGENCY,
                        SERVICE_CATEGORY_CMAS_CHILD_ABDUCTION_EMERGENCY,
                        SmsCbMessage.MESSAGE_FORMAT_3GPP2),
                new CbConfig(MESSAGE_ID_CMAS_ALERT_CHILD_ABDUCTION_EMERGENCY_LANGUAGE,
                        MESSAGE_ID_CMAS_ALERT_CHILD_ABDUCTION_EMERGENCY_LANGUAGE,
                        SmsCbMessage.MESSAGE_FORMAT_3GPP),
                new CbConfig(MESSAGE_ID_ETWS_EARTHQUAKE_WARNING,
                        MESSAGE_ID_ETWS_EARTHQUAKE_AND_TSUNAMI_WARNING,
                        SmsCbMessage.MESSAGE_FORMAT_3GPP),
                new CbConfig(MESSAGE_ID_ETWS_OTHER_EMERGENCY_TYPE,
                        MESSAGE_ID_ETWS_OTHER_EMERGENCY_TYPE,
                        SmsCbMessage.MESSAGE_FORMAT_3GPP),
        };

        verify(mSmsService, times(configs.length)).enableCellBroadcastRangeForSubscriber(anyInt(),
                mStartIds.capture(), mEndIds.capture(), mTypes.capture());

        for (int i = 0; i < configs.length; i++) {
            assertEquals("i=" + i, configs[i].startId, mStartIds.getAllValues().get(i).intValue());
            assertEquals("i=" + i, configs[i].endId, mEndIds.getAllValues().get(i).intValue());
            assertEquals("i=" + i, configs[i].type, mTypes.getAllValues().get(i).intValue());
        }
     }
}
