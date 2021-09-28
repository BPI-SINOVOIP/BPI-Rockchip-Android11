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

package com.android.car.settings.datausage;

import android.car.drivingstate.CarUxRestrictions;
import android.content.Context;
import android.net.ConnectivityManager;
import android.telephony.SubscriptionManager;
import android.telephony.SubscriptionPlan;

import androidx.preference.Preference;

import com.android.car.settings.common.FragmentController;
import com.android.car.settings.common.PreferenceController;
import com.android.car.settings.network.NetworkUtils;

/** Preference controller which shows how much data has been used so far. */
public class DataUsageEntryPreferenceController extends PreferenceController<Preference> {

    public DataUsageEntryPreferenceController(Context context, String preferenceKey,
            FragmentController fragmentController, CarUxRestrictions uxRestrictions) {
        super(context, preferenceKey, fragmentController, uxRestrictions);
    }

    @Override
    protected Class<Preference> getPreferenceType() {
        return Preference.class;
    }

    @Override
    protected int getAvailabilityStatus() {
        if (!NetworkUtils.hasMobileNetwork(
                getContext().getSystemService(ConnectivityManager.class))) {
            return UNSUPPORTED_ON_DEVICE;
        }
        return AVAILABLE;
    }

    @Override
    protected void updateState(Preference preference) {
        preference.setSummary(formatUsedData());
    }

    private CharSequence formatUsedData() {
        SubscriptionManager subscriptionManager = getContext().getSystemService(
                SubscriptionManager.class);
        int defaultSubId = subscriptionManager.getDefaultSubscriptionId();
        if (defaultSubId == SubscriptionManager.INVALID_SUBSCRIPTION_ID) {
            return null;
        }
        SubscriptionPlan defaultPlan = DataUsageUtils.getPrimaryPlan(subscriptionManager,
                defaultSubId);
        if (defaultPlan == null) {
            return null;
        }

        return DataUsageUtils.bytesToIecUnits(getContext(), defaultPlan.getDataUsageBytes());
    }

}
