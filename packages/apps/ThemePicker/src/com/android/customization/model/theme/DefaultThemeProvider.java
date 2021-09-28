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

import static com.android.customization.model.ResourceConstants.ANDROID_PACKAGE;
import static com.android.customization.model.ResourceConstants.ICONS_FOR_PREVIEW;
import static com.android.customization.model.ResourceConstants.OVERLAY_CATEGORY_COLOR;
import static com.android.customization.model.ResourceConstants.OVERLAY_CATEGORY_FONT;
import static com.android.customization.model.ResourceConstants.OVERLAY_CATEGORY_ICON_ANDROID;
import static com.android.customization.model.ResourceConstants.OVERLAY_CATEGORY_ICON_LAUNCHER;
import static com.android.customization.model.ResourceConstants.OVERLAY_CATEGORY_ICON_SETTINGS;
import static com.android.customization.model.ResourceConstants.OVERLAY_CATEGORY_ICON_SYSUI;
import static com.android.customization.model.ResourceConstants.OVERLAY_CATEGORY_ICON_THEMEPICKER;
import static com.android.customization.model.ResourceConstants.OVERLAY_CATEGORY_SHAPE;
import static com.android.customization.model.ResourceConstants.SYSUI_PACKAGE;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.res.Resources.NotFoundException;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.util.Log;

import androidx.annotation.Nullable;

import com.android.customization.model.CustomizationManager.OptionsFetchedListener;
import com.android.customization.model.ResourcesApkProvider;
import com.android.customization.model.theme.ThemeBundle.Builder;
import com.android.customization.model.theme.ThemeBundle.PreviewInfo.ShapeAppIcon;
import com.android.customization.model.theme.custom.CustomTheme;
import com.android.customization.module.CustomizationPreferences;
import com.android.wallpaper.R;
import com.android.wallpaper.asset.ResourceAsset;

import com.bumptech.glide.request.RequestOptions;
import com.google.android.apps.wallpaper.asset.ThemeBundleThumbAsset;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

/**
 * Default implementation of {@link ThemeBundleProvider} that reads Themes' overlays from a stub APK.
 */
public class DefaultThemeProvider extends ResourcesApkProvider implements ThemeBundleProvider {

    private static final String TAG = "DefaultThemeProvider";

    private static final String THEMES_ARRAY = "themes";
    private static final String TITLE_PREFIX = "theme_title_";
    private static final String FONT_PREFIX = "theme_overlay_font_";
    private static final String COLOR_PREFIX = "theme_overlay_color_";
    private static final String SHAPE_PREFIX = "theme_overlay_shape_";
    private static final String ICON_ANDROID_PREFIX = "theme_overlay_icon_android_";
    private static final String ICON_LAUNCHER_PREFIX = "theme_overlay_icon_launcher_";
    private static final String ICON_THEMEPICKER_PREFIX = "theme_overlay_icon_themepicker_";
    private static final String ICON_SETTINGS_PREFIX = "theme_overlay_icon_settings_";
    private static final String ICON_SYSUI_PREFIX = "theme_overlay_icon_sysui_";

    private static final String DEFAULT_THEME_NAME= "default";
    private static final String THEME_TITLE_FIELD = "_theme_title";
    private static final String THEME_ID_FIELD = "_theme_id";

    private final OverlayThemeExtractor mOverlayProvider;
    private List<ThemeBundle> mThemes;
    private final CustomizationPreferences mCustomizationPreferences;

    public DefaultThemeProvider(Context context, CustomizationPreferences customizationPrefs) {
        super(context, context.getString(R.string.themes_stub_package));
        mOverlayProvider = new OverlayThemeExtractor(context);
        mCustomizationPreferences = customizationPrefs;
    }

    @Override
    public void fetch(OptionsFetchedListener<ThemeBundle> callback, boolean reload) {
        if (mThemes == null || reload) {
            mThemes = new ArrayList<>();
            loadAll();
        }

        if(callback != null) {
            callback.onOptionsLoaded(mThemes);
        }
    }

    @Override
    public boolean isAvailable() {
        return mOverlayProvider.isAvailable() && super.isAvailable();
    }

