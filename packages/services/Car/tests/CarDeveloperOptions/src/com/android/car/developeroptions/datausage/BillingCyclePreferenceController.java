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

package com.android.car.developeroptions.datausage;

import android.content.Context;
import android.net.INetworkStatsService;
import android.net.NetworkPolicyManager;
import android.net.NetworkTemplate;
import android.os.INetworkManagementService;
import android.os.ServiceManager;
import android.os.UserManager;
import android.telephony.SubscriptionManager;
import android.telephony.TelephonyManager;

import com.android.car.developeroptions.core.BasePreferenceController;
import com.android.settingslib.NetworkPolicyEditor;

import androidx.preference.PreferenceScreen;

public class BillingCyclePreferenceController extends BasePreferenceController {
    private int mSubscriptionId;

    public BillingCyclePreferenceController(Context context, String preferenceKey) {
        super(context, preferenceKey);
    }

    public void init(int subscriptionId) {
        mSubscriptionId = subscriptionId;
    }

    @Override
    public void displayPreference(PreferenceScreen screen) {
        super.displayPreference(screen);
        BillingCyclePreference preference = screen.findPreference(getPreferenceKey());

        TemplatePreference.NetworkServices services = new TemplatePreference.NetworkServices();
        services.mNetworkService = INetworkManagementService.Stub.asInterface(
                ServiceManager.getService(Context.NETWORKMANAGEMENT_SERVICE));
        services.mStatsService = INetworkStatsService.Stub.asInterface(
                ServiceManager.getService(Context.NETWORK_STATS_SERVICE));
        services.mPolicyManager = mContext.getSystemService(NetworkPolicyManager.class);
        services.mPolicyEditor = new NetworkPolicyEditor(services.mPolicyManager);
        services.mTelephonyManager = mContext.getSystemService(TelephonyManager.class);
        services.mSubscriptionManager = mContext.getSystemService(SubscriptionManager.class);
        services.mUserManager = mContext.getSystemService(UserManager.class);

        NetworkTemplate mobileAll = NetworkTemplate.buildTemplateMobileAll(
                services.mTelephonyManager.getSubscriberId(mSubscriptionId));
        NetworkTemplate template = NetworkTemplate.normalize(mobileAll,
                services.mTelephonyManager.getMergedSubscriberIds());

        preference.setTemplate(template, mSubscriptionId, services);
    }

    @Override
    public int getAvailabilityStatus() {
        return AVAILABLE;
    }
}
