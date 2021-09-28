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
package com.android.wallpaper.testing;

import android.content.res.Resources;

import com.android.wallpaper.module.PartnerProvider;

import java.io.File;

/**
 * Test implementation for PartnerProvider.
 */
public class TestPartnerProvider implements PartnerProvider {
    private File mLegacyWallpaperDirectory;

    @Override
    public Resources getResources() {
        return null;
    }

    @Override
    public File getLegacyWallpaperDirectory() {
        return mLegacyWallpaperDirectory;
    }

    /**
     * Sets the File to be returned by subsequent calls to getLegacyWallpaperDirectory().
     *
     * @param dir The legacy wallpaper directory.
     */
    public void setLegacyWallpaperDirectory(File dir) {
        mLegacyWallpaperDirectory = dir;
    }

    @Override
    public String getPackageName() {
        return null;
    }

    @Override
    public boolean shouldHideDefaultWallpaper() {
        return false;
    }
}
