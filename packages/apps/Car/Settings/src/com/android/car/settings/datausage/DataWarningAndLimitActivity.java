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

import android.net.NetworkPolicyManager;
import android.net.NetworkTemplate;

import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

import com.android.car.settings.common.BaseCarSettingsActivity;

/** Separate activity to enter the data warning and limit settings via activity name. */
public class DataWarningAndLimitActivity extends BaseCarSettingsActivity {

    @Nullable
    @Override
    protected Fragment getInitialFragment() {
        NetworkTemplate template = getIntent().getParcelableExtra(
                NetworkPolicyManager.EXTRA_NETWORK_TEMPLATE);
        return DataWarningAndLimitFragment.newInstance(template);
    }
}
