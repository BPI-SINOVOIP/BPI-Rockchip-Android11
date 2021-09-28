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
package com.android.customization.model.theme;

import static com.android.customization.model.ResourceConstants.OVERLAY_CATEGORY_COLOR;
import static com.android.customization.model.ResourceConstants.OVERLAY_CATEGORY_FONT;
import static com.android.customization.model.ResourceConstants.OVERLAY_CATEGORY_ICON_ANDROID;
import static com.android.customization.model.ResourceConstants.OVERLAY_CATEGORY_ICON_LAUNCHER;
import static com.android.customization.model.ResourceConstants.OVERLAY_CATEGORY_ICON_SETTINGS;
import static com.android.customization.model.ResourceConstants.OVERLAY_CATEGORY_ICON_SYSUI;
import static com.android.customization.model.ResourceConstants.OVERLAY_CATEGORY_ICON_THEMEPICKER;
import static com.android.customization.model.ResourceConstants.OVERLAY_CATEGORY_SHAPE;

import android.provider.Settings;
import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.fragment.app.FragmentActivity;

import com.android.customization.model.CustomizationManager;
import com.android.customization.model.ResourceConstants;
import com.android.customization.model.theme.custom.CustomTheme;
import com.android.customization.module.ThemesUserEventLogger;

import org.json.JSONObject;

import java.util.HashSet;
import java.util.Map;
import java.util.Set;

public class ThemeManager implements CustomizationManager<ThemeBundle> {

    private static final Set<String> THEME_CATEGORIES = new HashSet<>();
    static {
        THEME_CATEGORIES.add(OVERLAY_CATEGORY_COLOR);
        THEME_CATEGORIES.add(OVERLAY_CATEGORY_FONT);
        THEME_CATEGORIES.add(OVERLAY_CATEGORY_SHAPE);
        THEME_CATEGORIES.add(OVERLAY_CATEGORY_ICON_ANDROID);
        THEME_CATEGORIES.add(OVERLAY_CATEGORY_ICON_SETTINGS);
        THEME_CATEGORIES.add(OVERLAY_CATEGORY_ICON_SYSUI);
        THEME_CATEGORIES.add(OVERLAY_CATEGORY_ICON_LAUNCHER);
        THEME_CATEGORIES.add(OVERLAY_CATEGORY_ICON_THEMEPICKER);
    }

    private final ThemeBundleProvider mProvider;
    private final OverlayManagerCompat mOverlayManagerCompat;

    protected final FragmentActivity mActivity;
    private final ThemesUserEventLogger mEventLogger;

    private Map<String, String> mCurrentOverlays;

    public ThemeManager(ThemeBundleProvider provider, FragmentActivity activity,
            OverlayManagerCompat overlayManagerCompat,
            ThemesUserEventLogger logger) {
        mProvider = provider;
        mActivity = activity;
        mOverlayManagerCompat = overlayManagerCompat;
        mEventLogger = logger;
    }

    @Override
    public boolean isAvailable() {
        return mOverlayManagerCompat.isAvailable() && mProvider.isAvailable();
    }

    @Override
    public void apply(ThemeBundle theme, Callback callback) {
        applyOverlays(theme, callback);
    }

    private void applyOverlays(ThemeBundle theme, Callback callback) {
        boolean allApplied = Settings.Secure.putString(mActivity.getContentResolver(),
                ResourceConstants.THEME_SETTING, theme.getSerializedPackagesWithTimestamp());
        if (theme instanceof CustomTheme) {
            storeCustomTheme((CustomTheme) theme);
        }
        mCurrentOverlays = null;
        if (allApplied) {
            mEventLogger.logThemeApplied(theme, theme instanceof CustomTheme);
            callback.onSuccess();
        } else {
            callback.onError(null);
        }
    }

    private void storeCustomTheme(CustomTheme theme) {
        mProvider.storeCustomTheme(theme);
    }

    @Override
    public void fetchOptions(OptionsFetchedListener<ThemeBundle> callback, boolean reload) {
        mProvider.fetch(callback, reload);
    }

    public Map<String, String> getCurrentOverlays() {
        if (mCurrentOverlays == null) {
            mCurrentOverlays = mOverlayManagerCompat.getEnabledOverlaysForTargets(
                    ResourceConstants.getPackagesToOverlay(mActivity));
            mCurrentOverlays.entrySet().removeIf(
                    categoryAndPackage -> !THEME_CATEGORIES.contains(categoryAndPackage.getKey()));
        }
        return mCurrentOverlays;
    }

    public String getStoredOverlays() {
        return Settings.Secure.getString(mActivity.getContentResolver(),
                ResourceConstants.THEME_SETTING);
    }

    public void removeCustomTheme(CustomTheme theme) {
        mProvider.removeCustomTheme(theme);
    }

    /**
     * @return an existing ThemeBundle that matches the same packages as the given one, if one
     * exists, or {@code null} otherwise.
     */
    @Nullable
    public ThemeBundle findThemeByPackages(ThemeBundle other) {
        return mProvider.findEquivalent(other);
    }

    /**
     * Store empty theme if no theme has been set yet. This will prevent Settings from showing the
     * suggestion to select a theme
     */
    public void storeEmptyTheme() {
        String themeSetting = Settings.Secure.getString(mActivity.getContentResolver(),
                ResourceConstants.THEME_SETTING);
        if (TextUtils.isEmpty(themeSetting)) {
            Settings.Secure.putString(mActivity.getContentResolver(),
                    ResourceConstants.THEME_SETTING, new JSONObject().toString());
        }
    }
}
