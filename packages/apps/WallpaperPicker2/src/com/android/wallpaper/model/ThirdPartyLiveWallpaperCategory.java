/*
 * Copyright (C) 2018 The Android Open Source Project
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

import android.content.Context;
import android.os.AsyncTask;

import androidx.annotation.Nullable;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;

/**
 * Category listing third party live wallpapers the user might have installed.
 */
public class ThirdPartyLiveWallpaperCategory extends WallpaperCategory {

    private final Set<String> mExcludedPackages;

    public ThirdPartyLiveWallpaperCategory(String title, String collectionId,
            List<WallpaperInfo> wallpapers,
            int priority, @Nullable Set<String> excludedLiveWallpaperPackageNames) {
        super(title, collectionId, wallpapers, priority);
        mExcludedPackages = excludedLiveWallpaperPackageNames;
    }

    @Override
    public void fetchWallpapers(Context context, WallpaperReceiver receiver, boolean forceReload) {
        if (forceReload) {
            FetchLiveWallpapersTask task = new FetchLiveWallpapersTask(context,
                    getMutableWallpapers(), mExcludedPackages, receiver);
            task.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);

        } else {
            super.fetchWallpapers(context, receiver, forceReload);
        }
    }

    @Override
    public boolean supportsThirdParty() {
        return true;
    }

    @Override
    public boolean containsThirdParty(String packageName) {
        if (!supportsThirdParty()) return false;
        synchronized (mWallpapersLock) {
            for (WallpaperInfo wallpaper : getMutableWallpapers()) {
                android.app.WallpaperInfo wallpaperComponent = wallpaper.getWallpaperComponent();
                if (wallpaperComponent != null
                        && wallpaperComponent.getPackageName().equals(packageName)) {
                    return true;
                }
            }
        }
        return super.containsThirdParty(packageName);
    }

    private class FetchLiveWallpapersTask extends AsyncTask<Void, Void, Void> {

        private final Context mContext;
        private final List<WallpaperInfo> mCategoryWallpapers;
        @Nullable private final Set<String> mExcludedPackages;
        @Nullable private final WallpaperReceiver mReceiver;

        FetchLiveWallpapersTask(Context context, List<WallpaperInfo> wallpapers,
                @Nullable Set<String> excludedPackages, @Nullable WallpaperReceiver receiver) {
            mContext = context;
            mCategoryWallpapers = wallpapers;
            mExcludedPackages = excludedPackages;
            mReceiver = receiver;
        }

        @Override
        protected Void doInBackground(Void... voids) {
            List<WallpaperInfo> liveWallpapers = LiveWallpaperInfo.getAll(mContext,
                    mExcludedPackages);
            synchronized (mWallpapersLock) {
                mCategoryWallpapers.clear();
                mCategoryWallpapers.addAll(liveWallpapers);
            }
            return null;
        }

        @Override
        protected void onPostExecute(Void aVoid) {
            // Perform a shallow clone so as not to pass the reference to the list along to clients.
            mReceiver.onWallpapersReceived(new ArrayList<>(mCategoryWallpapers));
        }
    }
}
