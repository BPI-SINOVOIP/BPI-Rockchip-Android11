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
package com.android.cts.verifier.managedprovisioning;

import android.app.admin.DevicePolicyManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.view.View;

import com.android.cts.verifier.ArrayTestListAdapter;
import com.android.cts.verifier.DialogTestListActivity;
import com.android.cts.verifier.R;

public class NonMarketAppsActivity extends DialogTestListActivity {

    protected DevicePolicyManager mDpm;

    public NonMarketAppsActivity() {
        super(R.layout.provisioning_byod,
              R.string.provisioning_byod_non_market_apps,
              R.string.provisioning_byod_non_market_apps_info,
              R.string.provisioning_byod_non_market_apps_info);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // Hiding button from default BYOD test layout, as it is not useful in this test.
        mPrepareTestButton = findViewById(R.id.prepare_test_button);
        mPrepareTestButton.setVisibility(View.INVISIBLE);
        mDpm = (DevicePolicyManager) getSystemService(Context.DEVICE_POLICY_SERVICE);
    }

    @Override
    protected void setupTests(ArrayTestListAdapter adapter) {
        DialogTestListItem disableNonMarketTest = new DialogTestListItem(this,
                R.string.provisioning_byod_nonmarket_deny,
                "BYOD_DisableNonMarketTest",
                R.string.provisioning_byod_nonmarket_deny_info,
                new Intent(ByodHelperActivity.ACTION_INSTALL_APK)
                        .putExtra(ByodHelperActivity.EXTRA_ALLOW_NON_MARKET_APPS, false));

        DialogTestListItem enableNonMarketTest = new DialogTestListItem(this,
                R.string.provisioning_byod_nonmarket_allow,
                "BYOD_EnableNonMarketTest",
                R.string.provisioning_byod_nonmarket_allow_info,
                new Intent(ByodHelperActivity.ACTION_INSTALL_APK)
                        .putExtra(ByodHelperActivity.EXTRA_ALLOW_NON_MARKET_APPS, true));

        DialogTestListItem disableNonMarketWorkProfileDeviceWideTest = new DialogTestListItem(
                this,
                R.string.provisioning_byod_nonmarket_deny_global,
                "BYOD_DisableNonMarketDeviceWideTest",
                R.string.provisioning_byod_nonmarket_deny_global_info,
                new Intent(ByodHelperActivity.ACTION_INSTALL_APK_WORK_PROFILE_GLOBAL_RESTRICTION)
                        .putExtra(ByodHelperActivity.EXTRA_ALLOW_NON_MARKET_APPS_DEVICE_WIDE,
                                false));

        DialogTestListItem enableNonMarketWorkProfileDeviceWideTest = new DialogTestListItem(
                this,
                R.string.provisioning_byod_nonmarket_allow_global,
                "BYOD_EnableNonMarketDeviceWideTest",
                R.string.provisioning_byod_nonmarket_allow_global_info,
                new Intent(ByodHelperActivity.ACTION_INSTALL_APK_WORK_PROFILE_GLOBAL_RESTRICTION)
                        .putExtra(ByodHelperActivity.EXTRA_ALLOW_NON_MARKET_APPS_DEVICE_WIDE,
                                true));

        DialogTestListItem disableNonMarketPrimaryUserDeviceWideTest = new DialogTestListItem(
                this,
                R.string.provisioning_byod_nonmarket_deny_global_primary,
                "BYOD_DisableNonMarketPrimaryUserDeviceWideTest",
                R.string.provisioning_byod_nonmarket_deny_global_primary_info,
                new Intent(ByodHelperActivity.ACTION_INSTALL_APK_PRIMARY_PROFILE_GLOBAL_RESTRICTION)
                        .putExtra(ByodHelperActivity.EXTRA_ALLOW_NON_MARKET_APPS_DEVICE_WIDE,
                                false));

        DialogTestListItem enableNonMarketPrimaryUserDeviceWideTest = new DialogTestListItem(
                this,
                R.string.provisioning_byod_nonmarket_allow_global_primary,
                "BYOD_EnableNonMarketPrimaryUserDeviceWideTest",
                R.string.provisioning_byod_nonmarket_allow_global_primary_info,
                new Intent(ByodHelperActivity.ACTION_INSTALL_APK_PRIMARY_PROFILE_GLOBAL_RESTRICTION)
                        .putExtra(ByodHelperActivity.EXTRA_ALLOW_NON_MARKET_APPS_DEVICE_WIDE,
                                true));

        adapter.add(disableNonMarketTest);
        adapter.add(enableNonMarketTest);
        adapter.add(disableNonMarketWorkProfileDeviceWideTest);
        adapter.add(enableNonMarketWorkProfileDeviceWideTest);
        adapter.add(disableNonMarketPrimaryUserDeviceWideTest);
        adapter.add(enableNonMarketPrimaryUserDeviceWideTest);
    }

    protected ComponentName getAdminComponent() {
        return DeviceAdminTestReceiver.getReceiverComponentName();
    }
}
