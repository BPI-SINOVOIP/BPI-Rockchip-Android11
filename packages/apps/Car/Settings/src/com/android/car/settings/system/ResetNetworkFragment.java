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

package com.android.car.settings.system;

import static android.app.Activity.RESULT_OK;

import android.content.Intent;
import android.os.Bundle;

import androidx.annotation.XmlRes;

import com.android.car.settings.R;
import com.android.car.settings.common.SettingsFragment;
import com.android.car.settings.security.CheckLockActivity;
import com.android.car.ui.toolbar.MenuItem;

import java.util.Collections;
import java.util.List;

/**
 * Presents the user with information about restoring network settings to the factory default
 * values. If a user confirms, they will first be required to authenticate then presented with a
 * secondary confirmation: {@link ResetNetworkConfirmFragment}.
 */
public class ResetNetworkFragment extends SettingsFragment {

    // Arbitrary request code for starting CheckLockActivity when the reset button is clicked.
    private static final int REQUEST_CODE = 123;

    private MenuItem mResetButton;

    @Override
    @XmlRes
    protected int getPreferenceScreenResId() {
        return R.xml.reset_network_fragment;
    }

    @Override
    public List<MenuItem> getToolbarMenuItems() {
        return Collections.singletonList(mResetButton);
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mResetButton = new MenuItem.Builder(getContext())
                .setTitle(R.string.reset_network_button_text)
                .setOnClickListener(i -> startActivityForResult(
                        new Intent(getContext(), CheckLockActivity.class), REQUEST_CODE))
                .build();
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == REQUEST_CODE && resultCode == RESULT_OK) {
            launchFragment(new ResetNetworkConfirmFragment());
        }
    }
}
