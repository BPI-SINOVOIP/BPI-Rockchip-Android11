/*
 * Copyright (C) 2018 The Android Open Source Project
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

package com.android.cts.deviceowner;

import static android.app.admin.DevicePolicyManager.PRIVATE_DNS_MODE_OFF;
import static android.app.admin.DevicePolicyManager.PRIVATE_DNS_MODE_OPPORTUNISTIC;
import static android.app.admin.DevicePolicyManager.PRIVATE_DNS_MODE_PROVIDER_HOSTNAME;

import android.app.admin.DevicePolicyManager;
import android.os.UserManager;

import static com.google.common.truth.Truth.assertThat;

import static org.testng.Assert.assertThrows;

public class PrivateDnsPolicyTest extends BaseDeviceOwnerTest {
    private static final String DUMMY_PRIVATE_DNS_HOST = "resolver.example.com";
    private static final String VALID_PRIVATE_DNS_HOST = "dns.google";

    private UserManager mUserManager;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mUserManager = mContext.getSystemService(UserManager.class);
        assertNotNull(mUserManager);
    }

    @Override
    protected void tearDown() throws Exception {
        super.tearDown();
        setUserRestriction(UserManager.DISALLOW_CONFIG_PRIVATE_DNS, false);
        mDevicePolicyManager.setGlobalPrivateDnsModeOpportunistic(getWho());
    }

    public void testDisallowPrivateDnsConfigurationRestriction() {
        setUserRestriction(UserManager.DISALLOW_CONFIG_PRIVATE_DNS, true);
        assertThat(mUserManager.hasUserRestriction(
                UserManager.DISALLOW_CONFIG_PRIVATE_DNS)).isTrue();
    }

    public void testClearDisallowPrivateDnsConfigurationRestriction() {
        setUserRestriction(UserManager.DISALLOW_CONFIG_PRIVATE_DNS, false);
        assertThat(mUserManager.hasUserRestriction(
                UserManager.DISALLOW_CONFIG_PRIVATE_DNS)).isFalse();
    }

    private void setUserRestriction(String restriction, boolean add) {
        DevicePolicyManager dpm = mContext.getSystemService(DevicePolicyManager.class);
        if (add) {
            dpm.addUserRestriction(getWho(), restriction);
        } else {
            dpm.clearUserRestriction(getWho(), restriction);
        }
    }

    /**
     * Call DevicePolicyManager.setGlobalPrivateDnsModeOpportunistic, expecting the result code
     * expectedResult.
     */
    private void callSetGlobalPrivateDnsOpportunisticModeExpectingResult(int expectedResult) {
        int resultCode = mDevicePolicyManager.setGlobalPrivateDnsModeOpportunistic(getWho());

        assertEquals(
                String.format(
                        "Call to setGlobalPrivateDnsModeOpportunistic "
                                + "should have produced result %d, but was %d",
                        expectedResult, resultCode),
                expectedResult, resultCode);
    }

    /**
     * Call DevicePolicyManager.setGlobalPrivateDnsModeSpecifiedHost with the given host, expecting
     * the result code expectedResult.
     */
    private void callSetGlobalPrivateDnsHostModeExpectingResult(String privateDnsHost,
            int expectedResult) {
        int resultCode = mDevicePolicyManager.setGlobalPrivateDnsModeSpecifiedHost(
                getWho(), privateDnsHost);

        assertEquals(
                String.format(
                        "Call to setGlobalPrivateDnsModeSpecifiedHost with host %s "
                                + "should have produced result %d, but was %d",
                        privateDnsHost, expectedResult, resultCode),
                expectedResult, resultCode);
    }

    public void testSetOpportunisticMode() {
        callSetGlobalPrivateDnsOpportunisticModeExpectingResult(
                DevicePolicyManager.PRIVATE_DNS_SET_NO_ERROR);

        assertThat(
                mDevicePolicyManager.getGlobalPrivateDnsMode(getWho())).isEqualTo(
                PRIVATE_DNS_MODE_OPPORTUNISTIC);
        assertThat(mDevicePolicyManager.getGlobalPrivateDnsHost(getWho())).isNull();
    }

    public void testSetSpecificHostMode() {
        callSetGlobalPrivateDnsHostModeExpectingResult(
                VALID_PRIVATE_DNS_HOST,
                DevicePolicyManager.PRIVATE_DNS_SET_NO_ERROR);

        assertThat(
                mDevicePolicyManager.getGlobalPrivateDnsMode(getWho())).isEqualTo(
                PRIVATE_DNS_MODE_PROVIDER_HOSTNAME);
        assertThat(
                mDevicePolicyManager.getGlobalPrivateDnsHost(getWho())).isEqualTo(
                VALID_PRIVATE_DNS_HOST);
    }

    public void testSetModeWithIncorrectHost() {
        assertThrows(
                NullPointerException.class,
                () -> mDevicePolicyManager.setGlobalPrivateDnsModeSpecifiedHost(getWho(), null));

        // This host does not resolve, so would output an error.
        callSetGlobalPrivateDnsHostModeExpectingResult(
                DUMMY_PRIVATE_DNS_HOST,
                DevicePolicyManager.PRIVATE_DNS_SET_ERROR_HOST_NOT_SERVING);
    }

    public void testCanSetModeDespiteUserRestriction() {
        // First set a specific host and assert that applied.
        callSetGlobalPrivateDnsHostModeExpectingResult(
                VALID_PRIVATE_DNS_HOST,
                DevicePolicyManager.PRIVATE_DNS_SET_NO_ERROR);
        assertThat(
                mDevicePolicyManager.getGlobalPrivateDnsMode(getWho())).isEqualTo(
                PRIVATE_DNS_MODE_PROVIDER_HOSTNAME);

        // Set a user restriction
        setUserRestriction(UserManager.DISALLOW_CONFIG_PRIVATE_DNS, true);

        // Next, set the mode to automatic and confirm that has applied.
        callSetGlobalPrivateDnsOpportunisticModeExpectingResult(
                DevicePolicyManager.PRIVATE_DNS_SET_NO_ERROR);

        assertThat(
                mDevicePolicyManager.getGlobalPrivateDnsMode(getWho())).isEqualTo(
                PRIVATE_DNS_MODE_OPPORTUNISTIC);
        assertThat(mDevicePolicyManager.getGlobalPrivateDnsHost(getWho())).isNull();
    }
}
