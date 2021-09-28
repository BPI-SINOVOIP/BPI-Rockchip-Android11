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

import androidx.annotation.Nullable;

/**
 * Interface for a presenter class which displays the currently set wallpaper(s) in the view.
 */
public interface CurrentWallpaperBottomSheetPresenter {
    /**
     * Sets whether to show the currently set wallpaper(s) by expanding the BottomSheet (true) or to
     * hide them by collapsing the BottomSheet (false).
     */
    void setCurrentWallpapersExpanded(boolean expanded);

    /**
     * Refreshes the currently set wallpaper.
     */
    void refreshCurrentWallpapers(@Nullable RefreshListener refreshListener);

    /**
     * A listener which is notified when a refresh of the current wallpapers UI is completed.
     */
    interface RefreshListener {
        void onCurrentWallpaperRefreshed();
    }
}
