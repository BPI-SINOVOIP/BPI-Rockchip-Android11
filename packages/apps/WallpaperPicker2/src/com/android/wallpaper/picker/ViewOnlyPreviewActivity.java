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

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;

import com.android.wallpaper.R;
import com.android.wallpaper.model.InlinePreviewIntentFactory;
import com.android.wallpaper.model.WallpaperInfo;
import com.android.wallpaper.module.InjectorProvider;

/**
 * Activity that displays a view-only preview of a specific wallpaper.
 */
public class ViewOnlyPreviewActivity extends BasePreviewActivity {

    /**
     * Returns a new Intent with the provided WallpaperInfo instance put as an extra.
     */
    public static Intent newIntent(Context context, WallpaperInfo wallpaper) {
        return new Intent(context, ViewOnlyPreviewActivity.class)
                .putExtra(EXTRA_WALLPAPER_INFO, wallpaper);
    }

    protected static Intent newIntent(Context context, WallpaperInfo wallpaper,
            boolean isVewAsHome) {
        return newIntent(context, wallpaper).putExtra(EXTRA_VIEW_AS_HODE, isVewAsHome);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_preview);
    }

    @Override
    public void onAttachedToWindow() {
        super.onAttachedToWindow();

        FragmentManager fm = getSupportFragmentManager();
        Fragment fragment = fm.findFragmentById(R.id.fragment_container);

        if (fragment == null) {
            Intent intent = getIntent();
            WallpaperInfo wallpaper = intent.getParcelableExtra(EXTRA_WALLPAPER_INFO);
            boolean testingModeEnabled = intent.getBooleanExtra(EXTRA_TESTING_MODE_ENABLED, false);
            boolean viewAsHome = intent.getBooleanExtra(EXTRA_VIEW_AS_HODE, true);
            fragment = InjectorProvider.getInjector().getPreviewFragment(
                    /* context */ this,
                    wallpaper,
                    PreviewFragment.MODE_VIEW_ONLY,
                    viewAsHome,
                    testingModeEnabled);
            fm.beginTransaction()
                    .add(R.id.fragment_container, fragment)
                    .commit();
        }
    }

    /**
     * Implementation that provides an intent to start a PreviewActivity.
     */
    public static class ViewOnlyPreviewActivityIntentFactory implements InlinePreviewIntentFactory {
        private boolean mIsHomeAndLockPreviews;
        private boolean mIsViewAsHome;

        @Override
        public Intent newIntent(Context context, WallpaperInfo wallpaper) {
            if (mIsHomeAndLockPreviews) {
                return ViewOnlyPreviewActivity.newIntent(context, wallpaper, mIsViewAsHome);
            }
            return ViewOnlyPreviewActivity.newIntent(context, wallpaper);
        }

        protected void setAsHomePreview(boolean isHomeAndLockPreview, boolean isViewAsHome) {
            mIsHomeAndLockPreviews = isHomeAndLockPreview;
            mIsViewAsHome = isViewAsHome;
        }
    }
}
