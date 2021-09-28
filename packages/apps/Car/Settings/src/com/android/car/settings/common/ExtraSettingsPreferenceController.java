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

import static com.android.settingslib.drawer.SwitchesProvider.METHOD_GET_DYNAMIC_SUMMARY;
import static com.android.settingslib.drawer.SwitchesProvider.METHOD_GET_DYNAMIC_TITLE;
import static com.android.settingslib.drawer.SwitchesProvider.METHOD_GET_PROVIDER_ICON;
import static com.android.settingslib.drawer.TileUtils.META_DATA_PREFERENCE_ICON_TINTABLE;
import static com.android.settingslib.drawer.TileUtils.META_DATA_PREFERENCE_ICON_URI;
import static com.android.settingslib.drawer.TileUtils.META_DATA_PREFERENCE_SUMMARY;
import static com.android.settingslib.drawer.TileUtils.META_DATA_PREFERENCE_SUMMARY_URI;
import static com.android.settingslib.drawer.TileUtils.META_DATA_PREFERENCE_TITLE;
import static com.android.settingslib.drawer.TileUtils.META_DATA_PREFERENCE_TITLE_URI;

import android.car.drivingstate.CarUxRestrictions;
import android.content.ContentResolver;
import android.content.Context;
import android.content.IContentProvider;
import android.content.Intent;
import android.database.ContentObserver;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.text.TextUtils;
import android.util.ArrayMap;
import android.util.Pair;

import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;
import androidx.preference.PreferenceGroup;

import com.android.car.apps.common.util.Themes;
import com.android.car.settings.R;
import com.android.settingslib.drawer.TileUtils;
import com.android.settingslib.utils.ThreadUtils;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/**
 * Injects preferences from other system applications at a placeholder location. The placeholder
 * should be a {@link PreferenceGroup} which sets the controller attribute to the fully qualified
 * name of this class. The preference should contain an intent which will be passed to
 * {@link ExtraSettingsLoader#loadPreferences(Intent)}.
 *
 * <p>For example:
 * <pre>{@code
 * <PreferenceCategory
 *     android:key="@string/pk_system_extra_settings"
 *     android:title="@string/system_extra_settings_title"
 *     settings:controller="com.android.settings.common.ExtraSettingsPreferenceController">
 *     <intent android:action="com.android.settings.action.EXTRA_SETTINGS">
 *         <extra android:name="com.android.settings.category"
 *                android:value="com.android.settings.category.system"/>
 *     </intent>
 * </PreferenceCategory>
 * }</pre>
 *
 * @see ExtraSettingsLoader
 */
// TODO: investigate using SettingsLib Tiles.
public class ExtraSettingsPreferenceController extends PreferenceController<PreferenceGroup> {
    private static final Logger LOG = new Logger(ExtraSettingsPreferenceController.class);

    @VisibleForTesting
    static final String META_DATA_DISTRACTION_OPTIMIZED = "distractionOptimized";

    private Context mContext;
    private ContentResolver mContentResolver;
    private ExtraSettingsLoader mExtraSettingsLoader;
    private boolean mSettingsLoaded;
    @VisibleForTesting
    List<DynamicDataObserver> mObservers = new ArrayList<>();

