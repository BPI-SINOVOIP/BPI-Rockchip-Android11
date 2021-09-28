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

package android.telephony3.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.fail;

import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;
import android.telephony.SubscriptionInfo;
import android.telephony.SubscriptionManager;
import android.telephony.TelephonyManager;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Verifies the APIs for apps with the READ_PHONE_STATE permission targeting pre-Q.
 *
 * @see android.telephony.cts.TelephonyManagerTest

 */
@RunWith(AndroidJUnit4.class)
public class TelephonyManagerTest {
    private Context mContext;
    private TelephonyManager mTelephonyManager;

    @Before
    public void setUp() throws Exception {
        mContext = InstrumentationRegistry.getContext();
        mTelephonyManager = (TelephonyManager) mContext.getSystemService(Context.TELEPHONY_SERVICE);
    }

    @Test
    public void testDeviceIdentifiersAreNotAccessible() throws Exception {
        // Apps with the READ_PHONE_STATE permission should no longer have access to device
        // identifiers. If an app's target SDK is less than Q and it has been granted the
        // READ_PHONE_STATE permission then a null value should be returned when querying for device
        // identifiers; this test verifies a null value is returned for device identifier queries.
        try {
            assertNull(
                    "An app targeting pre-Q with the READ_PHONE_STATE permission granted must "
                            + "receive null when invoking getDeviceId",
                    mTelephonyManager.getDeviceId());
            assertNull(
                    "An app targeting pre-Q with the READ_PHONE_STATE permission granted must "
                            + "receive null when invoking getImei",
                    mTelephonyManager.getImei());
            assertNull(
                    "An app targeting pre-Q with the READ_PHONE_STATE permission granted must "
                            + "receive null when invoking getMeid",
                    mTelephonyManager.getMeid());
            assertNull(
                    "An app targeting pre-Q with the READ_PHONE_STATE permission granted must "
                            + "receive null when invoking getSubscriberId",
                    mTelephonyManager.getSubscriberId());
            assertNull(
                    "An app targeting pre-Q with the READ_PHONE_STATE permission granted must "
                            + "receive null when invoking getSimSerialNumber",
                    mTelephonyManager.getSimSerialNumber());
            assertNull(
                    "An app targeting pre-Q with the READ_PHONE_STATE permission granted must "
                            + "receive null when invoking getNai",
                    mTelephonyManager.getNai());
            // Since Build.getSerial is not documented to return null in previous releases this test
            // verifies that the Build.UNKNOWN value is returned when the caller does not have
            // permission to access the device identifier.
            assertEquals(
                    "An app targeting pre-Q with the READ_PHONE_STATE permission granted must "
                            + "receive " + Build.UNKNOWN + " when invoking Build.getSerial",
                    Build.getSerial(), Build.UNKNOWN);
            // Previous getIccId documentation does not indicate the value returned if the ICC ID is
            // not available, so to prevent NPEs SubscriptionInfo#getIccId will return an empty
            // string if the caller does not have permission to access this identifier.
            if (mContext.getPackageManager().hasSystemFeature(PackageManager.FEATURE_TELEPHONY)) {
                SubscriptionManager subscriptionManager =
                        (SubscriptionManager) mContext.getSystemService(
                                Context.TELEPHONY_SUBSCRIPTION_SERVICE);
                int subId = subscriptionManager.getDefaultSubscriptionId();
                if (subId != SubscriptionManager.INVALID_SUBSCRIPTION_ID) {
                    SubscriptionInfo subInfo = subscriptionManager.getActiveSubscriptionInfo(
                            subId);
                    assertEquals(
                            "An app targeting pre-Q with the READ_PHONE_STATE permission granted "
                                    + "must receive an empty string when invoking getIccId",
                            "", subInfo.getIccId());
                }
            }
        } catch (SecurityException e) {
            fail("An app targeting pre-Q with the READ_PHONE_STATE permission granted must "
                    + "receive null (or "
                    + Build.UNKNOWN
                    + " for Build#getSerial) when querying for device identifiers, caught "
                    + "SecurityException instead: " + e);
        }
    }
}
