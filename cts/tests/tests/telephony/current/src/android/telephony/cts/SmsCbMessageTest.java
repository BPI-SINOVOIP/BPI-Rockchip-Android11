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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;

import android.content.ContentValues;
import android.database.Cursor;
import android.provider.Telephony;
import android.telephony.CbGeoUtils;
import android.telephony.SmsCbCmasInfo;
import android.telephony.SmsCbEtwsInfo;
import android.telephony.SmsCbLocation;
import android.telephony.SmsCbMessage;

import com.android.internal.telephony.gsm.SmsCbConstants;

import org.junit.Before;
import org.junit.Test;

import java.util.ArrayList;
import java.util.List;

public class SmsCbMessageTest {

    private SmsCbMessage mSmsCbMessage;

    private static final int TEST_MESSAGE_FORMAT = SmsCbMessage.MESSAGE_FORMAT_3GPP2;
    private static final int TEST_GEO_SCOPE = SmsCbMessage.GEOGRAPHICAL_SCOPE_PLMN_WIDE;
    private static final int TEST_SERIAL = 1234;
    private static final String TEST_PLMN = "111222";
    private static final SmsCbLocation TEST_LOCATION = new SmsCbLocation(TEST_PLMN, -1, -1);
    private static final int TEST_DCS = 0;
    private static final int TEST_SERVICE_CATEGORY = 4097;
    private static final String TEST_LANGUAGE = "en";
    private static final String TEST_BODY = "test body";
    private static final int TEST_PRIORITY = 0;
    private static final int TEST_ETWS_WARNING_TYPE =
            SmsCbConstants.MESSAGE_ID_ETWS_OTHER_EMERGENCY_TYPE;
    private static final SmsCbEtwsInfo TEST_ETWS_INFO = new SmsCbEtwsInfo(TEST_ETWS_WARNING_TYPE,
            false, false, false, null);
    private static final SmsCbCmasInfo TEST_CMAS_INFO =
            new SmsCbCmasInfo(SmsCbCmasInfo.CMAS_CLASS_REQUIRED_MONTHLY_TEST,
                    SmsCbCmasInfo.CMAS_CATEGORY_OTHER,
                    SmsCbCmasInfo.CMAS_RESPONSE_TYPE_EVACUATE,
                    SmsCbCmasInfo.CMAS_SEVERITY_UNKNOWN,
                    SmsCbCmasInfo.CMAS_URGENCY_EXPECTED, SmsCbCmasInfo.CMAS_CERTAINTY_LIKELY);

    private static final int TEST_MAX_WAIT_TIME = 0;
    private static final List<CbGeoUtils.Geometry> TEST_GEOS = new ArrayList<>();
    private static final long TEST_RECEIVED_TIME = 11000;
    private static final int TEST_SLOT = 0;
    private static final int TEST_SUB_ID = 1;

    @Before
    public void setUp() {
        TEST_GEOS.add(new CbGeoUtils.Geometry() {
            @Override
            public boolean contains(CbGeoUtils.LatLng p) {
                return false;
            }
        });
        mSmsCbMessage = new SmsCbMessage(TEST_MESSAGE_FORMAT, TEST_GEO_SCOPE, TEST_SERIAL,
                TEST_LOCATION, TEST_SERVICE_CATEGORY, TEST_LANGUAGE, TEST_DCS, TEST_BODY,
                TEST_PRIORITY, TEST_ETWS_INFO, null, TEST_MAX_WAIT_TIME, TEST_GEOS,
                TEST_RECEIVED_TIME, TEST_SLOT, TEST_SUB_ID);
    }

    @Test
    public void testSubId() {
        assertEquals(TEST_SUB_ID, mSmsCbMessage.getSubscriptionId());
    }


    @Test
    public void testGeographicalScope() {
        assertEquals(TEST_GEO_SCOPE, mSmsCbMessage.getGeographicalScope());
    }

    @Test
    public void testSerialNumber() {
        assertEquals(TEST_SERIAL, mSmsCbMessage.getSerialNumber());
    }

