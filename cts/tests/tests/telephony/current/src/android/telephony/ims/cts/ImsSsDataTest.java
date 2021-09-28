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

import static junit.framework.TestCase.assertEquals;
import static junit.framework.TestCase.assertFalse;

import static org.junit.Assert.assertTrue;

import android.os.Parcel;
import android.telephony.ims.ImsCallForwardInfo;
import android.telephony.ims.ImsReasonInfo;
import android.telephony.ims.ImsSsData;
import android.telephony.ims.ImsSsInfo;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.List;

@RunWith(AndroidJUnit4.class)
public class ImsSsDataTest {

    @Test
    public void testParcelUnparcel() {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }
        int serviceType = ImsSsData.SS_CLIP;
        int requestType = ImsSsData.SS_DEACTIVATION;
        int teleserviceType = ImsSsData.SS_ALL_TELESEVICES;
        int serviceClass = 1;
        int result = 1;

        ImsSsData data = new ImsSsData(serviceType, requestType, teleserviceType, serviceClass,
                result);

        Parcel parcel = Parcel.obtain();
        data.writeToParcel(parcel, 0);
        parcel.setDataPosition(0);
        ImsSsData unparceledData = ImsSsData.CREATOR.createFromParcel(parcel);
        parcel.recycle();

        assertTrue(unparceledData.isTypeClip());
    }

    @Test
    public void testServiceTypeCF() {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }
        int serviceType = ImsSsData.SS_CFU;
        ImsSsData data = new ImsSsData(serviceType, ImsSsData.SS_DEACTIVATION,
                ImsSsData.SS_ALL_TELESEVICES, 0, 0);

        ImsSsData unparceledData = (ImsSsData) parcelUnparcel(data);

        assertTrue(unparceledData.isTypeCf());
        assertTrue(unparceledData.isTypeUnConditional());
    }

    @Test
    public void testServiceTypeCW() {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }
        int serviceType = ImsSsData.SS_WAIT;
        ImsSsData data = new ImsSsData(serviceType, ImsSsData.SS_DEACTIVATION,
                ImsSsData.SS_ALL_TELESEVICES, 0, 0);

        ImsSsData unparceledData = (ImsSsData) parcelUnparcel(data);

        assertTrue(unparceledData.isTypeCw());
    }

    @Test
    public void testServiceTypeColr() {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }
        int serviceType = ImsSsData.SS_COLR;
        ImsSsData data = new ImsSsData(serviceType, ImsSsData.SS_DEACTIVATION,
                ImsSsData.SS_ALL_TELESEVICES, 0, 0);

        ImsSsData unparceledData = (ImsSsData) parcelUnparcel(data);

        assertTrue(unparceledData.isTypeColr());
    }

    @Test
    public void testServiceTypeColp() {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }
        int serviceType = ImsSsData.SS_COLP;
        ImsSsData data = new ImsSsData(serviceType, ImsSsData.SS_DEACTIVATION,
                ImsSsData.SS_ALL_TELESEVICES, 0, 0);

        ImsSsData unparceledData = (ImsSsData) parcelUnparcel(data);

        assertTrue(unparceledData.isTypeColp());
    }

    @Test
    public void testServiceTypeClir() {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }
        int serviceType = ImsSsData.SS_CLIR;
        ImsSsData data = new ImsSsData(serviceType, ImsSsData.SS_DEACTIVATION,
                ImsSsData.SS_ALL_TELESEVICES, 0, 0);

        ImsSsData unparceledData = (ImsSsData) parcelUnparcel(data);

        assertTrue(unparceledData.isTypeClir());
    }

    @Test
    public void testServiceTypeIcb() {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }
        int serviceType = ImsSsData.SS_INCOMING_BARRING_DN;
        ImsSsData data = new ImsSsData(serviceType, ImsSsData.SS_DEACTIVATION,
                ImsSsData.SS_ALL_TELESEVICES, 0, 0);

        ImsSsData unparceledData = (ImsSsData) parcelUnparcel(data);

        assertTrue(unparceledData.isTypeIcb());
    }

    @Test
    public void testServiceTypeIcbAnon() {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }
        int serviceType = ImsSsData.SS_INCOMING_BARRING_ANONYMOUS;
        ImsSsData data = new ImsSsData(serviceType, ImsSsData.SS_DEACTIVATION,
                ImsSsData.SS_ALL_TELESEVICES, 0, 0);

        ImsSsData unparceledData = (ImsSsData) parcelUnparcel(data);

        assertTrue(unparceledData.isTypeIcb());
    }

    @Test
    public void testServiceTypeBarring() {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }
        int serviceType = ImsSsData.SS_BAOC;
        ImsSsData data = new ImsSsData(serviceType, ImsSsData.SS_DEACTIVATION,
                ImsSsData.SS_ALL_TELESEVICES, 0, 0);
        ImsSsData unparceledData = (ImsSsData) parcelUnparcel(data);
        assertTrue(unparceledData.isTypeBarring());

        serviceType = ImsSsData.SS_BAOIC;
        data = new ImsSsData(serviceType, ImsSsData.SS_DEACTIVATION,
                ImsSsData.SS_ALL_TELESEVICES, 0, 0);
        unparceledData = (ImsSsData) parcelUnparcel(data);
        assertTrue(unparceledData.isTypeBarring());

        serviceType = ImsSsData.SS_BAOIC_EXC_HOME;
        data = new ImsSsData(serviceType, ImsSsData.SS_DEACTIVATION,
                ImsSsData.SS_ALL_TELESEVICES, 0, 0);
        unparceledData = (ImsSsData) parcelUnparcel(data);
        assertTrue(unparceledData.isTypeBarring());

        serviceType = ImsSsData.SS_BAIC;
        data = new ImsSsData(serviceType, ImsSsData.SS_DEACTIVATION,
                ImsSsData.SS_ALL_TELESEVICES, 0, 0);
        unparceledData = (ImsSsData) parcelUnparcel(data);
        assertTrue(unparceledData.isTypeBarring());

        serviceType = ImsSsData.SS_BAIC_ROAMING;
        data = new ImsSsData(serviceType, ImsSsData.SS_DEACTIVATION,
                ImsSsData.SS_ALL_TELESEVICES, 0, 0);
        unparceledData = (ImsSsData) parcelUnparcel(data);
        assertTrue(unparceledData.isTypeBarring());

        serviceType = ImsSsData.SS_ALL_BARRING;
        data = new ImsSsData(serviceType, ImsSsData.SS_DEACTIVATION,
                ImsSsData.SS_ALL_TELESEVICES, 0, 0);
        unparceledData = (ImsSsData) parcelUnparcel(data);
        assertTrue(unparceledData.isTypeBarring());

        serviceType = ImsSsData.SS_OUTGOING_BARRING;
        data = new ImsSsData(serviceType, ImsSsData.SS_DEACTIVATION,
                ImsSsData.SS_ALL_TELESEVICES, 0, 0);
        unparceledData = (ImsSsData) parcelUnparcel(data);
        assertTrue(unparceledData.isTypeBarring());

        serviceType = ImsSsData.SS_INCOMING_BARRING;
        data = new ImsSsData(serviceType, ImsSsData.SS_DEACTIVATION,
                ImsSsData.SS_ALL_TELESEVICES, 0, 0);
        unparceledData = (ImsSsData) parcelUnparcel(data);
        assertTrue(unparceledData.isTypeBarring());
    }

    @Test
    public void testRequestTypeInterrogation() {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }
        ImsSsData data = new ImsSsData(ImsSsData.SS_CFU, ImsSsData.SS_INTERROGATION,
                ImsSsData.SS_ALL_TELESEVICES, 0, 0);

        ImsSsData unparceledData = (ImsSsData) parcelUnparcel(data);
        assertTrue(unparceledData.isTypeInterrogation());
    }

    @Test
    public void testConstructor() {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }

        ImsSsData data = new ImsSsData(ImsSsData.SS_CFU, ImsSsData.SS_INTERROGATION,
                ImsSsData.SS_ALL_TELESEVICES, ImsSsData.SERVICE_CLASS_VOICE,
                ImsReasonInfo.CODE_LOCAL_IMS_SERVICE_DOWN);

        ImsSsData unparceledData = (ImsSsData) parcelUnparcel(data);
        assertEquals(ImsSsData.SS_ALL_TELESEVICES, unparceledData.getTeleserviceType());
        assertEquals(ImsReasonInfo.CODE_LOCAL_IMS_SERVICE_DOWN, unparceledData.getResult());
        assertEquals(ImsSsData.SERVICE_CLASS_VOICE, unparceledData.getServiceClass());
        assertEquals(ImsSsData.SS_INTERROGATION, unparceledData.getRequestType());
        assertEquals(ImsSsData.SS_CFU, unparceledData.getServiceType());

    }


    @Test
    public void testSetCallForwardingInfo() {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }

        ImsCallForwardInfo info = new ImsCallForwardInfo(ImsCallForwardInfo.CDIV_CF_REASON_ALL,
                ImsCallForwardInfo.STATUS_ACTIVE, ImsCallForwardInfo.TYPE_OF_ADDRESS_UNKNOWN,
                ImsSsData.SERVICE_CLASS_NONE, "5551212", 0);
        List<ImsCallForwardInfo> infos = new ArrayList<>();
        infos.add(info);

        ImsSsData.Builder dataBuilder = new ImsSsData.Builder(ImsSsData.SS_CFU,
                ImsSsData.SS_INTERROGATION, ImsSsData.SS_ALL_TELESEVICES, 0, 60);
        dataBuilder.setCallForwardingInfo(infos);

        ImsSsData unparceledData = (ImsSsData) parcelUnparcel(dataBuilder.build());

        assertFalse(unparceledData.getCallForwardInfo().isEmpty());
        ImsCallForwardInfo testInfo = unparceledData.getCallForwardInfo().get(0);
        assertEquals(info.getCondition(), testInfo.getCondition());
        assertEquals(info.getStatus(), testInfo.getStatus());
        assertEquals(info.getToA(), testInfo.getToA());
        assertEquals(info.getServiceClass(), testInfo.getServiceClass());
        assertEquals(info.getNumber(), testInfo.getNumber());
        assertEquals(info.getTimeSeconds(), testInfo.getTimeSeconds());
    }

    @Test
    public void testSetSuppServiceInfo() {
        if (!ImsUtils.shouldTestImsService()) {
            return;
        }

        ImsSsInfo info = new ImsSsInfo.Builder(ImsSsInfo.ENABLED)
                .setClirInterrogationStatus(ImsSsInfo.CLIR_STATUS_PROVISIONED_PERMANENT)
                .setClirOutgoingState(ImsSsInfo.CLIR_OUTGOING_SUPPRESSION)
                .setIncomingCommunicationBarringNumber("+16505551212")
                .setProvisionStatus(ImsSsInfo.SERVICE_PROVISIONED).build();
        List<ImsSsInfo> infos = new ArrayList<>();
        infos.add(info);

        ImsSsData.Builder dataBuilder = new ImsSsData.Builder(ImsSsData.SS_CLIR,
                ImsSsData.SS_INTERROGATION, ImsSsData.SS_ALL_TELESEVICES, 0, 60);
        dataBuilder.setSuppServiceInfo(infos);

        ImsSsData unparceledData = (ImsSsData) parcelUnparcel(dataBuilder.build());

        assertFalse(unparceledData.getSuppServiceInfo().isEmpty());
        ImsSsInfo testInfo = unparceledData.getSuppServiceInfo().get(0);
        assertEquals(info.getIncomingCommunicationBarringNumber(),
                testInfo.getIncomingCommunicationBarringNumber());
        assertEquals(info.getProvisionStatus(), testInfo.getProvisionStatus());
        assertEquals(info.getClirInterrogationStatus(), testInfo.getClirInterrogationStatus());
        assertEquals(info.getClirOutgoingState(), testInfo.getClirOutgoingState());
    }

    /**
     * Passing in Object here instead of ImsSsData because this may be run on devices where
     * ImsSsData does not exist.
     */
    private Object parcelUnparcel(Object dataObj) {
        ImsSsData data = (ImsSsData) dataObj;
        Parcel parcel = Parcel.obtain();
        data.writeToParcel(parcel, 0);
        parcel.setDataPosition(0);
        ImsSsData unparceledData = ImsSsData.CREATOR.createFromParcel(parcel);
        parcel.recycle();
        return unparceledData;
    }
}
