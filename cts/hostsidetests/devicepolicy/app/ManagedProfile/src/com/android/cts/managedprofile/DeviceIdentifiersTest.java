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
package com.android.cts.managedprofile;

import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;
import android.telephony.TelephonyManager;

/**
 * Verifies a profile owner on a personal device cannot access device identifiers.
 */
public class DeviceIdentifiersTest extends BaseManagedProfileTest {

    public void testProfileOwnerOnPersonalDeviceCannotGetDeviceIdentifiers() {
        // The profile owner with the READ_PHONE_STATE permission should still receive a
        // SecurityException when querying for device identifiers if it's not on an
        // organization-owned device.
        TelephonyManager telephonyManager = (TelephonyManager) mContext.getSystemService(
                Context.TELEPHONY_SERVICE);
        // Allow the APIs to also return null if the telephony feature is not supported.
        boolean hasTelephonyFeature =
                mContext.getPackageManager().hasSystemFeature(PackageManager.FEATURE_TELEPHONY);

        boolean mayReturnNull = !hasTelephonyFeature;

        assertAccessDenied(telephonyManager::getDeviceId, mayReturnNull);
        assertAccessDenied(telephonyManager::getImei, mayReturnNull);
        assertAccessDenied(telephonyManager::getMeid, mayReturnNull);
        assertAccessDenied(telephonyManager::getSubscriberId, mayReturnNull);
        assertAccessDenied(telephonyManager::getSimSerialNumber, mayReturnNull);
        assertAccessDenied(telephonyManager::getNai, mayReturnNull);
        assertAccessDenied(Build::getSerial, mayReturnNull);
    }

    private static <T> void assertAccessDenied(ThrowingProvider<T> provider,
            boolean mayReturnNull) {
        try {
            T object = provider.get();
            if (mayReturnNull) {
                assertNull(object);
            } else {
                fail("Expected SecurityException, received " + object);
            }
        } catch (SecurityException ignored) {
            // assertion succeeded
        } catch (Throwable th) {
            fail("Expected SecurityException but was: " + th);
        }
    }

    private interface ThrowingProvider<T> {
        T get() throws Throwable;
    }
}
