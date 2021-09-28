/*
 * Copyright (C) 2015 The Android Open Source Project
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
 * limitations under the License
 */

package com.android.tv.settings.advance_settings;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.os.Bundle;
import androidx.preference.Preference;
import androidx.preference.PreferenceScreen;
import android.text.TextUtils;
import android.util.Log;

import com.android.internal.logging.nano.MetricsProto;
import com.android.tv.settings.R;
import com.android.tv.settings.RadioPreference;
import com.android.tv.settings.SettingsPreferenceFragment;

import java.util.Iterator;
import java.util.List;

/**
 * The location settings screen in TV settings.
 */
public class DefaultLauncherFragment extends SettingsPreferenceFragment {

    private static final String TAG = "DefaultLauncherFragment";

    private PackageManager packageManger;

    public static DefaultLauncherFragment newInstance() {
        return new DefaultLauncherFragment();
    }

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        Log.i(TAG, "onCreatePreferences");
        final Context themedContext = getPreferenceManager().getContext();
        final PreferenceScreen screen = getPreferenceManager().createPreferenceScreen(themedContext);
        screen.setTitle(R.string.default_launcher);

        packageManger = getActivity().getPackageManager();
        PackageManager pm = packageManger;
        List<ResolveInfo> packageInfos = getResolveInfoList();
        ResolveInfo currentRI = getCurrentLauncher();

        Iterator<ResolveInfo> it = packageInfos.iterator();
        while (it.hasNext()) {
            ResolveInfo ri = it.next();
            if (ri != null && ri.activityInfo.applicationInfo.isDirectBootAware()) {
                it.remove();
            }
        }

        if (packageInfos != null && packageInfos.size() > 0) {
            Preference activePref = null;
            for (ResolveInfo resolveInfo : packageInfos) {
                final RadioPreference radioPreference = new RadioPreference(themedContext);
                radioPreference.setKey(resolveInfo.activityInfo.packageName);
                radioPreference.setPersistent(false);
                radioPreference.setTitle(resolveInfo.loadLabel(pm).toString());
                radioPreference.setIcon(resolveInfo.loadIcon(pm));
                radioPreference.setLayoutResource(R.layout.preference_reversed_widget);

                if (!TextUtils.isEmpty(currentRI.activityInfo.packageName)
                        && !TextUtils.isEmpty(resolveInfo.activityInfo.packageName)
                        && TextUtils.equals(currentRI.activityInfo.packageName, resolveInfo.activityInfo.packageName)) {
                    radioPreference.setChecked(true);
                    activePref = radioPreference;
                }

                screen.addPreference(radioPreference);
            }

            if (activePref != null && savedInstanceState == null) {
                scrollToPreference(activePref);
            }
        }
        setPreferenceScreen(screen);
    }

    @Override
    public boolean onPreferenceTreeClick(Preference preference) {
        if (preference instanceof RadioPreference) {
            final RadioPreference radioPreference = (RadioPreference) preference;
            radioPreference.clearOtherRadioPreferences(getPreferenceScreen());
            if (radioPreference.isChecked()) {
                String packageName = radioPreference.getKey();
                setDefaultLauncher(packageName);
            } else {
                radioPreference.setChecked(true);
            }
        }
        return super.onPreferenceTreeClick(preference);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
    }

    @Override
    public int getMetricsCategory() {
        return MetricsProto.MetricsEvent.MANAGE_APPLICATIONS;
    }

    private List<ResolveInfo> getResolveInfoList() {
        PackageManager pm = packageManger;
        Intent intent = new Intent(Intent.ACTION_MAIN, null);
        intent.addCategory(Intent.CATEGORY_HOME);
        intent.addCategory(Intent.CATEGORY_DEFAULT);
        return pm.queryIntentActivities(intent, 0);
    }

    private ResolveInfo getCurrentLauncher() {
        PackageManager pm = packageManger;
        Intent intent = new Intent(Intent.ACTION_MAIN, null);
        intent.addCategory(Intent.CATEGORY_HOME);
        intent.addCategory(Intent.CATEGORY_DEFAULT);
        return pm.resolveActivity(intent, 0);
    }

    private void setDefaultLauncher(String packageName) {
        PackageManager pm = packageManger;
        ResolveInfo currentLauncher = getCurrentLauncher();

        List<ResolveInfo> packageInfos = getResolveInfoList();

        ResolveInfo futureLauncher = null;

        for (ResolveInfo ri : packageInfos) {
            if (!TextUtils.isEmpty(ri.activityInfo.packageName) && !TextUtils.isEmpty(packageName)
                    && TextUtils.equals(ri.activityInfo.packageName, packageName)) {
                futureLauncher = ri;
            }
        }

        if (futureLauncher == null) {
            return;
        }

        pm.clearPackagePreferredActivities(currentLauncher.activityInfo.packageName);
        IntentFilter intentFilter = new IntentFilter();
        intentFilter.addAction(Intent.ACTION_MAIN);
        intentFilter.addCategory(Intent.CATEGORY_HOME);
        intentFilter.addCategory(Intent.CATEGORY_DEFAULT);
        ComponentName componentName = new ComponentName(futureLauncher.activityInfo.packageName,
                futureLauncher.activityInfo.name);
        ComponentName[] componentNames = new ComponentName[packageInfos.size()];
        int defaultMatch = 0;
        for (int i = 0; i < packageInfos.size(); i++) {
            ResolveInfo resolveInfo = packageInfos.get(i);
            componentNames[i] = new ComponentName(resolveInfo.activityInfo.packageName, resolveInfo.activityInfo.name);
            if (defaultMatch < resolveInfo.match) {
                defaultMatch = resolveInfo.match;
            }
        }
        pm.clearPackagePreferredActivities(currentLauncher.activityInfo.packageName);
        pm.addPreferredActivity(intentFilter, defaultMatch, componentNames, componentName);
    }

}