    private void loadAll() {
        // Add "Custom" option at the beginning.
        mThemes.add(new CustomTheme.Builder()
                .setId(CustomTheme.newId())
                .setTitle(mContext.getString(R.string.custom_theme))
                .build(mContext));

        addDefaultTheme();

        String[] themeNames = getItemsFromStub(THEMES_ARRAY);

        for (String themeName : themeNames) {
            // Default theme needs special treatment (see #addDefaultTheme())
            if (DEFAULT_THEME_NAME.equals(themeName)) {
                continue;
            }
            ThemeBundle.Builder builder = new Builder();
            try {
                builder.setTitle(mStubApkResources.getString(
                        mStubApkResources.getIdentifier(TITLE_PREFIX + themeName,
                                "string", mStubPackageName)));

                String shapeOverlayPackage = getOverlayPackage(SHAPE_PREFIX, themeName);
                mOverlayProvider.addShapeOverlay(builder, shapeOverlayPackage);

                String fontOverlayPackage = getOverlayPackage(FONT_PREFIX, themeName);
                mOverlayProvider.addFontOverlay(builder, fontOverlayPackage);

                String colorOverlayPackage = getOverlayPackage(COLOR_PREFIX, themeName);
                mOverlayProvider.addColorOverlay(builder, colorOverlayPackage);

                String iconAndroidOverlayPackage = getOverlayPackage(ICON_ANDROID_PREFIX,
                        themeName);

                mOverlayProvider.addAndroidIconOverlay(builder, iconAndroidOverlayPackage);

                String iconSysUiOverlayPackage = getOverlayPackage(ICON_SYSUI_PREFIX, themeName);

                mOverlayProvider.addSysUiIconOverlay(builder, iconSysUiOverlayPackage);

                String iconLauncherOverlayPackage = getOverlayPackage(ICON_LAUNCHER_PREFIX,
                        themeName);
                mOverlayProvider.addNoPreviewIconOverlay(builder, iconLauncherOverlayPackage);

                String iconThemePickerOverlayPackage = getOverlayPackage(ICON_THEMEPICKER_PREFIX,
                        themeName);
                mOverlayProvider.addNoPreviewIconOverlay(builder,
                        iconThemePickerOverlayPackage);

                String iconSettingsOverlayPackage = getOverlayPackage(ICON_SETTINGS_PREFIX,
                        themeName);

                mOverlayProvider.addNoPreviewIconOverlay(builder, iconSettingsOverlayPackage);

                mThemes.add(builder.build(mContext));
            } catch (NameNotFoundException | NotFoundException e) {
                Log.w(TAG, String.format("Couldn't load part of theme %s, will skip it", themeName),
                        e);
            }
        }

        addCustomThemes();
    }

    /**
     * Default theme requires different treatment: if there are overlay packages specified in the
     * stub apk, we'll use those, otherwise we'll get the System default values. But we cannot skip
     * the default theme.
     */
    private void addDefaultTheme() {
        ThemeBundle.Builder builder = new Builder().asDefault();

        int titleId = mStubApkResources.getIdentifier(TITLE_PREFIX + DEFAULT_THEME_NAME,
                "string", mStubPackageName);
        if (titleId > 0) {
            builder.setTitle(mStubApkResources.getString(titleId));
        } else {
            builder.setTitle(mContext.getString(R.string.default_theme_title));
        }

        try {
            String colorOverlayPackage = getOverlayPackage(COLOR_PREFIX, DEFAULT_THEME_NAME);
            mOverlayProvider.addColorOverlay(builder, colorOverlayPackage);
        } catch (NameNotFoundException | NotFoundException e) {
            Log.d(TAG, "Didn't find color overlay for default theme, will use system default");
            mOverlayProvider.addSystemDefaultColor(builder);
        }

        try {
            String fontOverlayPackage = getOverlayPackage(FONT_PREFIX, DEFAULT_THEME_NAME);
            mOverlayProvider.addFontOverlay(builder, fontOverlayPackage);
        } catch (NameNotFoundException | NotFoundException e) {
            Log.d(TAG, "Didn't find font overlay for default theme, will use system default");
            mOverlayProvider.addSystemDefaultFont(builder);
        }

        try {
            String shapeOverlayPackage = getOverlayPackage(SHAPE_PREFIX, DEFAULT_THEME_NAME);
            mOverlayProvider.addShapeOverlay(builder ,shapeOverlayPackage, false);
        } catch (NameNotFoundException | NotFoundException e) {
            Log.d(TAG, "Didn't find shape overlay for default theme, will use system default");
            mOverlayProvider.addSystemDefaultShape(builder);
        }

        List<ShapeAppIcon> icons = new ArrayList<>();
        for (String packageName : mOverlayProvider.getShapePreviewIconPackages()) {
            Drawable icon = null;
            CharSequence name = null;
            try {
                icon = mContext.getPackageManager().getApplicationIcon(packageName);
                ApplicationInfo appInfo = mContext.getPackageManager()
                        .getApplicationInfo(packageName, /* flag= */ 0);
                name = mContext.getPackageManager().getApplicationLabel(appInfo);
            } catch (NameNotFoundException e) {
                Log.d(TAG, "Couldn't find app " + packageName + ", won't use it for icon shape"
                        + "preview");
            } finally {
                if (icon != null && !TextUtils.isEmpty(name)) {
                    icons.add(new ShapeAppIcon(icon, name));
                }
            }
        }
        builder.setShapePreviewIcons(icons);

        try {
            String iconAndroidOverlayPackage = getOverlayPackage(ICON_ANDROID_PREFIX,
                    DEFAULT_THEME_NAME);
            mOverlayProvider.addAndroidIconOverlay(builder, iconAndroidOverlayPackage);
        } catch (NameNotFoundException | NotFoundException e) {
            Log.d(TAG, "Didn't find Android icons overlay for default theme, using system default");
            mOverlayProvider.addSystemDefaultIcons(builder, ANDROID_PACKAGE, ICONS_FOR_PREVIEW);
        }

        try {
            String iconSysUiOverlayPackage = getOverlayPackage(ICON_SYSUI_PREFIX,
                    DEFAULT_THEME_NAME);
            mOverlayProvider.addSysUiIconOverlay(builder, iconSysUiOverlayPackage);
        } catch (NameNotFoundException | NotFoundException e) {
            Log.d(TAG,
                    "Didn't find SystemUi icons overlay for default theme, using system default");
            mOverlayProvider.addSystemDefaultIcons(builder, SYSUI_PACKAGE, ICONS_FOR_PREVIEW);
        }

        mThemes.add(builder.build(mContext));
    }

