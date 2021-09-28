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
 * limitations under the License
 */

package com.android.tv.common.feature;

import android.content.Context;

import com.android.tv.common.dev.DeveloperPreference;

/** A {@link Feature} based on {@link DeveloperPreference<Boolean>}. */
public class DeveloperPreferenceFeature implements Feature {

    private final DeveloperPreference<Boolean> mPreference;

    /**
     * Create a developer preference feature.
     *
     * @param key the developer setting key.
     * @param defaultValue the value to return if the setting is undefined or empty.
     */
    public static DeveloperPreferenceFeature create(String key, boolean defaultValue) {
        return from(DeveloperPreference.create(key, defaultValue));
    }

    /**
     * Create a developer preference feature from an exiting {@link DeveloperPreference<Boolean>}.
     */
    public static DeveloperPreferenceFeature from(
            DeveloperPreference<Boolean> developerPreference) {
        return new DeveloperPreferenceFeature(developerPreference);
    }

    private DeveloperPreferenceFeature(DeveloperPreference<Boolean> mPreference) {
        this.mPreference = mPreference;
    }

    @Override
    public boolean isEnabled(Context context) {
        return mPreference.get(context);
    }

    @Override
    public String toString() {
        return mPreference.toString();
    }
}
