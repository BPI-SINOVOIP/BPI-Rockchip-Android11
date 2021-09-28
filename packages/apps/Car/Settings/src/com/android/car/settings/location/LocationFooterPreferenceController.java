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

package com.android.car.settings.location;

import android.car.drivingstate.CarUxRestrictions;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.location.LocationManager;

import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.preference.PreferenceGroup;

import com.android.car.settings.R;
import com.android.car.settings.common.FragmentController;
import com.android.car.settings.common.Logger;
import com.android.car.settings.common.PreferenceController;
import com.android.car.ui.preference.CarUiPreference;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Injects Location Footers into a {@link PreferenceGroup} with a matching key.
 */
public class LocationFooterPreferenceController extends PreferenceController<PreferenceGroup> {
    private static final Logger LOG = new Logger(LocationFooterPreferenceController.class);
    private static final Intent INJECT_INTENT =
            new Intent(LocationManager.SETTINGS_FOOTER_DISPLAYED_ACTION);

    private PackageManager mPackageManager;

    public LocationFooterPreferenceController(Context context, String preferenceKey,
            FragmentController fragmentController, CarUxRestrictions uxRestrictions) {
        super(context, preferenceKey, fragmentController, uxRestrictions);
        mPackageManager = context.getPackageManager();
    }

    @VisibleForTesting
    void setPackageManager(PackageManager packageManager) {
        mPackageManager = packageManager;
    }

    @Override
    protected Class<PreferenceGroup> getPreferenceType() {
        return PreferenceGroup.class;
    }

    @Override
    protected void onCreateInternal() {
        for (LocationFooter footer : getInjectedLocationFooters()) {
            try {
                String footerString = mPackageManager
                        .getResourcesForApplication(footer.mApplicationInfo)
                        .getString(footer.mFooterStringRes);

                // For each injected footer: Create a new preference, set the summary
                // and icon, then inject under the footer preference group.
                CarUiPreference newPreference = new CarUiPreference(getContext());
                newPreference.setSummary(footerString);
                newPreference.setIcon(R.drawable.ic_settings_about);
                newPreference.setSelectable(false);
                getPreference().addPreference(newPreference);
            } catch (PackageManager.NameNotFoundException exception) {
                LOG.w("Resources not found for application "
                        + footer.mApplicationInfo.packageName);
            }
        }
    }

    @Override
    protected void updateState(PreferenceGroup preferenceGroup) {
        preferenceGroup.setVisible(preferenceGroup.getPreferenceCount() > 0);
    }

    /**
     * Return a list of strings provided by ACTION_INJECT_FOOTER broadcast receivers. If there
     * are no injectors, an immutable emptry list is returned.
     */
    private List<LocationFooter> getInjectedLocationFooters() {
        List<ResolveInfo> resolveInfos = mPackageManager.queryBroadcastReceivers(
                INJECT_INTENT, PackageManager.GET_META_DATA);
        if (resolveInfos == null) {
            LOG.e("Unable to resolve intent " + INJECT_INTENT);
            return Collections.emptyList();
        } else {
            LOG.d("Found broadcast receivers: " + resolveInfos);
        }

        List<LocationFooter> locationFooters = new ArrayList<>(resolveInfos.size());
        for (ResolveInfo resolveInfo : resolveInfos) {
            ActivityInfo activityInfo = resolveInfo.activityInfo;
            ApplicationInfo appInfo = activityInfo.applicationInfo;

            // If a non-system app tries to inject footer, ignore it
            if ((appInfo.flags & ApplicationInfo.FLAG_SYSTEM) == 0) {
                LOG.w("Ignoring attempt to inject footer from a non-system app: " + resolveInfo);
                continue;
            }

            // If the injector does not have valid METADATA, ignore it
            if (activityInfo.metaData == null) {
                LOG.d("No METADATA in broadcast receiver " + activityInfo.name);
                continue;
            }

            // Get the footer text resource id from broadcast receiver's metadata
            int footerTextRes =
                    activityInfo.metaData.getInt(LocationManager.METADATA_SETTINGS_FOOTER_STRING);
            if (footerTextRes == 0) {
                LOG.w("No mapping of integer exists for "
                        + LocationManager.METADATA_SETTINGS_FOOTER_STRING);
                continue;
            }
            locationFooters.add(new LocationFooter(footerTextRes, appInfo));
        }
        return locationFooters;
    }

    /**
     * Contains information related to a footer.
     */
    private static class LocationFooter {
        // The string resource of the footer.
        @StringRes
        private final int mFooterStringRes;
        // Application info of the receiver injecting this footer.
        private final ApplicationInfo mApplicationInfo;

        LocationFooter(@StringRes int footerRes, ApplicationInfo appInfo) {
            mFooterStringRes = footerRes;
            mApplicationInfo = appInfo;
        }
    }
}
