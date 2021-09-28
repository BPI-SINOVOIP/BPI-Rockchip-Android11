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

package com.android.car.settings.common;

import static com.android.settingslib.drawer.CategoryKey.CATEGORY_DEVICE;
import static com.android.settingslib.drawer.TileUtils.META_DATA_PREFERENCE_ICON;
import static com.android.settingslib.drawer.TileUtils.META_DATA_PREFERENCE_ICON_URI;
import static com.android.settingslib.drawer.TileUtils.META_DATA_PREFERENCE_KEYHINT;
import static com.android.settingslib.drawer.TileUtils.META_DATA_PREFERENCE_SUMMARY;
import static com.android.settingslib.drawer.TileUtils.META_DATA_PREFERENCE_SUMMARY_URI;
import static com.android.settingslib.drawer.TileUtils.META_DATA_PREFERENCE_TITLE;
import static com.android.settingslib.drawer.TileUtils.META_DATA_PREFERENCE_TITLE_URI;

import android.app.ActivityManager;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;

import com.android.car.apps.common.util.Themes;
import com.android.car.settings.R;
import com.android.car.ui.preference.CarUiPreference;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Loads Activity with TileUtils.EXTRA_SETTINGS_ACTION.
 */
// TODO: investigate using SettingsLib Tiles.
public class ExtraSettingsLoader {
    private static final Logger LOG = new Logger(ExtraSettingsLoader.class);
    private static final String META_DATA_PREFERENCE_CATEGORY = "com.android.settings.category";
    private final Context mContext;
    private Map<Preference, Bundle> mPreferenceBundleMap;
    private PackageManager mPm;

    public ExtraSettingsLoader(Context context) {
        mContext = context;
        mPm = context.getPackageManager();
        mPreferenceBundleMap = new HashMap<>();
    }

    @VisibleForTesting
    void setPackageManager(PackageManager pm) {
        mPm = pm;
    }

    /**
     * Returns a map of {@link Preference} and {@link Bundle} representing settings injected from
     * system apps and their metadata. The given intent must specify the action to use for
     * resolving activities and a category with the key "com.android.settings.category" and one of
     * the values in {@link com.android.settingslib.drawer.CategoryKey}.
     *
     * @param intent intent specifying the extra settings category to load
     */
    public Map<Preference, Bundle> loadPreferences(Intent intent) {
        List<ResolveInfo> results = mPm.queryIntentActivitiesAsUser(intent,
                PackageManager.GET_META_DATA, ActivityManager.getCurrentUser());

        String extraCategory = intent.getStringExtra(META_DATA_PREFERENCE_CATEGORY);
        for (ResolveInfo resolved : results) {
            if (!resolved.system) {
                // Do not allow any app to be added to settings, only system ones.
                continue;
            }
            String key = null;
            String title = null;
            String summary = null;
            String category = null;
            ActivityInfo activityInfo = resolved.activityInfo;
            Bundle metaData = activityInfo.metaData;
            try {
                Resources res = mPm.getResourcesForApplication(activityInfo.packageName);
                if (metaData.containsKey(META_DATA_PREFERENCE_KEYHINT)) {
                    key = extractMetaDataString(metaData, META_DATA_PREFERENCE_KEYHINT, res);
                }
                if (!metaData.containsKey(META_DATA_PREFERENCE_TITLE_URI)) {
                    title = extractMetaDataString(metaData, META_DATA_PREFERENCE_TITLE, res);
                    if (TextUtils.isEmpty(title)) {
                        LOG.d("no title.");
                        title = activityInfo.loadLabel(mPm).toString();
                    }
                }
                if (!metaData.containsKey(META_DATA_PREFERENCE_SUMMARY_URI)) {
                    summary = extractMetaDataString(metaData, META_DATA_PREFERENCE_SUMMARY, res);
                    if (TextUtils.isEmpty(summary)) {
                        LOG.d("no description.");
                    }
                }
                category = extractMetaDataString(metaData, META_DATA_PREFERENCE_CATEGORY, res);
                if (TextUtils.isEmpty(category)) {
                    LOG.d("no category.");
                }
            } catch (PackageManager.NameNotFoundException | Resources.NotFoundException e) {
                LOG.d("Couldn't find info", e);
            }
            Drawable icon = null;
            if (metaData.containsKey(META_DATA_PREFERENCE_ICON)) {
                int iconRes = metaData.getInt(META_DATA_PREFERENCE_ICON);
                icon = ExtraSettingsUtil.loadDrawableFromPackage(mContext,
                        activityInfo.packageName, iconRes);
            } else if (!metaData.containsKey(META_DATA_PREFERENCE_ICON_URI)) {
                icon = mContext.getDrawable(R.drawable.ic_settings_gear);
                LOG.d("use default icon.");
            }
            Intent extraSettingIntent =
                    new Intent().setClassName(activityInfo.packageName, activityInfo.name);
            if (category == null) {
                // If category is not specified or not supported, default to device.
                category = CATEGORY_DEVICE;
            }

            if (!TextUtils.equals(extraCategory, category)) {
                continue;
            }
            CarUiPreference preference = new CarUiPreference(mContext);
            preference.setTitle(title);
            preference.setSummary(summary);
            if (key != null) {
                preference.setKey(key);
            }
            if (icon != null) {
                preference.setIcon(icon);
                if (ExtraSettingsUtil.isIconTintable(metaData)) {
                    preference.getIcon().setTintList(
                            Themes.getAttrColorStateList(mContext, R.attr.iconColor));
                }
            }
            preference.setIntent(extraSettingIntent);
            mPreferenceBundleMap.put(preference, metaData);
        }
        return mPreferenceBundleMap;
    }

    /**
     * Extracts the value in the metadata specified by the key.
     * If it is resource, resolve the string and return. Otherwise, return the string itself.
     */
    private String extractMetaDataString(Bundle metaData, String key, Resources res) {
        if (metaData.containsKey(key)) {
            if (metaData.get(key) instanceof Integer) {
                return res.getString(metaData.getInt(key));
            }
            return metaData.getString(key);
        }
        return null;
    }
}
