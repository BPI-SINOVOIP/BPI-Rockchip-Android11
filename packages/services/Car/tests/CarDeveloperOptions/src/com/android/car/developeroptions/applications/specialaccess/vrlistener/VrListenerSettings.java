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
package com.android.car.developeroptions.applications.specialaccess.vrlistener;

import android.app.settings.SettingsEnums;
import android.content.ComponentName;
import android.content.Context;
import android.provider.SearchIndexableResource;
import android.provider.Settings;
import android.service.vr.VrListenerService;

import androidx.annotation.VisibleForTesting;

import com.android.car.developeroptions.R;
import com.android.car.developeroptions.overlay.FeatureFactory;
import com.android.car.developeroptions.search.BaseSearchIndexProvider;
import com.android.car.developeroptions.utils.ManagedServiceSettings;
import com.android.settingslib.core.instrumentation.MetricsFeatureProvider;
import com.android.settingslib.search.Indexable;
import com.android.settingslib.search.SearchIndexable;

import java.util.ArrayList;
import java.util.List;

@SearchIndexable
public class VrListenerSettings extends ManagedServiceSettings {
    private static final String TAG = VrListenerSettings.class.getSimpleName();
    private static final Config CONFIG = new Config.Builder()
            .setTag(TAG)
            .setSetting(Settings.Secure.ENABLED_VR_LISTENERS)
            .setIntentAction(VrListenerService.SERVICE_INTERFACE)
            .setPermission(android.Manifest.permission.BIND_VR_LISTENER_SERVICE)
            .setNoun("vr listener")
            .setWarningDialogTitle(R.string.vr_listener_security_warning_title)
            .setWarningDialogSummary(R.string.vr_listener_security_warning_summary)
            .setEmptyText(R.string.no_vr_listeners)
            .build();

    @Override
    protected Config getConfig() {
        return CONFIG;
    }

    @Override
    public int getMetricsCategory() {
        return SettingsEnums.VR_MANAGE_LISTENERS;
    }

    @Override
    protected boolean setEnabled(ComponentName service, String title, boolean enable) {
        logSpecialPermissionChange(enable, service.getPackageName());
        return super.setEnabled(service, title, enable);
    }

    @Override
    protected int getPreferenceScreenResId() {
        return R.xml.vr_listeners_settings;
    }

    @VisibleForTesting
    void logSpecialPermissionChange(boolean enable, String packageName) {
        int logCategory = enable ? SettingsEnums.APP_SPECIAL_PERMISSION_VRHELPER_ALLOW
                : SettingsEnums.APP_SPECIAL_PERMISSION_VRHELPER_DENY;
        final MetricsFeatureProvider metricsFeatureProvider =
                FeatureFactory.getFactory(getContext()).getMetricsFeatureProvider();
        metricsFeatureProvider.action(
                metricsFeatureProvider.getAttribution(getActivity()),
                logCategory,
                getMetricsCategory(),
                packageName,
                0);
    }

    public static final Indexable.SearchIndexProvider SEARCH_INDEX_DATA_PROVIDER =
            new BaseSearchIndexProvider() {
                @Override
                public List<SearchIndexableResource> getXmlResourcesToIndex(Context context,
                        boolean enabled) {
                    final List<SearchIndexableResource> result = new ArrayList<>();

                    final SearchIndexableResource sir = new SearchIndexableResource(context);
                    sir.xmlResId = R.xml.vr_listeners_settings;
                    result.add(sir);
                    return result;
                }
            };

}
