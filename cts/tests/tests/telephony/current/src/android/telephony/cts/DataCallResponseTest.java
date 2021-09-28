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

import static com.google.common.truth.Truth.assertThat;

import android.net.InetAddresses;
import android.net.LinkAddress;
import android.os.Parcel;
import android.telephony.data.ApnSetting;
import android.telephony.data.DataCallResponse;

import org.junit.Test;

import java.net.InetAddress;
import java.util.Arrays;
import java.util.List;

public class DataCallResponseTest {
    private static final int CAUSE = 0;
    private static final int RETRY = -1;
    private static final int ID = 1;
    private static final int LINK_STATUS = 2;
    private static final int PROTOCOL_TYPE = ApnSetting.PROTOCOL_IP;
    private static final String IF_NAME = "IF_NAME";
    private static final List<LinkAddress> ADDRESSES = Arrays.asList(
            new LinkAddress(InetAddresses.parseNumericAddress("99.88.77.66"), 0));
    private static final List<InetAddress> DNSES =
            Arrays.asList(InetAddresses.parseNumericAddress("55.66.77.88"));
    private static final List<InetAddress> GATEWAYS =
            Arrays.asList(InetAddresses.parseNumericAddress("11.22.33.44"));
    private static final List<InetAddress> PCSCFS =
            Arrays.asList(InetAddresses.parseNumericAddress("22.33.44.55"));
    private static final int MTU_V4 = 1440;
    private static final int MTU_V6 = 1400;

    @Test
    public void testConstructorAndGetters() {
        DataCallResponse response = new DataCallResponse.Builder()
                .setCause(CAUSE)
                .setSuggestedRetryTime(RETRY)
                .setId(ID)
                .setLinkStatus(LINK_STATUS)
                .setProtocolType(PROTOCOL_TYPE)
                .setInterfaceName(IF_NAME)
                .setAddresses(ADDRESSES)
                .setDnsAddresses(DNSES)
                .setGatewayAddresses(GATEWAYS)
                .setPcscfAddresses(PCSCFS)
                .setMtuV4(MTU_V4)
                .setMtuV6(MTU_V6)
                .build();

        assertThat(response.getCause()).isEqualTo(CAUSE);
        assertThat(response.getSuggestedRetryTime()).isEqualTo(RETRY);
        assertThat(response.getId()).isEqualTo(ID);
        assertThat(response.getLinkStatus()).isEqualTo(LINK_STATUS);
        assertThat(response.getProtocolType()).isEqualTo(PROTOCOL_TYPE);
        assertThat(response.getInterfaceName()).isEqualTo(IF_NAME);
        assertThat(response.getAddresses()).isEqualTo(ADDRESSES);
        assertThat(response.getDnsAddresses()).isEqualTo(DNSES);
        assertThat(response.getGatewayAddresses()).isEqualTo(GATEWAYS);
        assertThat(response.getPcscfAddresses()).isEqualTo(PCSCFS);
        assertThat(response.getMtuV4()).isEqualTo(MTU_V4);
        assertThat(response.getMtuV6()).isEqualTo(MTU_V6);
    }

    @Test
    public void testEquals() {
        DataCallResponse response = new DataCallResponse.Builder()
                .setCause(CAUSE)
                .setSuggestedRetryTime(RETRY)
                .setId(ID)
                .setLinkStatus(LINK_STATUS)
                .setProtocolType(PROTOCOL_TYPE)
                .setInterfaceName(IF_NAME)
                .setAddresses(ADDRESSES)
                .setDnsAddresses(DNSES)
                .setGatewayAddresses(GATEWAYS)
                .setPcscfAddresses(PCSCFS)
                .setMtuV4(MTU_V4)
                .setMtuV6(MTU_V6)
                .build();

        DataCallResponse equalsResponse = new DataCallResponse.Builder()
                .setCause(CAUSE)
                .setSuggestedRetryTime(RETRY)
                .setId(ID)
                .setLinkStatus(LINK_STATUS)
                .setProtocolType(PROTOCOL_TYPE)
                .setInterfaceName(IF_NAME)
                .setAddresses(ADDRESSES)
                .setDnsAddresses(DNSES)
                .setGatewayAddresses(GATEWAYS)
                .setPcscfAddresses(PCSCFS)
                .setMtuV4(MTU_V4)
                .setMtuV6(MTU_V6)
                .build();

        assertThat(response).isEqualTo(equalsResponse);
    }

    @Test
    public void testNotEquals() {
        DataCallResponse response = new DataCallResponse.Builder()
                .setCause(CAUSE)
                .setSuggestedRetryTime(RETRY)
                .setId(ID)
                .setLinkStatus(LINK_STATUS)
                .setProtocolType(PROTOCOL_TYPE)
                .setInterfaceName(IF_NAME)
                .setAddresses(ADDRESSES)
                .setDnsAddresses(DNSES)
                .setGatewayAddresses(GATEWAYS)
                .setPcscfAddresses(PCSCFS)
                .setMtuV4(MTU_V4)
                .setMtuV6(MTU_V6)
                .build();

        DataCallResponse notEqualsResponse = new DataCallResponse.Builder()
                .setCause(1)
                .setSuggestedRetryTime(-1)
                .setId(1)
                .setLinkStatus(3)
                .setProtocolType(PROTOCOL_TYPE)
                .setInterfaceName(IF_NAME)
                .setAddresses(ADDRESSES)
                .setDnsAddresses(DNSES)
                .setGatewayAddresses(GATEWAYS)
                .setPcscfAddresses(PCSCFS)
                .setMtuV4(1441)
                .setMtuV6(1440)
                .build();

        assertThat(response).isNotEqualTo(notEqualsResponse);
        assertThat(response).isNotEqualTo(null);
        assertThat(response).isNotEqualTo(new String[1]);
    }

    @Test
    public void testParcel() {
        DataCallResponse response = new DataCallResponse.Builder()
                .setCause(CAUSE)
                .setSuggestedRetryTime(RETRY)
                .setId(ID)
                .setLinkStatus(LINK_STATUS)
                .setProtocolType(PROTOCOL_TYPE)
                .setInterfaceName(IF_NAME)
                .setAddresses(ADDRESSES)
                .setDnsAddresses(DNSES)
                .setGatewayAddresses(GATEWAYS)
                .setPcscfAddresses(PCSCFS)
                .setMtuV4(MTU_V4)
                .setMtuV6(MTU_V6)
                .build();

        Parcel stateParcel = Parcel.obtain();
        response.writeToParcel(stateParcel, 0);
        stateParcel.setDataPosition(0);

        DataCallResponse parcelResponse = DataCallResponse.CREATOR.createFromParcel(stateParcel);
        assertThat(response).isEqualTo(parcelResponse);
    }
}
