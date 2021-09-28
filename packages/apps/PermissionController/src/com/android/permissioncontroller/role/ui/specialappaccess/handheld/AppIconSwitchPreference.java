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

package com.android.permissioncontroller.role.ui.specialappaccess.handheld;

import android.content.Context;
import android.util.AttributeSet;

import androidx.annotation.AttrRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.StyleRes;
import androidx.preference.PreferenceViewHolder;
import androidx.preference.SwitchPreference;

import com.android.permissioncontroller.role.ui.handheld.AppIconPreference;

/**
 * {@link SwitchPreference} with {@link AppIconPreference.Mixin}.
 */
class AppIconSwitchPreference extends SwitchPreference {

    private AppIconPreference.Mixin mMixin;

    AppIconSwitchPreference(@NonNull Context context, @Nullable AttributeSet attrs,
            @AttrRes int defStyleAttr, @StyleRes int defStyleRes) {
        super(context, attrs, defStyleAttr, defStyleRes);

        init();
    }

    AppIconSwitchPreference(@NonNull Context context, @Nullable AttributeSet attrs,
            @AttrRes int defStyleAttr) {
        super(context, attrs, defStyleAttr);

        init();
    }

    AppIconSwitchPreference(@NonNull Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);

        init();
    }

    AppIconSwitchPreference(@NonNull Context context) {
        super(context);

        init();
    }

    private void init() {
        mMixin = new AppIconPreference.Mixin(getContext());
    }

    @Override
    public void onBindViewHolder(@NonNull PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        mMixin.onBindViewHolder(holder);
    }
}
