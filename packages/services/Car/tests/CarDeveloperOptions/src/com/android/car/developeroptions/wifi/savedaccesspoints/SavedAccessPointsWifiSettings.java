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

package com.android.car.developeroptions.wifi.savedaccesspoints;

import android.annotation.Nullable;
import android.app.Dialog;
import android.app.settings.SettingsEnums;
import android.content.Context;
import android.content.DialogInterface;
import android.net.wifi.WifiManager;
import android.os.Bundle;
import android.util.FeatureFlagUtils;
import android.util.Log;

import com.android.car.developeroptions.R;
import com.android.car.developeroptions.core.FeatureFlags;
import com.android.car.developeroptions.core.SubSettingLauncher;
import com.android.car.developeroptions.dashboard.DashboardFragment;
import com.android.car.developeroptions.development.featureflags.FeatureFlagPersistent;
import com.android.car.developeroptions.wifi.WifiConfigUiBase;
import com.android.car.developeroptions.wifi.WifiDialog;
import com.android.car.developeroptions.wifi.WifiSettings;
import com.android.car.developeroptions.wifi.details.WifiNetworkDetailsFragment;
import com.android.settingslib.wifi.AccessPoint;
import com.android.settingslib.wifi.AccessPointPreference;

/**
 * UI to manage saved networks/access points.
 */
public class SavedAccessPointsWifiSettings extends DashboardFragment
        implements WifiDialog.WifiDialogListener, DialogInterface.OnCancelListener {

    private static final String TAG = "SavedAccessPoints";

    private WifiManager mWifiManager;
    private Bundle mAccessPointSavedState;
    private AccessPoint mSelectedAccessPoint;

    // Instance state key
    private static final String SAVE_DIALOG_ACCESS_POINT_STATE = "wifi_ap_state";

    @Override
    public int getMetricsCategory() {
        return SettingsEnums.WIFI_SAVED_ACCESS_POINTS;
    }

    @Override
    protected int getPreferenceScreenResId() {
        return R.xml.wifi_display_saved_access_points;
    }

    @Override
    protected String getLogTag() {
        return TAG;
    }

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        mWifiManager = (WifiManager) getContext()
                .getApplicationContext().getSystemService(Context.WIFI_SERVICE);
        use(SavedAccessPointsPreferenceController.class)
                .setHost(this);
        use(SubscribedAccessPointsPreferenceController.class)
                .setHost(this);
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (savedInstanceState != null) {
            if (savedInstanceState.containsKey(SAVE_DIALOG_ACCESS_POINT_STATE)) {
                mAccessPointSavedState =
                        savedInstanceState.getBundle(SAVE_DIALOG_ACCESS_POINT_STATE);
            }
        }
    }

    public void showWifiDialog(@Nullable AccessPointPreference accessPoint) {
        removeDialog(WifiSettings.WIFI_DIALOG_ID);

        if (accessPoint != null) {
            // Save the access point and edit mode
            mSelectedAccessPoint = accessPoint.getAccessPoint();
        } else {
            // No access point is selected. Clear saved state.
            mSelectedAccessPoint = null;
            mAccessPointSavedState = null;
        }

        if (usingDetailsFragment(getContext())) {
            if (mSelectedAccessPoint == null) {
                mSelectedAccessPoint = new AccessPoint(getActivity(), mAccessPointSavedState);
            }
            final Bundle savedState = new Bundle();
            mSelectedAccessPoint.saveWifiState(savedState);

            new SubSettingLauncher(getContext())
                    .setTitleText(mSelectedAccessPoint.getTitle())
                    .setDestination(WifiNetworkDetailsFragment.class.getName())
                    .setArguments(savedState)
                    .setSourceMetricsCategory(getMetricsCategory())
                    .launch();
        } else {
            showDialog(WifiSettings.WIFI_DIALOG_ID);
        }
    }

    @Override
    public Dialog onCreateDialog(int dialogId) {
        switch (dialogId) {
            case WifiSettings.WIFI_DIALOG_ID:
                // Modify network
                if (mSelectedAccessPoint == null) {
                    // Restore AP from save state
                    mSelectedAccessPoint = new AccessPoint(getActivity(), mAccessPointSavedState);
                    // Reset the saved access point data
                    mAccessPointSavedState = null;
                }
                final WifiDialog dialog = WifiDialog.createModal(
                        getActivity(), this, mSelectedAccessPoint, WifiConfigUiBase.MODE_VIEW);
                dialog.setOnCancelListener(this);

                return dialog;
        }
        return super.onCreateDialog(dialogId);
    }

    @Override
    public int getDialogMetricsCategory(int dialogId) {
        switch (dialogId) {
            case WifiSettings.WIFI_DIALOG_ID:
                return SettingsEnums.DIALOG_WIFI_SAVED_AP_EDIT;
            default:
                return 0;
        }
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        // If the dialog is showing (indicated by the existence of mSelectedAccessPoint), then we
        // save its state.
        if (mSelectedAccessPoint != null) {
            mAccessPointSavedState = new Bundle();
            mSelectedAccessPoint.saveWifiState(mAccessPointSavedState);
            outState.putBundle(SAVE_DIALOG_ACCESS_POINT_STATE, mAccessPointSavedState);
        }
    }

    @Override
    public void onForget(WifiDialog dialog) {
        if (mSelectedAccessPoint != null) {
            if (mSelectedAccessPoint.isPasspointConfig()) {
                try {
                    mWifiManager.removePasspointConfiguration(
                            mSelectedAccessPoint.getPasspointFqdn());
                } catch (RuntimeException e) {
                    Log.e(TAG, "Failed to remove Passpoint configuration for "
                            + mSelectedAccessPoint.getConfigName());
                }
                if (isSubscriptionsFeatureEnabled()) {
                    use(SubscribedAccessPointsPreferenceController.class)
                            .postRefreshSubscribedAccessPoints();
                } else {
                    use(SavedAccessPointsPreferenceController.class)
                            .postRefreshSavedAccessPoints();
                }
            } else {
                // both onSuccess/onFailure will call postRefreshSavedAccessPoints
                mWifiManager.forget(mSelectedAccessPoint.getConfig().networkId,
                        use(SavedAccessPointsPreferenceController.class));
            }
            mSelectedAccessPoint = null;
        }
    }

    @Override
    public void onCancel(DialogInterface dialog) {
        mSelectedAccessPoint = null;
    }

    /**
     * Checks if showing WifiNetworkDetailsFragment when clicking saved network item.
     */
    public static boolean usingDetailsFragment(Context context) {
        return FeatureFlagUtils.isEnabled(context, FeatureFlags.WIFI_DETAILS_SAVED_SCREEN);
    }

    boolean isSubscriptionsFeatureEnabled() {
        return FeatureFlagUtils.isEnabled(getContext(), FeatureFlags.MOBILE_NETWORK_V2)
                && FeatureFlagPersistent.isEnabled(getContext(), FeatureFlags.NETWORK_INTERNET_V2);
    }
}