    @Test
    public void testLocation() {
        assertEquals(TEST_LOCATION, mSmsCbMessage.getLocation());
    }

    @Test
    public void testServiceCategory() {
        assertEquals(TEST_SERVICE_CATEGORY, mSmsCbMessage.getServiceCategory());
    }

    @Test
    public void testLanguage() {
        assertEquals(TEST_LANGUAGE, mSmsCbMessage.getLanguageCode());
    }

    @Test
    public void testBody() {
        assertEquals(TEST_BODY, mSmsCbMessage.getMessageBody());
    }

    @Test
    public void testGeometries() {
        assertEquals(TEST_GEOS, mSmsCbMessage.getGeometries());
    }

    @Test
    public void testMaxWaitTime() {
        assertEquals(TEST_MAX_WAIT_TIME, mSmsCbMessage.getMaximumWaitingDuration());
    }

    @Test
    public void testReceivedTime() {
        assertEquals(TEST_RECEIVED_TIME, mSmsCbMessage.getReceivedTime());
    }

    @Test
    public void testSlotIndex() {
        assertEquals(TEST_SLOT, mSmsCbMessage.getSlotIndex());
    }

    @Test
    public void testMessagePriority() {
        assertEquals(TEST_PRIORITY, mSmsCbMessage.getMessagePriority());
    }

    @Test
    public void testMessageFormat() {
        assertEquals(TEST_MESSAGE_FORMAT, mSmsCbMessage.getMessageFormat());
    }

    @Test
    public void testEtwsInfo() {
        assertEquals(TEST_ETWS_INFO, mSmsCbMessage.getEtwsWarningInfo());
    }

    @Test
    public void testDataCodingScheme() {
        assertEquals(TEST_DCS, mSmsCbMessage.getDataCodingScheme());
    }

    @Test
    public void testCmasInfo() {
        final SmsCbCmasInfo info =
                new SmsCbCmasInfo(SmsCbCmasInfo.CMAS_CLASS_REQUIRED_MONTHLY_TEST,
                        SmsCbCmasInfo.CMAS_CATEGORY_OTHER,
                        SmsCbCmasInfo.CMAS_RESPONSE_TYPE_EVACUATE,
                        SmsCbCmasInfo.CMAS_SEVERITY_UNKNOWN,
                        SmsCbCmasInfo.CMAS_URGENCY_EXPECTED, SmsCbCmasInfo.CMAS_CERTAINTY_LIKELY);

        mSmsCbMessage = new SmsCbMessage(TEST_MESSAGE_FORMAT, TEST_GEO_SCOPE, TEST_SERIAL,
                TEST_LOCATION, TEST_SERVICE_CATEGORY, TEST_LANGUAGE, 0, TEST_BODY, TEST_PRIORITY,
                null, info, TEST_MAX_WAIT_TIME, TEST_GEOS, TEST_RECEIVED_TIME,
                TEST_SLOT, TEST_SUB_ID);
        assertEquals(info, mSmsCbMessage.getCmasWarningInfo());
    }

    @Test
    public void testEmergency() {
        mSmsCbMessage = new SmsCbMessage(TEST_MESSAGE_FORMAT, TEST_GEO_SCOPE, TEST_SERIAL,
                TEST_LOCATION, TEST_SERVICE_CATEGORY, TEST_LANGUAGE, 0, TEST_BODY,
                SmsCbMessage.MESSAGE_PRIORITY_EMERGENCY,
                TEST_ETWS_INFO, null, TEST_MAX_WAIT_TIME, TEST_GEOS, TEST_RECEIVED_TIME,
                TEST_SLOT, TEST_SUB_ID);
        assertEquals(true, mSmsCbMessage.isEmergencyMessage());

        mSmsCbMessage = new SmsCbMessage(TEST_MESSAGE_FORMAT, TEST_GEO_SCOPE, TEST_SERIAL,
                TEST_LOCATION, TEST_SERVICE_CATEGORY, TEST_LANGUAGE, 0, TEST_BODY,
                SmsCbMessage.MESSAGE_PRIORITY_NORMAL,
                TEST_ETWS_INFO, null, TEST_MAX_WAIT_TIME, TEST_GEOS, TEST_RECEIVED_TIME,
                TEST_SLOT, TEST_SUB_ID);
        assertEquals(false, mSmsCbMessage.isEmergencyMessage());
    }

