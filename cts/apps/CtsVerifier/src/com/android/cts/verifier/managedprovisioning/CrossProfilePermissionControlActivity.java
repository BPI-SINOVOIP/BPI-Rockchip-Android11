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

package com.android.cts.verifier.managedprovisioning;

import android.app.admin.DevicePolicyManager;
import android.content.ComponentName;
import android.content.Intent;
import android.os.Bundle;
import android.provider.Settings;
import android.view.View;
import android.view.View.OnClickListener;

import com.android.cts.verifier.ArrayTestListAdapter;
import com.android.cts.verifier.DialogTestListActivity;
import com.android.cts.verifier.R;

import java.util.Set;

public class CrossProfilePermissionControlActivity extends DialogTestListActivity {
    static final String ACTION_CROSS_PROFILE_PERMISSION_CONTROL =
            "com.android.cts.verifier.managedprovisioning.action.CROSS_PROFILE_PERMISSION_CONTROL";
    private static final String TEST_APP_PACKAGE_NAME =
            "com.android.cts.crossprofilepermissioncontrol";
    private static final String OPEN_TEST_APP_ACTION =
            "com.android.cts.verifier.managedprovisioning.action.OPEN_CROSS_PROFILE_TEST_APP";

    protected DevicePolicyManager mDpm;

    public CrossProfilePermissionControlActivity() {
        super(R.layout.provisioning_byod,
                R.string.provisioning_byod_cross_profile_permission_control,
                R.string.provisioning_byod_cross_profile_permission_control_info,
                R.string.provisioning_byod_cross_profile_permission_control_instruction);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mDpm = getSystemService(DevicePolicyManager.class);
        mPrepareTestButton.setText(
                R.string.provisioning_byod_cross_profile_permission_control_prepare_button);
        mPrepareTestButton.setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View v) {
                    whitelistTestApp();
                }
            });
    }

    protected void whitelistTestApp() {
        mDpm.setCrossProfilePackages(getAdminComponent(), Set.of(TEST_APP_PACKAGE_NAME));
    }

    protected ComponentName getAdminComponent() {
        return DeviceAdminTestReceiver.getReceiverComponentName();
    }

    protected void setupInteractAcrossProfilesDisabledByDefault(ArrayTestListAdapter adapter) {
        adapter.add(new DialogTestListItem(
                this, R.string.provisioning_byod_cross_profile_permission_disabled_by_default,
                getTestIdPrefix() + "interactAcrossProfilesDisabledByDefault",
                R.string.provisioning_byod_cross_profile_permission_disabled_by_default_instruction,
                new Intent(Settings.ACTION_MANAGE_CROSS_PROFILE_ACCESS)));
    }

    protected void setupInteractAcrossProfilesEnabled(ArrayTestListAdapter adapter) {
        adapter.add(new DialogTestListItem(
                this, R.string.provisioning_byod_cross_profile_permission_enabled,
                getTestIdPrefix() + "interactAcrossProfilesEnabled",
                R.string.provisioning_byod_cross_profile_permission_enabled_instruction,
                new Intent(OPEN_TEST_APP_ACTION)));
    }

    protected void setupInteractAcrossProfilesDisabled(ArrayTestListAdapter adapter) {
        adapter.add(new DialogTestListItem(this,
                R.string.provisioning_byod_cross_profile_permission_disabled,
                getTestIdPrefix() + "DisableUnredactedNotifications",
                R.string.provisioning_byod_cross_profile_permission_disabled_instruction,
                new Intent(OPEN_TEST_APP_ACTION)));
    }

    protected String getTestIdPrefix() {
        return "BYOD_";
    }

    @Override
    protected void setupTests(ArrayTestListAdapter adapter) {
        setupInteractAcrossProfilesDisabledByDefault(adapter);
        setupInteractAcrossProfilesEnabled(adapter);
        setupInteractAcrossProfilesDisabled(adapter);
    }
}
