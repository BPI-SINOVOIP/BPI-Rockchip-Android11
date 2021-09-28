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
package com.android.wallpaper.model;

import static android.Manifest.permission.BIND_WALLPAPER;
import static android.content.pm.PackageManager.PERMISSION_GRANTED;

import android.content.Context;
import android.content.Intent;

import androidx.core.content.ContextCompat;

/**
 * Factory for getting an intent to show the in-app (inline) preview activity for a given
 * wallpaper, if appropriate for that wallpaper.
 */
public interface InlinePreviewIntentFactory {

    /**
     * @return whether it's possible to use the internal live picker, or {@code false} to use the
     * the Framework Activity.
     */
    default boolean shouldUseInternalLivePicker(Context context) {
        return ContextCompat.checkSelfPermission(context, BIND_WALLPAPER) == PERMISSION_GRANTED;
    }

    /**
     * Gets an intent to show the preview activity for the given wallpaper.
     *
     * @param ctx
     * @param wallpaper
     * @return Intent to show the inline preview activity.
     */
    Intent newIntent(Context ctx, WallpaperInfo wallpaper);
}