    public ExtraSettingsPreferenceController(Context context, String preferenceKey,
            FragmentController fragmentController, CarUxRestrictions restrictionInfo) {
        super(context, preferenceKey, fragmentController, restrictionInfo);
        mContext = context;
        mContentResolver = context.getContentResolver();
        mExtraSettingsLoader = new ExtraSettingsLoader(context);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    public void setExtraSettingsLoader(ExtraSettingsLoader extraSettingsLoader) {
        mExtraSettingsLoader = extraSettingsLoader;
    }

    @Override
    protected Class<PreferenceGroup> getPreferenceType() {
        return PreferenceGroup.class;
    }

    @Override
    protected void onApplyUxRestrictions(CarUxRestrictions uxRestrictions) {
        // If preference intents into an activity that's not distraction optimized, disable the
        // preference. This will override the UXRE flags config_ignore_ux_restrictions and
        // config_always_ignore_ux_restrictions because navigating to these non distraction
        // optimized activities will cause the blocking activity to come up, which dead ends the
        // user.
        for (int i = 0; i < getPreference().getPreferenceCount(); i++) {
            boolean restricted = false;
            Preference preference = getPreference().getPreference(i);
            if (uxRestrictions.isRequiresDistractionOptimization()
                    && !preference.getExtras().getBoolean(META_DATA_DISTRACTION_OPTIMIZED)
                    && getAvailabilityStatus() != AVAILABLE_FOR_VIEWING) {
                restricted = true;
            }
            preference.setEnabled(getAvailabilityStatus() != AVAILABLE_FOR_VIEWING);
            restrictPreference(preference, restricted);
        }
    }

    @Override
    protected void updateState(PreferenceGroup preference) {
        Map<Preference, Bundle> preferenceBundleMap = mExtraSettingsLoader.loadPreferences(
                preference.getIntent());
        if (!mSettingsLoaded) {
            addExtraSettings(preferenceBundleMap);
            mSettingsLoaded = true;
        }
        preference.setVisible(preference.getPreferenceCount() > 0);
    }

    @Override
    protected void onStartInternal() {
        mObservers.forEach(observer -> {
            observer.register(mContentResolver, /* register= */ true);
        });
    }

    @Override
    protected void onStopInternal() {
        mObservers.forEach(observer -> {
            observer.register(mContentResolver, /* register= */ false);
        });
    }

    /**
     * Adds the extra settings from the system based on the intent that is passed in the preference
     * group. All the preferences that resolve these intents will be added in the preference group.
     *
     * @param preferenceBundleMap a map of {@link Preference} and {@link Bundle} representing
     * settings injected from system apps and their metadata.
     */
    protected void addExtraSettings(Map<Preference, Bundle> preferenceBundleMap) {
        for (Preference setting : preferenceBundleMap.keySet()) {
            Bundle metaData = preferenceBundleMap.get(setting);
            boolean distractionOptimized = false;
            if (metaData.containsKey(META_DATA_DISTRACTION_OPTIMIZED)) {
                distractionOptimized =
                        metaData.getBoolean(META_DATA_DISTRACTION_OPTIMIZED);
            }
            setting.getExtras().putBoolean(META_DATA_DISTRACTION_OPTIMIZED, distractionOptimized);
            setting.getExtras().putBoolean(META_DATA_PREFERENCE_ICON_TINTABLE,
                    ExtraSettingsUtil.isIconTintable(metaData));
            getDynamicData(setting, metaData);
            getPreference().addPreference(setting);
        }
    }

    /**
     * Retrieve dynamic injected preference data and create observers for updates.
     */
    protected void getDynamicData(Preference preference, Bundle metaData) {
        if (metaData.containsKey(META_DATA_PREFERENCE_TITLE_URI)) {
            // Set a placeholder title before starting to fetch real title to prevent vertical
            // preference shift.
            preference.setTitle(R.string.empty_placeholder);
            Uri uri = ExtraSettingsUtil.getCompleteUri(metaData, META_DATA_PREFERENCE_TITLE_URI,
                    METHOD_GET_DYNAMIC_TITLE);
            refreshTitle(uri, preference);
            mObservers.add(new DynamicDataObserver(METHOD_GET_DYNAMIC_TITLE, uri, preference));
        }
        if (metaData.containsKey(META_DATA_PREFERENCE_SUMMARY_URI)) {
            // Set a placeholder summary before starting to fetch real summary to prevent vertical
            // preference shift.
            preference.setSummary(R.string.empty_placeholder);
            Uri uri = ExtraSettingsUtil.getCompleteUri(metaData, META_DATA_PREFERENCE_SUMMARY_URI,
                    METHOD_GET_DYNAMIC_SUMMARY);
            refreshSummary(uri, preference);
            mObservers.add(new DynamicDataObserver(METHOD_GET_DYNAMIC_SUMMARY, uri, preference));
        }
        if (metaData.containsKey(META_DATA_PREFERENCE_ICON_URI)) {
            // Set a placeholder icon before starting to fetch real icon to prevent horizontal
            // preference shift.
            preference.setIcon(R.drawable.ic_placeholder);
            Uri uri = ExtraSettingsUtil.getCompleteUri(metaData, META_DATA_PREFERENCE_ICON_URI,
                    METHOD_GET_PROVIDER_ICON);
            refreshIcon(uri, preference);
            mObservers.add(new DynamicDataObserver(METHOD_GET_PROVIDER_ICON, uri, preference));
        }
    }

    @VisibleForTesting
    void executeBackgroundTask(Runnable r) {
        ThreadUtils.postOnBackgroundThread(r);
    }

    @VisibleForTesting
    void executeUiTask(Runnable r) {
        ThreadUtils.postOnMainThread(r);
    }

    private void refreshTitle(Uri uri, Preference preference) {
        executeBackgroundTask(() -> {
            Map<String, IContentProvider> providerMap = new ArrayMap<>();
            String titleFromUri = TileUtils.getTextFromUri(
                    mContext, uri, providerMap, META_DATA_PREFERENCE_TITLE);
            if (!TextUtils.equals(titleFromUri, preference.getTitle())) {
                executeUiTask(() -> preference.setTitle(titleFromUri));
            }
        });
    }

    private void refreshSummary(Uri uri, Preference preference) {
        executeBackgroundTask(() -> {
            Map<String, IContentProvider> providerMap = new ArrayMap<>();
            String summaryFromUri = TileUtils.getTextFromUri(
                    mContext, uri, providerMap, META_DATA_PREFERENCE_SUMMARY);
            if (!TextUtils.equals(summaryFromUri, preference.getSummary())) {
                executeUiTask(() -> preference.setSummary(summaryFromUri));
            }
        });
    }

    private void refreshIcon(Uri uri, Preference preference) {
        executeBackgroundTask(() -> {
            Intent intent = preference.getIntent();
            String packageName = null;
            if (!TextUtils.isEmpty(intent.getPackage())) {
                packageName = intent.getPackage();
            } else if (intent.getComponent() != null) {
                packageName = intent.getComponent().getPackageName();
            }
            Map<String, IContentProvider> providerMap = new ArrayMap<>();
            Pair<String, Integer> iconInfo = TileUtils.getIconFromUri(
                    mContext, packageName, uri, providerMap);
            Drawable icon;
            if (iconInfo != null) {
                icon = ExtraSettingsUtil.loadDrawableFromPackage(mContext,
                        iconInfo.first, iconInfo.second);
            } else {
                LOG.w("Failed to get icon from uri " + uri);
                icon = mContext.getDrawable(R.drawable.ic_settings_gear);
                LOG.d("use default icon.");
            }
            if (icon != null) {
                executeUiTask(() -> {
                    preference.setIcon(icon);
                    if (preference.getExtras().getBoolean(META_DATA_PREFERENCE_ICON_TINTABLE)) {
                        preference.getIcon().setTintList(
                                Themes.getAttrColorStateList(mContext, R.attr.iconColor));
                    }
                });
            }
        });
    }

    /**
     * Observer for updating injected dynamic data.
     */
    private class DynamicDataObserver extends ContentObserver {
        private final String mMethod;
        private final Uri mUri;
        private final Preference mPreference;

        DynamicDataObserver(String method, Uri uri, Preference preference) {
            super(new Handler(Looper.getMainLooper()));
            mMethod = method;
            mUri = uri;
            mPreference = preference;
        }

        /** Registers or unregisters this observer to the given content resolver. */
        void register(ContentResolver cr, boolean register) {
            if (register) {
                cr.registerContentObserver(mUri, /* notifyForDescendants= */ false,
                        /* observer= */ this);
            } else {
                cr.unregisterContentObserver(this);
            }
        }

        @Override
        public void onChange(boolean selfChange) {
            switch (mMethod) {
                case METHOD_GET_DYNAMIC_TITLE:
                    refreshTitle(mUri, mPreference);
                    break;
                case METHOD_GET_DYNAMIC_SUMMARY:
                    refreshSummary(mUri, mPreference);
                    break;
                case METHOD_GET_PROVIDER_ICON:
                    refreshIcon(mUri, mPreference);
                    break;
            }
        }
    }
}