    @Test
    public void testIsEtws() {
        mSmsCbMessage = new SmsCbMessage(TEST_MESSAGE_FORMAT, TEST_GEO_SCOPE, TEST_SERIAL,
                TEST_LOCATION, TEST_SERVICE_CATEGORY, TEST_LANGUAGE, 0, TEST_BODY,
                SmsCbMessage.MESSAGE_PRIORITY_EMERGENCY,
                null, TEST_CMAS_INFO, TEST_MAX_WAIT_TIME, TEST_GEOS, TEST_RECEIVED_TIME,
                TEST_SLOT, TEST_SUB_ID);
        assertEquals(false, mSmsCbMessage.isEtwsMessage());

        mSmsCbMessage = new SmsCbMessage(TEST_MESSAGE_FORMAT, TEST_GEO_SCOPE, TEST_SERIAL,
                TEST_LOCATION, TEST_SERVICE_CATEGORY, TEST_LANGUAGE, 0, TEST_BODY,
                SmsCbMessage.MESSAGE_PRIORITY_EMERGENCY,
                TEST_ETWS_INFO, null, TEST_MAX_WAIT_TIME, TEST_GEOS, TEST_RECEIVED_TIME,
                TEST_SLOT, TEST_SUB_ID);
        assertEquals(true, mSmsCbMessage.isEtwsMessage());
    }

    @Test
    public void testIsCmas() {
        mSmsCbMessage = new SmsCbMessage(TEST_MESSAGE_FORMAT, TEST_GEO_SCOPE, TEST_SERIAL,
                TEST_LOCATION, TEST_SERVICE_CATEGORY, TEST_LANGUAGE, 0, TEST_BODY,
                SmsCbMessage.MESSAGE_PRIORITY_EMERGENCY,
                TEST_ETWS_INFO, null, TEST_MAX_WAIT_TIME, TEST_GEOS, TEST_RECEIVED_TIME,
                TEST_SLOT, TEST_SUB_ID);
        assertEquals(false, mSmsCbMessage.isCmasMessage());

        mSmsCbMessage = new SmsCbMessage(TEST_MESSAGE_FORMAT, TEST_GEO_SCOPE, TEST_SERIAL,
                TEST_LOCATION, TEST_SERVICE_CATEGORY, TEST_LANGUAGE, 0, TEST_BODY,
                SmsCbMessage.MESSAGE_PRIORITY_EMERGENCY,
                null, TEST_CMAS_INFO, TEST_MAX_WAIT_TIME, TEST_GEOS, TEST_RECEIVED_TIME,
                TEST_SLOT, TEST_SUB_ID);
        assertEquals(true, mSmsCbMessage.isCmasMessage());
    }

    @Test
    public void testNeedGeoFencingCheck() {
        assertEquals(false, mSmsCbMessage.needGeoFencingCheck());

        mSmsCbMessage = new SmsCbMessage(TEST_MESSAGE_FORMAT, TEST_GEO_SCOPE, TEST_SERIAL,
                TEST_LOCATION, TEST_SERVICE_CATEGORY, TEST_LANGUAGE, 0, TEST_BODY,
                SmsCbMessage.MESSAGE_PRIORITY_EMERGENCY,
                null, TEST_CMAS_INFO, 100, TEST_GEOS, TEST_RECEIVED_TIME,
                TEST_SLOT, TEST_SUB_ID);
        assertEquals(true, mSmsCbMessage.needGeoFencingCheck());
    }

    @Test
    public void testContentValues() {
        ContentValues cv = mSmsCbMessage.getContentValues();
        int serial = cv.getAsInteger(Telephony.CellBroadcasts.SERIAL_NUMBER);
        assertEquals(TEST_SERIAL, serial);
    }

