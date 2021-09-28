/*
 * Copyright (C) 2017 The Android Open Source Project
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

import android.content.pm.ActivityInfo;
import android.graphics.PixelFormat;
import android.os.Bundle;

import androidx.annotation.Nullable;

import com.android.wallpaper.R;

/**
 * Abstract base class for a wallpaper full-screen preview activity.
 */
public abstract class BasePreviewActivity extends BaseActivity {
    public static final String EXTRA_WALLPAPER_INFO =
            "com.android.wallpaper.picker.wallpaper_info";
    public static final String EXTRA_VIEW_AS_HODE =
            "com.android.wallpaper.picker.view_as_home";
    public static final String EXTRA_TESTING_MODE_ENABLED =
            "com.android.wallpaper.picker.testing_mode_enabled";

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().setColorMode(ActivityInfo.COLOR_MODE_WIDE_COLOR_GAMUT);
        setTheme(R.style.WallpaperTheme);
        getWindow().setFormat(PixelFormat.TRANSLUCENT);
    }
}
