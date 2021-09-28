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

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.service.carrier.CarrierService;
import android.telephony.SubscriptionInfo;
import android.telephony.SubscriptionManager;
import android.telephony.TelephonyManager;

import androidx.test.platform.app.InstrumentationRegistry;

import com.android.compatibility.common.util.ShellIdentityUtils;

import java.util.List;
import java.util.concurrent.Callable;

public class ImsUtils {
    public static final boolean VDBG = false;

    // ImsService rebind has an exponential backoff capping at 64 seconds. Wait for 70 seconds to
    // allow for the new poll to happen in the framework.
    public static final int TEST_TIMEOUT_MS = 70000;

    // Id for non compressed auto configuration xml.
    public static final int ITEM_NON_COMPRESSED = 2000;
    // Id for compressed auto configuration xml.
    public static final int ITEM_COMPRESSED = 2001;

    public static boolean shouldTestImsService() {
        final PackageManager pm = InstrumentationRegistry.getInstrumentation().getContext()
                .getPackageManager();
        boolean hasTelephony = pm.hasSystemFeature(PackageManager.FEATURE_TELEPHONY);
        boolean hasIms = pm.hasSystemFeature(PackageManager.FEATURE_TELEPHONY_IMS);
        return hasTelephony && hasIms;
    }

    public static int getPreferredActiveSubId() {
        Context context = InstrumentationRegistry.getInstrumentation().getContext();
        SubscriptionManager sm = (SubscriptionManager) context.getSystemService(
                Context.TELEPHONY_SUBSCRIPTION_SERVICE);
        List<SubscriptionInfo> infos = ShellIdentityUtils.invokeMethodWithShellPermissions(sm,
                SubscriptionManager::getActiveSubscriptionInfoList);

        int defaultSubId = SubscriptionManager.getDefaultVoiceSubscriptionId();
        if (defaultSubId != SubscriptionManager.INVALID_SUBSCRIPTION_ID
                && isSubIdInInfoList(infos, defaultSubId)) {
            return defaultSubId;
        }

        defaultSubId = SubscriptionManager.getDefaultSubscriptionId();
        if (defaultSubId != SubscriptionManager.INVALID_SUBSCRIPTION_ID
                && isSubIdInInfoList(infos, defaultSubId)) {
            return defaultSubId;
        }

        // Couldn't resolve a default. We can try to resolve a default using the active
        // subscriptions.
        if (!infos.isEmpty()) {
            return infos.get(0).getSubscriptionId();
        }
        // There must be at least one active subscription.
        return SubscriptionManager.INVALID_SUBSCRIPTION_ID;
    }

    private static boolean isSubIdInInfoList(List<SubscriptionInfo> infos, int subId) {
        return infos.stream().anyMatch(info -> info.getSubscriptionId() == subId);
    }

    /**
     * If a carrier app implements CarrierMessagingService it can choose to take care of handling
     * SMS OTT so SMS over IMS APIs won't be triggered which would be WAI so we do not run the tests
     * if there exist a carrier app that declares a CarrierMessagingService
     */
    public static boolean shouldRunSmsImsTests(int subId) {
        if (!shouldTestImsService()) {
            return false;
        }
        Context context = InstrumentationRegistry.getInstrumentation().getContext();
        TelephonyManager tm =
                (TelephonyManager) InstrumentationRegistry.getInstrumentation().getContext()
                        .getSystemService(Context.TELEPHONY_SERVICE);
        tm = tm.createForSubscriptionId(subId);
        List<String> carrierPackages = tm.getCarrierPackageNamesForIntent(
                new Intent(CarrierService.CARRIER_SERVICE_INTERFACE));

        if (carrierPackages == null || carrierPackages.size() == 0) {
            return true;
        }
        final PackageManager packageManager = context.getPackageManager();
        Intent intent = new Intent("android.service.carrier.CarrierMessagingService");
        List<ResolveInfo> resolveInfos = packageManager.queryIntentServices(intent, 0);
        for (ResolveInfo info : resolveInfos) {
            if (carrierPackages.contains(info.serviceInfo.packageName)) {
                return false;
            }
        }

        return true;
    }

    /**
     * Retry every 5 seconds until the condition is true or fail after TEST_TIMEOUT_MS seconds.
     */
    public static boolean retryUntilTrue(Callable<Boolean> condition) throws Exception {
        int retryCounter = 0;
        while (retryCounter < (TEST_TIMEOUT_MS / 5000)) {
            try {
                Boolean isSuccessful = condition.call();
                isSuccessful = (isSuccessful == null) ? false : isSuccessful;
                if (isSuccessful) return true;
            } catch (Exception e) {
                // we will retry
            }
            Thread.sleep(5000);
            retryCounter++;
        }
        return false;
    }
}