    @Test
    public void testCreateFromCursor() {
        Cursor cursor = mock(Cursor.class);
        doReturn(0).when(cursor).getColumnIndexOrThrow(eq(
                Telephony.CellBroadcasts.GEOGRAPHICAL_SCOPE));
        doReturn(TEST_GEO_SCOPE).when(cursor).getInt(0);

        doReturn(1).when(cursor).getColumnIndexOrThrow(eq(
                Telephony.CellBroadcasts.SERIAL_NUMBER));
        doReturn(TEST_SERIAL).when(cursor).getInt(1);

        doReturn(2).when(cursor).getColumnIndexOrThrow(eq(
                Telephony.CellBroadcasts.SERVICE_CATEGORY));
        doReturn(TEST_SERVICE_CATEGORY).when(cursor).getInt(2);

        doReturn(3).when(cursor).getColumnIndexOrThrow(eq(
                Telephony.CellBroadcasts.LANGUAGE_CODE));
        doReturn(TEST_LANGUAGE).when(cursor).getString(3);

        doReturn(4).when(cursor).getColumnIndexOrThrow(eq(
                Telephony.CellBroadcasts.MESSAGE_BODY));
        doReturn(TEST_BODY).when(cursor).getString(4);

        doReturn(5).when(cursor).getColumnIndexOrThrow(eq(
                Telephony.CellBroadcasts.MESSAGE_FORMAT));
        doReturn(TEST_MESSAGE_FORMAT).when(cursor).getInt(5);

        doReturn(6).when(cursor).getColumnIndexOrThrow(eq(
                Telephony.CellBroadcasts.MESSAGE_PRIORITY));
        doReturn(TEST_PRIORITY).when(cursor).getInt(6);

        doReturn(7).when(cursor).getColumnIndexOrThrow(eq(
                Telephony.CellBroadcasts.SLOT_INDEX));
        doReturn(TEST_SLOT).when(cursor).getInt(7);

        doReturn(8).when(cursor).getColumnIndexOrThrow(eq(
                Telephony.CellBroadcasts.SUBSCRIPTION_ID));
        doReturn(TEST_SUB_ID).when(cursor).getInt(8);

        doReturn(-1).when(cursor).getColumnIndexOrThrow(eq(
                Telephony.CellBroadcasts.PLMN));

        doReturn(-1).when(cursor).getColumnIndexOrThrow(eq(
                Telephony.CellBroadcasts.LAC));

        doReturn(-1).when(cursor).getColumnIndexOrThrow(eq(
                Telephony.CellBroadcasts.CID));

        doReturn(-1).when(cursor).getColumnIndex(eq(
                Telephony.CellBroadcasts.ETWS_WARNING_TYPE));

        doReturn(0).when(cursor).getColumnIndex(eq(
                Telephony.CellBroadcasts.ETWS_IS_PRIMARY));

        doReturn(-1).when(cursor).getColumnIndex(eq(
                Telephony.CellBroadcasts.CMAS_MESSAGE_CLASS));

        doReturn(9).when(cursor).getColumnIndex(eq(
                Telephony.CellBroadcasts.GEOMETRIES));
        // return empty string here to be parsed into empty array list
        doReturn("").when(cursor).getString(9);

        doReturn(10).when(cursor).getColumnIndexOrThrow(eq(
                Telephony.CellBroadcasts.RECEIVED_TIME));
        doReturn(TEST_RECEIVED_TIME).when(cursor).getLong(10);

        doReturn(11).when(cursor).getColumnIndexOrThrow(eq(
                Telephony.CellBroadcasts.MAXIMUM_WAIT_TIME));
        doReturn(TEST_MAX_WAIT_TIME).when(cursor).getInt(11);

        SmsCbMessage cbMessage = SmsCbMessage.createFromCursor(cursor);
        assertEquals(TEST_SERIAL, cbMessage.getSerialNumber());
        assertEquals(TEST_BODY, cbMessage.getMessageBody());
        assertNull(cbMessage.getEtwsWarningInfo());
        assertNull(cbMessage.getCmasWarningInfo());
    }
}