    @Override
    public void storeCustomTheme(CustomTheme theme) {
        if (mThemes == null) {
            fetch(options -> {
                addCustomThemeAndStore(theme);
            }, false);
        } else {
            addCustomThemeAndStore(theme);
        }
    }

    private void addCustomThemeAndStore(CustomTheme theme) {
        if (!mThemes.contains(theme)) {
            mThemes.add(theme);
        } else {
            mThemes.replaceAll(t -> theme.equals(t) ? theme : t);
        }
        JSONArray themesArray = new JSONArray();
        mThemes.stream()
                .filter(themeBundle -> themeBundle instanceof CustomTheme
                        && !themeBundle.getPackagesByCategory().isEmpty())
                .forEachOrdered(themeBundle -> addThemeBundleToArray(themesArray, themeBundle));
        mCustomizationPreferences.storeCustomThemes(themesArray.toString());
    }

    private void addThemeBundleToArray(JSONArray themesArray, ThemeBundle themeBundle) {
        JSONObject jsonPackages = themeBundle.getJsonPackages(false);
        try {
            jsonPackages.put(THEME_TITLE_FIELD, themeBundle.getTitle());
            if (themeBundle instanceof CustomTheme) {
                jsonPackages.put(THEME_ID_FIELD, ((CustomTheme)themeBundle).getId());
            }
        } catch (JSONException e) {
            Log.w("Exception saving theme's title", e);
        }
        themesArray.put(jsonPackages);
    }

    @Override
    public void removeCustomTheme(CustomTheme theme) {
        JSONArray themesArray = new JSONArray();
        mThemes.stream()
                .filter(themeBundle -> themeBundle instanceof CustomTheme
                        && ((CustomTheme) themeBundle).isDefined())
                .forEachOrdered(customTheme -> {
                    if (!customTheme.equals(theme)) {
                        addThemeBundleToArray(themesArray, customTheme);
                    }
                });
        mCustomizationPreferences.storeCustomThemes(themesArray.toString());
    }

    private void addCustomThemes() {
        String serializedThemes = mCustomizationPreferences.getSerializedCustomThemes();
        int customThemesCount = 0;
        if (!TextUtils.isEmpty(serializedThemes)) {
            try {
                JSONArray customThemes = new JSONArray(serializedThemes);
                for (int i = 0; i < customThemes.length(); i++) {
                    JSONObject jsonTheme = customThemes.getJSONObject(i);
                    CustomTheme.Builder builder = new CustomTheme.Builder();
                    try {
                        convertJsonToBuilder(jsonTheme, builder);
                    } catch (NameNotFoundException | NotFoundException e) {
                        Log.i(TAG, "Couldn't parse serialized custom theme", e);
                        builder = null;
                    }
                    if (builder != null) {
                        if (TextUtils.isEmpty(builder.getTitle())) {
                            builder.setTitle(mContext.getString(R.string.custom_theme_title,
                                    customThemesCount + 1));
                        }
                        mThemes.add(builder.build(mContext));
                    } else {
                        Log.w(TAG, "Couldn't read stored custom theme, resetting");
                        mThemes.add(new CustomTheme.Builder()
                                .setId(CustomTheme.newId())
                                .setTitle(mContext.getString(
                                        R.string.custom_theme_title, customThemesCount + 1))
                                .build(mContext));
                    }
                    customThemesCount++;
                }
            } catch (JSONException e) {
                Log.w(TAG, "Couldn't read stored custom theme, resetting", e);
                mThemes.add(new CustomTheme.Builder()
                        .setId(CustomTheme.newId())
                        .setTitle(mContext.getString(
                                R.string.custom_theme_title, customThemesCount + 1))
                        .build(mContext));
            }
        }
    }

