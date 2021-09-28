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

package com.android.cts.deviceowner;

import static com.google.common.truth.Truth.assertThat;

import android.provider.Telephony;

public class DefaultSmsApplicationTest extends BaseDeviceOwnerTest {

    public void testSetDefaultSmsApplication() {
        String previousSmsAppName = Telephony.Sms.getDefaultSmsPackage(mContext);
        String newSmsAppName = "android.telephony.cts.sms.simplesmsapp";

        mDevicePolicyManager.setDefaultSmsApplication(getWho(), newSmsAppName);
        String defaultSmsApp = Telephony.Sms.getDefaultSmsPackage(mContext);
        assertThat(defaultSmsApp).isNotNull();
        assertThat(defaultSmsApp).isEqualTo(newSmsAppName);

        // Restore previous default sms application
        mDevicePolicyManager.setDefaultSmsApplication(getWho(), previousSmsAppName);
        defaultSmsApp = Telephony.Sms.getDefaultSmsPackage(mContext);
        assertThat(defaultSmsApp).isNotNull();
        assertThat(defaultSmsApp).isEqualTo(previousSmsAppName);
    }

}
