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

package com.android.cts.deviceandprofileowner;

import static com.google.common.truth.Truth.assertThat;

import android.content.ComponentName;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public class InputMethodsTest extends BaseDeviceAdminTest {
    private final ComponentName badAdmin = new ComponentName("com.foo.bar", ".BazQuxReceiver");

    public void testPermittedInputMethods() {
        // All input methods are allowed.
        mDevicePolicyManager.setPermittedInputMethods(ADMIN_RECEIVER_COMPONENT, null);
        assertThat(
                mDevicePolicyManager.getPermittedInputMethods(ADMIN_RECEIVER_COMPONENT)).isNull();

        // Only system input methods are allowed.
        mDevicePolicyManager.setPermittedInputMethods(ADMIN_RECEIVER_COMPONENT, new ArrayList<>());
        assertThat(
                mDevicePolicyManager.getPermittedInputMethods(ADMIN_RECEIVER_COMPONENT)).isEmpty();

        // Some random methods.
        final List<String> packages = Arrays.asList("com.google.pkg.one", "com.google.pkg.two");
        mDevicePolicyManager.setPermittedInputMethods(ADMIN_RECEIVER_COMPONENT, packages);
        assertThat(
                mDevicePolicyManager.getPermittedInputMethods(ADMIN_RECEIVER_COMPONENT))
                .containsExactlyElementsIn(packages);
    }

    public void testPermittedInputMethodsThrowsIfWrongAdmin() {
        try {
            mDevicePolicyManager.setPermittedInputMethods(badAdmin, null);
            fail("setPermittedInputMethods didn't throw when passed invalid admin");
        } catch (SecurityException expected) {
        }
        try {
            mDevicePolicyManager.getPermittedInputMethods(badAdmin);
            fail("getPermittedInputMethods didn't throw when passed invalid admin");
        } catch (SecurityException expected) {
        }
    }
}
