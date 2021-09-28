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

import android.os.Parcel;
import android.telephony.data.ApnSetting;
import android.telephony.data.DataProfile;

import org.junit.Test;

public class DataProfileTest {
    private static final int PROFILE_ID = 1;
    private static final String APN = "FAKE_APN";
    private static final int PROTOCOL_TYPE = ApnSetting.PROTOCOL_IP;
    private static final int AUTH_TYPE = ApnSetting.AUTH_TYPE_NONE;
    private static final String USER_NAME = "USER_NAME";
    private static final String PASSWORD = "PASSWORD";
    private static final int TYPE = DataProfile.TYPE_3GPP2;
    private static final boolean IS_ENABLED = true;
    private static final int APN_BITMASK = 1;
    private static final int ROAMING_PROTOCOL_TYPE = ApnSetting.PROTOCOL_IP;
    private static final int BEARER_BITMASK = 14;
    private static final int MTU_V4 = 1440;
    private static final int MTU_V6 = 1400;
    private static final boolean IS_PREFERRED = true;
    private static final boolean IS_PERSISTENT = true;

    @Test
    public void testConstructorAndGetters() {
        DataProfile profile = new DataProfile.Builder()
                .setProfileId(PROFILE_ID)
                .setApn(APN)
                .setProtocolType(PROTOCOL_TYPE)
                .setAuthType(AUTH_TYPE)
                .setUserName(USER_NAME)
                .setPassword(PASSWORD)
                .setType(TYPE)
                .enable(IS_ENABLED)
                .setSupportedApnTypesBitmask(APN_BITMASK)
                .setRoamingProtocolType(ROAMING_PROTOCOL_TYPE)
                .setBearerBitmask(BEARER_BITMASK)
                .setMtuV4(MTU_V4)
                .setMtuV6(MTU_V6)
                .setPreferred(IS_PREFERRED)
                .setPersistent(IS_PERSISTENT)
                .build();

        assertThat(profile.getProfileId()).isEqualTo(PROFILE_ID);
        assertThat(profile.getApn()).isEqualTo(APN);
        assertThat(profile.getProtocolType()).isEqualTo(PROTOCOL_TYPE);
        assertThat(profile.getAuthType()).isEqualTo(AUTH_TYPE);
        assertThat(profile.getUserName()).isEqualTo(USER_NAME);
        assertThat(profile.getPassword()).isEqualTo(PASSWORD);
        assertThat(profile.getType()).isEqualTo(TYPE);
        assertThat(profile.isEnabled()).isEqualTo(IS_ENABLED);
        assertThat(profile.getSupportedApnTypesBitmask()).isEqualTo(APN_BITMASK);
        assertThat(profile.getRoamingProtocolType()).isEqualTo(ROAMING_PROTOCOL_TYPE);
        assertThat(profile.getBearerBitmask()).isEqualTo(BEARER_BITMASK);
        assertThat(profile.getMtuV4()).isEqualTo(MTU_V4);
        assertThat(profile.getMtuV6()).isEqualTo(MTU_V6);
        assertThat(profile.isPreferred()).isEqualTo(IS_PREFERRED);
        assertThat(profile.isPersistent()).isEqualTo(IS_PERSISTENT);
    }

    @Test
    public void testEquals() {
        DataProfile profile = new DataProfile.Builder()
                .setProfileId(PROFILE_ID)
                .setApn(APN)
                .setProtocolType(PROTOCOL_TYPE)
                .setAuthType(AUTH_TYPE)
                .setUserName(USER_NAME)
                .setPassword(PASSWORD)
                .setType(TYPE)
                .enable(IS_ENABLED)
                .setSupportedApnTypesBitmask(APN_BITMASK)
                .setRoamingProtocolType(ROAMING_PROTOCOL_TYPE)
                .setBearerBitmask(BEARER_BITMASK)
                .setMtuV4(MTU_V4)
                .setMtuV6(MTU_V6)
                .setPreferred(IS_PREFERRED)
                .setPersistent(IS_PERSISTENT)
                .build();

        DataProfile equalsProfile = new DataProfile.Builder()
                .setProfileId(PROFILE_ID)
                .setApn(APN)
                .setProtocolType(PROTOCOL_TYPE)
                .setAuthType(AUTH_TYPE)
                .setUserName(USER_NAME)
                .setPassword(PASSWORD)
                .setType(TYPE)
                .enable(IS_ENABLED)
                .setSupportedApnTypesBitmask(APN_BITMASK)
                .setRoamingProtocolType(ROAMING_PROTOCOL_TYPE)
                .setBearerBitmask(BEARER_BITMASK)
                .setMtuV4(MTU_V4)
                .setMtuV6(MTU_V6)
                .setPreferred(IS_PREFERRED)
                .setPersistent(IS_PERSISTENT)
                .build();

        assertThat(profile).isEqualTo(equalsProfile);
    }

    @Test
    public void testNotEquals() {
        DataProfile profile = new DataProfile.Builder()
                .setProfileId(PROFILE_ID)
                .setApn(APN)
                .setProtocolType(PROTOCOL_TYPE)
                .setAuthType(AUTH_TYPE)
                .setUserName(USER_NAME)
                .setPassword(PASSWORD)
                .setType(TYPE)
                .enable(IS_ENABLED)
                .setSupportedApnTypesBitmask(APN_BITMASK)
                .setRoamingProtocolType(ROAMING_PROTOCOL_TYPE)
                .setBearerBitmask(BEARER_BITMASK)
                .setMtuV4(MTU_V4)
                .setMtuV6(MTU_V6)
                .setPreferred(IS_PREFERRED)
                .setPersistent(IS_PERSISTENT)
                .build();

        DataProfile notEqualsProfile = new DataProfile.Builder()
                .setProfileId(0)
                .setApn(APN)
                .setProtocolType(PROTOCOL_TYPE)
                .setAuthType(AUTH_TYPE)
                .setUserName(USER_NAME)
                .setPassword(PASSWORD)
                .setType(TYPE)
                .enable(false)
                .setSupportedApnTypesBitmask(2)
                .setRoamingProtocolType(ROAMING_PROTOCOL_TYPE)
                .setBearerBitmask(3)
                .setMtuV4(1441)
                .setMtuV6(1401)
                .setPreferred(false)
                .setPersistent(false)
                .build();

        assertThat(profile).isNotEqualTo(notEqualsProfile);
        assertThat(profile).isNotEqualTo(null);
        assertThat(profile).isNotEqualTo(new String[1]);
    }

    @Test
    public void testParcel() {
        DataProfile profile = new DataProfile.Builder()
                .setProfileId(PROFILE_ID)
                .setApn(APN)
                .setProtocolType(PROTOCOL_TYPE)
                .setAuthType(AUTH_TYPE)
                .setUserName(USER_NAME)
                .setPassword(PASSWORD)
                .setType(TYPE)
                .enable(IS_ENABLED)
                .setSupportedApnTypesBitmask(APN_BITMASK)
                .setRoamingProtocolType(ROAMING_PROTOCOL_TYPE)
                .setBearerBitmask(BEARER_BITMASK)
                .setMtuV4(MTU_V4)
                .setMtuV6(MTU_V6)
                .setPreferred(IS_PREFERRED)
                .setPersistent(IS_PERSISTENT)
                .build();

        Parcel stateParcel = Parcel.obtain();
        profile.writeToParcel(stateParcel, 0);
        stateParcel.setDataPosition(0);

        DataProfile parcelProfile = DataProfile.CREATOR.createFromParcel(stateParcel);
        assertThat(profile).isEqualTo(parcelProfile);
    }
}
