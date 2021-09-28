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

package com.android.car.settings.accounts;

import android.content.Context;
import android.content.Intent;
import android.provider.Settings;

import androidx.annotation.XmlRes;

import com.android.car.settings.R;
import com.android.car.settings.common.SettingsFragment;

import java.util.Arrays;
import java.util.HashSet;

/**
 * Lists accounts the user can add.
 */
public class ChooseAccountFragment extends SettingsFragment {
    @Override
    @XmlRes
    protected int getPreferenceScreenResId() {
        return R.xml.choose_account_fragment;
    }

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        Intent intent = requireActivity().getIntent();
        ChooseAccountPreferenceController preferenceController = use(
                ChooseAccountPreferenceController.class, R.string.pk_add_account);

        String[] authorities = intent.getStringArrayExtra(Settings.EXTRA_AUTHORITIES);
        if (authorities != null) {
            preferenceController.setAuthorities(Arrays.asList(authorities));
        }

        String[] accountTypesForFilter = intent.getStringArrayExtra(Settings.EXTRA_ACCOUNT_TYPES);
        if (accountTypesForFilter != null) {
            preferenceController.setAccountTypesFilter(
                    new HashSet<>(Arrays.asList(accountTypesForFilter)));
        }
    }
}
