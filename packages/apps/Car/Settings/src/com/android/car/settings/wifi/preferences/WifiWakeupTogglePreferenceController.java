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

package com.android.car.settings.wifi.preferences;

import android.app.Service;
import android.car.drivingstate.CarUxRestrictions;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.location.LocationManager;
import android.net.wifi.WifiManager;
import android.provider.Settings;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.TextUtils;
import android.text.style.URLSpan;
import android.widget.Toast;

import androidx.annotation.VisibleForTesting;
import androidx.preference.TwoStatePreference;

import com.android.car.settings.R;
import com.android.car.settings.common.ConfirmationDialogFragment;
import com.android.car.settings.common.FragmentController;
import com.android.car.settings.common.Logger;
import com.android.car.settings.common.PreferenceController;
import com.android.settingslib.HelpUtils;

/** Business logic to allow auto-enabling of wifi near saved networks. */
public class WifiWakeupTogglePreferenceController extends PreferenceController<TwoStatePreference> {

    private static final Logger LOG = new Logger(WifiWakeupTogglePreferenceController.class);
    private LocationManager mLocationManager;
    @VisibleForTesting
    WifiManager mWifiManager;

    @VisibleForTesting
    final ConfirmationDialogFragment.ConfirmListener mConfirmListener = arguments -> {
        enableWifiScanning();
        if (mLocationManager.isLocationEnabled()) {
            setWifiWakeupEnabled(true);
        }
        refreshUi();
    };

    private final ConfirmationDialogFragment.NeutralListener mNeutralListener = arguments -> {
        Intent intent = HelpUtils.getHelpIntent(getContext(),
                getContext().getString(R.string.help_uri_wifi_scanning_required),
                getContext().getClass().getName());
        if (intent != null) {
            try {
                getContext().startActivity(intent);
            } catch (ActivityNotFoundException e) {
                LOG.e("Activity was not found for intent, " + intent.toString());
            }
        }
    };

    public WifiWakeupTogglePreferenceController(Context context, String preferenceKey,
            FragmentController fragmentController, CarUxRestrictions uxRestrictions) {
        super(context, preferenceKey, fragmentController, uxRestrictions);
        mLocationManager = (LocationManager) context.getSystemService(Service.LOCATION_SERVICE);
        mWifiManager = context.getSystemService(WifiManager.class);
    }

    @Override
    protected Class<TwoStatePreference> getPreferenceType() {
        return TwoStatePreference.class;
    }

    @Override
    protected void onCreateInternal() {
        ConfirmationDialogFragment dialog =
                (ConfirmationDialogFragment) getFragmentController().findDialogByTag(
                        ConfirmationDialogFragment.TAG);
        ConfirmationDialogFragment.resetListeners(
                dialog,
                mConfirmListener,
                /* rejectListener= */ null,
                mNeutralListener);
    }

    @Override
    protected void updateState(TwoStatePreference preference) {
        preference.setChecked(getWifiWakeupEnabled()
                && getWifiScanningEnabled()
                && mLocationManager.isLocationEnabled());
        if (!mLocationManager.isLocationEnabled()) {
            preference.setSummary(getNoLocationSummary());
        } else {
            preference.setSummary(R.string.wifi_wakeup_summary);
        }
    }

    @Override
    protected boolean handlePreferenceClicked(TwoStatePreference preference) {
        if (!mLocationManager.isLocationEnabled()) {
            Intent intent = new Intent(Settings.ACTION_LOCATION_SOURCE_SETTINGS);
            getContext().startActivity(intent);
        } else if (getWifiWakeupEnabled()) {
            setWifiWakeupEnabled(false);
        } else if (!getWifiScanningEnabled()) {
            showScanningDialog();
        } else {
            setWifiWakeupEnabled(true);
        }

        refreshUi();
        return true;
    }

    private boolean getWifiWakeupEnabled() {
        return Settings.Global.getInt(getContext().getContentResolver(),
                Settings.Global.WIFI_WAKEUP_ENABLED, 0) == 1;
    }

    private void setWifiWakeupEnabled(boolean enabled) {
        Settings.Global.putInt(getContext().getContentResolver(),
                Settings.Global.WIFI_WAKEUP_ENABLED,
                enabled ? 1 : 0);
    }

    private boolean getWifiScanningEnabled() {
        return mWifiManager.isScanAlwaysAvailable();
    }

    private void enableWifiScanning() {
        mWifiManager.setScanAlwaysAvailable(true);
        Toast.makeText(
                getContext(),
                getContext().getString(R.string.wifi_settings_scanning_required_enabled),
                Toast.LENGTH_SHORT).show();
    }

    private CharSequence getNoLocationSummary() {
        String highlightedWord = "link";
        CharSequence locationText = getContext()
                .getText(R.string.wifi_wakeup_summary_no_location);

        SpannableString msg = new SpannableString(locationText);
        int startIndex = locationText.toString().indexOf(highlightedWord);
        if (startIndex < 0) {
            LOG.e("Cannot create URL span");
            return locationText;
        }
        msg.setSpan(new URLSpan((String) null), startIndex, startIndex + highlightedWord.length(),
                Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        return msg;
    }

    private void showScanningDialog() {
        ConfirmationDialogFragment dialogFragment =
                getConfirmEnableWifiScanningDialogFragment();
        getFragmentController().showDialog(
                dialogFragment, ConfirmationDialogFragment.TAG);
    }

    private ConfirmationDialogFragment getConfirmEnableWifiScanningDialogFragment() {
        ConfirmationDialogFragment.Builder builder =
                new ConfirmationDialogFragment.Builder(getContext())
                        .setTitle(R.string.wifi_settings_scanning_required_title)
                        .setPositiveButton(
                                R.string.wifi_settings_scanning_required_turn_on, mConfirmListener)
                        .setNegativeButton(R.string.cancel, /* rejectListener= */ null);

        // Only show "learn more" if there is a help page to show
        if (!TextUtils.isEmpty(getContext().getString(R.string.help_uri_wifi_scanning_required))) {
            builder.setNeutralButton(R.string.learn_more, mNeutralListener);
        }
        return builder.build();
    }
}
