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
package com.android.customization.model.grid;

import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.content.res.Resources;
import android.database.Cursor;
import android.os.Bundle;
import android.view.SurfaceView;

import androidx.annotation.Nullable;
import androidx.annotation.WorkerThread;

import com.android.customization.model.ResourceConstants;
import com.android.wallpaper.R;
import com.android.wallpaper.util.PreviewUtils;

import com.bumptech.glide.Glide;

import java.util.ArrayList;
import java.util.List;

/**
 * Abstracts the logic to retrieve available grid options from the current Launcher.
 */
public class LauncherGridOptionsProvider {

    private static final String LIST_OPTIONS = "list_options";
    private static final String PREVIEW = "preview";
    private static final String DEFAULT_GRID = "default_grid";

    private static final String COL_NAME = "name";
    private static final String COL_ROWS = "rows";
    private static final String COL_COLS = "cols";
    private static final String COL_PREVIEW_COUNT = "preview_count";
    private static final String COL_IS_DEFAULT = "is_default";

    // Normal gird size name
    private static final String GRID_NAME_NORMAL = "normal";

    private static final String METADATA_KEY_PREVIEW_VERSION = "preview_version";

    private final Context mContext;
    private final PreviewUtils mPreviewUtils;
    private List<GridOption> mOptions;

    public LauncherGridOptionsProvider(Context context, String authorityMetadataKey) {
        mPreviewUtils = new PreviewUtils(context, authorityMetadataKey);
        mContext = context;
    }

    boolean areGridsAvailable() {
        return mPreviewUtils.supportsPreview();
    }

    /**
     * Retrieve the available grids.
     * @param reload whether to reload grid options if they're cached.
     */
    @WorkerThread
    @Nullable
    List<GridOption> fetch(boolean reload) {
        if (!areGridsAvailable()) {
            return null;
        }
        if (mOptions != null && !reload) {
            return mOptions;
        }
        ContentResolver resolver = mContext.getContentResolver();
        String iconPath = mContext.getResources().getString(Resources.getSystem().getIdentifier(
                ResourceConstants.CONFIG_ICON_MASK, "string", ResourceConstants.ANDROID_PACKAGE));
        try (Cursor c = resolver.query(mPreviewUtils.getUri(LIST_OPTIONS), null, null, null,
                null)) {
            mOptions = new ArrayList<>();
            while(c.moveToNext()) {
                String name = c.getString(c.getColumnIndex(COL_NAME));
                int rows = c.getInt(c.getColumnIndex(COL_ROWS));
                int cols = c.getInt(c.getColumnIndex(COL_COLS));
                int previewCount = c.getInt(c.getColumnIndex(COL_PREVIEW_COUNT));
                boolean isSet = Boolean.valueOf(c.getString(c.getColumnIndex(COL_IS_DEFAULT)));
                String title = GRID_NAME_NORMAL.equals(name)
                        ? mContext.getString(R.string.default_theme_title)
                        : mContext.getString(R.string.grid_title_pattern, cols, rows);
                mOptions.add(new GridOption(title, name, isSet, rows, cols,
                        mPreviewUtils.getUri(PREVIEW), previewCount, iconPath));
            }
            Glide.get(mContext).clearDiskCache();
        } catch (Exception e) {
            mOptions = null;
        }
        return mOptions;
    }

    /**
     * Request rendering of home screen preview via Launcher to Wallpaper using SurfaceView
     * @param name      the grid option name
     * @param bundle    surface view request bundle generated from
     *    {@link com.android.wallpaper.util.SurfaceViewUtils#createSurfaceViewRequest(SurfaceView)}.
     */
    Bundle renderPreview(String name, Bundle bundle) {
        bundle.putString("name", name);
        return mPreviewUtils.renderPreview(bundle);
    }

    int applyGrid(String name) {
        ContentValues values = new ContentValues();
        values.put("name", name);
        return mContext.getContentResolver().update(mPreviewUtils.getUri(DEFAULT_GRID), values,
                null, null);
    }
}
