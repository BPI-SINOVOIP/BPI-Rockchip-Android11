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

package com.android.cts.deviceandprofileowner;

import static com.google.common.truth.Truth.assertThat;

import android.app.admin.FactoryResetProtectionPolicy;

import java.util.ArrayList;
import java.util.List;

public class FactoryResetProtectionPolicyTest extends BaseDeviceAdminTest {

    public void testSetFactoryResetProtectionPolicy() {
        if (!mDevicePolicyManager.isFactoryResetProtectionPolicySupported()) {
            return;
        }

        List<String> accounts = new ArrayList<>();
        accounts.add("Account 1");
        accounts.add("Account 2");

        FactoryResetProtectionPolicy policy = new FactoryResetProtectionPolicy.Builder()
                .setFactoryResetProtectionAccounts(accounts)
                .setFactoryResetProtectionEnabled(false)
                .build();

        mDevicePolicyManager.setFactoryResetProtectionPolicy(ADMIN_RECEIVER_COMPONENT, policy);

        FactoryResetProtectionPolicy result = mDevicePolicyManager.getFactoryResetProtectionPolicy(
                ADMIN_RECEIVER_COMPONENT);

        assertThat(result).isNotNull();
        assertPoliciesAreEqual(policy, result);
    }

    public void testSetFactoryResetProtectionPolicy_nullPolicy() {
        if (!mDevicePolicyManager.isFactoryResetProtectionPolicySupported()) {
            return;
        }

        // Set a non-default policy
        FactoryResetProtectionPolicy policy = new FactoryResetProtectionPolicy.Builder()
                .setFactoryResetProtectionAccounts(new ArrayList<>())
                .setFactoryResetProtectionEnabled(false)
                .build();
        mDevicePolicyManager.setFactoryResetProtectionPolicy(ADMIN_RECEIVER_COMPONENT, policy);

        // Get the policy and assert it is not equal to the default policy
        FactoryResetProtectionPolicy result = mDevicePolicyManager.getFactoryResetProtectionPolicy(
                ADMIN_RECEIVER_COMPONENT);
        assertThat(result).isNotNull();
        assertPoliciesAreEqual(policy, result);

        // Set null policy
        mDevicePolicyManager.setFactoryResetProtectionPolicy(ADMIN_RECEIVER_COMPONENT, null);

        // Get the policy and assert it is equal to the default policy
        result = mDevicePolicyManager.getFactoryResetProtectionPolicy(ADMIN_RECEIVER_COMPONENT);
        assertThat(result).isNull();
    }

    private void assertPoliciesAreEqual(FactoryResetProtectionPolicy expectedPolicy,
            FactoryResetProtectionPolicy actualPolicy) {
        assertThat(actualPolicy.isFactoryResetProtectionEnabled()).isEqualTo(
                expectedPolicy.isFactoryResetProtectionEnabled());
        assertAccountsAreEqual(expectedPolicy.getFactoryResetProtectionAccounts(),
                actualPolicy.getFactoryResetProtectionAccounts());
    }

    private void assertAccountsAreEqual(List<String> expectedAccounts,
            List<String> actualAccounts) {
        assertThat(actualAccounts).containsExactlyElementsIn(expectedAccounts);
    }

}

