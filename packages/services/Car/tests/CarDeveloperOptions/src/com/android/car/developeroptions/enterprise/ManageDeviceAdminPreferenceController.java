/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the
 * License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */
package com.android.car.developeroptions.enterprise;

import android.content.Context;

import com.android.car.developeroptions.R;
import com.android.car.developeroptions.core.BasePreferenceController;
import com.android.car.developeroptions.overlay.FeatureFactory;


public class ManageDeviceAdminPreferenceController extends BasePreferenceController {

    private final EnterprisePrivacyFeatureProvider mFeatureProvider;

    public ManageDeviceAdminPreferenceController(Context context, String key) {
        super(context, key);
        mFeatureProvider = FeatureFactory.getFactory(context)
                .getEnterprisePrivacyFeatureProvider(context);
    }

    @Override
    public CharSequence getSummary() {
        final int activeAdmins
                = mFeatureProvider.getNumberOfActiveDeviceAdminsForCurrentUserAndManagedProfile();
        return activeAdmins == 0
                ? mContext.getResources().getString(R.string.number_of_device_admins_none)
                : mContext.getResources().getQuantityString(R.plurals.number_of_device_admins,
                        activeAdmins, activeAdmins);
    }

    @Override
    public int getAvailabilityStatus() {
        return mContext.getResources().getBoolean(R.bool.config_show_manage_device_admin)
                ? AVAILABLE_UNSEARCHABLE
                : UNSUPPORTED_ON_DEVICE;
    }

}
