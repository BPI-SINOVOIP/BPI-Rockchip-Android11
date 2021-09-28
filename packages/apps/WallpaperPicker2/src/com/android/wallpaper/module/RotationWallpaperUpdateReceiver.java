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
package com.android.wallpaper.module;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;

import com.android.wallpaper.compat.BuildCompat;
import com.android.wallpaper.util.DiskBasedLogger;
import com.android.wallpaper.util.FileMover;

import java.io.File;

/**
 * Receiver to run when the app was updated or on first boot to switch from live rotating wallpaper
 * to static rotating wallpaper.
 */
public class RotationWallpaperUpdateReceiver extends BroadcastReceiver {


    // Earlier versions of rotating wallpaper save the current rotation image as a file.
    // We can infer from the extistance of this file whether or not user had rotating live wallpaper
    private static final String ROTATING_WALLPAPER_FILE_PATH = "rotating_wallpaper.jpg";
    private static final String TAG = "RotationWallpaperUpdateReceiver";

    @Override
    public void onReceive(Context context, Intent intent) {
        // This receiver is a no-op on pre-N Android and should only respond to a
        // MY_PACKAGE_REPLACED intent.
        if (intent.getAction() == null
                || !(intent.getAction().equals(Intent.ACTION_MY_PACKAGE_REPLACED)
                || intent.getAction().equals(Intent.ACTION_BOOT_COMPLETED))
                || !BuildCompat.isAtLeastN()) {
            DiskBasedLogger.e(
                    TAG,
                    "Unexpected action or Android version!",
                    context);
            throw new IllegalStateException(
                    "Unexpected broadcast action or unsupported Android version");
        }

        PendingResult broadcastResult = goAsync();
        new Thread(() -> {
            Context appContext = context.getApplicationContext();
            Context deviceProtectedContext = appContext.createDeviceProtectedStorageContext();

            if (context.getFileStreamPath(ROTATING_WALLPAPER_FILE_PATH).exists()) {
                moveFileToProtectedStorage(appContext, deviceProtectedContext);
            }

            File wallpaperFile = deviceProtectedContext.getFileStreamPath(
                    ROTATING_WALLPAPER_FILE_PATH);
            if (wallpaperFile.exists()) {
                switchToStaticWallpaper(appContext, wallpaperFile);
            }
            broadcastResult.finish();
        }).start();
    }

    private void moveFileToProtectedStorage(Context context, Context deviceProtectedContext) {
        try {
            FileMover.moveFileBetweenContexts(context, ROTATING_WALLPAPER_FILE_PATH,
                    deviceProtectedContext, ROTATING_WALLPAPER_FILE_PATH);
        } catch (Exception ex) {
            DiskBasedLogger.e(
                    TAG,
                    "Failed to move rotating wallpaper file to device protected storage: "
                            + ex.getMessage(),
                    context);
        }
    }

    private void switchToStaticWallpaper(Context appContext, File wallpaperFile) {
        try {
            Injector injector = InjectorProvider.getInjector();
            WallpaperPreferences wallpaperPreferences = injector.getPreferences(appContext);
            if (wallpaperPreferences.getWallpaperPresentationMode()
                    != WallpaperPreferences.PRESENTATION_MODE_ROTATING) {
                return;
            }
            Bitmap bitmap = BitmapFactory.decodeFile(wallpaperFile.getAbsolutePath());

            injector.getWallpaperPersister(appContext).setWallpaperInRotation(bitmap,
                    wallpaperPreferences.getHomeWallpaperAttributions(),
                    wallpaperPreferences.getHomeWallpaperActionLabelRes(),
                    wallpaperPreferences.getHomeWallpaperActionIconRes(),
                    wallpaperPreferences.getHomeWallpaperActionUrl(),
                    wallpaperPreferences.getHomeWallpaperCollectionId());
            wallpaperFile.delete();

        } catch (Exception ex) {
            DiskBasedLogger.e(TAG, "Unable to set static wallpaper", appContext);
        }
    }
}
