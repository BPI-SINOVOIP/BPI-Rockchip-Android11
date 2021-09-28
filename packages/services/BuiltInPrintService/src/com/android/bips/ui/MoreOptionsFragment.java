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

package com.android.bips.ui;

import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Bundle;
import android.preference.Preference;
import android.preference.PreferenceCategory;
import android.preference.PreferenceFragment;
import android.print.PrintManager;
import android.printservice.recommendation.RecommendationInfo;
import android.util.Log;

import com.android.bips.R;

import java.net.InetAddress;
import java.text.Collator;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * A fragment allowing the user to review recommended print services and install or enable them.
 */
public class MoreOptionsFragment extends PreferenceFragment implements
        PrintManager.PrintServiceRecommendationsChangeListener {
    private static final String TAG = MoreOptionsFragment.class.getSimpleName();
    private static final boolean DEBUG = false;

    private static final String KEY_RECOMMENDATION_CATEGORY = "recommendation_category";
    private static final String KEY_MANAGE = "manage";
    private static final String PACKAGE_NAME_VENDING = "com.android.vending";
    private static final Collator COLLATOR = Collator.getInstance();

    private PrintManager mPrintManager;
    private PackageManager mPackageManager;
    private PreferenceCategory mRecommendations;
    private MoreOptionsActivity mActivity;
    private boolean mHasVending = false;
    private Map<String, RecommendationItem> mItems = new HashMap<>();

    @Override
    public void onCreate(Bundle in) {
        super.onCreate(in);

        addPreferencesFromResource(R.xml.more_options_prefs);

        mRecommendations = (PreferenceCategory) getPreferenceScreen().findPreference(
                KEY_RECOMMENDATION_CATEGORY);

        getPreferenceScreen().findPreference(KEY_MANAGE)
                .setOnPreferenceClickListener(preference -> {
                    startActivity(new Intent(android.provider.Settings.ACTION_PRINT_SETTINGS)
                            .setFlags(Intent.FLAG_ACTIVITY_NEW_TASK));
                    return true;
                });
    }

    @Override
    public void onStart() {
        super.onStart();
        mPackageManager = getContext().getPackageManager();

        try {
            // Attempt to get vending package, throws if missing
            mPackageManager.getPackageInfo(PACKAGE_NAME_VENDING, 0);
            mHasVending = true;
        } catch (PackageManager.NameNotFoundException ignored) {
        }

        mActivity = (MoreOptionsActivity) getActivity();

        mPrintManager = getContext().getSystemService(PrintManager.class);

        // Start listening for recommendation changes
        mPrintManager.addPrintServiceRecommendationsChangeListener(this, null);
        onPrintServiceRecommendationsChanged();
    }

    @Override
    public void onStop() {
        super.onStop();
        mPrintManager.removePrintServiceRecommendationsChangeListener(this);
    }

    @Override
    public void onPrintServiceRecommendationsChanged() {
        List<RecommendationInfo> infos = mPrintManager.getPrintServiceRecommendations();
        if (DEBUG) Log.d(TAG, "All recommendations count=" + infos.size());
        // Update items list with new recommendations. Recommendations don't go away.
        for (RecommendationInfo info : infos) {
            for (InetAddress address : info.getDiscoveredPrinters()) {
                if (address.equals(mActivity.mPrinterAddress)) {
                    // This recommendation matches so create or update an item for it
                    String packageName = info.getPackageName().toString();
                    RecommendationItem item = getOrCreateItem(info);
                    try {
                        // If this doesn't throw then the service is installed
                        item.mPrintService = mPackageManager.getPackageInfo(packageName, 0);
                    } catch (PackageManager.NameNotFoundException e) {
                        item.mPrintService = null;
                    }
                    break;
                }
            }
        }

        // Update preferences with ordering
        List<RecommendationItem> itemList = new ArrayList<>(mItems.values());
        Collections.sort(itemList);
        for (int index = 0; index < itemList.size(); index++) {
            itemList.get(index).updatePreference(index);
        }

        if (DEBUG) Log.d(TAG, "For this printer=" + mRecommendations.getPreferenceCount());

        // Show group if not empty
        if (mRecommendations.getPreferenceCount() == 0) {
            getPreferenceScreen().removePreference(mRecommendations);
        } else {
            getPreferenceScreen().addPreference(mRecommendations);
        }
    }

    private RecommendationItem getOrCreateItem(RecommendationInfo recommendationInfo) {
        String packageName = recommendationInfo.getPackageName().toString();
        RecommendationItem item = mItems.get(packageName);
        if (item == null) {
            item = new RecommendationItem(recommendationInfo);
            mItems.put(packageName, item);
        } else {
            item.mRecommendationInfo = recommendationInfo;
        }
        return item;
    }

    /** An item corresponding to a recommended print service. */
    private class RecommendationItem implements Comparable<RecommendationItem> {
        RecommendationInfo mRecommendationInfo;
        String mPackageName;
        Preference mPreference = new Preference(getContext());
        /** Present only if the corresponding print service is installed. */
        PackageInfo mPrintService;

        RecommendationItem(RecommendationInfo info) {
            mRecommendationInfo = info;
            mPackageName = mRecommendationInfo.getPackageName().toString();
        }

        void updatePreference(int order) {
            mPreference.setKey(mPackageName);
            mPreference.setTitle(mRecommendationInfo.getName());
            mPreference.setOrder(order);
            if (mPrintService != null) {
                updateEnabler();
                if (mRecommendations.findPreference(mPackageName) == null) {
                    mRecommendations.addPreference(mPreference);
                }
            } else if (mHasVending) {
                updateDownloader();
                if (mRecommendations.findPreference(mPackageName) == null) {
                    mRecommendations.addPreference(mPreference);
                }
            } else {
                mRecommendations.removePreference(mPreference);
            }
        }

        /** Update the preference to allow the user to enable an installed print service. */
        private void updateEnabler() {
            try {
                // Set icon from application if possible
                mPreference.setIcon(mPackageManager.getApplicationIcon(
                        mPrintService.packageName));
            } catch (PackageManager.NameNotFoundException e) {
                mPreference.setIcon(null);
                mPreference.setIconSpaceReserved(true);
            }
            mPreference.setSummary(R.string.recommendation_enable_summary);
            mPreference.setOnPreferenceClickListener(preference -> {
                // There's no activity to go directly to the print service screen
                startActivity(new Intent(android.provider.Settings.ACTION_PRINT_SETTINGS)
                        .setFlags(Intent.FLAG_ACTIVITY_NEW_TASK));
                return true;
            });
        }

        /** Update this preference to reflect a print service that must be downloaded. */
        private void updateDownloader() {
            mPreference.setIcon(R.drawable.ic_download_from_market);
            mPreference.setSummary(R.string.recommendation_install_summary);
            mPreference.setOnPreferenceClickListener(preference -> {
                Uri printServiceUri = Uri.parse("market://details?id="
                        + mRecommendationInfo.getPackageName());
                startActivity(new Intent(Intent.ACTION_VIEW, printServiceUri).setFlags(
                        Intent.FLAG_ACTIVITY_NEW_TASK));
                return true;
            });
        }

        @Override
        public int compareTo(RecommendationItem other) {
            // Sort items:
            // - first by single-vendor (more likely to be manufacturer-specific),
            // - then alphabetically.
            if (mRecommendationInfo.recommendsMultiVendorService()
                    != other.mRecommendationInfo.recommendsMultiVendorService()) {
                return mRecommendationInfo.recommendsMultiVendorService() ? 1 : -1;
            }

            return COLLATOR.compare(mRecommendationInfo.getName().toString(),
                    other.mRecommendationInfo.getName().toString());
        }
    }
}
