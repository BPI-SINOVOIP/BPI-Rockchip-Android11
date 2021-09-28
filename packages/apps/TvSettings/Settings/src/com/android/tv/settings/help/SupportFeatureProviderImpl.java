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

package com.android.tv.settings.help;

import android.app.Activity;
import android.content.Intent;

/**
 * Implementation for {@link SupportFeatureProvider}.
 */
public class SupportFeatureProviderImpl implements SupportFeatureProvider {

    public SupportFeatureProviderImpl() {
    }

    @Override
    public void startSupport(Activity activity) {
        if (activity != null) {
            final Intent intent = new Intent("com.android.settings.action.LAUNCH_HELP");
            activity.startActivityForResult(intent, 0);
        }
    }
}
