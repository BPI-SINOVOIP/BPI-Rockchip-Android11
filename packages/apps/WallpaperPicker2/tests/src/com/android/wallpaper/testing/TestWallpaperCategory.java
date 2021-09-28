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


import com.android.wallpaper.model.WallpaperCategory;
import com.android.wallpaper.model.WallpaperInfo;
import com.android.wallpaper.model.WallpaperRotationInitializer;
import com.android.wallpaper.model.WallpaperRotationInitializer.RotationInitializationState;

import java.util.List;

/**
 * Test-only subclass of {@link WallpaperCategory} which can be configured to provide a test double
 * {@link WallpaperRotationInitializer}.
 */
public class TestWallpaperCategory extends WallpaperCategory {
    private boolean mIsRotationEnabled;
    private TestWallpaperRotationInitializer mWallpaperRotationInitializer;

    public TestWallpaperCategory(String title, String collectionId, List<WallpaperInfo> wallpapers,
            int priority) {
        super(title, collectionId, wallpapers, priority);
        mIsRotationEnabled = false;
        mWallpaperRotationInitializer = new TestWallpaperRotationInitializer();
    }

    @Override
    public WallpaperRotationInitializer getWallpaperRotationInitializer() {
        return (mIsRotationEnabled) ? mWallpaperRotationInitializer : null;
    }

    @Override
    public List<WallpaperInfo> getMutableWallpapers() {
        return super.getMutableWallpapers();
    }

    /** Sets whether rotation is enabled on this category. */
    public void setIsRotationEnabled(boolean isRotationEnabled) {
        mIsRotationEnabled = isRotationEnabled;
    }

    public void setRotationInitializationState(@RotationInitializationState int rotationState) {
        mWallpaperRotationInitializer.setRotationInitializationState(rotationState);
    }
}