    @Nullable
    @Override
    public ThemeBundle.Builder parseThemeBundle(String serializedTheme) throws JSONException {
        JSONObject theme = new JSONObject(serializedTheme);
        try {
            ThemeBundle.Builder builder = new ThemeBundle.Builder();
            convertJsonToBuilder(theme, builder);
            return builder;
        } catch (NameNotFoundException | NotFoundException e) {
            Log.i(TAG, "Couldn't parse serialized custom theme", e);
            return null;
        }
    }

    @Nullable
    @Override
    public CustomTheme.Builder parseCustomTheme(String serializedTheme) throws JSONException {
        JSONObject theme = new JSONObject(serializedTheme);
        try {
            CustomTheme.Builder builder = new CustomTheme.Builder();
            convertJsonToBuilder(theme, builder);
            return builder;
        } catch (NameNotFoundException | NotFoundException e) {
            Log.i(TAG, "Couldn't parse serialized custom theme", e);
            return null;
        }
    }

    private void convertJsonToBuilder(JSONObject theme, ThemeBundle.Builder builder)
            throws JSONException, NameNotFoundException, NotFoundException {
        Map<String, String> customPackages = new HashMap<>();
        Iterator<String> keysIterator = theme.keys();

        while (keysIterator.hasNext()) {
            String category = keysIterator.next();
            customPackages.put(category, theme.getString(category));
        }
        mOverlayProvider.addShapeOverlay(builder,
                customPackages.get(OVERLAY_CATEGORY_SHAPE));
        mOverlayProvider.addFontOverlay(builder,
                customPackages.get(OVERLAY_CATEGORY_FONT));
        mOverlayProvider.addColorOverlay(builder,
                customPackages.get(OVERLAY_CATEGORY_COLOR));
        mOverlayProvider.addAndroidIconOverlay(builder,
                customPackages.get(OVERLAY_CATEGORY_ICON_ANDROID));
        mOverlayProvider.addSysUiIconOverlay(builder,
                customPackages.get(OVERLAY_CATEGORY_ICON_SYSUI));
        mOverlayProvider.addNoPreviewIconOverlay(builder,
                customPackages.get(OVERLAY_CATEGORY_ICON_SETTINGS));
        mOverlayProvider.addNoPreviewIconOverlay(builder,
                customPackages.get(OVERLAY_CATEGORY_ICON_LAUNCHER));
        mOverlayProvider.addNoPreviewIconOverlay(builder,
                customPackages.get(OVERLAY_CATEGORY_ICON_THEMEPICKER));
        if (theme.has(THEME_TITLE_FIELD)) {
            builder.setTitle(theme.getString(THEME_TITLE_FIELD));
        }
        if (builder instanceof CustomTheme.Builder && theme.has(THEME_ID_FIELD)) {
            ((CustomTheme.Builder) builder).setId(theme.getString(THEME_ID_FIELD));
        }
    }

    @Override
    public ThemeBundle findEquivalent(ThemeBundle other) {
        if (mThemes == null) {
            return null;
        }
        for (ThemeBundle theme : mThemes) {
            if (theme.isEquivalent(other)) {
                return theme;
            }
        }
        return null;
    }

    private String getOverlayPackage(String prefix, String themeName) {
        return getItemStringFromStub(prefix, themeName);
    }

    private ResourceAsset getDrawableResourceAsset(String prefix, String themeName) {
        int drawableResId = mStubApkResources.getIdentifier(prefix + themeName,
                "drawable", mStubPackageName);
        return drawableResId == 0 ? null : new ResourceAsset(mStubApkResources, drawableResId,
                RequestOptions.fitCenterTransform());
    }

    private ThemeBundleThumbAsset getThumbAsset(String prefix, String themeName) {
        int drawableResId = mStubApkResources.getIdentifier(prefix + themeName,
                "drawable", mStubPackageName);
        return drawableResId == 0 ? null : new ThemeBundleThumbAsset(mStubApkResources,
                drawableResId);
    }
}
