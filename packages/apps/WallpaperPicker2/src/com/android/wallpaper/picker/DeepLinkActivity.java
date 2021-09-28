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
package com.android.wallpaper.picker;

import android.app.Activity;
import android.os.Bundle;

import androidx.annotation.Nullable;

import com.android.wallpaper.module.InjectorProvider;

/**
 * An intermediate activity to redirect to a brand new target activity when the user clicks
 * the url link to deep link.
 */
public class DeepLinkActivity extends Activity {
    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        startActivity(InjectorProvider.getInjector().getDeepLinkRedirectIntent(
                this, getIntent().getData()));
        finish();
    }
}
