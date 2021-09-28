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


package android.telephony.cts;

import static junit.framework.Assert.fail;

import static org.junit.Assume.assumeTrue;

import android.content.pm.PackageManager;
import android.os.UserHandle;
import android.telephony.CbGeoUtils.Geometry;
import android.telephony.CellBroadcastIntents;
import android.telephony.SmsCbEtwsInfo;
import android.telephony.SmsCbLocation;
import android.telephony.SmsCbMessage;

import androidx.test.InstrumentationRegistry;

import com.android.internal.telephony.gsm.SmsCbConstants;

import org.junit.Before;
import org.junit.Test;

import java.util.ArrayList;
import java.util.List;

public class CellBroadcastIntentsTest {

    private static final int TEST_MESSAGE_FORMAT = SmsCbMessage.MESSAGE_FORMAT_3GPP2;
    private static final int TEST_GEO_SCOPE = SmsCbMessage.GEOGRAPHICAL_SCOPE_PLMN_WIDE;
    private static final int TEST_SERIAL = 1234;
    private static final String TEST_PLMN = "111222";
    private static final SmsCbLocation TEST_LOCATION = new SmsCbLocation(TEST_PLMN, -1, -1);
    private static final int TEST_SERVICE_CATEGORY = 4097;
    private static final String TEST_LANGUAGE = "en";
    private static final int TEST_DCS = 0;
    private static final String TEST_BODY = "test body";
    private static final int TEST_PRIORITY = 0;
    private static final int TEST_ETWS_WARNING_TYPE =
            SmsCbConstants.MESSAGE_ID_ETWS_OTHER_EMERGENCY_TYPE;
    private static final SmsCbEtwsInfo TEST_ETWS_INFO = new SmsCbEtwsInfo(TEST_ETWS_WARNING_TYPE,
            false, false, false, null);

    private static final int TEST_MAX_WAIT_TIME = 0;
    private static final List<Geometry> TEST_GEOS = new ArrayList<>();
    private static final int TEST_RECEIVED_TIME = 11000;
    private static final int TEST_SLOT = 0;
    private static final int TEST_SUB_ID = 1;

    @Before
    public void setUp() throws Exception {
        assumeTrue(InstrumentationRegistry.getContext().getPackageManager()
                .hasSystemFeature(PackageManager.FEATURE_TELEPHONY));
    }

    @Test
    public void testGetIntentForBackgroundReceivers() {
        try {
            SmsCbMessage message = new SmsCbMessage(TEST_MESSAGE_FORMAT, TEST_GEO_SCOPE,
                    TEST_SERIAL, TEST_LOCATION, TEST_SERVICE_CATEGORY, TEST_LANGUAGE, TEST_DCS,
                    TEST_BODY, TEST_PRIORITY, TEST_ETWS_INFO, null,
                    TEST_MAX_WAIT_TIME, TEST_GEOS, TEST_RECEIVED_TIME, TEST_SLOT, TEST_SUB_ID);

            CellBroadcastIntents.sendSmsCbReceivedBroadcast(
                    InstrumentationRegistry.getContext(), UserHandle.ALL, message,
                    null, null, 0, TEST_SLOT);
        } catch (SecurityException e) {
            // expected
            return;
        }
        fail();
    }
}
