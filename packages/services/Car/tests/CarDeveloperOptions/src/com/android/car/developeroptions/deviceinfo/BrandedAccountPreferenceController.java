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

package com.android.car.developeroptions.deviceinfo;

import android.accounts.Account;
import android.app.settings.SettingsEnums;
import android.content.Context;
import android.os.Bundle;

import androidx.preference.Preference;
import androidx.preference.PreferenceScreen;

import com.android.car.developeroptions.R;
import com.android.car.developeroptions.accounts.AccountDetailDashboardFragment;
import com.android.car.developeroptions.accounts.AccountFeatureProvider;
import com.android.car.developeroptions.core.BasePreferenceController;
import com.android.car.developeroptions.core.SubSettingLauncher;
import com.android.car.developeroptions.overlay.FeatureFactory;

public class BrandedAccountPreferenceController extends BasePreferenceController {
    private final Account[] mAccounts;

    public BrandedAccountPreferenceController(Context context, String key) {
        super(context, key);
        final AccountFeatureProvider accountFeatureProvider = FeatureFactory.getFactory(
                mContext).getAccountFeatureProvider();
        mAccounts = accountFeatureProvider.getAccounts(mContext);
    }

    @Override
    public int getAvailabilityStatus() {
        if (!mContext.getResources().getBoolean(
                R.bool.config_show_branded_account_in_device_info)) {
            return UNSUPPORTED_ON_DEVICE;
        }
        if (mAccounts != null && mAccounts.length > 0) {
            return AVAILABLE;
        }
        return DISABLED_FOR_USER;
    }

    @Override
    public void displayPreference(PreferenceScreen screen) {
        super.displayPreference(screen);
        final AccountFeatureProvider accountFeatureProvider = FeatureFactory.getFactory(
                mContext).getAccountFeatureProvider();
        final Preference accountPreference = screen.findPreference(getPreferenceKey());
        if (accountPreference != null && (mAccounts == null || mAccounts.length == 0)) {
            screen.removePreference(accountPreference);
            return;
        }

        accountPreference.setSummary(mAccounts[0].name);
        accountPreference.setOnPreferenceClickListener(preference -> {
            final Bundle args = new Bundle();
            args.putParcelable(AccountDetailDashboardFragment.KEY_ACCOUNT,
                    mAccounts[0]);
            args.putParcelable(AccountDetailDashboardFragment.KEY_USER_HANDLE,
                    android.os.Process.myUserHandle());
            args.putString(AccountDetailDashboardFragment.KEY_ACCOUNT_TYPE,
                    accountFeatureProvider.getAccountType());

            new SubSettingLauncher(mContext)
                    .setDestination(AccountDetailDashboardFragment.class.getName())
                    .setTitleRes(R.string.account_sync_title)
                    .setArguments(args)
                    .setSourceMetricsCategory(SettingsEnums.DEVICEINFO)
                    .launch();
            return true;
        });
    }
}
