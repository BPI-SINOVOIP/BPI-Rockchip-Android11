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

public class CommonCriteriaModeTest extends BaseDeviceAdminTest {

    public void testSettingCommonCriteriaMode() {
        try {
            // Default to false
            assertFalse(mDevicePolicyManager.isCommonCriteriaModeEnabled(ADMIN_RECEIVER_COMPONENT));
            assertFalse(mDevicePolicyManager.isCommonCriteriaModeEnabled(null));

            mDevicePolicyManager.setCommonCriteriaModeEnabled(ADMIN_RECEIVER_COMPONENT, true);
            assertTrue(mDevicePolicyManager.isCommonCriteriaModeEnabled(ADMIN_RECEIVER_COMPONENT));
            assertTrue(mDevicePolicyManager.isCommonCriteriaModeEnabled(null));
        } finally {
            mDevicePolicyManager.setCommonCriteriaModeEnabled(ADMIN_RECEIVER_COMPONENT, false);
        }
    }
}
